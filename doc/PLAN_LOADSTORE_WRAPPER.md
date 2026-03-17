# Plan: Load/Store GEN_INTERPRET Wrapper

## Problem

The current `ppc_opc_gen_interpret()` wrapper checks `exception_pending` after
the interpreter function returns. This flag is set by both:

- **Synchronous exceptions** (DSI/ISI) from `ppc_exception()` — must be
  dispatched immediately to the exception vector
- **Asynchronous interrupts** (DEC/ext) from the timer thread — should be
  deferred to the heartbeat at the next page dispatch

The wrapper cannot distinguish them. When a DEC interrupt fires during a
load/store interpreter call, the wrapper sees `exception_pending` set, clears
it, and dispatches to `npc` (which is still `pc+4` from the wrapper's own
setup, not `0x900`). This **eats the DEC exception** — the heartbeat never
sees it because `exception_pending` was already cleared.

This is a correctness bug: asynchronous exceptions are silently lost whenever
they coincide with a GEN_INTERPRET instruction execution.

## Solution

Add a new wrapper `ppc_opc_gen_interpret_loadstore()` for load/store opcodes.
Instead of polling `exception_pending`, it checks the **return value** of the
interpreter function to detect synchronous exceptions (DSI).

All load/store interpreter functions already call `ppc_read_effective_word()`
or `ppc_write_effective_word()` which return nonzero on MMU failure. Currently
the interpreter functions are `void` and discard this return value. We change
them to propagate it.

## Implementation

### Step 1: Change load/store interpreter function signatures

In `ppc_mmu.h`, change declarations from `void` to `int`:

```c
// Before:
void ppc_opc_stw(PPC_CPU_State &aCPU);
// After:
int ppc_opc_stw(PPC_CPU_State &aCPU);
```

Return 0 on success, nonzero (PPC_MMU_EXC) on exception. When the function
returns nonzero, `ppc_exception()` has already set up SRR0/SRR1/DAR/DSISR/npc.

### Step 2: Change load/store interpreter function bodies

In `ppc_mmu.cc`, change each function to return the MMU result. Example:

```c
// Before:
void ppc_opc_stw(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
    ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, aCPU.gpr[rS]);
}

// After:
int ppc_opc_stw(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
    return ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, aCPU.gpr[rS]);
}
```

For functions with update forms (stwu, lwzu, etc.) that modify rA on success:

```c
int ppc_opc_stwu(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
    int ret = ppc_write_effective_word(aCPU, aCPU.gpr[rA] + imm, aCPU.gpr[rS]);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += imm;
    }
    return ret;
}
```

For functions with multiple memory accesses (lmw, stmw, lswi, etc.), return
on first failure.

### Step 3: Add `ppc_opc_gen_interpret_loadstore()` wrapper

In `ppc_opc.h`, add:

```c
static inline void ppc_opc_gen_interpret_loadstore(
    JITC &jitc, int (*func)(PPC_CPU_State &))
{
    jitc.clobberAll();
    // Store current_opc, pc, npc (same as ppc_opc_gen_interpret)
    jitc.emitMOV32((NativeReg)16, jitc.current_opc);
    jitc.emitSTR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, current_opc));
    jitc.emitLDR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, current_code_base));
    jitc.emitMOV32((NativeReg)17, jitc.pc);
    jitc.emit32(0x0B110210); // ADD W16, W16, W17
    jitc.emitSTR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, pc));
    jitc.emit32(0x11001210); // ADD W16, W16, #4
    jitc.emitSTR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, npc));

    // X0 = &CPU state (X20)
    jitc.emit32(0xAA1403E0); // MOV X0, X20
    // Call interpreter function — returns int in W0
    jitc.emitBLR((NativeAddress)func);

    // Check return value: W0 != 0 means synchronous exception (DSI/ISI).
    // ppc_exception() already set SRR0/SRR1/npc and exception_pending.
    // Clear exception_pending and dispatch to npc.
    // If W0 == 0: no exception. Continue normally.
    // Any async DEC/ext remains in dec_exception/ext_exception flags
    // and will be handled by the heartbeat at the next page dispatch.
    NativeAddress noExc = jitc.emitBxxFixup();
    jitc.emit32(a64_STRBw(31, 20,
        offsetof(PPC_CPU_State, exception_pending)));
    jitc.emitLDR32_cpu((NativeReg)0, offsetof(PPC_CPU_State, npc));
    jitc.emitBLR((NativeAddress)ppc_new_pc_asm);
    // Patch: CBZ W0, #skip (branch if return value == 0)
    {
        NativeAddress here = jitc.asmHERE();
        sint64 offset = (sint64)(here - noExc);
        sint32 imm19 = (sint32)(offset / 4);
        uint32 insn = 0x34000000 | ((imm19 & 0x7FFFF) << 5) | 0;
        *(uint32 *)noExc = insn;  // CBZ W0, #offset
    }
}
```

