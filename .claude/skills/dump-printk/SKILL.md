---
name: dump-printk
description: Extract and display the Linux kernel printk ring buffer from a PearPC memory dump
allowed-tools: Bash, Read
argument-hint: [dump-file] [search-term]
---

# Extract Kernel Printk Buffer

Use `scripts/debug/memdump.py printk` to extract the printk ring buffer.

```
python3 scripts/debug/memdump.py printk $ARGUMENTS
```

If no arguments, use `memdump_jit.bin`. Report the last printk message, any Oops, and what boot stage was reached.
