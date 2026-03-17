# AArch64 JIT Port

This document describes the plan for porting PearPC's JIT compiler to AArch64 (ARM64), targeting macOS on Apple Silicon.

## Current Status

The aarch64 JIT backend compiles and links on macOS ARM64 (`aarch64-jit` branch).

**Done:**
- Build system: `configure --enable-cpu=jitc_aarch64` works
- Translation cache with W^X support (MAP_JIT + pthread_jit_write_protect_np)
- AArch64 instruction encoder (aarch64asm.cc/h)
- Assembly bootstrap (jitc_tools.S): entry, PC lookup, heartbeat, exception handlers
- MMU stub (jitc_mmu.S): identity mapping, memory access stubs
- Full opcode dispatch tables (ppc_dec.cc)
- All 143 PPC interpreter opcode implementations (copied from x86_64, platform-independent C++)
- All gen_ functions return flowEndBlockUnreachable (no native code generation yet)

**Next:** Get the first PPC instructions executing through the JIT loop.

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

### Register Mapping

AArch64 has 31 GPRs -- much more than x86_64's 16. Current allocation:

```
X0-X7    : Scratch / function arguments (caller-saved)
X8       : Indirect result register (caller-saved)
X9-X15   : Temporaries (caller-saved) -- future PPC register cache
X16-X17  : IP0/IP1 intra-procedure scratch -- used by emitBLR/emitMOV64
X18      : Platform register (reserved on macOS) -- DO NOT USE
X19      : JITC pointer (callee-saved, set once in ppc_start_jitc_asm)
X20      : CPU state pointer (callee-saved, set once in ppc_start_jitc_asm)
X21-X28  : Callee-saved -- future PPC register cache
X29 (FP) : Frame pointer
X30 (LR) : Link register
SP       : Stack pointer (must be 16-byte aligned)
```

**Important conventions for generated code:**

- **X16-X17**: Used by `emitBLR()` / `emitMOV64()` to load function addresses. Any `emitBLR` clobbers X16. Generated code must not rely on X16/X17 across BLR calls.
- **X0-X7, X9-X15**: Clobbered by any BLR call (caller-saved per AArch64 ABI). This includes calls to asm helpers like `ppc_heartbeat_ext_rel_asm`, `ppc_new_pc_this_page_asm`, `ppc_read/write_effective_*_asm`.
- **X19-X28**: Preserved across BLR calls (callee-saved). X19 and X20 are reserved. X21-X28 are available for values that must survive across helper calls (e.g. branch target offsets). Currently used as scratch by `ppc_new_pc_this_page_asm` (X21).
- **CPU state** (`[X20, #offset]`): Always accessible. Gen_ functions read/write PPC registers via `emitLDR32_cpu` / `emitSTR32_cpu`. This is memory, not a register, so it survives all calls.

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

**Performance:** ~27 ns per toggle (~20 ns W->X, ~34 ns X->W). Apple implements this as a fast MSR instruction flipping APRR permission bits, not a kernel trap. We toggle twice per new page translation (write on, then execute on). Once code is cached, zero toggles until a new page needs translating. Essentially free.

### Instruction Cache Coherency

Unlike x86 where icache is coherent with dcache, ARM64 requires explicit cache maintenance:
- After writing code: `__builtin___clear_cache(start, end)`
- Or use `sys_icache_invalidate(start, size)` on macOS
- Must be done before toggling to execute mode

### AArch64 Assembly Gotchas (macOS)

