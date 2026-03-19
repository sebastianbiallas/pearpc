#!/usr/bin/env python3
"""Audit JIT interpreter stubs against generic CPU implementations.

Finds JIT interpreter functions that are no-op stubs (empty bodies,
return-only) where the generic CPU has a real implementation. These
are the #1 source of silent JIT boot failures.

Usage:
  python3 scripts/debug/stub_audit.py [opcode-name]

Without arguments: scan all JIT interpreter functions.
With an opcode name: show side-by-side diff for that specific opcode.
"""

import re
import sys
import os

GENERIC_DIR = 'src/cpu/cpu_generic'
JIT_DIR = 'src/cpu/cpu_jitc_aarch64'


def extract_functions(directory, pattern):
    """Extract function bodies matching pattern from all .cc files in directory."""
    funcs = {}
    for fname in sorted(os.listdir(directory)):
        if not fname.endswith('.cc'):
            continue
        path = os.path.join(directory, fname)
        with open(path, 'r') as f:
            text = f.read()
        # Find function definitions matching pattern
        # Match: return_type ppc_opc_NAME(args) { ... }
        for m in re.finditer(
            r'^(?:int|void|static\s+\w+)\s+(ppc_opc_\w+)\s*\([^)]*\)\s*\{',
            text, re.MULTILINE
        ):
            name = m.group(1)
            if not re.search(pattern, name):
                continue
            # Extract body by counting braces
            start = m.end() - 1  # the opening {
            depth = 0
            end = start
            for i in range(start, len(text)):
                if text[i] == '{':
                    depth += 1
                elif text[i] == '}':
                    depth -= 1
                    if depth == 0:
                        end = i + 1
                        break
            body = text[start:end].strip()
            funcs[name] = {
                'file': fname,
                'body': body,
                'lines': body.count('\n') + 1,
            }
    return funcs


def is_stub(body):
    """Check if a function body is a no-op stub."""
    # Remove braces and whitespace
    inner = body.strip('{}').strip()
    # Empty body
    if not inner:
        return True
    # Single return statement with constant
    if re.match(r'^return\s+(0|PPC_MMU_OK)\s*;$', inner.strip()):
        return True
    return False


def main():
    opcode_filter = sys.argv[1] if len(sys.argv) > 1 else None

    if opcode_filter:
        pattern = re.escape(opcode_filter)
    else:
        pattern = r'.'  # match all

    generic = extract_functions(GENERIC_DIR, pattern)
    jit = extract_functions(JIT_DIR, pattern)

    if opcode_filter:
        # Side-by-side diff for a specific opcode
        matches = [n for n in set(list(generic.keys()) + list(jit.keys()))
                   if opcode_filter in n]
        if not matches:
            print(f'No functions matching "{opcode_filter}" found.')
            return 1
        for name in sorted(matches):
            g = generic.get(name)
            j = jit.get(name)
            print(f'=== {name} ===')
            if g:
                print(f'--- generic ({g["file"]}, {g["lines"]} lines) ---')
                print(g['body'])
            else:
                print('--- generic: NOT FOUND ---')
            print()
            if j:
                is_nop = is_stub(j['body'])
                label = ' *** NO-OP STUB ***' if is_nop else ''
                print(f'+++ jit ({j["file"]}, {j["lines"]} lines){label} +++')
                print(j['body'])
            else:
                print('+++ jit: NOT FOUND +++')
            print()
        return 0

    # Full audit: find all JIT stubs where generic has real implementation
    stubs = []
    missing = []
    ok = []

    for name in sorted(generic.keys()):
        if name.startswith('ppc_opc_gen_'):
            continue  # skip gen_ wrappers
        j = jit.get(name)
        if j is None:
            missing.append(name)
        elif is_stub(j['body']):
            g = generic[name]
            if not is_stub(g['body']):
                stubs.append((name, j['file'], g['lines']))
            # else: both are stubs, fine
        else:
            ok.append(name)

    if stubs:
        print(f'*** {len(stubs)} NO-OP STUBS (generic has real implementation): ***')
        for name, jfile, glines in stubs:
            print(f'  {name:40s}  jit: {jfile}  (generic: {glines} lines)')
        print()

    if missing:
        print(f'--- {len(missing)} functions in generic but NOT in JIT: ---')
        for name in missing:
            print(f'  {name}')
        print()

    if ok:
        print(f'OK: {len(ok)} functions have real implementations in both.')

    return 1 if stubs else 0


if __name__ == '__main__':
    sys.exit(main())
