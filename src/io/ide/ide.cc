/*
 *	PearPC
 *	ide.cc
 *
 *	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
 *	Some ideas from harddrv.cc from Bochs (http://bochs.sf.net)
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

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include "tools/data.h"
#include "tools/snprintf.h"
#include "system/arch/sysendian.h"
#include "cpu/cpu.h"
#include "cpu/debug.h"
#include "cpu/mem.h"
#include "io/pic/pic.h"
#include "io/pci/pci.h"
#include "debug/tracers.h"
#include "ide.h"
#include "ata.h"
#include "cd.h"

#define IDE_ADDRESS_ISA_BASE	0x1f0
#define IDE_ADDRESS_ISA_BASE2	0x354

#define IDE_ADDRESS_DATA	0x0	// R/W
#define IDE_ADDRESS_ERROR	0x1	// R
#define IDE_ADDRESS_FEATURE	0x1	// W
#define IDE_ADDRESS_SEC_CNT	0x2	// R/W ATA
#define IDE_ADDRESS_INTR_REASON	0x2	// R/W ATAPI
#define IDE_ADDRESS_SEC_NO	0x3	// R/W
#define IDE_ADDRESS_CYL_LSB	0x4	// R/W
#define IDE_ADDRESS_CYL_MSB	0x5	// R/W
#define IDE_ADDRESS_DRV_HEAD	0x6	// R/W
#define IDE_ADDRESS_STATUS	0x7	// R
#define IDE_ADDRESS_COMMAND	0x7	// W

#define IDE_ADDRESS_STATUS2	0x12	// R
#define IDE_ADDRESS_OUTPUT	0x12	// W
#define IDE_ADDRESS_DEV_ADR	0x13	// R

#define IDE_DRIVE_HEAD_LBA	(1<<6)
#define IDE_DRIVE_HEAD_SLAVE	(1<<4)
#define IDE_DRIVE_HEAD(v)	((v) & 0xf)

#define IDE_OUTPUT_RESET	4
#define IDE_OUTPUT_INT		2

#define IDE_STATUS_BSY		(1<<7)
#define IDE_STATUS_RDY		(1<<6)
#define IDE_STATUS_WFT		(1<<5)
#define IDE_STATUS_SKC		(1<<4)
#define IDE_STATUS_DRQ		(1<<3)
#define IDE_STATUS_CORR		(1<<2)
#define IDE_STATUS_IDX		(1<<1)
#define IDE_STATUS_ERR		(1<<0)

#define IDE_CONFIG_ATAPI	(1<<15)
#define IDE_CONFIG_HD		(1<<6)

#define IDE_COMMAND_RESET_ATAPI		0x08
#define IDE_COMMAND_RECALIBRATE		0x10
#define IDE_COMMAND_READ_SECTOR		0x20
#define IDE_COMMAND_WRITE_SECTOR	0x30
#define IDE_COMMAND_FIX_PARAM		0x91
#define IDE_COMMAND_IDENT_ATAPI		0xa1
#define IDE_COMMAND_PACKET		0xa0
#define IDE_COMMAND_SET_MULTIPLE	0xc6
#define IDE_COMMAND_READ_SECTOR_DMA	0xc8
#define IDE_COMMAND_WRITE_SECTOR_DMA	0xca
#define IDE_COMMAND_STANDBY_IMMEDIATE	0xe0
#define IDE_COMMAND_SLEEP		0xe6
#define IDE_COMMAND_FLUSH_CACHE		0xe7
#define IDE_COMMAND_IDENT		0xec
#define IDE_COMMAND_SET_FEATURE		0xef
#define IDE_COMMAND_READ_NATIVE_MAX	0xf8

#define IDE_COMMAND_FEATURE_ENABLE_WRITE_CACHE	0x02
#define IDE_COMMAND_FEATURE_SET_TRANSFER_MODE	0x03
#define IDE_COMMAND_FEATURE_ENABLE_APM		0x05
#define IDE_COMMAND_FEATURE_SET_PIO_MODE	0x08
#define IDE_COMMAND_FEATURE_DISABLE_WRITE_CACHE	0x82
#define IDE_COMMAND_FEATURE_ENABLE_LOOKAHEAD	0xaa
#define IDE_COMMAND_FEATURE_DISABLE_LOOKAHEAD	0x55
#define IDE_COMMAND_FEATURE_ENABLE_PW_DEFAULT	0xcc
#define IDE_COMMAND_FEATURE_DISABLE_PW_DEFAULT	0x66

// .259
// 6 Byte CDB
#define IDE_ATAPI_COMMAND_TEST_READY	0x00 // .499
#define IDE_ATAPI_COMMAND_REQ_SENSE	0x03 // .450
#define IDE_ATAPI_COMMAND_FORMAT_UNIT	0x04 // .271
#define IDE_ATAPI_COMMAND_INQUIRY	0x12 // .310
#define IDE_ATAPI_COMMAND_MODE_SELECT6	0x15
#define IDE_ATAPI_COMMAND_MODE_SENSE6	0x1a
#define IDE_ATAPI_COMMAND_START_STOP	0x1b // .493
#define IDE_ATAPI_COMMAND_TOGGLE_LOCK	0x1e // .335

// 10 Byte CDB
#define IDE_ATAPI_COMMAND_READ_FMT_CAP	0x23 // .400
#define IDE_ATAPI_COMMAND_READ_CAPACITY	0x25 // .349
#define IDE_ATAPI_COMMAND_READ10	0x28 // .337
#define IDE_ATAPI_COMMAND_SEEK10	0x2b // .460
#define IDE_ATAPI_COMMAND_ERASE10	0x2c // .269
#define IDE_ATAPI_COMMAND_WRITE10	0x2a // .502
#define IDE_ATAPI_COMMAND_VER_WRITE10	0x2e // .510
#define IDE_ATAPI_COMMAND_VERIFY10	0x2f // .500
#define IDE_ATAPI_COMMAND_SYNC_CACHE	0x35 // .497
#define IDE_ATAPI_COMMAND_WRITE_BUF	0x3b // .512
#define IDE_ATAPI_COMMAND_READ_BUF	0x3c // .342
#define IDE_ATAPI_COMMAND_READ_SUBCH	0x42 // .406
#define IDE_ATAPI_COMMAND_READ_TOC	0x43 // .413
#define IDE_ATAPI_COMMAND_READ_HEADER	0x44
#define IDE_ATAPI_COMMAND_PLAY_AUDIO10	0x45 // .329
#define IDE_ATAPI_COMMAND_GET_CONFIG	0x46 // .284
#define IDE_ATAPI_COMMAND_PLAY_AUDIOMSF	0x47 // .333
#define IDE_ATAPI_COMMAND_EVENT_INFO	0x4a // .287
#define IDE_ATAPI_COMMAND_TOGGLE_PAUSE	0x4b // .327
#define IDE_ATAPI_COMMAND_STOP		0x4e // .495
#define IDE_ATAPI_COMMAND_READ_INFO	0x51 // .362
#define IDE_ATAPI_COMMAND_READ_TRK_INFO	0x52 // .423
#define IDE_ATAPI_COMMAND_RES_TRACK	0x53 // .455
#define IDE_ATAPI_COMMAND_SEND_OPC	0x54 // .482
#define IDE_ATAPI_COMMAND_MODE_SELECT10	0x55 // .322
#define IDE_ATAPI_COMMAND_REPAIR_TRACK	0x58 // .441
#define IDE_ATAPI_COMMAND_MODE_SENSE10	0x5a // .324
#define IDE_ATAPI_COMMAND_CLOSE_TRACK	0x5b // .264
#define IDE_ATAPI_COMMAND_READ_BUF_CAP	0x5c // .348

// 12 Byte CDB
#define IDE_ATAPI_COMMAND_BLANK		0xa1 // .260
#define IDE_ATAPI_COMMAND_SEND_KEY	0xa3 // .478
#define IDE_ATAPI_COMMAND_REPORT_KEY	0xa4 // .443
#define IDE_ATAPI_COMMAND_PLAY_AUDIO12	0xa5 // .331
#define IDE_ATAPI_COMMAND_LOAD_CD	0xa6 // .315
#define IDE_ATAPI_COMMAND_SET_RD_AHEAD	0xa7 // .486
#define IDE_ATAPI_COMMAND_READ12	0xa8 // .339
#define IDE_ATAPI_COMMAND_WRITE12	0xaa // .507
#define IDE_ATAPI_COMMAND_GET_PERF	0xac // .299
#define IDE_ATAPI_COMMAND_READ_DVD_S	0xad // .368
#define IDE_ATAPI_COMMAND_SET_STREAM	0xb6 // .488
#define IDE_ATAPI_COMMAND_READ_CD_MSF	0xb9 // .360
#define IDE_ATAPI_COMMAND_SCAN		0xba // .458
#define IDE_ATAPI_COMMAND_SET_CD_SPEED	0xbb // .484
#define IDE_ATAPI_COMMAND_PLAY_CD	0xbc
#define IDE_ATAPI_COMMAND_MECH_STATUS	0xbd // .317
#define IDE_ATAPI_COMMAND_READ_CD	0xbe // .352
#define IDE_ATAPI_COMMAND_SEND_DVD_S	0xbf // .470

enum {
	IDE_TRANSFER_MODE_NONE,
	IDE_TRANSFER_MODE_READ,
	IDE_TRANSFER_MODE_WRITE,
	IDE_TRANSFER_MODE_DMA,
};

#define IDE_ATAPI_PACKET_SIZE	12

#define IDE_ATAPI_SENSE_NONE		0
#define IDE_ATAPI_SENSE_NOT_READY	2
#define IDE_ATAPI_SENSE_ILLEGAL_REQUEST	5
#define IDE_ATAPI_SENSE_UNIT_ATTENTION	6

#define IDE_ATAPI_ASC_INV_FIELD_IN_CMD_PACKET		0x24
#define IDE_ATAPI_ASC_MEDIUM_NOT_PRESENT		0x3a
#define IDE_ATAPI_ASC_SAVING_PARAMETERS_NOT_SUPPORTED	0x39
#define IDE_ATAPI_ASC_LOGICAL_BLOCK_OOR			0x21

#define IDE_ATAPI_INTR_REASON_C_D		(1<<0)
#define IDE_ATAPI_INTR_REASON_I_O		(1<<1)
#define IDE_ATAPI_INTR_REASON_REL		(1<<2)
#define IDE_ATAPI_INTR_REASON_TAG(v)		((v)>>3)
#define IDE_ATAPI_INTR_REASON_SET_TAG(v, a)	(((v)&7) | ((a)<<3))

#define IDE_ATAPI_TRANSFER_HDR_SYNC 0x80
#define IDE_ATAPI_TRANSFER_HDR_SECTOR_SUB 0x40
#define IDE_ATAPI_TRANSFER_HDR_SECTOR 0x20
#define IDE_ATAPI_TRANSFER_DATA 0x10
#define IDE_ATAPI_TRANSFER_ECC 0x08

struct IDEDriveState {
	uint8 status;
	uint8 head;
	uint8 outreg;
	uint8 error;
	uint8 feature;
	union {
		uint8 sector_count;
		uint8 intr_reason;
	};
	uint8 sector_no;
	union {
		uint16 cyl;
		uint16 byte_count;
	};
	
	int sectorpos;
	int drqpos;
	uint8 sector[2352];
	int current_sector_size;
	int current_command;
	uint32 dma_lba_start;
	uint32 dma_lba_count;

	int mode;
	int atapi_transfer_request;
};

struct IDEState {
	int drive;
	uint8 drive_head;
	IDEConfig config[2];
	IDEDriveState state[2];

	bool one_time_shit;
};

IDEState gIDEState;

/*******************************************************************************
 *	IDE - Controller PCI
 */

#define IDE_PCI_REG_0_CMD	0
#define IDE_PCI_REG_0_CTRL	1
#define IDE_PCI_REG_1_CMD	2
#define IDE_PCI_REG_1_CTRL	3
#define IDE_PCI_REG_BMDMA	4

/*
 * CMD64x specific registers definition.
 */
#define CFR		0x50
#define   CFR_INTR_CH0		0x02
#define CNTRL		0x51
#define	  CNTRL_DIS_RA0		0x40
#define   CNTRL_DIS_RA1		0x80
#define	  CNTRL_ENA_2ND		0x08

#define	CMDTIM		0x52
#define	ARTTIM0		0x53
#define	DRWTIM0		0x54
#define ARTTIM1 	0x55
#define DRWTIM1		0x56
#define ARTTIM23	0x57
#define   ARTTIM23_DIS_RA2	0x04
#define   ARTTIM23_DIS_RA3	0x08
#define   ARTTIM23_INTR_CH1	0x10
#define ARTTIM2		0x57
#define ARTTIM3		0x57
#define DRWTIM23	0x58
#define DRWTIM2		0x58
#define BRST		0x59
#define DRWTIM3		0x5b

