#!/usr/bin/env python3
"""Find key kernel symbols by searching memory dumps for known patterns.

Searches for known strings and instruction patterns to locate important
kernel symbols and data structures in PearPC memory dumps.

Usage: python3 test/find_kernel_symbols.py memdump_generic.bin
"""

import sys
import struct
import argparse
import re


KERNEL_PHYS_BASE = 0x01400000
KERNEL_VIRT_BASE = 0xc0000000


def pa_to_va(pa):
    """Convert physical address to kernel virtual address."""
    if pa >= KERNEL_PHYS_BASE:
        return pa - KERNEL_PHYS_BASE + KERNEL_VIRT_BASE
    return pa


def va_to_pa(va):
    """Convert kernel virtual address to physical address."""
    if va >= KERNEL_VIRT_BASE:
        return va - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE
    return va


def read_u32(data, offset):
    """Read a big-endian uint32 from data at offset."""
    if offset + 4 > len(data) or offset < 0:
        return None
    return struct.unpack_from('>I', data, offset)[0]


def read_u16(data, offset):
    """Read a big-endian uint16 from data at offset."""
    if offset + 2 > len(data) or offset < 0:
        return None
    return struct.unpack_from('>H', data, offset)[0]


def find_string(data, pattern, start=0):
    """Find all occurrences of a byte pattern in data."""
    results = []
    pos = start
    if isinstance(pattern, str):
        pattern = pattern.encode('ascii')
    while True:
        idx = data.find(pattern, pos)
        if idx == -1:
            break
        results.append(idx)
        pos = idx + 1
    return results


def extract_string_at(data, offset, max_len=256):
    """Extract a NUL-terminated ASCII string from data."""
    end = offset
    while end < len(data) and end - offset < max_len and data[end] != 0:
        end += 1
    try:
        return data[offset:end].decode('ascii', errors='replace')
    except Exception:
        return None


def is_ppc_insn(word):
    """Check if a 32-bit word looks like a valid PPC instruction."""
    if word == 0:
        return False
    opcode = (word >> 26) & 0x3f
    # Common PPC opcodes
    return opcode in {
        7, 8, 10, 11, 12, 13, 14, 15,  # cmpli, twi, subfic, cmpwi, addic, addic., addi, addis
        16, 17, 18, 19,                  # bc, sc, b, CR ops
        20, 21, 23, 24, 25, 26, 27, 28, 29,  # rlwimi, rlwinm, rlwnm, ori, oris, xori, xoris, andi., andis.
        31, 32, 33, 34, 35, 36, 37, 38, 39,  # ext, lwz, lwzu, lbz, lbzu, stw, stwu, stb, stbu
        40, 41, 42, 43, 44, 45, 46, 47,  # lhz, lhzu, lha, lhau, sth, sthu, lmw, stmw
        48, 49, 50, 51, 52, 53, 54, 55,  # lfs, lfsu, lfd, lfdu, stfs, stfsu, stfd, stfdu
        59, 63,                            # FP ops
    }


def find_calibrate_delay(data):
    """Find calibrate_delay by searching for its printk string."""
    hits = find_string(data, b'Calibrating delay loop')
    results = []
    for hit in hits:
        s = extract_string_at(data, hit)
        results.append((hit, s))
    return results


def find_linux_version(data):
    """Find the Linux version string."""
    results = []
    for prefix in [b'Linux version ', b'<4>Linux version ', b'<6>Linux version ']:
        for hit in find_string(data, prefix):
            s = extract_string_at(data, hit)
            results.append((hit, s))
    return results


def find_init_call_patterns(data):
    """Find __initcall patterns - look for arrays of kernel function pointers."""
    results = []
    # Kernel function pointers are in the range 0xc0000000-0xc1000000
    # __initcall_start is an array of such pointers
    # Search for runs of 4+ consecutive kernel pointers
    kernel_start = KERNEL_PHYS_BASE
    kernel_end = min(kernel_start + 0x00800000, len(data))

    for offset in range(kernel_start, kernel_end - 16, 4):
        vals = []
        for i in range(8):
            v = read_u32(data, offset + i * 4)
            if v is None:
                break
            vals.append(v)
        if len(vals) >= 8:
            # Check if all values look like kernel VA pointers
            if all(0xc0000000 <= v <= 0xc1000000 for v in vals):
                results.append((offset, vals[:8]))
                # Skip ahead past this run
    return results


def find_jiffies_candidates(data):
    """Search for jiffies by looking for counter-like values.

    jiffies is typically a 32-bit counter incremented in do_timer.
    In a fresh boot it's usually a small value (< 0x10000).
    We look for stw instructions that store to addresses in the BSS range.
    """
    results = []
    # Look for the string "jiffies" directly
    for hit in find_string(data, b'jiffies'):
        s = extract_string_at(data, hit)
        results.append(('string', hit, s))

    # Look for do_timer string reference
    for hit in find_string(data, b'do_timer'):
        s = extract_string_at(data, hit)
        results.append(('string', hit, s))

    return results


