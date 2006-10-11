/*
 *	PearPC
 *	usb.cc
 *
 *	Copyright (C) 2003 Sebastian Biallas (sb@biallas.net)
 *
 *	References:
 *	[1] OpenHCI - Open Host Controller Interface Specification for USB
 *	              Revision 1.0a - hcir1_0a.pdf
 *	[2] Linux USB ohci-driver
 *	    (C) Copyright 1999 Roman Weissgaerber <weissg@vienna.at>
 *	    (C) Copyright 2000-2001 David Brownell <dbrownell@users.sourceforge.net>
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
#include "usb.h"

#include <cstring>

#define NUM_INTS 32		/* part of the OHCI standard */
#define MAX_ROOT_PORTS	15	/* maximum OHCI root hub ports */

struct ohci_hcca {
	uint32	int_table[NUM_INTS];	/* Interrupt ED table */
	uint16	frame_no;		/* current frame number */
	uint16	pad1;			/* set to 0 on each frame_no change */
	uint32	done_head;		/* info returned for an interrupt */
	uint8	reserved_for_hc[116];
} PACKED;

// [1].122
#define OHCI_REG_REVISION	0x00	// [1].123
#define OHCI_REG_CONTROL	0x04	// [1].123
#define OHCI_REG_CMDSTATUS	0x08	// [1].126
#define OHCI_REG_INTRSTATUS	0x0c	// [1].126
#define OHCI_REG_INTRENABLE	0x10	// [1].126
#define OHCI_REG_INTRDISABLE	0x14	// [1].126
#define OHCI_REG_HCCA		0x18	// [1].126
#define OHCI_REG_ED_PERIODCUR	0x1c	// [1].126
#define OHCI_REG_ED_CONTROL_HD	0x20	// [1].126
#define OHCI_REG_ED_CONTROL_CUR	0x24	// [1].126
#define OHCI_REG_ED_BULK_HD	0x28	// [1].126
#define OHCI_REG_ED_BULK_CUR	0x2c	// [1].126
#define OHCI_REG_DONEHEAD	0x30	// [1].126
#define OHCI_REG_FMINTERVAL	0x34	// [1].126
#define OHCI_REG_FMREMAIN	0x38	// [1].126
#define OHCI_REG_FMNUMBER	0x3c	// [1].126
#define OHCI_REG_PERIODICSTART	0x40	// [1].126
#define OHCI_REG_LSTHRESH	0x44	// [1].126
#define OHCI_REG_ROOTHUB_A	0x48
#define OHCI_REG_ROOTHUB_B	0x4c
#define OHCI_REG_ROOTHUB_STAT	0x50
#define OHCI_REG_ROOTHUB_PORTS	0x54

/*
 *	bits in ohci_hcregs.control
 */
#define OHCI_CTRL_CBSR	(3 << 0)	/* control/bulk service ratio */
#define OHCI_CTRL_PLE	(1 << 2)	/* periodic list enable */
#define OHCI_CTRL_IE	(1 << 3)	/* isochronous enable */
#define OHCI_CTRL_CLE	(1 << 4)	/* control list enable */
#define OHCI_CTRL_BLE	(1 << 5)	/* bulk list enable */
#define OHCI_CTRL_HCFS	(3 << 6)	/* host controller functional state */
#define OHCI_CTRL_IR	(1 << 8)	/* interrupt routing */
#define OHCI_CTRL_RWC	(1 << 9)	/* remote wakeup connected */
#define OHCI_CTRL_RWE	(1 << 10)	/* remote wakeup enable */

/* pre-shifted values for HCFS */
#	define OHCI_USB_RESET	(0 << 6)
#	define OHCI_USB_RESUME	(1 << 6)
#	define OHCI_USB_OPER	(2 << 6)
#	define OHCI_USB_SUSPEND	(3 << 6)

/*
 *	bits in ohci_hcregs.{intrstatus|intrenable|intrdisable}
 */
