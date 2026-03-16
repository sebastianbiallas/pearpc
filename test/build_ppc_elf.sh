#!/bin/bash
#
# Build a minimal PPC ELF using Docker with Debian's cross-compiler.
# Outputs the ELF to test/test_loop.elf
#
# Usage: ./test/build_ppc_elf.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT="$SCRIPT_DIR/test_loop.elf"

echo "=== Building PPC ELF via Docker ==="

docker run --rm \
    -v "$SCRIPT_DIR:/work" \
    -w /work \
    debian:bookworm \
    bash -c '
        apt-get update -qq && \
        apt-get install -y -qq gcc-powerpc-linux-gnu binutils-powerpc-linux-gnu >/dev/null 2>&1 && \
        echo "Cross-compiler installed." && \
        powerpc-linux-gnu-as -mbig -mregnames -o test_loop.o test_loop.S && \
        powerpc-linux-gnu-ld -T test_loop.ld -o test_loop.elf test_loop.o && \
        powerpc-linux-gnu-objdump -d test_loop.elf && \
        echo "---" && \
        powerpc-linux-gnu-readelf -h test_loop.elf && \
        echo "Build successful: test_loop.elf"
    '

echo ""
echo "Output: $OUTPUT"
ls -la "$OUTPUT"
