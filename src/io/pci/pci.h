/*
 *	PearPC
 *	pci.h
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

#ifndef __IO_PCI_H__
#define __IO_PCI_H__

#include "tools/data.h"
#include "system/types.h"
#include "system/display.h"

#define IO_PCI_PA_START		0xfec00000
#define IO_PCI_PA_END		0xfef00000

#define IO_ISA_PA_START		0xfe000000
#define IO_ISA_PA_END		0xfe200000

#define IO_PCI_DEVICE_PA_START	0x80000000
#define IO_PCI_DEVICE_PA_END	0x81000000

extern uint32 gPCI_Data;
extern Container *gPCI_Devices;

#define PCI_ADDRESS_SPACE_MEM		0
#define PCI_ADDRESS_SPACE_MEM_PREFETCH	8
#define PCI_ADDRESS_SPACE_IO		1

class PCI_Device: public Object {
public:
	uint8	mConfig[64*4];
	char	*mName;
	uint8	mBus;
	uint8	mUnit;
	uint	mIORegsCount;
	uint	mIORegSize[6];
	uint	mIORegType[6];

	uint32	mAddress[6];
	uint32	mPort[6];

			PCI_Device(const char *name, uint8 bus, uint8 unit);
			~PCI_Device();
	virtual	int	compareTo(const Object *obj) const;

	void		assignMemAddress(uint r, uint32 address);
	void		assignIOPort(uint r, uint32 port);
	virtual bool	readMem(uint32 address, uint32 &data, uint size);
	virtual bool	readIO(uint32 address, uint32 &data, uint size);
	virtual bool	writeMem(uint32 address, uint32 data, uint size);
	virtual bool	writeIO(uint32 address, uint32 data, uint size);

	virtual void	readConfig(uint reg);
	virtual bool	readDeviceMem(uint r, uint32 address, uint32 &data, uint size);
	virtual bool	readDeviceIO(uint r, uint32 port, uint32 &data, uint size);
	virtual void	writeConfig(uint reg, int offset, int size);
	virtual bool	writeDeviceMem(uint r, uint32 address, uint32 data, uint size);
	virtual bool	writeDeviceIO(uint r, uint32 port, uint32 data, uint size);
	virtual	void	setCommand(uint16 command);
	virtual void	setStatus(uint16 status);
};

void pci_write(uint32 addr, uint32 data, int size);
void pci_read(uint32 addr, uint32 &data, int size);
bool isa_write(uint32 addr, uint32 data, int size);
bool isa_read(uint32 addr, uint32 &data, int size);
bool pci_write_device(uint32 addr, uint32 data, int size);
bool pci_read_device(uint32 addr, uint32 &data, int size);

void pci_init();
void pci_init_config();

#endif

