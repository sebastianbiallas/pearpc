/*
 *	PearPC
 *	ppc_cpu.cc
 *
 *	Copyright (C) 2003 Sebastian Biallas (sb@biallas.net)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <cerrno>
#include <cstring>

#include "debug/tracers.h"
#include "system/sys.h"
#include "system/systhread.h"
#include "system/systimer.h"
#include "ppc_cpu.h"
#include "ppc_mmu.h"
#include "jitc.h"
#include "jitc_asm.h"
#include "jitc_debug.h"

PPC_CPU_State gCPU;
bool gSinglestep = false;
uint32 gBreakpoint;
uint32 gBreakpoint2;

extern "C" void ppc_display_jitc_stats()
{
	ht_printf("pg.dest:   write: %qd    out of pages: %qd   out of tc: %qd\r", &gJITC.destroy_write, &gJITC.destroy_oopages, &gJITC.destroy_ootc);
}

void ppc_fpu_test();

uint64 gJITCCompileTicks;
uint64 gJITCRunTicks;
uint64 gJITCRunTicksStart;

uint64 gClientClockFrequency;
uint64 gClientTimeBaseFrequency;
uint64 gStartHostCPUTicks;

uint64 ppc_get_cpu_ideal_timebase()
{
	uint64 ticks = sys_get_cpu_ticks();
//	if (ticks < gElapsedHostCPUTicks) {
//		FIXME: overflow		
//	}
	uint64 ticks_per_sec = sys_get_cpu_ticks_per_second();
	return (ticks - gStartHostCPUTicks) * gClientTimeBaseFrequency / ticks_per_sec;
}

uint64 ppc_get_cpu_timebase()
{
	// FIXME: once "mttb" is implemented, keep track of modified TB register
	//        So for now, itb = tb.
	return ppc_get_cpu_ideal_timebase();
}

sys_timer gDECtimer;

static void decTimerCB(sys_timer t)
{
	ppc_cpu_atomic_raise_dec_exception();
}

void ppc_run()
{
//	ppc_fpu_test();
//	return;
	gJITCRunTicks = 0;
	gJITCCompileTicks = 0;
	gJITCRunTicksStart = jitcDebugGetTicks();
	PPC_CPU_TRACE("execution started at %08x\n", gCPU.pc);
	jitcDebugInit();
	gStartHostCPUTicks = sys_get_cpu_ticks();
	gClientClockFrequency = PPC_CLOCK_FREQUENCY;
	gClientTimeBaseFrequency = PPC_TIMEBASE_FREQUENCY;

/*	uint64 q = sys_get_cpu_ticks_per_second();
	PPC_CPU_WARN("clock ticks / second = %08qx\n", &q);
	q = sys_get_cpu_ticks();
	PPC_CPU_WARN("ticks = %08qx\n", &q);
	q = sys_get_cpu_ticks();
	PPC_CPU_WARN("ticks = %08qx\n", &q);
	q = sys_get_cpu_ticks();
	PPC_CPU_WARN("ticks = %08qx\n", &q);*/

	if (!sys_create_timer(&gDECtimer, decTimerCB)) {
		ht_printf("Unable to create timer\n");
		exit(1);
	}

	ppc_start_jitc_asm(gCPU.pc);
}

void ppc_stop()
{
	gCPU.exception_pending = true;
	gCPU.stop_exception = true;
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

#define CPU_KEY_PVR	"cpu_pvr"

#include "configparser.h"

bool cpu_init()
{
	memset(&gCPU, 0, sizeof gCPU);
	gCPU.pvr = gConfig->getConfigInt(CPU_KEY_PVR);
	
	// initialize srs (mostly for prom)
	for (int i=0; i<16; i++) {
		gCPU.sr[i] = 0x2aa*i;
	}
	
	gCPU.x87cw = 0x37f;

	return jitc_init(2048, 16*1024*1024);
}

void cpu_init_config()
{
	gConfig->acceptConfigEntryIntDef("cpu_pvr", 0x88302);
}
