#!/usr/bin/env python3
"""Compare generic CPU trace with aarch64 JIT trace to find divergence."""

import sys
import re
import argparse


def parse_generic_line(line):
    """Parse a trace_generic.log line."""
    m = re.match(
        r'(\d+) pc=([0-9a-f]+) msr=([0-9a-f]+) cr=([0-9a-f]+) '
        r'lr=([0-9a-f]+) ctr=([0-9a-f]+) '
        r'r0=([0-9a-f]+) r1=([0-9a-f]+) r2=([0-9a-f]+) '
        r'r3=([0-9a-f]+) r4=([0-9a-f]+) r5=([0-9a-f]+) '
        r'dec=([0-9a-f]+)',
        line.strip())
    if not m:
        return None
    return {
        'seq': int(m.group(1)),
        'pc': int(m.group(2), 16),
        'msr': int(m.group(3), 16),
        'cr': int(m.group(4), 16),
        'lr': int(m.group(5), 16),
        'ctr': int(m.group(6), 16),
        'r0': int(m.group(7), 16),
        'r1': int(m.group(8), 16),
        'r2': int(m.group(9), 16),
        'r3': int(m.group(10), 16),
        'r4': int(m.group(11), 16),
        'r5': int(m.group(12), 16),
        'dec': int(m.group(13), 16),
    }


def parse_jit_line(line):
    """Parse a jitc_trace.log state line (not DISPATCH/TRANSLATE)."""
    m = re.match(
        r'(\d+) pc=([0-9a-f]+) msr=([0-9a-f]+) cr=([0-9a-f]+) '
        r'lr=([0-9a-f]+) ctr=([0-9a-f]+) '
        r'r0=([0-9a-f]+) r1=([0-9a-f]+) r2=([0-9a-f]+) '
        r'r3=([0-9a-f]+) r4=([0-9a-f]+) r5=([0-9a-f]+) '
        r'dec=([0-9a-f]+)',
        line.strip())
    if not m:
        return None
    return {
        'seq': int(m.group(1)),
        'pc': int(m.group(2), 16),
        'msr': int(m.group(3), 16),
        'cr': int(m.group(4), 16),
        'lr': int(m.group(5), 16),
        'ctr': int(m.group(6), 16),
        'r0': int(m.group(7), 16),
        'r1': int(m.group(8), 16),
        'r2': int(m.group(9), 16),
        'r3': int(m.group(10), 16),
        'r4': int(m.group(11), 16),
        'r5': int(m.group(12), 16),
        'dec': int(m.group(13), 16),
    }


def load_trace(filename, parser):
    """Load trace entries from file."""
    entries = []
    with open(filename) as f:
        for line in f:
            e = parser(line)
            if e:
                entries.append(e)
    return entries


def build_pc_index(entries):
    """Build dict: pc -> list of (index, entry)."""
    idx = {}
    for i, e in enumerate(entries):
        pc = e['pc']
        if pc not in idx:
            idx[pc] = []
        idx[pc].append((i, e))
    return idx


def compare_entries(g, j, fields=None):
    """Compare two trace entries, return list of differing fields."""
    if fields is None:
        fields = ['cr', 'lr', 'ctr', 'r0', 'r1', 'r2', 'r3', 'r4', 'r5', 'msr']
    diffs = []
    for f in fields:
        if g.get(f) != j.get(f):
            diffs.append((f, g.get(f), j.get(f)))
    return diffs


