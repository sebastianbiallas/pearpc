# Debugging JIT Kernel Boot Issues

This document describes the methodology for diagnosing problems in the aarch64 JIT compiler, particularly when the emulated Linux kernel fails to boot correctly. It is based on hard-won experience debugging real issues.

## Golden Rule

**The generic CPU interpreter is the reference implementation.** If the generic CPU boots the kernel correctly but the JIT does not, the bug is in the JIT. The generic CPU is not slow — do not dismiss differences as "timing" or "performance" issues. PPC emulation is deterministic.

## Debug Outputs

The emulator produces several debug outputs during a run:

| File | Contents | When |
|------|----------|------|
| `jitc.log` | Every PPC instruction compiled + AArch64 codegen | Always (compile-time) |
| `jitc_trace.log` | Every dispatch: PC, MSR, CR, LR, CTR, r0-r5, DEC | Always (runtime) |
| `memdump_jit.bin` | Full 128MB RAM dump | On JIT exit |
| `memdump_generic.bin` | Full 128MB RAM dump | On generic exit |
| stderr | `[JITC] WARNING`, `[WATCH]`, `[DEC]`, dispatch stats | Always |

## Step-by-Step Methodology

### 1. Check jitc.log for unknown opcodes

**Always do this first.** Unknown opcodes compile to `ppc_opc_gen_invalid` which prints `[JITC] WARNING: unknown opcode XXXXXXXX at pc_ofs=XXXX` to stderr at JIT compile time, then generates a program exception (vector 0x700) at runtime. The warning is now automatic, but you should still check for it in the boot log.

```sh
grep '\[JITC\] WARNING' boot_log.log
```

If you find unknown opcodes, check if the generic CPU has them registered in `src/cpu/cpu_generic/ppc_dec.cc` and add the missing entries to `src/cpu/cpu_jitc_aarch64/ppc_dec.cc`. Cache hint opcodes like `dss` (XO 822), `dstst` (XO 374), and `dst` (XO 342) are no-ops but must be registered.

To compare the full opcode tables:

```sh
diff <(grep -Eo 'ppc_opc_table_group2\[[0-9]+\]' src/cpu/cpu_generic/ppc_dec.cc | \
       grep -Eo '\[[0-9]+\]' | tr -d '[]' | sort -n) \
     <(grep -Eo 'ppc_opc_table_gen_group2\[[0-9]+\]' src/cpu/cpu_jitc_aarch64/ppc_dec.cc | \
       grep -Eo '\[[0-9]+\]' | tr -d '[]' | sort -n) | grep '^<'
```

### 2. Extract the kernel printk buffer

The kernel may boot successfully but the console driver may not work, making it look like nothing happened. The printk buffer is in RAM and survives in the memory dump.

```sh
python3 scripts/debug/dump_kernel_log.py memdump_jit.bin
```

If you see `"Linux version ..."`, `"Calibrating delay loop..."`, etc., the kernel is making progress even if the screen is blank.

### 3. Check the dispatch trace tail

The last entries in `jitc_trace.log` reveal what the CPU is doing:

```sh
tail -20 jitc_trace.log
```

Common patterns:

| Pattern | Meaning |
|---------|---------|
| Same 2-3 PCs repeating, MSR has `0x0004xxxx` (MSR_POW) | Kernel idle loop -- boot completed |
| Same 2-3 PCs repeating, one is `__delay` (bdnz at `c000828c`) | Stuck in calibrate_delay or panic reboot countdown |
| PC at `00000700` | Program exception -- check for unknown opcodes |
| PC at `00000300` | DSI exception -- page fault, may be normal |
| PC at `00000900` | DEC exception -- timer interrupt |

### 4. Compare memory dumps

When the JIT produces different behavior than generic, compare key memory locations:

```sh
python3 scripts/debug/compare_dumps.py memdump_generic.bin memdump_jit.bin 0x002432e8:4 0x001fca24:4
```

Common addresses to check (these vary per kernel -- use `find_kernel_symbols.py` to locate them):

| Symbol | How to find | What it means |
|--------|------------|---------------|
| `jiffies` | Inside `do_timer`: `lwz rX, offset(rY); addi rX, rX, 1; stw rX, offset(rY)` | Timer tick counter, must increment |
| `loops_per_jiffy` | Inside `calibrate_delay`: `stw r0, offset(r29)` with initial value 0x1000 | CPU speed calibration result |
| `bh_task_vec` | Array of 20-byte entries with func=`c001ba90` (bh_action) | Bottom-half tasklet table |
| `bh_base` | Array of 32 function pointers, entry 0 = timer_bh | Bottom-half handler table |

