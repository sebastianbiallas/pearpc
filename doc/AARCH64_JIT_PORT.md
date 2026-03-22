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
- Most gen_ functions use GEN_INTERPRET (call C++ interpreter via BLR)
- 6 core load/store opcodes (lwz/stw/lbz/stb/lhz/sth) have native code generation with TLB fast path
- ALU immediate/register opcodes and branches have native code generation
- Load/store GEN_INTERPRET wrapper checks return value (not exception_pending) for DSI detection
- TLB slow-path DSI fix: all slow-path C functions return int (0=OK, 1=DSI), asm stubs check return value instead of exception_pending; async DEC/ext exceptions stay pending for heartbeat. Read slow paths store results in cpu->temp.
- emitBxxFixup fix: captures address AFTER emit32() to handle fragment overflow correctly
- Wide conditional fixups: `asmBccFixup`/`asmCBZwFixup`/`asmCBNZwFixup` emit inverted-condition + unconditional B to avoid ±1MB range limit across fragments
- dss/dstst cache hint opcodes are now registered (no-ops)
- ppc_opc_gen_invalid now prints `[JITC] WARNING: unknown opcode XXXXXXXX at pc_ofs=XXXX` to stderr at JIT compile time

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

**MMU stub clobber set (jitc_mmu.S):**

The asm read/write stubs (`PPC_STUB_READ_*`, `PPC_STUB_WRITE_*`) only clobber: `X0-X5, X9, X16, X17`. Registers **W6-W8, W10-W15** survive stub calls and can be used by generated code to hold values across calls.

- **W6**: Reserved for EA preservation in load/store-with-update codegen. The update variants (lwzu, stwu, etc.) save the effective address in W6 before the stub call, then store W6 to gpr[rA] after the stub returns. This avoids a memory round-trip through `cpu->temp2` and also fixes a correctness bug where the dword read stub's slow path would overwrite temp2.

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
- Conditional branches (`b.eq`, `b.ne`) have +/-1MB range and cannot reach external symbols or cross-fragment fixup targets. The fixup functions (`asmBccFixup`, `asmCBZwFixup`, `asmCBNZwFixup`) use inverted condition + unconditional `b` to avoid this limit.
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

All 200+ PPC opcodes generate AArch64 code that calls the C++ interpreter function via `BLR`. Load/store opcodes use a separate `ppc_opc_gen_interpret_loadstore()` wrapper that checks the function return value (not `exception_pending`) for DSI detection. The `ppc_opc_gen_interpret()` helper emits:
1. Store `current_opc`, `pc`, `npc` to CPU state
2. `MOV X0, X20` (first arg = CPU state pointer)
3. Load function address into X16
4. `BLR X16` (call interpreter)

This validates the entire JIT pipeline (page translation, fragment caching, W^X, branch handling) before optimizing individual opcodes.

Test results: `test_loop.elf` prints "Hello from PPC!" and counts 1-10. `test_alu.elf` passes all 22 ALU/memory/branch tests. `test_mem.elf` passes all 18 memory/TLB stress tests.

### Phase 4: Native Code Generation (IN PROGRESS)

Replace interpreter calls with actual AArch64 instructions. Each native gen_ function loads PPC registers from `[X20, #offset]`, performs the operation with AArch64 instructions, and stores the result back. 6 core load/store opcodes now have native code generation with inline TLB fast path (see below).

**Done - ALU/compare/rotate/SPR (native codegen, flowContinue):**
- Immediate: `addi`/`li`, `addis`/`lis`, `ori`, `oris`, `xori`, `xoris`, `andi.`, `andis.`, `addic`, `addic.`, `subfic`, `mulli`
- Register: `add`, `subf`, `and`, `or`, `xor`, `neg`, `mullw`, `mulhw`, `mulhwu`, `slw`, `srw`, `divw`, `divwu`, `andc`, `orc`, `nor`, `nand`, `eqv`, `sraw`, `srawi`, `cntlzw`, `extsb`, `extsh`
- Carry-aware: `addc`, `adde`, `addze`, `addme`, `subfc`, `subfe`, `subfze`, `subfme`
- Compare: `cmpi`, `cmpli`, `cmp`, `cmpl` — native CR update via BFI
- Rotate/mask: `rlwinm`, `rlwimi`, `rlwnm`
- SPR/CR: `mfcr`, `mtcrf`, `mcrxr`, `mfspr`, `mtspr`, `mftb`, `mcrf`, `mfmsr`, all CR logical ops

**Done - branches:**
- `b`/`bl` - unconditional branch (computes EA, sets LR, jumps to ppc_new_pc_asm)
- `bc` - conditional branch: decodes BO/BI/BD at JIT compile time, emits native condition test:
  - CR-only (beq/bne/blt/bgt/ble/bge): TBZ/TBNZ on CR bit (6-8 insn vs 22)
  - CTR-only (bdnz/bdz): SUBS+CBZ/CBNZ (8-10 insn)
  - Unconditional (BO=0x14): direct dispatch (4-5 insn)
  - Fallback: LK=1 or combined CTR+CR → interpreter