#define BMIDECR0	0x70
#define MRDMODE		0x71
#define   MRDMODE_INTR_CH0	0x04
#define   MRDMODE_INTR_CH1	0x08
#define   MRDMODE_BLK_CH0	0x10
#define   MRDMODE_BLK_CH1	0x20
#define BMIDESR0	0x72
#define UDIDETCR0	0x73
#define DTPR0		0x74
#define BMIDECR1	0x78
#define BMIDECSR	0x79
#define BMIDESR1	0x7A
#define UDIDETCR1	0x7B
#define DTPR1		0x7C

/*
 *	Bus master IDE consts
 */

// bus master IDE command register (CR)
#define	BM_IDE_CR_WRITE		(1<<3)	// read otherwise
#define	BM_IDE_CR_START		(1<<0)	// stop otherwise
#define	BM_IDE_CR_MASK		(BM_IDE_CR_WRITE | BM_IDE_CR_START)

// bus master IDE status register (SR)
#define	BM_IDE_SR_SIMPLEX_ONLY	(1<<7)	// duplex otherwise
#define	BM_IDE_SR_DMA1_CAPABLE	(1<<6)
#define	BM_IDE_SR_DMA0_CAPABLE	(1<<5)
#define	BM_IDE_SR_INTERRUPT	(1<<2)
#define	BM_IDE_SR_ERROR		(1<<1)
#define	BM_IDE_SR_ACTIVE	(1<<0)
#define	BM_IDE_SR_MASK		(BM_IDE_SR_ERROR | BM_IDE_SR_INTERRUPT \
	| BM_IDE_SR_DMA0_CAPABLE | BM_IDE_SR_DMA1_CAPABLE | BM_IDE_SR_SIMPLEX_ONLY \
	| BM_IDE_SR_ACTIVE)

class IDE_Controller: public PCI_Device {
public:

	IDE_Controller()
	    :PCI_Device("IDE-Controller", 0x01, 0x01)
	{
		mIORegSize[IDE_PCI_REG_0_CMD] = 0x10;
		mIORegSize[IDE_PCI_REG_0_CTRL] = 0x10;
		mIORegSize[IDE_PCI_REG_1_CMD] = 0;		// no secondary controller for now
		mIORegSize[IDE_PCI_REG_1_CTRL] = 0;
		mIORegSize[IDE_PCI_REG_BMDMA] = 0x10;

		mIORegType[IDE_PCI_REG_0_CMD] = PCI_ADDRESS_SPACE_IO;
		mIORegType[IDE_PCI_REG_0_CTRL] = PCI_ADDRESS_SPACE_IO;
		mIORegType[IDE_PCI_REG_BMDMA] = PCI_ADDRESS_SPACE_IO;

		mConfig[0x00] = 0x95;	// vendor ID
		mConfig[0x01] = 0x10;
		mConfig[0x02] = 0x46;	// unit ID
		mConfig[0x03] = 0x06;

		mConfig[0x04] = 0x01;	// command
		mConfig[0x05] = 0x00;	
		mConfig[0x06] = 0x00; 	// status
		mConfig[0x07] = 0x00;

		mConfig[0x08] = 0x07;	// ide-controller revision
		mConfig[0x09] = 0x8f;	// bit7: bm-ide
		mConfig[0x0a] = 0x01;	// ide-controller 
		mConfig[0x0b] = 0x01;	// controller for mass-storage

		mConfig[0x0e] = 0x00;	// header-type

		assignIOPort(IDE_PCI_REG_0_CMD, 0x0001c40);
		assignIOPort(IDE_PCI_REG_0_CTRL, 0x0001c30);
		assignIOPort(IDE_PCI_REG_BMDMA, 0x0001c00);

		mConfig[0x3c] = 0x1a;	// irq
		mConfig[0x3d] = 1;
		mConfig[0x3e] = 0x02;	// min grand
		mConfig[0x3f] = 0x04;	// max latency
	};
	
/*******************************************************************************
 *	IDE - Controller Core
 */

	/*
	 *	makeLogical return a maximum of 0x10000000, so it's safe
	 *	to use uint32 here.
	 */
	uint32 makeLogical(int head, int cyl, int sec_no)
	{
		if (gIDEState.config[gIDEState.drive].lba) {
			return (head << 24) | (cyl << 8) | sec_no;
		} else {
			return cyl * gIDEState.config[gIDEState.drive].hd.heads * gIDEState.config[gIDEState.drive].hd.spt
			+ head * gIDEState.config[gIDEState.drive].hd.spt
        		+ sec_no - 1;
		}
	};

	void incAddress()
	{
		gIDEState.state[gIDEState.drive].sector_count--;

		if (gIDEState.config[gIDEState.drive].lba) {
			uint32 cur = makeLogical(gIDEState.state[gIDEState.drive].head, 
				gIDEState.state[gIDEState.drive].cyl, 
				gIDEState.state[gIDEState.drive].sector_no);
			cur++;
			gIDEState.state[gIDEState.drive].head      = (cur>>24) & 0xf;
			gIDEState.state[gIDEState.drive].cyl       = cur>>8;
			gIDEState.state[gIDEState.drive].sector_no = cur & 0xff;
		} else {
			gIDEState.state[gIDEState.drive].sector_no++;
			if (gIDEState.state[gIDEState.drive].sector_no > gIDEState.config[gIDEState.drive].hd.spt) {
				gIDEState.state[gIDEState.drive].sector_no = 1;
				gIDEState.state[gIDEState.drive].head++;
				if (gIDEState.state[gIDEState.drive].head >= gIDEState.config[gIDEState.drive].hd.heads) {
					gIDEState.state[gIDEState.drive].head = 0;
					gIDEState.state[gIDEState.drive].cyl++;
				}
			}
		}
	};
	
	void raiseInterrupt(int bus)
	{
		IO_IDE_TRACE("MRDMODE: %02x\n", mConfig[MRDMODE]);
		if (!(gIDEState.state[gIDEState.drive].outreg & IDE_OUTPUT_INT) && !(mConfig[MRDMODE] & MRDMODE_BLK_CH0)) {
			pic_raise_interrupt(mConfig[0x3c]);
		}
		mConfig[MRDMODE] |= MRDMODE_INTR_CH0 << bus;
	};
	
	void cancelInterrupt(int bus)
	{
		pic_cancel_interrupt(IO_PIC_IRQ_IDE0);
		mConfig[MRDMODE] &= ~(MRDMODE_INTR_CH0 << bus);
	};
	
void drive_ident()
{
#define AW(a, b) (((a)<<8)|(b))
	uint16 id[256];
	if (gIDEState.config[gIDEState.drive].installed) {
		gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_DRQ | IDE_STATUS_SKC;
	} else {
		gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
		gIDEState.state[gIDEState.drive].error = 0x4; // abort command
		return;
	}
	memset(&id, 0, sizeof id);
	if (gIDEState.config[gIDEState.drive].protocol == IDE_ATA) {
//		id[0] = IDE_CONFIG_HD;
		id[0] = 0x0c5a;
		id[1] = gIDEState.config[gIDEState.drive].hd.cyl;
		id[3] = gIDEState.config[gIDEState.drive].hd.heads;
		id[4] = gIDEState.config[gIDEState.drive].hd.spt*gIDEState.config[gIDEState.drive].bps;
		id[5] = gIDEState.config[gIDEState.drive].bps;
		id[6] = gIDEState.config[gIDEState.drive].hd.spt;
	} else {
//		id[0] = IDE_CONFIG_ATAPI | (5 << 8) | (1 << 7) | (1 << 6) | (0 << 0);
		id[0] = 0x85c0;
	}

	id[10] = AW(' ',' ');
	id[11] = AW(' ',' ');
	id[12] = AW(' ',' ');
	id[13] = AW(' ',' ');
	id[14] = AW(' ',' ');
	id[15] = AW('F','o');
	id[16] = AW('u','n');
	id[17] = AW('d','a');
	id[18] = AW('t','i');
	id[19] = AW('o','n');
		
	if (gIDEState.config[gIDEState.drive].protocol == IDE_ATA) {
		id[20] = 3;	//buffer type
		id[21] = 512;	//buffer size / 512
		id[22] = 4;	// ECC-Bytes
	} else {
		id[20] = 3;	//buffer type
		id[21] = 0x100;	//buffer size / 512
		id[22] = 0;	// ECC-Bytes
	}
	id[23] = AW('F','I');
	id[24] = AW('R','M');
	id[25] = AW('W','A');
	id[26] = AW('R','E');
	if (gIDEState.drive==0) {	
		id[27] = AW('E','I');
		id[28] = AW('N',' ');
		id[29] = AW('G','E');
		id[30] = AW('B','U');
		id[31] = AW('E','S');
		id[32] = AW('C','H');
		id[33] = AW('!',' ');
		id[34] = AW(' ',' ');
	} else {
		id[27] = AW('Z','W');
		id[28] = AW('E','I');
		id[29] = AW(' ','G');
		id[30] = AW('E','B');
		id[31] = AW('U','E');
		id[32] = AW('S','C');
		id[33] = AW('H','!');
		id[34] = AW(' ',' ');
	}
	id[35] = AW(' ',' ');
	id[36] = AW(' ',' ');
	id[37] = AW(' ',' ');
	id[38] = AW(' ',' ');
	id[39] = AW(' ',' ');
	id[40] = AW(' ',' ');
	id[41] = AW(' ',' ');
	id[42] = AW(' ',' ');
	id[43] = AW(' ',' ');
	id[44] = AW(' ',' ');
	id[45] = AW(' ',' ');
	id[46] = AW(' ',' ');

	if (gIDEState.config[gIDEState.drive].protocol == IDE_ATA) {
		id[47] = 1; // sectors per interrupt
		id[48] = 0; // 32 bit i/o
		id[49] = (1<<9)|(1<<8);  // LBA & DMA
		id[51] = 0x200; // pio time
		id[52] = 0x200; // dma time
// from Linux kernel's hdreg.h:
//	unsigned short	field_valid;	

/* (word 53)
 *  2:	ultra_ok	word  88
 *  1:	eide_ok		words 64-70
 *  0:	cur_ok		words 54-58
 */

// see also: static int config_drive_for_dma (ide_drive_t *drive) in ide-dma.c

		id[53] = 4; // fieldValidity: Multi DMA fields valid

		id[54] = gIDEState.config[gIDEState.drive].hd.cyl;
		id[55] = gIDEState.config[gIDEState.drive].hd.heads;
		id[56] = gIDEState.config[gIDEState.drive].hd.spt;

		uint32 sectors = gIDEState.config[gIDEState.drive].hd.cyl 
			* gIDEState.config[gIDEState.drive].hd.heads 
			* gIDEState.config[gIDEState.drive].hd.spt;
		id[57] = sectors;
		id[58] = sectors >> 16;
		id[59] = 0; // multisector bla
		id[60] = sectors;       // lba capacity
		id[61] = sectors >> 16; // lba capacity cont.
		id[62] = 0;       // obsolete single word dma (linux dma_1word)
		id[63] = 7|0x404; // multiple word dma info   (linux dma_mword)
		id[64] = 1; // eide pio modes
		id[65] = 480; // eide min dma cycle time
		id[66] = 480; // eide recommended dma cycle time
		id[67] = 0;
		id[68] = 0;
		id[69] = 0; // res
		id[70] = 0; // res

		id[80] = (1<<2) | (1<<1);
		id[82] = (1<<14) | (1<<9) | (1<<5) | (1<<3); // command set 1
		id[83] = (1<<14); // command set 2
		id[84] = (1<<14); // set feature extensions
		id[85] = (1<<5); // set feature enabled
		id[86] = (1<<14); // set feature enabled 2
		id[87] = (1<<14); // set feature default
		id[88] = 7; // dma ultra
			    // bit 15 set indicates UDMA(mode 7) capable
			    // bit 14 set indicates UDMA(133) capable
			    // bit 13 set indicates UDMA(100) capable
			    // bit 12 set indicates UDMA(66) capable
			    // bit 11 set indicates UDMA(44) capable
			    // bits 0-2 ???

		id[93] = (1<<14) | 1; // hw config
	} else {
		id[47] = 0; // sectors per interrupt
		id[48] = 1; // 32 bit i/o
		id[49] = (1<<9); // lba
		id[53] = 3;
		id[63] = 0x103;
		id[64] = 0x1;
		id[65] = 0xb4;
		id[66] = 0xb4;
		id[67] = 0x12c;
		id[68] = 0xb4;
		id[71] = 30;
		id[72] = 30;
		id[80] = 0x1e; // supports up to ATA/ATAPI-4		
	}

	gIDEState.state[gIDEState.drive].sectorpos = 0;
	gIDEState.state[gIDEState.drive].sector_count = 0;

	for (int i=0; i<256; i++) {
		gIDEState.state[gIDEState.drive].sector[i*2] = id[i];
		gIDEState.state[gIDEState.drive].sector[i*2+1] = id[i]>>8;
	}
};

