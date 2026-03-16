# PearPC Architecture

PearPC is a PowerPC (G4) platform emulator that emulates a complete Macintosh-like system, including CPU, memory management, PCI bus, and peripheral devices. It can boot Mac OS X and Linux/PPC.

## High-Level Overview

```
┌─────────────────────────────────────────────────────────┐
│                      main.cc                            │
│               (config, init, boot sequence)             │
├───────────┬──────────┬──────────┬───────────────────────┤
│  CPU      │  I/O     │  System  │  Tools                │
│  Engine   │  Devices │  Layer   │  (data structs, etc.) │
├───────────┼──────────┼──────────┼───────────────────────┤
│ generic   │ pci/     │ osapi/   │ data.h (containers)   │
│ jitc_x86  │ graphic/ │  posix   │ stream.h (I/O)        │
│ jitc_x64  │ ide/     │  win32   │ str.h (strings)       │
│           │ cuda/    │  beos    │ configfile.h          │
│           │ prom/    │ ui/      │ snprintf.h            │
│           │ pic/     │  sdl     │ except.h              │
│           │ nvram/   │  x11     │ endianess.h           │
│           │ macio/   │  win32   │ crc32.h               │
│           │ 3c90x/   │  beos    │                       │
│           │ rtl8139/ │  gtk     │                       │
│           │ serial/  │  qt      │                       │
│           │ usb/     │ arch/    │                       │
│           │          │  generic │                       │
│           │          │  x86     │                       │
│           │          │  x86_64  │                       │
└───────────┴──────────┴──────────┴───────────────────────┘
```

## Source Layout

```
src/
├── main.cc              Entry point, boot sequence
├── configparser.cc/h    Configuration file parser
├── info.h               Version and identity constants
├── cpu/                  CPU emulation
│   ├── cpu.h            Public CPU interface
│   ├── common.h         Shared PPC types (registers, CR, vectors)
│   ├── mem.h            Memory access interface
│   ├── cpu_generic/     Pure C++ interpreter (portable)
│   ├── cpu_jitc_x86/    x86 32-bit JIT compiler
│   └── cpu_jitc_x86_64/ x86_64 JIT compiler
├── io/                   Emulated I/O devices
│   ├── io.h/cc          Memory-mapped I/O dispatcher
│   ├── pci/             PCI bus and bridge
│   ├── graphic/         Graphics card (frame buffer)
│   ├── ide/             IDE/ATAPI controller (disks, CD-ROM)
│   ├── cuda/            CUDA controller (ADB keyboard/mouse, power)
│   ├── prom/            Open Firmware boot ROM
│   │   └── fs/          Filesystem support (HFS, HFS+)
│   ├── pic/             Programmable interrupt controller
│   ├── nvram/           Non-volatile RAM
│   ├── macio/           Mac I/O controller
│   ├── 3c90x/           3Com 90x NIC
│   ├── rtl8139/         Realtek 8139 NIC
│   ├── serial/          Serial port
│   └── usb/             USB controller (stub)
├── system/               Platform abstraction layer
│   ├── display.h/cc     Display abstraction (SystemDisplay base class)
│   ├── keyboard.h/cc    Keyboard abstraction (ADB keycodes)
│   ├── mouse.h/cc       Mouse abstraction
│   ├── event.h          Event system (key/mouse events)
│   ├── device.h/cc      Device base class (SystemDevice)
│   ├── systhread.h      Threading primitives
│   ├── systimer.h       Timer abstraction
│   ├── sysclk.h         Clock interface
│   ├── sysvm.h          Virtual memory management
│   ├── sysvaccel.h      Video acceleration
│   ├── types.h          Platform types and compiler macros
│   ├── osapi/           OS API implementations
│   │   ├── posix/       Linux, FreeBSD, macOS (pthreads, mmap, etc.)
│   │   ├── win32/       Windows (Win32 API, SCSI/ASPI)
│   │   └── beos/        BeOS/Haiku
│   ├── ui/              UI backend implementations
│   │   ├── sdl/         SDL3 (cross-platform, default on macOS)
│   │   ├── x11/         X Window System
│   │   ├── win32/       Windows native
│   │   ├── beos/        BeOS/Haiku native
│   │   ├── gtk/         GTK+ (stub)
│   │   └── qt/          Qt (stub)
│   └── arch/            Host architecture specifics
│       ├── generic/     Portable fallback
│       ├── x86/         x86 optimizations (video accel ASM)
│       └── x86_64/      x86_64 optimizations (video accel ASM)
├── debug/                Debugger and disassemblers
│   ├── debugger.h/cc    Interactive debugger
│   ├── ppcdis.h/cc      PowerPC disassembler
│   ├── x86dis.h/cc      x86 disassembler (for JIT debugging)
│   └── asm.h/cc         Assembler (LEX/YACC-based)
└── tools/                Utility libraries
    ├── data.h/cc        Container framework (Array, List, AVLTree, etc.)
    ├── stream.h/cc      Stream I/O abstraction
    ├── str.h/cc         String class
    ├── endianess.h/cc   Byte order conversion
    ├── except.h/cc      Exception hierarchy
    ├── snprintf.h/cc    Printf implementation
    └── crc32.h/cc       CRC-32
```

