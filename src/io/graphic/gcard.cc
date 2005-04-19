/*
 *	PearPC
 *	gcard.cc
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

#include <cstdlib>
#include <cstring>

#include "debug/tracers.h"
#include "system/display.h"
#include "system/arch/sysendian.h"
#include "tools/snprintf.h"
#include "cpu/cpu.h"
#include "io/pic/pic.h"
#include "gcard.h"

struct VMode {
	int width, height, bytesPerPixel;
};

static VMode stdVModes[] = {
	{width: 640, height: 480, bytesPerPixel: 2},
	{width: 640, height: 480, bytesPerPixel: 4},
	{width: 800, height: 600, bytesPerPixel: 2},
	{width: 800, height: 600, bytesPerPixel: 4},
	{width: 1024, height: 768, bytesPerPixel: 2},
	{width: 1024, height: 768, bytesPerPixel: 4},
	{width: 1152, height: 864, bytesPerPixel: 2},
	{width: 1152, height: 864, bytesPerPixel: 4},
	{width: 1280, height: 720, bytesPerPixel: 2},
	{width: 1280, height: 720, bytesPerPixel: 4},
	{width: 1280, height: 768, bytesPerPixel: 2},
	{width: 1280, height: 768, bytesPerPixel: 4},
	{width: 1280, height: 960, bytesPerPixel: 2},
	{width: 1280, height: 960, bytesPerPixel: 4},
	{width: 1280, height: 1024, bytesPerPixel: 2},
	{width: 1280, height: 1024, bytesPerPixel: 4},
	{width: 1360, height: 768, bytesPerPixel: 2},
	{width: 1360, height: 768, bytesPerPixel: 4},
	{width: 1600, height: 900, bytesPerPixel: 2},
	{width: 1600, height: 900, bytesPerPixel: 4},
	{width: 1600, height: 1024, bytesPerPixel: 2},
	{width: 1600, height: 1024, bytesPerPixel: 4},
	{width: 1600, height: 1200, bytesPerPixel: 2},
	{width: 1600, height: 1200, bytesPerPixel: 4},
};

static Container *gGraphicModes;

PCI_GCard::PCI_GCard()
	:PCI_Device("pci-graphic", 0x00, 0x07)
{
	mIORegSize[0] = 0x800000;
	mIORegType[0] = PCI_ADDRESS_SPACE_MEM_PREFETCH;

//	mConfig[0x00] = 0x02;	// vendor ID
//	mConfig[0x01] = 0x10;
//	mConfig[0x02] = 0x45;	// unit ID
//	mConfig[0x03] = 0x52;
	mConfig[0x00] = 0x66;	// vendor ID
	mConfig[0x01] = 0x66;
	mConfig[0x02] = 0x66;	// unit ID
	mConfig[0x03] = 0x66;

	mConfig[0x08] = 0x00;	// revision
	mConfig[0x09] = 0x00;
	mConfig[0x0a] = 0x00;
	mConfig[0x0b] = 0x03;

	mConfig[0x0e] = 0x00;	// header-type
	
	assignMemAddress(0, IO_GCARD_FRAMEBUFFER_PA_START);
	
	mConfig[0x3c] = IO_PIC_IRQ_GCARD;
	mConfig[0x3d] = 1;
	mConfig[0x3e] = 0;
	mConfig[0x3f] = 0;
}

bool PCI_GCard::readDeviceMem(uint r, uint32 address, uint32 &data, uint size)
{
	IO_GRAPHIC_TRACE("read %d, %08x, %d\n", r, address, size);
	data = 0;
	return true;
}

bool PCI_GCard::writeDeviceMem(uint r, uint32 address, uint32 data, uint size)
{
	IO_GRAPHIC_TRACE("write %d, %08x, %08x, %d\n", r, address, data, size);
	return true;
}

#define MAYBE_PPC_HALF_TO_BE(a) ppc_half_from_LE(a)
#define MAYBE_PPC_WORD_TO_BE(a) ppc_word_from_LE(a)
#define MAYBE_PPC_DWORD_TO_BE(a) ppc_dword_from_LE(a)

/*#define MAYBE_PPC_HALF_TO_BE(a) (a)
#define MAYBE_PPC_WORD_TO_BE(a) (a)
#define MAYBE_PPC_DWORD_TO_BE(a) (a)*/

void FASTCALL gcard_write_1(uint32 addr, uint32 data)
{
	addr -= IO_GCARD_FRAMEBUFFER_PA_START;
	*(uint8*)(gFrameBuffer+addr) = data;
	damageFrameBuffer(addr);
}

