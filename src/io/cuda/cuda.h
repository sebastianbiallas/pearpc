/*
 *	PearPC
 *	cuda.h
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

#ifndef __IO_CUDA_H__
#define __IO_CUDA_H__

#include "system/types.h"

#define IO_CUDA_PA_START 0x80816000
#define IO_CUDA_PA_END 0x80817E00

void cuda_write(uint32 addr, uint32 data, int size);
void cuda_read(uint32 addr, uint32 &data, int size);
bool cuda_interrupt();

void cuda_init();
void cuda_done();
void cuda_init_config();

bool cuda_prom_get_key(uint32 &key);

#endif

