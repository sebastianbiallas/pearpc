# Abstract PPC Instruction Semantics

## Motivation

PearPC currently has three representations of PPC instruction semantics:

1. **Generic interpreter** (`cpu_generic/`): C++ functions that execute instructions concretely on `PPC_CPU_State`.
2. **JIT codegen** (`cpu_jitc_*/ppc_alu.cc` etc.): Functions that emit native code for each instruction.
3. **(Missing) Analysis**: There is no way to ask "what state does this instruction read and write?" without running it.

This leads to two problems:

- **Duplication**: The interpreter and each JIT backend re-implement the same decode logic and semantic edge cases (e.g., rA=0 means literal zero, not GPR0). Bugs can appear in one backend but not another.
- **No optimization infrastructure**: Dead code elimination, register allocation hints, and compare+branch fusion all require knowing an instruction's read/write sets. Without a classification mechanism, these optimizations require either a separate hand-maintained classifier (two sources of truth) or are simply not implemented.

## Core Idea

Define the operational semantics of each PPC instruction **once**, parameterized over an abstract backend. Different backends provide different interpretations:

| Backend | `read_gpr(rA)` | `add(a, b)` | `write_gpr(rD, val)` |
|---------|----------------|-------------|----------------------|
| **Interpreter** | returns `gCPU.gpr[rA]` | returns `a + b` | stores to `gCPU.gpr[rD]` |
| **JIT codegen** | emits LDR / uses regalloc | emits ADD | emits STR / marks dirty |
| **Analysis** | records "reads GPR(rA)" | no-op | records "writes GPR(rD)" |

The semantics function handles all encoding subtleties (rA=0, SPR decoding, CRM masks, update forms) exactly once. Each backend inherits the correct behavior automatically.

## Semantics Function Design

### Example: `addi` (add immediate)

```cpp
template<typename B>
void semantics_addi(B &b, uint32 opc) {
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);

    auto val = (rA == 0) ? b.imm(0) : b.read_gpr(rA);
    val = b.add(val, b.imm(imm));
    b.write_gpr(rD, val);
}
```

### Example: `stwu` (store word with update)

```cpp
template<typename B>
void semantics_stwu(B &b, uint32 opc) {
    int rS, rA;
    uint32 disp;
    PPC_OPC_TEMPL_D_SImm(opc, rS, rA, disp);

    // rA != 0 for update forms (architecture requirement)
    auto addr = b.add(b.read_gpr(rA), b.imm(disp));
    b.write_mem(addr, b.read_gpr(rS), 4);
    b.write_gpr(rA, addr);  // update: rA gets effective address
}
```

### Example: `cmpwi` (compare word immediate)

```cpp
template<typename B>
void semantics_cmpwi(B &b, uint32 opc) {
    int crfD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, crfD, rA, imm);
    crfD >>= 2;  // field number

    auto a = b.read_gpr(rA);
    auto result = b.cmp_signed(a, b.imm(imm));
    b.write_cr_field(crfD, result);
    b.read_xer_so();  // SO bit copied into CR field
}
```

### Example: `add.` (add with Rc=1)

```cpp
template<typename B>
void semantics_addx(B &b, uint32 opc) {
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);

    auto val = b.add(b.read_gpr(rA), b.read_gpr(rB));
    b.write_gpr(rD, val);

    if (opc & PPC_OPC_OE) {
        b.write_xer_ov(val);  // writes OV and SO
    }
    if (opc & PPC_OPC_Rc) {
        b.write_cr0_from_result(val);  // writes CR field 0, reads XER SO
    }
}
```

### Example: `bcx` (branch conditional)

```cpp
template<typename B>
void semantics_bcx(B &b, uint32 opc) {
    int BO, BI, BD;
    PPC_OPC_TEMPL_B(opc, BO, BI, BD);

    if (!(BO & 4)) {
        b.read_ctr();
        b.write_ctr();  // CTR decremented
    }
    if (!(BO & 16)) {
        b.read_cr_bit(BI);
    }
    if (opc & PPC_OPC_LK) {
        b.write_lr();
    }
    b.branch();
}
```

### Example: `mtcrf` (move to CR fields)