	void atapi_command_nop()
	{
		gIDEState.state[gIDEState.drive].intr_reason |= IDE_ATAPI_INTR_REASON_C_D|IDE_ATAPI_INTR_REASON_I_O;
		gIDEState.state[gIDEState.drive].intr_reason &= ~IDE_ATAPI_INTR_REASON_REL;
		gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_SKC;
	};

	void atapi_command_error(uint8 sense_key, uint8 asc)
	{
		memset(&gIDEState.config[gIDEState.drive].cdrom.sense, 0, sizeof gIDEState.config[gIDEState.drive].cdrom.sense);
		gIDEState.state[gIDEState.drive].error = sense_key << 4;
		gIDEState.state[gIDEState.drive].intr_reason |= IDE_ATAPI_INTR_REASON_C_D|IDE_ATAPI_INTR_REASON_I_O;
		gIDEState.state[gIDEState.drive].intr_reason &= ~IDE_ATAPI_INTR_REASON_REL;
		gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
    
		gIDEState.config[gIDEState.drive].cdrom.sense.sense_key = sense_key;
		gIDEState.config[gIDEState.drive].cdrom.sense.asc = asc;
		gIDEState.config[gIDEState.drive].cdrom.sense.ascq = 0;
	};

	void atapi_start_send_command(uint8 command, int reqlen, int alloclen, int sectorpos=0, int sectorsize=2048)
	{
		if (gIDEState.state[gIDEState.drive].byte_count == 0xffff) gIDEState.state[gIDEState.drive].byte_count = 0xfffe;
		if ((gIDEState.state[gIDEState.drive].byte_count & 1) && !(alloclen <= gIDEState.state[gIDEState.drive].byte_count)) {
			gIDEState.state[gIDEState.drive].byte_count--;
		}
		if (!gIDEState.state[gIDEState.drive].byte_count) {
			IO_IDE_ERR("byte_count==0\n");
		}
		if (!alloclen) alloclen = gIDEState.state[gIDEState.drive].byte_count;
		gIDEState.state[gIDEState.drive].intr_reason |= IDE_ATAPI_INTR_REASON_I_O;
		gIDEState.state[gIDEState.drive].intr_reason &= ~IDE_ATAPI_INTR_REASON_C_D;
		gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_DRQ | IDE_STATUS_SKC;
		gIDEState.state[gIDEState.drive].sectorpos = sectorpos;
		gIDEState.state[gIDEState.drive].current_sector_size = sectorsize;
		gIDEState.state[gIDEState.drive].drqpos = 0;
		if (gIDEState.state[gIDEState.drive].byte_count > reqlen) gIDEState.state[gIDEState.drive].byte_count = reqlen;
		if (gIDEState.state[gIDEState.drive].byte_count > alloclen) gIDEState.state[gIDEState.drive].byte_count = alloclen;

		gIDEState.config[gIDEState.drive].cdrom.atapi.command = command;
		gIDEState.config[gIDEState.drive].cdrom.atapi.drq_bytes = gIDEState.state[gIDEState.drive].byte_count;
		gIDEState.config[gIDEState.drive].cdrom.atapi.total_remain = MIN(reqlen, alloclen);
	};

	void atapi_start_mode_sense(const byte *src, int size)
	{
		gIDEState.state[gIDEState.drive].sector[0] = (size-2) >> 8;
		gIDEState.state[gIDEState.drive].sector[1] =  size-2;
		gIDEState.state[gIDEState.drive].sector[2] = 0;
		gIDEState.state[gIDEState.drive].sector[3] = 0;
		gIDEState.state[gIDEState.drive].sector[4] = 0;
		gIDEState.state[gIDEState.drive].sector[5] = 0;
		gIDEState.state[gIDEState.drive].sector[6] = 0;
		gIDEState.state[gIDEState.drive].sector[7] = 0;
	}

	static uint8 bcd_encode(uint8 value)
	{
		return (value / 10)*16+(value % 10);
	}

