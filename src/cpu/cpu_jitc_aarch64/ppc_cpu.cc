/*
 *  PearPC
 *  ppc_cpu.cc
 *
 *  Copyright (C) 2026 Sebastian Biallas (sb@biallas.net)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

#include "debug/tracers.h"
#include "system/sys.h"
#include "system/sysbt.h"
#include "system/sysclk.h"
#include "system/systhread.h"
#include "system/systimer.h"
#include "cpu/cpu.h"
#include "ppc_cpu.h"
#include "ppc_dec.h"
#include "ppc_mmu.h"
#include "jitc.h"
#include "jitc_asm.h"
#include "jitc_debug.h"

//static
PPC_CPU_State *gCPU;
bool gSinglestep = false;
uint32 gBreakpoint;
uint32 gBreakpoint2;

extern "C" void ppc_display_jitc_stats(PPC_CPU_State &aCPU)
{
    JITC &jitc = *aCPU.jitc;
    ht_printf("pg.dest:   write: %qd    out of pages: %qd   out of tc: %qd\r", &jitc.destroy_write,
              &jitc.destroy_oopages, &jitc.destroy_ootc);
}

void ppc_fpu_test();

uint64 gJITCCompileTicks;
uint64 gJITCRunTicks;
uint64 gJITCRunTicksStart;

uint64 gClientClockFrequency;
uint64 gClientBusFrequency;
uint64 gClientTimeBaseFrequency;
uint64 gStartHostCLKTicks;
uint64 gTBreadITB;
int gHostClockScale;

uint64 ppc_get_cpu_ideal_timebase()
{
    uint64 ticks = sys_get_hiresclk_ticks();
    if (gHostClockScale < 0) {
        return (ticks - gStartHostCLKTicks) >> (-gHostClockScale);
    } else {
        return (ticks - gStartHostCLKTicks) << gHostClockScale;
    }
}

uint64 ppc_get_cpu_timebase()
{
    uint64 ticks = sys_get_hiresclk_ticks();
    if (gHostClockScale < 0) {
        gCPU->tb += (ticks - gTBreadITB) >> (-gHostClockScale);
    } else {
        gCPU->tb += (ticks - gTBreadITB) << gHostClockScale;
    }

    gTBreadITB = ticks;
    return gCPU->tb;
}

sys_timer gDECtimer;
sys_semaphore gCPUDozeSem;

extern "C" void cpu_doze()
{
    if (!gCPU->exception_pending) {
        sys_lock_semaphore(gCPUDozeSem);
        sys_wait_semaphore_bounded(gCPUDozeSem, 10);
        sys_unlock_semaphore(gCPUDozeSem);
    }
}

void ppc_cpu_wakeup()
{
    sys_signal_semaphore(gCPUDozeSem);
}

static void decTimerCB(sys_timer t)
{
    static int dc = 0; dc++;
    if (dc <= 10 || dc % 100 == 0)
        fprintf(stderr, "[DEC] #%d\n", dc);
    ppc_cpu_atomic_raise_dec_exception(*gCPU);
}

void ppc_cpu_run()
{
    gJITCRunTicks = 0;
    gJITCCompileTicks = 0;
    gJITCRunTicksStart = jitcDebugGetTicks();
    PPC_CPU_TRACE("execution started at %08x\n", gCPU->pc);
    jitcDebugInit();

    if (!sys_create_timer(&gDECtimer, decTimerCB)) {
        PPC_CPU_ERR("unable to create DEC timer\n");
    }
    // Ensure SIGALRM is not blocked on this thread
    {
        sigset_t set;
        pthread_sigmask(SIG_BLOCK, NULL, &set);
        fprintf(stderr, "[INIT] SIGALRM blocked: %d\n", sigismember(&set, SIGALRM));
        if (sigismember(&set, SIGALRM)) {
            sigdelset(&set, SIGALRM);
            pthread_sigmask(SIG_SETMASK, &set, NULL);
            fprintf(stderr, "[INIT] SIGALRM unblocked\n");
        }
    }
    if ((sizeof *gCPU) % 16) {
        PPC_CPU_ERR("sizeof gCPU (%d) is not multiple of 16 (aarch64 alignment)\n",
                  (int)(sizeof *gCPU));
    }
    ht_printf("*** &gCPU: %p, &gJITC: %p\n", gCPU, gCPU->jitc);
    ht_printf("sizeof cpu: %d\n", int(sizeof(*gCPU)));
    PPC_CPU_TRACE("entering JIT at PC=0x%08x\n", gCPU->pc);

    ppc_start_jitc_asm(gCPU->pc, &gCPU, sizeof *gCPU);
    ht_printf("JIT returned\n");

    // Dump memory on exit for debugging
    extern byte *gMemory;
    extern uint32 gMemorySize;
    FILE *df = fopen("memdump_jit.bin", "wb");
    if (df) {
        fwrite(gMemory, 1, gMemorySize, df);
        fclose(df);
        ht_printf("[DUMP] wrote memdump_jit.bin (%u bytes) on JIT exit\n", gMemorySize);
    }
}

void ppc_cpu_map_framebuffer(uint32 pa, uint32 ea)
{
    // use BAT for framebuffer
    gCPU->dbatu[0] = ea | (7 << 2) | 0x3;
    gCPU->dbatl[0] = pa;

    gCPU->dbat_bl[0] = (~gCPU->dbatu[0] << 15) & 0xfffe0000;
    gCPU->dbat_nbl[0] = ~gCPU->dbat_bl[0];

    gCPU->dbat_bepi[0] = gCPU->dbatu[0] & gCPU->dbat_bl[0];
    gCPU->dbat_brpn[0] = gCPU->dbatl[0] & gCPU->dbat_bl[0];
}


extern "C" void crash_dump_cpu_state();
extern void jitc_dump_and_exit(int code);

void ppc_fatal(const char *fmt, ...)
{
    static bool already_crashing = false;
    if (already_crashing) _exit(1);
    already_crashing = true;

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "\n*** FATAL: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);

    ppc_cpu_crash_dump(1);
}

void ppc_cpu_crash_dump(int code)
{
    crash_dump_cpu_state();
    sys_print_backtrace(64);
    jitc_dump_and_exit(code);
}

void ppc_cpu_stop()
{
    ppc_cpu_atomic_raise_stop_exception(*gCPU);
}

uint64 ppc_get_clock_frequency(int cpu)
{
    return gClientClockFrequency;
}

uint64 ppc_get_bus_frequency(int cpu)
{
    return gClientBusFrequency;
}

uint64 ppc_get_timebase_frequency(int cpu)
{
    return gClientTimeBaseFrequency;
}


void ppc_machine_check_exception()
{
    PPC_CPU_ERR("machine check exception\n");
}

uint32 ppc_cpu_get_gpr(int cpu, int i)
{
    return gCPU->gpr[i];
}

void ppc_cpu_set_gpr(int cpu, int i, uint32 newvalue)
{
    gCPU->gpr[i] = newvalue;
}

void ppc_cpu_set_msr(int cpu, uint32 newvalue)
{
    gCPU->msr = newvalue;
}

void ppc_cpu_set_pc(int cpu, uint32 newvalue)
{
    gCPU->pc = newvalue;
}

uint32 ppc_cpu_get_pc(int cpu)
{
    return gCPU->pc_ofs + gCPU->current_code_base;
}

uint32 ppc_cpu_get_pvr(int cpu)
{
    return gCPU->pvr;
}


void ppc_set_singlestep_v(bool v, const char *file, int line, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    ht_fprintf(stdout, "singlestep %s from %s:%d, info: ", v ? "set" : "cleared", file, line);
    ht_vfprintf(stdout, format, arg);
    ht_fprintf(stdout, "\n");
    va_end(arg);
    gSinglestep = v;
}

void ppc_set_singlestep_nonverbose(bool v)
{
    gSinglestep = v;
}

#define CPU_KEY_PVR "cpu_pvr"

#include "configparser.h"

JITC *gJITC;
bool ppc_cpu_init()
{
    gCPU = ppc_malloc(sizeof *gCPU);
    memset(gCPU, 0, sizeof *gCPU);
    gCPU->pvr = gConfig->getConfigInt(CPU_KEY_PVR);

    ppc_dec_init();
    // initialize srs (mostly for prom)
    for (int i = 0; i < 16; i++) {
        gCPU->sr[i] = 0x2aa * i;
    }

    gCPU->x87cw = 0x37f;

    sys_create_semaphore(&gCPUDozeSem);

    gStartHostCLKTicks = sys_get_hiresclk_ticks();
    uint64 q = sys_get_hiresclk_ticks_per_second();
    gHostClockScale = 0;
    while (q < PPC_TIMEBASE_FREQUENCY) {
        gHostClockScale++;
        q <<= 1;
    }
    while (q > (PPC_TIMEBASE_FREQUENCY * 2)) {
        gHostClockScale--;
        q >>= 1;
    }
    gTBreadITB = sys_get_hiresclk_ticks();
    gClientTimeBaseFrequency = q;
    fprintf(stderr, "[INIT] gClientTimeBaseFrequency=%llu gHostClockScale=%d\n", gClientTimeBaseFrequency, gHostClockScale);
    gClientBusFrequency = gClientTimeBaseFrequency * 4;
    gClientClockFrequency = gClientBusFrequency * 5;
    gCPU->jitc = new JITC;
    gJITC = gCPU->jitc;
    return gCPU->jitc->init(4096, 64 * 1024 * 1024);
}

void ppc_cpu_init_config()
{
    gConfig->acceptConfigEntryIntDef("cpu_pvr", 0x000c0201);
}