#define OHCI_INTR_SO	(1 << 0)	/* scheduling overrun */
#define OHCI_INTR_WDH	(1 << 1)	/* writeback of done_head */
#define OHCI_INTR_SF	(1 << 2)	/* start frame */
#define OHCI_INTR_RD	(1 << 3)	/* resume detect */
#define OHCI_INTR_UE	(1 << 4)	/* unrecoverable error */
#define OHCI_INTR_FNO	(1 << 5)	/* frame number overflow */
#define OHCI_INTR_RHSC	(1 << 6)	/* root hub status change */
#define OHCI_INTR_OC	(1 << 30)	/* ownership change */
#define OHCI_INTR_MIE	(1 << 31)	/* master interrupt enable */

/*
 *	bits in ohci_hcregs.cmdstatus
 */
#define OHCI_HCR	(1 << 0)	/* host controller reset */
#define OHCI_CLF  	(1 << 1)	/* control list filled */
#define OHCI_BLF  	(1 << 2)	/* bulk list filled */
#define OHCI_OCR  	(1 << 3)	/* ownership change request */
#define OHCI_SOC  	(3 << 16)	/* scheduling overrun count */

/*
 *	bits in ohci_hcregs.roothub.portstatus [1].142
 */
#define OHCI_RH_PS_CCS	(1 << 0)	/* current connect status */
#define OHCI_RH_PS_PES	(1 << 1)	/* port enable status */
#define OHCI_RH_PS_PSS	(1 << 2)	/* port suspend status */
#define OHCI_RH_PS_POCI	(1 << 3)	/* port overrun current indicator */
#define OHCI_RH_PS_PRS	(1 << 4)	/* port reset status */
#define OHCI_RH_PS_PPS	(1 << 8)	/* port power status */
#define OHCI_RH_PS_LSDA	(1 << 8)	/* low speed device attached */
#define OHCI_RH_PS_CSC	(1 << 16)	/* connect status change */
#define OHCI_RH_PS_PESC	(1 << 17)	/* port enable status change */
#define OHCI_RH_PS_PSSC	(1 << 18)	/* port suspend status change */
#define OHCI_RH_PS_OCIC	(1 << 19)	/* overrun current indicator change */
#define OHCI_RH_PS_PRSC	(1 << 20)	/* port reset status change */

struct ohci_hcregs {
	/* control and status registers */
	uint32	control;
	uint32	cmdstatus;
	uint32	intrstatus;
	uint32	intrenable;
	uint32	intrdisable;
	/* memory pointers */
	uint32	hcca;
	uint32	ed_periodcurrent;
	uint32	ed_controlhead;
	uint32	ed_controlcurrent;
	uint32	ed_bulkhead;
	uint32	ed_bulkcurrent;
	uint32	donehead;
	/* frame counters */
	uint32	fminterval;
	uint32	fmremaining;
	uint32	fmnumber;
	uint32	periodicstart;
	uint32	lsthresh;
	/* Root hub ports */
	struct	ohci_roothub_regs {
		uint32	a;
		uint32	b;
		uint32	status;
		uint32	portstatus[MAX_ROOT_PORTS];
	} roothub;
};
 
static inline const char *hc_regname(uint32 a)
{
	a >>= 2;
	if (a > 20) return "unknown";
	char *names[] = {"revision","control","cmdstatus","intrstatus","intrenable",
	"intrdisable","hcca","ed_periodcurrent","ed_controlhead","ed_controlcurrent",
	"ed_bulkhead","ed_bulkcurrent","donehead","fminterval","fmremaining",
	"fmnumber","periodicstart", "lsthresh", "roothub.a", "roothub.b", "roothub.status"};
	return names[a];
}

extern bool gSinglestep;

/*
 *
 */
