---
name: analyze-jitc-log
description: Analyze jitc.log to find JIT compilation issues — fragment gaps, branch overflow, bad codegen
allowed-tools: Bash, Read, Grep
argument-hint: [log-file]
---

# Analyze jitc.log

The jitc.log file contains all translated PPC→AArch64 code. Each entry shows:
- PPC instruction offset + disassembly
- Native AArch64 addresses + instruction encoding + disassembly

## Steps

1. **Read the END of the log first** — the most recently compiled code is at the bottom:
   ```
   tail -100 ${ARGUMENTS:-jitc.log}
   ```

2. **Check for fragment gaps** — look for large jumps in native addresses between consecutive instructions. Two consecutive AArch64 instructions should be 4 bytes apart. If the gap is >512 bytes, a new fragment was allocated:
   ```
   python3 -c "
   import re, sys
   lines = open('${ARGUMENTS:-jitc.log}').readlines()
   prev_addr = None
   for line in lines:
       m = re.match(r'\s+([0-9a-f]+)\s+[0-9a-f]+\s+', line)
       if m and len(m.group(1)) > 6:  # native address (long hex)
           addr = int(m.group(1), 16)
           if prev_addr and addr - prev_addr > 512:
               print(f'FRAGMENT GAP: {prev_addr:x} -> {addr:x} (distance: {addr - prev_addr} bytes)')
           prev_addr = addr
   "
   ```

3. **Check for conditional branch overflow risk** — any B.cc (0x54xxxxxx) instruction followed by a fragment gap means the conditional branch might not reach its target. B.cc range is ±1MB (±0x100000):
   - Look for `54` prefix instructions near fragment boundaries
   - Check if the gap exceeds 1MB

4. **Check for unresolved fixups** — `14000000` is `B #0` (branch to self), which means a fixup was never resolved:
   ```
   grep '14000000' ${ARGUMENTS:-jitc.log}
   ```

5. **Check for GEN_INTERPRET overhead** — look for the pattern: store current_opc, compute pc from base+offset, store pc, store npc, mov x0 cpu, blr interpreter. Count how many bytes each PPC instruction takes in native code.

6. **Identify the PPC page being compiled** — the first column shows PPC offsets within the page. The `current_code_base` (stored at CPU offset 904) gives the page base EA.

## Key addresses
- CPU state offsets: pc=800, npc=804, current_opc=808, current_code_base=904
- X19 = JITC pointer, X20 = CPU state pointer
- Fragment size = 512 bytes
- Translation cache starts around 0x1050xxxxx or 0x1014xxxxx

## Common issues
- **Fragment gap + conditional branch** = potential range overflow (root cause of prior kernel stall)
- **Unresolved fixup (14000000)** = compiler bug, will branch to self infinitely
- **emitBLR crossing fragment** = the MOVZ+MOVK+BLR sequence split across fragments (should be prevented by emitAssure)