- `.globl sym; sym:` on one line via semicolon does NOT work. Must use `.macro do_export` with `.globl` and label on separate lines.
- Logical immediates (`and`, `orr`, `eor` with #imm) can only encode repeating bit patterns. Constants like `0x87c0ffff` must be loaded into a temp register first.
- Conditional branches (`b.eq`, `b.ne`) have +/-1MB range and cannot reach external symbols. Use inverted condition + unconditional `b` instead.
- X18 is reserved on macOS. Using it will corrupt the platform's TLS.

## Porting Strategy

### Phase 1: Infrastructure (DONE)

All files created, compiles and links on macOS ARM64.

### Phase 2: Test with ELF Loading (DONE)

PearPC's built-in ELF loader (`mapped_load_elf()` in `src/io/prom/promboot.cc`) loads PPC ELFs via config:

```
prom_bootmethod = "force"
prom_loadfile = "test/test_loop.elf"
```

Cross-compile PPC ELFs using Docker:
```sh
./test/build_ppc_elf.sh
```

Run headless:
```sh
./src/ppc --headless test/test_loop.cfg
```

Custom opcodes for test I/O (defined in `ppc_dec.cc ppc_opc_special`):
- `0x00333303` - print string (r3=addr, r4=length)
- `0x00333304` - exit (r3=exit code)

### Phase 3: Naive Opcode Execution (DONE)

All 200+ PPC opcodes generate AArch64 code that calls the C++ interpreter function via `BLR`. The `ppc_opc_gen_interpret()` helper emits:
1. Store `current_opc`, `pc`, `npc` to CPU state
2. `MOV X0, X20` (first arg = CPU state pointer)
3. Load function address into X16
4. `BLR X16` (call interpreter)

This validates the entire JIT pipeline (page translation, fragment caching, W^X, branch handling) before optimizing individual opcodes.

Test results: `test_loop.elf` prints "Hello from PPC!" and counts 1-10. `test_alu.elf` passes all 22 ALU/memory/branch tests. `test_mem.elf` passes all 18 memory/TLB stress tests.

### Phase 4: Native Code Generation (IN PROGRESS)

Replace interpreter calls with actual AArch64 instructions. Each native gen_ function loads PPC registers from `[X20, #offset]`, performs the operation with AArch64 instructions, and stores the result back.

**Done - ALU immediate:**
- `addi`/`li` - immediate add (handles rA==0 for load immediate)
- `addis`/`lis` - immediate add shifted
- `ori`, `oris`, `xori`, `xoris` - OR/XOR immediate variants
- `cmpi` - compare immediate (falls back to interpreter for CR update)

**Done - ALU register:**
- `add`, `subf`, `and`, `or`, `xor` - register-register operations
- `neg`, `mullw` - negate, multiply
- `slw`, `srw` - shifts
- `rlwinm` - rotate and mask (interpreter fallback)

**Done - branches:**
- `b`/`bl` - unconditional branch (computes EA, sets LR, jumps to ppc_new_pc_asm)
- `bc` - conditional branch: uses interpreter for condition eval, then checks npc to dispatch. For same-page not-taken, continues without dispatch overhead.

**Done - load/store with TLB fast path:**
- `lwz`, `stw`, `lbz`, `stb`, `lhz`, `sth` - native gen_ functions that call asm TLB stubs
- All indexed forms (`lwzx`, `stwx`, `lbzx`, `stbx`, `lhzx`, `sthx`) and update forms implemented in interpreter

**Not yet optimized:**
- `andi.`, `andis.` - AND immediate with CR0 update
- `subfic`, `addic`, `addic.`, `mulli` - other immediates
- `divwu`, `divw`, `sraw`, `srawi` - divide, arithmetic shift
- `rlwimi`, `rlwnm` - rotate and mask variants
- `extsb`, `extsh`, `cntlzw` - extend/count
- `cmpi`, `cmpli`, `cmp`, `cmpl` - native CR update (currently interpreter)

**FPU load/store (needed for yaboot):**

Yaboot uses FPU load/store in function prologues to save/restore callee-saved FP registers. No actual FP computation — just memory transfers. The opcodes are:

- `stfd`/`stfdu` - store float double (8 bytes from fpr[] to memory)
- `lfd`/`lfdu` - load float double (8 bytes from memory to fpr[])
- `stfs`/`stfsu` - store float single (convert double→single, store 4 bytes)
- `lfs`/`lfsu` - load float single (load 4 bytes, convert single→double)
- Indexed variants: `stfdx`, `lfdx`, `stfsx`, `lfsx`, etc.

Implementation: copy from the generic CPU's `ppc_mmu.cc`. The functions are simple — check MSR_FP (raise exception if FPU disabled), compute EA, read/write memory via `ppc_read/write_effective_dword/word`. The double values are stored in the `fpr[32]` array as host `uint64` (raw IEEE 754 bits). All go through `GEN_INTERPRET`.

**FPU arithmetic (not needed for yaboot, needed for kernel):**

All FPU arithmetic opcodes (fadd, fmul, fdiv, fsqrt, fmadd, etc.) are implemented in `ppc_fpu.cc` using host `double` arithmetic. These already exist in the aarch64 port but are only reachable through the interpreter.

**AltiVec:** All use interpreter calls. Not needed for boot.

#### Branch Dispatch Strategy

All branch gen_ functions use the interpreter to evaluate the branch condition (handles all BO variants, CTR decrement, LK bit). The optimization is in **how the target is dispatched**:

**`b`/`bl` (unconditional):** Compute target EA at translation time. Jump via `ppc_new_pc_asm` (full dispatch) or `ppc_new_pc_rel_asm`.

**`bc` (conditional, `gen_bcx`):** Interpreter sets `npc`. Generated code compares `npc` with the expected fall-through address (`current_code_base + pc + 4`):
- **Not taken** (`npc == fall-through`): patched `B.EQ` skips the dispatch, continues to next translated instruction (`flowContinue`).
- **Taken, backward same-page, entry exists**: heartbeat + direct `BR` to the known native address. No dispatch overhead at all.
- **Taken, same-page, entry not yet known**: heartbeat + `ppc_new_pc_this_page_asm` (fast lookup in ClientPage entrypoints array, no `jitcNewPC`).
- **Taken, cross-page**: `ppc_new_pc_asm` (full dispatch).

**`bclr`/`bcctr` (indirect):** Interpreter sets `npc` from LR or CTR. Generated code checks if `npc` is on the current page:
- **Same page** (`npc - current_code_base < 4096`): heartbeat + `ppc_new_pc_this_page_asm`.
- **Cross page**: `ppc_new_pc_asm`.

**`ppc_new_pc_this_page_asm`** (assembly helper): Translates EA→PA, then directly indexes `clientPages[PA >> 12]->entrypoints[(PA & 0xFFF) >> 2]` to get the native address. No `jitcNewPC` call, no icache flush, no W^X toggle. Falls back to `jitcNewPC` only if the entrypoint doesn't exist.

**`ppc_new_pc_asm`** (assembly helper): Full dispatch — heartbeat check, EA→PA translation, `jitcNewPC` (page lookup, possibly translate, icache flush, W^X toggle). Used for cross-page branches.

### Phase 5: MMU Fast Path (DONE)

TLB-based memory access with assembly fast path + C++ slow path.

**Assembly fast path (jitc_mmu.S):**
- 10 stubs: read byte/half_z/half_s/word/dword, write byte/half/word/dword
- Inline TLB lookup: hash EA → 32-entry table, compare page tag (~6 instructions on hit)
- Byte swap: REV/REV16 for big-endian guest on little-endian host
- Page-cross detection: falls through to slow path

**C++ slow path (ppc_mmu.cc):**
- On TLB miss: EA→PA translation via `ppc_effective_to_physical()`, fill TLB, access memory
- Only RAM pages cached in TLB; IO goes through slow path every time

**TLB invalidation:**
- Invalidate-all stores 0xFFFFFFFF (not 0 — page 0x00000000 was false-hitting)
- Single-entry invalidation implemented

### Phase 6: Same-Page Branch Optimization (DONE for backward branches)

For backward same-page `bc` branches (tight loops), the generated code now jumps directly to the already-translated native entry point via `BR X16`. Each loop iteration: interpreter evaluates condition → heartbeat check → direct jump. No `jitcNewPC`, no icache flush.

**Remaining bottleneck:** Forward branches and function call/return still go through `ppc_new_pc_asm` → `jitcNewPC` → 64MB icache flush. The x86_64 JIT uses self-modifying code (backpatching) for this — on aarch64 with W^X, this requires toggling permissions.

**icache flush scope:** Currently flushing all 64MB on every `jitcNewPC`. Fragment-level flush was attempted but broke things (needs investigation — likely fragments spanning multiple allocations or the lookup-only case accessing stale fragment pointers). This is the biggest remaining performance issue.

### Phase 7: Register Caching

The x86_64 JIT maps frequently-used PPC GPRs to native registers, avoiding load/store to CPU state on every instruction. With AArch64's 31 GPRs (vs x86_64's 16), we can cache many more PPC registers:

- X21-X28 (8 callee-saved regs) for the 8 most-used PPC GPRs
- X9-X15 (7 caller-saved regs) for additional caching within blocks
- LRU-based allocation already implemented in `JITC::allocRegister()`

This is the biggest performance win after native code generation.

### Phase 8: Full Bootstrap (IN PROGRESS)

**Progress:**
- Open Firmware PROM: boots, detects Apple Partition Map on Mandrake ISO, loads yaboot ELF
- Yaboot: BSS clear, palette setup, PROM device discovery (finddevice, getprop for CD)

**Remaining:**
- Continue yaboot boot (load kernel from CD)
- Boot Mandrake Linux PPC from CD ISO
- Profile and optimize hot paths

## Lessons Learned

1. **Silent no-op stubs are fatal bugs in disguise.** `lmw`/`stmw` being empty stubs meant callee-saved registers were never saved/restored. Symptom: loop counter stuck. Cause: completely unrelated opcode. Every unimplemented opcode must abort with an error message.

2. **Don't blindly copy x86_64.** The TLB fast path works better as "asm check + C++ slow path" than reimplementing BAT/page-walk in assembly. The bcx works better as "interpreter eval + native dispatch" than fully native condition checking.

3. **Tracing is essential for JIT debugging.** `jitc_trace.log` logs every `jitcNewPC` dispatch with CPU state. This found both the BSS loop bottleneck and the r30 register corruption. Flush frequently — buffered output is lost on timeout/crash.

