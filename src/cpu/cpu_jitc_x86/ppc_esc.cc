/*
 *	PearPC
 *	ppc_esc.cc
 *
 *	Copyright (C) 2005 Sebastian Biallas (sb@biallas.net)
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

#include "system/types.h"
#include "debug/tracers.h"
#include "ppc_cpu.h"
#include "ppc_esc.h"
#include "ppc_mmu.h"
#include "jitc_asm.h"

typedef void (*ppc_escape_function)(uint32 *stack, uint32 client_pc);

static byte *memory_handle(uint32 ea, int flags)
{
	uint32 pa;
	byte *ptr = NULL;
	if ((ppc_effective_to_physical_vm(ea, flags, pa) & flags) == flags) {
		ppc_direct_physical_memory_handle(pa, ptr);
	}
	return ptr;
}

static void return_to_dsi_exception_handler(uint32 ea, uint32 *stack, uint32 client_pc)
{
	/*
	 *	stack contains the value of ESP before calling our
	 *	escape function. So we can modify 
	 *
	 *	            (stack - 4)
	 *
	 *      if we want to return to a different function.
	 */
	stack[-4] = (uint32)&ppc_dsi_exception_special_asm;
	gCPU.pc_ofs = client_pc;
	gCPU.dar = ea;
}

static void escape_memset(uint32 *stack, uint32 client_pc)
{
	// memset(dest [r4], c [r5], size [r6])	

	uint32 dest = gCPU.gpr[4];
	uint32 c = gCPU.gpr[5];
	uint32 size = gCPU.gpr[6];
	if (dest & 0xfff) {
		byte *dst = memory_handle(dest, PPC_MMU_READ | PPC_MMU_WRITE);
		if (!dst) {
			return_to_dsi_exception_handler(dest, stack, client_pc);
			return;
		}
		uint32 a = 4096 - (dest & 0xfff);
		a = MIN(a, size);
		memset(dst, c, a);
		size -= a;
		dest += a;
	}
	while (size >= 4096) {
		byte *dst = memory_handle(dest, PPC_MMU_READ | PPC_MMU_WRITE);
		if (!dst) {
			return_to_dsi_exception_handler(dest, stack, client_pc);
			gCPU.gpr[4] = dest;
			gCPU.gpr[6] = size;
			return;
		}
		memset(dst, c, 4096);
		dest += 4096;
		size -= 4096;
	}
	if (size) {
		byte *dst = memory_handle(dest, PPC_MMU_READ | PPC_MMU_WRITE);
		if (!dst) {
			return_to_dsi_exception_handler(dest, stack, client_pc);
			gCPU.gpr[4] = dest;
			gCPU.gpr[6] = size;
			return;
		}
		memset(dst, c, size);
	}
}

static void escape_memcpy(uint32 *stack, uint32 client_pc)
{
}

static ppc_escape_function escape_functions[] = {
	escape_memset,
	escape_memcpy,
};

void FASTCALL ppc_escape_vm(uint32 func, uint32 *stack, uint32 client_pc)
{
	if (func >= (sizeof escape_functions / sizeof escape_functions[0])) {
		PPC_ESC_WARN("unimplemented escape function %d\n", func);
	} else {
		escape_functions[func](stack, client_pc);
	}
}

