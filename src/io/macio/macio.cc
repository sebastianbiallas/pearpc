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

#include "debug/tracers.h"
#include "macio.h"

#define MACIO_DBDMA_ADDRESS_CONTROL	0x00
#define MACIO_DBDMA_ADDRESS_STATUS	0x04
#define MACIO_DBDMA_ADDRESS_CMD_PTR_HI	0x08
#define MACIO_DBDMA_ADDRESS_CMD_PTR_LO	0x0c
#define MACIO_DBDMA_ADDRESS_INTR_SEL	0x10
#define MACIO_DBDMA_ADDRESS_BRANCH_SEL	0x14
#define MACIO_DBDMA_ADDRESS_WAIT_SEL	0x18
#define MACIO_DBDMA_ADDRESS_MODES	0x1c
#define MACIO_DBDMA_ADDRESS_DATA_PTR_HI	0x20
#define MACIO_DBDMA_ADDRESS_DATA_PTR_LO	0x24
#define MACIO_DBDMA_ADDRESS_ADDRESS_HI	0x2c
	
/*
 *	Channel control and status flags
 */
#define MACIO_DBDMA_RUN		0x8000
#define MACIO_DBDMA_PAUSE	0x4000
#define MACIO_DBDMA_FLUSH	0x2000
#define MACIO_DBDMA_WAKE	0x1000
#define MACIO_DBDMA_DEAD	0x800
#define MACIO_DBDMA_ACTIVE	0x400
#define MACIO_DBDMA_BT		0x100
#define MACIO_DBDMA_S7		0x80
#define MACIO_DBDMA_S6		0x40
#define MACIO_DBDMA_S5		0x20
#define MACIO_DBDMA_S4		0x10
#define MACIO_DBDMA_S3		0x8
#define MACIO_DBDMA_S2		0x4
#define MACIO_DBDMA_S1		0x2
#define MACIO_DBDMA_S0		0x1

/*
 *	commands
 */

#define MACIO_DBDMA_CMD_OUTPUT_MORE	0
#define MACIO_DBDMA_CMD_OUTPUT_LAST	1
#define MACIO_DBDMA_CMD_INPUT_MORE	2
#define MACIO_DBDMA_CMD_INPUT_LAST	3
#define MACIO_DBDMA_CMD_STORE_QUAD	4
#define MACIO_DBDMA_CMD_LOAD_QUAD	5
#define MACIO_DBDMA_CMD_NOP		6
#define MACIO_DBDMA_CMD_STOP		7

/*
 *	keys
 */

#define  MACIO_DBDMA_KEY_STREAM0	0
#define  MACIO_DBDMA_KEY_STREAM1	1
#define  MACIO_DBDMA_KEY_STREAM2	2
#define  MACIO_DBDMA_KEY_STREAM3	3
#define  MACIO_DBDMA_KEY_REGS		5
#define  MACIO_DBDMA_KEY_SYSTEM		6
#define  MACIO_DBDMA_KEY_DEVICE		7

#define  MACIO_DBDMA_INT_NEVER		0
#define  MACIO_DBDMA_INT_IF_TRUE	1
#define  MACIO_DBDMA_INT_IF_FALSE	2
#define  MACIO_DBDMA_INT_ALWAYS		3

#define  MACIO_DBDMA_BRANCH_NEVER	0
#define  MACIO_DBDMA_BRANCH_IF_TRUE	1
#define  MACIO_DBDMA_BRANCH_IF_FALSE	2
#define  MACIO_DBDMA_BRANCH_ALWAYS	3

#define  MACIO_DBDMA_WAIT_NEVER		0
#define  MACIO_DBDMA_WAIT_IF_TRUE	1
#define  MACIO_DBDMA_WAIT_IF_FALSE	2
#define  MACIO_DBDMA_WAIT_ALWAYS	3

struct MacIO_DBDMA_ChannelRegs {
        uint32 control;
        uint32 status;
        uint32 commandPtrHi;
        uint32 commandPtrLo;
        uint32 interruptSelect;
        uint32 branchSelect;
        uint32 waitSelect;
        uint32 transferModes;
        uint32 data2PtrHi;
        uint32 data2PtrLo;
        uint32 addressHi;
};
 