def main():
    parser = argparse.ArgumentParser(description='Compare PearPC generic vs JIT traces')
    parser.add_argument('generic', help='trace_generic.log')
    parser.add_argument('jit', help='jitc_trace.log')
    parser.add_argument('--pc', type=lambda x: int(x, 0), default=None,
                        help='Only compare at this PC (hex)')
    parser.add_argument('--max-diffs', type=int, default=20,
                        help='Max divergences to report')
    args = parser.parse_args()

    print(f"Loading generic trace: {args.generic}")
    generic = load_trace(args.generic, parse_generic_line)
    print(f"  {len(generic)} entries")

    print(f"Loading JIT trace: {args.jit}")
    jit = load_trace(args.jit, parse_jit_line)
    print(f"  {len(jit)} entries")

    if not generic or not jit:
        print("ERROR: One or both traces are empty")
        sys.exit(1)

    # NOTE: Generic trace has effective addresses (EA), JIT trace has physical addresses (PA).
    # For kernel code: EA = PA - 0x01400000 + 0xc0000000 (typical Linux mapping).
    # We convert JIT PAs to EAs for comparison.
    KERNEL_PA_BASE = 0x01400000
    KERNEL_EA_BASE = 0xc0000000

    def jit_pa_to_ea(pa):
        if pa >= KERNEL_PA_BASE and pa < KERNEL_PA_BASE + 0x01000000:
            return pa - KERNEL_PA_BASE + KERNEL_EA_BASE
        return pa  # non-kernel addresses (prom etc.) — no conversion

    # Convert JIT PCs to EA for comparison
    for e in jit:
        e['ea'] = jit_pa_to_ea(e['pc'])
        e['pa'] = e['pc']

    for e in generic:
        e['ea'] = e['pc']

    # Summary stats
    gen_pcs = set(e['ea'] for e in generic)
    jit_pcs = set(e['ea'] for e in jit)
    common = gen_pcs & jit_pcs
    print(f"\nGeneric unique EAs: {len(gen_pcs)}")
    print(f"JIT unique EAs:     {len(jit_pcs)}")
    print(f"Common EAs:         {len(common)}")

    # PC range analysis
    gen_kernel = [e for e in generic if e['ea'] >= 0xc0000000]
    jit_kernel = [e for e in jit if e['ea'] >= 0xc0000000]
    print(f"\nGeneric kernel-range entries: {len(gen_kernel)}")
    print(f"JIT kernel-range entries:     {len(jit_kernel)}")

    # Find first DEC write (DEC changes from 0)
    for e in generic:
        if e['dec'] != 0:
            print(f"\nGeneric: first non-zero DEC at seq={e['seq']} pc={e['pc']:08x} dec={e['dec']:08x}")
            break

    for e in jit:
        if e['dec'] != 0:
            print(f"JIT:     first non-zero DEC at seq={e['seq']} pc={e['pc']:08x} dec={e['dec']:08x}")
            break

    if args.pc is not None:
        # Compare at a specific EA
        gen_at_pc = [e for e in generic if e['ea'] == args.pc]
        jit_at_pc = [e for e in jit if e['ea'] == args.pc]
        print(f"\nEntries at EA {args.pc:08x}: generic={len(gen_at_pc)} jit={len(jit_at_pc)}")
        n = min(len(gen_at_pc), len(jit_at_pc), args.max_diffs)
        for i in range(n):
            diffs = compare_entries(gen_at_pc[i], jit_at_pc[i])
            if diffs:
                print(f"  [{i}] DIVERGE gen.seq={gen_at_pc[i]['seq']} jit.seq={jit_at_pc[i]['seq']}:")
                for f, gv, jv in diffs:
                    print(f"    {f}: generic={gv:08x} jit={jv:08x}")
            else:
                print(f"  [{i}] MATCH gen.seq={gen_at_pc[i]['seq']} jit.seq={jit_at_pc[i]['seq']}")
    else:
        # Find divergences at common EAs (match by order of occurrence)
        gen_idx = {}
        for i, e in enumerate(generic):
            ea = e['ea']
            if ea not in gen_idx:
                gen_idx[ea] = []
            gen_idx[ea].append((i, e))
        jit_idx = {}
        for i, e in enumerate(jit):
            ea = e['ea']
            if ea not in jit_idx:
                jit_idx[ea] = []
            jit_idx[ea].append((i, e))

        divergences = []
        for ea in sorted(common):
            gen_list = gen_idx.get(ea, [])
            jit_list = jit_idx.get(ea, [])
            n = min(len(gen_list), len(jit_list))
            for i in range(n):
                gi, ge = gen_list[i]
                ji, je = jit_list[i]
                diffs = compare_entries(ge, je)
                if diffs:
                    divergences.append((ea, ge, je, diffs))
                    if len(divergences) >= args.max_diffs:
                        break
            if len(divergences) >= args.max_diffs:
                break

        print(f"\nFirst {len(divergences)} divergences at common EAs:")
        for ea, ge, je, diffs in divergences:
            print(f"  EA={ea:08x} gen.seq={ge['seq']} jit.seq={je['seq']}:")
            for f, gv, jv in diffs:
                print(f"    {f}: generic={gv:08x} jit={jv:08x}")


if __name__ == '__main__':
    main()