	/*
	 *	gIDEState.config[gIDEState.drive] must be acquired
	 */
	bool atapi_check_dma()
	{
		if (gIDEState.config[gIDEState.drive].cdrom.dma) {
			gIDEState.state[gIDEState.drive].dma_lba_count = gIDEState.config[gIDEState.drive].cdrom.remain;
			gIDEState.state[gIDEState.drive].dma_lba_start = gIDEState.config[gIDEState.drive].cdrom.next_lba;
			gIDEState.config[gIDEState.drive].device->setMode(
				gIDEState.state[gIDEState.drive].atapi_transfer_request, 
				gIDEState.state[gIDEState.drive].current_sector_size);
			gIDEState.config[gIDEState.drive].device->seek(gIDEState.state[gIDEState.drive].dma_lba_start);
			gIDEState.state[gIDEState.drive].status &= ~IDE_STATUS_DRQ;
			gIDEState.state[gIDEState.drive].intr_reason |= IDE_ATAPI_INTR_REASON_C_D;
			bmide_start_dma(false);
			return true;
		}
		return false;
	}
	
void receive_atapi_packet()
{
	uint8 command = gIDEState.state[gIDEState.drive].sector[0];
	IO_IDE_TRACE("ATAPI command(%02x)\n", command);
	CDROMDevice *dev = (CDROMDevice *)gIDEState.config[gIDEState.drive].device;
	uint8 *sector = gIDEState.state[gIDEState.drive].sector;
	switch (command) {
	case IDE_ATAPI_COMMAND_TEST_READY:
		if (dev->isReady()) {
			atapi_command_nop();
		} else {
			atapi_command_error(IDE_ATAPI_SENSE_NOT_READY, IDE_ATAPI_ASC_MEDIUM_NOT_PRESENT);
		}
		raiseInterrupt(0);
		break;
	case IDE_ATAPI_COMMAND_REQ_SENSE: {
		// .450
		int len = sector[4];
		atapi_start_send_command(command, 18, len);
		sector[0] = 0xf0; // valid + current error info
		sector[1] = 0x00;
		sector[2] = gIDEState.config[gIDEState.drive].cdrom.sense.sense_key;
		sector[3] = gIDEState.config[gIDEState.drive].cdrom.sense.info[0];
		sector[4] = gIDEState.config[gIDEState.drive].cdrom.sense.info[1];
		sector[5] = gIDEState.config[gIDEState.drive].cdrom.sense.info[2];
		sector[6] = gIDEState.config[gIDEState.drive].cdrom.sense.info[3];
		sector[7] = 10;
		sector[8] = gIDEState.config[gIDEState.drive].cdrom.sense.spec_info[0];
		sector[9] = gIDEState.config[gIDEState.drive].cdrom.sense.spec_info[1];
		sector[10] = gIDEState.config[gIDEState.drive].cdrom.sense.spec_info[2];
		sector[11] = gIDEState.config[gIDEState.drive].cdrom.sense.spec_info[3];
		sector[12] = gIDEState.config[gIDEState.drive].cdrom.sense.asc;
		sector[13] = gIDEState.config[gIDEState.drive].cdrom.sense.ascq;
		sector[14] = gIDEState.config[gIDEState.drive].cdrom.sense.fruc;
		sector[15] = gIDEState.config[gIDEState.drive].cdrom.sense.key_spec[0];
		sector[16] = gIDEState.config[gIDEState.drive].cdrom.sense.key_spec[1];
		sector[17] = gIDEState.config[gIDEState.drive].cdrom.sense.key_spec[2];
		raiseInterrupt(0);
		break;
	}
	case IDE_ATAPI_COMMAND_INQUIRY: {
		// .310
		int len = sector[4];
		atapi_start_send_command(command, 36, len);
		memset(sector, 0, sizeof gIDEState.state[gIDEState.drive].sector);
		sector[0] = 0x05; 
		sector[1] = 0x80; // Removable Medium
		sector[2] = 0x00; // ATAPI
		sector[3] = 0x32; // ATAPI / format = 2
		sector[4] = 62-4;
		sector[5] = 0x00;
		sector[6] = 0x00;
		sector[7] = 0x00;

		memcpy(sector+8, "SPIRO   ", 8);
		memcpy(sector+16, "MULTIMAX 3000   ", 16);
		memcpy(sector+32, "0.1 ", 4);
		
		sector[58] = 0x03;
		sector[59] = 0xa0; // MMC-4
		sector[60] = 0x15;
		sector[61] = 0xe0; // ATA/ATAPI-6
		
		raiseInterrupt(0);
		break;
	}
	case IDE_ATAPI_COMMAND_START_STOP: {
		bool eject = sector[4] & 2;
		bool start = sector[4] & 1;
		if (!eject && !start) {
			atapi_command_nop();
		} else if (!eject && start) {
			atapi_command_nop();
		} else if (eject && !start) {
                        dev->eject();
			atapi_command_nop();
		} else {
			dev->eject();
			atapi_command_nop();
		}
		raiseInterrupt(0);
		break;
	}
	case IDE_ATAPI_COMMAND_TOGGLE_LOCK:
		if (dev->isReady()) {
			dev->setLock(sector[4] & 1);
			atapi_command_nop();
		} else {
			atapi_command_error(IDE_ATAPI_SENSE_NOT_READY, IDE_ATAPI_ASC_MEDIUM_NOT_PRESENT);
		}
		raiseInterrupt(0);
		break;
	case IDE_ATAPI_COMMAND_READ_CAPACITY:
		if (dev->isReady()) {
			atapi_start_send_command(command, 8, 8);
			uint32 capacity = dev->getCapacity();
			sector[0] = capacity >> 24;
			sector[1] = capacity >> 16;
			sector[2] = capacity >> 8;
			sector[3] = capacity;
			sector[4] = 2048 >> 24;
			sector[5] = 2048 >> 16;
			sector[6] = 2048 >> 8;
			sector[7] = 2048;
		} else {
			atapi_command_error(IDE_ATAPI_SENSE_NOT_READY, IDE_ATAPI_ASC_MEDIUM_NOT_PRESENT);
		}
		raiseInterrupt(0);
		break;
	case IDE_ATAPI_COMMAND_READ10:
		if (dev->isReady()) {
			uint16 len = ((uint16)sector[7]<<8)|(sector[8]);
			if (!len) {
				atapi_command_nop();
			} else {
				uint32 lba = ((uint32)sector[2]<<24)|((uint32)sector[3]<<16)|((uint32)sector[4]<<8)|sector[5];
				if (lba + len > dev->getCapacity()) {
					atapi_command_error(IDE_ATAPI_SENSE_ILLEGAL_REQUEST, IDE_ATAPI_ASC_LOGICAL_BLOCK_OOR);
				} else {
					IO_IDE_TRACE("read cd: lba: 0x%08x len: %d\n", lba, len);
					gIDEState.state[gIDEState.drive].current_sector_size = 2048;
					gIDEState.state[gIDEState.drive].atapi_transfer_request = 0x10; // only data
					uint secsize = gIDEState.state[gIDEState.drive].current_sector_size;
					atapi_start_send_command(command, len*secsize, len*secsize, secsize, secsize);
					gIDEState.config[gIDEState.drive].cdrom.remain = len;
					gIDEState.config[gIDEState.drive].cdrom.next_lba = lba;
					if (atapi_check_dma()) return;
				}
			}
		} else {
			atapi_command_error(IDE_ATAPI_SENSE_NOT_READY, IDE_ATAPI_ASC_MEDIUM_NOT_PRESENT);
		}
		raiseInterrupt(0);
		break;
	case IDE_ATAPI_COMMAND_SEEK10:
		if (dev->isReady()) {
			uint32 lba = ((uint32)sector[2]<<24)|((uint32)sector[3]<<16)|((uint32)sector[4]<<8)|sector[5];
			if (lba > dev->getCapacity()) {
				atapi_command_error(IDE_ATAPI_SENSE_ILLEGAL_REQUEST, IDE_ATAPI_ASC_LOGICAL_BLOCK_OOR);
			} else {
				atapi_command_nop();				
			}
		} else {
			atapi_command_error(IDE_ATAPI_SENSE_NOT_READY, IDE_ATAPI_ASC_MEDIUM_NOT_PRESENT);
		}
		raiseInterrupt(0);
		break;
	case IDE_ATAPI_COMMAND_READ_SUBCH:
		SINGLESTEP("IDE_ATAPI_COMMAND_READ_SUBCH\n");
		atapi_command_error(IDE_ATAPI_SENSE_ILLEGAL_REQUEST, IDE_ATAPI_ASC_INV_FIELD_IN_CMD_PACKET);
		raiseInterrupt(0);
		break;
	case IDE_ATAPI_COMMAND_READ_TOC:
		// .413
		if (dev->isReady()) {
			bool msf = sector[1] & 2;
			uint8 start_track = sector[6];
			int len = ((uint16)sector[7]<<8)|(sector[8]);
			// first check for ATAPI-SFF 8020 style
			uint8 format = sector[2] & 0xf;
			if (!format) {
				// then for MMC-2 style 
				format = sector[9] >> 6;
			}
			switch (format) {
			case 0: {
				// .415
				// start_track == track
				atapi_start_send_command(command, 12, len);	
				sector[0] = 0;
				sector[1] = 10;
				sector[2] = 1; // first track
				sector[3] = 1; // last track
				sector[4] = 0x00; // res
				sector[5] = 0x16; // .408 (Data track, copy allowed :) )
				sector[6] = 1;    // track number
				sector[7] = 0x00; // res
				if (msf) {
//					SINGLESTEP("");
					MSF m;
					CDROMDevice::MSFfromLBA(m, 0);
					sector[8] = 0x00;
					sector[9] = m.m;
					sector[10] = m.s;
					sector[11] = m.f; 
				} else {
					sector[8] = 0x00; // LBA
					sector[9] = 0x00;
					sector[10] = 0x00;
					sector[11] = 0x00; // LBA 
				}
				break;				
			}
			case 1:
				// .418 Multisession information
				// start_track == 0
				atapi_start_send_command(command, 12, len);
				sector[0] = 0;
				sector[1] = 10;
				sector[2] = 1; // first session
				sector[3] = 1; // last session
				sector[4] = 0x00; // res
				sector[5] = 0x16; // .408 (Data track, copy allowed :) )
				sector[6] = 1;    // first track number in last complete session
				sector[7] = 0x00; // res
				if (msf) {
					MSF m;
					CDROMDevice::MSFfromLBA(m, 0);
					sector[8] = 0x00;
					sector[9] = m.m;
					sector[10] = m.s;
					sector[11] = m.f; 
				} else {
					sector[8] = 0x00; // LBA
					sector[9] = 0x00;
					sector[10] = 0x00;
					sector[11] = 0x00; // LBA 
				}
				break;
			case 2:
				// .420 Raw TOC
				// start_track == session number
				atapi_start_send_command(command, 48, len);
				sector[0] = 0;
				sector[1] = 46;
				sector[2] = 1; // first session
				sector[3] = 1; // last session
				// points a0-af tracks b0-bf				
				sector[4] = 1;    // session number
				sector[5] = 0x16; // .408 (Data track, copy allowed :) )
				sector[6] = 0;    // track number
				sector[7] = 0xa0; // point (lead-in)
				sector[8] = 0x00; // min
				sector[9] = 0x00; // sec
				sector[10] = 0x00; // frame
				sector[11] = 0x00;   // zero
				sector[12] = 0x01;   // first track
				sector[13] = 0x00;   // disk type
				sector[14] = 0x00;   // 				

				sector[15] = 1;    // session number
				sector[16] = 0x16; // .408 (Data track, copy allowed :) )
				sector[17] = 0;    // track number
				sector[18] = 0xa1; // point
				sector[19] = 0x00; // min
				sector[20] = 0x00; // sec
				sector[21] = 0x00; // frame
				sector[22] = 0x00;   // zero
				sector[23] = 0x01;   // last track
				sector[24] = 0x00;   // 
				sector[25] = 0x00;   // 

				sector[26] = 1;    // session number
				sector[27] = 0x16; // .408 (Data track, copy allowed :) )
				sector[28] = 0;    // track number
				sector[29] = 0xa2; // point (lead-out)
				sector[30] = 0x00; // min
				sector[31] = 0x00; // sec
				sector[32] = 0x00; // frame
				MSF msf;
				// FIXME?
				CDROMDevice::MSFfromLBA(msf, dev->getCapacity()+0);
				sector[33] = 0x00;   // zero
				sector[34] = msf.m;   // start
				sector[35] = msf.s;   //  of
				sector[36] = msf.f;   //  leadout

				sector[37] = 1;    // session number
				sector[38] = 0x16; // .408 (Data track, copy allowed :) )
				sector[39] = 0;    // track number
				sector[40] = 1;    // point
				sector[41] = 0x00; // min
				sector[42] = 0x00; // sec
				sector[43] = 0x00; // frame
				CDROMDevice::MSFfromLBA(msf, 0);
				sector[44] = 0x00;   // zero
				sector[45] = msf.m;  // start
				sector[46] = msf.s;  //  of
				sector[47] = msf.f;  //  track
				break;
			case 3:
				// PMA
			case 4:
				// ATIP
			case 5: 
				// CDTEXT
			default:
				IO_IDE_ERR("read toc: format %d not supported.\n", format);
			}
		} else {
			atapi_command_error(IDE_ATAPI_SENSE_NOT_READY, IDE_ATAPI_ASC_MEDIUM_NOT_PRESENT);
		}
		raiseInterrupt(0);
		break;
	case IDE_ATAPI_COMMAND_READ_HEADER:
	case IDE_ATAPI_COMMAND_PLAY_AUDIO10:
	case IDE_ATAPI_COMMAND_PLAY_AUDIOMSF:
	case IDE_ATAPI_COMMAND_TOGGLE_PAUSE:
	case IDE_ATAPI_COMMAND_STOP:
	case IDE_ATAPI_COMMAND_MODE_SELECT10:
	case IDE_ATAPI_COMMAND_READ_INFO:
		IO_IDE_WARN("ATAPI command 0x%08x not impl.\n", command);
		atapi_command_error(IDE_ATAPI_SENSE_ILLEGAL_REQUEST, IDE_ATAPI_ASC_INV_FIELD_IN_CMD_PACKET);
		raiseInterrupt(0);
		break;
	case IDE_ATAPI_COMMAND_MODE_SENSE6:
	case IDE_ATAPI_COMMAND_MODE_SENSE10: {
		// .324
		uint8 pc = sector[2] >> 6;
		uint8 pagecode = sector[2] & 0x3f;
		int len;
		if (command == IDE_ATAPI_COMMAND_MODE_SENSE6) {
			len = ((uint16)sector[4]<<8)|(sector[5]);
		} else {
			len = ((uint16)sector[7]<<8)|(sector[8]);
		}
		memset(sector, 0, sizeof gIDEState.state[gIDEState.drive].sector);
		// pagecode: .517
		switch (pc) {
		case 0x00:
			switch (pagecode) {
			case 0x01:
				IO_IDE_ERR("MODE SENSE error recovery\n");
				break;
			case 0x1a:
				// Power Condition Page
				// .537
				atapi_start_send_command(command, 20, len);
				atapi_start_mode_sense(sector+8, 20);
				sector[8] = 0x1a;
				sector[9] = 10;
				sector[10] = 0x00;
				sector[11] = 0x00; // idle=0, standby=0
				sector[12] = 0x00;
				sector[13] = 0x00;
				sector[14] = 0x00;
				sector[15] = 0x00;
				sector[16] = 0x00;
				sector[17] = 0x00;
				sector[18] = 0x00;
				sector[19] = 0x00;
				raiseInterrupt(0);
				break;
			case 0x2a:
				// Capabilities and Mechanical Status Page
				// .573
				atapi_start_send_command(command, 36, len);
				atapi_start_mode_sense(sector+8, 36);
				sector[8] = 0x2a;
				sector[9] = 28;
				sector[10] = 0x3; // CD-RW + CD-R read
				sector[11] = 0;
				sector[12] = 0;
				sector[13] = 3<<5;
				sector[14] = (dev->isLocked() ? (1<<1):0)
					| 1 | (1<<3) | (1<<5);
				sector[15] = 0;
				sector[16] = 706 >> 8;
				sector[17] = 706;
				sector[18] = 0;
				sector[19] = 2;
				sector[20] = 512 >> 8;
				sector[21] = 512;
				sector[22] = 706 >> 8;
				sector[23] = 706;
				sector[24] = 0;
				sector[25] = 0;
				sector[26] = 0;
				sector[27] = 0;
				sector[28] = 0;
				sector[29] = 0;
				sector[30] = 0;
				sector[31] = 0;
				sector[32] = 0;
				sector[33] = 0;
				sector[34] = 0;
				sector[35] = 0;
				raiseInterrupt(0);
				break;
			case 0x31:
				// Apple Features
				atapi_start_send_command(command, 16, len);
				atapi_start_mode_sense(sector+8, 16);
				sector[8] = 0x31;
				sector[9] = 6;
				sector[10] = '.';
				sector[11] = 'A';
				sector[12] = 'p';
				sector[13] = 'p';
				sector[14] = 0;
				sector[15] = 0;
				raiseInterrupt(0);
				break;
			case 0x0d:
			case 0x0e:
			case 0x3f:
				atapi_command_error(IDE_ATAPI_SENSE_ILLEGAL_REQUEST, IDE_ATAPI_ASC_INV_FIELD_IN_CMD_PACKET);
				raiseInterrupt(0);
				break;
			default:;
				IO_IDE_ERR("query MODE SENSE not impl. for 0x%08x\n", pagecode);
			}
			raiseInterrupt(0);
			break;
		case 0x01:
			switch (pagecode) {
			case 0x01:
			case 0x2a:
			case 0x0d:
			case 0x0e:
			case 0x3f:
				IO_IDE_ERR("change MODE SENSE not impl. for 0x%08x\n", pagecode);
			default:
				atapi_command_error(IDE_ATAPI_SENSE_ILLEGAL_REQUEST, IDE_ATAPI_ASC_INV_FIELD_IN_CMD_PACKET);
				raiseInterrupt(0);
				break;
			}
			break;
		case 0x02:
			switch (pagecode) {
			case 0x01:
			case 0x2a:
			case 0x0d:
			case 0x0e:
			case 0x3f:
				IO_IDE_ERR("default MODE SENSE not impl. for 0x%08x\n", pagecode);
			default:
				atapi_command_error(IDE_ATAPI_SENSE_ILLEGAL_REQUEST, IDE_ATAPI_ASC_INV_FIELD_IN_CMD_PACKET);
				raiseInterrupt(0);
				break;
			}
			break;
		case 0x03:
			atapi_command_error(IDE_ATAPI_SENSE_ILLEGAL_REQUEST, IDE_ATAPI_ASC_SAVING_PARAMETERS_NOT_SUPPORTED);
			raiseInterrupt(0);
			break;
		}
		break;
	}
	case IDE_ATAPI_COMMAND_GET_CONFIG: {
		// .173 .242 .284
		uint8 RT = sector[1] & 3;
		int len = ((uint16)sector[7]<<8)|(sector[8]);
		int feature = ((uint16)sector[2]<<8)|(sector[3]);
		IO_IDE_TRACE("get_config: RT=%x len=%d f=%x\n", RT, len, feature);
		memset(sector, 0, sizeof gIDEState.state[gIDEState.drive].sector);
		sector[0] = 0x00; // length msb
		sector[1] = 0x00;
		sector[2] = 0x00;
		
		sector[6] = 0x00;
		sector[7] = 0x08; // Current: Profile 8: CDROM
		if (RT == 2) {
			switch (feature) {
			case 0: // Profile List
				sector[3] = 16-4;
				sector[8] = 0x00;
				sector[9] = 0x00;  // Profile List
				sector[10] = 0x03; // Version 0, Persistent=1, Current=1
				sector[11] = 0x04; // Additional Length
				sector[12] = 0x00;
				sector[13] = 0x08; // CDROM
				sector[14] = 0x01; // active	
				sector[15] = 0x00;
				atapi_start_send_command(command, 16, len);
				break;
			case 0x21:  // Incremental Streaming Writable .192
			case 0x2d:  // Track At Once Feature .212
			case 0x2e:  // Session At Once Feature .214
			case 0x103: // CD Audio External Play Feature .224				
			case 0x106: // DVD CSS Feature .106
				sector[3] = 4; // not av.
				atapi_start_send_command(command, 8, len);
				break;
			default: 
				sector[3] = 4; // not av.
				atapi_start_send_command(command, 8, len);
				IO_IDE_WARN("moep\n");
			}
			raiseInterrupt(0);
			break;
		}
		if (RT != 0) IO_IDE_ERR("moepmoep\n");
		sector[3] = 0x44; // length lsb (72-4)
		
		// Features:
		//  Feature 1, Profile List
		sector[8] = 0x00;
		sector[9] = 0x00;  // Profile List
		sector[10] = 0x03; // Version 0, Persistent=1, Current=1
		sector[11] = 0x04; // Additional Length
		sector[12] = 0x00;
		sector[13] = 0x08; // CDROM
		sector[14] = 0x01; // active	
		sector[15] = 0x00;
		
		//  Feature 2, Core
		sector[16] = 0x00;
		sector[17] = 0x01; // Core
		sector[18] = 0x03; // Version 0, Persistent=1, Current=1
		sector[19] = 0x04; // Additional Length
		sector[20] = 0x00;
		sector[21] = 0x00;
		sector[22] = 0x00;
		sector[23] = 0x02; // 02=ATAPI

		//  Feature 3, Morphing
		sector[24] = 0x00;
		sector[25] = 0x02; // Morphing
		sector[26] = 0x07; // Version 1, Persistent=1, Current=1
		sector[27] = 0x04; // Additional Length
		sector[28] = 0x00; // ASYNC = 0 (ATAPI)
		sector[29] = 0x00;
		sector[30] = 0x00;
		sector[31] = 0x00;
		
		//  Feature 4, Removable
		sector[32] = 0x00;
		sector[33] = 0x03; // Removable
		sector[34] = 0x03; // Version 0, Persistent=1, Current=1
		sector[35] = 0x04; // Additional Length
		sector[36] = 0x19; // Tray-Type-Loading, Eject=1, Jmpr=0, LockAllow=1
		sector[37] = 0x00;
		sector[38] = 0x00;
		sector[39] = 0x00;
		
		//  Feature 5, Random Readable
		sector[40] = 0x00;
		sector[41] = 0x10; // Random Readable
		sector[42] = 0x03; // Version 0, Persistent=1, Current=1 [FIXME?]
		sector[43] = 0x08; // Additional Length
		sector[44] = 0x00; // Logical Block Size MSB
		sector[45] = 0x00;
		sector[46] = 0x08;
		sector[47] = 0x00; // Logical Block Size LSB
		sector[48] = 0x00; // Blocking MSB
		sector[49] = 0x10; // Blocking LSB
		sector[50] = 0x00;
		sector[51] = 0x00; // PP=0
		
		//  Feature 6, CD Read
		sector[52] = 0x00;
		sector[53] = 0x1e; // CD Read
		sector[54] = 0x0b; // Version 2, Persistent=1, Current=1 [FIXME?]
		sector[55] = 0x04; // Additional Length
		sector[56] = 0x00; // DAP=0, C2=0, CD-Text=0
		sector[57] = 0x00;
		sector[58] = 0x00;
		sector[59] = 0x00;
		
		//  Feature 7, Power Managment
		sector[60] = 0x01;
		sector[61] = 0x00; // Power Managment
		sector[62] = 0x03; // Version 0, Persistent=1, Current=1 [FIXME?]
		sector[63] = 0x00; // Additional Length
		
		//  Feature 8, Timeout
		sector[64] = 0x01;
		sector[65] = 0x05; // Timeout
		sector[66] = 0x07; // Version 0, Persistent=1, Current=1 [FIXME?]
		sector[67] = 0x04; // Additional Length
		sector[68] = 0x00; // Group 3=0
		sector[69] = 0x00;
		sector[70] = 0x00;
		sector[71] = 0x00;
		
/*		IO_IDE_WARN("ATAPI command 0x%08x not impl.\n", command);
		atapi_command_error(IDE_ATAPI_SENSE_ILLEGAL_REQUEST, IDE_ATAPI_ASC_INV_FIELD_IN_CMD_PACKET);*/
		raiseInterrupt(0);
		break;
	}
	case IDE_ATAPI_COMMAND_SET_CD_SPEED: {
		// .484
		atapi_command_nop();		
		raiseInterrupt(0);
		break;
	}
	case IDE_ATAPI_COMMAND_READ_CD: {
		// .352
		if (dev->isReady()) {
			uint32 len = ((uint32)sector[6]<<16)|((uint32)sector[7]<<8)|(sector[8]);
			if (!len) {
				atapi_command_nop();
			} else {
				int sec_type = (sector[1] >> 2) & 7;  // .353
		    		bool dap = (sector[1] >> 1) & 1;
				bool rel = sector[1] & 1;
				uint32 lba = ((uint32)sector[2]<<24)|((uint32)sector[3]<<16)|((uint32)sector[4]<<8)|sector[5];
				int sub_ch = sector[10] & 3;
				if (sec_type) IO_IDE_ERR("sec_type not supported\n");
				if (rel) IO_IDE_ERR("rel not supported\n");
				if (sub_ch) IO_IDE_ERR("sub-channel not supported\n");
				// .95 .96
				gIDEState.state[gIDEState.drive].atapi_transfer_request = sector[9];
				if (gIDEState.state[gIDEState.drive].atapi_transfer_request & 7) IO_IDE_ERR("c2 not supported\n");
				if (lba + len > dev->getCapacity()) {
					atapi_command_error(IDE_ATAPI_SENSE_ILLEGAL_REQUEST, IDE_ATAPI_ASC_LOGICAL_BLOCK_OOR);
				} else {
					IO_IDE_TRACE("read cd: lba: %08x len: %08x\n", lba, len);
					switch (gIDEState.state[gIDEState.drive].atapi_transfer_request & 0xf8) {  // .355
					case 0x00: // nothing
						gIDEState.state[gIDEState.drive].current_sector_size = 0;
						atapi_command_nop();
						raiseInterrupt(0);
						return;
					case 0x10: // user data
						gIDEState.state[gIDEState.drive].current_sector_size = 2048;
						break;
					case 0xf8: // everything
						// SYNC+HEADER+UserDATA+EDC+(Mode1 Pad)+ECC
						gIDEState.state[gIDEState.drive].current_sector_size = 12+4+2048+288;
						gIDEState.state[gIDEState.drive].atapi_transfer_request = 0xb8; // mode1 -> skip sub-header
						break;
					default:
						IO_IDE_ERR("unknown main channel selection in READ_CD\n");
					}
					uint cursize = gIDEState.state[gIDEState.drive].current_sector_size;
					atapi_start_send_command(command, len*cursize, len*cursize, cursize, cursize);
					gIDEState.config[gIDEState.drive].cdrom.remain = len;
					gIDEState.config[gIDEState.drive].cdrom.next_lba = lba;
					if (atapi_check_dma()) return;
				}
			}
		} else {
			atapi_command_error(IDE_ATAPI_SENSE_NOT_READY, IDE_ATAPI_ASC_MEDIUM_NOT_PRESENT);
		}
		raiseInterrupt(0);
		break;
	}
	case IDE_ATAPI_COMMAND_LOAD_CD:
	case IDE_ATAPI_COMMAND_READ12:
	case IDE_ATAPI_COMMAND_MECH_STATUS:
	case IDE_ATAPI_COMMAND_PLAY_CD:
	case IDE_ATAPI_COMMAND_READ_CD_MSF:
	case IDE_ATAPI_COMMAND_SCAN:
		IO_IDE_WARN("unknown ATAPI command 0x%08x\n", command);
		IO_IDE_ERR("deshalb.\n");
		atapi_command_error(IDE_ATAPI_SENSE_ILLEGAL_REQUEST, IDE_ATAPI_ASC_INV_FIELD_IN_CMD_PACKET);
		raiseInterrupt(0);
		break;
	default:
		IO_IDE_WARN("unknown ATAPI command 0x%08x\n", command);
		IO_IDE_ERR("deshalb.\n");
		atapi_command_error(IDE_ATAPI_SENSE_ILLEGAL_REQUEST, IDE_ATAPI_ASC_INV_FIELD_IN_CMD_PACKET);
		raiseInterrupt(0);
		break;
	}
	// sanity check:
	if (gIDEState.config[gIDEState.drive].cdrom.dma) {
		IO_IDE_WARN("can't use dma with atapi command %x\n", command);
	}
}