class PCI_USB: public PCI_Device {
public:
	ohci_hcregs hcregs;
	uint rootport_count;

PCI_USB()
	:PCI_Device("pci-usb", 0x01, 0x06)
{
	mIORegSize[0] = 0x1000;
	mIORegType[0] = PCI_ADDRESS_SPACE_MEM;

	mConfig[0x00] = 0x45;	// vendor ID
	mConfig[0x01] = 0x10;
	mConfig[0x02] = 0x61;	// unit ID
	mConfig[0x03] = 0xc8;
	
	mConfig[0x08] = 0x10;	// revision
	mConfig[0x09] = 0x10; 	// 
	mConfig[0x0a] = 0x03;	// 
	mConfig[0x0b] = 0x0c;	// 

	mConfig[0x0e] = 0x00;	// header-type
	
	assignMemAddress(0, 0x80881000);
	
	mConfig[0x3c] = 0x03;
	mConfig[0x3d] = 0x03;
	mConfig[0x3e] = 0x03;
	mConfig[0x3f] = 0x03;
	
	rootport_count = 1;
	reset();
}

void	reset()
{
	memset(&hcregs, 0, sizeof hcregs);
	hcregs.fminterval = 0x2edf;	// [1].134
	hcregs.lsthresh = 0x628;	// [1].137
	hcregs.roothub.a = 0		// [1].138
		| (1<<12) 		// No overcurrent protection supported
		| (0<<10)		// always 0
		| (1<<9)		// Ports are always powered on when the HC is powered on
		| (0<<8)		// all ports are powered at the same time.
		| rootport_count;	// number of rootports
//	hcregs.roothub.portstatus[0] = ;
}

virtual bool readDeviceMem(uint r, uint32 address, uint32 &data, uint size)
{
	if (r != 0) return false;
	if (size != 4) return false;
	IO_USB_TRACE("read(r=%d, a=%08x (%s), %d)\n", r, address, hc_regname(address), size);
	
	switch (address) {
	case OHCI_REG_REVISION:
		// [1].123
		data = 0x10;
		break;
	case OHCI_REG_CONTROL:
		data = hcregs.control;
		break;
	case OHCI_REG_CMDSTATUS:
		data = hcregs.cmdstatus;
		break;
	case OHCI_REG_INTRSTATUS:
		data = hcregs.intrstatus;
		break;
	case OHCI_REG_INTRENABLE:
		data = hcregs.intrenable;
		break;
	case OHCI_REG_INTRDISABLE:
		data = hcregs.intrdisable;
		break;
	case OHCI_REG_HCCA:
		data = hcregs.hcca;
		break;
	case OHCI_REG_ED_PERIODCUR:
		data = hcregs.ed_periodcurrent;
		break;
	case OHCI_REG_ED_CONTROL_HD:
		data = hcregs.ed_controlhead;
		break;
	case OHCI_REG_ED_CONTROL_CUR:
		data = hcregs.ed_controlcurrent;
		break;
	case OHCI_REG_ED_BULK_HD:
		data = hcregs.ed_bulkhead;
		break;
	case OHCI_REG_ED_BULK_CUR:
		data = hcregs.ed_bulkcurrent;
		break;
	case OHCI_REG_DONEHEAD:
		data = hcregs.donehead;
		break;
	case OHCI_REG_FMINTERVAL:
		data = hcregs.fminterval;
		break;
	case OHCI_REG_FMREMAIN:
		data = hcregs.fmremaining;
		break;
	case OHCI_REG_FMNUMBER:
		data = hcregs.fmnumber;
		break;
	case OHCI_REG_PERIODICSTART:
		data = hcregs.periodicstart;
		break;
	case OHCI_REG_LSTHRESH:
		data = hcregs.lsthresh;
		break;
	case OHCI_REG_ROOTHUB_A:
		data = hcregs.roothub.a;
		break;
	case OHCI_REG_ROOTHUB_B:
		data = hcregs.roothub.b;
		break;
	case OHCI_REG_ROOTHUB_STAT:
		data = hcregs.roothub.status;
		break;
	default:
		address -= OHCI_REG_ROOTHUB_PORTS;
		address >>= 2;
		if (address < rootport_count) {
			data = hcregs.roothub.portstatus[address];
			break;
		}
		return false;
	}
	
//	gSinglestep = true;
	return true;
}

virtual bool writeDeviceMem(uint r, uint32 address, uint32 data, uint size)
{
	if (r != 0) return false;
	if (size != 4) return false;
	IO_USB_TRACE("write(r=%d, a=%08x (%s), data=%08x, %d)\n", r, address, hc_regname(address), data, size);

	switch (address) {
	case OHCI_REG_REVISION:
		// [1].123
		IO_USB_WARN("revision is read only.\n");
		return true;
	case OHCI_REG_CONTROL:
		hcregs.control = data;
		break;
	case OHCI_REG_CMDSTATUS:
		if (data & OHCI_HCR) {
			reset();
			return true;
		}
		hcregs.cmdstatus = data;
		break;
	case OHCI_REG_INTRSTATUS:
		hcregs.intrstatus = data;
		break;
	case OHCI_REG_INTRENABLE:
		hcregs.intrenable = data;
		break;
	case OHCI_REG_INTRDISABLE:
		hcregs.intrdisable = data;
		break;
	case OHCI_REG_HCCA:
		hcregs.hcca = data;
		break;
	case OHCI_REG_ED_PERIODCUR:
		hcregs.ed_periodcurrent = data;
		break;
	case OHCI_REG_ED_CONTROL_HD:
		hcregs.ed_controlhead = data;
		break;
	case OHCI_REG_ED_CONTROL_CUR:
		hcregs.ed_controlcurrent = data;
		break;
	case OHCI_REG_ED_BULK_HD:
		hcregs.ed_bulkhead = data;
		break;
	case OHCI_REG_ED_BULK_CUR:
		hcregs.ed_bulkcurrent = data;
		break;
	case OHCI_REG_DONEHEAD:
		hcregs.donehead = data;
		break;
	case OHCI_REG_FMINTERVAL:
		hcregs.fminterval = data;
		break;
	case OHCI_REG_FMREMAIN:
		hcregs.fmremaining = data;
		break;
	case OHCI_REG_FMNUMBER:
		hcregs.fmnumber = data;
		break;
	case OHCI_REG_PERIODICSTART:
		hcregs.periodicstart = data;
		break;
	case OHCI_REG_LSTHRESH:
		hcregs.lsthresh = data;
		break;
	case OHCI_REG_ROOTHUB_A:
		hcregs.roothub.a = data;
		break;
	case OHCI_REG_ROOTHUB_B:
		hcregs.roothub.b = data;
		break;
	case OHCI_REG_ROOTHUB_STAT:
		hcregs.roothub.status = data;
		break;
	default:
		address -= OHCI_REG_ROOTHUB_PORTS;
		address >>= 2;
		if (address < rootport_count) {
			if (data & OHCI_RH_PS_CCS) {
				// writing 1 to CCS clears PES
				hcregs.roothub.portstatus[address] &= ~OHCI_RH_PS_PES;
			}
			
			// writing 1 to these bits clears them
			hcregs.roothub.portstatus[address] &= ~(data & 
				(OHCI_RH_PS_CSC | OHCI_RH_PS_PESC | OHCI_RH_PS_PSSC 
				| OHCI_RH_PS_OCIC | OHCI_RH_PS_PRSC));

			// writing 1 to these bits set them only if CCS is set
			uint32 p = data & OHCI_RH_PS_PES | OHCI_RH_PS_PSS | OHCI_RH_PS_PRS;
			if (p) {
				if (hcregs.roothub.portstatus[address] & OHCI_RH_PS_CCS) {
					hcregs.roothub.portstatus[address] |= p;
				} else {
					// attempt to enable/suspend/reset disconnected port
					hcregs.roothub.portstatus[address] |= OHCI_RH_PS_CSC;
				}
			}

			// we dont support power switching
			hcregs.roothub.portstatus[address] |= OHCI_RH_PS_PPS;
			break;
		}
	}

//	gSinglestep = true;
	return true;
}

};


#include "configparser.h"

#define USB_KEY_INSTALLED	"pci_usb_installed"

void usb_init()
{
	if (gConfig->getConfigInt(USB_KEY_INSTALLED)) {
		gPCI_Devices->insert(new PCI_USB());
	}
}

void usb_done()
{
}

void usb_init_config()
{
	gConfig->acceptConfigEntryIntDef(USB_KEY_INSTALLED, 0);
}
