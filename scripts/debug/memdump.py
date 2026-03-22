#!/usr/bin/env python3
"""Swiss-army knife for PearPC memory dump analysis.

Subcommands:
  printk  [dump] [term]       - Extract printk ring buffer, optionally search for term
  find    [dump] [hex-value]  - Search for a 32-bit big-endian value in the dump
  read    [dump] [pa] [count] - Read count 32-bit words at physical address
  diff    [dump1] [dump2] [pa] [count] - Compare words between two dumps
  regs    [dump] [pa]         - Decode Linux 2.4 PPC pt_regs at physical address
  oops    [dump]              - Find and decode kernel Oops from printk buffer
  pte     [dump] [va] [pgd]   - Walk Linux PGD→PTE for a VA, decode flags, audit page

All addresses are in hex (0x prefix optional). PPC data is big-endian.
Kernel VA to PA: PA = VA - 0xC0000000 (for VA >= 0xC0000000).
"""

import sys
import struct
import argparse


def read_dump(filename):
    with open(filename, 'rb') as f:
        return f.read()


def read_u32(data, offset):
    if offset + 4 > len(data):
        return None
    return struct.unpack('>I', data[offset:offset+4])[0]


def va_to_pa(va):
    if va >= 0xC0000000:
        return va - 0xC0000000
    return va


def parse_hex(s):
    return int(s, 16) if not s.startswith('0x') and not s.startswith('0X') else int(s, 0)


def cmd_printk(args):
    data = read_dump(args.dump)
    if args.term:
        # Search for the term with log-level prefixes first, then raw
        term = args.term.encode('latin-1')
        idx = -1
        for prefix in [b'<4>', b'<6>', b'<3>', b'<5>', b'<0>', b'']:
            idx = data.find(prefix + term)
            if idx >= 0:
                break
    else:
        idx = data.find(b'<4>Linux version')
    if idx < 0:
        print(f'"{args.term or "<4>Linux version"}" not found in {args.dump}')
        return 1

    # Show context: 200 bytes before, 4000 after
    start = max(0, idx - 200) if args.term else idx
    chunk = data[start:start + 4000]
    for c in chunk.decode('latin-1'):
        if ord(c) < 32 and c not in '\n\r\t':
            pass
        else:
            print(c, end='')
    print()
    return 0


def cmd_find(args):
    data = read_dump(args.dump)
    target_val = parse_hex(args.value)
    target = struct.pack('>I', target_val)
    idx = 0
    count = 0
    while count < args.max:
        idx = data.find(target, idx)
        if idx < 0:
            break
        # Show context
        print(f'Found {target_val:08x} at PA 0x{idx:08x} (VA 0x{idx + 0xC0000000:08x}):')
        start = max(0, idx - 8)
        for off in range(start, min(idx + 12, len(data)), 4):
            v = read_u32(data, off)
            marker = ' <<<<' if off == idx else ''
            print(f'  {off:08x}: {v:08x}{marker}')
        print()
        count += 1
        idx += 1
    if count == 0:
        print(f'{target_val:08x} not found in {args.dump}')
        return 1
    print(f'Total: {count} occurrence(s)')
    return 0


def cmd_read(args):
    data = read_dump(args.dump)
    pa = parse_hex(args.pa)
    if pa >= 0xC0000000:
        pa = va_to_pa(pa)
        print(f'(converted VA to PA 0x{pa:08x})')
    count = args.count
    print(f'{args.dump} at PA 0x{pa:08x}, {count} words:')
    for i in range(count):
        off = pa + i * 4
        v = read_u32(data, off)
        if v is None:
            print(f'  {off:08x}: (out of bounds)')
            break
        va = off + 0xC0000000
        extra = ''
        if v >= 0xC0000000 and v < 0xD0000000:
            extra = f'  [kernel VA → PA 0x{va_to_pa(v):08x}]'
        elif v == 0:
            extra = '  [NULL]'
        print(f'  {off:08x}: {v:08x}{extra}')
    return 0


def cmd_diff(args):
    data1 = read_dump(args.dump1)
    data2 = read_dump(args.dump2)
    pa = parse_hex(args.pa)
    if pa >= 0xC0000000:
        pa = va_to_pa(pa)
    count = args.count
    diffs = 0
    print(f'Comparing {args.dump1} vs {args.dump2} at PA 0x{pa:08x}, {count} words:')
    for i in range(count):
        off = pa + i * 4
        v1 = read_u32(data1, off)
        v2 = read_u32(data2, off)
        if v1 is None or v2 is None:
            break
        if v1 != v2:
            print(f'  {off:08x}: {v1:08x} vs {v2:08x}  DIFFER')
            diffs += 1
        elif args.all:
            print(f'  {off:08x}: {v1:08x}')
    print(f'{diffs} difference(s)')
    return 0


