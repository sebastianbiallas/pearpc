/*
 *	PearPC
 *	pci.cc
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

#include "tools/data.h"
#include "system/arch/sysendian.h"
#include "cpu/cpu.h"
#include "cpu/debug.h"
#include "cpu/mem.h"
#include "debug/tracers.h"
#include "pci.h"

#define PCI_ADDRESS_ECD(v) ((v) & 0x80000000)
#define PCI_ADDRESS_BUS(v) (((v)>>16) & 0xff)
#define PCI_ADDRESS_UNIT(v) (((v)>>11) & 0x1f)
#define PCI_ADDRESS_FUNCT(v) (((v)>>8) & 7)
#define PCI_ADDRESS_REG(v) (((v)) & 0xfc)
#define PCI_ADDRESS_TYPE(v) ((v) & 3)

uint32 gPCI_Address;
uint32 gPCI_Data;
static uint32 gPCI_Data_LE;
Container *gPCI_Devices;

class PCI_Bridge: public PCI_Device {
public:
	PCI_Bridge(const char *aName, uint8 aBus, uint8 aUnit)
		:PCI_Device(aName, aBus, aUnit)
	{
		mIORegsCount = 2;
	}
		
};

class PCI_BridgeHost: public PCI_Bridge {
public:
			PCI_BridgeHost();
};

class PCI_BridgeP2P: public PCI_Bridge {
public:
			PCI_BridgeP2P();
};

PCI_Device::PCI_Device(const char *aName, uint8 aBus, uint8 aUnit)
	:Object()
{
	mName = strdup(aName);
	mUnit = aUnit;
	mBus = aBus;
	mIORegsCount = 6;
	memset(&mConfig, 0, sizeof mConfig);
	memset(&mAddress, 0, sizeof mAddress);
	memset(&mPort, 0, sizeof mPort);
	memset(&mIORegType, 0, sizeof mIORegType);
	memset(&mIORegSize, 0, sizeof mIORegSize);
}

PCI_Device::~PCI_Device()
{
	free(mName);
}

int PCI_Device::compareTo(const Object *obj) const
{
	int busdelta = (int)mBus - (int)((PCI_Device*)obj)->mBus;
	if (!busdelta) {
		return (int)mUnit - (int)((PCI_Device*)obj)->mUnit;
	}
	return busdelta;
}

void PCI_Device::assignMemAddress(uint r, uint32 aAddress)
{
	IO_PCI_TRACE("assign-address[mem]: %s: %d: %08x\n", mName, r, aAddress);
	mAddress[r] = aAddress;
	mConfig[0x4] |= 0x2;	// Enable response in memory space
	mConfig[0x10+4*r] = aAddress | mIORegType[r];
	mConfig[0x11+4*r] = aAddress>>8;
	mConfig[0x12+4*r] = aAddress>>16;
	mConfig[0x13+4*r] = aAddress>>24;
}

void PCI_Device::assignIOPort(uint r, uint32 aPort)
{
	IO_PCI_TRACE("assign-address[io]: %s: %d: %08x\n", mName, r, aPort);
	mPort[r] = aPort;
	mConfig[0x4] |= 0x1;	// Enable response in io space
	mConfig[0x10+4*r] = aPort | 1; // Mark as io address
	mConfig[0x11+4*r] = aPort>>8;
	mConfig[0x12+4*r] = aPort>>16;
	mConfig[0x13+4*r] = aPort>>24;
}

bool PCI_Device::readMem(uint32 aAddress, uint32 &data, uint size)
{
	for (uint i=0; i < mIORegsCount; i++) {
		if ((mIORegType[i] & 1) == PCI_ADDRESS_SPACE_MEM 
		    && aAddress >= mAddress[i] && aAddress < (mAddress[i] + mIORegSize[i])) {
			if (!readDeviceMem(i, aAddress-mAddress[i], data, size)) {
				IO_PCI_ERR("%s: reg: %d: %08x read unimpl.\n", mName, i, aAddress-mAddress[i]);
			}
			return true;
		}
	}
	return false;
}

bool PCI_Device::readIO(uint32 aPort, uint32 &data, uint size)
{
	for (uint i=0; i < mIORegsCount; i++) {
		if (mIORegType[i] == PCI_ADDRESS_SPACE_IO
		    && aPort >= mPort[i] && aPort < (mPort[i] + mIORegSize[i])) {
			if (!readDeviceIO(i, aPort-mPort[i], data, size)) {
				IO_PCI_ERR("%s: reg: %d: %08x read(%d) unimpl.\n", mName, i, aPort-mPort[i], size);
			}
			return true;
		}
	}
	return false;
}

bool PCI_Device::writeMem(uint32 aAddress, uint32 data, uint size)
{
	for (uint i=0; i < mIORegsCount; i++) {
		if ((mIORegType[i] & 1) == PCI_ADDRESS_SPACE_MEM 
		    && aAddress >= mAddress[i] && aAddress < (mAddress[i] + mIORegSize[i])) {
			if (!writeDeviceMem(i, aAddress-mAddress[i], data, size)) {
				IO_PCI_ERR("%s: reg: %d: %08x write unimpl.\n", mName, i, aAddress-mAddress[i]);
			}
			return true;
		}
	}
	return false;
}

bool PCI_Device::writeIO(uint32 aPort, uint32 data, uint size)
{
	for (uint i=0; i < mIORegsCount; i++) {
		if (mIORegType[i] == PCI_ADDRESS_SPACE_IO
		    && aPort >= mPort[i] && aPort < (mPort[i] + mIORegSize[i])) {
			if (!writeDeviceIO(i, aPort-mPort[i], data, size)) {
				IO_PCI_ERR("%s: reg: %d: %08x write(%d) unimpl.\n", mName, i, aPort-mPort[i], size);
			}
			return true;
		}
	}
	return false;
}

bool PCI_Device::readDeviceMem(uint r, uint32 address, uint32 &data, uint size)
{
	return false;
}

bool PCI_Device::readDeviceIO(uint r, uint32 io, uint32 &data, uint size)
{
	return false;
}

bool PCI_Device::writeDeviceMem(uint r, uint32 address, uint32 data, uint size)
{
	return false;
}

bool PCI_Device::writeDeviceIO(uint r, uint32 io, uint32 data, uint size)
{
	return false;
}

void PCI_Device::readConfig(uint reg)
{
	gPCI_Data = ppc_word_from_LE(*(uint32*)&(mConfig[reg]));
}

void PCI_Device::writeConfig(uint reg, int offset, int size)
{
	if (reg >= 0x10 && reg < (4*mIORegsCount+0x10)) {
		uint rreg = (reg-0x10) >> 2;
		if (mIORegSize[rreg]) {
			if (gPCI_Data != 0xffffffff && gPCI_Data != 0) {
				if (mIORegType[rreg]==PCI_ADDRESS_SPACE_MEM) {
					assignMemAddress(rreg, gPCI_Data & ~0xf);
				} else {
					assignIOPort(rreg,  gPCI_Data & ~0x3);
				}
			}
			if ((mIORegType[rreg] & 1) == PCI_ADDRESS_SPACE_MEM) {
				uint32 x = 8;
				for (int i=2; i<31; i++) {
					gPCI_Data &= ~x;
					x <<= 1;
					if (x >= mIORegSize[rreg]) break;
				}
				gPCI_Data &= ~0xf;
			} else {
				uint32 x = 2;
				for (int i=2; i<31; i++) {
					gPCI_Data &= ~x;
					x <<= 1;
					if (x >= mIORegSize[rreg]) break;
				}
				gPCI_Data &= ~0x3;
			}
			gPCI_Data |= mIORegType[rreg];
		}
	}
	*(uint32*)&(mConfig[reg]) = ppc_word_to_LE(gPCI_Data);
}

void PCI_Device::setCommand(uint16 command)
{
}

void PCI_Device::setStatus(uint16 status)
{
}

PCI_BridgeP2P::PCI_BridgeP2P()
	:PCI_Bridge("pci-bridge-p2p", 0x00, 0x0d)
{
	mConfig[0x00] = 0x11;	// vendor ID
	mConfig[0x01] = 0x10;
	mConfig[0x02] = 0x26;	// unit ID
	mConfig[0x03] = 0x00;
	
	mConfig[0x08] = 0x02;	// revision
	mConfig[0x09] = 0x00; 	// programming interface code
	mConfig[0x0a] = 0x04;	// pci2pci
	mConfig[0x0b] = 0x06;	// bridge

	mConfig[0x0e] = 0x01;	// header-type
	
	mConfig[0x18] = 0x0;	// primary bus number
	mConfig[0x19] = 0x1;	// secondary bus number
	mConfig[0x1a] = 0x1;	// highest bus number behind bridge
	mConfig[0x1c] = 0x10; 	// i/o base behind bridge
	mConfig[0x1d] = 0x20;	// i/o limit 

	mConfig[0x20] = 0x80; 	// memory base
	mConfig[0x21] = 0x80; 	// memory base
	mConfig[0x22] = 0x90; 	// memory limit
	mConfig[0x23] = 0x80; 	// memory limit

	mConfig[0x24] = 0x00; 	// prefetch memory base
	mConfig[0x25] = 0x84; 	// prefetch memory base
	mConfig[0x26] = 0x00; 	// prefetch memory limit
	mConfig[0x27] = 0x85; 	// prefetch memory limit
	
	mConfig[0x32] = 0x00;	// upper io limit
}

PCI_BridgeHost::PCI_BridgeHost()
	:PCI_Bridge("pci-bridge-host", 0x00, 0x0e)
{
	mConfig[0x00] = 0x11;	// vendor ID
	mConfig[0x01] = 0x10;
	mConfig[0x02] = 0x26;	// unit ID
	mConfig[0x03] = 0x00;
	
	mConfig[0x08] = 0x00;	// revision
	mConfig[0x09] = 0x01; 
	mConfig[0x0a] = 0x04;	// host
	mConfig[0x0b] = 0x06;	// bridge

	mConfig[0x0e] = 0x01;	// header-type
	
	mConfig[0x18] = 0x0;	// primary bus number
	mConfig[0x19] = 0x1;	// secondary bus number
	mConfig[0x1a] = 0x0;	// highest bus number behind bridge
	mConfig[0x1c] = 0x0; 	// i/o behind bridge

	mConfig[0x20] = 0x0; 	// memory base
	mConfig[0x21] = 0x0; 	// memory base
	mConfig[0x22] = 0x1; 	// memory limit
	mConfig[0x23] = 0x0; 	// memory limit

	mConfig[0x24] = 0x0; 	// prefetch memory base
	mConfig[0x25] = 0x0; 	// prefetch memory base
	mConfig[0x26] = 0x0; 	// prefetch memory limit
	mConfig[0x27] = 0x0; 	// prefetch memory limit
}

/**************************************************************************************
 *
 */
 
