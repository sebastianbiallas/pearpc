/*
 *	PearPC
 *	macio.h
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

#ifndef __IO_MACIO_H__
#define __IO_MACIO_H__

#include "io/pci/pci.h"

class PCI_MacIO: public PCI_Device {
public:
			PCI_MacIO();
	virtual bool	readDeviceMem(uint r, uint32 address, uint32 &data, uint size);
	virtual bool	writeDeviceMem(uint r, uint32 address, uint32 data, uint size);
};

void macio_init();
void macio_done();
void macio_init_config();

#endif