## CPU Emulation

PearPC emulates a PowerPC G3/G4 (32-bit PPC ISA) with three backend implementations, selected at compile time:

### Generic Interpreter (`cpu_generic/`)

Pure C++ interpreter. Decodes and executes one PPC instruction at a time. Portable to any host architecture but slow. This is the only option for ARM64 (Apple Silicon) hosts.

Key modules mirror PPC functional units:
- `ppc_alu.cc` - Integer arithmetic/logic
- `ppc_fpu.cc` - Floating-point unit
- `ppc_vec.cc` - AltiVec/VMX vector unit
- `ppc_mmu.cc` - Memory management (page tables, TLB, BATs)
- `ppc_opc.cc` - Instruction decode and dispatch
- `ppc_exc.cc` - Exception handling
- `ppc_dec.cc` - Decrementer (timer)

### x86 JIT Compiler (`cpu_jitc_x86/`)

Translates PPC instructions to native x86 (32-bit) machine code at runtime. Includes:
- `jitc.cc/h` - Translation cache and fragment management
- `x86asm.cc/h` - x86 code emitter
- `jitc_mmu.S` - MMU fast path in assembly (~1700 lines)
- `jitc_tools.S` - Exception handlers, heartbeat in assembly (~850 lines)

### x86_64 JIT Compiler (`cpu_jitc_x86_64/`)

Same architecture as the x86 JIT but targeting x86_64. Takes advantage of 16 GPRs and 16 XMM registers for better register allocation.

### CPU Interface (`cpu.h`)

All three backends implement the same interface:
- `ppc_cpu_init()` / `ppc_cpu_run()` / `ppc_cpu_stop()` - lifecycle
- `ppc_cpu_get_gpr()` / `ppc_cpu_set_gpr()` - register access
- `ppc_cpu_raise_ext_exception()` - external interrupt injection
- `ppc_cpu_get_pvr()` - processor version (G3 or G4 selectable)

## Memory-Mapped I/O

The I/O dispatcher (`io/io.cc`) routes memory accesses by physical address to the appropriate device:

| Address Range | Device |
|---|---|
| `0x80000000-0x81000000` | PCI device memory |
| `0x80800000` area | PIC (interrupt controller) |
| `0x80816000` area | CUDA (ADB/power management) |
| `0x80860000` area | NVRAM |
| `0x84000000-0x85000000` | Graphics frame buffer |
| `0xFE000000-0xFE200000` | ISA bus |
| `0xFEC00000-0xFEF00000` | PCI configuration space |

All I/O devices are `PCI_Device` subclasses registered on the PCI bus, with 64 configuration registers and up to 6 I/O resource regions.

## Platform Abstraction

PearPC uses a three-layer platform abstraction:

### OS API Layer (`system/osapi/`)

Low-level OS primitives: threading (`systhread.h`), timers (`systimer.h`), file I/O, virtual memory (`sysvm.h`), CD-ROM access, Ethernet TUN/TAP. Three implementations: POSIX (Linux/macOS/FreeBSD), Win32, BeOS.

### Architecture Layer (`system/arch/`)

Host CPU-specific code: endianness handling, video pixel format conversion (with optional ASM acceleration on x86/x86_64), feature detection.

### UI Layer (`system/ui/`)

Display rendering, keyboard/mouse input, and GUI dialogs. Each backend implements:
- `initUI()` - create display, keyboard, mouse
- `runUI()` - run event loop (SDL: main thread; others: separate thread)
- `doneUI()` - cleanup
- `SystemDisplay` subclass - rendering, mode switching, mouse grab
- `SystemKeyboard` subclass - LED state, key event dispatch
- `SystemMouse` subclass - button/motion event dispatch

The `SystemDisplay` base class provides:
- Frame buffer management with damage tracking for incremental redraws
- Built-in VT100 virtual terminal (character display on top of graphics)
- Pixel format conversion between client (emulated) and host display

## Boot Sequence

1. `main()` loads config file, initializes subsystems
2. Physical memory allocated, CPU initialized
3. CUDA pre-initialized (power management)
4. UI created (`initUI`), I/O devices initialized
5. Page table set up at configured physical address
6. Open Firmware (`prom_init`) initializes device tree
7. Boot file loaded from configured device (CD/HD)
8. CPU starts executing (`ppc_cpu_run` in background thread)
9. UI event loop runs on main thread (`runUI`)

## Configuration

Runtime configuration via text file (see `ppccfg.example`):
- Display: resolution, fullscreen, redraw interval
- Boot: method (auto/select/force), kernel args
- Devices: IDE master/slave (HDD images, ISO files, native CD-ROM)
- Network: 3c90x or RTL8139 NIC with MAC address
- CPU: PVR selection (G3/G4), memory size

## Build System

GNU Autotools (autoconf + automake). Key configure options:
- `--enable-cpu=generic|jitc_x86|jitc_x86_64` (auto-detected from host)
- `--enable-ui=sdl|x11|win32|beos|gtk|qt` (platform-specific default)
- `--enable-debug`, `--enable-release`, `--enable-profiling`

Dependencies: SDL3 (for SDL UI), pthreads, pkg-config. Optional: X11 libraries, Qt, GTK.