static void pci_config_read_write(bool write, int offset, int size)
{
	IO_PCI_TRACE("ecd: %d bus: 0x%02x unit: 0x%02x funct: 0x%02x reg: 0x%02x type: %d\n", 
		PCI_ADDRESS_ECD(gPCI_Address)?1:0, PCI_ADDRESS_BUS(gPCI_Address), 
		PCI_ADDRESS_UNIT(gPCI_Address), PCI_ADDRESS_FUNCT(gPCI_Address),
		PCI_ADDRESS_REG(gPCI_Address),PCI_ADDRESS_TYPE(gPCI_Address));
	
	if (PCI_ADDRESS_FUNCT(gPCI_Address)) {
		IO_PCI_ERR("PCI: func != 0\n");
	}
	if (PCI_ADDRESS_TYPE(gPCI_Address)) {
		IO_PCI_ERR("PCI: type != 0\n");
	}
	if (PCI_ADDRESS_ECD(gPCI_Address)) {
		PCI_Device empty("", PCI_ADDRESS_BUS(gPCI_Address), PCI_ADDRESS_UNIT(gPCI_Address));
		PCI_Device *p = (PCI_Device*)gPCI_Devices->get(gPCI_Devices->find(&empty));
		if (!p) {
			if (!write) gPCI_Data = 0;
			return;
		}
		if (write) {
			gPCI_Data = ppc_word_from_LE(gPCI_Data_LE);
			p->writeConfig(PCI_ADDRESS_REG(gPCI_Address), offset, size);
		} else {
			p->readConfig(PCI_ADDRESS_REG(gPCI_Address));
			gPCI_Data_LE = ppc_word_to_LE(gPCI_Data);
		}
	} else {
		IO_PCI_WARN("PCI: ecd != 1\n");
	}
}