4. **Test ELFs run with MMU on** (MSR=0x2030). The PROM sets up page tables. Addresses outside mapped regions fail with PPC_MMU_FATAL, which looks like a JIT bug but is actually a test bug.

## Key Files

```
src/cpu/cpu_jitc_aarch64/
├── Makefile.am
├── aarch64asm.h/cc    # AArch64 instruction encoding
├── jitc.h/cc          # Translation cache, page management, W^X
├── jitc_common.h      # CPU state offsets for assembly
├── jitc_asm.h         # C++ declarations for assembly functions
├── jitc_tools.S       # Entry point, branch handling, exceptions
├── jitc_mmu.S         # TLB fast path (stub), memory access stubs
├── jitc_debug.h/cc    # Debug logging
├── jitc_types.h       # Type definitions
├── ppc_cpu.h/cc       # CPU state struct, init, run
├── ppc_dec.h/cc       # Opcode dispatch tables, decoder
├── ppc_alu.h/cc       # ALU interpreter + gen_ stubs
├── ppc_fpu.h/cc       # FPU interpreter + gen_ stubs
├── ppc_vec.h/cc       # AltiVec gen_ stubs
├── ppc_mmu.h/cc       # MMU translation, load/store interpreter
├── ppc_opc.h/cc       # Branch/SPR/CR/misc interpreter + dispatch
├── ppc_exc.h/cc       # Exception handling
├── ppc_esc.h          # Escape opcode declarations
└── ppc_tools.h        # Portable helpers (carry, rotate)
```

## References

- [ARM Architecture Reference Manual (ARMv8-A)](https://developer.arm.com/documentation/ddi0487/latest)
- [Apple ARM64 ABI](https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms)
- Existing x86_64 JIT in `src/cpu/cpu_jitc_x86_64/`
- ELF loader in `src/io/prom/promboot.cc` (`mapped_load_elf`)
