/*
 *	PearPC
 *	io.cc
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

#include "io.h"
#include "io/3c90x/3c90x.h"
#include "io/graphic/gcard.h"
#include "io/pic/pic.h"
#include "io/pci/pci.h"
#include "io/cuda/cuda.h"
#include "io/nvram/nvram.h"


/*
 *	The xx_glue functions are needed for the jitc
 */
extern "C" uint32 FASTCALL io_mem_read_glue(uint32 addr, int size)
{
//	gCPU.pc = gCPU.current_code_base + gCPU.pc_ofs;
	uint32 ret;
	io_mem_read(addr, ret, size);
	return ret;
}

extern "C" void FASTCALL io_mem_write_glue(uint32 addr, uint32 data, int size)
{
//	gCPU.pc = gCPU.current_code_base + gCPU.pc_ofs;
	io_mem_write(addr, data, size);
}

extern "C" uint64 FASTCALL io_mem_read64_glue(uint32 addr)
{
//	gCPU.pc = gCPU.current_code_base + gCPU.pc_ofs;
	uint64 ret;
	io_mem_read64(addr, ret);
	return ret;
}

extern "C" void FASTCALL io_mem_write64_glue(uint32 addr, uint64 data)
{
//	gCPU.pc = gCPU.current_code_base + gCPU.pc_ofs;
	io_mem_write64(addr, data);
}

extern "C" void FASTCALL io_mem_read128_glue(uint32 addr, uint128 *data)
{
//	gCPU.pc = gCPU.current_code_base + gCPU.pc_ofs;
	io_mem_read128(addr, data);
}

extern "C" void FASTCALL io_mem_read128_native_glue(uint32 addr, uint128 *data)
{
//	gCPU.pc = gCPU.current_code_base + gCPU.pc_ofs;
	io_mem_read128_native(addr, data);
}

extern "C" void FASTCALL io_mem_write128_glue(uint32 addr, uint128 *data)
{
//	gCPU.pc = gCPU.current_code_base + gCPU.pc_ofs;
	io_mem_write128(addr, data);
}

extern "C" void FASTCALL io_mem_write128_native_glue(uint32 addr, uint128 *data)
{
//	gCPU.pc = gCPU.current_code_base + gCPU.pc_ofs;
	io_mem_write128_native(addr, data);
}
 
void io_init()
{
	pci_init();
	cuda_init();
	pic_init();
	nvram_init();
}

void io_done()
{
	pci_done();
	cuda_done();
	pic_done();
	nvram_done();
}

void io_init_config()
{
	pci_init_config();
	cuda_init_config();
	pic_init_config();
	nvram_init_config();
}
