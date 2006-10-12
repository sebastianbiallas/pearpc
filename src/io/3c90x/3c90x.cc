/*
 *	PearPC
 *	3c90x.cc
 *
 *	3Com 3C905C Emulation
 *	References:
 *	[1] 3c90xc.pdf ("3C90xC NICs Technical Reference" 3Com(r) part number 89-0931-000)
 *	[2] Linux Kernel 2.4.22 (drivers/net/3c59x.c)
 *
 *	Copyright (C) 2004 John Kelley (pearpc@kelley.ca)
 *	Copyright (C) 2003 Stefan Weyergraf
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
#include <unistd.h>

#include "system/sys.h"
#include "system/systhread.h"
#include "cpu/debug.h"
#include "cpu/mem.h"
#include "system/sysethtun.h"
#include "system/arch/sysendian.h"
#include "tools/crc32.h"
#include "tools/data.h"
#include "tools/endianess.h"
#include "tools/except.h"
#include "tools/snprintf.h"
#include "io/pic/pic.h"
#include "io/pci/pci.h"
#include "debug/tracers.h"
#include "3c90x.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define MAX_PACKET_SIZE		16384

enum Command {
	CmdTotalReset = 0<<11,
	CmdSelectWindow = 1<<11,
	CmdEnableDC = 2<<11,			// CmdStartCoax
	CmdRxDisable = 3<<11,
	CmdRxEnable = 4<<11,
	CmdRxReset = 5<<11,
	CmdStall = 6<<11,
	CmdTxDone = 7<<11,
	CmdRxDiscard = 8<<11,
	CmdTxEnable = 9<<11,
	CmdTxDisable = 10<<11,
	CmdTxReset = 11<<11,
	CmdReqIntr = 12<<11,			// CmdFakeIntr
	CmdAckIntr = 13<<11,
	CmdSetIntrEnb = 14<<11,
	CmdSetIndicationEnable = 15<<11,	// CmdSetStatusEnb
	CmdSetRxFilter = 16<<11,
	CmdSetRxEarlyThresh = 17<<11,
	CmdSetTxThreshold = 18<<11,		// aka TxAgain ?
	CmdSetTxStartThresh = 19<<11,		// set TxStartTresh
//	CmdStartDMAUp = 20<<11,
//	CmdStartDMADown = (20<<11)+1,
	CmdStatsEnable = 21<<11,
	CmdStatsDisable = 22<<11,
	CmdDisableDC = 23<<11,			// CmdStopCoax
	CmdSetTxReclaimThresh = 24<<11,
	CmdSetHashFilterBit = 25<<11
};

/*
 *	IntStatusBits
 */
enum IntStatusBits {
	IS_interruptLatch =	1<<0,
	IS_hostError =		1<<1,
	IS_txComplete =		1<<2,
	/* bit 3 is unspecified */
	IS_rxComplete =		1<<4,
	IS_rxEarly =		1<<5,
	IS_intRequested =	1<<6,
	IS_updateStats =	1<<7,
	IS_linkEvent =		1<<8,
	IS_dnComplete =		1<<9,
	IS_upComplete =		1<<10,
	IS_cmdInProgress =	1<<11,
	/* bit 12 is unspecified */
	/* [15:13] is currently selected window */
};

/*
 *	DmaCtrlBits ([1] p.96)
 */
enum DmaCtrlBits {
	/* bit 0 unspecified */
	DC_dnCmplReq =		1<<1,
	DC_dnStalled =		1<<2,
	DC_upComplete =		1<<3,	// FIXME: same as in IntStatus, but always visible
	DC_dnComplete =		1<<4,	// same as above ^^^
	DC_upRxEarlyEnable =	1<<5,
	DC_armCountdown =	1<<6,
	DC_dnInProg =		1<<7,
	DC_counterSpeed =	1<<8,
	DC_countdownMode =	1<<9,
	/* bits 10-15 unspecified */
	DC_upAltSeqDisable =	1<<16,
	DC_dnAltSeqDisable =	1<<17,
	DC_defeatMWI =		1<<20,
	DC_defeatMRL =		1<<21,
	DC_upOverDiscEnable =	1<<22,
	DC_targetAbort =	1<<30,
	DC_masterAbort =	1<<31
};

/*
 *	MII Registers
 */
/*enum MIIControlBits {
	MIIC_collision =	1<<7,
	MIIC_fullDuplex =	1<<8,
	MIIC_restartNegote =	1<<9,
	MIIC_collision =	1<<7,
	rest missing
};*/

struct MIIRegisters {
	uint16 control;
	uint16 status;
	uint16 id0;
	uint16 id1;
	uint16 advert;
	uint16 linkPartner;
	uint16 expansion;
	uint16 nextPage;
} PACKED;

/*
 *	Registers
 */
struct RegWindow {
	byte b[16];
	uint16 u16[8];
};

struct Registers {
	// 0x10 bytes missing (current window)
	uint32	r0;
	uint32	r1;
	uint8	TxPktId;
	uint8	r2;
	uint8	Timer;
	uint8	TxStatus;
	uint16	r3;
	uint16	__dontUseMe;//	really:	uint16	IntStatusAuto;
	uint32	DmaCtrl;		// [1] p.95 (dn), p.100 (up)
	uint32	DnListPtr;	// [1] p.98
	uint16	r4;
	uint8	DnBurstThresh;	// [1] p.97
	uint8	r5;
	uint8	DnPriorityThresh;
	uint8	DnPoll;		// [1] p.100
	uint16	r6;
	uint32	UpPktStatus;
	uint16	FreeTimer;
	uint16	Countdown;
	uint32	UpListPtr;	// [1] p.115
	uint8	UpPriorityThresh;
	uint8	UpPoll;
	uint8	UpBurstThresh;
	uint8	r7;
	uint32	RealTimeCount;
	uint8	ConfigAddress;
	uint8	r8;
	uint8	r9;
	uint8	r10;
	uint8	ConfigData;
	uint8	r11;
	uint8	r12;
	uint8	r13;
	uint32	r14[9];
	uint32	DebugData;
	uint16	DebugControl;
	uint16	r15;
	uint16	DnMaxBurst;
	uint16	UpMaxBurst;
	uint16	PowerMgmtCtrl;
	uint16	r16;
} PACKED;

#define RA_INV	0

static byte gRegAccess[0x70] =
{
/* 0x10 */
	RA_INV, RA_INV, RA_INV, RA_INV,
/* 0x14 */
	RA_INV, RA_INV, RA_INV, RA_INV, 
/* 0x18 */
	1,				/* TxPktId */
	RA_INV,
	1, 				/* Timer */
	1,				/* TxStatus */
/* 0x1c */
	RA_INV, RA_INV,
	RA_INV, RA_INV,			/* IntStatusAuto */
/* 0x20 */
	4, RA_INV, RA_INV, RA_INV,	/* DmaCtrl */
/* 0x24 */
	4, RA_INV, RA_INV, RA_INV,	/* DnListPtr */
/* 0x28 */
	RA_INV, RA_INV,
	1,				/* DnBurstThresh */
	RA_INV,
/* 0x2c */
	1,				/* DnPriorityThresh */
	1,				/* DnPoll */
	RA_INV,
	1,
/* 0x30 */
	4, RA_INV, RA_INV, RA_INV,	/* UpPktStatus */
/* 0x34 */
	2, RA_INV,			/* FreeTimer */
	2, RA_INV,			/* Countdown */
/* 0x38 */
	4, RA_INV, RA_INV, RA_INV,	/* UpListPtr */
/* 0x3c */
	1,				/* UpPriorityThresh */
	1,				/* UpPoll */
	1,				/* UpBurstThresh */
	RA_INV,
/* 0x40 */
	4, RA_INV, RA_INV, RA_INV,	/* RealTimeCount */
/* 0x44 */
	1,				/* ConfigAddress */
	RA_INV,
	RA_INV,
	RA_INV,
/* 0x48 */
	1,				/* ConfigData */
	RA_INV,
	RA_INV,
	RA_INV,
/* 0x4c */
	RA_INV, RA_INV, RA_INV, RA_INV,
/* 0x50 */
	RA_INV, RA_INV, RA_INV, RA_INV,
	RA_INV, RA_INV, RA_INV, RA_INV,
	RA_INV, RA_INV, RA_INV, RA_INV,
	RA_INV, RA_INV, RA_INV, RA_INV,
	RA_INV, RA_INV, RA_INV, RA_INV,
	RA_INV, RA_INV, RA_INV, RA_INV,
	RA_INV, RA_INV, RA_INV, RA_INV,
	RA_INV, RA_INV, RA_INV, RA_INV,
/* 0x70 */
	4, RA_INV, RA_INV, RA_INV,	/* DebugData */
/* 0x74 */
	2, RA_INV,			/* DebugControl */
	RA_INV, RA_INV,
/* 0x78 */
	2, RA_INV,			/* DnMaxBurst */
	2, RA_INV,			/* UpMaxBurst */
/* 0x7c */
	2, RA_INV,			/* PowerMgmtCtrl */
	RA_INV, RA_INV
};

/*
 *	Window 0
 */
