# Timer Architecture in PearPC (aarch64 JIT)

This document describes the timer, DEC register, timebase, PIC, and CUDA timing
mechanisms as implemented in the aarch64 JIT backend.

---

## 1. PPC Time Base (TBL/TBU)

The PowerPC Time Base is a 64-bit free-running counter stored in
`PPC_CPU_State::tb` (`ppc_cpu.h:93`). It is split into a low 32-bit half
(TBL, SPR 268) and a high 32-bit half (TBU, SPR 269).

### How `ppc_get_cpu_timebase()` works

Defined in `ppc_cpu.cc:76-87`.

Each call reads the host high-resolution clock via `sys_get_hiresclk_ticks()`
and computes the delta since the last read (`gTBreadITB`). The delta is scaled
by `gHostClockScale` and accumulated into `gCPU->tb`:

- If `gHostClockScale < 0`: `delta >> (-gHostClockScale)` (host clock faster).
- If `gHostClockScale >= 0`: `delta << gHostClockScale` (host clock slower).

`gTBreadITB` is updated after each read so the timebase advances incrementally.

There is also `ppc_get_cpu_ideal_timebase()` (`ppc_cpu.cc:66-74`) which
computes the absolute timebase from `gStartHostCLKTicks` without accumulating.
Used by the DEC register logic to avoid drift.

### gHostClockScale initialization

Computed in `ppc_cpu_init()` (`ppc_cpu.cc:271-283`). The host clock frequency
is compared against `PPC_TIMEBASE_FREQUENCY` (= `PPC_BUS_FREQUENCY / 4` =
`PPC_CLOCK_FREQUENCY / 20` = 10 MHz for the default 200 MHz clock;
see `ppc_cpu.h:33-35`). The scale factor is adjusted until the scaled host
frequency is within a factor of 2 of the target.

### Guest access to TBL/TBU

- **mftb (read)**: `ppc_opc_mftb()` in `ppc_alu.cc:2295`. SPR 268 (TBL)
  returns low 32 bits, SPR 269 (TBU) returns high 32 bits.
- **mtspr (write)**: `ppc_opc_mtspr()` in `ppc_alu.cc:2496-2498`. SPR 284
  calls `writeTBL()`, SPR 285 calls `writeTBU()`. Both in `ppc_opc.cc:81-91`.

### Host clock backend

On POSIX with `HAVE_GETTIMEOFDAY`, `sys_get_hiresclk_ticks()` returns
microseconds from `gettimeofday()`, `sys_get_hiresclk_ticks_per_second()`
returns 1,000,000. See `src/system/osapi/posix/syshiresclk.cc`.

---

## 2. DEC Register (Decrementer)

The DEC register is a 32-bit countdown timer at `PPC_CPU_State::dec`
(`ppc_cpu.h:90`). When it transitions from non-negative to negative (bit 31
set), a decrementer exception is raised.

### readDEC

Defined in `ppc_opc.cc:45-53`. Computes current DEC from ideal timebase:

```
itb = ppc_get_cpu_ideal_timebase() - gDECwriteITB
aCPU.dec = gDECwriteValue - itb
```

Called from `ppc_opc_mfspr()` for SPR 22 (`ppc_alu.cc:2162`).

### writeDEC

Defined in `ppc_opc.cc:55-79`. Two paths:

1. **Old DEC non-negative, new DEC negative**: fires callback immediately
   via `sys_set_timer(gDECtimer, 0, 0, false)`.
2. **Otherwise**: computes nanoseconds, clamps to max 20 ms, arms one-shot
   host timer via `sys_set_timer()`.

Called from `ppc_opc_mtspr()` for SPR 22 (`ppc_alu.cc:2473`).

### Host timer (macOS)

`sys_set_timer()` (`src/system/osapi/posix/systimer.cc:217-239`) uses
`dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER)` on a serial GCD queue.
Each call cancels any existing timer and creates a new one-shot.

On non-macOS POSIX, falls back to `setitimer(ITIMER_REAL)` + `SIGALRM`.

### Timer creation

`gDECtimer` created in `ppc_cpu_run()` (`ppc_cpu.cc:122-125`) with
`decTimerCB` as callback.

---

## 3. DEC Exception Delivery

### decTimerCB

`ppc_cpu.cc:106-112`. Called from GCD queue thread. Calls
`ppc_cpu_atomic_raise_dec_exception(*gCPU)`.

### Atomic flag setting

`jitc_tools.S:654-662`. Uses LDAXR/STLXR to atomically set:

```
exception_pending |= 0x0101  (sets dec_exception + exception_pending)
```

Flag layout in PPC_CPU_State (packed into one 32-bit word):

| Byte offset | Field             | Bit mask    |
|-------------|-------------------|-------------|
| +0          | exception_pending | 0x00000001  |
| +1          | dec_exception     | 0x00000100  |
| +2          | ext_exception     | 0x00010000  |
| +3          | stop_exception    | 0x01000000  |

