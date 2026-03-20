#!/bin/bash
# Run all PearPC headless test ELFs and report results.
# Usage: test/run_tests.sh [path/to/ppc] [timeout_seconds]
#
# Each test is run with a timeout (default 30s). If a test hangs
# (e.g. infinite loop in JIT), it is killed and reported as TIMEOUT.

set -euo pipefail

PPC="${1:-./src/ppc}"
TIMEOUT="${2:-30}"

if [ ! -x "$PPC" ]; then
    echo "ERROR: $PPC not found or not executable (build first)" >&2
    exit 1
fi

TESTS=(
    test/test_loop.cfg
    test/test_alu.cfg
    test/test_mem.cfg
    test/test_dsi.cfg
    test/test_branch_loop.cfg
    test/test_fpu_exc.cfg
    test/test_fpu_arith.cfg
    test/test_altivec.cfg
    test/test_crlogical.cfg
)

passed=0
failed=0
skipped=0
failures=()

for cfg in "${TESTS[@]}"; do
    name="${cfg##*/}"
    name="${name%.cfg}"
    elf="${cfg%.cfg}.elf"

    if [ ! -f "$elf" ]; then
        printf "%-24s SKIP (no .elf)\n" "$name"
        ((skipped++))
        continue
    fi

    if output=$(timeout "$TIMEOUT" "$PPC" --headless "$cfg" 2>&1); then
        printf "%-24s PASS\n" "$name"
        ((passed++))
    else
        rc=$?
        if [ $rc -eq 124 ]; then
            printf "%-24s TIMEOUT (${TIMEOUT}s)\n" "$name"
        else
            printf "%-24s FAIL (exit $rc)\n" "$name"
            echo "$output" | tail -5 | sed 's/^/  /'
        fi
        failures+=("$name")
        ((failed++))
    fi
done

echo ""
echo "=== Results: $passed passed, $failed failed, $skipped skipped ==="

if [ ${#failures[@]} -gt 0 ]; then
    echo "Failed: ${failures[*]}"
    exit 1
fi