**Done - load/store with TLB fast path:**
- All base D-form: `lwz`, `stw`, `lbz`, `stb`, `lhz`, `sth`, `lha`
- All D-form with update: `lwzu`, `stwu`, `lbzu`, `stbu`, `lhzu`, `sthu`, `lhau`
- All X-form indexed: `lwzx`, `stwx`, `lbzx`, `stbx`, `lhzx`, `sthx`, `lhax`
- All X-form indexed with update: `lwzux`, `stwux`, `lbzux`, `stbux`, `lhzux`, `sthux`, `lhaux`
- Byte-reversed: `lwbrx`, `lhbrx`, `stwbrx`, `sthbrx` (native codegen with REV/REV16)
- Multiple word: `lmw`, `stmw` (unrolled for count ≤ 4, interpreter for larger)
- Reservation: `lwarx`, `stwcx.` (native codegen with wide conditional fixup for CR0 update)
- Update variants save EA to `cpu->temp2` before the asm stub call (which clobbers W0), then write EA to `gpr[rA]` after successful return. DSI never returns, so rA stays unmodified on exception.
- All return `flowContinue` — no dispatch overhead between load/store instructions.

**Done - stub pointer optimization:**
- All 15 asm stub calls (7 load/store, 2 branch dispatch, 4 exception, 2 TLB + gcard_osi) stored as function pointers in `PPC_CPU_State.stubs[]`
- `asmCALL_cpu(PPC_STUB_*)` emits `LDR X16, [X20, #offset]; BLR X16` — 2 instructions instead of `MOVZ+MOVK+MOVK+BLR` (4 instructions)
- Saves 2 instructions at ~58 call sites across the translation cache
- Precomputed size calculations updated to use `JITC::asmCALL_cpu_size` (8 bytes)

**Done - FP load/store (native codegen via TLB stubs):**

All FP load/store variants have native codegen:
- Double: `lfd`/`lfdu`/`lfdx`/`lfdux` — 64-bit load via `PPC_STUB_READ_DWORD`, store to FPR
- Double stores: `stfd`/`stfdu`/`stfdx`/`stfdux` — load FPR, 64-bit write via `PPC_STUB_WRITE_DWORD`
- Single loads: `lfs`/`lfsu`/`lfsx`/`lfsux` — 32-bit read via `PPC_STUB_READ_WORD` + `FMOV S0,W0` + `FCVT D0,S0` for IEEE single→double
- Single stores: `stfs`/`stfsu`/`stfsx`/`stfsux` — load FPR + `FCVT S0,D0` + `FMOV W1,S0` + 32-bit write via `PPC_STUB_WRITE_WORD`

All do `gen_check_fpu()` (MSR_FP check). The `checkedFloat` flag prevents redundant MSR checks across consecutive FP instructions in the same block.

**Done - FPU arithmetic (native with rounding-mode guard):**

AArch64 FADD/FSUB/FMUL/FDIV operate on IEEE 754 binary64 with exactly 53-bit mantissa — no extended precision mode (unlike x87). IEEE 754 guarantees correctly-rounded results, so PPC and AArch64 hardware produce bit-identical results for the same inputs and rounding mode. AArch64 FMADD is genuinely fused (single rounding), matching PPC fmadd semantics.

**Strategy:** When FPSCR[RN] == 0 (round-to-nearest, ~99.9% of code), use AArch64 FP instructions directly. When rounding mode != 0 or Rc=1, fall back to the C++ interpreter. FPSCR exception flags (XX, OX, UX, VX*) are skipped on the native path — these are sticky and rarely checked.

Native codegen in `ppc_fpu.cc`:
- Bit ops: `fmr`, `fneg`, `fabs`, `fnabs` — integer load, bit manipulation, store (always native, no FPSCR dependency)
- Double: `fadd`, `fsub`, `fmul`, `fdiv`, `fsqrt` — with rounding-mode guard
- Single: `fadds`, `fsubs`, `fmuls`, `fdivs` — double op + `FCVT S,D` + `FCVT D,S` sandwich
- FMA: `fmadd`→`FMADD`, `fmsub`→`FNMSUB`, `fnmadd`→`FNMADD`, `fnmsub`→`FMSUB` (PPC↔AArch64 mapping accounts for negation differences), plus single variants
- Convert: `frsp` (FCVT S,D + FCVT D,S), `fctiwz` (FCVTZS W,D + FMOV D,X)
- Select: `fsel` (FCMP D,#0 + FCSEL D,D,D,GE — no rounding dependency)
- Compare: `fcmpu`, `fcmpo` — native FCMP with CR update; NaN (B.VS) fallback uses wide conditional fixup for cross-fragment safety

**AltiVec:** All use interpreter calls. Not needed for boot.

#### Branch Dispatch Strategy

**`b`/`bl` (unconditional):** Compute target EA at translation time. Jump via `ppc_new_pc_asm` (full dispatch) or `ppc_new_pc_rel_asm`.

**`bc` (conditional, `gen_bcx`):** BO/BI/BD decoded at JIT compile time. Three native codegen paths:

- **CR-only** (BO & 4, ~90% of branches: beq/bne/blt/bgt/ble/bge): Load CR word, TBZ/TBNZ on the specific CR bit (bit position = 31-BI). If taken, dispatch via `ppc_new_pc_asm`. If not taken, fall through (`flowContinue`). **6-8 instructions** vs 22 with interpreter.
- **CTR-only** (BO & 0x10, bdnz/bdz): SUBS to decrement CTR, CBZ/CBNZ to test. **8-10 instructions**.
- **Unconditional** (BO & 0x14 == 0x14): Direct dispatch. **4-5 instructions**.
- **Fallback** (LK=1 or combined CTR+CR): Uses interpreter via `ppc_opc_gen_interpret` + `gen_dispatch_npc`.

No exceptions possible on branches, so no pc/npc/current_opc stores needed for native paths. All taken branches go through `ppc_new_pc_asm` which calls `ppc_heartbeat_ext_asm`, so heartbeat is always checked.

**`bclr`/`bcctr` (indirect):** Interpreter sets `npc` from LR or CTR. Dispatch via `gen_dispatch_npc` (`ppc_new_pc_asm`).

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
- Page-spanning accesses (e.g., 4-byte load at page offset 0xFFD): detected by the asm stub's page-offset check, handled byte-by-byte in the slow path with separate EA→PA translation per byte. PPC does NOT raise alignment exceptions for unaligned integer loads — they are architecturally valid.
- Only RAM pages cached in TLB; IO goes through slow path every time

**TLB invalidation:**
- Invalidate-all stores 0xFFFFFFFF (not 0 — page 0x00000000 was false-hitting)
- Single-entry invalidation implemented

### Phase 6: Same-Page Branch Optimization (DONE)

Conditional branches (`bc`) now use native TBZ/TBNZ/CBZ/CBNZ codegen (see Phase 4). Taken branches dispatch via `ppc_new_pc_asm` which handles heartbeat and page lookup. Not-taken branches fall through with zero overhead (`flowContinue`).

**Remaining bottleneck:** All taken branches and function call/return go through `ppc_new_pc_asm` → `jitcNewPC` → 64MB icache flush. The x86_64 JIT uses self-modifying code (backpatching) for this — on aarch64 with W^X, this requires toggling permissions.

**icache flush scope:** Currently flushing all 64MB on every `jitcNewPC`. Fragment-level flush was attempted but broke things (needs investigation — likely fragments spanning multiple allocations or the lookup-only case accessing stale fragment pointers). This is the biggest remaining performance issue.

### Phase 7: Register Caching

The x86_64 JIT maps frequently-used PPC GPRs to native registers, avoiding load/store to CPU state on every instruction. With AArch64's 31 GPRs (vs x86_64's 16), we can cache many more PPC registers:

- X21-X28 (8 callee-saved regs) for the 8 most-used PPC GPRs
- X9-X15 (7 caller-saved regs) for additional caching within blocks
- LRU-based allocation already implemented in `JITC::allocRegister()`

This is the biggest performance win after native code generation.

### Phase 8: Exception Handling (DONE)

PPC exceptions (DSI, ISI) are synchronous — they fire at the faulting instruction and dispatch to exception vectors (DSI=0x300, ISI=0x400). The kernel relies on these for demand paging.

#### How the three CPU backends handle DSI

**Generic CPU (interpreter):**
`ppc_effective_to_physical()` calls `ppc_exception(PPC_EXC_DSI, ...)` inline when the page table walk fails. `ppc_exception` sets SRR0=PC, SRR1=MSR, DAR, DSISR, clears MSR, sets `npc = 0x300`. The main loop does `pc = npc`, so the next instruction is the DSI handler. Returns `PPC_MMU_EXC`.

**x86_64 JIT:**
The full BAT/page-table walk is reimplemented in hand-written assembly (`jitc_mmu.S`, ~500 lines). On page fault, the assembly jumps directly to `ppc_dsi_exception_asm`. The C++ `ppc_effective_to_physical` just returns `PPC_MMU_FATAL` (no `ppc_exception` call) — it's only used for the interpreter fallback path.

**Our aarch64 JIT:**
`ppc_effective_to_physical()` calls `ppc_exception()` on page fault AND protection failure (matching generic CPU). Two dispatch paths:

1. **Asm stub path** (native gen_ lwz/stw/etc.): TLB fast path in asm → miss → C++ slow path → `ppc_effective_to_physical()` → `ppc_exception()` sets everything up → asm stub checks `exception_pending` → `.Ldsi_from_slow` clears flag, loads `npc`, dispatches via `ppc_new_pc_asm`. The native gen_ functions store `aCPU.pc` (full effective PC = `current_code_base + pc_ofs`) before the BLR so that `ppc_exception()` reads the correct SRR0.

2. **Interpreter path** (GEN_INTERPRET opcodes): `ppc_opc_gen_interpret()` emits code that calls the interpreter function, then checks `exception_pending`. If set: clears flag, loads `npc`, dispatches via `ppc_new_pc_asm`. The interpreter wrapper stores `pc` and `npc` before the call.

3. **ISI path** (code fetch): `ppc_effective_to_physical_code_c()` → `ppc_exception(ISI)` → returns EA → asm checks `exception_pending` → `.Lnewpc_isi` clears flag, loads `npc` (=0x400), dispatches via `ppc_new_pc_asm`.

#### Exception flag lifecycle

The `exception_pending`, `dec_exception`, `ext_exception` flags must be cleared at the right time:
- **DSI/ISI**: `ppc_exception()` sets `exception_pending`. The asm handler (`.Ldsi_from_slow`, `.Lnewpc_isi`) or the `ppc_opc_gen_interpret` post-check clears it before dispatching to the vector.
- **Decrementer/External**: The timer callback or PIC sets `exception_pending` + `dec/ext_exception` atomically. The heartbeat detects them and jumps to `ppc_dec/ext_exception_asm`. These handlers MUST clear both their specific flag AND `exception_pending` before dispatching — otherwise the heartbeat re-triggers the same exception on every check and the kernel never makes progress. (The x86 JIT uses `ppc_atomic_cancel_dec/ext_exception_macro` for this.)

#### icbi (Instruction Cache Block Invalidate)

The PPC `icbi` instruction tells the CPU that code at a given address has been modified and the instruction cache must be invalidated. In a JIT, this means destroying the translated code for that physical page.

Implementation: `ppc_opc_icbi()` translates EA→PA (with `PPC_MMU_NO_EXC` to avoid exceptions), looks up `jitc.clientPages[PA >> 12]`, and calls `jitcDestroyAndFreeClientPage()` if present. Uses `GEN_INTERPRET_ENDBLOCK` to end the current basic block after invalidation (prevents executing stale chained branches into the destroyed page).