void pci_write(uint32 addr, uint32 data, int size)
{
	if (addr != IO_PCI_PA_START) IO_PCI_TRACE("write (%d) @%08x: %08x (from %08x, %08x)\n", size, addr, data, gCPU.pc, gCPU.lr);
	addr -= IO_PCI_PA_START;
	switch (addr) {
	case 0:
	case 0xcf8:
		if (size != 4) {
			IO_PCI_ERR("pci bla\n");
		}
		gPCI_Address = data;
		pci_config_read_write(false, 0, 4);
		return;
	case 0x200000:
	case 0x200cfc:
		if (size == 1) {
			void *p = &gPCI_Data_LE;
			*(uint8 *)p = data;
			pci_config_read_write(true, 0, 1);
			return;
		}
		if (size == 2) {
			void *p = &gPCI_Data_LE;
			*(uint16 *)p = ppc_half_to_LE(data);
			pci_config_read_write(true, 0, 2);
			return;
		}
		if (size != 4) {
			IO_PCI_ERR("pci bla\n");
		}
		gPCI_Data_LE = ppc_word_to_LE(data);
		pci_config_read_write(true, 0, 4);
		return;
	case 0x200001:
	case 0x200cfd: {
		if (size != 1) {
			IO_PCI_ERR("pci bla\n");
		}
		char *p = (char*)&gPCI_Data_LE;
		*(uint8 *)(p+1) = data;
		pci_config_read_write(true, 1, 1);
		return;
	}
	case 0x200002:
	case 0x200cfe: {
		if (size > 2) {
			IO_PCI_ERR("pci bla\n");
		}
		if (size == 1) {
			char *p = (char *)&gPCI_Data_LE;
			*(uint8 *)(p+2) = data;
			pci_config_read_write(true, 2, 1);
			return;
		}
		char *p = (char *)&gPCI_Data_LE;
		*(uint16 *)(p+2) = ppc_half_to_LE(data);
		pci_config_read_write(true, 2, 2);
		return;
	}
	case 0x200003:
	case 0x200cff: {
		if (size != 1) {
			IO_PCI_ERR("pci bla\n");
		}
		char *p = (char *)&gPCI_Data_LE;
		*(uint8 *)(p+3) = data;
		pci_config_read_write(true, 3, 1);
		return;
	}
	}
	SINGLESTEP("unknown service\n");
}

