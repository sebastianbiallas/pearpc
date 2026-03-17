# PearPC Test ELFs

Minimal PPC ELF test programs for validating the JIT compiler.

## How tests work

Each test is a bare-metal PPC assembly program (`.S` file) that runs
directly on the emulated CPU. No OS, no libc. The PROM's ELF loader
(`mapped_load_elf()` in `src/io/prom/promboot.cc`) loads the ELF into
memory with 1:1 page table mappings and jumps to `_start`.

Tests use custom opcodes for I/O:
- `.long 0x00333303` — print string (`r3` = address, `r4` = length)
- `.long 0x00333304` — exit (`r3` = exit code, 0 = success)

These are handled in `ppc_opc_special()` in `ppc_dec.cc`.

## Memory layout

- Code loaded at `0x100000` (defined by `test_loop.ld` linker script)
- Stack set up by the ELF loader (mapped pages near the code)
- Page table at PA `0x300000` (set up by the PROM, SDR1 = `0x300003`)
- MMU is ON (MSR = `0x2030`: IR=1, DR=1)

## Building

Tests are cross-compiled using a PPC cross-compiler in Docker (amd64):

```sh
docker run --rm --platform linux/amd64 \
    -v $(pwd)/test:/work -w /work \
    debian:bookworm bash -c '
    apt-get update -qq >/dev/null 2>&1 &&
    apt-get install -y -qq gcc-powerpc-linux-gnu binutils-powerpc-linux-gnu >/dev/null 2>&1 &&
    powerpc-linux-gnu-as -mbig -mregnames -o TEST.o TEST.S &&
    powerpc-linux-gnu-ld -T test_loop.ld -o TEST.elf TEST.o
'
```

Or use the helper script for `test_loop`:
```sh
./test/build_ppc_elf.sh
```

## Running

```sh
# With GUI (use for boot tests):
./src/ppc test/test_loop.cfg

# Headless (use for automated tests):
./src/ppc --headless test/test_alu.cfg
```

Exit code 0 = all tests passed. Nonzero = number of failures.

## Test programs

| Test | Config | Description |
|------|--------|-------------|
| `test_loop.S` | `test_loop.cfg` | Hello world + counting loop. Basic JIT validation. |
| `test_alu.S` | `test_alu.cfg` | 22 ALU tests: addi, addis, add, subf, ori, and, xor, slw, srw, neg, mullw, oris, xori, xoris, or, stw/lwz, stb/lbz, sth/lhz, rlwinm, cmp/branch, mtspr/mfspr, mfcr/cmpwi. |
| `test_mem.S` | `test_mem.cfg` | 18 memory tests: word/half/byte store/load, byte-in-word extraction, multi-page stride, 1024-word XOR loop, cross-size access patterns. |
| `test_dsi.S` | `test_dsi.cfg` | DSI exception handling: installs handler at vector 0x300, accesses unmapped pages to trigger DSI, handler creates PTE and returns via rfi, verifies retry succeeds. Tests lwz/stw/sth/stb/lhz/lbz through DSI-mapped pages. |

## Writing a new test

1. Create `test_foo.S` with `_start` as entry point
2. Use the `PRINT` and `CHECK`/`CHECK32` macros from existing tests
3. Keep a fail counter in `r30`, exit with `.long 0x00333304`
4. Create `test_foo.cfg` (copy from `test_alu.cfg`, change `prom_loadfile`)
5. Build with Docker (see above)
6. All linker scripts use `test_loop.ld` (loads at `0x100000`)

## Linker script

`test_loop.ld` places all code at address `0x100000`:
```
ENTRY(_start)
SECTIONS {
    . = 0x100000;
    .text : { *(.text) }
}
```