```cpp
template<typename B>
void semantics_mtcrf(B &b, uint32 opc) {
    int rS;
    uint32 CRM;
    // decode CRM and rS from instruction
    PPC_OPC_TEMPL_XFX(opc, rS, CRM);

    auto val = b.read_gpr(rS);
    for (int i = 0; i < 8; i++) {
        if (CRM & (1 << (7 - i))) {
            b.write_cr_field(i, val);  // only selected fields
        }
    }
}
```

## Backend Interface

The backend must provide operations at the right abstraction level. The semantics
functions call backend methods that deal with PPC-level concepts (GPRs, CR fields,
XER sub-fields), not host-level details (ARM64 registers, NZCV flags).

### State Access

```
read_gpr(reg)           → reads a GPR (0-31)
write_gpr(reg, val)     → writes a GPR
read_fpr(reg)           → reads an FPR (0-31)
write_fpr(reg, val)     → writes an FPR
read_cr_bit(bit)        → reads a single CR bit (0-31)
read_cr_field(field)    → reads a 4-bit CR field (0-7)
write_cr_field(field, val) → writes a 4-bit CR field
write_cr_bit(bit, val)  → writes a single CR bit
write_cr0_from_result(val) → writes CR0 from signed comparison with 0 + XER SO
read_xer_so()           → reads XER Summary Overflow
read_xer_ca()           → reads XER Carry
write_xer_ca(val)       → writes XER Carry
write_xer_ov(val)       → writes XER OV and SO
read_xer()              → reads entire XER (mfxer)
write_xer(val)          → writes entire XER (mtspr)
read_lr()               → reads Link Register
write_lr()              → writes Link Register
read_ctr()              → reads Count Register
write_ctr()             → writes Count Register
read_fpscr()            → reads FPSCR
write_fpscr(val)        → writes FPSCR
```

### Computation

```
imm(val)                → immediate constant
add(a, b)               → addition
sub(a, b)               → subtraction
and_(a, b)              → bitwise AND
or_(a, b)               → bitwise OR
xor_(a, b)              → bitwise XOR
not_(a)                 → bitwise NOT
shl(a, b)               → shift left
shr(a, b)               → shift right (logical)
sar(a, b)               → shift right (arithmetic)
mul(a, b)               → multiply
div_s(a, b)             → signed divide
div_u(a, b)             → unsigned divide
cmp_signed(a, b)        → signed comparison (produces CR-like result)
cmp_unsigned(a, b)      → unsigned comparison
rotl(a, b)              → rotate left
mask(mb, me)            → generate rotation mask
extend_sign(a, bits)    → sign extension
```

### Memory

```
read_mem(addr, size)    → load from memory (1/2/4/8 bytes)
write_mem(addr, val, size) → store to memory
```

### Control Flow

```
branch()                → unconditional branch (marks block end)
branch_cond()           → conditional branch (marks block end)
trap()                  → trap instruction
syscall()               → system call
```

### Fallback

```
everything()            → "this instruction may read/write anything"
```

For instructions that are too complex to model precisely, or for unimplemented
opcodes that fall back to the interpreter, calling `everything()` is always safe.
The analysis backend treats this as "all state is live, nothing is dead." This
ensures correctness at the cost of missed optimizations.

## Analysis Backend

The analysis backend records read/write sets into a compact structure:

```cpp
struct InsnEffect {
    uint32 gpr_read;      // bitmask: which GPRs are read
    uint32 gpr_write;     // bitmask: which GPRs are written
    uint32 cr_read;       // bitmask: which CR bits are read (0-31)
    uint32 cr_write;      // bitmask: which CR bits are written (0-31)
    uint32 fpr_read;      // bitmask: which FPRs are read
    uint32 fpr_write;     // bitmask: which FPRs are written
    uint8  xer_read;      // bits: CA=0, OV=1, SO=2, whole=3..7
    uint8  xer_write;
    bool   lr_read, lr_write;
    bool   ctr_read, ctr_write;
    bool   fpscr_read, fpscr_write;
    bool   reads_memory;
    bool   writes_memory;
    bool   is_branch;
    bool   is_everything;  // fallback: assume worst case
};
```

Backend method implementations are trivial:

```cpp
struct AnalysisBackend {
    InsnEffect fx;

    void read_gpr(int r)           { fx.gpr_read |= (1u << r); }
    void write_gpr(int r, auto)    { fx.gpr_write |= (1u << r); }
    void read_cr_bit(int bit)      { fx.cr_read |= (1u << bit); }
    void write_cr_field(int f, auto) {
        fx.cr_write |= (0xFu << (f * 4));  // 4 bits per field
    }
    void read_xer_ca()             { fx.xer_read |= XER_CA; }
    void write_xer_ca(auto)        { fx.xer_write |= XER_CA; }
    void write_mem(auto, auto, int) { fx.writes_memory = true; }
    void read_mem(auto, int)       { fx.reads_memory = true; }
    void branch()                  { fx.is_branch = true; }
    void everything()              { fx.is_everything = true; }

    // Computation ops are no-ops — we don't track values
    auto imm(uint32)               { return 0; }
    auto add(auto, auto)           { return 0; }
    auto sub(auto, auto)           { return 0; }
    // ...
};
```

## Liveness Analysis

Given the analysis backend, liveness analysis is a backward scan over a basic block.

### Basic Block Discovery

A basic block ends at:
- Any branch instruction (`is_branch == true`)
- Page boundary (offset == 4096)
- Any `everything()` instruction (conservative block break)

The pre-scan identifies block boundaries first, then analyzes backward within each block.

### Backward Liveness Pass

```
live = {all GPRs, all CR bits, all XER, LR, CTR}  // conservative at block exit

for each instruction from LAST to FIRST:
    effect = analyze(insn)

    if effect.is_everything:
        live = {everything}
        continue

    // Determine which writes are dead
    dead_gpr   = effect.gpr_write & ~live.gpr
    dead_cr    = effect.cr_write  & ~live.cr
    dead_xer   = effect.xer_write & ~live.xer
    // Memory writes are NEVER dead (observable side effect)

    // Update liveness: kill writes, then gen reads
    live.gpr = (live.gpr & ~effect.gpr_write) | effect.gpr_read
    live.cr  = (live.cr  & ~effect.cr_write)  | effect.cr_read
    live.xer = (live.xer & ~effect.xer_write) | effect.xer_read
    // ... same for LR, CTR, FPR, FPSCR

    store dead_gpr, dead_cr, dead_xer for this instruction
```

After the backward pass, each instruction has a set of dead outputs. The codegen
backend can skip emitting code for those outputs.

### What the Codegen Backend Does With Liveness

The codegen functions consult the liveness map to skip dead writes:

- **Dead CR0 from Rc=1**: Skip the entire `gen_update_cr0` sequence (10 instructions saved). Just emit the ALU operation without the flag-setting variant.
- **Dead CR field from compare**: Skip the 9-instruction `gen_cr_insert_*` sequence. The comparison itself can also be skipped if the instruction has no other effects.
- **Dead XER OV/SO from OE=1**: Skip the overflow computation.
- **Dead GPR write**: Rare in practice (compilers don't generate dead stores often), but free to skip.

## Compare+Branch Optimization

The highest-value optimization from the abstract semantics framework is
eliminating the CR pack/unpack round-trip for compare+branch sequences.

### The Problem

The current AArch64 codegen for `cmpwi cr0, r3, 0` + `ble target`
emits ~13 native instructions:

```
; cmpwi cr0, r3, 0
ldr  w16, [x20, #20]       ; load gpr[3]
cmp  w16, #0x0              ; native compare → sets NZCV
csinc w0, wzr, wzr, eq      ; pack LT/GT/EQ into nibble
csinc w0, w0, w0, ge        ;
movz w1, #0x2               ;
lslv w0, w1, w0             ;
ldr  w1, [x20, #400]        ; load XER for SO bit
add  w0, w0, w1, lsr #31    ;
ldr  w1, [x20, #392]        ; load CR
bfm  w1, w0, #4, #3         ; insert into CR field 0
str  w1, [x20, #392]        ; store CR back

; ble target
ldr  w16, [x20, #392]       ; load CR again
tbz  w16, #30, not_taken    ; test GT bit
```

The `cmp` instruction already sets the AArch64 NZCV flags with exactly
the information we need for `ble`. But the codegen immediately destroys
NZCV by packing the result into the PPC CR format, storing it to memory,
then loading it back and testing a bit. This round-trip is the hot path
in every loop.

### Upward-Exposed Reads

The key insight: we don't need full inter-block liveness (which would
require fixed-point iteration for loops). Instead, we compute a simpler
per-block property: **upward-exposed reads** — the set of resources
that are read before being written in a block.

For each block, a single forward scan determines this:

```
exposed_reads = {}
for each instruction from FIRST to LAST:
    effect = analyze(insn)
    exposed_reads |= (effect.reads & ~already_written)
    already_written |= effect.writes
```

If CR field F is NOT in the exposed reads of a block, then CR field F
is **dead on entry** to that block — it will be overwritten before
anyone reads it. This is a per-block property, computed once, with no
iteration and no dependence on other blocks.

### Why This Works for Loops

Consider the most common pattern — a counted loop:

```
start:
    lwz   r3, 0(r4)        ; loop body
    addi  r4, r4, 4         ; loop body
    cmpwi cr0, r3, 0        ; compare — writes CR0
    ble   start              ; branch — reads CR0
    ; fall-through code that reads CR0
```

The forward scan of this block from `start`:
- `lwz`: doesn't read or write CR0
- `addi`: doesn't read or write CR0
- `cmpwi`: **writes** CR0 → CR0 is dead on entry

The branch target is `start`, and CR0 is dead at `start`. Therefore
on the taken path (back to `start`), CR0 does not need to be in the
CPU state.

### Stale CR and the Heartbeat

The JITC heartbeat (`ppc_heartbeat_ext_asm`) runs on every taken branch
via `ppc_new_pc_asm` / `ppc_new_pc_rel_asm`. It checks `exception_pending`
and `MSR_EE` — it does not read CR.

If a DEC or external exception fires during the heartbeat, the exception
mechanism saves `SRR0` (PC) and `SRR1` (MSR). The OS exception handler
then saves CR (among other registers) to the interrupted thread's stack.
If CR0 is stale at this point, the OS saves a stale value.

This is safe because CR0 is **dead** at the resume point. When the OS
eventually restores the thread and returns via `rfi`, execution resumes
at the point where CR0 was stale. But the program will overwrite CR0
before reading it — that's what "dead" means. The stale value is never
observed by the program.

### Implementation: Two Layers

The optimization is implemented in two independent layers:

#### Layer 1: Deferred CR Materialization (within-block)

This is a codegen-level mechanism, independent of the abstract semantics
framework. It was used by the original x86 JIT and is ported to aarch64.

The JITC state tracks whether native flags (NZCV on aarch64, EFLAGS on
x86) hold a valid but not-yet-materialized CR field result:

```cpp
struct JITC {
    PPC_CRx nativeFlags;          // which CR field (PPC_CR0..PPC_CR7)
    RegisterState nativeFlagsState; // rsUnused or rsDirty
    bool nativeFlagsSigned;        // signed or unsigned compare
};
```

**Compare codegen** (`cmpwi cr0, r3, 0`):
- Emit native `cmp` (sets NZCV)
- Call `jitc.mapFlagsDirty(PPC_CR0, true)` — record that NZCV
  holds the signed comparison result for CR0
- Do NOT pack the result into the PPC CR field yet

**Every other instruction's codegen**:
- Call `jitc.clobberFlags()` at the start. If flags are dirty,
  this emits the CR pack+store sequence to materialize them
  before the instruction clobbers NZCV.

**Branch codegen** (`ble target`):
- Check `jitc.flagsMapped() && jitc.getFlagsMapping() == cr`
- If yes: NZCV is valid for this CR field. Emit native `b.le`
  directly, skipping the CR load + bit test.
- If no: fall back to the normal CR load + TBZ/TBNZ path.

This handles the common case where a compare is immediately
followed by a branch on the same CR field — no analysis needed,
just codegen bookkeeping.

```
; without deferred flags (current, 13 insns):
ldr  w16, [x20, #20]       ; load gpr[3]
cmp  w16, #0x0              ; sets NZCV
csinc ...                    ; pack into CR nibble (9 insns)
str  w1, [x20, #392]        ; store CR
ldr  w16, [x20, #392]       ; load CR again
tbz  w16, #30, not_taken    ; test bit

; with deferred flags (optimized, ~5 insns):
ldr  w16, [x20, #20]       ; load gpr[3]
cmp  w16, #0x0              ; sets NZCV, flags deferred
; branch codegen sees flags are mapped:
b.le taken_dispatch          ; use NZCV directly
; fall-through: materialize CR for subsequent code
csinc ...                    ; pack CR (9 insns)
str  w1, [x20, #392]
; continue
taken_dispatch:
; dispatch to target
```

The taken path skips the CR pack entirely. The fall-through path
materializes CR only when needed by subsequent code.