void pci_read(uint32 addr, uint32 &data, int size)
{
//	SINGLESTEP("usdf\n");
	if (addr != IO_PCI_PA_START) IO_PCI_TRACE("read (%d) @%08x (from %08x, lr: %08x) -> \n", size, addr, gCPU.pc, gCPU.lr);
	addr -= IO_PCI_PA_START;
	switch (addr) {
	case 0:
	case 0xcf8:
		data = gPCI_Address;
		return;
	case 0x200000:
	case 0x200cfc:
		if (size == 1) {
			void *p = &gPCI_Data_LE;
			data = *(uint8 *)p;
			IO_PCI_TRACE("%02x\n", data);
			return;
		}
		if (size == 2) {
			void *p = &gPCI_Data_LE;
			data = ppc_half_from_LE(*(uint16 *)p);
			IO_PCI_TRACE("%04x\n", data);
			return;
		}
		if (size != 4) {
			IO_PCI_ERR("pci bla\n");
		}
		data = ppc_word_from_LE(gPCI_Data_LE);
		IO_PCI_TRACE("%08x\n", data);
		return;
	case 0x200001:
	case 0x200cfd: {
		if (size != 1) {
			IO_PCI_ERR("pci bla\n");
		}
		char *p = (char*)&gPCI_Data_LE;
		data = *(uint8 *)(p+1);
		IO_PCI_TRACE("%02x\n", data);
		return;
	}
	case 0x200002: 
	case 0x200cfe: {
		if (size > 2) {
			IO_PCI_ERR("pci bla\n");
		}
		if (size==1) {
			char *p = (char*)&gPCI_Data_LE;
			data = *(uint8 *)(p+2);
			IO_PCI_TRACE("%02x\n", data);
			return;
		}
		char *p = (char*)&gPCI_Data_LE;
		data = ppc_half_from_LE(*(uint16 *)(p+2));
		IO_PCI_TRACE("%04x\n", data);
		return;
	}
	case 0x200003:
	case 0x200cff:
		if (size != 1) {
			IO_PCI_ERR("pci bla\n");
		}
		char *p = (char*)&gPCI_Data_LE;
		data = *(uint8 *)(p+3);
		IO_PCI_TRACE("%02x\n", data);
		return;
	}
	data = 0;
	SINGLESTEP("%08x unknown service\n", addr);
}

