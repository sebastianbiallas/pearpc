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

- **macOS (Apple Silicon)**: Uses the generic CPU interpreter + SDL3. Builds and runs natively as arm64.
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

## License

GPLv2. See [COPYING](COPYING).