Gotchas:
- Must check `aCPU.jitc != NULL` (the validation reference interpreter has no JITC)
- Must check `PA < gMemorySize` (I/O addresses have no client pages)
- `__builtin___clear_cache()` in `jitcNewPC` must flush per-fragment, not a contiguous range from result to tcp — fragments may be non-contiguous in the translation cache

### Phase 9: Full Bootstrap (IN PROGRESS)

**Progress:**
- Open Firmware PROM: boots fully
- Yaboot: full boot menu displayed, kernel loaded
- Kernel: boots through prom_init, device tree copy, display init, early init with interrupts
- Lock-step validation: **5.3 billion instructions** validated with zero mismatches
- Decrementer and external interrupt delivery working

**Current state (2026-03-21):**
Mandrake Linux PPC boots into the installer. The full kernel boot sequence completes: memory setup, PCI probing, CUDA/ADB, IDE, USB, SCSI, framebuffer console, TCP/IP, Unix sockets, ramdisk decompression, ext2 root mount, PCMCIA, DVD-ROM driver, ISO 9660 CD mount, and userspace init.

**Previously with validation enabled:**
The kernel reached a CUDA IFR polling loop and stalled. This was a validation limitation: the reference interpreter and JIT both read the same CUDA hardware, causing double I/O side effects (reading IFR clears flags, so the second read consumes the state change meant for the first).

**Remaining:**
- Investigate MSR 0x2340 (unsupported bits) — generic CPU never hits this, so it's likely a JIT bug causing wrong MSR values. Currently masked with a warning.
- Profile and optimize hot paths

### Decrementer Timer (sys_set_timer) on macOS

The PPC decrementer (DEC register) generates periodic interrupts. The emulator implements it via a host timer:

1. Kernel writes DEC register → `writeDEC()` → `sys_set_timer()` with one-shot interval
2. Timer fires → `decTimerCB()` → `ppc_cpu_atomic_raise_dec_exception()` → sets `exception_pending + dec_exception`
3. Heartbeat detects flags → dispatches to vector 0x900
4. Kernel's dec handler runs, writes new DEC value → back to step 1

**The three timer backends:**

- **Generic CPU**: No host timer. Decrements `pdec` every instruction inline. When `pdec==0`, sets exception flags directly. Precise but instruction-count-based, not wall-clock.
- **x86_64 JIT (Linux)**: Uses `timer_create` + `SIGEV_SIGNAL` (POSIX realtime timer). Reliable. Signal delivered to the process, handler fires.
- **aarch64 JIT (macOS)**: `timer_create` not available. Falls back to `setitimer(ITIMER_REAL)` + `SIGALRM`.

**The macOS setitimer problem:**

`setitimer` + `SIGALRM` is unreliable on macOS with multiple threads. A test program shows:
- Single-threaded busy loop: 160 fires in 2s at 10ms interval (expected 200) — works
- Multi-threaded (simulating SDL + JIT): 1 fire in 2s — broken
- `sleep()` + signal handler re-arm: 1 fire in 2s — broken

The issue: `SIGALRM` delivery on macOS is not guaranteed to reach the intended thread when multiple threads exist. PearPC has the CPU thread + SDL UI thread + CUDA event thread.

**Current fix: `dispatch_source` (GCD timer)**

On macOS, `sys_set_timer` uses `dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER)` instead of `setitimer`. This fires the callback on a dedicated GCD queue thread, independent of signal delivery. Each `writeDEC` cancels the old dispatch_source and creates a new one-shot.

**Memory ordering requirement:**

The GCD timer callback runs on a different thread than the JIT. The callback writes `exception_pending` via `ppc_cpu_atomic_raise_dec_exception` (ldaxr/stlxr with release semantics). The JIT's heartbeat reads it via `ldar` (acquire semantics). Without acquire/release, the JIT thread may never see the flags set by the timer thread (ARM64 weak memory ordering).

**The gCPU swap bug:**

The validation framework's `refStepOne()` temporarily swapped `gCPU` to point to the reference CPU state. If the timer callback fired during this window, it wrote `dec_exception` to the REFERENCE CPU instead of the JIT CPU. The exception was lost. Fix: don't swap `gCPU`, only swap `gMemory`. The interpreter functions already take `aCPU` as a parameter.

**readDEC / writeDEC:**

`readDEC` computes `DEC = gDECwriteValue - (ideal_timebase - gDECwriteITB)`. This is a wall-clock heuristic: DEC counts down at the ideal timebase rate. When the kernel reads DEC shortly after writing it, the value is close to what was written. The x86 JIT uses the identical formula.

The interpreter's `mfspr DEC` (case 22) was returning `aCPU.dec` directly without calling `readDEC()`. This returned stale values (usually 0). The x86 JIT's gen_ function calls `readDEC` but we use `GEN_INTERPRET(mfspr)` which goes through the interpreter. Fix: call `readDEC(aCPU)` before returning `aCPU.dec` in the interpreter's mfspr handler.

**Open issues:**

- The DEC interval computed by the kernel (0x26c90 ≈ 10ms at 16MHz) is correct for wall-clock time but much larger than what the generic CPU sees (0x53d), because generic counts instructions not wall-clock. This means the JIT gets ~100 timer interrupts/second while generic gets ~7000/second. This shouldn't prevent booting but makes init slower.
- `setitimer` fallback on non-macOS POSIX without `timer_create` may have similar multi-thread issues. Not tested.
- The dispatch_source cancel/recreate cycle on every `writeDEC` is suboptimal. Could use a single persistent timer and just update its fire time.

## Translation Cache and Fragment Design