### Step 4: Add `GEN_INTERPRET_LOADSTORE` macro

In `ppc_dec.cc`:

```c
#define GEN_INTERPRET_LOADSTORE(name) \
    JITCFlow ppc_opc_gen_##name(JITC &jitc) { \
        ppc_opc_gen_interpret_loadstore(jitc, ppc_opc_##name); \
        return flowEndBlock; \
    }
```

Returns `flowEndBlock` because a DSI ends the current basic block (matching
the x86_64 JIT's `stwcx_` gen which returns `flowEndBlock`).

### Step 5: Convert load/store opcodes

In `ppc_dec.cc`, change from `GEN_INTERPRET` to `GEN_INTERPRET_LOADSTORE`:

**Integer load/store:**
- `lwz`, `lwzu`, `lwzx`, `lwzux`
- `stw`, `stwu`, `stwx`, `stwux`
- `lbz`, `lbzu`, `lbzx`, `lbzux`
- `stb`, `stbu`, `stbx`, `stbux`
- `lhz`, `lhzu`, `lhzx`, `lhzux`
- `sth`, `sthu`, `sthx`, `sthux`
- `lha`, `lhau`, `lhax`, `lhaux`

**Byte-reversed:**
- `lwbrx`, `stwbrx`, `lhbrx`, `sthbrx`

**Multi/string:**
- `lmw`, `stmw`
- `lswi`, `lswx`, `stswi`, `stswx`

**Atomic:**
- `lwarx`, `stwcx_`

**FPU load/store:**
- `lfd`, `lfdu`, `lfdx`, `lfdux`
- `stfd`, `stfdu`, `stfdx`, `stfdux`
- `lfs`, `lfsu`, `lfsx`, `lfsux`
- `stfs`, `stfsu`, `stfsx`, `stfsux`

**Cache:**
- `dcbz` (can raise DSI for unmapped addresses)

### Step 6: Leave `ppc_opc_gen_interpret()` unchanged

Non-load/store opcodes (ALU, branches, SPR access, etc.) don't trigger DSI.
The existing `exception_pending` check in the old wrapper only fires for
concurrent DEC/ext interrupts, which is harmless — the worst case is a lost
DEC tick, identical to what happens when DEC fires between any two JIT
instructions.

## Files to modify

| File | Change |
|------|--------|
| `ppc_opc.h` | Add `ppc_opc_gen_interpret_loadstore()` |
| `ppc_mmu.h` | Change load/store declarations to return `int` |
| `ppc_alu.h` | Change `lwarx`/`stwcx_` declarations to return `int` |
| `ppc_mmu.cc` | Change load/store bodies to return MMU result |
| `ppc_fpu.cc` | Change FPU load/store bodies to return MMU result |
| `ppc_dec.cc` | Add `GEN_INTERPRET_LOADSTORE` macro; convert opcodes |

## Notes

- The native gen_ load/store functions (gen_lwz, gen_stw, etc. that use the
  asm TLB stubs) are unaffected. Their DSI handling is already correct via
  the `.Ldsi_from_slow` asm path.
- `stwcx_` already returns early on DSI (from the lwarx/stwcx fix). Changing
  it to return `int` just makes the return value visible to the wrapper.
- `dcbi` and `icbi` use `PPC_MMU_NO_EXC` flag so they don't raise DSI —
  they stay with `GEN_INTERPRET`.