	bool bm_ide_dotransfer(bool &prd_exhausted, uint32 bmide_prd_addr, byte bmide_command, byte bmide_status, uint32 lba, uint32 count)
	{
		IO_IDE_TRACE("BM IDE transfer: prd_addr = %08x, lba = %08x, size = %08x\n", bmide_prd_addr, lba, count ? count : gIDEState.state[gIDEState.drive].sector_count);
//		printf("holla die waldfee v2\n");

		struct prd_entry {
			uint32 addr PACKED;
			uint32 size PACKED;
		};

		uint32 prd_addr = ppc_word_from_BE(bmide_prd_addr);
/*		byte *xx;
		if (ppc_direct_physical_memory_handle(prd_addr, xx)) {
			IO_IDE_ERR("false 47\n");
			return false;
		}*/
//		prd_entry *prd = (prd_entry*)xx;
		prd_entry prd;
		ppc_dma_read(&prd, prd_addr, 8);

		prd_exhausted = false;
		int pr_left = ppc_word_from_LE(prd.size) & 0xffff;
		if (!pr_left) pr_left = 64*1024;
/*		byte *pr;
		if (ppc_direct_physical_memory_handle(prd->addr, pr)) {
			IO_IDE_ERR("false 46\n");
			return false;
		}*/
		uint32 pr = ppc_word_from_LE(prd.addr);
		bool write_to_mem = bmide_command & BM_IDE_CR_WRITE;
		bool write_to_device = !write_to_mem;
		while (true) {
			int to_transfer = gIDEState.state[gIDEState.drive].current_sector_size;
			int transfer_at_once = to_transfer;
			if (transfer_at_once > pr_left) transfer_at_once = pr_left;
			do {
				uint8 buffer[transfer_at_once];
				if (write_to_device) {
					ppc_dma_read(buffer, pr, transfer_at_once);
					if (gIDEState.config[gIDEState.drive].device->write(buffer, transfer_at_once) != transfer_at_once) {
						IO_IDE_ERR("false3\n");
						return false;
					}
				} else {
					if (gIDEState.config[gIDEState.drive].device->read(buffer, transfer_at_once) != transfer_at_once) {
						IO_IDE_ERR("false3\n");
						return false;
					}
					ppc_dma_write(pr, buffer, transfer_at_once);
				}
				pr_left -= transfer_at_once;
				pr += transfer_at_once;
				to_transfer -= transfer_at_once;
				if (pr_left < 0) {
					IO_IDE_ERR("pr_left became negative!\n");
				}
				if (!pr_left) {
        				if (prd.size & 0x80000000) {
						// no more prd's, but still something to transfer -> error
						bool ready;
						if (count) {
							ready = false;
						} else {
							ready = gIDEState.state[gIDEState.drive].sector_count > 1;
						}
						if (to_transfer || ready) {
							IO_IDE_ERR("false1\n");
							return false;
						}
						prd_exhausted = true;
					} else {
						// get next prd
						prd_addr += 8;
						ppc_dma_read(&prd, prd_addr, 8);
						pr_left = prd.size & 0xffff;
						if (!pr_left) pr_left = 64*1024;
						pr = ppc_word_from_LE(prd.addr);
						transfer_at_once = to_transfer;
						if (transfer_at_once > pr_left) transfer_at_once = pr_left;
					}
				}
			} while (to_transfer);
			if (count) {
				count--;
				if (!count) break;
			} else {
				incAddress();
				if (!gIDEState.state[gIDEState.drive].sector_count) break;
			}
		}
		gIDEState.config[gIDEState.drive].device->release();
		return true;
        };

	bool read_bmdma_reg(uint32 port, uint32 &data, uint size)
	{
		IO_IDE_TRACE("bm-dma: read port: %08x, size: %d from (%08x, lr: %08x)\n", port, size, gCPU.pc, gCPU.lr);
		switch (port) {
		case 0:
			if (size==1) {
				IO_IDE_TRACE("bmide command = %02x\n", mConfig[BMIDECR0]);
				data = mConfig[BMIDECR0] & BM_IDE_CR_MASK;
				return true;
			}
			break;
		case 1:
			if (size==1) {
				IO_IDE_TRACE("bmide MRDMODE = %02x\n", mConfig[MRDMODE]);
				data = mConfig[MRDMODE];
				return true;
			}
			break;
		case 2:
			if (size==1) {
				IO_IDE_TRACE("bmide status = %02x\n", mConfig[BMIDESR0]);
				data = mConfig[BMIDESR0] & BM_IDE_SR_MASK;
				return true;
			}
			break;
		case 3: 
			if (size == 1) {
				data = mConfig[UDIDETCR0];
				return true;
			}
			break;
		case 4:
			if (size==4) {
				IO_IDE_TRACE("bmide prd address: %08x(endianess)\n", mConfig[DTPR0]);
				memcpy(&data, &mConfig[DTPR0], 4);// FIXME: endianess
				return true;
			}
			break;
		}
		return false;
	}
	
	bool bmide_start_dma(bool startbit)
	{
		IO_IDE_TRACE("start dma %d\n", gIDEState.state[gIDEState.drive].mode);
		switch (gIDEState.state[gIDEState.drive].mode) {
		case IDE_TRANSFER_MODE_NONE:
			/*
			 * wait for both:
			 * bmide start and appropriate device command
			 */
			gIDEState.state[gIDEState.drive].mode = IDE_TRANSFER_MODE_DMA;
			if (startbit) mConfig[BMIDESR0] |= BM_IDE_SR_ACTIVE;
			mConfig[BMIDESR0] &= ~BM_IDE_SR_ERROR;
			return true;
		case IDE_TRANSFER_MODE_DMA:
			break;
		default:
			IO_IDE_ERR("invalid gIDEState.mode in write_bmdma_reg\n");
		} 
		gIDEState.state[gIDEState.drive].mode = IDE_TRANSFER_MODE_NONE;
		bool prd_exhausted;
		uint32 bmide_prd_addr;
		memcpy(&bmide_prd_addr, &mConfig[DTPR0], 4);	// FIXME: endianess
		if (bm_ide_dotransfer(prd_exhausted, bmide_prd_addr, 
				mConfig[BMIDECR0], mConfig[BMIDESR0], 
				gIDEState.state[gIDEState.drive].dma_lba_start, 
				gIDEState.state[gIDEState.drive].dma_lba_count)) {			
			if (prd_exhausted) {
				mConfig[BMIDESR0] &= ~BM_IDE_SR_ACTIVE;
			} else {
				mConfig[BMIDESR0] |= BM_IDE_SR_ACTIVE;
			}
			mConfig[BMIDESR0] &= ~BM_IDE_SR_ERROR;
			mConfig[BMIDESR0] |= BM_IDE_SR_INTERRUPT;
			raiseInterrupt(0);
			return true;
		}
		return false;
	}
	
