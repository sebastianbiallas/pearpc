# AArch64 JIT Port

This document describes the plan for porting PearPC's JIT compiler to AArch64 (ARM64), targeting macOS on Apple Silicon.

## How the x86_64 JIT Works

Understanding the existing JIT is essential before porting.

### Core Concepts

1. **Page-based translation cache**: PPC address space is divided into 4 KB pages. Each page is translated independently and cached as native code.

2. **Translation cache**: A single large RWX memory block (64 MB). Divided into 512-byte **fragments**. A translated page may span multiple fragments, linked by JMP instructions.

3. **ClientPage**: Tracks one translated 4 KB PPC page. Contains 1024 entry points (one per 4-byte PPC instruction offset). Managed via LRU for eviction.

4. **Basic block compilation**: Code is generated one basic block at a time (until a branch or page boundary). Register mappings are flushed at block boundaries.

### Execution Flow

```
ppc_cpu_run()
  └─> ppc_start_jitc_asm(pc, &gCPU, sizeof(gCPU))     [C++ -> ASM entry]
        ├─ Copy CPU state to stack (register-relative access)
        └─> ppc_new_pc_asm(initial_pc)                   [lookup/translate]
              ├─ ppc_heartbeat_ext_asm()                  [check interrupts]
              ├─ ppc_effective_to_physical_code()          [EA -> PA via TLB]
              ├─ jitcNewPC(physical_addr)                  [find or compile]
              │     ├─ Page already translated? Return cached entry point
              │     └─ Not translated? Call jitcStartTranslation()
              │           └─ Loop: fetch PPC opcode, call ppc_gen_opc()
              │                    which emits native instructions
              └─> JMP to native code                      [execute]
                    ├─ Runs until branch/page boundary
                    ├─ Same-page branch: patched direct JMP
                    ├─ Cross-page branch: -> ppc_new_pc_asm (re-translate)
                    └─ Exception: -> exception handler -> jitcNewPC(vector)
```

### Key Assembly Functions (jitc_tools.S)

| Function | Purpose |
|----------|---------|
| `ppc_start_jitc_asm` | Entry from C++; copies CPU state to stack, jumps to first PC |
| `ppc_new_pc_asm` | Translate EA->PA, call jitcNewPC, jump to native code |
| `ppc_new_pc_rel_asm` | Same-page relative branch (optimization) |
| `ppc_new_pc_this_page_asm` | Intra-page branch with backpatching |
| `ppc_heartbeat_ext_asm` | Check pending exceptions at page boundaries |
| `ppc_*_exception_asm` | Exception handlers (DSI, ISI, external, decrementer, etc.) |
| `ppc_mmu_tlb_invalidate_all_asm` | TLB flush |

### Register Allocation

The x86_64 JIT maps PPC registers to native registers dynamically:
- 16 GPRs available (RAX-R15, minus RSP)
- LRU-based allocation: least recently used native register gets spilled
- PPC CPU state lives on the stack, accessed via RSP-relative offsets
- At block boundaries, all dirty registers are flushed to memory

### Memory Access

- TLB: 32-entry per-type (code/data-read/data-write) software TLB
- Fast path in assembly: check TLB, load/store directly
- Slow path: call C++ MMU functions for page table walks

## AArch64 Differences from x86_64

### Instruction Set

| Aspect | x86_64 | AArch64 |
|--------|--------|---------|
| Encoding | Variable length (1-15 bytes) | Fixed 4 bytes |
| Registers | 16 GPR + 16 XMM | 31 GPR (X0-X30) + 32 SIMD (V0-V31) |
| Condition codes | FLAGS register, set implicitly | NZCV, set only by explicit flag-setting variants |
| Addressing | Complex (base+index*scale+disp) | Simpler (base+offset, base+reg) |
| PC-relative | RIP-relative addressing | ADRP+ADD for +-4GB, limited branch range |
| Branch range | JMP rel32 = +/-2GB | B = +/-128MB, conditional = +/-1MB |

### Register Mapping (Proposed)

AArch64 has 31 GPRs -- much more than x86_64's 16. Proposed allocation:

```
X0-X7    : Scratch / function arguments (caller-saved)
X8       : Indirect result register
X9-X15   : Temporary registers (caller-saved) -- use for PPC register cache
X16-X17  : IP0/IP1 (intra-procedure scratch, linker use) -- avoid
X18      : Platform register (reserved on macOS) -- DO NOT USE
X19-X28  : Callee-saved -- use for long-lived PPC register mappings
X29 (FP) : Frame pointer
X30 (LR) : Link register
SP       : Stack pointer (must be 16-byte aligned)
```

Key difference: X18 is **reserved on macOS** (platform register). Must not be used.

