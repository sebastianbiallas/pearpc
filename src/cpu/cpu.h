/*
 *	PearPC
 *	cpu.h
 *
 *	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
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

#ifndef __CPU_H__
#define __CPU_H__

#include "system/types.h"

uint64	ppc_get_clock_frequency();
uint64	ppc_get_bus_frequency();
uint64	ppc_get_timebase_frequency();

bool	cpu_init();
void	cpu_init_config();

// May only be called from within a CPU thread.
void	ppc_run();
uint32	ppc_cpu_get_gpr(int i);
void	ppc_cpu_set_gpr(int i, uint32 newvalue);

#endif