	bool write_bmdma_reg(uint32 port, uint32 data, uint size)
	{
		IO_IDE_TRACE("bm-dma: write port: %08x, data: %08x, size: %d from (%08x, lr: %08x)\n", port, data, size, gCPU.pc, gCPU.lr);
		switch (port) {
		case 0:
			if (size==1) {
				byte prev_command = mConfig[BMIDECR0];
				mConfig[BMIDECR0] = data & BM_IDE_CR_MASK;

				byte set_command = mConfig[BMIDECR0] & (prev_command ^ mConfig[BMIDECR0]);
				byte reset_command = (~mConfig[BMIDECR0]) & (prev_command ^ mConfig[BMIDECR0]);

				if (set_command & BM_IDE_CR_START) {
					bmide_start_dma(true);
				}
				if (reset_command & BM_IDE_CR_START) {
					cancelInterrupt(0);
				}
				IO_IDE_TRACE("bmide command: want set %02x, now %02x\n", data, mConfig[BMIDECR0]);
				return true;
			}
			break;
		case 1: {
			IO_IDE_TRACE("bmide MRDMODE <- %02x\n", data);
			mConfig[MRDMODE] &= ~(MRDMODE_BLK_CH0 | MRDMODE_BLK_CH1);
			mConfig[MRDMODE] |= data & (MRDMODE_BLK_CH0 | MRDMODE_BLK_CH1);
			if (data & MRDMODE_INTR_CH0) {
//				mConfig[MRDMODE] &= ~MRDMODE_INTR_CH0;
//				cancelInterrupt(0);
			}
			if (data & MRDMODE_INTR_CH1) {
//				mConfig[MRDMODE] &= ~MRDMODE_INTR_CH1;
//				cancelInterrupt(1);
			}
			/*
			 *	if interrupts become unblocked and are pending -> signal them
			 */
			if (!(mConfig[MRDMODE] & MRDMODE_BLK_CH0) && (mConfig[MRDMODE] & MRDMODE_INTR_CH0)) {
				raiseInterrupt(0);
			}
			if (!(mConfig[MRDMODE] & MRDMODE_BLK_CH1) && (mConfig[MRDMODE] & MRDMODE_INTR_CH1)) {
				raiseInterrupt(1);
			}
			return true;
		}
		case 2:
			if (size==1) {
/*				byte set_status = data & ((data & BM_IDE_SR_MASK) ^ mConfig[BMIDESR0]);
				byte reset_status = (~data) & ((data & BM_IDE_SR_MASK) ^ mConfig[BMIDESR0]);*/
				if (data & BM_IDE_SR_ERROR) mConfig[BMIDESR0] &= ~BM_IDE_SR_ERROR;
				if (data & BM_IDE_SR_INTERRUPT) {
					mConfig[BMIDESR0] &= ~BM_IDE_SR_INTERRUPT;
					cancelInterrupt(0);
				}
				if (data & BM_IDE_SR_DMA0_CAPABLE) mConfig[BMIDESR0] |= BM_IDE_SR_DMA0_CAPABLE;
				if (data & BM_IDE_SR_DMA1_CAPABLE) mConfig[BMIDESR0] |= BM_IDE_SR_DMA1_CAPABLE;
				if ((~data) & BM_IDE_SR_DMA0_CAPABLE) mConfig[BMIDESR0] &= ~BM_IDE_SR_DMA0_CAPABLE;
				if ((~data) & BM_IDE_SR_DMA1_CAPABLE) mConfig[BMIDESR0] &= ~BM_IDE_SR_DMA1_CAPABLE;
				IO_IDE_TRACE("bmide status: want set %02x, now %02x\n", data, mConfig[BMIDESR0]);
				return true;
			}
			break;
		case 3: 
			if (size == 1) {
				mConfig[UDIDETCR0] = data;
				return true;
			}
			break;
		case 4:
			if (size==4) {
				IO_IDE_TRACE("bmide prd address: %08x\n", data);
				memcpy(&mConfig[DTPR0], &data, 4);// FIXME: endianess
				return true;
			}
		}
		return false;
	}