With 31 GPRs we can keep many more PPC registers resident in native registers, reducing spills significantly compared to x86_64.

### W^X on macOS ARM64

macOS ARM64 enforces Write XOR Execute (W^X). JIT code cannot be simultaneously writable and executable. The solution:

```c
// Allocate JIT memory
void *cache = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_JIT | MAP_ANON | MAP_PRIVATE, -1, 0);

// To emit code (per-thread):
pthread_jit_write_protect_np(0);   // writable, not executable
// ... emit native instructions ...
__builtin___clear_cache(start, end); // flush icache
pthread_jit_write_protect_np(1);   // executable, not writable

// Jump to generated code
```

**Verified working** on macOS ARM64 without special entitlements or code signing.

The toggle is **per-thread**, so the compilation thread can be in write mode while other threads execute previously generated code. PearPC's architecture (CPU thread separate from UI thread) fits this well.

### Instruction Cache Coherency

Unlike x86 where icache is coherent with dcache, ARM64 requires explicit cache maintenance:
- After writing code: `__builtin___clear_cache(start, end)`
- Or use `sys_icache_invalidate(start, size)` on macOS
- Must be done before toggling to execute mode

## Porting Strategy

### Phase 1: Minimal Proof of Concept

Goal: Execute a single PPC instruction via AArch64 JIT.

1. Create `src/cpu/cpu_jitc_aarch64/` directory
2. Implement minimal `aarch64asm.h/cc` -- emit MOV, ADD, LDR, STR, B, BL, RET
3. Implement `jitc.cc` with simplified translation cache (reuse page structure from x86_64)
4. Implement `ppc_cpu.cc` with `ppc_cpu_run()` entry point
5. Implement `jitc_tools.S` with:
   - `ppc_start_jitc_asm` -- entry from C++
   - `ppc_new_pc_asm` -- minimal translate-and-jump
   - `ppc_heartbeat_ext_asm` -- exception check stub
6. Implement a few PPC opcodes in `ppc_opc.cc`:
   - `addi` (add immediate) -- simplest ALU op
   - `b` / `bl` (branch) -- control flow
   - `sc` (system call) -- to exit cleanly

### Phase 2: Test Harness

A minimal PPC ELF (or raw binary) that exercises the JIT:

```asm
# test.ppc.S -- minimal PPC test
    .text
    .globl _start
_start:
    li      r3, 0       # r3 = 0
    li      r4, 10      # r4 = 10
loop:
    addi    r3, r3, 1   # r3++
    cmpw    r3, r4      # compare
    blt     loop        # loop if r3 < 10
    # r3 should be 10
    sc                  # system call (exit)
```

Load this directly into PearPC memory at a known address, set PC, and run. No PROM, no devices -- just the CPU loop.

### Phase 3: Core ALU/Branch Instructions

Implement enough opcodes to run simple programs:
- Integer ALU: `add`, `addi`, `sub`, `and`, `or`, `xor`, `slw`, `srw`, `cmp`
- Branch: `b`, `bl`, `bc`, `bclr`, `bcctr`
- Load/Store: `lwz`, `stw`, `lbz`, `stb`, `lhz`, `sth`
- System: `sc`, `mfspr`, `mtspr`, `mfcr`, `mtcrf`

### Phase 4: MMU and Full Bootstrap

- Port TLB fast path to AArch64 assembly
- Port exception handlers
- Boot Open Firmware

## Key Files to Create

```
src/cpu/cpu_jitc_aarch64/
├── Makefile.am
├── aarch64asm.h       # AArch64 instruction encoding + register defs
├── aarch64asm.cc      # Code emitter implementation
├── jitc.h             # JITC class (largely shared with x86_64)
├── jitc.cc            # Translation cache, page management
├── jitc_common.h      # CPU state offsets for assembly
├── jitc_tools.S       # Entry point, branch handling, exceptions
├── jitc_mmu.S         # TLB fast path
├── ppc_cpu.cc         # CPU init/run
├── ppc_opc.cc         # PPC opcode -> AArch64 code generation
├── ppc_alu.cc         # Integer ALU instruction generation
├── ppc_fpu.cc         # FPU instruction generation
├── ppc_vec.cc         # AltiVec instruction generation
├── ppc_mmu.cc         # MMU instruction generation
├── ppc_exc.cc         # Exception instruction generation
└── ppc_dec.cc         # Decrementer
```

## References

- [ARM Architecture Reference Manual (ARMv8-A)](https://developer.arm.com/documentation/ddi0487/latest)
- [Apple ARM64 ABI](https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms)
- Existing x86_64 JIT in `src/cpu/cpu_jitc_x86_64/`
