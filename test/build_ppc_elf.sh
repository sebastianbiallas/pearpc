#!/bin/bash
#
# Build minimal PPC ELFs using Docker with Debian's cross-compiler.
#
# Usage:
#   ./test/build_ppc_elf.sh              # build all targets
#   ./test/build_ppc_elf.sh test_loop    # build only test_loop
#   ./test/build_ppc_elf.sh test_bench   # build only test_bench
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TARGET="${1:-all}"

# Generate the build script that runs inside Docker
BUILD_SCRIPT="$SCRIPT_DIR/.docker_build.sh"
cat > "$BUILD_SCRIPT" << 'SCRIPT'
#!/bin/bash
set -e
TARGET="$1"

build_asm() {
    local name="$1"
    local ld="$2"
    echo "--- Building $name (asm) ---"
    powerpc-linux-gnu-as -mbig -mregnames -o "${name}.o" "${name}.S"
    powerpc-linux-gnu-ld -T "$ld" -o "${name}.elf" "${name}.o"
    powerpc-linux-gnu-objdump -d "${name}.elf"
    echo "Build successful: ${name}.elf"
}

build_c() {
    local name="$1"
    local ld="$2"
    echo "--- Building $name (C) ---"
    powerpc-linux-gnu-gcc -c -O2 -ffreestanding -nostdlib -nostartfiles \
        -mno-eabi -mregnames -mbig-endian -fno-builtin -fno-stack-protector \
        -DDATA_SIZE=131072 -o "${name}.o" "${name}.c"
    powerpc-linux-gnu-as -mbig -mregnames -o bench_crt0.o bench_crt0.S
    powerpc-linux-gnu-ld -T "$ld" -o "${name}.elf" bench_crt0.o "${name}.o"
    powerpc-linux-gnu-objdump -d "${name}.elf"
    powerpc-linux-gnu-readelf -h "${name}.elf"
    echo "Build successful: ${name}.elf"
}

apt-get update -qq
apt-get install -y -qq gcc-powerpc-linux-gnu binutils-powerpc-linux-gnu >/dev/null 2>&1
echo "Cross-compiler installed."

# Assembly targets
for name in test_loop test_branch_loop test_copy test_alu test_mem test_dsi test_fpu_exc test_altivec; do
    if [ "$TARGET" = "all" ] || [ "$TARGET" = "$name" ]; then
        if [ -f "${name}.S" ]; then
            ld_script="${name}.ld"
            [ -f "$ld_script" ] || ld_script="test_loop.ld"
            build_asm "$name" "$ld_script"
        fi
    fi
done

# C targets
for name in test_bench; do
    if [ "$TARGET" = "all" ] || [ "$TARGET" = "$name" ]; then
        if [ -f "${name}.c" ]; then
            build_c "$name" "${name}.ld"
        fi
    fi
done

echo "=== All requested builds complete ==="
SCRIPT
chmod +x "$BUILD_SCRIPT"

echo "=== Building PPC ELFs via Docker ==="

docker run --rm \
    --platform linux/amd64 \
    -v "$SCRIPT_DIR:/work" \
    -w /work \
    debian:bookworm \
    bash .docker_build.sh "$TARGET"

rm -f "$BUILD_SCRIPT"

echo ""
echo "=== Build complete ==="
ls -la "$SCRIPT_DIR"/*.elf 2>/dev/null || true