void FASTCALL gcard_write_2(uint32 addr, uint32 data)
{
	addr -= IO_GCARD_FRAMEBUFFER_PA_START;
	*(uint16*)(gFrameBuffer+addr) = MAYBE_PPC_HALF_TO_BE(data);
	damageFrameBuffer(addr);
}

void FASTCALL gcard_write_4(uint32 addr, uint32 data)
{
	addr -= IO_GCARD_FRAMEBUFFER_PA_START;
	*(uint32*)(gFrameBuffer+addr) = MAYBE_PPC_WORD_TO_BE(data);
	damageFrameBuffer(addr);
}

void FASTCALL gcard_write_8(uint32 addr, uint64 data)
{
	addr -= IO_GCARD_FRAMEBUFFER_PA_START;
	*(uint64*)(gFrameBuffer+addr) = MAYBE_PPC_DWORD_TO_BE(data);
	damageFrameBuffer(addr);
}

void FASTCALL gcard_write_16(uint32 addr, uint128 *data)
{
	addr-= IO_GCARD_FRAMEBUFFER_PA_START;
#if HOST_ENDIANESS == HOST_ENDIANESS_LE
	uint8 *src = (uint8 *)data;

	for (int i=0; i<16; i++) {
		gFrameBuffer[addr+15-i] = src[i];
	}
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
	memmove(gFrameBuffer+addr, data, 16);
#else
#error Unsupported endianess
#endif
	//*(uint64*)(gFrameBuffer+addr) = MAYBE_PPC_DWORD_TO_BE(data->h);
	//*(uint64*)(gFrameBuffer+addr+8) = MAYBE_PPC_DWORD_TO_BE(data->l);
	damageFrameBuffer(addr);
}

void FASTCALL gcard_write_16_native(uint32 addr, uint128 *data)
{
	addr-= IO_GCARD_FRAMEBUFFER_PA_START;

	memmove(gFrameBuffer+addr, data, 16);

	damageFrameBuffer(addr);
}

void FASTCALL gcard_read_1(uint32 addr, uint32 &data)
{
	addr-= IO_GCARD_FRAMEBUFFER_PA_START;
	data = (*(uint8*)(gFrameBuffer+addr));
}

void FASTCALL gcard_read_2(uint32 addr, uint32 &data)
{
	addr-= IO_GCARD_FRAMEBUFFER_PA_START;
	data = MAYBE_PPC_HALF_TO_BE(*(uint16*)(gFrameBuffer+addr));
}

void FASTCALL gcard_read_4(uint32 addr, uint32 &data)
{
	addr-= IO_GCARD_FRAMEBUFFER_PA_START;
	data = MAYBE_PPC_WORD_TO_BE(*(uint32*)(gFrameBuffer+addr));
}

void FASTCALL gcard_read_8(uint32 addr, uint64 &data)
{
	addr-= IO_GCARD_FRAMEBUFFER_PA_START;
	data = MAYBE_PPC_DWORD_TO_BE(*(uint64*)(gFrameBuffer+addr));
}

void FASTCALL gcard_read_16(uint32 addr, uint128 *data)
{
	addr-= IO_GCARD_FRAMEBUFFER_PA_START;
#if HOST_ENDIANESS == HOST_ENDIANESS_LE
	uint8 *store = (uint8 *)data;

	for (int i=0; i<16; i++) {
		store[i] = gFrameBuffer[addr+15-i];
	}
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
	memmove(data, gFrameBuffer+addr, 16);
#else
#error Unsupported endianess
#endif
	//data->h = MAYBE_PPC_DWORD_TO_BE(*(uint64*)(gFrameBuffer+addr));
	//data->l = MAYBE_PPC_DWORD_TO_BE(*(uint64*)(gFrameBuffer+addr+8));
}

void FASTCALL gcard_read_16_native(uint32 addr, uint128 *data)
{
	addr-= IO_GCARD_FRAMEBUFFER_PA_START;

	memmove(data, gFrameBuffer+addr, 16);
}

static bool gVBLon = false;
static int gCurrentGraphicMode;

void gcard_raise_interrupt()
{
	if (gVBLon) pic_raise_interrupt(IO_PIC_IRQ_GCARD);
}

