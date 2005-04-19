/*
 *	PearPC
 *	gcard.h
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

#ifndef __IO_GCARD_H__
#define __IO_GCARD_H__

#include "system/types.h"
#include "system/display.h"
#include "debug/tracers.h"

#define IO_GCARD_FRAMEBUFFER_EA 0xd0000000
#define IO_GCARD_FRAMEBUFFER_PA_START 0x84000000
#define IO_GCARD_FRAMEBUFFER_PA_END 0x85000000
//#define IO_GCARD_FRAMEBUFFER_PA_START 0x81000000
//#define IO_GCARD_FRAMEBUFFER_PA_END 0x82000000

#include "io/pci/pci.h"

class PCI_GCard: public PCI_Device {
public:
			PCI_GCard();
	virtual bool	readDeviceMem(uint r, uint32 address, uint32 &data, uint size);
	virtual bool	writeDeviceMem(uint r, uint32 address, uint32 data, uint size);
};


void FASTCALL gcard_write_1(uint32 addr, uint32 data);
void FASTCALL gcard_write_2(uint32 addr, uint32 data);
void FASTCALL gcard_write_4(uint32 addr, uint32 data);
void FASTCALL gcard_write_8(uint32 addr, uint64 data);
void FASTCALL gcard_write_16(uint32 addr, uint128 *data);
void FASTCALL gcard_write_16_native(uint32 addr, uint128 *data);
void FASTCALL gcard_read_1(uint32 addr, uint32 &data);
void FASTCALL gcard_read_2(uint32 addr, uint32 &data);
void FASTCALL gcard_read_4(uint32 addr, uint32 &data);
void FASTCALL gcard_read_8(uint32 addr, uint64 &data);
void FASTCALL gcard_read_16(uint32 addr, uint128 *data);
void FASTCALL gcard_read_16_native(uint32 addr, uint128 *data);

static inline void gcard_write(uint32 addr, uint32 data, int size) 
{
	switch (size) {
	case 1:
	    gcard_write_1(addr, data);
	    break;
	case 2:
	    gcard_write_2(addr, data);
	    break;
	case 4:
	    gcard_write_4(addr, data);
	    break;
	default:
	    IO_GRAPHIC_ERR("unknown size %d", size);
	}
}

static inline void gcard_read(uint32 addr, uint32 &data, int size) 
{
	switch (size) {
	case 1:
	    gcard_read_1(addr, data);
	    break;
	case 2:
	    gcard_read_2(addr, data);
	    break;
	case 4:
	    gcard_read_4(addr, data);
	    break;
	default:
	    IO_GRAPHIC_ERR("unknown size");
	}
}

static inline void gcard_write64(uint32 addr, uint64 data) 
{
	    gcard_write_8(addr, data);
}

static inline void gcard_read64(uint32 addr, uint64 &data) 
{
	    gcard_read_8(addr, data);
}

static inline void gcard_write128(uint32 addr, uint128 *data) 
{
	    gcard_write_16(addr, data);
}

static inline void gcard_write128_native(uint32 addr, uint128 *data) 
{
	    gcard_write_16_native(addr, data);
}

static inline void gcard_read128(uint32 addr, uint128 *data) 
{
	    gcard_read_16(addr, data);
}

static inline void gcard_read128_native(uint32 addr, uint128 *data) 
{
	    gcard_read_16_native(addr, data);
}

void gcard_raise_interrupt();

void gcard_osi(int cpu);
bool gcard_set_mode(DisplayCharacteristics &mode);


void gcard_init();
void gcard_done();
void gcard_init_modes();
void gcard_init_host_modes();
void gcard_init_config();

bool displayCharacteristicsFromString(DisplayCharacteristics &aChar, const String &s);
void gcard_add_characteristic(const DisplayCharacteristics &aChar);
bool gcard_supports_characteristic(const DisplayCharacteristics &aChar);
bool gcard_finish_characteristic(DisplayCharacteristics &aChar);

#endif