#### Layer 2: Cross-Block Dead CR (future, needs analysis)

Layer 1 always materializes CR on the fall-through path and before
any block exit (via `clobberAll` → `clobberFlags`). The upward-exposed
reads analysis from the abstract semantics framework can extend this:

If the analysis proves that CR field F is dead at the branch target
(overwritten before being read), the branch codegen can skip
materialization on the taken path entirely — even through the heartbeat
dispatch. This is safe because "dead" means no code path from that
point will observe CR before overwriting it (see Stale CR and the
Heartbeat above).

#### Step 1: Single-block dead CR (taken path only)

An on-demand forward scan from the branch target offset determines
whether CR field F is killed before being read within that block:

```
bool is_cr_field_dead_at(physpage, offset, field):
    cr_field_mask = 0xF << ((7 - field) * 4)
    for each instruction from offset:
        fx = ppc_analyze_insn(opc)
        if fx.is_everything: return false  // conservative
        if fx.cr_read & cr_field_mask: return false  // live
        if (fx.cr_write & cr_field_mask) == cr_field_mask: return true // dead
        if fx.is_branch: return false  // end of block
    return false  // page end
```

When CR is dead at the target, the taken path skips the 9-instruction
`gen_cr_insert` flush entirely. The not-taken path still flushes
(conservative). This catches the common loop pattern:

```
start:
    lwz   r3, 0(r4)        ; loop body — doesn't touch CR
    addi  r4, r4, 4
    cmpwi cr0, r3, 0        ; kills CR0 before anyone reads it
    ble   start
```

The scan from `start` sees `lwz`, `addi` (no CR), then `cmpwi` writes
CR0 → CR0 is dead at `start`. The `ble`'s taken path emits just
`b.le` + dispatch (3 instructions) instead of `b.le` + flush + dispatch
(12 instructions).

#### Step 2: Full fixed-point liveness (both paths)

Step 1 only eliminates the flush on the taken path. To skip the flush
on **both** paths (taken and not-taken), we need CR to be dead at both
successors of the branch. This requires knowing liveness at the
fall-through entry too — which depends on what follows, including
other branches that might loop back.

This is a classic backward dataflow problem over the control flow graph:

```
live_in[B] = (live_out[B] - kill[B]) | gen[B]
live_out[B] = union of live_in[successors of B]
```

For loops, this requires fixed-point iteration (a block's `live_out`
depends on its own `live_in` via back edges). The iteration converges
in 2-3 passes for typical code.

When CR field F is dead at both successors of a branch, the compare
+ branch becomes just:

```
ldr  w16, [x20, #20]       ; load GPR
cmp  w16, #0x0              ; sets NZCV
b.le taken_dispatch          ; native branch — no flush anywhere
; not-taken: no flush, continue
```

Three instructions total for the compare+branch, down from 13.

### Limitations

- **Branch target on a different page**: the forward scan can't reach
  it. Conservatively assume CR is live. (Step 2 with a page-level CFG
  would have the same limitation.)
- **CR passes through the target block untouched**: Step 1 returns
  "live" (conservative). Step 2's fixed-point analysis handles this
  by propagating through successor blocks.
- **`everything()` instructions**: any unmodeled opcode before the
  first CR write forces the scan to return "live". Higher semantics
  coverage reduces this.

## Implementation Status

### What's done

**Semantics framework** (branch: `abstract-semantics`):

- `ConcreteSemantics<CPU>` — interpreter backend, parameterized on CPU state
  type. Works with both generic and JITC `PPC_CPU_State` structs.
- `LivenessSemantics` — records read/write bitmasks into `InsnEffect`.
- `ppc_semantics_alu.h` — ~50 ALU/compare/shift/rotate/CR logical templates.
- `ppc_semantics_branch.h` — bx, bcx, bclrx, bcctrx.
- `ppc_semantics_mem.h` — all integer load/store forms (D-form + X-form).
- `ppc_semantics_spr.h` — mfspr, mtspr (XER/LR/CTR), mfcr, mtcrf, mcrf.
- `ppc_semantics_fpu.h` — fcmpu/fcmpo, generic FPU arithmetic (Rc=1 → CR1),
  FPU load/store (lfs/lfd/stfs/stfd + update + indexed forms).
