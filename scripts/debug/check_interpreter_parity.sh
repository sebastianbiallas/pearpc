#!/bin/bash
#
# Check that aarch64 JIT interpreter functions match generic CPU behavior.
# Finds commented-out ppc_exception() calls, missing exception handling,
# and other discrepancies that could cause silent correctness bugs.
#
# Usage: ./scripts/debug/check_interpreter_parity.sh
#

GENERIC=src/cpu/cpu_generic
AARCH64=src/cpu/cpu_jitc_aarch64

echo "=== Commented-out ppc_exception() calls ==="
echo "(These are silently dropped exceptions — bugs!)"
echo ""
if grep -n '//[[:space:]]*\(return \)\?ppc_exception(' "$AARCH64"/*.cc | grep -v 'already set'; then
    echo ""
    echo "*** FOUND commented-out exceptions above — these need to be enabled"
else
    echo "None found. Good."
fi

echo ""
echo "=== ppc_exception() in generic but not in aarch64 ==="
echo "(Functions where generic raises exceptions but aarch64 doesn't)"
echo ""

# Extract function names that call ppc_exception in generic
generic_funcs=$(grep -B20 'ppc_exception' "$GENERIC"/ppc_opc.cc "$GENERIC"/ppc_alu.cc 2>/dev/null | \
    grep '^void ppc_opc_\|^int ppc_opc_' | \
    sed 's/.*ppc_opc_/ppc_opc_/' | sed 's/(.*//' | sort -u)

for func in $generic_funcs; do
    # Check if the aarch64 version also calls ppc_exception (uncommented)
    # Use sed to extract only the body of the matching function (stop at next function def)
    has_exc=$(grep -A200 "int ${func}(" "$AARCH64"/ppc_alu.cc "$AARCH64"/ppc_opc.cc 2>/dev/null | \
        sed -n '1p; 2,/^[a-zA-Z].*ppc_opc_/{ /^[a-zA-Z].*ppc_opc_/q; p; }' | \
        grep -v '//' | grep 'ppc_exception' | head -1)
    if [ -z "$has_exc" ]; then
        echo "  MISSING: $func — generic calls ppc_exception, aarch64 does not"
    fi
done

echo ""
echo "=== MSR_POW handling ==="
if grep -q 'MSR_POW' "$AARCH64"/ppc_opc.cc; then
    echo "OK: aarch64 ppc_set_msr handles MSR_POW"
else
    echo "*** MISSING: aarch64 ppc_set_msr does not handle MSR_POW"
fi

echo ""
echo "=== GEN_INTERPRET wrapper check ==="
echo "(Opcodes using GEN_INTERPRET that have ppc_exception in their interpreter)"
echo ""

# Find all GEN_INTERPRET (not LOADSTORE/BRANCH/ENDBLOCK) opcodes
gen_interpret_only=$(grep 'GEN_INTERPRET(' "$AARCH64"/ppc_dec.cc | \
    grep -v 'LOADSTORE\|BRANCH\|ENDBLOCK' | \
    sed 's/.*GEN_INTERPRET(//;s/).*//')

for opc in $gen_interpret_only; do
    # Check if the interpreter function calls ppc_exception
    # Use sed to extract only the body of the matching function (stop at next function def)
    has_exc=$(grep -A200 "int ppc_opc_${opc}(" "$AARCH64"/ppc_alu.cc "$AARCH64"/ppc_opc.cc 2>/dev/null | \
        sed -n '1p; 2,/^[a-zA-Z].*ppc_opc_/{ /^[a-zA-Z].*ppc_opc_/q; p; }' | \
        grep -v '//' | grep 'ppc_exception' | head -1)
    if [ -n "$has_exc" ]; then
        echo "  WARNING: $opc uses GEN_INTERPRET but calls ppc_exception — exception will be silently dropped!"
    fi
done

echo ""
echo "=== Done ==="