def cmd_regs(args):
    data = read_dump(args.dump)
    pa = parse_hex(args.pa)
    if pa >= 0xC0000000:
        pa = va_to_pa(pa)
    print(f'pt_regs at PA 0x{pa:08x}:')
    for i in range(32):
        v = read_u32(data, pa + i * 4)
        if v is None:
            print(f'  r{i:2d}: (out of bounds)')
            return 1
        print(f'  r{i:2d}: {v:08x}')

    fields = [
        ('nip', 0x80), ('msr', 0x84), ('orig_gpr3', 0x88), ('ctr', 0x8C),
        ('link', 0x90), ('xer', 0x94), ('ccr', 0x98), ('mq', 0x9C),
        ('trap', 0xA0), ('dar', 0xA4), ('dsisr', 0xA8), ('result', 0xAC),
    ]
    for name, off in fields:
        v = read_u32(data, pa + off)
        if v is None:
            break
        print(f'  {name:10s}: {v:08x}')

    # Decode trap type
    trap = read_u32(data, pa + 0xA0)
    trap_names = {
        0x0100: 'System Reset', 0x0200: 'Machine Check', 0x0300: 'DSI',
        0x0400: 'ISI', 0x0500: 'External Interrupt', 0x0600: 'Alignment',
        0x0700: 'Program', 0x0800: 'FPU Unavailable', 0x0900: 'Decrementer',
        0x0C00: 'System Call',
    }
    if trap and trap in trap_names:
        print(f'\n  Trap type: {trap_names[trap]}')
    return 0


def cmd_oops(args):
    data = read_dump(args.dump)
    idx = data.find(b'<4>Oops:')
    if idx < 0:
        print(f'No Oops found in {args.dump}')
        # Check for panic
        idx2 = data.find(b'<0>Kernel panic')
        if idx2 >= 0:
            chunk = data[idx2:idx2+200].decode('latin-1')
            for c in chunk:
                if ord(c) < 32 and c not in '\n\r\t':
                    pass
                else:
                    print(c, end='')
            print()
        return 1

    # Extract Oops text
    chunk = data[idx:idx+1500].decode('latin-1')
    lines = []
    for c in chunk:
        if ord(c) < 32 and c not in '\n\r\t':
            pass
        else:
            lines.append(c)
    oops_text = ''.join(lines)
    print(oops_text[:oops_text.find('\x00')] if '\x00' in oops_text else oops_text)

    # Try to extract REGS address for pt_regs decode
    import re
    m = re.search(r'REGS:\s*([0-9a-fA-F]+)', oops_text)
    if m:
        regs_va = int(m.group(1), 16)
        regs_pa = va_to_pa(regs_va)
        print(f'\n--- pt_regs at VA {regs_va:08x} (PA {regs_pa:08x}) ---')
        # Just decode a few key fields
        nip = read_u32(data, regs_pa + 0x80)
        msr = read_u32(data, regs_pa + 0x84)
        ctr = read_u32(data, regs_pa + 0x8C)
        lr = read_u32(data, regs_pa + 0x90)
        trap = read_u32(data, regs_pa + 0xA0)
        if nip is not None:
            print(f'  NIP={nip:08x} MSR={msr:08x} CTR={ctr:08x} LR={lr:08x} TRAP={trap:04x}')
    return 0