- `ppc_semantics_dispatch.h` — opcode → semantics function dispatch.
- `ppc_opc_decode.h` — shared decode macros (extracted from 4 backends).
- Generic interpreter's ALU and CR/SPR move functions replaced with
  `ConcreteSemantics` template wrappers. Validated by booting Linux.

**Prescan and liveness** (in JITC):

- Per-block prescan in `jitcNewEntrypoint` — computes `InsnEffect` and
  backward liveness for each basic block.
- Re-prescans at `flowEndBlock` boundaries.
- Prescan coverage: ~94-98% of instructions analyzed, depending on
  workload (FPU-heavy code benefits from FPU semantics coverage).

**Compare+branch optimization** (aarch64 JITC):

- *Layer 1: Deferred flags* — ported from x86 JIT. Compare codegen defers
  CR materialization in NZCV; branch codegen uses native `b.cond` instead
  of CR load + TBZ. Saves 2 instructions per compare+branch. 71% of
  conditional branches use this path.
- *Layer 2, step 1: Dead CR on taken path* — on-demand forward scan from
  branch target. If CR field is killed before read within the target block,
  skip the 9-instruction CR flush on the taken path. 39% of deferred-flags
  branches benefit (60k+ on Mac OS X boot). Hot loop back-edge becomes
  3 instructions (down from 13).

**Tests:**

- 136 unit tests (`test/test_semantics.cc`) — InsnEffect, liveness,
  ConcreteSemantics, SPR/CR moves.
- 18 deferred flags tests (`test/test_defflags.S`) — all compare+branch
  patterns.
- 9 existing JITC tests pass. Linux and Mac OS X boot correctly.

### Next steps

**Layer 2, step 2: Full fixed-point liveness (both paths)**

To skip the CR flush on BOTH the taken and not-taken paths, we need CR
to be dead at both successors. This requires:

1. Build a control flow graph for the page (basic blocks + edges)
2. Run backward dataflow: `live_in[B] = (live_out[B] - kill[B]) | gen[B]`
   with `live_out[B] = union of live_in[successors]`
3. Iterate until stable (fixed-point, typically 2-3 passes)

When CR is dead at both successors, the compare+branch becomes 3
instructions total with zero CR materialization on either path.

**Expand semantics coverage:**

- Remaining FPU FPSCR-manipulating opcodes (mtfsb0x, mtfsb1x, mtfsfx, mtfsfix).
- More SPRs in mfspr/mtspr (currently only XER/LR/CTR are precise).
- AltiVec opcodes.

**Potential future work:**

