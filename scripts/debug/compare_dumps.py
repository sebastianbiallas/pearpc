#!/usr/bin/env python3
"""Compare two memory dumps at specific kernel data structure addresses.

Compares values at predefined or user-specified addresses between two PearPC
memory dumps (e.g., generic CPU vs JIT), showing differences.

Usage: python3 test/compare_dumps.py memdump_generic.bin memdump_jit.bin [addr:size ...]
"""

import sys
import struct
import argparse


KERNEL_PHYS_BASE = 0x01400000
KERNEL_VIRT_BASE = 0xc0000000

# Default addresses to compare (physical addresses with sizes in bytes)
# These are common kernel data structures found during debugging
DEFAULT_ADDRESSES = [
    # Exception vectors
    ('exception_vector_0x300', 0x00000300, 32),
    ('exception_vector_0x400', 0x00000400, 32),
    ('exception_vector_0x500', 0x00000500, 32),
    ('exception_vector_0x600', 0x00000600, 32),
    ('exception_vector_0x700', 0x00000700, 32),
    # Kernel entry (first instructions)
    ('kernel_entry', KERNEL_PHYS_BASE, 32),
]


def pa_to_va(pa):
    """Convert physical address to kernel virtual address."""
    if pa >= KERNEL_PHYS_BASE:
        return pa - KERNEL_PHYS_BASE + KERNEL_VIRT_BASE
    return pa


def read_u32(data, offset):
    """Read a big-endian uint32."""
    if offset + 4 > len(data) or offset < 0:
        return None
    return struct.unpack_from('>I', data, offset)[0]


def read_u16(data, offset):
    """Read a big-endian uint16."""
    if offset + 2 > len(data) or offset < 0:
        return None
    return struct.unpack_from('>H', data, offset)[0]


def read_u8(data, offset):
    """Read a single byte."""
    if offset >= len(data) or offset < 0:
        return None
    return data[offset]


def format_value(val):
    """Format a 32-bit value with annotation if it looks like a kernel pointer."""
    if val is None:
        return '(out of range)'
    s = f'0x{val:08x}'
    if 0xc0000000 <= val <= 0xc1000000:
        pa = val - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE
        s += f'  [kernel VA -> PA=0x{pa:08x}]'
    elif val == 0:
        s += '  [NULL]'
    return s


def compare_at_address(data1, data2, pa, size, label=None):
    """Compare memory at a given physical address between two dumps."""
    va = pa_to_va(pa)
    header = f'PA=0x{pa:08x} VA=0x{va:08x}'
    if label:
        header = f'{label} ({header})'
    else:
        header = f'{header}'

    diffs = 0
    results = []

    for off in range(0, size, 4):
        addr = pa + off
        v1 = read_u32(data1, addr)
        v2 = read_u32(data2, addr)

        if v1 is None and v2 is None:
            results.append((off, None, None, False))
            continue

        differ = (v1 != v2)
        if differ:
            diffs += 1
        results.append((off, v1, v2, differ))

    # Print header
    status = 'DIFFER' if diffs > 0 else 'MATCH'
    print(f'\n  {header}: {status} ({diffs} word(s) differ)')

    # Print details
    for off, v1, v2, differ in results:
        addr = pa + off
        va_off = pa_to_va(addr)
        marker = ' ***' if differ else ''
        s1 = format_value(v1)
        s2 = format_value(v2)
        if differ or size <= 32:
            # Show all words for small regions, only diffs for large ones
            print(f'    +0x{off:04x} (PA=0x{addr:08x}): dump1={s1}  dump2={s2}{marker}')

    return diffs


def parse_addr_spec(spec):
    """Parse an address specification like '0x1234:16' or '0x1234'.
    Returns (pa, size)."""
    if ':' in spec:
        addr_str, size_str = spec.split(':', 1)
        return int(addr_str, 0), int(size_str, 0)
    else:
        return int(spec, 0), 4


def scan_for_differences(data1, data2, start, end, context_words=1):
    """Scan a memory range and report all differing words."""
    diffs = []
    for off in range(start, min(end, len(data1), len(data2)) - 3, 4):
        v1 = read_u32(data1, off)
        v2 = read_u32(data2, off)
        if v1 is not None and v2 is not None and v1 != v2:
            diffs.append((off, v1, v2))
    return diffs


def main():
    parser = argparse.ArgumentParser(
        description='Compare two PearPC memory dumps at specific addresses')
    parser.add_argument('dump1', help='First memory dump file (e.g., memdump_generic.bin)')
    parser.add_argument('dump2', help='Second memory dump file (e.g., memdump_jit.bin)')
    parser.add_argument('addresses', nargs='*',
                        help='Addresses to compare (hex PA, optionally :size). '
                             'E.g., 0x001fca24:4 0x0025ac00:160')
    parser.add_argument('--scan', type=lambda x: int(x, 0), nargs=2,
                        metavar=('START', 'END'),
                        help='Scan a PA range for all differences')
    parser.add_argument('--max-diffs', type=int, default=50,
                        help='Maximum differences to show in scan mode (default: 50)')
    parser.add_argument('--no-defaults', action='store_true',
                        help='Skip default address comparisons')
    args = parser.parse_args()

    try:
        with open(args.dump1, 'rb') as f:
            data1 = f.read()
    except FileNotFoundError:
        print(f"Error: file not found: {args.dump1}", file=sys.stderr)
        sys.exit(1)

    try:
        with open(args.dump2, 'rb') as f:
            data2 = f.read()
    except FileNotFoundError:
        print(f"Error: file not found: {args.dump2}", file=sys.stderr)
        sys.exit(1)

    print(f"Dump 1: {args.dump1} ({len(data1)} bytes)")
    print(f"Dump 2: {args.dump2} ({len(data2)} bytes)")

    total_diffs = 0

    # Scan mode
    if args.scan:
        start, end = args.scan
        print(f"\nScanning PA range 0x{start:08x} - 0x{end:08x} for differences...")
        diffs = scan_for_differences(data1, data2, start, end)
        if not diffs:
            print("  No differences found.")
        else:
            shown = min(len(diffs), args.max_diffs)
            for pa, v1, v2 in diffs[:shown]:
                va = pa_to_va(pa)
                print(f'  PA=0x{pa:08x} VA=0x{va:08x}: '
                      f'dump1={format_value(v1)}  dump2={format_value(v2)}')
            if len(diffs) > shown:
                print(f'  ... and {len(diffs) - shown} more differences')
            print(f'\n  Total differing words: {len(diffs)}')
        return

    # Compare default addresses
    if not args.no_defaults and not args.addresses:
        print("\n--- Default kernel structure addresses ---")
        for label, pa, size in DEFAULT_ADDRESSES:
            d = compare_at_address(data1, data2, pa, size, label)
            total_diffs += d

    # Compare user-specified addresses
    if args.addresses:
        print("\n--- User-specified addresses ---")
        for spec in args.addresses:
            try:
                pa, size = parse_addr_spec(spec)
            except ValueError:
                print(f"\n  Error: invalid address spec '{spec}' "
                      f"(expected hex like 0x1234 or 0x1234:16)", file=sys.stderr)
                continue
            d = compare_at_address(data1, data2, pa, size)
            total_diffs += d

    print(f"\n--- Summary ---")
    print(f"Total differing words across checked addresses: {total_diffs}")


if __name__ == '__main__':
    main()