struct RegWindow0 {
	uint32	r0;
	uint32	BiosRomAddr;
	uint8	BiosRomData;
	uint8	r1;
	uint16	EepromCommand;
	uint16	EepromData;
	uint16	XXX;		// IntStatus/CommandRegister
} PACKED;

enum W0_Offsets {
	W0_EEPROMCmd = 0xa,
	W0_EEPROMData = 0xc
};

enum W0_EEPROMOpcode {
	EEOP_SubCmd = 0<<6,
	EEOP_WriteReg = 1<<6,
	EEOP_ReadReg = 2<<6,
	EEOP_EraseReg = 3<<6
};

enum W0_EEPROMSubCmd {
	EESC_WriteDisable = 0<<4,
	EESC_WriteAll = 1<<4,
	EESC_EraseAll = 2<<4,
	EESC_WriteEnable = 3<<4
};

/*
 *	Window 2
 */
struct RegWindow2 {
	uint16	StationAddress[6];
	uint16	StationMask[6];
	uint16	ResetOptions;
	uint16	XXX;		// IntStatus/CommandRegister
} PACKED;

/*
 *	Window 3
 */
struct RegWindow3 {
	uint32	InternalConfig;	// [1] p.58,76
	uint16	MaxPktSize;
	uint16	MacControl;	// [1] p.179
	uint16	MediaOptions;	// [1] p.78 (EE), p.181
	uint16	RxFree;
	uint16	TxFree;		// [1] p.101
	uint16	XXX;		// IntStatus/CommandRegister
} PACKED;

/*
 *	Window 4
 */
enum W4_PhysMgmtBits {
	PM_mgmtClk	= 1<<0,
	PM_mgmtData	= 1<<1,
	PM_mgmtDir	= 1<<2
};

struct RegWindow4 {
	uint16	r0;
	uint16	r1;
	uint16	FifoDiagnostic;
	uint16	NetDiagnostic;	// [1] p.184
	uint16	PhysMgmt;	// [1] p.186
	uint16	MediaStatus;	// [1] p.182
	byte	BadSSD;
	byte	UpperBytesOK;
	uint16	XXX;		// IntStatus/CommandRegister
} PACKED;

/*
 *	Window 5
 */
enum RxFilterBits {	// [1] p.112
	RXFILT_receiveIndividual = 1,
	RXFILT_receiveMulticast = 2,
	RXFILT_receiveBroadcast = 4,
	RXFILT_receiveAllFrames = 8,
	RXFILT_receiveMulticastHash = 16
};
 
struct RegWindow5 {
	uint16	TxStartThresh;
	uint16	r0;
	uint16	r1;
	uint16	RxEarlyThresh;
	byte	RxFilter;	// [1] p.112
	byte	TxReclaimThresh;
	uint16	InterruptEnable;	// [1] p.120
	uint16	IndicationEnable;// [1] p.120
	uint16	XXX;		// IntStatus/CommandRegister
} PACKED;

/*
 *	Window 6
 */
struct RegWindow6 {
	uint8	CarrierLost;
	uint8	SqeErrors;
	uint8	MultipleCollisions;
	uint8	SingleCollisions;
	uint8	LateCollisions;
	uint8	RxOverruns;
	uint8	FramesXmittedOk;
	uint8	FramesRcvdOk;
	uint8	FramesDeferred;
	uint8	UpperFramesOk;
	uint16	BytesRcvdOk;
	uint16	BytesXmittedOk;
	uint16	XXX;		// IntStatus/CommandRegister
} PACKED;

/*
 *	EEPROM
 */
enum EEPROMField {
	EEPROM_NodeAddress0 =		0x00,
	EEPROM_NodeAddress1 =		0x01,
	EEPROM_NodeAddress2 =		0x02,
	EEPROM_DeviceID =		0x03,
	EEPROM_ManifacturerID =		0x07,
	EEPROM_PCIParam =		0x08,
	EEPROM_RomInfo =		0x09,
	EEPROM_OEMNodeAddress0 =	0x0a,
	EEPROM_OEMNodeAddress1 =	0x0b,
	EEPROM_OEMNodeAddress2 =	0x0c,
	EEPROM_SoftwareInfo =		0x0d,
	EEPROM_CompWord =		0x0e,
	EEPROM_SoftwareInfo2 =		0x0f,
	EEPROM_Caps =			0x10,
	EEPROM_InternalConfig0 =	0x12,
	EEPROM_InternalConfig1 =	0x13,
	EEPROM_SubsystemVendorID =	0x17,
	EEPROM_SubsystemID =		0x18,
	EEPROM_MediaOptions =		0x19,
	EEPROM_SmbAddress =		0x1b,
	EEPROM_PCIParam2 =		0x1c,
	EEPROM_PCIParam3 =		0x1d,
	EEPROM_Checksum =		0x20
};

/*
 *	Up/Downloading
 */
 
// must be on 8-byte physical address boundary
struct DPD0 {
	uint32	DnNextPtr;
	uint32	FrameStartHeader;
/*	DPDFragDesc Frags[n] */
};

enum FrameStartHeaderBits {
	FSH_rndupBndry =	3<<0,
	FSH_pktId =		15<<2,
	/* 12:10 unspecified */
	FSH_crcAppendDisable =	1<<13,
	FSH_txIndicate =	1<<15,
	FSH_dnComplete =	1<<16,
	FSH_reArmDisable =	1<<23,
	FSH_lastKap =		1<<24,
	FSH_addIpChecksum =	1<<25,
	FSH_addTcpChecksum =	1<<26,
	FSH_addUdpChecksum =	1<<27,
	FSH_rndupDefeat =	1<<28,
	FSH_dpdEmpty =		1<<29,
	/* 30 unspecified */
	FSH_dnIndicate =	1<<31
};

// must be on 16-byte physical address boundary
struct DPD1 {
	uint32	DnNextPtr;
	uint32	ScheduleTime;
	uint32	FrameStartHeader;
	uint32	res;
/*	DPDFragDesc Frags[n] */
};

struct DPDFragDesc {
	uint32	DnFragAddr;
	uint32	DnFragLen;	// [12:0] fragLen, [31] lastFrag
} PACKED;

// must be on 8-byte physical address boundary
struct UPD {
	uint32	UpNextPtr;
	uint32	UpPktStatus;
/*	UPDFragDesc Frags[n] */
};

struct UPDFragDesc {
	uint32	UpFragAddr;
	uint32	UpFragLen;	// [12:0] fragLen, [31] lastFrag
} PACKED;

#define MAX_DPD_FRAGS	63
#define MAX_UPD_FRAGS	63
#define MAX_UPD_SIZE	(sizeof(UPD) + sizeof(UPDFragDesc)*MAX_UPD_FRAGS) // 512

enum UpPktStatusBits {
	UPS_upPktLen = 0x1fff,
	/* 13 unspecified */
	UPS_upError = 1<<14,
	UPS_upComplete = 1<<15,
	UPS_upOverrun = 1<<16,
	UPS_runtFrame = 1<<17,
	UPS_alignmentError = 1<<18,
	UPS_crcError = 1<<19,
	UPS_oversizedFrame = 1<<20,
	/* 22:21 unspecified */
	UPS_dribbleBits = 1<<23,
	UPS_upOverflow = 1<<24,
	UPS_ipChecksumError = 1<<25,
	UPS_tcpChecksumError = 1<<26,
	UPS_udpChecksumError = 1<<27,
	UPD_impliedBufferEnable = 1<<28,
	UPS_ipChecksumChecked = 1<<29,
	UPS_tcpChecksumChecked = 1<<30,
	UPS_udpChecksumChecked = 1<<31
};

// IEEE 802.3 MAC, Ethernet-II
struct EthFrameII {
	byte	destMAC[6];
	byte	srcMAC[6];
	byte	type[2];
} PACKED;

/*
 *	misc
 */
static int compareMACs(byte a[6], byte b[6])
{
	for (uint i = 0; i < 6; i++) {
		if (a[i] != b[i]) return a[i] - b[i];
	}
	return 0;
}

/*
 *
 */
class _3c90x_NIC: public PCI_Device {
protected:
	uint16		mEEPROM[0x40];
	bool		mEEPROMWritable;
	Registers	mRegisters;
	RegWindow	mWindows[8];
	uint16		mIntStatus;
	bool		mRxEnabled;
	bool		mTxEnabled;
	bool		mUpStalled;
	bool		mDnStalled;
	byte		mRxPacket[MAX_PACKET_SIZE];
	uint		mRxPacketSize;
	EthTunDevice *	mEthTun;
	sys_mutex	mLock;
	
	union {
		MIIRegisters s;
		uint16       reg[8];
	} 		mMIIRegs;

