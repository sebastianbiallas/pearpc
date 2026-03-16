# PearPC - PowerPC Architecture Emulator

PearPC is an architecture-independent PowerPC platform emulator capable of running most PowerPC operating systems.

## Features

- **CPU emulation**: G3/G4 with AltiVec support
  - **Generic interpreter**: Portable to any host architecture (including ARM64/Apple Silicon)
  - **JITC x86**: JIT compiler for x86 hosts (~15x slower than native)
  - **JITC x86_64**: JIT compiler for x86_64 hosts
- **IDE controller** (CMD646 with bus-mastering): hard disks and CD-ROM via image files or host devices
- **PCI bridge**: enough to work with
- **Heathrow PIC**: programmable interrupt controller
- **VIA-CUDA**: with attached ADB keyboard and mouse
- **Network**: 3Com 3C90x or Realtek 8139 via host ethernet tunnel
- **NVRAM**: 8 KiB non-volatile storage
- **USB**: placeholder hub (sufficient for client OS detection)
- **Open Firmware PROM**: boots Yaboot and BootX from HFS/HFS+ partitions

## Tested Client Operating Systems

The following operating systems run (to some extent) inside PearPC:

- **Mandrake Linux 9.1 PPC**: Runs well
- **Darwin PPC**: Runs well
- **Mac OS X 10.3**: Runs well with some caveats

## Building

### Prerequisites

- C++ compiler with C++11 support (GCC or Clang)
- GNU Autotools (autoconf, automake)
- pkg-config
- SDL3 development libraries

On macOS:
```sh
brew install autoconf automake sdl3 pkg-config
```

### Compile

```sh
./autogen.sh
./configure
make
```

The configure script auto-detects your platform. You can override with:
- `--enable-cpu=generic|jitc_x86|jitc_x86_64`
- `--enable-ui=sdl|x11|win32|beos`

The binary is produced at `src/ppc`.

### Platform Notes

| Host | CPU Backend | UI Backend |
|------|-------------|------------|
| macOS (Apple Silicon) | generic (interpreter) | SDL3 |
| macOS (Intel) | jitc_x86_64 or generic | SDL3 |
| Linux (x86_64) | jitc_x86_64 | SDL3 or X11 |
| Windows | jitc_x86 or jitc_x86_64 | Win32 native |

## Quick Start

### 1. Get a client OS image

Download [Mandrake Linux 9.1 PPC](https://archive.org/details/MandrakeLinux9.1ppc) from the Internet Archive.

### 2. Configure

Copy `ppccfg.example` and edit it, or use the provided `ppccfg.test` for a quick CD boot:

```sh
# Place the ISO in the project root, then:
./src/ppc ppccfg.test
```

### 3. Run

```sh
./src/ppc your-config-file
```

## Usage

### Keyboard

Keypresses are sent directly to the client when the PearPC window is focused. PearPC uses a raw keyboard layout -- configure your preferred layout in the client OS.

Press **F11** to enter compose mode for key combinations that your host window manager intercepts (e.g., to send Ctrl+Alt+Del: press F11, then Ctrl, Alt, Del, then F11 again).

If the keyboard behaves strangely after Alt-Tab, press **Alt+Ctrl+Shift** to reset.

### Mouse

The client mouse is independent of the host mouse. Press **F12** to toggle mouse grab (switch between host and client mouse). The window title indicates the current mode.

### Key Bindings

| Key | Action |
|-----|--------|
| F11 | Compose mode (for intercepted key combos) |
| F12 | Toggle mouse grab |
| Alt+Return | Toggle fullscreen |

These can be changed in the config file.

## Configuration Reference

PearPC uses plain text configuration files. See `ppccfg.example` for a fully commented example. Key options:

| Option | Default | Description |
|--------|---------|-------------|
| `ppc_start_resolution` | `"800x600x15"` | Window size and depth (15 or 32) |
| `ppc_start_full_screen` | `0` | Start in fullscreen mode |
| `redraw_interval_msec` | `20` | Redraw interval (10-500ms) |
| `memory_size` | `0x8000000` | RAM size (min 64 MiB) |
| `cpu_pvr` | `0x00088302` | Processor version: G3=0x00088302, G4=0x000c0201 |
| `prom_bootmethod` | `"auto"` | Boot method: auto, select, or force |
| `prom_driver_graphic` | | Set to `"video.x"` for Mac OS X |
| `pci_ide0_master_installed` | `0` | Enable IDE master (HDD or CD-ROM) |
| `pci_ide0_master_image` | | Path to disk image (.img) or ISO (.iso) |
| `pci_ide0_master_type` | | `"hd"`, `"cdrom"`, or `"nativecdrom"` |
| `pci_ide0_slave_installed` | `0` | Enable IDE slave |
| `pci_3c90x_installed` | `0` | Enable 3Com NIC |
| `pci_rtl8139_installed` | `0` | Enable Realtek NIC |
| `nvram_file` | `"nvram"` | NVRAM storage file |

## Architecture

See [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md) for a detailed description of the codebase.

## Authors

- **Sebastian Biallas** - Main developer
- **Stefan Weyergraf** - Major contributions
- **Daniel Foesch** - AltiVec and core updates

See [AUTHORS](AUTHORS) for the full list of contributors.

Contains code from: [HT Editor](https://hte.sf.net), Linux, [Bochs](https://bochs.sf.net), [MOL](https://www.maconlinux.org/), Yaboot, HFS/HFS+ utils.

## License

GPLv2. See [COPYING](COPYING).