	void ide_write_reg(uint32 addr, uint32 data, int size)
	{
		if (size != 1) {
			if (size != 2) {
				IO_IDE_ERR("ide size bla\n");
			}
			if (addr != IDE_ADDRESS_DATA) {
				IO_IDE_ERR("ide size bla\n");
			}
//			IO_IDE_TRACE("data <- %04x\n", data);
			switch (gIDEState.state[gIDEState.drive].current_command) {
			case IDE_COMMAND_WRITE_SECTOR: 
				gIDEState.state[gIDEState.drive].sector[gIDEState.state[gIDEState.drive].sectorpos++] = data >> 8;
				gIDEState.state[gIDEState.drive].sector[gIDEState.state[gIDEState.drive].sectorpos++] = data;
				if (gIDEState.state[gIDEState.drive].sectorpos == 512) {
					if (gIDEState.state[gIDEState.drive].mode == IDE_TRANSFER_MODE_WRITE) {
						uint32 pos = makeLogical(gIDEState.state[gIDEState.drive].head, 
							gIDEState.state[gIDEState.drive].cyl, 
							gIDEState.state[gIDEState.drive].sector_no);
						incAddress();
						IO_IDE_TRACE(" write sector cont. (%08x, %d)\n", pos, gIDEState.state[gIDEState.drive].sector_count);
						IDEDevice *dev = gIDEState.config[gIDEState.drive].device;
						dev->acquire();
						dev->setMode(ATA_DEVICE_MODE_PLAIN, 512);
						dev->seek(pos);
						dev->writeBlock(gIDEState.state[gIDEState.drive].sector);
						dev->release();
						if (gIDEState.state[gIDEState.drive].sector_count) {
							gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_DRQ | IDE_STATUS_SKC;
						} else {
							gIDEState.state[gIDEState.drive].mode = IDE_TRANSFER_MODE_NONE;
							gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_SKC;
						}
						raiseInterrupt(0);
					} else {
						IO_IDE_ERR("invalid state in %s:%d\n", __FILE__, __LINE__);
						gIDEState.state[gIDEState.drive].mode = IDE_TRANSFER_MODE_NONE;
					}
					gIDEState.state[gIDEState.drive].sectorpos = 0;
				}
				break;
			case IDE_COMMAND_PACKET:
				if (gIDEState.state[gIDEState.drive].sectorpos >= IDE_ATAPI_PACKET_SIZE) {
					IO_IDE_ERR("sectorpos >= PACKET_SIZE\n");
				}
				gIDEState.state[gIDEState.drive].sector[gIDEState.state[gIDEState.drive].sectorpos++] = data >> 8;
				gIDEState.state[gIDEState.drive].sector[gIDEState.state[gIDEState.drive].sectorpos++] = data;
				if (gIDEState.state[gIDEState.drive].sectorpos >= IDE_ATAPI_PACKET_SIZE) {
					// ATAPI packet received
					IDEDevice *dev = gIDEState.config[gIDEState.drive].device;
					dev->acquire();
					receive_atapi_packet();	
					dev->release();
				}
				break;
			}
			return;
		}
		switch (addr) {
		case IDE_ADDRESS_FEATURE: {
			IO_IDE_TRACE("feature <- %x\n", data);
			gIDEState.state[gIDEState.drive].feature = data;
			return;
		}
		case IDE_ADDRESS_COMMAND: {
			IO_IDE_TRACE("command register (%02x)\n", data);
			gIDEState.state[gIDEState.drive].current_command = data;
			gIDEState.one_time_shit = true;
			switch (data) {
			case IDE_COMMAND_RESET_ATAPI: {
				if (gIDEState.config[gIDEState.drive].protocol != IDE_ATAPI) {
					IO_IDE_WARN("reset non ATAPI-Drive\n");
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
					gIDEState.state[gIDEState.drive].error = 0x4;
					break;
				}
				gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_SKC;
				gIDEState.state[gIDEState.drive].error = 0; 
				gIDEState.state[gIDEState.drive].sector_count = 1;
				gIDEState.state[gIDEState.drive].sector_no = 1;
				gIDEState.state[gIDEState.drive].cyl = 0xeb14;
				gIDEState.state[gIDEState.drive].head = 0;
				// no interrupt:
				return;
			}
			case IDE_COMMAND_RECALIBRATE: {
				if (gIDEState.config[gIDEState.drive].protocol != IDE_ATA) {
					IO_IDE_WARN("recalibrate non ATA-Drive\n");
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
					gIDEState.state[gIDEState.drive].error = 0x4;
					break;
				}
				if (gIDEState.config[gIDEState.drive].installed) {
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_SKC;
					gIDEState.state[gIDEState.drive].error = 0; 
				} else {
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
					gIDEState.state[gIDEState.drive].error = 0x2; // Track 0 not found
				}
				break;
			}
			case IDE_COMMAND_READ_SECTOR: {
				if (gIDEState.config[gIDEState.drive].protocol != IDE_ATA) {
					IO_IDE_WARN("read sector from non ATA-Disk\n");
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
					gIDEState.state[gIDEState.drive].error = 0x4;
					break;
				}
				uint32 pos = makeLogical(gIDEState.state[gIDEState.drive].head, 
					gIDEState.state[gIDEState.drive].cyl, 
					gIDEState.state[gIDEState.drive].sector_no);
				IO_IDE_TRACE("read sector(%08x, %d)\n", pos, gIDEState.state[gIDEState.drive].sector_count);
				gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_DRQ | IDE_STATUS_SKC;

				gIDEState.state[gIDEState.drive].mode = IDE_TRANSFER_MODE_READ;
				IDEDevice *dev = gIDEState.config[gIDEState.drive].device;
				dev->acquire();
				dev->setMode(ATA_DEVICE_MODE_PLAIN, 512);
				dev->seek(pos);
				dev->readBlock(gIDEState.state[gIDEState.drive].sector);
				dev->release();
				gIDEState.state[gIDEState.drive].sectorpos = 0;
				gIDEState.state[gIDEState.drive].error = 0;
				break;
			}
			case IDE_COMMAND_WRITE_SECTOR: {
				if (gIDEState.config[gIDEState.drive].protocol != IDE_ATA) {
					IO_IDE_WARN("write sector to non ATA-Disk\n");
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
					gIDEState.state[gIDEState.drive].error = 0x4;
					break;
				}
				IO_IDE_TRACE("write sector(%08x, %d)\n", 
					makeLogical(gIDEState.state[gIDEState.drive].head, 
						gIDEState.state[gIDEState.drive].cyl, 
						gIDEState.state[gIDEState.drive].sector_no), 
					gIDEState.state[gIDEState.drive].sector_count);
				gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_DRQ | IDE_STATUS_SKC;
				gIDEState.state[gIDEState.drive].mode = IDE_TRANSFER_MODE_WRITE;
				gIDEState.state[gIDEState.drive].sectorpos = 0;
				gIDEState.state[gIDEState.drive].error = 0;
				return;
			}
			case IDE_COMMAND_FIX_PARAM: {
				if (gIDEState.config[gIDEState.drive].protocol != IDE_ATA) {
					IO_IDE_WARN("recalibrate non ATA-Drive\n");
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
					gIDEState.state[gIDEState.drive].error = 0x4;
					break;
				}
				if (gIDEState.config[gIDEState.drive].installed) {
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_SKC;
					gIDEState.state[gIDEState.drive].error = 0;
				} else {
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
					gIDEState.state[gIDEState.drive].error = 0x2; // Track 0 not found
				}
				break;
			}
			case IDE_COMMAND_READ_SECTOR_DMA: {
				if (gIDEState.config[gIDEState.drive].protocol != IDE_ATA) {
					IO_IDE_WARN("read sector from non ATA-Disk\n");
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
					gIDEState.state[gIDEState.drive].error = 0x4;
					break;
				}
				gIDEState.state[gIDEState.drive].dma_lba_start = makeLogical(
					gIDEState.state[gIDEState.drive].head, 
					gIDEState.state[gIDEState.drive].cyl, 
					gIDEState.state[gIDEState.drive].sector_no);
				gIDEState.state[gIDEState.drive].dma_lba_count = 0;
				gIDEState.state[gIDEState.drive].current_sector_size = 512;
				IO_IDE_TRACE("read sector dma(%08x, %d)\n", 
					gIDEState.state[gIDEState.drive].dma_lba_start, 
					gIDEState.state[gIDEState.drive].sector_count);
				gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_SKC;
				gIDEState.config[gIDEState.drive].device->acquire();
				gIDEState.config[gIDEState.drive].device->setMode(ATA_DEVICE_MODE_PLAIN, 512);
				gIDEState.config[gIDEState.drive].device->seek(gIDEState.state[gIDEState.drive].dma_lba_start);
				bmide_start_dma(false);
				// no interrupt here:
				return;
			}
			case IDE_COMMAND_WRITE_SECTOR_DMA: {
				if (gIDEState.config[gIDEState.drive].protocol != IDE_ATA) {
					IO_IDE_WARN("write sector to non ATA-Disk\n");
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
					gIDEState.state[gIDEState.drive].error = 0x4;
					break;
				}
				gIDEState.state[gIDEState.drive].dma_lba_start = makeLogical(
					gIDEState.state[gIDEState.drive].head, 
					gIDEState.state[gIDEState.drive].cyl, 
					gIDEState.state[gIDEState.drive].sector_no);
				gIDEState.state[gIDEState.drive].dma_lba_count = 0;
				gIDEState.state[gIDEState.drive].current_sector_size = 512;
				IO_IDE_TRACE("write sector dma(%08x, %d)\n", 
					gIDEState.state[gIDEState.drive].dma_lba_start, 
					gIDEState.state[gIDEState.drive].sector_count);
				gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_SKC;
				gIDEState.config[gIDEState.drive].device->acquire();				
				gIDEState.config[gIDEState.drive].device->setMode(ATA_DEVICE_MODE_PLAIN, 512);
				gIDEState.config[gIDEState.drive].device->seek(gIDEState.state[gIDEState.drive].dma_lba_start);
				bmide_start_dma(false);
				// no interrupt here:
				return;
			}
			case IDE_COMMAND_IDENT: {
				if (gIDEState.config[gIDEState.drive].protocol == IDE_ATAPI) {
					gIDEState.drive_head &= ~0xf;
					gIDEState.state[gIDEState.drive].sector_no = 1;
					gIDEState.state[gIDEState.drive].sector_count = 1;
					gIDEState.state[gIDEState.drive].cyl = 0xeb14;
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
					gIDEState.state[gIDEState.drive].error = 0x4;
					break;
				}
				drive_ident();
				break;
			}
			case IDE_COMMAND_IDENT_ATAPI: {			
				if (gIDEState.config[gIDEState.drive].protocol == IDE_ATA) {
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
					gIDEState.state[gIDEState.drive].error = 0x4;
					break;
				}
				drive_ident();
				break;
			}
			case IDE_COMMAND_PACKET: {
				if (gIDEState.config[gIDEState.drive].protocol != IDE_ATAPI) {
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
					gIDEState.state[gIDEState.drive].error = 0x4;
					break;
				}
				if (gIDEState.state[gIDEState.drive].feature & 1) {
//					IO_IDE_WARN("ATAPI feature dma\n");
					gIDEState.config[gIDEState.drive].cdrom.dma = true;
				} else {
					gIDEState.config[gIDEState.drive].cdrom.dma = false;
				}				
				if (gIDEState.state[gIDEState.drive].feature & 2) {
					IO_IDE_ERR("ATAPI feature overlapped not supported\n");
				}
				gIDEState.state[gIDEState.drive].sector_count = 1;
				gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_SKC | IDE_STATUS_DRQ;
				gIDEState.state[gIDEState.drive].sectorpos = 0;
				// don't raise interrupt:
				return;
			}
			case IDE_COMMAND_SET_FEATURE: {
				switch (gIDEState.state[gIDEState.drive].feature) {
				case IDE_COMMAND_FEATURE_ENABLE_WRITE_CACHE:
				case IDE_COMMAND_FEATURE_SET_TRANSFER_MODE:
				case IDE_COMMAND_FEATURE_ENABLE_APM:
				case IDE_COMMAND_FEATURE_SET_PIO_MODE:
				case IDE_COMMAND_FEATURE_DISABLE_WRITE_CACHE:
				case IDE_COMMAND_FEATURE_ENABLE_LOOKAHEAD:
				case IDE_COMMAND_FEATURE_DISABLE_LOOKAHEAD:
				case IDE_COMMAND_FEATURE_ENABLE_PW_DEFAULT:
				case IDE_COMMAND_FEATURE_DISABLE_PW_DEFAULT:
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY;
					gIDEState.state[gIDEState.drive].error = 0;
					break;
				default:
					IO_IDE_WARN("set feature: unkown sub-command (0x%02x)\n", gIDEState.state[gIDEState.drive].feature);
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
					gIDEState.state[gIDEState.drive].error = 0x4;
					break;
				}
				// FIXME: dont raise interrupt?
				break;
			}
			case IDE_COMMAND_STANDBY_IMMEDIATE:
				IO_IDE_WARN("command STANDBY_IMMEDIATE stub\n");
				// FIXME: dont raise interrupt?
				break;
			case IDE_COMMAND_FLUSH_CACHE:
				gIDEState.config[gIDEState.drive].device->acquire();
				gIDEState.config[gIDEState.drive].device->flush();
				gIDEState.config[gIDEState.drive].device->release();
				break;
			case IDE_COMMAND_SLEEP: 
				IO_IDE_WARN("command SLEEP stub\n");
				// FIXME: dont raise interrupt?
				break;
			case IDE_COMMAND_SET_MULTIPLE:
				IO_IDE_WARN("command SET MULTIPLE not implemented\n");
				gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
				gIDEState.state[gIDEState.drive].error = 0x4;
				break;
			case IDE_COMMAND_READ_NATIVE_MAX:
				IO_IDE_WARN("command READ NATIVE MAX ADDRESS not implemented\n");
				gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_ERR;
				gIDEState.state[gIDEState.drive].error = 0x4;
				break;
			default:			
				IO_IDE_ERR("command '%x' not impl\n", data);
			}
			raiseInterrupt(0);
			return;
		}
		case IDE_ADDRESS_DRV_HEAD: {
			IO_IDE_TRACE("drive head <- %x\n", data);
			gIDEState.drive_head = data | 0xa0;
			if (!(gIDEState.drive_head & IDE_DRIVE_HEAD_SLAVE)) {
				if (gIDEState.config[0].installed) {
					gIDEState.state[0].status &= ~IDE_STATUS_ERR;
					gIDEState.state[0].status |= IDE_STATUS_RDY;
					if (!gIDEState.one_time_shit) {
						gIDEState.state[0].status |= IDE_STATUS_SKC;
						if (gIDEState.config[0].protocol == IDE_ATA) {
							gIDEState.state[0].cyl = 0;
							gIDEState.state[0].sector_count = 1;
							gIDEState.state[0].sector_no = 1;
						} else {
							gIDEState.state[0].cyl = 0xeb14;
						}
					}
					gIDEState.state[0].error = 1;
				} else {
					gIDEState.state[0].status |= IDE_STATUS_ERR;
					gIDEState.state[0].error = 4; // abort
					// FIXME: is this correct?
					// should we allow setting gIDEState.drive
					// to drives not present or return here?
				}
				gIDEState.drive = 0;
			} else {
				if (gIDEState.config[1].installed) {
					gIDEState.state[1].status &= ~IDE_STATUS_ERR;
					gIDEState.state[1].status |= IDE_STATUS_RDY;
					if (!gIDEState.one_time_shit) {
						gIDEState.state[1].status |= IDE_STATUS_SKC;
						if (gIDEState.config[1].protocol == IDE_ATA) {
							gIDEState.state[1].cyl = 0;
							gIDEState.state[1].sector_count = 1;
							gIDEState.state[1].sector_no = 1;
						} else {
							gIDEState.state[1].cyl = 0xeb14;
						}
					}
					gIDEState.state[1].error = 1;
				} else {
					gIDEState.state[1].status |= IDE_STATUS_ERR;
					gIDEState.state[1].error = 4; // abort
				}
				gIDEState.drive = 1;
				// FIXME: see above
			}
			gIDEState.config[gIDEState.drive].lba = gIDEState.drive_head & IDE_DRIVE_HEAD_LBA;
			gIDEState.state[gIDEState.drive].head = gIDEState.drive_head & 0x0f;
			return;
		}
		case IDE_ADDRESS_OUTPUT: {
			if (data & 4) {
				// reset
			}
			IO_IDE_TRACE("output register <- %x\n", data);
			gIDEState.state[gIDEState.drive].outreg = data;
			return;
		}
		case IDE_ADDRESS_SEC_CNT: {
			IO_IDE_TRACE("sec_cnt <- %x\n", data);
			gIDEState.state[gIDEState.drive].sector_count = data;
			return;
		}
		case IDE_ADDRESS_SEC_NO: {
			IO_IDE_TRACE("sec_no <- %x\n", data);
			gIDEState.state[gIDEState.drive].sector_no = data;
			return;
		}
		case IDE_ADDRESS_CYL_LSB: {
			IO_IDE_TRACE("cyl_lsb <- %x\n", data);
			gIDEState.state[gIDEState.drive].cyl = (data&0xff) + (gIDEState.state[gIDEState.drive].cyl & 0xff00);
			return;
		}
		case IDE_ADDRESS_CYL_MSB: 
			IO_IDE_TRACE("cyl_msb <- %x\n", data);
			gIDEState.state[gIDEState.drive].cyl = ((data<<8)&0xff00) + (gIDEState.state[gIDEState.drive].cyl & 0xff);
			return;
		}
		IO_IDE_ERR("hae?\n");
	};

void ide_read_reg(uint32 addr, uint32 &data, int size)
{
	if (size != 1) {
		if (size != 2) {
			IO_IDE_ERR("ide size bla\n");
		}
		if (addr != IDE_ADDRESS_DATA) {
			IO_IDE_ERR("ide size bla\n");
		}
		if (!(gIDEState.state[gIDEState.drive].status & IDE_STATUS_DRQ)) {
			IO_IDE_WARN("read data w/o DRQ, last command: 0x%08x\n", gIDEState.state[gIDEState.drive].current_command);
			return;
		}
		switch (gIDEState.state[gIDEState.drive].current_command) {
		case IDE_COMMAND_READ_SECTOR: 
		case IDE_COMMAND_IDENT:
		case IDE_COMMAND_IDENT_ATAPI:
			data = ((uint16)gIDEState.state[gIDEState.drive].sector[gIDEState.state[gIDEState.drive].sectorpos++])<<8;
			data |= gIDEState.state[gIDEState.drive].sector[gIDEState.state[gIDEState.drive].sectorpos++];
//			IO_IDE_TRACE("data: %04x\n", data);
			if (gIDEState.state[gIDEState.drive].sectorpos == 512) {
				if (gIDEState.state[gIDEState.drive].mode == IDE_TRANSFER_MODE_READ) {
					incAddress();
					if (gIDEState.state[gIDEState.drive].sector_count) {
						gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_SKC | IDE_STATUS_DRQ;
						gIDEState.state[gIDEState.drive].mode = IDE_TRANSFER_MODE_READ;
						uint32 pos = makeLogical(gIDEState.drive_head & 0xf, gIDEState.state[gIDEState.drive].cyl, gIDEState.state[gIDEState.drive].sector_no);
						IO_IDE_TRACE(" read sector cont. (%08x, %d)\n", pos, gIDEState.state[gIDEState.drive].sector_count);
						IDEDevice *dev = gIDEState.config[gIDEState.drive].device;
						dev->acquire();
						dev->setMode(ATA_DEVICE_MODE_PLAIN, 512);
						dev->seek(pos);
						dev->readBlock(gIDEState.state[gIDEState.drive].sector);
						dev->release();
						raiseInterrupt(0);
					} else {
						gIDEState.state[gIDEState.drive].mode = IDE_TRANSFER_MODE_NONE;
						gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY;
					}
				} else {
					gIDEState.state[gIDEState.drive].mode = IDE_TRANSFER_MODE_NONE;
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY;
				}
				gIDEState.state[gIDEState.drive].sectorpos = 0;
			}
			break;
		case IDE_COMMAND_PACKET:
			if (gIDEState.state[gIDEState.drive].sectorpos == gIDEState.state[gIDEState.drive].current_sector_size) {
				switch (gIDEState.config[gIDEState.drive].cdrom.atapi.command) {
				case IDE_ATAPI_COMMAND_READ10:
				case IDE_ATAPI_COMMAND_READ12: {
					CDROMDevice *dev = (CDROMDevice *)gIDEState.config[gIDEState.drive].device;
					if (!dev->isReady()) {
						IO_IDE_ERR("read with cdrom not ready\n");
					}
					dev->acquire();
					dev->setMode(IDE_ATAPI_TRANSFER_DATA, 
						gIDEState.state[gIDEState.drive].current_sector_size);
					dev->seek(gIDEState.config[gIDEState.drive].cdrom.next_lba);
					dev->readBlock(gIDEState.state[gIDEState.drive].sector);
					dev->release();
					gIDEState.config[gIDEState.drive].cdrom.next_lba++;
					gIDEState.config[gIDEState.drive].cdrom.remain--;
					gIDEState.state[gIDEState.drive].sectorpos = 0;
					break;
				}
				case IDE_ATAPI_COMMAND_READ_CD: {
					CDROMDevice *dev = (CDROMDevice *)gIDEState.config[gIDEState.drive].device;
					if (!dev->isReady()) {
						IO_IDE_ERR("read with cdrom not ready\n");
					}
					dev->acquire();
					dev->setMode(gIDEState.state[gIDEState.drive].atapi_transfer_request, 
						gIDEState.state[gIDEState.drive].current_sector_size);
					dev->seek(gIDEState.config[gIDEState.drive].cdrom.next_lba);
					dev->readBlock(gIDEState.state[gIDEState.drive].sector);
					dev->release();
					gIDEState.config[gIDEState.drive].cdrom.next_lba++;
					gIDEState.config[gIDEState.drive].cdrom.remain--;
					gIDEState.state[gIDEState.drive].sectorpos = 0;
					break;
				}
				default:
					IO_IDE_ERR("unknown atapi state\n");
				}
			}
			data = ((uint16)gIDEState.state[gIDEState.drive].sector[gIDEState.state[gIDEState.drive].sectorpos++])<<8;
			data |= gIDEState.state[gIDEState.drive].sector[gIDEState.state[gIDEState.drive].sectorpos++];
//			IO_IDE_TRACE("data: %04x\n", data);
			gIDEState.state[gIDEState.drive].drqpos += 2;
			if (gIDEState.state[gIDEState.drive].drqpos >= gIDEState.config[gIDEState.drive].cdrom.atapi.drq_bytes) {
				gIDEState.state[gIDEState.drive].drqpos = 0;
				gIDEState.config[gIDEState.drive].cdrom.atapi.total_remain -= gIDEState.config[gIDEState.drive].cdrom.atapi.drq_bytes;
				if (gIDEState.config[gIDEState.drive].cdrom.atapi.total_remain > 0) {
					gIDEState.state[gIDEState.drive].status &= ~IDE_STATUS_BSY;
					gIDEState.state[gIDEState.drive].status |= IDE_STATUS_DRQ;
					gIDEState.state[gIDEState.drive].intr_reason |= IDE_ATAPI_INTR_REASON_I_O;
					gIDEState.state[gIDEState.drive].intr_reason &= ~IDE_ATAPI_INTR_REASON_C_D;
					if (gIDEState.state[gIDEState.drive].byte_count > gIDEState.config[gIDEState.drive].cdrom.atapi.total_remain) {
						gIDEState.state[gIDEState.drive].byte_count = gIDEState.config[gIDEState.drive].cdrom.atapi.total_remain;
					}
					gIDEState.config[gIDEState.drive].cdrom.atapi.drq_bytes = gIDEState.state[gIDEState.drive].byte_count;
				} else {
					gIDEState.state[gIDEState.drive].status = IDE_STATUS_RDY | IDE_STATUS_SKC;
					gIDEState.state[gIDEState.drive].intr_reason |= IDE_ATAPI_INTR_REASON_I_O | IDE_ATAPI_INTR_REASON_C_D;
					gIDEState.state[gIDEState.drive].intr_reason &= ~IDE_ATAPI_INTR_REASON_REL;
				}
				raiseInterrupt(0);
			}
			break;
		default:
			IO_IDE_ERR("data read + DRQ after 0x08%x\n", gIDEState.state[gIDEState.drive].current_command);
		}
		return;
	}
	switch (addr) {
	case IDE_ADDRESS_ERROR: {
		IO_IDE_TRACE("error: %02x\n", gIDEState.state[gIDEState.drive].error);
		data = gIDEState.state[gIDEState.drive].error;
		gIDEState.state[gIDEState.drive].status &= ~IDE_STATUS_ERR;
		return ;
	}
	case IDE_ADDRESS_DRV_HEAD: {
		IO_IDE_TRACE("drive_head: %02x\n", gIDEState.drive_head);
		data = (gIDEState.state[gIDEState.drive].head & 0x0f)
		 | (gIDEState.drive_head & 0x10)
		 | 0xa0
		 | (gIDEState.config[gIDEState.drive].lba ? (1<<6): 0);
		return ;
	}
	case IDE_ADDRESS_STATUS: {
		IO_IDE_TRACE("status: %02x\n", gIDEState.state[gIDEState.drive].status);
		data = gIDEState.state[gIDEState.drive].status;
		cancelInterrupt(0);
		return ;
	}
	case IDE_ADDRESS_STATUS2: {
		IO_IDE_TRACE("alt-status register: %02x\n", gIDEState.state[gIDEState.drive].status);
		data = gIDEState.state[gIDEState.drive].status;
		return;
	}
	case IDE_ADDRESS_SEC_CNT: {
		data = gIDEState.state[gIDEState.drive].sector_count;
		IO_IDE_TRACE("sec_cnt: %x (from: @%08x)\n", data, gCPU.pc);
		return;
	}
	case IDE_ADDRESS_SEC_NO: {
		data = gIDEState.state[gIDEState.drive].sector_no;
		IO_IDE_TRACE("sec_no: %x\n", data);
		return;
	}
	case IDE_ADDRESS_CYL_LSB: {
		data = gIDEState.state[gIDEState.drive].cyl & 0xff;
		IO_IDE_TRACE("cyl_lsb: %x\n", data);
		return;
	}
	case IDE_ADDRESS_CYL_MSB: {
		data = (gIDEState.state[gIDEState.drive].cyl & 0xff00) >> 8;
		IO_IDE_TRACE("cyl_msb: %x\n", data);
		return;
	}
	}
	IO_IDE_ERR("hae?\n");
};

/********************************************************************************
 *	PCI Interface
 */	
	virtual bool	readDeviceIO(uint r, uint32 port, uint32 &data, uint size)
	{
		switch (r) {
		case IDE_PCI_REG_0_CMD:
			ide_read_reg(port, data, size);
			return true;
		case IDE_PCI_REG_0_CTRL:
			ide_read_reg(port+0x10, data, size);
			return true;
		case IDE_PCI_REG_BMDMA:
			return read_bmdma_reg(port, data, size);
		}
		return false;
	};
	
