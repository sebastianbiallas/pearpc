/*
 *	PearPC
 *	pic.cc
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

#include <cstdlib>
#include <cstring>

#include "tools/snprintf.h"
#include "cpu_generic/ppc_cpu.h"
#include "cpu_generic/ppc_exc.h"
#include "cpu_generic/ppc_tools.h"
#include "io/cuda/cuda.h"
#include "pic.h"
#include "debug/tracers.h"
#include "system/systhread.h"

uint32 PIC_enable_low;
uint32 PIC_enable_high;
uint32 PIC_pending_low;
uint32 PIC_pending_high;
uint32 PIC_pending_level;

sys_mutex PIC_mutex;

void pic_renew_interrupts()
{
	if (((PIC_pending_low | PIC_pending_level) & PIC_enable_low) || (PIC_pending_high & PIC_enable_high)) {
		ppc_raise_ext_exception();	
	} else {
		ppc_cancel_ext_exception();
	}
}

void pic_write(uint32 addr, uint32 data, int size)
{
	IO_PIC_TRACE("write word @%08x: %08x (from %08x)\n", addr, data, gCPU.pc);
	addr -= IO_PIC_PA_START;
	switch (addr) {
	case 0x24:
	case 0x14: {
		// enable /disable
		data = ppc_word_to_BE(data);
		int o=0;
		if (addr == 0x14) {
			o = 32;
			PIC_enable_high = data;
		} else {
			PIC_enable_low = data;
			IO_PIC_TRACE("enable / disable\n");
		}
		int x = 1;
		for (int i=0; i<31; i++) {
			if (data & x) {
				IO_PIC_TRACE("enable %d\n", o+i);
//				gIRQ_Enable[o+i] = true;
			} else {
//				gIRQ_Enable[o+i] = false;
			}
			x<<=1;
		}
		break;
	}
	case 0x18:
		// ack irq
		IO_PIC_TRACE("ack high\n");
		data = ppc_word_to_BE(data);
		PIC_pending_high &= ~data;
		break;
	case 0x28:
		// ack irq
		IO_PIC_TRACE("ack low\n");
		data = ppc_word_to_BE(data);
		PIC_pending_low &= ~data;
		break;
	case 0x38: 
		IO_PIC_TRACE("sound\n");
		data = 0;
		break;
	default:
		IO_PIC_ERR("unknown service %08x (write(%d) %08x from %08x)\n", addr, size, data, gCPU.pc);
	}
	pic_renew_interrupts();
}

void pic_read(uint32 addr, uint32 &data, int size)
{
	IO_PIC_TRACE("read word @%08x (from %08x)\n", addr, gCPU.pc);
	addr -= IO_PIC_PA_START;
	switch (addr) {
	case 0x24:
	case 0x14: {
		// enable /disable
		uint32 r;
		if (addr == 0x14) {
			r = PIC_enable_high;
		} else {
			r = PIC_enable_low;
		}
		IO_PIC_TRACE("enable / disable %08x\n", r);
		data = ppc_word_to_BE(r);
		break;
	}
	case 0x10:
		IO_PIC_TRACE("interrupt high? (pending_high is %08x)\n", PIC_pending_high);
		data = ppc_word_to_BE(PIC_pending_high);
		break;
	case 0x1c:
		IO_PIC_TRACE("level2\n");
		data = ppc_word_to_BE(0);
		break;
	case 0x20:
		IO_PIC_TRACE("interrupt low? (pending_low is %08x)\n", PIC_pending_low);
		data = ppc_word_to_BE(PIC_pending_low);
		break;
	case 0x2c:
		// level
		IO_PIC_TRACE("level1 (%08x)\n", PIC_pending_level);
		data = ppc_word_to_BE(PIC_pending_level);
		break;
	case 0x38:
		IO_PIC_TRACE("sound\n");
		data = 0;
		break;
	default:
		IO_PIC_ERR("unknown service %08x (read(%d) from %08x)\n", addr, size, gCPU.pc);
	}
}

extern uint64 gJITCCompileTicks;
extern uint64 gJITCRunTicks;
extern uint64 gJITCRunTicksStart;
//#include "cpu_jitc_x86/jitc.h"

extern "C" bool pic_check_interrupt()
{
//	ht_printf("stata: %016qx %016qx %08x   %08x\n", &gJITC.stata, &gCPU.tb, gCPU.dec, gCPU.lr);
//	ht_printf("tb: %016qx %08x   %08x\n", &gCPU.tb, gCPU.dec, gCPU.lr);
//	ht_printf("jitcTicks: compile: %08qx   run: %08qx\n", &gJITCCompileTicks, &gJITCRunTicks);
//	PIC_pending_low |= (1<<IO_PIC_IRQ_NMI_XMON);	
	cuda_interrupt();
	if (((PIC_pending_low | PIC_pending_level) & PIC_enable_low) || (PIC_pending_high & PIC_enable_high)) {
		return true;
	}
	return false;
}

void pic_raise_interrupt(int intr)
{
	sys_lock_mutex(PIC_mutex);
	uint32 mask, pending;
	int intr_;
	if (intr>31) {
		mask = PIC_enable_high;
		pending = PIC_pending_high;
		intr_ = intr-32;
	} else {
		mask = PIC_enable_low;
		pending = PIC_pending_low;
		intr_ = intr;
	}
	uint32 ibit = 1<<intr_;
	bool level = false;
	if (intr>31) {
		PIC_pending_high |= ibit;
	} else {
		PIC_pending_low |= ibit;
		if (IO_PIC_LEVEL_TYPE & ibit) {
			PIC_pending_level |= ibit;
			level = true;
		}
	}
	/*
	 *	edge type:
	 *	signal int if not masked and state raises from low to high
	 *
	 *	level type:
	 *	signal int if not masked and state high
	 */
	if ((mask & ibit) && 
	    (level || !(pending & ibit))) {
		IO_PIC_TRACE("*signal int: %d\n", intr);
		ppc_raise_ext_exception();
	} else {
		IO_PIC_TRACE("/signal int: %d\n", intr);
	}
	sys_unlock_mutex(PIC_mutex);
}

void pic_cancel_interrupt(int intr)
{
	sys_lock_mutex(PIC_mutex);
	if (intr>31) {
	        PIC_pending_high &= ~(1<<(intr-32));
	} else {
		PIC_pending_low &= ~(1<<intr);
		PIC_pending_level &= ~(1<<intr);
	}
	if (((PIC_pending_low | PIC_pending_level) & PIC_enable_low) || (PIC_pending_high & PIC_enable_high)) {
		ppc_raise_ext_exception();	
	} else {
		ppc_cancel_ext_exception();
	}
	sys_unlock_mutex(PIC_mutex);
}

void pic_init()
{
	PIC_pending_low = 0;
	PIC_pending_high = 0;
	PIC_enable_low = 0;
	PIC_enable_high = 0;
	sys_create_mutex(&PIC_mutex);
}

void pic_init_config()
{
}