- `ConstantSemantics` — constant propagation/folding.
- Replace JITC interpreter fallback functions with `ConcreteSemantics`
  instantiation (eliminate the JITC's separate interpreter copy).

### File layout

```
src/cpu/
    ppc_opc_decode.h          ← shared decode macros
    ppc_concrete_sem.h        ← ConcreteSemantics<CPU>
    ppc_liveness_sem.h        ← LivenessSemantics + InsnEffect
    ppc_liveness.h            ← backward liveness pass + dead CR scan
    ppc_semantics_alu.h       ← ALU/compare/shift/rotate/CR logical
    ppc_semantics_branch.h    ← branch opcodes
    ppc_semantics_mem.h       ← load/store opcodes
    ppc_semantics_spr.h       ← SPR/CR move opcodes
    ppc_semantics_fpu.h       ← FPU compare, arithmetic, load/store
    ppc_semantics_dispatch.h  ← opcode → semantics dispatch
    cpu_generic/ppc_alu.cc    ← uses ConcreteSemantics for ALU
    cpu_generic/ppc_opc.cc    ← uses ConcreteSemantics for mcrf/mfcr/mtcrf
```

## Design Decisions

### 1. Value Representation

Each backend defines its own `Value` type:

```cpp
struct InterpreterBackend { using Value = uint32; };
struct AnalysisBackend    { using Value = int; /* dummy, never inspected */ };
struct JITBackend         { using Value = NativeReg; };
```

The analysis backend returns dummy zeros from every computation. Values flow
through the template mechanically but are never inspected — only the
`read_*`/`write_*` side effects matter. Semantics functions use `auto` for
intermediate values, so the same template works with all three `Value` types.

### 2. Control Flow: Encoding-Dependent vs Value-Dependent

Semantics functions distinguish two kinds of control flow:

**Encoding-dependent** branches (rA==0, Rc bit, OE bit, CRM mask, BO flags)
are determined by the instruction word and are evaluated in the semantics
function directly. All backends can evaluate these:

```cpp
auto val = (rA == 0) ? b.imm(0) : b.read_gpr(rA);  // encoding-dependent: fine
if (opc & PPC_OPC_Rc) { b.write_cr0_from_result(val); }  // encoding-dependent: fine
```

**Value-dependent** branches (divide-by-zero, FP NaN, trap conditions, DSI
exceptions) depend on runtime data. The analysis backend has no concrete values
and cannot evaluate these. The rule:

> **Value-dependent behavior lives inside backend methods, not in the
> semantics function.**

The semantics function reports effects unconditionally. The backend handles
the conditional behavior internally:

```cpp
// NOT this (analysis backend can't evaluate):
if (divisor == 0) { b.trap(); } else { b.write_gpr(rD, b.div_s(a, b)); }

// This (all effects reported unconditionally):
auto result = b.div_s(a, divisor);  // backend handles div-by-zero internally
b.write_gpr(rD, result);
```

For the analysis backend, this is conservative (over-reports effects) and
correct. For the interpreter and JIT backends, the actual runtime check lives
inside `div_s`, `read_mem`, etc.

### 3. DSI Exception Handling

Memory accesses can raise DSI (Data Storage Interrupt) exceptions. On a DSI,
the instruction must not modify any architected state — a faulting `lwz rD`
must not write rD, and a faulting `stwu` must not update rA.

The semantics function stays linear and does not branch on DSI:

```cpp
template<typename B>
void semantics_lwz(B &b, uint32 opc) {
    int rD, rA;
    uint32 disp;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, disp);

    auto addr = (rA == 0) ? b.imm(0) : b.read_gpr(rA);
    addr = b.add(addr, b.imm(disp));
    auto val = b.read_mem(addr, 4);  // may DSI
    b.write_gpr(rD, val);            // semantics doesn't know about DSI
}
```

Each backend handles DSI differently:

**Interpreter backend** — uses an internal poison flag:

```cpp
struct InterpreterBackend {
    bool exception = false;

    Value read_mem(Value addr, int size) {
        uint32 result;
        if (ppc_read_effective_word(addr, result) != PPC_MMU_OK) {
            exception = true;
            return 0;  // garbage, won't matter
        }
        return result;
    }

    void write_gpr(int r, Value val) {
        if (exception) return;  // silently becomes no-op
        gCPU.gpr[r] = val;
    }

    // All write_* methods check the poison flag
};
```

Once `read_mem` or `write_mem` sets the exception flag, all subsequent state
writes become no-ops. This correctly handles update forms: `lwzu` does
`write_gpr(rD, val)` then `write_gpr(rA, addr)` — if the load faulted, both
writes are suppressed. Exactly right per PPC architecture.

**Analysis backend** — no exception concept. Always reports all effects
(reads memory, writes GPR). This is conservative and correct for liveness:
the analysis must assume the instruction can complete, otherwise it might
incorrectly consider a write dead.

**JIT backend** — DSI is handled at the native code level. The memory access
slow path branches directly to the exception handler and never returns to the
instruction sequence. Register writes in the emitted code are only reached if
the memory access succeeded. No poison flag needed.

### 4. Memory Access

Memory operations use a simple interface:

```
read_mem(addr, size)         → load from memory (1/2/4/8 bytes)
write_mem(addr, val, size)   → store to memory
```

This is sufficient because:

- **MMU translation, alignment, endianness, MMIO dispatch** are backend
  concerns, not part of the PPC instruction semantics at this level
- **Byte-reversed loads** (`lwbrx`): expressed as
  `b.bswap(b.read_mem(addr, 4))` — byte swap is a computation
- **String load/store** (`lswx`/`stswx`): number of registers depends on
  XER at runtime (value-dependent). Use `everything()` for these rare opcodes.
- **Atomic** (`lwarx`/`stwcx.`): could add `read_mem_reserve`/`write_mem_conditional`,
  or use `everything()` since they're rare

The analysis backend just sets `reads_memory = true` or `writes_memory = true`.
Memory writes are never considered dead (observable side effect).

Update forms are expressed naturally:

```cpp
template<typename B>
void semantics_lwzu(B &b, uint32 opc) {
    int rD, rA;
    uint32 disp;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, disp);

    auto addr = b.add(b.read_gpr(rA), b.imm(disp));
    auto val = b.read_mem(addr, 4);
    b.write_gpr(rD, val);
    b.write_gpr(rA, addr);  // update: rA ← EA
}
```

The analysis backend correctly captures: reads GPR(rA), reads memory, writes
GPR(rD), writes GPR(rA). The interpreter backend's poison flag correctly
suppresses both GPR writes on DSI.

### 5. Partial Register Access

PPC state is not accessed uniformly — some registers have sub-field structure:

- **CR**: 32 bits, accessed as 8 fields (4 bits each) by compares, as individual
  bits by CR logical ops (crand, cror...), as the full 32 bits by mfcr/mtcrf
- **XER**: sub-fields SO, OV, CA have independent read/write patterns. OE=1
  writes OV+SO, carry instructions write CA, mfxer reads all
- **FPSCR**: similar sub-field patterns

The liveness domain tracks at the finest granularity needed per element:

| State | Granularity | Representation |
|-------|-------------|----------------|
| GPRs | per-register | `uint32` bitmask (32 GPRs) |
| FPRs | per-register | `uint32` bitmask (32 FPRs) |
| CR | per-bit | `uint32` bitmask (32 CR bits) |
| XER | per-sub-field | `uint8` bitmask (CA, OV, SO) |
| LR, CTR | whole register | `bool` each |
| FPSCR | whole register | `bool` (conservative) |

The backend interface uses higher-level operations that map to this:

```cpp
// Analysis backend
void write_cr_field(int f, Value) {
    fx.cr_write |= (0xFu << (f * 4));  // expands field to 4 bits
}
void read_cr_bit(int bit) {
    fx.cr_read |= (1u << bit);         // single bit
}
void write_xer_ca(Value) {
    fx.xer_write |= XER_CA_BIT;
}
void write_xer_ov(Value) {
    fx.xer_write |= XER_OV_BIT | XER_SO_BIT;  // OE=1 writes both
}
void read_xer() {
    fx.xer_read |= XER_ALL;  // mfxer reads everything
}
```

This correctly handles containment: writing CR field 0 kills liveness of CR
bits 0-3. Reading CR bit 2 makes that bit (and thus its containing field) live.

### 6. JIT Codegen Backend

The JIT backend is the hardest to fit into the template. The core tension:
JIT register allocation decisions are interleaved with computation. The
template's value-passing style (`auto result = b.add(a, b)`) doesn't know
the destination register before emitting the instruction, but the register
allocator needs to know it.

The pragmatic conclusion: **the JIT backend remains hand-written**. The
semantics template serves the interpreter and analysis backends — that
eliminates the two-sources-of-truth problem for instruction classification.
The JIT is a third representation justified by performance needs. It consults
the liveness map produced by the analysis backend to skip dead writes, but
its codegen is not expressed through the template.

This is acceptable because:
- The analysis backend is the primary consumer of the semantics functions
- The interpreter backend validates the semantics against the reference implementation
- The JIT backend's unique concerns (register allocation, instruction selection,
  fixups, native code layout) are fundamentally different from the abstract interface
- The JIT already needs to exist as hand-written code for performance

### 7. Pre-scan Performance

The pre-scan decodes each instruction in the basic block and calls the analysis
backend to collect `InsnEffect` — just reading bit fields and setting bitmask
bits. This is negligible compared to native code emission. The main codegen
pass already decodes each instruction, so the overhead is one extra (lightweight)
decode pass per basic block. Worth measuring but not a design concern.

## Open Questions

1. **`lmw`/`stmw` (load/store multiple)**: The register range is
   encoding-determined (rD through r31), so the analysis can enumerate them
   with a loop. This works cleanly. But `lswx`/`stswx` (string load/store)
   depend on XER for the count — these need `everything()`.

2. **FPU exception model**: FP instructions can raise various FP exceptions
   that update FPSCR. Should the semantics function model each FPSCR sub-field,
   or treat FPSCR conservatively as a single unit? Given that FPSCR updates
   are complex and rarely optimization-relevant, conservative treatment
   (`reads_fpscr` / `writes_fpscr` as booleans) is probably sufficient.

3. **Validation strategy**: When the interpreter backend is introduced
   (Phase 3), how do we validate it matches the existing interpreter?
   Run both side-by-side and compare all state after each instruction?
   Differential testing against the existing generic interpreter?