**Never hardcode kernel addresses.** Always derive them from the actual memory dump using disassembly.

### 5. Disassemble from the dump

```sh
python3 scripts/debug/disasm_ppc.py memdump_jit.bin 0x00007b78 32
```

This decodes PPC instructions at the given physical address. Use it to understand what kernel code is doing at specific PCs from the trace.

### 6. Add targeted tracing

If the above steps don't reveal the issue, add tracing to the C code:

- **`ppc_write_physical_word`** in `ppc_mmu.cc` -- trace writes to specific PA ranges
- **`ppc_read_physical_word`** in `ppc_mmu.cc` -- trace reads from specific PA ranges
- **`jitcNewPC`** in `jitc.cc` -- add watchpoints that check memory values at dispatch time (catches TLB fast-path writes that bypass C code)

## Common JIT Bugs

### Missing opcodes in gen table

**Symptom:** Kernel panics or hits an infinite loop. stderr shows `[JITC] WARNING: unknown opcode XXXXXXXX at pc_ofs=XXXX`.

**Cause:** Opcode not registered in `ppc_opc_table_gen_group2[]` in `ppc_dec.cc`. Falls through to `ppc_opc_gen_invalid` which prints a WARNING to stderr and generates a program exception.

**Fix:** Register the opcode. Use `GEN_INTERPRET(name)` for non-memory opcodes, `GEN_INTERPRET_LOADSTORE(name)` for load/store, `GEN_INTERPRET_ENDBLOCK(name)` for MSR-changing opcodes.

### DEC timer race in TLB slow path

**Symptom:** Kernel data corruption. Loads return wrong values intermittently. Stores silently fail.

**Cause:** The DEC timer fires a signal that sets `exception_pending`. If the TLB slow-path asm checks `exception_pending` after a successful load/store, it mistakes the DEC interrupt for a DSI exception and dispatches to the exception handler, discarding the load result or skipping subsequent instructions.

**Fix:** Slow-path C functions return `int` (0=OK, 1=DSI). Asm stubs check the return value, not `exception_pending`. Async DEC/ext exceptions stay pending for the heartbeat to handle at the next dispatch.

### emitBxxFixup fragment overflow

**Symptom:** Branches go to wrong targets. Code execution falls through to garbage.

**Cause:** `emitBxxFixup()` captured the instruction address before `emit32()`, but `emit32()` can trigger a fragment overflow that moves `tcp` to a new fragment. The fixup then patches the wrong location (the linking branch in the old fragment).

**Fix:** Capture address AFTER `emit32()`: `return currentPage->tcp - 4`.

### Native load/store without npc setup

**Symptom:** DSI exception handler dispatches to wrong PC.

**Cause:** Native `lwz`/`stw` gen functions didn't set `npc = pc + 4` before calling the asm TLB stub. If a DSI occurs, the exception handler reads `npc` to find where to dispatch, but `npc` has a stale value.

**Fix:** Always emit `npc = pc + 4` in the prologue of native load/store gen functions.

## Debug Tool Reference

### scripts/debug/dump_kernel_log.py

```sh
python3 scripts/debug/dump_kernel_log.py memdump_jit.bin
```

Extracts the Linux kernel printk ring buffer from a memory dump by searching for `<N>Linux version` markers.

### scripts/debug/find_kernel_symbols.py

```sh
python3 scripts/debug/find_kernel_symbols.py memdump_generic.bin
```

Finds kernel symbols by searching for known string patterns and instruction sequences.

### scripts/debug/disasm_ppc.py

```sh
python3 scripts/debug/disasm_ppc.py memdump_jit.bin 0x00007b78 32
python3 scripts/debug/disasm_ppc.py --pa memdump_jit.bin 0x00007b78 32  # show PA instead of VA
```

Decodes PPC instructions from a memory dump. Covers branches, load/store, ALU, SPR moves, cache ops, FP, and more.

### scripts/debug/compare_dumps.py

```sh
python3 scripts/debug/compare_dumps.py memdump_generic.bin memdump_jit.bin 0x002432e8:4 0x0025ac00:160
python3 scripts/debug/compare_dumps.py --scan 0x001c9000 0x001ca000 memdump_generic.bin memdump_jit.bin
```

Compares two memory dumps at specific addresses or scans a range for differences.
