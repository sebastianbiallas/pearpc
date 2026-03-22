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
- **Live CR + immediate branch**: This is compare+branch fusion. The comparison result is live but consumed immediately — emit native CMP + B.cond instead of CMP + CR pack + LDR CR + TBZ.

## Migration Path

The abstract semantics framework can be introduced incrementally:

### Phase 1: Analysis Backend Only

Write semantics functions for the ~30 most common opcodes (ALU, compare, branch,
basic load/store). These are called ONLY by the pre-scan for liveness analysis.
The existing interpreter and JIT gen functions remain unchanged.

For unimplemented semantics functions, use the `everything()` fallback — correct
but conservative.

### Phase 2: Expand Coverage

Add semantics functions for remaining opcodes. Each new semantics function
immediately improves optimization opportunities (more dead writes detected).

### Phase 3: Interpreter Backend

Replace the generic interpreter's opcode handlers with the semantics functions
instantiated with a concrete execution backend. This validates the semantics
functions against the existing interpreter (any divergence is a bug).

Now the semantics are defined once. The generic interpreter and the analysis
pass share the same source.

### Phase 4: JIT Codegen Backend

This is the most complex step. The JIT codegen backend needs to emit efficient
native code, use the register allocator, handle fixups, etc. It may not be
practical to express all of this through the template interface — some opcodes
may need backend-specific codegen that goes beyond what the abstract interface
can express.

A pragmatic approach: the semantics functions handle the common case, and the
JIT backend can override specific opcodes with hand-written codegen when the
abstract interface is too limiting.

### Where the Semantics Functions Live

The semantics functions are architecture-independent — they describe PPC
instruction behavior, not how to execute or compile them. They belong in
the shared CPU layer:

```
src/cpu/
    ppc_semantics.h      ← template semantics functions
    ppc_semantics_alu.h   ← ALU opcodes
    ppc_semantics_fpu.h   ← FPU opcodes
    ppc_semantics_mem.h   ← load/store opcodes
    ppc_semantics_branch.h ← branch/CR opcodes
    ppc_analysis.h        ← AnalysisBackend + InsnEffect + liveness
    cpu_generic/          ← (later) ConcreteBackend wrapping interpreter
    cpu_jitc_aarch64/     ← (later) CodegenBackend wrapping JIT
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