bool isa_read(uint32 addr, uint32 &data, int size)
{
	// Translate address into port
	addr -= IO_ISA_PA_START;
	ObjHandle oh = gPCI_Devices->findFirst();
	while (oh != InvObjHandle) {
		PCI_Device *pd = (PCI_Device*)gPCI_Devices->get(oh);
		if (pd->readIO(addr, data, size)) {
			return true;
		}
		oh = gPCI_Devices->findNext(oh);
	}
	data = 0;
//	gSinglestep = true;
	IO_PCI_WARN("port %08x not registered! (for read)\n", addr);
	return false;
}

bool isa_write(uint32 addr, uint32 data, int size)
{
	// Translate address into port
	addr -= IO_ISA_PA_START;
	ObjHandle oh = gPCI_Devices->findFirst();
	while (oh != InvObjHandle) {
		PCI_Device *pd = (PCI_Device*)gPCI_Devices->get(oh);
		if (pd->writeIO(addr, data, size)) {			
			return true;
		}
		oh = gPCI_Devices->findNext(oh);
	}
//	gSinglestep = true;
	IO_PCI_WARN("port %08x not registered! (for write)\n", addr);
	return false;
}

bool pci_write_device(uint32 addr, uint32 data, int size)
{
	IO_PCI_TRACE("write DEVICE (%d) @%08x %08x (from %08x, lr: %08x)\n", size, addr, data, gCPU.pc, gCPU.lr);
	ObjHandle oh = gPCI_Devices->findFirst();
	while (oh != InvObjHandle) {
		PCI_Device *pd = (PCI_Device*)gPCI_Devices->get(oh);
		if (pd->writeMem(addr, data, size)) {			
			return true;
		}
		oh = gPCI_Devices->findNext(oh);
	}
	return false;
}

bool pci_read_device(uint32 addr, uint32 &data, int size)
{
	IO_PCI_TRACE("read DEVICE (%d) @%08x (from %08x, lr: %08x)\n", size, addr, gCPU.pc, gCPU.lr);
	ObjHandle oh = gPCI_Devices->findFirst();
	while (oh != InvObjHandle) {
		PCI_Device *pd = (PCI_Device*)gPCI_Devices->get(oh);
		if (pd->readMem(addr, data, size)) {
			IO_PCI_TRACE("->%08x\n", data);
			return true;
		}
		oh = gPCI_Devices->findNext(oh);
	}
	data = 0;
	return false;
}

// PCI devices
#include "io/graphic/gcard.h"
#include "io/ide/ide.h"
#include "io/macio/macio.h"
#include "io/3c90x/3c90x.h"
#include "io/rtl8139/rtl8139.h"
#include "io/usb/usb.h"
#include "io/serial/serial.h"

void pci_init()
{
	gPCI_Devices = new AVLTree(true);
	gPCI_Devices->insert(new PCI_BridgeP2P());
//	gPCI_Devices->insert(new PCI_BridgeHost());

	gcard_init();
	ide_init();
	macio_init();
	_3c90x_init();
	rtl8139_init();
	usb_init();
	serial_init();
}

void pci_done()
{
	serial_done();
	usb_done();
	rtl8139_done();
	_3c90x_done();
	macio_done();
	ide_done();
	gcard_done();

	delete gPCI_Devices;
}

void pci_init_config()
{
	gcard_init_config();
	ide_init_config();
	macio_init_config();
	_3c90x_init_config();
	rtl8139_init_config();
	usb_init_config();
	serial_init_config();
}