def find_lwarx_stwcx_loops(data):
    """Find lwarx/stwcx. atomic loops, which are used around jiffies updates."""
    results = []
    kernel_start = KERNEL_PHYS_BASE
    kernel_end = min(kernel_start + 0x00400000, len(data))

    for offset in range(kernel_start, kernel_end - 32, 4):
        insn = read_u32(data, offset)
        if insn is None:
            continue
        # lwarx: opcode 31, XO=20 (bits 21-30)
        if (insn >> 26) == 31 and ((insn >> 1) & 0x3ff) == 20:
            # Found lwarx, search forward for matching stwcx. (XO=150)
            for fwd in range(1, 8):
                next_insn = read_u32(data, offset + fwd * 4)
                if next_insn is None:
                    break
                if (next_insn >> 26) == 31 and ((next_insn >> 1) & 0x3ff) == 150:
                    # Found lwarx/stwcx. pair
                    # Check nearby for addi rX, rX, 1 pattern
                    region_start = max(offset - 16, kernel_start)
                    region_end = min(offset + (fwd + 4) * 4, kernel_end)
                    has_increment = False
                    for check in range(region_start, region_end, 4):
                        ci = read_u32(data, check)
                        if ci is not None:
                            # addi rX, rX, 1: opcode 14, rD == rA, SIMM == 1
                            if (ci >> 26) == 14 and (ci & 0xffff) == 1:
                                rd = (ci >> 21) & 0x1f
                                ra = (ci >> 16) & 0x1f
                                if rd == ra:
                                    has_increment = True
                                    break
                    results.append((offset, fwd, has_increment))
                    break
    return results


def dump_addresses(data, addresses):
    """Read and display values at specific addresses."""
    print("\n--- Values at specified addresses ---")
    for addr_spec in addresses:
        if ':' in addr_spec:
            addr_str, size_str = addr_spec.split(':', 1)
            pa = int(addr_str, 0)
            size = int(size_str, 0)
        else:
            pa = int(addr_spec, 0)
            size = 4

        va = pa_to_va(pa)
        print(f"\n  PA=0x{pa:08x} VA=0x{va:08x} ({size} bytes):")
        for off in range(0, size, 4):
            v = read_u32(data, pa + off)
            if v is not None:
                # Check if value looks like a kernel pointer
                ptr_note = ""
                if 0xc0000000 <= v <= 0xc1000000:
                    ptr_note = f"  (kernel ptr -> PA=0x{va_to_pa(v):08x})"
                elif v == 0:
                    ptr_note = "  (NULL)"
                print(f"    +0x{off:04x}: 0x{v:08x}{ptr_note}")
            else:
                print(f"    +0x{off:04x}: (out of range)")


def main():
    parser = argparse.ArgumentParser(
        description='Find key kernel symbols in PearPC memory dumps')
    parser.add_argument('dumpfile', help='Memory dump file (e.g., memdump_generic.bin)')
    parser.add_argument('--addr', nargs='*', default=[],
                        help='Additional addresses to dump (hex PA, optionally :size)')
    parser.add_argument('--atomic-loops', action='store_true',
                        help='Search for lwarx/stwcx. atomic loops')
    parser.add_argument('--initcalls', action='store_true',
                        help='Search for __initcall pointer arrays')
    args = parser.parse_args()

    try:
        with open(args.dumpfile, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: file not found: {args.dumpfile}", file=sys.stderr)
        sys.exit(1)

    print(f"Dump file: {args.dumpfile} ({len(data)} bytes)")
    print(f"Kernel mapping: VA 0x{KERNEL_VIRT_BASE:08x} -> PA 0x{KERNEL_PHYS_BASE:08x}")

    # Find calibrate_delay
    print("\n--- Calibrating delay loop ---")
    hits = find_calibrate_delay(data)
    if hits:
        for pa, s in hits:
            va = pa_to_va(pa)
            print(f"  PA=0x{pa:08x} VA=0x{va:08x}: {s}")
    else:
        print("  (not found)")

    # Find Linux version string
    print("\n--- Linux version ---")
    hits = find_linux_version(data)
    if hits:
        for pa, s in hits:
            va = pa_to_va(pa)
            # Truncate long strings for readability
            display = s[:120] + '...' if len(s) > 120 else s
            print(f"  PA=0x{pa:08x} VA=0x{va:08x}: {display}")
    else:
        print("  (not found)")

    # Find jiffies-related
    print("\n--- jiffies / do_timer references ---")
    hits = find_jiffies_candidates(data)
    if hits:
        for kind, pa, s in hits:
            va = pa_to_va(pa)
            display = s[:120] + '...' if len(s) > 120 else s
            print(f"  [{kind}] PA=0x{pa:08x} VA=0x{va:08x}: {display}")
    else:
        print("  (not found)")

    # Find lwarx/stwcx. atomic loops
    if args.atomic_loops:
        print("\n--- lwarx/stwcx. atomic loops ---")
        loops = find_lwarx_stwcx_loops(data)
        if loops:
            for pa, span, has_inc in loops:
                va = pa_to_va(pa)
                inc_note = " [has increment]" if has_inc else ""
                print(f"  PA=0x{pa:08x} VA=0x{va:08x}: "
                      f"lwarx...stwcx. span={span} insns{inc_note}")
            print(f"  Total: {len(loops)} loops found")
            # Highlight increment loops as jiffies candidates
            inc_loops = [l for l in loops if l[2]]
            if inc_loops:
                print(f"  Increment loops (jiffies candidates): {len(inc_loops)}")
                for pa, span, _ in inc_loops:
                    va = pa_to_va(pa)
                    print(f"    PA=0x{pa:08x} VA=0x{va:08x}")
        else:
            print("  (none found)")

    # Find __initcall arrays
    if args.initcalls:
        print("\n--- __initcall pointer arrays ---")
        arrays = find_init_call_patterns(data)
        if arrays:
            # Show first few candidates
            for pa, vals in arrays[:10]:
                va = pa_to_va(pa)
                ptrs = ' '.join(f'{v:08x}' for v in vals)
                print(f"  PA=0x{pa:08x} VA=0x{va:08x}: {ptrs}")
            if len(arrays) > 10:
                print(f"  ... and {len(arrays) - 10} more candidates")
        else:
            print("  (none found)")

    # Dump specific addresses
    if args.addr:
        dump_addresses(data, args.addr)


if __name__ == '__main__':
    main()