void gcard_osi(int cpu)
{
	IO_GRAPHIC_TRACE("osi: %d\n", ppc_cpu_get_gpr(cpu, 5));
	switch (ppc_cpu_get_gpr(cpu, 5)) {
	case 4:
		// cmount
		return;
	case 28: {
		// set_vmode
		uint vmode = ppc_cpu_get_gpr(cpu, 6)-1;
		if (vmode > gGraphicModes->count() || ppc_cpu_get_gpr(cpu, 7)) {
			ppc_cpu_set_gpr(cpu, 3, 1);
			return;
		}
		DisplayCharacteristics *chr = (DisplayCharacteristics *)(*gGraphicModes)[vmode];
		IO_GRAPHIC_TRACE("set mode %d\n", vmode);
		if (gDisplay->changeResolution(*chr)) {
			ppc_cpu_set_gpr(cpu, 3, 0);
			gcard_set_mode(*chr);
		} else {
			ppc_cpu_set_gpr(cpu, 3, 1);
		}
		return;
	}
	case 29: {
/*		
typedef struct osi_get_vmode_info {
	short		num_vmodes;
	short		cur_vmode;		// 1,2,... 
	short		num_depths;
	short		cur_depth_mode;		// 0,1,2,... 
	short		w,h;
	int		refresh;		// Hz/65536 

	int		depth;
	short		row_bytes;
	short		offset;
} osi_get_vmode_info_t;
*/
		// get_vmode_info
		int vmode = ppc_cpu_get_gpr(cpu, 6) - 1;
		int depth_mode = ppc_cpu_get_gpr(cpu, 7);
		if (vmode == -1) {
			vmode = gCurrentGraphicMode;
			depth_mode = ((DisplayCharacteristics *)(*gGraphicModes)[vmode])->bytesPerPixel*8;
		}
		if (vmode > (int)gGraphicModes->count() || vmode < 0) {
			ppc_cpu_set_gpr(cpu, 3, 1);
			return;
		}
		DisplayCharacteristics *chr = ((DisplayCharacteristics *)(*gGraphicModes)[vmode]);
		ppc_cpu_set_gpr(cpu, 3, 0);
		ppc_cpu_set_gpr(cpu, 4, (gGraphicModes->count()<<16) | (vmode+1));
		ppc_cpu_set_gpr(cpu, 5, (1<<16) | 0);
		ppc_cpu_set_gpr(cpu, 6, (chr->width << 16) | chr->height);
		ppc_cpu_set_gpr(cpu, 7, chr->vsyncFrequency << 16);
		ppc_cpu_set_gpr(cpu, 8, chr->bytesPerPixel*8);
		ppc_cpu_set_gpr(cpu, 9, ((chr->scanLineLength)<<16) | 0);
		return;
	}
	case 31:
		// set_video_power
		ppc_cpu_set_gpr(cpu, 3, 0);
		return;
	case 39:
		IO_GRAPHIC_TRACE("video_ctrl: %d\n", ppc_cpu_get_gpr(cpu, 6));
		// video_ctrl
		switch (ppc_cpu_get_gpr(cpu, 6)) {
		case 0:
			gVBLon = false;
			break;
		case 1:
			gVBLon = true;
			break;
		default:
			IO_GRAPHIC_ERR("39\n");
		}
		ppc_cpu_set_gpr(cpu, 3, 0);
		return;
	case 47:
//		printf("%c\n", gCPU.gpr[6]);
		return;
	case 59: {
		// set_color
		uint32 r7 = ppc_cpu_get_gpr(cpu, 7);
		gDisplay->setColor(ppc_cpu_get_gpr(cpu, 6), MK_RGB((r7>>16)&0xff, (r7>>8)&0xff, r7&0xff));
		ppc_cpu_set_gpr(cpu, 3, 0);
		return;
	}
	case 64: {
		// get_color
		RGB c = gDisplay->getColor(ppc_cpu_get_gpr(cpu, 6));
		ppc_cpu_set_gpr(cpu, 3, (RGB_R(c) << 16) | (RGB_G(c) << 8) | (RGB_B(c)));
		return;
	}
	case 116:
		// hardware_cursor_bla
//		SINGLESTEP("hw cursor!! %d, %d, %d\n", gCPU.gpr[6], gCPU.gpr[7], gCPU.gpr[8]);
		IO_GRAPHIC_TRACE("hw cursor!! %d, %d, %d\n", ppc_cpu_get_gpr(cpu, 6), ppc_cpu_get_gpr(cpu, 7), ppc_cpu_get_gpr(cpu, 8));
		gDisplay->setHWCursor(ppc_cpu_get_gpr(cpu, 6), ppc_cpu_get_gpr(cpu, 7), ppc_cpu_get_gpr(cpu, 8), NULL);
		return;
	}
	IO_GRAPHIC_ERR("unknown osi function\n");
}

