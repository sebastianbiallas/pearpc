# PearPC

PearPC is an architecture-independent PowerPC platform emulator capable of running most PowerPC operating systems.

## What is this?

PearPC emulates a PowerPC G3/G4-based Macintosh system, including CPU, memory management, PCI bus, and peripheral devices. It can boot Mac OS X and Linux distributions compiled for PowerPC (such as Mandrake Linux PPC).

## Building

### Prerequisites

- C++ compiler (GCC or Clang with C++11 support)
- GNU Autotools (autoconf, automake)
- pkg-config
- SDL3 development libraries (`brew install sdl3` on macOS)

### Build Steps

```sh
./autogen.sh
./configure --enable-ui=sdl
make
```

The binary is produced at `src/ppc`.

### Platform Notes

- **macOS (Apple Silicon)**: Uses the aarch64 JIT compiler (`cpu_jitc_aarch64`) + SDL3. Builds and runs natively as arm64. Falls back to generic CPU interpreter if JIT is disabled.
- **macOS (Intel)**: Can use either the generic interpreter or the x86_64 JIT compiler.
- **Linux (x86_64)**: Can use x86_64 JIT for best performance. UI options: SDL, X11.
- **Windows**: Win32 native UI, x86 or x86_64 JIT.

## Running

```sh
./src/ppc <configfile>
```

See `ppccfg.example` for a full configuration reference. A minimal test config `ppccfg.test` is provided for booting from a CD ISO image.

### Quick Start with Mandrake Linux PPC

1. Download Mandrake Linux 9.1 PPC CD1 from https://archive.org/details/MandrakeLinux9.1ppc
2. Place `MandrakeLinux-9.1-CD1.ppc.iso` in the project root
3. Run: `./src/ppc ppccfg.test`
4. Press **F12** to toggle mouse grab

## Architecture

See [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md) for a detailed description of the codebase structure, CPU emulation backends, I/O subsystem, platform abstraction layers, and boot sequence.

## Key Directories

| Directory | Purpose |
|---|---|
| `src/cpu/` | CPU emulation (interpreter + JIT backends) |
| `src/io/` | Emulated devices (PCI, IDE, graphics, network, etc.) |
| `src/system/` | Platform abstraction (display, input, threading, OS API) |
| `src/debug/` | Debugger, PPC/x86 disassemblers |
| `src/tools/` | Utility libraries (containers, strings, streams) |
| `scripts/debug/` | Debug tools for analyzing memory dumps, kernel logs, PPC disassembly |
| `test/` | Test programs (PPC ELFs, test harnesses) |

## Debugging

See [doc/AGENT_DEBUGGING.md](doc/AGENT_DEBUGGING.md) for the full debugging methodology, including how to diagnose JIT kernel boot issues, what tools to use, and common pitfalls.

## Code Style

The existing codebase uses tabs for indentation. **New code should use spaces** (4 per indent level). A `.clang-format` file is provided to enforce this.

### Style Summary

- **Indentation**: 4 spaces (no tabs in new code)
- **Braces**: opening brace on same line for control structures, next line for functions
- **Pointers**: `Type *name` (star attached to the name)
- **Line length**: ~120 characters
- **Includes**: don't reorder (preserve grouping)

### Applying

Format new or changed files only -- do not reformat the entire codebase:

```sh
clang-format -i src/cpu/cpu_jitc_aarch64/myfile.cc
```

### What Not to Reformat

Legacy code in `src/cpu/cpu_jitc_x86/`, `src/cpu/cpu_jitc_x86_64/`, `src/cpu/cpu_generic/`, and `src/io/` uses tabs. Leave it as-is unless you're making substantive changes to a file.

## License

GPLv2. See [COPYING](COPYING).
