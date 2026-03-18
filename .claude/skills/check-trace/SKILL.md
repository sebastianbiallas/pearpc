---
name: check-trace
description: Analyze the JIT dispatch trace to diagnose boot stalls or crashes
allowed-tools: Bash, Read, Grep
argument-hint: [pattern-or-address]
---

# Check JIT Dispatch Trace

Analyze `jitc_trace.log` for the pattern or address `$ARGUMENTS`.

If no argument, check the tail of the trace to diagnose what the CPU is doing.

## Steps

1. **Check trace tail** (last 30 lines):
   ```
   tail -30 jitc_trace.log
   ```

2. **Identify the pattern**:
   - Same 2-3 PCs repeating with MSR_POW (0x4xxxx) → idle loop
   - Same 2-3 PCs, DEC frozen → DEC timer not firing
   - DEC changes but idle continues → scheduler not switching
   - Dispatches to PA 0x00000300 → DSI exceptions
   - Dispatches to PA 0x00000900 → DEC interrupts
   - Dispatches to PA 0x00000C00 → syscalls (sc instruction)

3. **Count exception dispatches**:
   ```
   grep -c 'pa=00000300' jitc_trace.log   # DSI count
   grep -c 'pa=00000900' jitc_trace.log   # DEC count
   grep -c 'pa=00000c00' jitc_trace.log   # SC count
   ```

4. **If argument is a hex address**, search for dispatches to/from that address:
   ```
   grep 'pa=ADDRESS\|ea=ADDRESS' jitc_trace.log | head -20
   ```

5. **Check DEC timer status**:
   ```
   grep '\[DEC\]' BOOT_LOG | tail -5
   ```

6. **Check WATCH events**:
   ```
   grep '\[WATCH\]' BOOT_LOG | tail -10
   ```

## Key fields in trace lines

```
DISPATCH ea=EFFECTIVE_ADDR pa=PHYSICAL_ADDR msr=MSR_VALUE
SEQ_NUM pc=PA msr=MSR cr=CR lr=LR ctr=CTR r0=R0 r1=SP r2=R2 r3=R3 r4=R4 r5=R5 dec=DEC
```

- `ea` = effective (virtual) address the kernel sees
- `pa` = physical address after translation
- `dec` = decrementer register value (should change over time)
- `r1` = stack pointer (0xC01Cxxxx for idle thread, 0xC04Dxxxx for init thread)
