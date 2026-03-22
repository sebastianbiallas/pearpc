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

- Code (`.text`) loaded at `0x100000` (defined by `test_loop.ld` linker script)
- Data (`.data`/`.bss`) at `0x200000` — must be on a separate page from code to avoid JIT self-modifying code issues
- Stack set up by the ELF loader (mapped pages near the code)
- Page table at PA `0x300000` (set up by the PROM, SDR1 = `0x300003`)
- MMU is ON (MSR = `0x2030`: IR=1, DR=1)

**Important:** Do not use addresses within the `.text` section for scratch data — it overwrites your own instructions. Use `.data` section labels instead.

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

Run all tests:
```sh
make test
# or directly:
test/run_tests.sh
```

Each test is run with a 30-second timeout. If a test hangs (e.g. infinite
loop due to a JIT bug), it is killed and reported as `TIMEOUT`. You can
override the timeout:
```sh
test/run_tests.sh ./src/ppc 60   # 60-second timeout per test
```

Run a single test:
```sh
# Headless:
./src/ppc --headless test/test_alu.cfg

# With GUI (use for boot tests):
./src/ppc test/test_loop.cfg
```

Exit code 0 = all tests passed. Nonzero = number of failures.

## Test programs

| Test | Config | Description |
|------|--------|-------------|
| `test_loop.S` | `test_loop.cfg` | Hello world + counting loop. Basic JIT validation. |
| `test_alu.S` | `test_alu.cfg` | 51 ALU tests: addi, addis, add, subf, ori, and, xor, slw, srw, neg, mullw, oris, xori, xoris, or, stw/lwz, stb/lbz, sth/lhz, rlwinm, cmp/branch, mtspr/mfspr, mfcr/cmpwi, mfmsr, mtmsr, mulli, mulhwu, rlwimi, nor, orc, cntlzw, subfic, lwbrx, stwbrx, divwu, addic, adde, subfe, srawi, lwarx/stwcx. reservation semantics. |
| `test_mem.S` | `test_mem.cfg` | 18 memory tests: word/half/byte store/load, byte-in-word extraction, multi-page stride, 1024-word XOR loop, cross-size access patterns. |
| `test_dsi.S` | `test_dsi.cfg` | DSI exception handling: installs handler at vector 0x300, accesses unmapped pages to trigger DSI, handler creates PTE and returns via rfi, verifies retry succeeds. Tests lwz/stw/sth/stb/lhz/lbz through DSI-mapped pages. |
| `test_branch_loop.S` | `test_branch_loop.cfg` | 20 branch tests: counted loops with `bl` calls inside (same-page `ble` + `bl` dispatch), `bdnz`/`bdz`, `bctr`/`bctrl`/`blrl`, conditional `bclr` variants (`beqlr`, `bnelr`, `bltlr`, `bgelr`, `bgtlr`, `blelr`) testing both taken and not-taken paths. |
| `test_fpu_arith.S` | `test_fpu_arith.cfg` | 48 FPU tests: fabs, fnabs, fadd/fsub/fmul/fdiv (double+single), fmadd/fmsub/fnmadd/fnmsub (double+single), fsqrt, fcmpu, frsp, fctiwz, fsel, lfs/stfs/lfsu/stfsu (single↔double conversion, rA update), FPSCR rounding modes (mffs, mtfsfi, fdiv 10/3 under RN=0/1/2/3, negative under RN=3). |
| `test_fpu_exc.S` | `test_fpu_exc.cfg` | 24 FPU tests: NO_FPU exception handling (installs handler at 0x800, verifies lfd/fadd/stfd/fdivs/lfs/stfs raise NO_FPU when MSR_FP=0, checks SRR0/SRR1), NO_FPU vs DSI priority, fmr (64-bit copy), fneg (sign bit flip for +val, -val, -0.0). |
| `test_altivec.S` | `test_altivec.cfg` | 12 AltiVec tests: MSR_VEC enable via rfi, vxor, vspltisw, vspltisb, vadduwm, vsubuwm, vand, vaddubm, vmrghw, vcmpequw. (with CR6), lvx/stvx round-trip, vspltw. |
| `test_crlogical.S` | `test_crlogical.cfg` | CR logical operations: crand, crandc, cror, crorc, crxor, crnand, crnor, creqv, plus crclr/crset aliases. |

## Writing a new test

1. Create `test_foo.S` with `_start` as entry point
2. Use the `PRINT` and `CHECK`/`CHECK32` macros from existing tests
3. Keep a fail counter in `r30`, exit with `.long 0x00333304`
4. Create `test_foo.cfg` (copy from `test_alu.cfg`, change `prom_loadfile`)
5. Build with Docker (see above)
6. All linker scripts use `test_loop.ld` (loads at `0x100000`)

## Linker script

`test_loop.ld` places code at `0x100000` and data at `0x200000`:
```
ENTRY(_start)
SECTIONS {
    . = 0x100000;
    .text : { *(.text) }
    . = 0x200000;
    .data : { *(.data) }
    .bss  : { *(.bss) }
}
```
