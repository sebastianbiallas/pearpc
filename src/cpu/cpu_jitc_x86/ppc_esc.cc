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

static byte *memory_handle_phys(uint32 pa)
{
	byte *ptr = NULL;
	ppc_direct_physical_memory_handle(pa, ptr);
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
	PPC_ESC_TRACE(" return_to_dsi(%08x, %08x, %08x)\n", ea, stack, client_pc);
	stack[-1] = (uint32)&ppc_dsi_exception_special_asm;
	gCPU.pc_ofs = client_pc;
	gCPU.dar = ea;
}

static void escape_version(uint32 *stack, uint32 client_pc)
{
	gCPU.gpr[4] = PPC_ESCAPE_IF_VERSION;
}

static void escape_memset(uint32 *stack, uint32 client_pc)
{
	// memset(dest [r4], c [r5], size [r6])	
	uint32 dest = gCPU.gpr[4];
	uint32 c = gCPU.gpr[5];
	uint32 size = gCPU.gpr[6];
	PPC_ESC_TRACE("memest(%08x, %02x, %d)\n", dest, c, size);
	if (!size) return;
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
	// memcpy(dest [r4], src [r5], size [r6])	
	uint32 dest = gCPU.gpr[4];
	uint32 source = gCPU.gpr[5];
	uint32 size = gCPU.gpr[6];
	PPC_ESC_TRACE("memcpy(%08x, %08x, %d)\n", dest, source, size);
	if (!size) return;
	while (size) {
		byte *dst = memory_handle(dest, PPC_MMU_READ | PPC_MMU_WRITE);
		if (!dst) {
			return_to_dsi_exception_handler(dest, stack, client_pc);
			gCPU.gpr[4] = dest;
			gCPU.gpr[5] = source;
			gCPU.gpr[6] = size;
			return;
		}
		byte *src = memory_handle(source, PPC_MMU_READ);
		if (!src) {
			return_to_dsi_exception_handler(source, stack, client_pc);
			gCPU.gpr[4] = dest;
			gCPU.gpr[5] = source;
			gCPU.gpr[6] = size;
			return;
		}
		uint32 s = 4096 - (dest & 0xfff);
		uint32 s2 = 4096 - (source & 0xfff);
		s = MIN(s, s2);
		s = MIN(s, size);
		memcpy(dst, src, s);
		dest += s;
		source += s;
		size -= s;
	}
}

static void escape_bzero(uint32 *stack, uint32 client_pc)
{
	// bzero(dest [r4], size [r5])
	// basically this is memset with predefined CHAR of 0x0

	uint32 dest = gCPU.gpr[4];
	const uint32 c = 0;
	uint32 size = gCPU.gpr[5];
	PPC_ESC_TRACE("bzero(%08x, %08x)\n", dest, size);
	if (!size) return;
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
			gCPU.gpr[5] = size;
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
			gCPU.gpr[5] = size;
			return;
		}
		memset(dst, c, size);
	}
}

static void escape_bzero_phys(uint32 *stack, uint32 client_pc)
{
	// bzero(dest [r4], size [r5])
	// basically this is memset with predefined CHAR of 0x0

	uint32 dest = gCPU.gpr[4];
	const uint32 c = 0;
	uint32 size = gCPU.gpr[5];
	PPC_ESC_TRACE("bzero_phys(%08x, %08x)\n", dest, size);
	if (gCPU.msr & MSR_PR) return;
	byte *dst = memory_handle_phys(dest);
	memset(dst, c, size);
}