The JIT translation cache is a single 64 MB block allocated at init with `MAP_JIT` (W^X). It is subdivided into **131,072 fragments** of 512 bytes each, managed as a free list. Each translated PPC page (4 KB, 1024 possible entrypoints) is represented by a **ClientPage** that owns a chain of one or more fragments. Up to 4,096 ClientPages can exist simultaneously.

### Fragment allocation and linking

When codegen for a PPC instruction exhausts the current fragment, `emit32()` calls `jitcEmitNextFragment()` which pops a fragment off the free list. If the new fragment is physically contiguous with the old one (within 20 bytes), `bytesLeft` is simply extended — no branch needed. Otherwise, an unconditional `B` is emitted at the end of the old fragment to link to the new one. 4 bytes are always reserved at the end of each fragment for this linking branch.

Because fragments come from a free list, two consecutive fragments in a ClientPage's chain can be megabytes apart in the 64 MB cache. This is why conditional branch fixups must use the wide pattern (inverted-condition + unconditional B) — a `B.cond` with ±1 MB range cannot safely span a fragment boundary, but unconditional `B` with ±128 MB always reaches within the 64 MB cache.

### Wide conditional fixups

AArch64 conditional branches (`B.cond`, `CBZ`, `CBNZ`) have ±1 MB range (imm19). Unconditional `B` has ±128 MB (imm26). Forward conditional branch fixups — where a branch is emitted with offset=0 and patched later via `asmResolveFixup` — can span fragment boundaries, making the target unreachable by a conditional branch.

The fixup functions (`asmBccFixup`, `asmCBZwFixup`, `asmCBNZwFixup`) solve this with a 2-instruction wide pattern:

```
emitAssure(8)    // CRITICAL: both instructions must be in the same fragment
B.inv_cond +8    // inverted condition, fixed offset, skips next insn if cond is FALSE
B 0              // unconditional placeholder, patched by asmResolveFixup
```

The `emitAssure(8)` is critical: the `B.inv_cond +8` has a fixed offset that assumes the `B` placeholder is exactly 4 bytes later. If a fragment boundary fell between the two instructions, the `+8` would jump into garbage in the old fragment instead of reaching the `B` in the new fragment.

The inverted-condition branch has a fixed +8 offset (never patched — it always skips exactly one instruction). When the original condition is TRUE, the inverted branch is not taken, so execution falls through to the unconditional `B` which jumps to the fixup target. When the original condition is FALSE, the inverted branch skips the `B` and execution continues after both instructions.

`asmResolveFixup` receives the address of the unconditional `B` and patches it with the correct offset. Since `B` has ±128 MB range, it always reaches within the 64 MB cache.

`asmBFixup` (unconditional) does not need this pattern — it already emits a `B` with ±128 MB range.

**jitc.log appearance:** The `B 0` placeholder shows as `14000000  b <fixup>` in jitc.log. This looks like an unresolved fixup but is expected — the log captures instructions at `emit32()` time, before `asmResolveFixup` patches the instruction word in memory. The patched value is only visible by disassembling the translation cache at runtime, not in the compile-time log. Thousands of these entries in the log during a kernel boot is normal.

### LRU eviction

When the free fragment list is empty, the **least-recently-used ClientPage** is destroyed and its fragments returned to the free list. ClientPages are kept in a doubly-linked LRU list; every `jitcNewPC` dispatch touches the accessed page (moves it to MRU end). When a new ClientPage is needed and none are free, 5 LRU pages are pre-evicted to reduce immediate re-eviction pressure.

### Cache invalidation triggers

The JIT cache is keyed by **physical address**, not effective address. This means TLB and segment register changes do not directly invalidate JIT code — only the PPC MMU's software TLB is flushed, and the next memory access refills it with the correct mapping.

Explicit JIT cache invalidation happens only via:
- **`icbi`** (Instruction Cache Block Invalidate): destroys the ClientPage containing the target physical address. This is how the kernel signals that it has overwritten code (e.g., module loading, self-modifying code).
- **LRU eviction**: when fragments or ClientPages run out, old pages are destroyed.

`tlbie`, `tlbia`, `mtsr`, `mtsrin` invalidate the PPC TLB but do **not** touch the JIT cache. Context switches in the guest OS change segment registers (which changes effective→physical mapping) but the JIT cache remains valid because it is indexed by physical address. The next code fetch re-translates the effective address through the new segment registers to find the (possibly different) physical page, which may already have a cached ClientPage.

### Statistics

Three counters are tracked and printed by `ppc_display_jitc_stats()`:

| Counter | Meaning |
|---------|---------|
| `destroy_write` | Pages destroyed by `icbi` (client wrote to code) |
| `destroy_oopages` | Pages destroyed because ClientPage pool was full |
| `destroy_ootc` | Pages destroyed because fragment free list was empty |

### Future research: is the fragment design actually needed?

The 512-byte fragment scheme was inherited from the x86_64 JIT (which itself was designed for 32-bit x86). It solves a real problem — without it, each ClientPage would need a contiguous allocation up to some worst-case size, leading to external fragmentation of the 64 MB cache. Fragments turn this into a linked-list allocator where any free 512-byte slot can be used.

But several questions are worth investigating:

1. **Do we ever fill the 64 MB cache?** There are no utilization metrics (peak fragments in use, high-water mark, free list depth over time). A Linux boot might only translate a few thousand pages. If the working set fits comfortably, the fragment scheme adds complexity (linking branches, fixup range issues) for a problem that may not exist in practice.

2. **What is the actual eviction rate?** The `destroy_ootc` / `destroy_oopages` counters exist but are rarely examined. If they stay near zero during a full boot, the cache is oversized and fragmentation is irrelevant. If they are high, the cache is pressure-limited and fragment size matters.

