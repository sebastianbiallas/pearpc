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

#include <cstring>

#include "debug/tracers.h"
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

uint64 gJITCCompileTicks;
uint64 gJITCRunTicks;
uint64 gJITCRunTicksStart;

void ppc_fpu_test();

void ppc_run()
{
//	ppc_fpu_test();
//	return;
	gJITCRunTicks = 0;
	gJITCCompileTicks = 0;
//	gJITCRunTicksStart = jitcDebugGetTicks();
	PPC_CPU_TRACE("execution started at %08x\n", gCPU.pc);
	jitcDebugInit();
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
	
	return jitc_init(2048, 16*1024*1024);
}

void cpu_init_config()
{
	gConfig->acceptConfigEntryIntDef("cpu_pvr", 0x88302);
}
