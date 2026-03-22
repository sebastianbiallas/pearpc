---
name: compare-dumps
description: Compare memory regions between generic and JIT memory dumps to find divergences
allowed-tools: Bash, Read
argument-hint: [subcommand] [args...]
---

# Compare Memory Dumps

Use `scripts/debug/memdump.py` subcommands:

```
# Read words at a PA (accepts kernel VA, auto-converts):
python3 scripts/debug/memdump.py read memdump_jit.bin PA [count]

# Diff two dumps at a PA:
python3 scripts/debug/memdump.py diff memdump_generic.bin memdump_jit.bin PA [count]

# Search for a 32-bit value:
python3 scripts/debug/memdump.py find memdump_jit.bin HEXVALUE

# Scan a range for differences:
python3 scripts/debug/compare_dumps.py memdump_generic.bin memdump_jit.bin --scan PA_START PA_END
```

Kernel VA to PA: PA = VA - 0xC0000000. All addresses in hex.

## Known kernel addresses (Mandrake 2.4.21)
- jiffies: PA 0x002432e8
- lost_ticks: PA 0x0025abe4
- initcall table: PA 0x0022f330 - 0x0022f420