3. **When does guest context-switch invalidation happen?** The guest kernel changes segment registers on process context switches, but since the JIT cache is physical-address-keyed, these don't cause invalidation. Only `icbi` (module load, runtime code generation) and LRU pressure destroy pages. If the guest never issues `icbi` during normal boot, the cache is append-only until full.

4. **Could we use a bump allocator instead?** If the cache rarely fills up, a simple bump allocator (advance a pointer, no free list, no fragments) would eliminate linking branches, remove the fragment-boundary fixup problem entirely, and simplify the code. When the cache fills, invalidate everything and start over. This is what some simpler JITs do (LuaJIT's trace compiler, for example). The cost is that eviction is all-or-nothing rather than per-page LRU, but if pressure is rare, this is acceptable.

5. **Could fragments be larger?** If the bump allocator is too aggressive, increasing `FRAGMENT_SIZE` from 512 to e.g. 4096 bytes would make fragment boundaries rare enough that most single-instruction codegen never crosses one. The wide fixup pattern would still be needed for correctness, but the linking branch overhead (one `B` per fragment boundary) drops proportionally.

6. **What is the code density?** How many native bytes does the average PPC instruction compile to? If it's ~20 bytes (typical for a load with TLB stub call), a 4 KB PPC page (1024 instructions) generates ~20 KB of native code = ~40 fragments. With 131K fragments available, that's ~3,200 pages before eviction — close to the 4,096 ClientPage limit. Actual density measurements would inform both fragment size and cache size decisions.

Adding instrumentation (`jitc.peakFragmentsUsed`, `jitc.totalFragmentsAllocated`, histograms of fragments-per-page) would answer most of these questions with a single boot run.

## Lessons Learned

1. **Silent no-op stubs are fatal bugs in disguise.** `lmw`/`stmw` being empty stubs meant callee-saved registers were never saved/restored. `icbi` being a no-op meant the JIT code cache was never invalidated after code was overwritten. Every unimplemented opcode must abort with an error message.

2. **Don't blindly copy x86_64.** The TLB fast path works better as "asm check + C++ slow path" than reimplementing BAT/page-walk in assembly. For branches, we initially used "interpreter eval + native dispatch" but ultimately replaced it with fully native TBZ/TBNZ/CBZ/CBNZ codegen — the interpreter overhead (22 insn storing pc/npc/opc + calling C++) dominated when the actual condition check is just 1-2 instructions.

3. **Tracing is essential for JIT debugging.** `jitc_trace.log` logs every `jitcNewPC` dispatch with CPU state. This found both the BSS loop bottleneck and the r30 register corruption. Flush frequently — buffered output is lost on timeout/crash.

4. **Test ELFs run with MMU on** (MSR=0x2030). The PROM sets up page tables. Addresses outside mapped regions fail with PPC_MMU_FATAL, which looks like a JIT bug but is actually a test bug.

5. **PPC exceptions are core execution semantics, not error handling.** The kernel relies on page faults (DSI/ISI) for demand paging. The generic CPU handles them inline via `ppc_exception()` which modifies `npc`. The JIT must do the same — either inline or via exception_pending + immediate check.

6. **Lock-step validation is invaluable and fast enough.** Running a reference interpreter alongside the JIT, comparing registers after every instruction, at billions of instructions without issue. Key design: shared memory, compare GPRs + SPRs + BATs + SRs. PROM calls resync caller-saved only (r0-r13, CR, LR, CTR, XER); callee-saved r14-r31 mismatches are real bugs. I/O reads (PA >= gMemorySize) and volatile SPR reads (mftb, mfspr) cause benign mismatches — resync on those. **Do not try to skip the reference step for I/O — this corrupts the reference PC state and causes false mismatches.** The validation overhead is acceptable; the kernel stalls not because validation is slow but because both paths read the same I/O devices.

7. **Understand the three-way architecture.** Generic CPU, x86_64 JIT, and aarch64 JIT handle exceptions differently. Always check all three before implementing. The generic CPU is the reference for correctness; the x86_64 JIT shows how to do it in a JIT context.

8. **One missing line can cause spectacular failures that look like something else entirely.** The root cause of the kernel boot crash was `ibat_nbl[idx] = ~ibat_bl[idx]` missing from `ppc_opc_batu_helper()` for the IBAT case (the DBAT case had it). Without this, `addr &= ibat_nbl[i]` zeroed the entire address during BAT translation, causing every kernel virtual address to map to PA 0. This manifested as "stale JIT code cache" because PA 0 had old translations — but the real bug was the address translation itself. The icbi investigation was a red herring twice: first because the translations at PA 0 were correctly cached (from wrong translations), and second because icbi was working correctly (the page was already evicted when icbi fired).

9. **When a symptom looks like X, verify X before implementing a fix for X.** The symptom "JIT executes old rfi instead of current nop at PA 0" looked like a stale code cache problem. icbi was implemented to fix it. But the real question was: *why does c0003c58 translate to PA 0?* If we had checked the BAT translation output directly instead of assuming code cache staleness, we would have found the ibat_nbl bug immediately. Always verify the full chain: EA → BAT/page-table → PA → code cache lookup. Don't assume the last step is broken when an earlier step might be wrong.

10. **Exception handlers must clean up their trigger flags.** The decrementer and external exception handlers must clear `dec_exception`/`ext_exception` AND `exception_pending` before dispatching to the exception vector. Without this, the heartbeat re-detects the same pending exception on every basic block boundary, causing an infinite loop of exception delivery. The kernel appeared to "stall" but was actually re-entering the same exception handler endlessly. The x86 JIT uses atomic cancel macros for this; our simpler `strb wzr` works because the handler runs with MSR_EE=0 (no re-entrant interrupts).

11. **Shared I/O state is the Achilles' heel of lock-step validation.** The JIT and reference interpreter share the same I/O devices (CUDA, PIC, etc.). I/O registers have side effects on read — reading CUDA IFR can clear interrupt flags, reading the shift register advances the CUDA state machine. With both paths reading the same register, the state advances twice as fast and flags get consumed by the wrong reader. This cannot be fixed by skipping the reference step (breaks PC tracking) or syncing state after (damage already done). The correct fix would be I/O read result caching (return the JIT's result to the reference without a second read) or separate I/O device instances.