static void escape_bcopy(uint32 *stack, uint32 client_pc)
{
	// memcpy(src [r4], dest [r5], size [r6], reverse [r7])
	uint32 source = gCPU.gpr[4];
	uint32 dest = gCPU.gpr[5];
	uint32 size = gCPU.gpr[6];
	bool reverse = gCPU.gpr[7];
	PPC_ESC_TRACE("bcopy%s(%08x, %08x, %d)\n", reverse ? "_reverse" : "", source, dest, size);
	if (dest == source) return;
	if (dest < source) {
		if (dest + size <= source) goto do_memcpy;
//do_memmove:
		while (size) {
			byte *dst = memory_handle(dest, PPC_MMU_READ | PPC_MMU_WRITE);
			if (!dst) {
				return_to_dsi_exception_handler(dest, stack, client_pc);
				gCPU.gpr[4] = source;
				gCPU.gpr[5] = dest;
				gCPU.gpr[6] = size;
				return;
			}
			byte *src = memory_handle(source, PPC_MMU_READ);
			if (!src) {
				return_to_dsi_exception_handler(source, stack, client_pc);
				gCPU.gpr[4] = source;
				gCPU.gpr[5] = dest;
				gCPU.gpr[6] = size;
				return;
			}
			uint32 s = 4096 - (dest & 0xfff);
			uint32 s2 = 4096 - (source & 0xfff);
			s = MIN(s, s2);
			s = MIN(s, size);
			memmove(dst, src, s);
			dest += s;
			source += s;
			size -= s;
		}
	} else {
		// dest > source
		if (reverse) goto do_reverse_memmove;
		if (source + size <= dest) {
do_memcpy:
			while (size) {
				byte *dst = memory_handle(dest, PPC_MMU_READ | PPC_MMU_WRITE);
				if (!dst) {
					return_to_dsi_exception_handler(dest, stack, client_pc);
					gCPU.gpr[4] = source;
					gCPU.gpr[5] = dest;
					gCPU.gpr[6] = size;
					return;
				}
				byte *src = memory_handle(source, PPC_MMU_READ);
				if (!src) {
					return_to_dsi_exception_handler(source, stack, client_pc);
					gCPU.gpr[4] = source;
					gCPU.gpr[5] = dest;
					gCPU.gpr[6] = size;
					return;
				}
				uint32 s = 4096 - (dest & 0xfff);
				uint32 s2 = 4096 - (source & 0xfff);
				s = MIN(s, s2);
				s = MIN(s, size);
				memcpy(dst, src, s);
				dest += s;
				source += s;
				size -= s;
			}
		} else {
			dest += size;
			source += size;
do_reverse_memmove:
			while (size) {
				uint32 s = dest & 0xfff;
				uint32 s2 = source & 0xfff;
				if (!s) s = 0x1000;
				if (!s2) s2 = 0x1000;
				s = MIN(s, s2);
				s = MIN(s, size);
				dest -= s;
				source -= s;
				size -= s;
				byte *dst = memory_handle(dest, PPC_MMU_READ | PPC_MMU_WRITE);
				if (!dst) {
					return_to_dsi_exception_handler(dest, stack, client_pc);
					gCPU.gpr[4] = source + s;
					gCPU.gpr[5] = dest + s;
					gCPU.gpr[6] = size + s;
					gCPU.gpr[7] = 1;
					return;
				}
				byte *src = memory_handle(source, PPC_MMU_READ);
				if (!src) {
					return_to_dsi_exception_handler(source, stack, client_pc);
					gCPU.gpr[4] = source + s;
					gCPU.gpr[5] = dest + s;
					gCPU.gpr[6] = size + s;
					gCPU.gpr[7] = 1;
					return;
				}
				memmove(dst, src, s);
			}
		}
	}
}

static void escape_bcopy_phys(uint32 *stack, uint32 client_pc)
{
	// bcopy_phys(src [r4], dest [r5], size [r6])
	// bcopy_physvirt(src [r4], dest [r5], size [r6])
	uint32 source = gCPU.gpr[4];
	uint32 dest = gCPU.gpr[5];
	uint32 size = gCPU.gpr[6];
	PPC_ESC_TRACE("bcopy_phys(%08x, %08x, %d)\n", source, dest, size);
	if (gCPU.msr & MSR_PR) return;
	byte *dst = memory_handle_phys(dest);
	if (!dst) return;
	byte *src = memory_handle_phys(source);
	if (!src) return;
	memcpy(dst, src, size);
}

static void escape_copy_page(uint32 *stack, uint32 client_pc)
{
	// copy_page(src [r4], dest [r5])
	uint32 source = gCPU.gpr[4];
	uint32 dest = gCPU.gpr[5];
	byte *dst = memory_handle_phys(dest << 12);
	if (!dst) return;
	byte *src = memory_handle_phys(source << 12);
	if (!src) return;
	memcpy(dst, src, 4096);	
}

static ppc_escape_function escape_functions[] = {
	escape_version,
	
	escape_memset,
	escape_memcpy,
	escape_bzero,
	escape_bzero_phys,
	escape_bcopy,
	escape_bcopy_phys,
	escape_bcopy_phys,
	escape_copy_page,
};

void FASTCALL ppc_escape_vm(uint32 func, uint32 *stack, uint32 client_pc)
{
	if (func >= (sizeof escape_functions / sizeof escape_functions[0])) {
		PPC_ESC_WARN("unimplemented escape function %d\n", func);
	} else {
		escape_functions[func](stack, client_pc);
	}
}