	virtual bool	writeDeviceIO(uint r, uint32 port, uint32 data, uint size)
	{
		switch (r) {
		case IDE_PCI_REG_0_CMD:
			ide_write_reg(port, data, size);
			return true;
		case IDE_PCI_REG_0_CTRL:
			ide_write_reg(port+0x10, data, size);
			return true;
		case IDE_PCI_REG_BMDMA:
			return write_bmdma_reg(port, data, size);
		}
		return false;
	};
	
	virtual void	readConfig(uint reg)
	{
		if (reg >= BMIDECR0 && reg <= DTPR0) {
			// they are already set...
			// hook here, if you need notify on read
		}
		PCI_Device::readConfig(reg);
	};
	
	virtual void writeConfig(uint reg, int offset, int size)
	{
		if (reg == 0x30) {
			// ROM-Address
			// FIXME: Who needs this?
			gPCI_Data &= ~3;
		}
		if (reg >= BMIDECR0 && reg <= DTPR0) {
			// FIXME: please fix this. I won't.
			if (size != 1) IO_IDE_ERR("size != 1 bla in writeConfig()\n");
			uint32 data = gPCI_Data >> (offset*8);
			write_bmdma_reg(reg-BMIDECR0+offset, data, size);
			return ;
		}
		PCI_Device::writeConfig(reg, offset, size);
	};
	
};

/*
 *
 */
 
IDEConfig *ide_get_config(int disk)
{
	switch (disk) {
	case 0:
	case 1:
		return &gIDEState.config[disk];
		break;
	}
	return NULL;
}

#define IDE_KEY_IDE0_MASTER_INSTALLED	"pci_ide0_master_installed"
#define IDE_KEY_IDE0_MASTER_TYPE	"pci_ide0_master_type"
#define IDE_KEY_IDE0_MASTER_IMG		"pci_ide0_master_image"
#define IDE_KEY_IDE0_SLAVE_INSTALLED	"pci_ide0_slave_installed"
#define IDE_KEY_IDE0_SLAVE_TYPE		"pci_ide0_slave_type"
#define IDE_KEY_IDE0_SLAVE_IMG		"pci_ide0_slave_image"

#include "configparser.h"
#include "tools/except.h"
void ide_init()
{
	memset(&gIDEState, 0, sizeof gIDEState);
	for (int DISK=0; DISK<2; DISK++) {
		const char *instkeys[] = {IDE_KEY_IDE0_MASTER_INSTALLED, IDE_KEY_IDE0_SLAVE_INSTALLED};
		const char *instkey = instkeys[DISK];
		const char *typekeys[] = {IDE_KEY_IDE0_MASTER_TYPE, IDE_KEY_IDE0_SLAVE_TYPE};
		const char *typekey = typekeys[DISK];
		const char *imgkeys[] = {IDE_KEY_IDE0_MASTER_IMG, IDE_KEY_IDE0_SLAVE_IMG};
		const char *imgkey = imgkeys[DISK];
		if (gConfig->getConfigInt(instkey)) {
			const char *masterslave[] = {"master", "slave"};
			if (!gConfig->haveKey(imgkey)) throw new MsgfException("no disk image specified for ide%d %s.", 0, masterslave[DISK]);
			String img, tmp, ext;
			gConfig->getConfigString(imgkey, img);
			if (gConfig->haveKey(typekey)) {
				String type;
				gConfig->getConfigString(typekey, type);
				if (type == (String)"hd") {
					ext = "img";
				} else if (type == (String)"cdrom") {
					ext = "iso";
				} else {
					IO_IDE_ERR("key '%s' must be set to 'hd' or 'cdrom'\n", typekey);
				}
			} else {
				// type isn't specified, so we must rely on the file extension
				if (!img.rightSplit('.', tmp, ext)) {
					IO_IDE_ERR("unknown disk image (file extension is neither 'img' nor 'iso').\n");
				}
			}
			String name;
			name.assignFormat("ide%d", DISK);
			if (ext == (String)"img") {
				gIDEState.config[DISK].protocol = IDE_ATA;
				gIDEState.config[DISK].device = new ATADeviceFile(name, img);
				const char *error;
				if ((error = gIDEState.config[DISK].device->getError())) IO_IDE_ERR("%s\n", error);
				gIDEState.config[DISK].hd.cyl = ((ATADevice*)gIDEState.config[DISK].device)->mCyl;
				gIDEState.config[DISK].hd.heads = ((ATADevice*)gIDEState.config[DISK].device)->mHeads;
				gIDEState.config[DISK].hd.spt = ((ATADevice*)gIDEState.config[DISK].device)->mSpt;
				gIDEState.config[DISK].bps = 512;
				gIDEState.config[DISK].lba = true;
			} else if (ext == (String)"iso") {
				gIDEState.config[DISK].protocol = IDE_ATAPI;
				gIDEState.config[DISK].device = new CDROMDeviceFile(name);
				((CDROMDeviceFile *)gIDEState.config[DISK].device)->changeDataSource(img);
				const char *error;
				if ((error = gIDEState.config[DISK].device->getError())) IO_IDE_ERR("%s\n", error);
				((CDROMDeviceFile *)gIDEState.config[DISK].device)->setReady(true);
				gIDEState.config[DISK].bps = 2048;
				gIDEState.config[DISK].lba = false;
			} else {
				IO_IDE_ERR("unknown disk image (file extension is neither 'img' nor 'iso').\n");
			}
			gIDEState.config[DISK].installed = true;
		} else {
			gIDEState.config[DISK].installed = false;
		}
	}

	gIDEState.state[0].status = IDE_STATUS_RDY;
	gIDEState.state[1].status = IDE_STATUS_RDY;
	gIDEState.one_time_shit = false;
	if (gIDEState.config[0].installed || gIDEState.config[1].installed) {
		gPCI_Devices->insert(new IDE_Controller());
	}
}

void ide_done()
{
	delete gIDEState.config[0].device;
	delete gIDEState.config[1].device;
}

void ide_init_config()
{
	gConfig->acceptConfigEntryIntDef(IDE_KEY_IDE0_MASTER_INSTALLED, 0);
	gConfig->acceptConfigEntryString(IDE_KEY_IDE0_MASTER_TYPE, false);
	gConfig->acceptConfigEntryString(IDE_KEY_IDE0_MASTER_IMG, false);
	gConfig->acceptConfigEntryIntDef(IDE_KEY_IDE0_SLAVE_INSTALLED, 0);
	gConfig->acceptConfigEntryString(IDE_KEY_IDE0_SLAVE_TYPE, false);
	gConfig->acceptConfigEntryString(IDE_KEY_IDE0_SLAVE_IMG, false);
}