def cmd_pte(args):
    """Walk Linux 2.4 PPC page table: PGD → PTE for a virtual address."""
    data = read_dump(args.dump)
    va = parse_hex(args.va)
    pgd_pa = va_to_pa(parse_hex(args.pgd))

    RAM_SIZE = len(data)
    pgd_idx = (va >> 22) & 0x3FF
    pgd_entry_pa = pgd_pa + pgd_idx * 4
    pgd_val = read_u32(data, pgd_entry_pa)

    print(f'VA:  0x{va:08x}')
    print(f'PGD: PA 0x{pgd_pa:08x}  index={pgd_idx}')
    print(f'PGD[{pgd_idx}] at PA 0x{pgd_entry_pa:08x} = 0x{pgd_val:08x}')

    if pgd_val == 0:
        print('  → NULL — no page table for this VA')
        # Show neighbors
        print('Nearby PGD entries:')
        for off in range(-3, 4):
            idx = pgd_idx + off
            if 0 <= idx < 1024:
                v = read_u32(data, pgd_pa + idx * 4)
                tag = ' ←' if off == 0 else ''
                print(f'  PGD[{idx:4d}] = 0x{v:08x}{tag}')
        return 1

    pte_table_va = pgd_val & 0xFFFFF000
    pte_table_pa = va_to_pa(pte_table_va)
    pte_idx = (va >> 12) & 0x3FF
    pte_pa = pte_table_pa + pte_idx * 4
    pte_val = read_u32(data, pte_pa)

    print(f'PTE table: VA 0x{pte_table_va:08x} → PA 0x{pte_table_pa:08x}')
    print(f'PTE[{pte_idx}] at PA 0x{pte_pa:08x} = 0x{pte_val:08x}')

    if pte_val == 0:
        print('  → NOT PRESENT')
    else:
        rpn = pte_val & 0xFFFFF000
        flags = pte_val & 0xFFF
        flag_names = []
        if flags & 0x001: flag_names.append('PRESENT')
        if flags & 0x002: flag_names.append('RW')
        if flags & 0x004: flag_names.append('USER')
        if flags & 0x008: flag_names.append('WRITETHRU')
        if flags & 0x010: flag_names.append('NOCACHE')
        if flags & 0x020: flag_names.append('ACCESSED')
        if flags & 0x040: flag_names.append('DIRTY')
        if flags & 0x080: flag_names.append('DIRTY2')
        if flags & 0x100: flag_names.append('HWACCESSED')
        if flags & 0x200: flag_names.append('HASHPTE')
        if flags & 0x400: flag_names.append('EXEC')
        print(f'  RPN = 0x{rpn:08x}  flags = 0x{flags:03x} [{" ".join(flag_names)}]')
        if rpn >= RAM_SIZE:
            print(f'  *** BOGUS: RPN 0x{rpn:08x} outside RAM ({RAM_SIZE // (1024*1024)}MB)')
        else:
            print(f'  Valid: maps to PA 0x{rpn | (va & 0xFFF):08x}')

    # Audit entire PTE page
    zero = valid = bogus = 0
    for i in range(1024):
        v = read_u32(data, pte_table_pa + i * 4)
        if v == 0:
            zero += 1
        elif (v & 0xFFFFF000) < RAM_SIZE:
            valid += 1
        else:
            bogus += 1
    print(f'\nPTE page audit: {zero} zero, {valid} valid, {bogus} bogus (of 1024)')
    if bogus > 0:
        print('  *** PAGE TABLE CORRUPTION — bogus entries indicate unzeroed page (dcbz bug?)')

    # Show neighbors
    print(f'\nNearby PTEs:')
    for off in range(-3, 4):
        idx = pte_idx + off
        if 0 <= idx < 1024:
            v = read_u32(data, pte_table_pa + idx * 4)
            page_va = (va & 0xFFC00000) | (idx << 12)
            tag = ' ←' if off == 0 else ''
            status = ''
            if v == 0:
                status = 'NOT_PRESENT'
            elif (v & 0xFFFFF000) >= RAM_SIZE:
                status = 'BOGUS'
            print(f'  PTE[{idx:4d}] (VA 0x{page_va:08x}) = 0x{v:08x} {status}{tag}')
    return 0


def main():
    parser = argparse.ArgumentParser(description='PearPC memory dump analysis tool')
    sub = parser.add_subparsers(dest='cmd')

    p = sub.add_parser('printk', help='Extract printk ring buffer')
    p.add_argument('dump', help='Memory dump file')
    p.add_argument('term', nargs='?', default=None, help='Search term (default: Linux version)')

    p = sub.add_parser('find', help='Search for a 32-bit BE value')
    p.add_argument('dump', help='Memory dump file')
    p.add_argument('value', help='Hex value to search for (e.g., bfedde60)')
    p.add_argument('--max', type=int, default=20, help='Max results')

    p = sub.add_parser('read', help='Read words at a physical address')
    p.add_argument('dump', help='Memory dump file')
    p.add_argument('pa', help='Physical (or virtual) address in hex')
    p.add_argument('count', type=int, nargs='?', default=8, help='Number of words')

    p = sub.add_parser('diff', help='Compare words between two dumps')
    p.add_argument('dump1', help='First memory dump')
    p.add_argument('dump2', help='Second memory dump')
    p.add_argument('pa', help='Physical (or virtual) address in hex')
    p.add_argument('count', type=int, nargs='?', default=16, help='Number of words')
    p.add_argument('--all', action='store_true', help='Show matching words too')

    p = sub.add_parser('regs', help='Decode pt_regs at address')
    p.add_argument('dump', help='Memory dump file')
    p.add_argument('pa', help='Physical (or virtual) address of pt_regs')

    p = sub.add_parser('oops', help='Find and decode kernel Oops')
    p.add_argument('dump', help='Memory dump file')

    p = sub.add_parser('pte', help='Walk Linux PGD→PTE chain for a virtual address')
    p.add_argument('dump', help='Memory dump file')
    p.add_argument('va', help='Virtual address to look up (hex)')
    p.add_argument('pgd', help='PGD physical address (hex) — find via task_struct→mm→pgd')

    args = parser.parse_args()
    if not args.cmd:
        parser.print_help()
        return 1

    cmds = {
        'printk': cmd_printk, 'find': cmd_find, 'read': cmd_read,
        'diff': cmd_diff, 'regs': cmd_regs, 'oops': cmd_oops,
        'pte': cmd_pte,
    }
    return cmds[args.cmd](args)


if __name__ == '__main__':
    sys.exit(main() or 0)