12. **Fragment-based icache flush is necessary.** The JIT translation cache uses non-contiguous fragments linked by branch instructions. `__builtin___clear_cache(result, tcp)` crashes if the range spans a gap between fragments. Use `jitcFlushClientPage()` which walks the fragment chain and flushes each fragment individually.

13. **AArch64 conditional branches have limited range — never use them for cross-fragment fixups.** CBZ/CBNZ have ±1MB range (19-bit signed immediate). B.cc has ±1MB. Unconditional B has ±128MB (26-bit). When a forward fixup in generated code spans emitted instructions that include `emitBLR` calls (16 bytes each), the total distance can exceed 1MB if a fragment boundary falls in between. The `& 0x7FFFF` mask silently wraps the offset, and bit 18 being set makes the CPU interpret it as a large negative offset — branching far backward into unrelated compiled code. The symptom is a SIGSEGV at a small address like `0x328` or `0x384` (field offsets in `PPC_CPU_State` accessed via a NULL pointer). **Fixed:** All conditional fixup functions (`asmBccFixup`, `asmCBZwFixup`, `asmCBNZwFixup`) now emit a 2-instruction wide fixup: an inverted-condition branch that skips +8 bytes, followed by an unconditional `B` placeholder (±128MB range). `asmResolveFixup` patches the unconditional B, which is always in range. This is the same pattern used by .NET CoreCLR for long AArch64 branches. Cost: 4 extra bytes per fixup site.

14. **Don't inline TLB lookups into JIT output.** The TLB fast path (~6 instructions) lives in a shared asm stub called via BLR. Inlining it into every load/store site was attempted and reverted: each site bloated from ~12 to ~30 instructions, wasting translation cache and host icache. The x86 JIT had the same inline code written and `#if 0`'d it out. One shared copy stays hot in icache; hundreds of inline copies compete with everything else.

15. **Print all host registers in the crash handler.** The default crash handler only printed pc/lr/sp, which made it impossible to distinguish "X20 corrupted" from "X0 not set from X20". Adding all x0-x28 registers plus the instruction word at the faulting PC immediately revealed the true bug: X20 was valid but X0 was 0. De-sliding the faulting PC with ASLR offset (`nm` address vs crash address of a known symbol) identified the exact C++ function being called with wrong arguments.

16. **CR field encoding is 9 instructions and hard to shrink.** `cmpw`/`cmpwi` (~70k occurrences) encode a PPC CR nibble `{LT=8, GT=4, EQ=2, SO=1}` from AArch64 NZCV flags. The current sequence:

    ```
    cmp   w16, w17           ; set NZCV
    cset  w0, ne             ; EQ=0, NE=1
    csinc w0, w0, w0, ge     ; EQ=0, GT=1, LT=2
    movz  w1, #2
    lslv  w0, w1, w0         ; 2<<index = {2,4,8}
    ldr   w1, [x20, #xer]
    add   w0, w0, w1, lsr #31  ; OR in SO bit
    ldr   w1, [x20, #cr]
    bfi   w1, w0, #shift, #4   ; insert nibble
    str   w1, [x20, #cr]
    ```

    The `MOVZ+LSLV` (2 insns) computes `2 << index` where index ∈ {0,1,2} → {2,4,8}. Replacing with `LSL W0, W0, #2` (1 insn) computes `index << 2` = {0,4,8} — wrong for EQ (produces 0 instead of 2). The bit positions {8,4,2} = {2^3, 2^2, 2^1} are powers of 2 but not uniformly spaced from any linear index, so a constant shift always breaks one case. Compensating (e.g. CSET+ADD for the EQ bit) costs ≥1 extra instruction, negating the saving. Reading NZCV directly via `MRS` doesn't help either: N is at bit 31 and Z at bit 30, but PPC has LT at bit 3 and EQ at bit 1 — the positions don't align, and GT (=!N&!Z) has no direct flag bit. The real win would be lazy CR evaluation (keep NZCV flags live and only materialize the CR nibble when something reads it), but that requires tracking flag liveness across basic blocks.

## Testing Strategy

**Goal: 100% test coverage of all opcodes with native JIT codegen.**

Every opcode that has a native `gen_` function (as opposed to `GEN_INTERPRET` / `GEN_INTERPRET_LOADSTORE`) must be exercised by at least one test ELF. This ensures that JIT codegen bugs are caught by the test suite, not by a kernel boot failure weeks later.

The test infrastructure is described in `test/README.md`. Tests are bare-metal PPC assembly programs that run on the emulated CPU with MMU enabled, using custom opcodes for I/O (`0x00333303` print, `0x00333304` exit). Each test checks results inline and reports pass/fail.

### Current coverage