PCI_MacIO::PCI_MacIO()
	:PCI_Device("pci-macio", 0x01, 0x05)
{
	mIORegSize[0] = 0x80000;
	mIORegType[0] = PCI_ADDRESS_SPACE_MEM;

	mConfig[0x00] = 0x6b;	// vendor ID
	mConfig[0x01] = 0x10;
	mConfig[0x02] = 0x17;	// unit ID
	mConfig[0x03] = 0x00;

	mConfig[0x08] = 0x00;	// revision
	mConfig[0x09] = 0x00; 	// programming interface code
	mConfig[0x0a] = 0x00;	// pci2pci
	mConfig[0x0b] = 0xff;	// bridge

	mConfig[0x0e] = 0x00;	// header-type

	assignMemAddress(0, 0x80800000);

	mConfig[0x3c] = 0x18;
	mConfig[0x3d] = 1;
	mConfig[0x3e] = 0;
	mConfig[0x3f] = 0;	
}

bool PCI_MacIO::readDeviceMem(uint r, uint32 address, uint32 &data, uint size)
{
	if (r==0 && address >= 0x8000 && address < 0x8100) {
		address -= 0x8000;
		IO_MACIO_TRACE("dbdma: read(%d) @%08x\n", size, address);
		if (size != 4) IO_MACIO_ERR("read with size != 4\n");
		data = 0;
		switch (address) {
		case MACIO_DBDMA_ADDRESS_CONTROL:
			return true;
		case MACIO_DBDMA_ADDRESS_STATUS:
			return true;
		case MACIO_DBDMA_ADDRESS_CMD_PTR_HI:
			return true;
		case MACIO_DBDMA_ADDRESS_CMD_PTR_LO:
			return true;
		case MACIO_DBDMA_ADDRESS_INTR_SEL:
			return true;
		case MACIO_DBDMA_ADDRESS_BRANCH_SEL:
			return true;
		case MACIO_DBDMA_ADDRESS_WAIT_SEL:
			return true;
		case MACIO_DBDMA_ADDRESS_MODES:
			return true;
		case MACIO_DBDMA_ADDRESS_DATA_PTR_HI:
			return true;
		case MACIO_DBDMA_ADDRESS_DATA_PTR_LO:
			return true;
		case MACIO_DBDMA_ADDRESS_ADDRESS_HI:
			return true;
		}
	}
	return false;
}

bool PCI_MacIO::writeDeviceMem(uint r, uint32 address, uint32 data, uint size)
{
	if (r==0 && address >= 0x8000 && address < 0x8100) {
		address -= 0x8000;
		IO_MACIO_TRACE("dbdma: write(%d) @%08x: %08x\n", size, address, data);
		if (size != 4) IO_MACIO_ERR("read with size != 4\n");
		switch (address) {
		case MACIO_DBDMA_ADDRESS_CONTROL:
			return true;
		case MACIO_DBDMA_ADDRESS_STATUS:
			return true;
		case MACIO_DBDMA_ADDRESS_CMD_PTR_HI:
			return true;
		case MACIO_DBDMA_ADDRESS_CMD_PTR_LO:
			return true;
		case MACIO_DBDMA_ADDRESS_INTR_SEL:
			return true;
		case MACIO_DBDMA_ADDRESS_BRANCH_SEL:
			return true;
		case MACIO_DBDMA_ADDRESS_WAIT_SEL:
			return true;
		case MACIO_DBDMA_ADDRESS_MODES:
			return true;
		case MACIO_DBDMA_ADDRESS_DATA_PTR_HI:
			return true;
		case MACIO_DBDMA_ADDRESS_DATA_PTR_LO:
			return true;
		case MACIO_DBDMA_ADDRESS_ADDRESS_HI:
			return true;
		}
	}
	return false;
}

void macio_init()
{
	gPCI_Devices->insert(new PCI_MacIO());
}

void macio_done()
{
}

void macio_init_config()
{
}
