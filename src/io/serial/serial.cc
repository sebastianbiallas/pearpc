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

#define SERIAL_DATA		0x00
#define SERIAL_INTR		0x01
#define SERIAL_INTR_ID		0x02
#define SERIAL_FORMAT		0x03
#define SERIAL_CONTROL_OUT	0x04
#define SERIAL_STATE		0x05
#define SERIAL_CONTROL_IN	0x06
#define SERIAL_SCRATCH		0x07

/* activate with FORMAT_DLAB: */
#define SERIAL_LATCH_LSB	0x00
#define SERIAL_LATCH_MSB	0x01

/* SERIAL_INTR flags */
#define INTR_RxRD	0x01
#define INTR_TBE	0x02
#define INTR_ERBK	0x04
#define INTR_SINP	0x08

/* SERIAL_INTR_ID flags */
#define INTR_PND	0x01
#define INTR_ID0	0x02
#define INTR_ID1	0x04

#define INTR_ID			(ID0|ID1)
#define INTR_INPUT_CHANGE	(0)
#define INTR_BUFFER_EMPTY	(ID0)
#define INTR_RECEIVED		(ID1)
#define INTR_ERR		(ID0|ID1)

/* SERIAL_FORMAT flags */
#define FORMAT_DATA	0x03
#define FORMAT_STOP	0x04
#define FORMAT_PARITY	0x38
#define FORMAT_BRK	0x40
#define FORMAT_DLAB	0x80

/* SERIAL_CONTROL_OUT */
#define OUT_DTR		0x01
#define OUT_RTS		0x02
#define OUT_1		0x04
#define OUT_2		0x08
#define OUT_LOOP	0x10

/* SERIAL_STATE */
#define STATE_RxRD	0x01
#define STATE_OVFL	0x02
#define STATE_PAR	0x04
#define STATE_FRM	0x08
#define STATE_BRK	0x10
#define STATE_TBE	0x20
#define STATE_TXE	0x40

/* SERIAL_CONTROL_IN */
#define IN_DCTS		0x01
#define IN_DDSR		0x02
#define IN_DRI		0x04
#define IN_DDCD		0x08
#define IN_CTS		0x10
#define IN_DSR		0x20
#define IN_RI		0x40
#define IN_DCD		0x80

extern bool gSinglestep;

struct SerialState {
	uint8	intr;
	uint8	intr_id;
	uint8	format;
	uint8	scratch;
};

/*
 *
 */
class PCI_Serial: public PCI_Device {
	SerialState state;
public:

PCI_Serial()
	:PCI_Device("pci-serial", 0x01, 0x42)
{
	mIORegSize[0] = 0x0010;
	mIORegType[0] = PCI_ADDRESS_SPACE_IO;

	mConfig[0x00] = 0x00;	// vendor ID
	mConfig[0x01] = 0x00;
	mConfig[0x02] = 0x00;	// unit ID
	mConfig[0x03] = 0x00;
	
	mConfig[0x08] = 0x00;	// revision
	mConfig[0x09] = 0x00; 	// 
	mConfig[0x0a] = 0x00;	// 
	mConfig[0x0b] = 0x00;	// 

	mConfig[0x0e] = 0x00;	// header-type
	
	assignIOPort(0, 0x000003f8);
	
	mConfig[0x3c] = 0x03;
	mConfig[0x3d] = 0x03;
	mConfig[0x3e] = 0x03;
	mConfig[0x3f] = 0x03;
	
	reset();
}

void	reset()
{
	memset(&state, 0, sizeof state);
}

static const char *a2n(int a)
{
	if (a <= 0x7) {
		static const char *n[] = {"data", "intr", "intr_id", "format", "ctrlout", "state", "ctrlin", "scratch"};
		return n[a];
	} else {
		static char bl[10];
		sprintf(bl, "%08x", a);
	}
}

virtual bool readDeviceIO(uint r, uint32 address, uint32 &data, uint size)
{
	IO_SERIAL_TRACE("read(r=%d, a=%s, %d)\n", r, a2n(address), size);
	if (r != 0) return false;
	if (size != 1) return false;

	if ((state.format & FORMAT_DLAB) && !(address & 0xe)) {
		switch (address) {
		case SERIAL_LATCH_LSB:
		case SERIAL_LATCH_MSB:;
		}
		return false;
	}

	switch (address) {
	case SERIAL_DATA:
		data = 0;		
		return true;
	case SERIAL_INTR:
		data = state.intr & 0x0f;
		return true;
	case SERIAL_INTR_ID:
		data = state.intr_id & 0x07;
		return true;
	case SERIAL_FORMAT:
		data = state.format;
		return true;
	case SERIAL_CONTROL_OUT:
		return true;
	case SERIAL_STATE:
		return true;
	case SERIAL_CONTROL_IN:
		return true;
	case SERIAL_SCRATCH:
		data = state.scratch;
		return true;
	default:
		return false;
	}
	
//	gSinglestep = true;
	return true;
}

virtual bool writeDeviceIO(uint r, uint32 address, uint32 data, uint size)
{
	IO_SERIAL_TRACE("write(r=%d, a=%s, data=%08x, %d)\n", r, a2n(address), data, size);
	if (r != 0) return false;
	if (size != 1) return false;

	if ((state.format & FORMAT_DLAB) && !(address & 0xe)) {
		switch (address) {
		case SERIAL_LATCH_LSB:
		case SERIAL_LATCH_MSB:;
		}
		return false;
	}

	switch (address) {
	case SERIAL_DATA:
		ht_printf("%c\n", data);
//		gDisplay->printf("%c", data);
		break;
	case SERIAL_INTR:
		state.intr = data;
		return true;
	case SERIAL_INTR_ID:
		state.intr_id = data;
		return true;
	case SERIAL_FORMAT:
		return true;
	case SERIAL_CONTROL_OUT:
		return true;
	case SERIAL_STATE:
		return true;
	case SERIAL_CONTROL_IN:
		return true;
	case SERIAL_SCRATCH:
		state.scratch = data;
		return true;
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
	if (gConfig->getConfigInt(SERIAL_KEY_INSTALLED)) {
		gPCI_Devices->insert(new PCI_Serial());
	}
}

void serial_done()
{
}

void serial_init_config()
{
	gConfig->acceptConfigEntryIntDef(SERIAL_KEY_INSTALLED, 0);
}
