/*
 *	PearPC
 *	ppc_exc.cc - AArch64 JIT exception handling (stub)
 *
 *	Copyright (C) 2026 Sebastian Biallas (sb@biallas.net)
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

#include "tools/snprintf.h"
#include "debug/tracers.h"
#include "info.h"
#include "ppc_cpu.h"
#include "ppc_exc.h"
#include "ppc_mmu.h"

/*
 *	The atomic exception raise/cancel functions
 *	(ppc_cpu_atomic_raise_dec_exception, ppc_cpu_atomic_raise_ext_exception,
 *	 ppc_cpu_atomic_raise_stop_exception, ppc_cpu_atomic_cancel_ext_exception)
 *	are defined in jitc_tools.S using AArch64 load-exclusive/store-exclusive
 *	instructions for proper atomicity.
 */

extern PPC_CPU_State *gCPU;

void ppc_cpu_raise_ext_exception()
{
    ppc_cpu_atomic_raise_ext_exception(*gCPU);
}

void ppc_cpu_cancel_ext_exception()
{
    ppc_cpu_atomic_cancel_ext_exception(*gCPU);
}