	uint32		mMIIReadWord;
	uint64		mMIIWriteWord;
	uint		mMIIWrittenBits;
	uint16		mLastHiClkPhysMgmt;
	byte		mMAC[6];

void PCIReset()
{
	// PCI config
	memset(mConfig, 0, sizeof mConfig);
	// 0-3 set by totalReset()
//	mConfig[0x04] = 0x07;	// io+memory+master

	mConfig[0x08] = 0x00;	// revision
	mConfig[0x09] = 0x00; 	//
	mConfig[0x0a] = 0x00;	// ClassCode 0x20000: Ethernet network controller
	mConfig[0x0b] = 0x02;	//

	mConfig[0x0e] = 0x00;	// header-type (single-function PCI device)

	mConfig[0x3c] = IO_PIC_IRQ_ETHERNET0;	// interrupt line
	mConfig[0x3d] = 1;	// interrupt pin (default is 1)
	mConfig[0x3e] = 5;	// MinGnt (default is 5 = 0x05 = 0101b)
	mConfig[0x3f] = 48;	// MaxLat (default is 48 = 0x30 = 110000b)

	mConfig[0x34] = 0xdc;

	mIORegSize[0] = 0x100;
	mIORegType[0] = PCI_ADDRESS_SPACE_IO;
	assignIOPort(0, 0x1000);
}

void totalReset()
{
	/* FIXME: resetting can be done more fine-grained (see TotalReset cmd).
	 *        this is reset ALL regs.
	 */
	if (sizeof (Registers) != 0x70) {
		IO_3C90X_ERR("sizeof Registers = %08x/%d\n", sizeof (Registers), sizeof (Registers));
	}

	RegWindow3 &w3 = (RegWindow3&)mWindows[3];
	RegWindow5 &w5 = (RegWindow5&)mWindows[5];

	// internals
	mEEPROMWritable = false;
	memset(&mWindows, 0, sizeof mWindows);
	memset(&mRegisters, 0, sizeof mRegisters);
	mIntStatus = 0;
	mRxEnabled = false;
	mTxEnabled = false;
	mUpStalled = false;
	mDnStalled = false;
	w3.MaxPktSize = 1514 /* FIXME: should depend on sizeof mRxPacket*/;
	w3.RxFree = 16*1024;
	w3.TxFree = 16*1024;
	mRxPacketSize = 0;
	w5.TxStartThresh = 8188;
	memset(mEEPROM, 0, sizeof mEEPROM);
	mEEPROM[EEPROM_NodeAddress0] =		(mMAC[0]<<8) | mMAC[1];
	mEEPROM[EEPROM_NodeAddress1] =		(mMAC[2]<<8) | mMAC[3];
	mEEPROM[EEPROM_NodeAddress2] =		(mMAC[4]<<8) | mMAC[5];
	mEEPROM[EEPROM_DeviceID] =		0x9200;
	mEEPROM[EEPROM_ManifacturerID] =	0x6d50;
	mEEPROM[EEPROM_PCIParam] =		0x2940;
	mEEPROM[EEPROM_RomInfo] =		0;	// no ROM
	mEEPROM[EEPROM_OEMNodeAddress0] =	mEEPROM[EEPROM_NodeAddress0];
	mEEPROM[EEPROM_OEMNodeAddress1] =	mEEPROM[EEPROM_NodeAddress1];
	mEEPROM[EEPROM_OEMNodeAddress2] =	mEEPROM[EEPROM_NodeAddress2];
	mEEPROM[EEPROM_SoftwareInfo] =		0x4010;
	mEEPROM[EEPROM_CompWord] =		0;
	mEEPROM[EEPROM_SoftwareInfo2] =		0x00aa;
	mEEPROM[EEPROM_Caps] =			0x72a2;
	mEEPROM[EEPROM_InternalConfig0] =	0;
	mEEPROM[EEPROM_InternalConfig1] =	0x0050;	// default is 0x0180
	mEEPROM[EEPROM_SubsystemVendorID] =	0x10b7;
	mEEPROM[EEPROM_SubsystemID] =		0x9200;
	mEEPROM[EEPROM_MediaOptions] =		0x000a;
	mEEPROM[EEPROM_SmbAddress] =		0x6300;
	mEEPROM[EEPROM_PCIParam2] =		0xffb7;
	mEEPROM[EEPROM_PCIParam3] =		0xb7b7;
	mEEPROM[EEPROM_Checksum] =		0;

	// MII
	memset(&mMIIRegs, 0, sizeof mMIIRegs);
	mMIIRegs.s.status = (1<<14) | (1<<13) | (1<<12) | (1<<11) | (1<<5) | (1<<3) | (1<<2) | 1;
	mMIIRegs.s.linkPartner = (1<<14) | (1<<7) | 1;
	mMIIRegs.s.advert = (1<<14) | (1 << 10) | (1<<7) | 1;
	mMIIReadWord = 0;
	mMIIWriteWord = 0;
	mMIIWrittenBits = 0;
	mLastHiClkPhysMgmt = 0;

	// Register follow-ups
	w3.MediaOptions = mEEPROM[EEPROM_MediaOptions];
	w3.InternalConfig = mEEPROM[EEPROM_InternalConfig0] |
		(mEEPROM[EEPROM_InternalConfig1] << 16);

	// PCI config follow-ups
	mConfig[0x00] = mEEPROM[EEPROM_SubsystemVendorID] & 0xff;	// vendor ID
	mConfig[0x01] = mEEPROM[EEPROM_SubsystemVendorID] >> 8;
	mConfig[0x02] = mEEPROM[EEPROM_DeviceID] & 0xff;	// unit ID
	mConfig[0x03] = mEEPROM[EEPROM_DeviceID] >> 8;
}

void readRegWindow(uint window, uint32 port, uint32 &data, uint size)
{
	IO_3C90X_TRACE("readRegWindow(%d, %08x, %08x)\n", window, port, size);
	switch (window) {
	/* window 0 */
	case 0: {
		RegWindow0 &w0 = (RegWindow0&)mWindows[0];
		switch (port) {
		case W0_EEPROMCmd: {
			if (size != 2) {
				IO_3C90X_WARN("EepromCommand, size != 2\n");
				SINGLESTEP("");
			}
			data = w0.EepromCommand;
			break;
		}
		case W0_EEPROMData: {
			if (size != 2) {
				IO_3C90X_WARN("EepromData, size != 2\n");
				SINGLESTEP("");
			}
			data = w0.EepromData;
			break;
		}
		default:
			IO_3C90X_WARN("reading here unimpl.0\n");
			SINGLESTEP("");
			break;
		}
		break;
	}
	/* window 1 */
	case 1: {
		data = 0;
		//RegWindow1 &w1 = (RegWindow1&)mWindows[1];
		memcpy(&data, &mWindows[1].b[port], size);
		break;
	}
	/* window 2 */
	case 2: {
		data = 0;
		//RegWindow2 &w2 = (RegWindow2&)mWindows[2];
		memcpy(&data, &mWindows[2].b[port], size);
		break;
	}
	/* window 3 */
	case 3: {
		data = 0;
		//RegWindow3 &w3 = (RegWindow3&)mWindows[3];
		memcpy(&data, &mWindows[3].b[port], size);
		break;
	}
	/* window 4 */
	case 4: {
		RegWindow4 &w4 = (RegWindow4&)mWindows[4];
		data = 0;
		switch (port) {
		case 8: {
			// MII-interface
			if (size != 2) {
				IO_3C90X_WARN("alignment.4.8.read\n");
				SINGLESTEP("");
			}
			bool mgmtData = mMIIReadWord & 0x80000000;
//			IO_3C90X_TRACE("Read cycle mgmtData=%d\n", mgmtData ? 1 : 0);
			if (mgmtData) {
				data = w4.PhysMgmt | PM_mgmtData;
			} else {
				data = w4.PhysMgmt & (~PM_mgmtData);
			}
/*			IO_3C90X_TRACE("read PhysMgmt = %04x (mgmtData = %d)\n",
				data, mgmtData ? 1 : 0);*/
			break;
		}
		case 0xc: {
			if (size != 1) {
				IO_3C90X_WARN("alignment.4.c.read\n");
			}
			// reading clears
			w4.BadSSD = 0;
			memcpy(&data, &mWindows[4].b[port], size);
			break;
		}
		default:
			memcpy(&data, &mWindows[4].b[port], size);
		}
		break;
	}
	/* Window 5 */
	case 5: {
		data = 0;
		//RegWindow5 &w5 = (RegWindow5&)mWindows[5];
		memcpy(&data, &mWindows[5].b[port], size);
		break;
	}
	/* Window 6 */
	case 6: {
		RegWindow6 &w6 = (RegWindow6&)mWindows[6];
		// reading clears
		if ((port == 0xa) && (size == 2)) {
			// FIXME: BytesRcvdOk really is 20 bits !
			// when reading here, write upper 4 bits
			// in w4.UpperBytesOk[3:0]. no clearing.
			w6.BytesRcvdOk = 0;
		} else if ((port == 0xc) && (size == 2)) {
			// FIXME: BytesXmittedOk really is 20 bits !
			// when reading here, write upper 4 bits
			// in w4.UpperBytesOk[7:4]. no clearing.
			w6.BytesXmittedOk = 0;
		} else if ((port == 0) && (size == 1)) {
			w6.CarrierLost = 0;
		} else if ((port == 8) && (size == 1)) {
			w6.FramesDeferred = 0;
		} else if ((port == 7) && (size == 1)) {
			// FIXME: FramesRcvdOk really is 10 bits !
			// when reading here, write upper 2 bits
			// in w6.UpperFramesOk[1:0]. no clearing.
		} else if ((port == 6) && (size == 1)) {
			// FIXME: FramesXmittedOk really is 10 bits !
			// when reading here, write upper 2 bits
			// in w6.UpperFramesOk[5:4]. no clearing.
		} else if ((port == 4) && (size == 1)) {
			w6.LateCollisions = 0;
		} else if ((port == 2) && (size == 1)) {
			w6.MultipleCollisions = 0;
		} else if ((port == 5) && (size == 1)) {
			w6.RxOverruns = 0;
		} else if ((port == 3) && (size == 1)) {
			w6.SingleCollisions = 0;
		} else if ((port == 1) && (size == 1)) {
			w6.SqeErrors = 0;
		}
		data = 0;
		memcpy(&data, &mWindows[6].b[port], size);
		break;
	}
	/* Window 7 */
	case 7: {
		data = 0;
		//RegWindow7 &w7 = (RegWindow7&)mWindows[7];
		memcpy(&data, &mWindows[7].b[port], size);
		break;
	}
	default:
		IO_3C90X_WARN("reading here unimpl.\n");
		SINGLESTEP("");
	}
	IO_3C90X_TRACE("= %04x\n", data);
}

void writeRegWindow(uint window, uint32 port, uint32 data, uint size)
{
	IO_3C90X_TRACE("writeRegWindow(%d, %08x, %08x, %08x)\n", window, port, data, size);
	switch (window) {
	/* Window 0 */
	case 0: {
		RegWindow0 &w0 = (RegWindow0&)mWindows[0];
		switch (port) {
		case W0_EEPROMCmd: {
			if (size != 2) {
				IO_3C90X_WARN("EepromCommand, size != 2\n");
				SINGLESTEP("");
			}
			w0.EepromCommand = data & 0xff7f;	// clear eepromBusy
			uint eeprom_addr =  ((data >> 2) & 0xffc0) | (data & 0x3f);
			switch (data & 0xc0) {
			case EEOP_SubCmd:
				switch (data & 0x30) {
				case EESC_WriteDisable:
					IO_3C90X_TRACE("EESC_WriteDisable\n");
					mEEPROMWritable = false;
					break;
				case EESC_WriteAll:
					IO_3C90X_WARN("WriteAll not impl.\n");
					SINGLESTEP("");
					memset(mEEPROM, 0xff, sizeof mEEPROM);
					mEEPROMWritable = false;
					break;
				case EESC_EraseAll:
					IO_3C90X_WARN("EraseAll not impl.\n");
					SINGLESTEP("");
					memset(mEEPROM, 0, sizeof mEEPROM);
					mEEPROMWritable = false;
					break;
				case EESC_WriteEnable:
					IO_3C90X_TRACE("EESC_WriteEnable\n");
					mEEPROMWritable = true;
					break;
				default:
					IO_3C90X_WARN("impossible\n");
					SINGLESTEP("");
				}
				break;
			case EEOP_WriteReg:
				if (mEEPROMWritable) {
					if (eeprom_addr*2 < sizeof mEEPROM) {
						// disabled
						IO_3C90X_WARN("EEOP_WriteReg(addr = %04x, %04x) oldvalue = %04x\n", eeprom_addr, w0.EepromData, mEEPROM[eeprom_addr]);
						SINGLESTEP("");
						mEEPROM[eeprom_addr] = w0.EepromData;
					} else {
						IO_3C90X_WARN("FAILED(out of bounds): EEOP_WriteReg(addr = %04x, %04x) oldvalue = %04x\n", eeprom_addr, w0.EepromData, mEEPROM[eeprom_addr]);
						SINGLESTEP("");
					}
					mEEPROMWritable = false;
				} else {
					IO_3C90X_WARN("FAILED(not writable): EEOP_WriteReg(addr = %04x, %04x) oldvalue = %04x\n", eeprom_addr, w0.EepromData, mEEPROM[eeprom_addr]);
					SINGLESTEP("");
				}
				break;
			case EEOP_ReadReg:
				if (eeprom_addr*2 < sizeof mEEPROM) {
					w0.EepromData = mEEPROM[eeprom_addr];
					IO_3C90X_TRACE("EEOP_ReadReg(addr = %04x) = %04x\n", eeprom_addr, w0.EepromData);
				} else {
					IO_3C90X_WARN("FAILED(out of bounds): EEOP_ReadReg(addr = %04x)\n", eeprom_addr);
					SINGLESTEP("");
				}
				break;
			case EEOP_EraseReg:
				if (mEEPROMWritable) {
					if (eeprom_addr*2 < sizeof mEEPROM) {
						// disabled
						IO_3C90X_WARN("EEOP_EraseReg(addr = %04x) oldvalue = %04x\n", eeprom_addr, mEEPROM[eeprom_addr]);
						SINGLESTEP("");
						mEEPROM[eeprom_addr] = 0;
					} else {
						IO_3C90X_WARN("FAILED(out of bounds): EEOP_EraseReg(addr = %04x) oldvalue = %04x\n", eeprom_addr, mEEPROM[eeprom_addr]);
						SINGLESTEP("");
					}
					mEEPROMWritable = false;
				} else {
					IO_3C90X_WARN("FAILED(not writable): EEOP_EraseReg(addr = %04x) oldvalue = %04x\n", eeprom_addr, mEEPROM[eeprom_addr]);
					SINGLESTEP("");
				}
				break;
			default:
				IO_3C90X_WARN("impossible\n");
				SINGLESTEP("");
			}
			break;
		}
		case W0_EEPROMData:
			if (size != 2) {
				IO_3C90X_WARN("EepromData, size != 2\n");
				SINGLESTEP("");
			}
			w0.EepromData = data;
			break;
		default:
			IO_3C90X_WARN("writing here unimpl.0\n");
			SINGLESTEP("");
			break;
		}
		break;
	}
	/* Window 2 */
	case 2: {
//		RegWindow2 &w2 = (RegWindow2&)mWindows[2];
		if (port+size<=0xc) {
			IO_3C90X_TRACE("StationAddress or StationMask\n");
			/* StationAddress or StationMask */
			memcpy(&mWindows[2].b[port], &data, size);
		} else {
			IO_3C90X_WARN("writing here unimpl.2\n");
			SINGLESTEP("");
		}
		break;
	}
	/* Window 3 */
	case 3: {
/*	uint32	InternalConfig PACKED;
	uint16	MaxPktSize PACKED;
	uint16	MacControl PACKED;
	uint16	MediaOptions PACKED;
	uint16	RxFree PACKED;
	uint16	TxFree PACKED;*/
		RegWindow3 &w3 = (RegWindow3&)mWindows[3];
		switch (port) {
		case 0:
			if (size != 4) {
				IO_3C90X_WARN("alignment.3.0\n");
				SINGLESTEP("");
			}
			IO_3C90X_TRACE("InternalConfig\n");
			w3.InternalConfig = data;
			break;
		case 4:
			if (size != 2) {
				IO_3C90X_WARN("alignment.3.4\n");
				SINGLESTEP("");
			}
			IO_3C90X_ERR("MaxPktSize\n");
			w3.MaxPktSize = data;
			break;
		case 6:
			if (size != 2) {
				IO_3C90X_WARN("alignment.3.6\n");
				SINGLESTEP("");
			}
			IO_3C90X_TRACE("MacControl\n");
			if (data != 0) {
				IO_3C90X_WARN("setting MacControl != 0\n");
				SINGLESTEP("");
			}
			w3.MacControl = data;
			break;
		case 8:
			if (size != 2) {
				IO_3C90X_WARN("alignment.3.8\n");
				SINGLESTEP("");
			}
			IO_3C90X_TRACE("MediaOptions\n");
			w3.MediaOptions = data;
			break;
		case 10:
			if (size != 2) {
				IO_3C90X_WARN("alignment.3.10\n");
				SINGLESTEP("");
			}
			IO_3C90X_WARN("RxFree\n");
			SINGLESTEP("");
			w3.RxFree = data;
			break;
		case 12:
			if (size != 2) {
				IO_3C90X_WARN("alignment.3.12\n");
				SINGLESTEP("");
			}
			IO_3C90X_WARN("TxFree\n");
			SINGLESTEP("");
			w3.TxFree = data;
			break;
		default:
			IO_3C90X_WARN("writing here unimpl.3\n");
			SINGLESTEP("");
		}
		break;
	}
	/* Window 4 */
	case 4: {
		RegWindow4 &w4 = (RegWindow4&)mWindows[4];
		switch (port) {
		case 6: {
			if (size != 2) {
				IO_3C90X_WARN("alignment.4.6\n");
				SINGLESTEP("");
			}
			uint mask = 0xf341;
			IO_3C90X_TRACE("NetDiagnostic = %04x, old = %04x\n", ((w4.NetDiagnostic)&~mask)|(data&mask), w4.NetDiagnostic);
			w4.NetDiagnostic &= ~mask;
			w4.NetDiagnostic |= data & mask;
			break;
		}
		case 8: {
			// MII-interface
			if (size != 2) {
				IO_3C90X_WARN("alignment.4.8\n");
				SINGLESTEP("");
			}
/*			IO_3C90X_TRACE("PhysMgmt = %04x (clk=%d, data=%d, dir=%d), old = %04x\n",
				data, (data & PM_mgmtClk) ? 1 : 0, (data & PM_mgmtData) ? 1 : 0,
					(data & PM_mgmtDir) ? 1 : 0, w4.PhysMgmt);*/
			bool hiClk = !(w4.PhysMgmt & PM_mgmtClk) && (data & PM_mgmtClk);
			if (hiClk) {
				// Z means lo edge of mgmtDir
				bool Z = (mLastHiClkPhysMgmt & PM_mgmtDir) && !(data & PM_mgmtDir);
//				IO_3C90X_TRACE("hi-edge, Z=%d\n", Z ? 1 : 0);
				if (Z) {
//					IO_3C90X_TRACE("Z-cycle, %016qx, written bits=%d\n", &mMIIWriteWord, mMIIWrittenBits);
					// check if the 5 frames have been sent
					if (((mMIIWriteWord >> (mMIIWrittenBits-32-2)) & 0x3ffffffffULL) == 0x3fffffffdULL) {
						uint opcode = (mMIIWriteWord >> (mMIIWrittenBits-32-2-2)) & 3;
						uint PHYaddr = (mMIIWriteWord >> (mMIIWrittenBits-32-2-2-5)) & 0x1f;
						uint REGaddr = (mMIIWriteWord >> (mMIIWrittenBits-32-2-2-5-5)) & 0x1f;
//						IO_3C90X_TRACE("prefixed Z-cycle, opcode=%d, PHY=%02x, REG=%02x\n", opcode, PHYaddr, REGaddr);
						if ((PHYaddr == 0x18 /* hardcoded address [1] p.196 */)
						&& (REGaddr < 0x10)) {
							switch (opcode) {
							case 1: {
								// Opcode Write
								IO_3C90X_TRACE("Opcode Write\n");
								if (mMIIWrittenBits == 64) {
									uint32 value = mMIIWriteWord & 0xffff;
//									IO_3C90X_TRACE("NOT writing 0x%04x to register. feature disabled. (old = 0x%04x)\n", value, mMIIRegs[REGaddr]);
									IO_3C90X_TRACE("Writing 0x%04x to MII register %d (old = 0x%04x)\n", value, REGaddr, mMIIRegs.reg[REGaddr]);
									mMIIRegs.reg[REGaddr] = value;
								} else {
									IO_3C90X_TRACE("But invalid write count=%d\n", mMIIWrittenBits);
								}
								mMIIWriteWord = 0;
								break;
							}
							case 2: {
								// Opcode Read
								IO_3C90X_TRACE("Opcode Read\n");
								if (mMIIWrittenBits == 32+2+2+5+5) {
									// msb gets sent first and is zero to indicated success
									// the register to be sent follows msb to lsb
									mMIIReadWord = mMIIRegs.reg[REGaddr] << 15;
									IO_3C90X_TRACE("Read 0x%04x from register %d\n", mMIIRegs.reg[REGaddr], REGaddr);
								} else {
									IO_3C90X_TRACE("But invalid write count=%d\n", mMIIWrittenBits);
								}
								mMIIWriteWord = 0;
								break;
							}
							default:
								// error
								IO_3C90X_TRACE("Invalid opcode %d\n", (mMIIWriteWord >> 10) & 3);
								mMIIReadWord = 0xffffffff;
							}
						} else {
							// error
							IO_3C90X_TRACE("Invalid PHY or REG\n");
							mMIIReadWord = 0xffffffff;
						}
					}
					mMIIWrittenBits = 0;
					w4.PhysMgmt = data;
				} else if (data & PM_mgmtDir) {
					// write
					bool mgmtData = data & PM_mgmtData;
//					IO_3C90X_TRACE("Write cycle mgmtData=%d\n", mgmtData ? 1 : 0);
					w4.PhysMgmt = data;
					mMIIWriteWord <<= 1;
					mMIIWriteWord |= mgmtData ? 1 : 0;
					mMIIWrittenBits++;
				} else {
					// read
					bool mgmtData = mMIIReadWord & 0x80000000;
//					IO_3C90X_TRACE("Read cycle mgmtData=%d\n", mgmtData ? 1 : 0);
					w4.PhysMgmt = data;
					if (mgmtData) {
						w4.PhysMgmt = w4.PhysMgmt | PM_mgmtData;
					} else {
						w4.PhysMgmt = w4.PhysMgmt & (~PM_mgmtData);
					}
					mMIIReadWord <<= 1;
				}
				mLastHiClkPhysMgmt = w4.PhysMgmt;
			} else {
				w4.PhysMgmt = data;
			}
			break;
		}
		case 10: {
			if (size != 2) {
				IO_3C90X_WARN("alignment.4.10\n");
				SINGLESTEP("");
			}
			uint mask = 0x10cc;
			IO_3C90X_TRACE("MediaStatus = %04x, old = %04x\n", ((w4.MediaStatus)&~mask)|(data&mask), w4.MediaStatus);
			w4.MediaStatus &= ~mask;
			w4.MediaStatus |= data & mask;
			w4.MediaStatus |= 0x8000;	// auiDisable always on
			break;
		}
		default:
			IO_3C90X_WARN("generic to window 4\n");
			SINGLESTEP("");
			memcpy(&mWindows[4].b[port], &data, size);
		}
		break;
	}
	/**/
	default:
		IO_3C90X_WARN("writing here unimpl.\n");
		SINGLESTEP("");
	}
}

void setCR(uint16 cr)
{
	IO_3C90X_TRACE("setCR(cr = %x)\n", cr);
	switch (cr & (31<<11)) {
	case CmdTotalReset:
		// FIXME: care about params
		IO_3C90X_TRACE("TotalReset\n");
		totalReset();
		break;
	case CmdSelectWindow: {
		IO_3C90X_TRACE("SelectWindow (window = %d) oldwindow = %d\n", cr & 7, mIntStatus >> 13);
		mIntStatus &= 0x1fff;
		mIntStatus |= (cr & 7)<<13;
		break;
	}
	case CmdTxReset:
		IO_3C90X_TRACE("TxReset\n");
		break;
	case CmdRxReset:
		IO_3C90X_TRACE("RxReset\n");
		break;
	case CmdSetIndicationEnable: {
		RegWindow5 &w5 = (RegWindow5&)mWindows[5];
		IO_3C90X_TRACE("SetIndicationEnable(%04x) oldvalue = %04x\n", cr & 0x7fe, w5.IndicationEnable);
		w5.IndicationEnable = cr & 0x7fe;
		break;
	}
	case CmdSetIntrEnb: {
		RegWindow5 &w5 = (RegWindow5&)mWindows[5];
		IO_3C90X_TRACE("SetIntrEnab(%04x) oldvalue = %04x\n", cr & 0x7fe, w5.InterruptEnable);
		w5.InterruptEnable = cr & 0x7fe;
		break;
	}
	case CmdStatsEnable:
		/* implement me */
		IO_3C90X_TRACE("StatsEnable\n");
		break;
	case CmdStatsDisable:
		/* implement me */
		IO_3C90X_TRACE("StatsDisable\n");
		break;
	case CmdEnableDC:
		/* implement me */
		IO_3C90X_TRACE("EnableDC\n");
		break;
	case CmdDisableDC:
		/* implement me */
		IO_3C90X_TRACE("DisableDC\n");
		break;
	case CmdStall: {
		/* FIXME: threading */
		switch (cr & 3) {
		case 0: /* UpStall */
		case 1: /* UpUnstall */ {
			IO_3C90X_TRACE("Stall(%s)\n", ((cr & 3) == 0) ? "UpStall" : "UpUnstall");
			bool stall = !(cr & 1);
			mUpStalled = stall;
/*			bool stall = cr & 1;
			mRegisters.DmaCtrl &= ~DC_upStalled;
			if (stall) mRegisters.DmaCtrl |= DC_upStalled;*/
			checkUpWork();
			break;
		}
		case 2: /* DnStall */
		case 3: /* DnUnstall */ {
			IO_3C90X_TRACE("Stall(%s)\n", ((cr & 3) == 2) ? "DnStall" : "DnUnstall");
			bool stall = !(cr & 1);
			mDnStalled = stall;
			mRegisters.DmaCtrl &= ~DC_dnStalled;
			if (stall) mRegisters.DmaCtrl |= DC_dnStalled;
			checkDnWork();
			break;
		}
		}
		break;
	}
	case CmdSetRxFilter: {
		IO_3C90X_TRACE("SetRxFilter(%02x)\n", cr & 31);
		RegWindow5 &w5 = (RegWindow5&)mWindows[5];
		w5.RxFilter = cr & 31;
		break;
	}
	case CmdSetTxReclaimThresh: {
		IO_3C90X_TRACE("SetTxReclaimHash(%02x)\n", cr & 255);
		RegWindow5 &w5 = (RegWindow5&)mWindows[5];
		w5.TxReclaimThresh = cr & 255;
		break;
	}
	case CmdSetTxStartThresh: {
		IO_3C90X_WARN("SetTxStartTresh(%02x)\n", (cr & 0x7ff) << 2);
//		SINGLESTEP("");
		RegWindow5 &w5 = (RegWindow5&)mWindows[5];
		w5.TxStartThresh = (cr & 0x7ff) << 2;
		break;
	}
	case CmdSetHashFilterBit: {
		bool value = cr & 0x400;
		uint which = cr & 0x3f;
		IO_3C90X_WARN("SetHashFilterBit(which=%d, value=%d)\n", which, value ? 1 : 0);
		break;
	}
	case CmdSetRxEarlyThresh: {
		IO_3C90X_TRACE("SetTxStartTresh(%02x)\n", (cr & 0x7ff) << 2);
		RegWindow5 &w5 = (RegWindow5&)mWindows[5];
		w5.RxEarlyThresh = (cr & 0x7ff) << 2;
		break;
	}
	case CmdRxEnable: {
		IO_3C90X_TRACE("RxEnable\n");
		mRxEnabled = true;
		break;
	}
	case CmdRxDisable: {
		IO_3C90X_TRACE("ExDisable\n");
		mRxEnabled = false;
		break;
	}
	case CmdTxEnable: {
		IO_3C90X_TRACE("TxEnable\n");
		mTxEnabled = true;
		break;
	}
	case CmdTxDisable: {
		IO_3C90X_TRACE("TxDisable\n");
		mTxEnabled = false;
		break;
	}
	case CmdAckIntr: {
		/*
		0x1	interruptLatchAck
		0x2	linkEventAck
		0x20	rxEarlyAck
		0x40	intRequestedAck
		0x200	dnCompleteAck
		0x400	upCompleteAck
		*/
		IO_3C90X_TRACE("AckIntr(%04x)\n", cr & 0x7ff);
		// ack/clear corresponding bits in IntStatus
		uint ISack = 0;
		if (cr & 0x01) ISack |= IS_interruptLatch;
		if (cr & 0x02) ISack |= IS_linkEvent;
		if (cr & 0x20) ISack |= IS_rxEarly;
		if (cr & 0x40) ISack |= IS_intRequested;
		if (cr & 0x200) ISack |= IS_dnComplete;
		if (cr & 0x400) ISack |= IS_upComplete;
		acknowledge(ISack);
		break;
	}
/*	case CmdReqIntr: {
		RegWindow5 &w5 = (RegWindow5&)mWindows[5];
		// set intRequested in IntStatus
		mIntStatus |= IS_intRequested;

// FIXME:		generate Interrupt (if enabled)
		break;
	}*/

/*
	case CmdTxDone:
	case CmdRxDiscard:
	case CmdSetTxThreshold:
*/
	default:
		IO_3C90X_WARN("command not implemented: %x\n", cr);
		SINGLESTEP("");
	}
}

void txDPD0(DPD0 *dpd)
{
	// FIXME: createHostStruct()
	DPDFragDesc *frags = (DPDFragDesc*)(dpd+1);
	uint32 fsh = dpd->FrameStartHeader;
	IO_3C90X_TRACE("fsh = %08x\n", fsh);
	if (fsh & FSH_dpdEmpty) {
		// modify FrameStartHeader in DPD (!)
		dpd->FrameStartHeader |= FSH_dnComplete;
		// set next DnListPtr
		mRegisters.DnListPtr = dpd->DnNextPtr;
		IO_3C90X_TRACE("dpd empty\n");
		return;
	}
	byte pbuf[MAX_PACKET_SIZE];
	byte *p = pbuf;
	// some packet drivers need padding
	uint framePrefix = mEthTun->getWriteFramePrefix();
	memset(p, 0, framePrefix);
	p += framePrefix;
	//
	uint i = 0;
	// assemble packet from fragments (up to MAX_DPD_FRAGS fragments)
	while (i < MAX_DPD_FRAGS) {
		uint addr = frags->DnFragAddr;
		uint len = frags->DnFragLen & 0x1fff;
		IO_3C90X_TRACE("frag %d: %08x, len %04x (full: %08x)\n", i, addr, len, frags->DnFragLen);
//		dumpMem(addr, len);
		if (p-pbuf+len >= sizeof pbuf) {
			IO_3C90X_WARN("packet too big ! (%d >= %d)\n", p-pbuf+len, sizeof pbuf);
			SINGLESTEP("");
			return;
		}
		if (!ppc_dma_read(p, addr, len)) {
			IO_3C90X_WARN("frag addr invalid! cancelling\n");
			SINGLESTEP("");
			return;
		}
		p += len;
		// last fragment ?
		if (frags->DnFragLen & 0x80000000) break;
		frags++;
		i++;
	}
	uint psize = p-pbuf;
	if (!(fsh & FSH_rndupDefeat)) {
		// round packet length
		switch (fsh & FSH_rndupBndry) {
		case 0: {
			// 4 bytes
			uint gap = ((psize+3) & ~3) -psize;
			memset(pbuf+psize, 0, gap);
			psize += gap;
			break;
		}
		case 2: {
			// 2 bytes
			uint gap = ((psize+1) & ~1) -psize;
			memset(pbuf+psize, 0, gap);
			psize += gap;
			break;
		}
		}
	}
	//FSH_reArmDisable =	1<<23,
	//FSH_lastKap =		1<<24,
	//FSH_addIpChecksum =	1<<25,
	//FSH_addTcpChecksum =	1<<26,
	//FSH_addUdpChecksum =	1<<27,
	if (fsh & (0x1f << 23)) {
		IO_3C90X_WARN("unsupported flags in fsh, fsh = %08x\n", fsh);
		SINGLESTEP("");
	}

	if (psize<60) {
		// pad packet to at least 60 bytes (+4 bytes crc = 64 bytes)
		memset(pbuf+psize, 0, (60-psize));
		psize = 60;
	}
	// append crc
	if (!(fsh & FSH_crcAppendDisable)) {
		uint32 crc = ether_crc(psize, pbuf);
		pbuf[psize+0] = crc;
		pbuf[psize+1] = crc>>8;
		pbuf[psize+2] = crc>>16;
		pbuf[psize+3] = crc>>24;
		psize += 4;
		IO_3C90X_TRACE("packet has crc: %08x\n", crc);
	}

//	IO_3C90X_TRACE("tx(%d):\n", psize);
//	dumpMem(pbuf, psize);
	uint w = mEthTun->sendPacket(pbuf, psize);
	if (w) {
		if (w == psize) {
			IO_3C90X_TRACE("EthTun: %d bytes sent.\n", psize);
		} else {
			IO_3C90X_TRACE("EthTun: ARGH! send error: only %d of %d bytes sent\n", w, psize);
		}
	} else {
		IO_3C90X_TRACE("EthTun: ARGH! send error in packet driver.\n");
	}
	// indications
	mRegisters.DmaCtrl |= DC_dnComplete;
	uint inds = 0;
	if (fsh & FSH_dnIndicate) inds |= IS_dnComplete;
	if (fsh & FSH_txIndicate) inds |= IS_txComplete;
	indicate(inds);
	// modify FrameStartHeader in DPD (!)
	dpd->FrameStartHeader |= FSH_dnComplete;
	// set next DnListPtr, TxPktId
	mRegisters.DnListPtr = dpd->DnNextPtr;
	uint pktId = (fsh & FSH_pktId) >> 2;
	mRegisters.TxPktId = pktId;
	// maybe generate interrupt
	maybeRaiseIntr();
}

bool passesRxFilter(byte *pbuf, uint psize)
{
	EthFrameII *f = (EthFrameII*)pbuf;
	RegWindow5 &w5 = (RegWindow5&)mWindows[5];
	if (w5.RxFilter & RXFILT_receiveAllFrames) return true;
	// FIXME: Multicast hashing not implemented
	if (w5.RxFilter & RXFILT_receiveMulticastHash) return true;
	// FIXME: Multicasting not understood
	if (w5.RxFilter & RXFILT_receiveMulticast) return true;
	if (w5.RxFilter & RXFILT_receiveBroadcast) {
		byte broadcastMAC[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
		if (compareMACs(f->destMAC, broadcastMAC) == 0) return true;
	}
	if (w5.RxFilter & RXFILT_receiveIndividual) {
		byte destMAC[6];
		byte thisMAC[6];
		RegWindow2 &w2 = (RegWindow2&)mWindows[2];
		for (uint i = 0; i < 6; i++) {
			destMAC[i] = f->destMAC[i] & ~w2.StationMask[i];
			thisMAC[i] = w2.StationAddress[i] & ~w2.StationMask[i];
		}
		return compareMACs(destMAC, thisMAC) == 0;
	}
	return false;
}

void rxUPD(UPD *upd)
{
	// FIXME: threading to care about (mRegisters.DmaCtrl & DC_upAltSeqDisable)
	IO_3C90X_TRACE("rxUPD()\n");

	bool error = false;

	if (upd->UpPktStatus & UPS_upComplete) {
		// IO_3C90X_WARN("UPD already upComplete!\n");

		// the top of the ring buffer is already used, 
	        // stall the upload and throw away the packet.
		// the ring buffers are filled.

		mUpStalled = true;
		return;
	}

	uint upPktStatus = 0;

	if (mRegisters.UpPoll) {
		IO_3C90X_WARN("UpPoll unsupported\n");
		SINGLESTEP("");
		return;
	}
	// FIXME:
//	if (mRegisters.DmaCtrl & DC_upRxEarlyEnable)
//		IO_3C90X_ERR("DC_upRxEarlyEnable unsupported\n");

	if ((mRxPacketSize > 0x1fff) || (mRxPacketSize > sizeof mRxPacket)) {
		IO_3C90X_TRACE("oversized frame\n");
		upd->UpPktStatus = UPS_upError | UPS_oversizedFrame;
		error = true;
	}

	if (mRxPacketSize < 60) {
		// pad packet to at least 60 bytes (+4 bytes crc = 64 bytes)
		memset(mRxPacket+mRxPacketSize, 0, (60-mRxPacketSize));
		mRxPacketSize = 60;
	}

	// IO_3C90X_TRACE("rx(%d):\n", mRxPacketSize);
	// dumpMem((unsigned char*)mRxPacket, mRxPacketSize);

/*	RegWindow5 &w5 = (RegWindow5&)mWindows[5];
	if ((mRxPacketSize < 60) && (w5.RxEarlyThresh >= 60)) {
		IO_3C90X_TRACE("runt frame\n");
		upPktStatus |= UPS_upError | UPS_runtFrame;
		upd->UpPktStatus = upPktStatus;
		error = true;
	}*/
	if (upd->UpPktStatus & UPD_impliedBufferEnable) {
		IO_3C90X_WARN("UPD_impliedBufferEnable unsupported\n");
		SINGLESTEP("");
		return;
	}
	UPDFragDesc *frags = (UPDFragDesc*)(upd+1);

	byte *p = mRxPacket;
	uint i = 0;
	while (!error && i < MAX_UPD_FRAGS) {	// (up to MAX_UPD_FRAGS fragments)
		uint32 addr = frags->UpFragAddr;
		uint len = frags->UpFragLen & 0x1fff;
		IO_3C90X_TRACE("frag %d: %08x, len %04x (full: %08x)\n", i, addr, len, frags->UpFragLen);
		if (p-mRxPacket+len > sizeof mRxPacket) {
	    		upPktStatus |= UPS_upError | UPS_upOverflow;
			upd->UpPktStatus = upPktStatus;
			IO_3C90X_TRACE("UPD overflow!\n");
			SINGLESTEP("");
			error = true;
			break;
		}

		if (!ppc_dma_write(addr, p, len)) {
			upPktStatus |= UPS_upError;
			upd->UpPktStatus = upPktStatus;
			IO_3C90X_WARN("invalid UPD fragment address! (%08x)\n", addr);
			SINGLESTEP("");
			error = true;
			break;
		}
		p += len;
		// last fragment ?
		if (frags->UpFragLen & 0x80000000) break;
		frags++;
		i++;
	}

	if (!error) {
		IO_3C90X_TRACE("successfully uploaded packet of %d bytes\n", mRxPacketSize);
	}
	upPktStatus |= mRxPacketSize & 0x1fff;
	upPktStatus |= UPS_upComplete;
	upd->UpPktStatus = upPktStatus;

	mRxPacketSize = 0;

	/* the client OS is waiting for a change in status, but won't see it */
	/* until we dma our local copy upd->UpPktStatus back to the client address space */
	if (!ppc_dma_write(mRegisters.UpListPtr+4, &upd->UpPktStatus, sizeof(upd->UpPktStatus))) {
	  upPktStatus |= UPS_upError;
	  upd->UpPktStatus = upPktStatus; /* can't get this error out, anyways */
	  IO_3C90X_WARN("invalid UPD UpListPtr address! (%08x)\n",mRegisters.UpListPtr+4);
	  SINGLESTEP("");
	  error = true;
	}

	mRegisters.UpListPtr = upd->UpNextPtr;

	// indications
	mRegisters.DmaCtrl |= DC_upComplete;
	indicate(IS_upComplete);
	maybeRaiseIntr();
}

void indicate(uint indications)
{
	RegWindow5 &w5 = (RegWindow5&)mWindows[5];
	if (w5.IndicationEnable & indications != indications) {
		IO_3C90X_TRACE("some masked: %08x\n", w5.IndicationEnable & indications);
	}
	mIntStatus |= w5.IndicationEnable & indications;
	if (indications & IS_upComplete) {
		mRegisters.DmaCtrl |= DC_upComplete;
	}
	if (indications & IS_dnComplete) {
		mRegisters.DmaCtrl |= DC_dnComplete;
	}
	IO_3C90X_TRACE("indicate(%08x) mIntStatus now = %08x\n", indications, mIntStatus);
}

void acknowledge(uint indications)
{
//	RegWindow5 &w5 = (RegWindow5&)mWindows[5];
	mIntStatus &= ~indications;
	if (indications & IS_upComplete) {
		mRegisters.DmaCtrl &= ~DC_upComplete;
	}
	if (indications & IS_dnComplete) {
		mRegisters.DmaCtrl &= ~DC_dnComplete;
	}
	IO_3C90X_TRACE("acknowledge(%08x) mIntStatus now = %08x\n", indications, mIntStatus);
}

void maybeRaiseIntr()
{
	RegWindow5 &w5 = (RegWindow5&)mWindows[5];
	if (w5.IndicationEnable & w5.InterruptEnable & mIntStatus) {
		mIntStatus |= IS_interruptLatch;
		IO_3C90X_TRACE("Generating interrupt. mIntStatus=%04x\n", mIntStatus);
		pic_raise_interrupt(mConfig[0x3c]);
	}
}

void checkDnWork()
{
	while (!mDnStalled && (mRegisters.DnListPtr != 0)) {
		byte dpd[512];
		
		if (ppc_dma_read(dpd, mRegisters.DnListPtr, sizeof dpd)) {
			// get packet type
			byte type = dpd[7]>>6;
			switch (type) {
			case 0:
			case 2: {
				DPD0 *p = (DPD0*)dpd;
				IO_3C90X_TRACE("Got a type 0 DPD !\n");
				IO_3C90X_TRACE("DnNextPtr is %08x\n", p->DnNextPtr);
				txDPD0(p);
				break;
			}
			case 1: {
				IO_3C90X_TRACE("Got a type 1 DPD ! not implemented !\n");
				IO_3C90X_TRACE("DnNextPtr is %08x\n", ((DPD1*)dpd)->DnNextPtr);
				SINGLESTEP("");
				mRegisters.DnListPtr = 0;
				break;
			}
			default:
				IO_3C90X_TRACE("unsupported packet type 3\n");
				mRegisters.DnListPtr = 0;
				SINGLESTEP("");
				break;
			}
		} else {
			IO_3C90X_WARN("DnListPtr invalid!\n");
			break;
		}
	}
}

void checkUpWork()
{
	if (mRxEnabled && !mUpStalled && mRxPacketSize && (mRegisters.UpListPtr != 0) ) {
		byte upd[MAX_UPD_SIZE];
		if (ppc_dma_read(upd, mRegisters.UpListPtr, sizeof upd)) {
			UPD *p = (UPD*)upd;
			rxUPD(p);
		} else {
			IO_3C90X_WARN("invalid address in UpListPtr!\n");
			SINGLESTEP("");
		}
	} else {
		IO_3C90X_TRACE("Not uploading, because: mUpStalled(=%d) or UpListPtr == 0 (=%08x) or not mRxPacketSize(=%d)\n", mUpStalled, mRegisters.UpListPtr, mRxPacketSize);
	}
}

public:
_3c90x_NIC(EthTunDevice *aEthTun, const byte *mac)
: PCI_Device("3c90x Network interface card", 0x1, 0xc)
{
	int e;
	if ((e = sys_create_mutex(&mLock))) throw IOException(e);
	mEthTun = aEthTun;
	memcpy(mMAC, mac, 6);
	PCIReset();
	totalReset();
}

virtual ~_3c90x_NIC()
{
	mEthTun->shutdownDevice();
	delete mEthTun;
	sys_destroy_mutex(mLock);
}

void readConfig(uint reg)
{
	if (reg >= 0xdc) {
		IO_3C90X_WARN("re\n");
		SINGLESTEP("");
	}
	sys_lock_mutex(mLock);
	PCI_Device::readConfig(reg);
	sys_unlock_mutex(mLock);
}

void writeConfig(uint reg, int offset, int size)
{
	sys_lock_mutex(mLock);
	if (reg >= 0xdc) {
		IO_3C90X_WARN("jg\n");
		SINGLESTEP("");
	}
	PCI_Device::writeConfig(reg, offset, size);
	sys_unlock_mutex(mLock);
}

bool readDeviceIO(uint r, uint32 port, uint32 &data, uint size)
{
	if (r != 0) return false;
	bool retval = false;
	sys_lock_mutex(mLock);
	if (port == 0xe) {
		// IntStatus (no matter which window)
		if (size != 2) {
			IO_3C90X_WARN("unaligned read from IntStatus\n");
			SINGLESTEP("");
		}
		IO_3C90X_TRACE("read IntStatus = %04x\n", mIntStatus);
		data = mIntStatus;
		retval = true;
	} else if (port >= 0 && (port+size <= 0x0e)) {
		// read from window
		uint curwindow = mIntStatus >> 13;
		readRegWindow(curwindow, port, data, size);
		retval = true;
	} else if ((port+size > 0x1e) && (port <= 0x1f)) {
		if ((port != 0x1e) || (size != 2)) {
			IO_3C90X_WARN("unaligned read from IntStatusAuto\n");
			SINGLESTEP("");
		}
		RegWindow5 &w5 = (RegWindow5&)mWindows[5];
		// side-effects of reading IntStatusAuto:
		// 1.clear InterruptEnable
		w5.InterruptEnable = 0;
		// 2.clear some flags
		acknowledge(IS_dnComplete | IS_upComplete
			| IS_rxEarly | IS_intRequested
			| IS_interruptLatch | IS_linkEvent);
		data = mIntStatus;
		IO_3C90X_TRACE("read IntStatusAuto = %04x\n", data);
		retval = true;
	} else if ((port >= 0x10) && (port+size <= 0x10 + sizeof(Registers))) {
		byte l = gRegAccess[port-0x10];
		if (l != size) {
			IO_3C90X_WARN("invalid/unaligned read from register port=%04x, size=%d (expecting size %d)\n", port, size, l);
			SINGLESTEP("");
		}
		// read from (standard) register
		data = 0;
		memcpy(&data, ((byte*)&mRegisters)+port-0x10, size);
		switch (port) {
		case 0x1a:
			IO_3C90X_TRACE("read Timer = %08x\n", data);
			break;
		case 0x20:
			IO_3C90X_TRACE("read DmaCtrl = %08x\n", data);
			break;
		case 0x24:
			IO_3C90X_TRACE("read DownListPtr = %08x\n", data);
			break;
		case 0x38:
			IO_3C90X_TRACE("read UpListPtr = %08x\n", data);
			break;
		default:
			IO_3C90X_WARN("read reg %04x (size %d) = %08x\n", port, size, data);
			SINGLESTEP("");
			break;
		}
		retval = true;
	}
	sys_unlock_mutex(mLock);
	return retval;
}

bool writeDeviceIO(uint r, uint32 port, uint32 data, uint size)
{
	if (r != 0) return false;
	bool retval = false;
	sys_lock_mutex(mLock);
	if (port == 0xe) {
		// CommandReg (no matter which window)
		if (size != 2) {
			IO_3C90X_WARN("unaligned write to CommandReg\n");
			SINGLESTEP("");
		}
		setCR(data);
		retval = true;
	} else if (port >= 0 && (port+size <= 0x0e)) {
		// write to window
		uint curwindow = mIntStatus >> 13;
		writeRegWindow(curwindow, port, data, size);
		retval = true;
	} else if (port >= 0x10 && (port + size <= 0x10 + sizeof(Registers))) {
		byte l = gRegAccess[port-0x10];
		if (l != size) {
			IO_3C90X_WARN("invalid/unaligned write to register port=%04x, size=%d\n", port, size);
			SINGLESTEP("");
		}
		switch (port) {
		case 0x20: {
			uint DmaCtrlRWMask = DC_upRxEarlyEnable	| DC_counterSpeed |
				DC_countdownMode | DC_defeatMWI | DC_defeatMRL |
				DC_upOverDiscEnable;
			mRegisters.DmaCtrl &= ~DmaCtrlRWMask;
			mRegisters.DmaCtrl |= data & DmaCtrlRWMask;
			IO_3C90X_TRACE("write DmaCtrl %08x (now = %08x)\n", data, mRegisters.DmaCtrl);
			break;
		}
		case 0x24: {
			if (!mRegisters.DnListPtr) {
				mRegisters.DnListPtr = data;
				IO_3C90X_TRACE("write DnListPtr (now = %08x)\n", mRegisters.DnListPtr);
			} else {
				IO_3C90X_TRACE("didn't write DnListPtr cause it's not 0 (now = %08x)\n", mRegisters.DnListPtr);
			}
			checkDnWork();
			break;
		}
		case 0x38: {
			mRegisters.UpListPtr = data;
			IO_3C90X_TRACE("write UpListPtr (now = %08x)\n", mRegisters.UpListPtr);
			checkUpWork();
			break;
		}
		case 0x2d:
			IO_3C90X_WARN("DnPoll\n");
			SINGLESTEP("");
			break;
		case 0x2a:
			memcpy(((byte*)&mRegisters)+port-0x10, &data, size);
			IO_3C90X_TRACE("write DnBurstThresh\n");
			break;
		case 0x2c:
			memcpy(((byte*)&mRegisters)+port-0x10, &data, size);
			IO_3C90X_TRACE("write DnPriorityThresh\n");
			break;
		case 0x2f:
			// used by Darwin as TxFreeThresh. Not documented in [1].
			memcpy(((byte*)&mRegisters)+port-0x10, &data, size);
			IO_3C90X_TRACE("write TxFreeThresh\n");
			break;
		case 0x3c:
			memcpy(((byte*)&mRegisters)+port-0x10, &data, size);
			IO_3C90X_TRACE("write UpPriorityThresh\n");
			break;
		case 0x3e:
			memcpy(((byte*)&mRegisters)+port-0x10, &data, size);
			IO_3C90X_TRACE("write UpBurstThresh\n");
			break;
		default:
			IO_3C90X_WARN("write to register port=%04x, size=%d\n", port, size);
			SINGLESTEP("");
			// write to (standard) register
			memcpy(((byte*)&mRegisters)+port-0x10, &data, size);
		}
		retval = true;
	}
	sys_unlock_mutex(mLock);
	return retval;
}

/* new */
void handleRxQueue()
{
	while (1) {
		while (mEthTun->waitRecvPacket() != 0) {
			// don't block the system in case of (repeated) error(s)
			sys_suspend();
		}
		sys_lock_mutex(mLock);
		if (mRxPacketSize) {
			IO_3C90X_TRACE("Argh. old packet not yet uploaded. waiting some more...\n");
		} else {
			mRxPacketSize = mEthTun->recvPacket(mRxPacket, sizeof mRxPacket);
			if (mRxEnabled && (mRxPacketSize > sizeof(EthFrameII))) {
				indicate(IS_rxComplete);
				maybeRaiseIntr();
				acknowledge(IS_rxComplete);
				if (!passesRxFilter(mRxPacket, mRxPacketSize)) {
					IO_3C90X_TRACE("EthTun: %d bytes received. But they don't pass the filter.\n", mRxPacketSize);
					mRxPacketSize = 0;
				} else {
					IO_3C90X_TRACE("EthTun: %d bytes received.\n", mRxPacketSize);
				}
			}  else {
				// don't block the system in case of (repeated) error(s)
				mRxPacketSize = 0;
				sys_suspend();
			}
		}
		checkUpWork();
		sys_unlock_mutex(mLock);
	}
}

};

static void *_3c90xHandleRxQueue(void *nic)
{
	_3c90x_NIC *NIC = (_3c90x_NIC *)nic;
	NIC->handleRxQueue();
	return NULL;
}

#include "configparser.h"
#include "tools/strtools.h"

bool _3c90x_installed = false;

#define _3C90X_KEY_INSTALLED	"pci_3c90x_installed"
#define _3C90X_KEY_MAC		"pci_3c90x_mac"

void _3c90x_init()
{
	if (gConfig->getConfigInt(_3C90X_KEY_INSTALLED)) {
		_3c90x_installed = true;
		byte mac[6];
		mac[0] = 0xde;
		mac[1] = 0xad;
		mac[2] = 0xca;
		mac[3] = 0xfe;
		mac[4] = 0x12;
		mac[5] = 0x34;
		if (gConfig->haveKey(_3C90X_KEY_MAC)) {
			String macstr_;
			gConfig->getConfigString(_3C90X_KEY_MAC, macstr_);
			// do something useful with mac
			const char *macstr = macstr_.contentChar();
			byte cfgmac[6];
			for (uint i = 0; i < 6; i++) {
				uint64 v;
				if (!parseIntStr(macstr, v, 16) || (v>255) || ((*macstr != ':') && (i!=5))) {
					IO_3C90X_ERR("error in config key %s:"
					"expected format: XX:XX:XX:XX:XX:XX, "
					"where X stands for any digit or the letters a-f, A-F (error at: %s)\n",_3C90X_KEY_MAC, macstr);
				}
				macstr++;
				cfgmac[i] = v;
			}
			memcpy(mac, cfgmac, sizeof mac);
		}
		EthTunDevice *ethTun = createEthernetTunnel();
		if (!ethTun) {
			IO_3C90X_ERR("Couldn't create ethernet tunnel\n");
			exit(1);
		}
		if (ethTun->initDevice()) {
			IO_3C90X_ERR("Couldn't initialize ethernet tunnel\n");
			exit(1);
		}
#if 0
		printf("Creating 3com 3c90x NIC emulation with eth_addr = ");
		for (uint i = 0; i < 6; i++) {
			if (i < 5) {
				printf("%02x:", mac[i]);
			} else {
				printf("%02x", mac[i]);
			}
		}
		printf("\n");
#endif
		_3c90x_NIC *MyNIC = new _3c90x_NIC(ethTun, mac);
		gPCI_Devices->insert(MyNIC);
		sys_thread rxthread;
		sys_create_thread(&rxthread, 0, _3c90xHandleRxQueue, MyNIC);
	}
}

void _3c90x_done()
{
}

void _3c90x_init_config()
{
	gConfig->acceptConfigEntryIntDef(_3C90X_KEY_INSTALLED, 0);
	gConfig->acceptConfigEntryString(_3C90X_KEY_MAC, false);
}
