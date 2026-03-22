---
name: analyze-oops
description: Analyze a kernel Oops from the printk buffer in a PearPC memory dump
allowed-tools: Bash, Read, Grep
argument-hint: [dump-file]
---

# Analyze Kernel Oops

Use `scripts/debug/memdump.py` to extract and analyze the Oops.

## Steps

1. Extract the Oops:
   ```
   python3 scripts/debug/memdump.py oops ${ARGUMENTS:-memdump_jit.bin}
   ```

2. Decode pt_regs (use the REGS address from the Oops output):
   ```
   python3 scripts/debug/memdump.py regs DUMP_FILE REGS_ADDRESS
   ```

3. Disassemble around NIP and LR (convert VA to PA: PA = VA - 0xC0000000):
   ```
   python3 scripts/debug/disasm_ppc.py DUMP_FILE PA_OF_NIP 16
   python3 scripts/debug/disasm_ppc.py DUMP_FILE PA_OF_LR 16
   ```

4. Search for the NIP value in both dumps:
   ```
   python3 scripts/debug/memdump.py find memdump_generic.bin NIP_VALUE
   python3 scripts/debug/memdump.py find memdump_jit.bin NIP_VALUE
   ```

5. Check if NIP is a valid address:
   - 0xC0xxxxxx = kernel code
   - 0xBFxxxxxx = PROM virtual address (prom_mem_phys_to_virt)
   - 0xFDxxxxxx = PCI I/O space (not executable)

Report: exception type, faulting address, caller, what the code was trying to do.
