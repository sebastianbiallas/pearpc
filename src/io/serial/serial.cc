/*
 *	PearPC
 *	serial.cc
 *
 *	Copyright (C) 2005 Daniel Foesch (dfoesch@cs.nmsu.edu)
 *
 *	Reverse Engineered from how OpenBIOS access the serial device
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

#include "debug/tracers.h"
#include "system/arch/sysendian.h"
#include "io/pci/pci.h"
#include "serial.h"

#include <cstring>

#define SERIAL_DATA	0x00
#define SERIAL_CONTROL	0x05

extern bool gSinglestep;

/*
 *
 */
class PCI_SERIAL: public PCI_Device {
public:

PCI_SERIAL()
	:PCI_Device("pci-serial", 0x01, 0x42)
{
	mIORegSize[0] = 0x0010;
	mIORegType[0] = PCI_ADDRESS_SPACE_MEM;

	mConfig[0x00] = 0x00;	// vendor ID
	mConfig[0x01] = 0x00;
	mConfig[0x02] = 0x00;	// unit ID
	mConfig[0x03] = 0x00;
	
	mConfig[0x08] = 0x00;	// revision
	mConfig[0x09] = 0x00; 	// 
	mConfig[0x0a] = 0x00;	// 
	mConfig[0x0b] = 0x00;	// 

	mConfig[0x0e] = 0x00;	// header-type
	
	assignMemAddress(0, 0x800003f8);
	
	mConfig[0x3c] = 0x03;
	mConfig[0x3d] = 0x03;
	mConfig[0x3e] = 0x03;
	mConfig[0x3f] = 0x03;
	
	reset();
}

void	reset()
{
	return;
}

virtual bool readDeviceMem(uint r, uint32 address, uint32 &data, uint size)
{
	if (r != 0) return false;
	if (size != 1) return false;
	IO_USB_TRACE("read(r=%d, a=%08x, %d)\n", r, address, size);

	switch (address) {
	case SERIAL_DATA:
		data = fgetc(stdin);
		break;

	case SERIAL_CONTROL:
		data = 0x60;
		return true;
	default:
		return false;
	}
	
//	gSinglestep = true;
	return true;
}

virtual bool writeDeviceMem(uint r, uint32 address, uint32 data, uint size)
{
	if (r != 0) return false;
	if (size != 1) return false;
	IO_USB_TRACE("write(r=%d, a=%08x, data=%08x, %d)\n", r, address, data, size);

	switch (address) {
	case SERIAL_DATA:
		ht_printf("%c", data);
		gDisplay->printf("%c", data);
		break;

	case SERIAL_CONTROL:
		// intentionally left blank
		break;

	default:
		return false;
	}

//	gSinglestep = true;
	return true;
}

};


#include "configparser.h"

#define SERIAL_KEY_INSTALLED	"pci_serial_installed"

void serial_init()
{
	void *err;

	if (gConfig->getConfigInt(SERIAL_KEY_INSTALLED)) {
		err = gPCI_Devices->insert(new PCI_SERIAL());
	}
}

void serial_done()
{
}

void serial_init_config()
{
	gConfig->acceptConfigEntryIntDef(SERIAL_KEY_INSTALLED, 0);
}