| Test ELF | Opcodes covered |
|----------|----------------|
| `test_alu.S` | ALU arithmetic/logic, shifts, rotates, compares, SPR, multiply, divide, byte-reversed load/store, lwarx/stwcx. |
| `test_mem.S` | Word/half/byte load/store, multi-page stride, lmw/stmw |
| `test_dsi.S` | DSI exception handling through lwz/stw/sth/stb/lhz/lbz |
| `test_branch_loop.S` | b/bl/blr/bctr/bctrl/blrl, conditional branches (beq/bne/blt/bgt/ble/bge), bdnz/bdz |
| `test_fpu_arith.S` | 48 tests: fabs, fnabs, fadd/fsub/fmul/fdiv (double+single), fmadd/fmsub/fnmadd/fnmsub (double+single), fsqrt, fcmpu, frsp, fctiwz, fsel, lfs/stfs/lfsu/stfsu, FPSCR rounding modes (mffs, mtfsfi, all 4 RN modes) |
| `test_fpu_exc.S` | FPU exception handling (NO_FPU when MSR_FP=0), fmr, fneg |
| `test_altivec.S` | AltiVec enable, vector ALU/compare/splat/merge/load/store |
| `test_crlogical.S` | crand/crandc/cror/crorc/crxor/crnand/crnor/creqv |

### Coverage gaps (native codegen but no dedicated test)

**ALU:** andc, eqv, nand, extsb, extsh, mulhw, rlwnm, addc, addze, addme, subfc, subfze, subfme, cmpl, cmpli

**Load/store:** All update variants (lwzu, stwu, lbzu, stbu, lhzu, sthu, lhau and their X-form equivalents), lha/lhax, lhbrx, sthbrx, FP indexed variants (lfdx, stfdx, lfdu, stfdu, lfsx, stfsx, etc.)

**Branch:** bc with LK=1 + CR condition, bc with combined CTR+CR

**SPR/system:** mfsr, mtsr, mfsrin, mtsrin, tlbie

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

## XER Summary Overflow (SO) in CR Packing

### Background

The PPC architecture specifies that every CR update (compares, Rc=1 ALU ops)
must copy the SO (Summary Overflow) bit from XER into bit 0 of the CR field.
This means every compare and every Rc=1 instruction must read XER.

In the aarch64 JIT, this costs 2 instructions per CR update:

```
ldr w1, [x20, #xer]          ; load XER
add w0, w0, w1, lsr #31      ; OR in SO bit (bit 31 → bit 0)
```

With ~102K compare/Rc=1 instructions per boot, this is ~204K wasted
instructions (1.8% of total JIT output).

### Which instructions set SO?

SO is set by the "o" (overflow) variants of ALU instructions. It is a
**sticky** bit — once set, it stays set until explicitly cleared by `mtspr XER`
or `mcrxr`.

**Instructions that set XER[SO] and XER[OV]:**
- `addox`, `addcox`, `addeox`, `addmeox`, `addzeox`
- `subfox`, `subfcox`, `subfeox`, `subfmeox`, `subfzeox`
- `mullwox` (multiply overflow)
- `divwox`, `divwuox` (divide overflow)
- `negox`

**None of these are currently implemented** in PearPC — they all call
`PPC_ALU_ERR("...ox unimplemented")`. This means SO is never set in practice.

### Which instructions read SO?

**In the CR packing path (native JIT codegen):**
- `cmp`, `cmpi`, `cmpl`, `cmpli` — all four compare instructions
- All Rc=1 instructions (`add.`, `and.`, `or.`, `andi.`, etc.) via `gen_update_cr0`

**In interpreter functions:**
- `ppc_update_cr0()` — reads `xer & XER_SO` to set CR0[SO]
- Compare interpreter functions — same

**Other readers:**
- `mfspr XER` — reads the full XER register (returns SO/OV/CA bits)
- `mcrxr` — moves XER[SO/OV/CA] to a CR field (currently unimplemented)
- `mfcr` — reads the full CR (which contains SO bits already packed)

### How x86_64 JIT handles this

The x86_64 JIT defines `HANDLE_SO` as an **empty macro** (unless `EXACT_SO`
is defined at compile time):

```asm
#ifndef EXACT_SO
#define HANDLE_SO          // no-op — skip SO entirely
#else
#define HANDLE_SO test byte ptr [xer+3], 1<<7; jnz 4f
#endif
```

This means the x86_64 JIT **never** copies SO into CR fields. Since no
overflow instruction is implemented, this is safe and saves significant
overhead.

### Optimization plan

**Option A (minimal, safe):** Remove the XER load from `gen_cr_insert_signed`
and `gen_cr_insert_unsigned`. Since no "o" instruction is implemented, SO is
always 0, making the load + ADD a guaranteed no-op. If an "o" instruction is
later implemented, add SO handling back.

**Option B (runtime config):** Add a config option `ppc_exact_so` (default
off). When off, skip SO in JIT codegen. When on, emit the XER load. This
matches the x86_64 `EXACT_SO` compile-time flag but makes it runtime
configurable, so users running software that depends on overflow detection
can enable it.

**Option C (lazy SO):** Track whether any "o" instruction has been compiled
in the current translation cache. If not, skip SO in all CR packing. If one
is compiled, invalidate the cache and recompile with SO enabled. This is
automatic but complex.

Option A is recommended for now, with Option B as a future enhancement when
"o" instructions are implemented.

### Impact

Removing the SO load saves 2 instructions per CR update:
- ~102K compare/Rc=1 instructions × 2 = **~204K instructions saved (1.8%)**
- Also eliminates a load-use dependency stall on `w1` in the hot path

## References

- [ARM Architecture Reference Manual (ARMv8-A)](https://developer.arm.com/documentation/ddi0487/latest)
- [Apple ARM64 ABI](https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms)
- Existing x86_64 JIT in `src/cpu/cpu_jitc_x86_64/`
- ELF loader in `src/io/prom/promboot.cc` (`mapped_load_elf`)