### Heartbeat check

`ppc_heartbeat_ext_asm` (`jitc_tools.S:246-302`) and
`ppc_heartbeat_ext_rel_asm` (`jitc_tools.S:312-363`) check at every page
dispatch and branch target:

1. Acquire-load `exception_pending` word (LDAR) to sync with timer thread.
2. If nonzero: check `stop_exception`, then `MSR[EE]` (bit 15).
3. If EE set: check `ext_exception` then `dec_exception`, branch to handler.

### ppc_dec_exception_asm

`jitc_tools.S:480-501`:

1. Store current effective PC → SRR0
2. SRR1 = MSR & 0x87c0ffff
3. Atomically clear dec_exception + exception_pending, preserve ext_exception
4. Clear MSR, invalidate TLB, dispatch to vector 0x900

---

## 4. CUDA Timer (VIA Timer 1)

The CUDA chip (`src/io/cuda/cuda.cc`) contains a VIA 6522 with Timer 1.

### VIA Timer frequency

~783,361 Hz. Encoded as `VIA_TIMER_FREQ_DIV_HZ_TIMES_1000 = 783361404`
(`cuda.cc:173`).

### Timer 1 registers

- `T1CL`/`T1CH`: current counter (offsets 4*RS, 5*RS where RS=0x200)
- `T1LL`/`T1LH`: latch for auto-reload
- IFR bit `T1_INT` (0x40): set on overflow

### cuda_start_T1

`cuda.cc:521-533`. Called on T1CH write. Computes `T1_end` in host ticks,
clears T1_INT.

### cuda_update_T1

`cuda.cc:493-519`. Called on T1CL/T1CH/IFR reads. If time < T1_end: timer
running, compute remaining. If time >= T1_end: overflow, auto-reload in
continuous mode, set T1_INT.

### CUDA → PIC

CUDA operations call `pic_raise_interrupt(IO_PIC_IRQ_CUDA)` (IRQ #18,
edge-type). VIA Timer 1's T1_INT is polled by the guest via IFR reads,
not directly routed to PIC.

---

## 5. PIC Interrupt Controller

`src/io/pic/pic.cc`. Memory-mapped at PA 0x80800000-0x80800040.

### pic_raise_interrupt (`pic.cc:142-183`)

1. Lock PIC_mutex
2. Set pending bit
3. If level-type: set PIC_pending_level
4. If enabled AND (level OR rising edge): call `ppc_cpu_raise_ext_exception()`
5. Call `ppc_cpu_wakeup()`

### pic_cancel_interrupt (`pic.cc:185-196`)

Clear pending/level bits, call `pic_renew_interrupts()` to re-evaluate.

### Register interface

| Offset | Read            | Write              |
|--------|-----------------|--------------------|
| 0x10   | pending_high    | -                  |
| 0x14   | enable_high     | set enable_high    |
| 0x18   | -               | ack pending_high   |
| 0x20   | pending_low     | -                  |
| 0x24   | enable_low      | set enable_low     |
| 0x28   | -               | ack pending_low    |
| 0x2c   | pending_level   | -                  |

---

## 6. External Interrupt Exception (0x500)

### Raising

`ppc_cpu_raise_ext_exception()` (`ppc_exc.cc:96-98`) →
`ppc_cpu_atomic_raise_ext_exception()` (`jitc_tools.S:665-674`):

```
exception_pending |= 0x00010001  (sets ext_exception + exception_pending)
```

### Cancelling

`ppc_cpu_atomic_cancel_ext_exception` (`jitc_tools.S:689-700`): clears
ext_exception + exception_pending, preserves dec_exception.

### Dispatch

`ppc_ext_exception_asm` (`jitc_tools.S:454-472`): stores SRR0/SRR1,
clears flags, dispatches to vector 0x500.

---

## 7. Kernel-side Timer Flow (Linux 2.4 PPC)

### DEC-based tick

1. Kernel writes DEC via `mtspr` → PearPC arms host timer
2. Host timer fires → `decTimerCB` → atomic `dec_exception` set
3. Heartbeat detects → dispatches to vector 0x900
4. Kernel `timer_interrupt` (c0007b78) reads `mftb`, checks elapsed
5. If tick_period elapsed: advances `last_tb_stamp`, processes tick
6. Calls `do_timer` (c0020368) which raises softirq + wakes softirq thread
7. Kernel reloads DEC → cycle repeats

### External interrupt path

1. Device calls `pic_raise_interrupt(IRQ)` → PIC → ext_exception
2. Heartbeat → vector 0x500 → kernel external interrupt handler
3. Handler reads PIC pending registers, dispatches to device driver
4. Driver acks via PIC register write

### CUDA in the kernel

Kernel VIA driver programs Timer 1 via T1CH/T1CL. On CUDA IRQ (0x500 vector,
IRQ 18), reads VIA IFR to check T1_INT for periodic ADB polling, independent
of the DEC-based system timer.