/*
 * displayCharacteristicsFromString tries to create a(n unfinished) characteristic
 * from a String of the form [0-9]+x[0-9]+x(15|32)(@[0-9]+)?
 */
 
bool displayCharacteristicsFromString(DisplayCharacteristics &aChar, const String &s)
{
	String width, height, depth;
	String tmp, tmp2;
	if (!s.leftSplit('x', width, tmp)) return false;
	if (!width.toInt(aChar.width)) return false;
	if (!tmp.leftSplit('x', height, tmp2)) return false;
	if (!height.toInt(aChar.height)) return false;
	if (tmp2.leftSplit('@', depth, tmp)) {
		if (!depth.toInt(aChar.bytesPerPixel)) return false;	
		if (!tmp.toInt(aChar.vsyncFrequency)) return false;	
	} else {
		aChar.vsyncFrequency = -1;
		if (!tmp2.toInt(aChar.bytesPerPixel)) return false;
	}
	aChar.scanLineLength = -1;
	aChar.redShift = -1;
	aChar.redSize = -1;
	aChar.greenShift = -1;
	aChar.greenSize = -1;
	aChar.blueShift = -1;
	aChar.blueSize = -1;
	return true;
}

void gcard_add_characteristic(const DisplayCharacteristics &aChar)
{
	if (!gcard_supports_characteristic(aChar)) {
		DisplayCharacteristics *chr = new DisplayCharacteristics;
		*chr = aChar;
		gGraphicModes->insert(chr);
	}
}

bool gcard_supports_characteristic(const DisplayCharacteristics &aChar)
{
	return gGraphicModes->contains(&aChar);
}

/*
 *	gcard_finish_characteristic will fill out all fields 
 *	of aChar that aren't initialized yet (set to -1).
 */
bool gcard_finish_characteristic(DisplayCharacteristics &aChar)
{
	if (aChar.width == -1 || aChar.height == -1 || aChar.bytesPerPixel == -1) return false;
	if (aChar.vsyncFrequency == -1) aChar.vsyncFrequency = 60;
	if (aChar.scanLineLength == -1) aChar.scanLineLength = aChar.width * aChar.bytesPerPixel;
	switch (aChar.bytesPerPixel) {
	case 2:
		
		if (aChar.redShift == -1) aChar.redShift = 10;
		if (aChar.redSize == -1) aChar.redSize = 5;
		if (aChar.greenShift == -1) aChar.greenShift = 5;
		if (aChar.greenSize == -1) aChar.greenSize = 5;
		if (aChar.blueShift == -1) aChar.blueShift = 0;
		if (aChar.blueSize == -1) aChar.blueSize = 5;
		break;
	case 4:
		if (aChar.redShift == -1) aChar.redShift = 16;
		if (aChar.redSize == -1) aChar.redSize = 8;
		if (aChar.greenShift == -1) aChar.greenShift = 8;
		if (aChar.greenSize == -1) aChar.greenSize = 8;
		if (aChar.blueShift == -1) aChar.blueShift = 0;
		if (aChar.blueSize == -1) aChar.blueSize = 8;
		break;
	default:
		return false;
	}
	return true;
}

bool gcard_set_mode(DisplayCharacteristics &mode)
{
	uint tmp = gGraphicModes->getObjIdx(gGraphicModes->find(&mode));
	if (tmp == InvIdx) {
		return false;
	} else {
		gCurrentGraphicMode = tmp;
		return true;
	}
}

void gcard_init_modes()
{
	gGraphicModes = new Array(true);
	for (uint i=0; i < (sizeof stdVModes / sizeof stdVModes[0]); i++) {
		DisplayCharacteristics chr;
		chr.width = stdVModes[i].width;
		chr.height = stdVModes[i].height;
		chr.bytesPerPixel = stdVModes[i].bytesPerPixel;
		chr.scanLineLength = -1;
		chr.vsyncFrequency = -1;
		chr.redShift = -1;
		chr.redSize = -1;
		chr.greenShift = -1;
		chr.greenSize = -1;
		chr.blueShift = -1;
		chr.blueSize = -1;
		gcard_finish_characteristic(chr);
		gcard_add_characteristic(chr);
	}
}

void gcard_init_host_modes()
{
	Array modes(true);	
	gDisplay->getHostCharacteristics(modes);
	foreach (DisplayCharacteristics, chr, modes, {
		gcard_finish_characteristic(*chr);
		gcard_add_characteristic(*chr);
	});
}

void gcard_init()
{
	gPCI_Devices->insert(new PCI_GCard());
}

void gcard_done()
{
}

void gcard_init_config()
{
}
