/*
 *	PearPC
 *	pic.h
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

#ifndef __IO_PIC_H__
#define __IO_PIC_H__

#include "system/types.h"
#include "system/display.h"

#define IO_PIC_PA_START 0x80800000
#define IO_PIC_PA_END 0x80800040

/*
 *	interrupts < 32 and in this mask are level'd (20 <= irq <= 28)
 *	all other are of edge type
 */
#define IO_PIC_LEVEL_TYPE 0x1ff00000

#define IO_PIC_IRQ_ETHERNET0	5
#define IO_PIC_IRQ_ETHERNET1	7
#define IO_PIC_IRQ_CUDA		18
#define IO_PIC_IRQ_NMI_XMON	20
#define IO_PIC_IRQ_GCARD	23
#define IO_PIC_IRQ_IDE0		26
#define IO_PIC_IRQ_USB		28

void pic_write(uint32 addr, uint32 data, int size);
void pic_read(uint32 addr, uint32 &data, int size);

void pic_raise_interrupt(int intr);
void pic_cancel_interrupt(int intr);

void pic_init();
void pic_done();
void pic_init_config();


#endif

