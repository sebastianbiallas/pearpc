/*
 *	PearPC
 *	cuda.cc
 *
 *	Copyright (C) 2003 Sebastian Biallas (sb@biallas.net)
 *	Copyright (C) 2004 Stefan Weyergraf (stefan@weyergraf.de)
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
 *
 *	From Linux 2.6.4:
 *	The VIA (versatile interface adapter) interfaces to the CUDA,
 *	a 6805 microprocessor core which controls the ADB (Apple Desktop
 *	Bus) which connects to the keyboard and mouse.  The CUDA also
 *	controls system power and the RTC (real time clock) chip.
 *
 *	See also:
 *	http://www.howell1964.freeserve.co.uk/parts/6522_VIA.htm
 *
 *	References:
 *	[1] http://bbc.nvg.org/doc/datasheets/R6522_r9.zip
 */

#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <ctime>

#include "tools/snprintf.h"
#include "debug/tracers.h"
#include "io/pic/pic.h"
#include "system/keyboard.h"
#include "system/mouse.h"
#include "system/sys.h"
#include "system/sysclk.h"
#include "system/systhread.h"

#include "cuda.h"

//#define IO_CUDA_TRACE2(str...) ht_printf(str)
#define IO_CUDA_TRACE2(str...) 

//#define IO_CUDA_TRACE3(str...) ht_printf(str)
#define IO_CUDA_TRACE3(str...) 

#define RS		(0x200)
#define B		0		/* B-side data */
#define A		RS		/* A-side data */
#define DIRB		(2*RS)		/* B-side direction (1=output) */
#define DIRA		(3*RS)		/* A-side direction (1=output) */
#define T1CL		(4*RS)		/* Timer 1 ctr/latch (low 8 bits) */
#define T1CH		(5*RS)		/* Timer 1 counter (high 8 bits) */
#define T1LL		(6*RS)		/* Timer 1 latch (low 8 bits) */
#define T1LH		(7*RS)		/* Timer 1 latch (high 8 bits) */
#define T2CL		(8*RS)		/* Timer 2 ctr/latch (low 8 bits) */
#define T2CH		(9*RS)		/* Timer 2 counter (high 8 bits) */
#define SR		(10*RS)		/* Shift register */
#define ACR		(11*RS)		/* Auxiliary control register */
#define PCR		(12*RS)		/* Peripheral control register */
#define IFR		(13*RS)		/* Interrupt flag register */
#define IER		(14*RS)		/* Interrupt enable register */
#define ANH		(15*RS)		/* A-side data, no handshake */

/* Bits in B data register: all active low */
#define TREQ		0x08		/* Transfer request (input) */
#define TACK		0x10		/* Transfer acknowledge (output) */
#define TIP		0x20		/* Transfer in progress (output) */

/* Bits in ACR */
#define SR_CTRL		0x1c		/* Shift register control bits */
#define SR_EXT		0x0c		/* Shift on external clock */
#define SR_OUT		0x10		/* Shift out if 1 */

/* Bits in IFR and IER */
#define IER_SET		0x80		/* set bits in IER */
#define IER_CLR		0		/* clear bits in IER */
#define SR_INT		0x04		/* Shift register full/empty */

/* Bits in ACR */
#define T1MODE          0xc0            /* Timer 1 mode */
#define T1MODE_CONT     0x40            /*  continuous interrupts */

/* Bits in IFR and IER */
#define T1_INT          0x40            /* Timer 1 interrupt */

/* commands (1st byte) */
#define ADB_PACKET			0
#define CUDA_PACKET			1
#define ERROR_PACKET			2
#define TIMER_PACKET			3
#define POWER_PACKET			4
#define MACIIC_PACKET			5
#define PMU_PACKET			6

/* CUDA commands (2nd byte) */
#define CUDA_WARM_START			0x0
#define CUDA_AUTOPOLL			0x1
#define CUDA_GET_6805_ADDR		0x2
#define CUDA_GET_TIME			0x3
#define CUDA_GET_PRAM			0x7
#define CUDA_SET_6805_ADDR		0x8
#define CUDA_SET_TIME			0x9
#define CUDA_POWERDOWN			0xa
#define CUDA_POWERUP_TIME		0xb
#define CUDA_SET_PRAM			0xc
#define CUDA_MS_RESET			0xd
#define CUDA_SEND_DFAC			0xe
#define CUDA_BATTERY_SWAP_SENSE		0x10
#define CUDA_RESET_SYSTEM		0x11
#define CUDA_SET_IPL			0x12
#define CUDA_FILE_SERVER_FLAG		0x13
#define CUDA_SET_AUTO_RATE		0x14
#define CUDA_GET_AUTO_RATE		0x16
#define CUDA_SET_DEVICE_LIST		0x19
#define CUDA_GET_DEVICE_LIST		0x1a
#define CUDA_SET_ONE_SECOND_MODE	0x1b
#define CUDA_SET_POWER_MESSAGES		0x21
#define CUDA_GET_SET_IIC		0x22
#define CUDA_WAKEUP			0x23
#define CUDA_TIMER_TICKLE		0x24
#define CUDA_COMBINED_FORMAT_IIC	0x25


/* ADB commands */
#define ADB_BUSRESET			0x00
#define ADB_FLUSH               	0x01
#define ADB_WRITEREG			0x08
#define ADB_READREG			0x0c

/* ADB device commands */
#define ADB_CMD_SELF_TEST		0xff
#define ADB_CMD_CHANGE_ID		0xfe
#define ADB_CMD_CHANGE_ID_AND_ACT	0xfd
#define ADB_CMD_CHANGE_ID_AND_ENABLE	0x00

/* ADB default device IDs (upper 4 bits of ADB command byte) */
#define ADB_DONGLE			1
#define ADB_KEYBOARD			2
#define ADB_MOUSE			3
#define ADB_TABLET			4
#define ADB_MODEM			5
#define ADB_MISC			7

#define ADB_RET_OK			0
#define ADB_RET_INUSE			1
#define ADB_RET_NOTPRESENT		2
#define ADB_RET_TIMEOUT			3
#define ADB_RET_UNEXPECTED_RESULT	4
#define ADB_RET_REQUEST_ERROR		5
#define ADB_RET_BUS_ERROR		6

#define ADB_PACKET			0
#define CUDA_PACKET			1
#define ERROR_PACKET			2
#define TIMER_PACKET			3
#define POWER_PACKET			4
#define MACIIC_PACKET			5
#define PMU_PACKET			6

// VIA timer runs at a frequency of 1/1.27655us
// or 783361.40378364 ticks/second
#define VIA_TIMER_FREQ_DIV_HZ_TIMES_1000 (783361404ULL)

enum cuda_state {
	cuda_idle,
	cuda_writing,
	cuda_reading,
};

struct cuda_control {
	byte rA;
	byte rB;
	byte rDIRB;
	byte rDIRA;
	byte rT1CL;
	byte rT1CH;
	byte rT1LL;
	byte rT1LH;
	byte rT2CL;
	byte rT2CH;
	byte rSR;
	byte rACR;
	byte rPCR;
	byte rIFR;
	byte rIER;
	byte rANH;

	// private
	uint64	T1_end;		// in cpu ticks
	bool	autopoll;
	cuda_state state;
	int	left;
	int	pos;
	uint8	data[100];

	int	keybid;
	int	keybhandler;
	int	mouseid;
	int	mousehandler;

	sys_semaphore idle_sem;
};

static cuda_control	gCUDA;
static sys_mutex	gCUDAMutex;

static void cuda_send_packet(uint8 type, int nb, ...)
{
	gCUDA.data[0] = type;
	va_list va;
	va_start(va, nb);
	for (int i=0; i<nb; i++) {
		uint8 b = va_arg(va, int);
		gCUDA.data[i+1] = b;
	}
	va_end(va);
	gCUDA.pos = 0;
	gCUDA.left = nb+1;
	gCUDA.rIFR |= SR_INT;
	gCUDA.rB &= ~TREQ;
	gCUDA.rB |= TIP;
	pic_raise_interrupt(IO_PIC_IRQ_CUDA);
}

static void cuda_receive_adb_packet()
{
	IO_CUDA_TRACE3("ADB_PACKET %02x %02x %02x %02x %02x\n", gCUDA.data[1], gCUDA.data[2], gCUDA.data[3], gCUDA.data[4], gCUDA.data[5]);
//	gSinglestep = true;
	IO_CUDA_TRACE2("ADB_PACKET ");
	if (gCUDA.data[1] == ADB_BUSRESET) {
		IO_CUDA_TRACE2("ADB_BUSRESET %02x\n", gCUDA.data[2]);
		cuda_send_packet(ADB_PACKET, 2, 0, 0);
		return;
	}
	int devaddr = gCUDA.data[1] >> 4;
	int cmd = gCUDA.data[1] & 0xf;
	if (cmd == ADB_FLUSH) {
		// FIXME: ok?
		cuda_send_packet(ADB_PACKET, 2, 0, 0);
		return;
	}
	int reg = cmd & 3;
	cmd &= 0xc;
	IO_CUDA_TRACE3("devaddr %x reg %x cmd %s\n", devaddr, reg, (cmd==ADB_WRITEREG)?"write":"read");
	switch (cmd) {
	case ADB_WRITEREG:
		switch (reg) {
		case 2:
			if (devaddr == gCUDA.keybid) {
				// LED stat
	ht_printf("ADB_PACKET %02x %02x %02x %02x %02x\n", gCUDA.data[1], gCUDA.data[2], gCUDA.data[3], gCUDA.data[4], gCUDA.data[5]);
	ht_printf("devaddr %x reg %x cmd %s\n", devaddr, reg, (cmd==ADB_WRITEREG)?"write":"read");
				cuda_send_packet(ADB_PACKET, 1, ADB_RET_OK);
			} else if (devaddr == gCUDA.mouseid) {
//				gSinglestep = true;
				cuda_send_packet(ADB_PACKET, 1, ADB_RET_OK);
			} else {
				cuda_send_packet(ADB_PACKET, 1, ADB_RET_NOTPRESENT);
			}
			break;
		case 3:
			if (devaddr == gCUDA.keybid) {
				switch (gCUDA.data[3]) {
				case ADB_CMD_SELF_TEST:
					cuda_send_packet(ADB_PACKET, 1, ADB_RET_OK);
					break;
				case ADB_CMD_CHANGE_ID:
				case ADB_CMD_CHANGE_ID_AND_ACT:
				case ADB_CMD_CHANGE_ID_AND_ENABLE:
					gCUDA.keybid = gCUDA.data[2] & 0xf;
					cuda_send_packet(ADB_PACKET, 1, ADB_RET_OK);
					break;
				default:
					gCUDA.keybid = gCUDA.data[2] & 0xf;
					gCUDA.keybhandler = gCUDA.data[3];
					cuda_send_packet(ADB_PACKET, 1, ADB_RET_OK);
					break;
				}
			} else if (devaddr == gCUDA.mouseid) {
				switch (gCUDA.data[3]) {
				case ADB_CMD_SELF_TEST:
					cuda_send_packet(ADB_PACKET, 1, ADB_RET_OK);
					break;
				case ADB_CMD_CHANGE_ID:
				case ADB_CMD_CHANGE_ID_AND_ACT:
				case ADB_CMD_CHANGE_ID_AND_ENABLE:
					gCUDA.mouseid = gCUDA.data[2] & 0xf;
					cuda_send_packet(ADB_PACKET, 1, ADB_RET_OK);
					break;
				default:
					gCUDA.mouseid = gCUDA.data[2] & 0xf;
//					gCUDA.mousehandler = gCUDA.data[3];
					cuda_send_packet(ADB_PACKET, 1, ADB_RET_OK);
					break;
				}
			} else {
				cuda_send_packet(ADB_PACKET, 1, ADB_RET_NOTPRESENT);
			}
			break;
		default:
			IO_CUDA_ERR("unknown reg %02x for device %02x\n", reg, devaddr);
		}
		break;
	case ADB_READREG: {
		switch (reg) {
		case 1:
			if (devaddr == gCUDA.keybid) {
				IO_CUDA_WARN("keyb reg1\n");
				cuda_send_packet(ADB_PACKET, 1, ADB_RET_OK);
			} else if (devaddr == gCUDA.mouseid) {
//				gSinglestep = true;
				IO_CUDA_WARN("read reg 1 of mouse unsupported.\n");
				cuda_send_packet(ADB_PACKET, 1, ADB_RET_OK);
			} else {
				cuda_send_packet(ADB_PACKET, 1, ADB_RET_NOTPRESENT);
			}
			break;		
		case 2:
			if (devaddr == gCUDA.keybid) {
				// LED stat
				// 111b == all off
				int ledstat = gKeyboard->getKeybLEDs();
				int keyb = 0xff;
				if (!(ledstat & KEYB_LED_NUM)) keyb &= ~0x80;
				if (!(ledstat & KEYB_LED_SCROLL)) keyb &= ~0x40;
				if (!(ledstat & KEYB_LED_CAPS)) keyb &= ~0x20;
				cuda_send_packet(ADB_PACKET, 3, ADB_RET_OK, 0xff, keyb);
			} else if (devaddr == gCUDA.mouseid) {
//				gSinglestep = true;
				IO_CUDA_WARN("read reg 2 of mouse unsupported.\n");
			} else {
				cuda_send_packet(ADB_PACKET, 1, ADB_RET_NOTPRESENT);
			}
			break;
		case 3:
			if (devaddr == gCUDA.keybid) {
				cuda_send_packet(ADB_PACKET, 3, ADB_RET_OK, gCUDA.keybhandler, gCUDA.keybid);
			} else if (devaddr == gCUDA.mouseid) {
				cuda_send_packet(ADB_PACKET, 3, ADB_RET_OK, gCUDA.mousehandler, gCUDA.mouseid);
			} else {
				cuda_send_packet(ADB_PACKET, 1, ADB_RET_NOTPRESENT);
			}
			break;
		default:
			IO_CUDA_ERR("unknown reg %02x for device %02x\n", reg, devaddr);
		}
		break;
	}
	default:
		IO_CUDA_ERR("unknown adb command\n");
	}
/*	
	default:
		switch (gCUDA.data[1] & 0xf0) {
		case (ADB_KEYBOARD << 4):
			switch (gCUDA.data[1] & 0xf) {
				case 0xf: 
					IO_CUDA_TRACE2("KEYBOARD: GET DEVICE INFO %02x\n", gCUDA.data[1]);
					cuda_send_packet(ADB_PACKET, 4, 0, 0, 0, 1);
					return;
			}			
			IO_CUDA_TRACE2("KEYBOARD: unknown %02x\n", gCUDA.data[1]);
			cuda_send_packet(ADB_PACKET, 1, 0x2);
			return;
		}
		IO_CUDA_TRACE2("unknown adb (%02x)!\n", gCUDA.data[1]);
//		IO_CUDA_ERR("!\n");
		cuda_send_packet(ADB_PACKET, 1, 0x2);
	}
*/	
}

static void cuda_receive_cuda_packet()
{
	IO_CUDA_TRACE2("CUDA_PACKET ");
	switch (gCUDA.data[1]) {
	case CUDA_AUTOPOLL: {
		IO_CUDA_TRACE2("CUDA_AUTOPOLL=%02x\n", gCUDA.data[2]);
		if (gCUDA.data[2]) {
			gCUDA.autopoll = true;
		} else {
			gCUDA.autopoll = false;
		}
		cuda_send_packet(CUDA_PACKET, 1, gCUDA.data[2]);
		break;
	}
	case CUDA_GET_TIME: {
		IO_CUDA_TRACE2("CUDA_GET_TIME %02x\n", gCUDA.data[2]);
		time_t tt;
		time(&tt);
		uint32 t = (uint32)tt+ 2082844800;
		cuda_send_packet(CUDA_PACKET, 6, 0, 0, t>>24, t>>16, t>>8, t);
		break;
	}
	case CUDA_SET_TIME: {
		IO_CUDA_TRACE2("CUDA_SET_TIME %02x\n", gCUDA.data[2]);
		cuda_send_packet(CUDA_PACKET, 1, 0);
		break;
	}
	case CUDA_RESET_SYSTEM: {
		IO_CUDA_ERR("reset!\n");
		break;
	}
	case CUDA_FILE_SERVER_FLAG: {
		IO_CUDA_TRACE2("FILE_SERVER_FLAG %02x\n", gCUDA.data[2]);
		cuda_send_packet(CUDA_PACKET, 1, 0);
		break;
	}
	case CUDA_SET_DEVICE_LIST: {
		IO_CUDA_TRACE2("SET_DEVICE_LIST %02x %02x %02x\n", gCUDA.data[2], gCUDA.data[3], gCUDA.data[4]);
		cuda_send_packet(CUDA_PACKET, 1, 0);
		break;		
	}
	case CUDA_SET_AUTO_RATE: {
		IO_CUDA_TRACE2("SET_AUTO_RATE %02x\n", gCUDA.data[2]);
		cuda_send_packet(CUDA_PACKET, 1, 0);
		break;		
	}
	case CUDA_SET_POWER_MESSAGES: {
		IO_CUDA_TRACE2("CUDA_SET_POWER_MESSAGES %02x\n", gCUDA.data[2]);
		cuda_send_packet(CUDA_PACKET, 1, 0);
		break;
	}
	case CUDA_POWERDOWN: {
		IO_CUDA_ERR("power down!\n");
		break;
	}
	default:
		IO_CUDA_ERR("unknown cuda (%02x)!\n", gCUDA.data[1]);
	}
}

static void cuda_receive_packet()
{
	IO_CUDA_TRACE2("cuda received packet: (%d) ", gCUDA.pos);
	switch (gCUDA.data[0]) {
	case ADB_PACKET:
		cuda_receive_adb_packet();
		break;
	case CUDA_PACKET:
		cuda_receive_cuda_packet();
		break;
	case ERROR_PACKET:
		IO_CUDA_TRACE2("ERROR_PACKET %02x %02x\n", gCUDA.data[1], gCUDA.data[2]);
		IO_CUDA_ERR("error packet\n");
		break;
	case TIMER_PACKET:
		IO_CUDA_TRACE2("TIMER_PACKET %02x %02x\n", gCUDA.data[1], gCUDA.data[2]);
		IO_CUDA_ERR("timer packet\n");
		break;
	case POWER_PACKET:
		IO_CUDA_TRACE2("POWER_PACKET %02x %02x\n", gCUDA.data[1], gCUDA.data[2]);
		IO_CUDA_ERR("power packet\n");
		break;
	case MACIIC_PACKET:
		IO_CUDA_TRACE2("MACIIC_PACKET %02x %02x\n", gCUDA.data[1], gCUDA.data[2]);
		IO_CUDA_ERR("maciic packet\n");
		break;
	case PMU_PACKET:
		IO_CUDA_TRACE2("PMU_PACKET %02x %02x\n", gCUDA.data[1], gCUDA.data[2]);
		IO_CUDA_ERR("pmu packet\n");
		break;
	default:
		IO_CUDA_TRACE2("unknown generic (%02x)!\n", gCUDA.data[0]);
		IO_CUDA_ERR("unknown packet\n", gCUDA.data[0]);
		break;
	}
}

static void cuda_update_T1()
{
	uint64 clk = sys_get_hiresclk_ticks();
	if (clk < gCUDA.T1_end) {
		uint64 ticks_per_sec = 1000ULL * sys_get_hiresclk_ticks_per_second();
		uint64 T1 = (gCUDA.T1_end - clk) * VIA_TIMER_FREQ_DIV_HZ_TIMES_1000 / ticks_per_sec;
		gCUDA.rT1CL = T1;
		gCUDA.rT1CH = T1 >> 8;
		gCUDA.rIFR &= ~T1_INT;
		//
//		uint64 tmp = gCUDA.T1_end - clk;
//		IO_CUDA_WARN("T1 running, T1 now %04x, T1_end-clk=%08qx\n", (uint32)T1, &tmp);
	} else {
		uint64 ticks_per_sec = 1000ULL * sys_get_hiresclk_ticks_per_second();
		uint64 T1_latch = (gCUDA.rT1LH << 8) | gCUDA.rT1LL;
		uint64 full_T1_interval_ticks = (T1_latch+1) * ticks_per_sec / VIA_TIMER_FREQ_DIV_HZ_TIMES_1000;
		uint64 T1_end = clk + full_T1_interval_ticks - (clk - gCUDA.T1_end) % full_T1_interval_ticks;
		gCUDA.T1_end = T1_end;
		uint64 T1 = (gCUDA.T1_end - clk) * VIA_TIMER_FREQ_DIV_HZ_TIMES_1000 / ticks_per_sec;
		gCUDA.rT1CL = T1;
		gCUDA.rT1CH = T1 >> 8;
		gCUDA.rIFR |= T1_INT;
		//
//		uint64 tmp = gCUDA.T1_end - clk;
//		IO_CUDA_WARN("T1 overflowed, setting interrupt flag, T1 set to %04x, T1_end-clk=%08qx, T1_latch = %04x\n", (uint32)T1, &tmp, T1_latch);
	}
}

static void cuda_start_T1()
{
	uint64 clk = sys_get_hiresclk_ticks();
	uint64 ticks_per_sec = 1000ULL * sys_get_hiresclk_ticks_per_second();
	uint32 T1 = (gCUDA.rT1CH << 8) | gCUDA.rT1CL;
/*	uint64 tmp = static_cast<uint64>(T1) * ticks_per_sec / VIA_TIMER_FREQ_DIV_HZ_TIMES_1000;
	printf("T1 for %lld ticks (%g seconds vs. %g)\n",
		   tmp, static_cast<double>(tmp)/static_cast<double>(ticks_per_sec / 1000),
		   static_cast<double>(T1) * 1.27655 / 1000000.0);*/
	gCUDA.T1_end = clk + T1 * ticks_per_sec / VIA_TIMER_FREQ_DIV_HZ_TIMES_1000;
	gCUDA.rIFR &= ~T1_INT;
	IO_CUDA_TRACE("T1 restarted, T1 = %08x\n", T1);
}

void cuda_write(uint32 addr, uint32 data, int size)
{
	sys_lock_mutex(gCUDAMutex);

	IO_CUDA_TRACE("%d write word @%08x: %08x\n", gCUDA.state, addr, data);
	addr -= IO_CUDA_PA_START;
	switch (addr) {
	case A:
		IO_CUDA_TRACE("->A\n");
		gCUDA.rA = data;
		break;
	case B: {
		bool ack = false;
		if (gCUDA.rB & TACK) {
			if (!(data & TACK)) {
				gCUDA.rIFR |= SR_INT;
				if (gCUDA.state == cuda_idle) {
					data &= ~TREQ;
				}
				ack = true;
			}
		} else {
			if ((data & TACK)) {
				gCUDA.rIFR |= SR_INT;
				if (gCUDA.state == cuda_idle) {
					if (data & TIP) {
						data |= TREQ;
					} else {
						data &= ~TREQ;
        				}
				}
				ack = true;
			}
		}
		if ((gCUDA.state == cuda_reading) && ack) {
			gCUDA.data[gCUDA.pos] = gCUDA.rSR;
			IO_CUDA_TRACE2(";; %d:%x\n", gCUDA.pos, gCUDA.rSR);
			gCUDA.pos++;
			if (gCUDA.pos > 10) {
				gCUDA.pos = 0;
				IO_CUDA_ERR("cuda overflow!\n");
			}
		}
		if ((gCUDA.state == cuda_writing) && ack) {
			if (gCUDA.left <= 1) {
				data |= TREQ;
			}
//			gCUDA.rB = data;
//			break;
		}
		if ((gCUDA.rB & TIP) && !(data & TIP)) {
			gCUDA.rIFR |= SR_INT;
//			IO_CUDA_TRACE2("^ from: %08x %02x\n", gCPU.pc, gCUDA.rIFR);
			if (gCUDA.rACR & SR_OUT) {
				gCUDA.state = cuda_reading;
				IO_CUDA_TRACE2("CUDA CHANGE STATE %d: to %d\n", __LINE__, gCUDA.state);
				gCUDA.pos = 1;
				data |= TREQ;
				gCUDA.data[0] = gCUDA.rSR;
			} else {
				if (gCUDA.left) {
					gCUDA.state = cuda_writing;
					IO_CUDA_TRACE2("CUDA CHANGE STATE %d: to %d\n", __LINE__, gCUDA.state);
				} else {
//					data &= ~TIP;
				}
				data &= ~TREQ;
			}
		}
		pic_raise_interrupt(IO_PIC_IRQ_CUDA);
		if (!(gCUDA.rB & TIP) && (data & TIP)) {
			gCUDA.rIFR |= SR_INT;
//			IO_CUDA_TRACE2("v from: %08x %d\n", gCPU.pc, gCUDA.state);
			data |= TREQ | TIP;
			gCUDA.rB = data;
			if (gCUDA.state == cuda_reading) {
				cuda_receive_packet();
				if (!gCUDA.left) {
//					pic_cancel_interrupt(IO_PIC_IRQ_CUDA);
					gCUDA.rIFR &= ~SR_INT;
				}
			} else if (gCUDA.state == cuda_writing) {
				IO_CUDA_TRACE2("cuda sent packet (%d)\n", gCUDA.pos);
				gCUDA.left = 0;
				gCUDA.pos = 0;
			}
			gCUDA.state = cuda_idle;
			sys_signal_semaphore(gCUDA.idle_sem);
			IO_CUDA_TRACE2("CUDA CHANGE STATE %d: to %d\n", __LINE__, gCUDA.state);
		} else {
			gCUDA.rB = data;
		}
		IO_CUDA_TRACE("->B(%02x)\n", gCUDA.rB);
		break;
	}
    	case DIRB:
		IO_CUDA_TRACE("->DIRB\n");
		gCUDA.rDIRB = data;
		break;
    	case DIRA:
		IO_CUDA_TRACE("->DIRA\n");
		gCUDA.rDIRA = data;
		break;
    	case T1CL:
		IO_CUDA_TRACE("->T1CL\n");
		// same as writing to T1LL
		gCUDA.rT1CL = data;
		gCUDA.rT1LL = data;
		break;
    	case T1CH:
		IO_CUDA_TRACE("->T1CH\n");
		/* from [1]: "[T1C-L] is loaded automatically from the low-order\
		 * latch (T1L-L) when the processor writes into the high-order counter\
		 * (T1C-H)"
		 * and: "8 bits loaded into high-order latches. also at this time both \
		 * high- and low-order latches transferred into T1 counter"
		 */
		gCUDA.rT1LH = data;
		gCUDA.rT1CH = gCUDA.rT1LH;
		gCUDA.rT1CL = gCUDA.rT1LL;
		cuda_start_T1();
		break;
    	case T1LL:
		IO_CUDA_TRACE("->T1LL\n");
		/* from [1]: "this operation is no different than a write into reg 4"
		 * reg4 is T1CL
		 */
		gCUDA.rT1CL = data;
		gCUDA.rT1LL = data;
		break;
    	case T1LH:
		IO_CUDA_TRACE("->T1LH\n");
		gCUDA.rT1LH = data;
		break;
    	case T2CL:
		IO_CUDA_ERR("->T2CL\n");
		gCUDA.rT2CL = data;
		break;
    	case T2CH:
		IO_CUDA_ERR("->T2CH\n");
		gCUDA.rT2CH = data;
		break;
    	case ACR:
		IO_CUDA_TRACE("->ACR\n");
		gCUDA.rACR = data;
		break;
    	case SR:
		IO_CUDA_TRACE("->SR\n");
		gCUDA.rSR = data;
		break;
    	case PCR:
		IO_CUDA_TRACE("->PCR\n");
		gCUDA.rPCR = data;
		break;
    	case IFR:
		IO_CUDA_TRACE("->IFR\n");
		gCUDA.rIFR = data;
		break;
    	case IER:
		IO_CUDA_TRACE("->IER\n");
		gCUDA.rIER = data;
		break;
    	case ANH:
		IO_CUDA_TRACE("->ANH\n");
		gCUDA.rANH = data;
		break;
	default:
		IO_CUDA_ERR("unknown service\n");
	}

	sys_unlock_mutex(gCUDAMutex);
}

void cuda_read(uint32 addr, uint32 &data, int size)
{
	sys_lock_mutex(gCUDAMutex);

	IO_CUDA_TRACE("%d read word @%08x\n", gCUDA.state, addr);
	addr -= IO_CUDA_PA_START;
	switch (addr) {
	case A:
		IO_CUDA_TRACE("A(%02x)->\n", gCUDA.rA);
		data = gCUDA.rA;
		break;
	case B:
		IO_CUDA_TRACE("B(%02x)->\n", gCUDA.rB);
		data = gCUDA.rB;
		break;
	case DIRB:
		IO_CUDA_TRACE("DIRB(%02x)->\n", gCUDA.rDIRB);
		data = gCUDA.rDIRB;
		break;
	case DIRA:
		IO_CUDA_TRACE("DIRA->\n");
		data = gCUDA.rDIRA;
		break;
	case T1CL:
		IO_CUDA_TRACE("T1CL->\n");
		cuda_update_T1();
		data = gCUDA.rT1CL;
		break;
	case T1CH: {
		IO_CUDA_TRACE("T1CH->\n");
		cuda_update_T1();
//		uint64 clk = sys_get_cpu_ticks();
//		IO_CUDA_WARN("read %08x: T1 = %04x clk = %08qx, T1_end = %08qx\n",
//			gCPU.current_code_base + gCPU.pc_ofs,
//			(gCUDA.rT1CH<<8) | gCUDA.rT1CL,
//			&clk, &gCUDA.T1_end);
		data = gCUDA.rT1CH;
		break;
	}
	case T1LL:
		IO_CUDA_WARN("T1LL->\n");
		data = gCUDA.rT1LL;
		break;
	case T1LH:
		IO_CUDA_WARN("T1LH->\n");
		data = gCUDA.rT1LH;
		break;
	case T2CL:
		IO_CUDA_ERR("T2CL->\n");
		data = gCUDA.rT2CL;
		break;
	case T2CH:
		IO_CUDA_ERR("T2CH->\n");
		data = gCUDA.rT2CH;
		break;
	case ACR:
		IO_CUDA_TRACE("ACR->\n");
		data = gCUDA.rACR;
		break;
	case SR:
		IO_CUDA_TRACE("SR->\n");
		data = gCUDA.rSR;
		if (gCUDA.state == cuda_writing) {
			if (gCUDA.left) {
				data = gCUDA.data[gCUDA.pos];
//				IO_CUDA_TRACE2("::%d:%02x from %08x\n", gCUDA.pos, data, gCPU.pc);
				gCUDA.pos++;
				gCUDA.left--;
			}
			if (gCUDA.left <= 0) {
				IO_CUDA_TRACE2("stop\n");
				gCUDA.rB |= TREQ;
				gCUDA.rB &= ~TIP;
			}
			gCUDA.rIFR &= ~SR_INT;
		} else if (gCUDA.state == cuda_reading) {
			gCUDA.rB &= ~TREQ;
			gCUDA.rIFR &= ~SR_INT;
		} else {
			gCUDA.rB |= TREQ;
			gCUDA.rIFR &= ~SR_INT;
		}
		break;
	case PCR:
		IO_CUDA_TRACE("PCR->\n");
		data = gCUDA.rPCR;
		break;
	case IFR:
		data = gCUDA.rIFR;
		if (gCUDA.state == cuda_idle) {
			if (!gCUDA.left /*&& !(gCUDA.rIER & SR_INT)*/) {
				if (cuda_interrupt()) {
					data |= SR_INT;
//					if (gCUDA.autopoll) pic_raise_interrupt(IO_PIC_IRQ_CUDA);
				}
			}
//			ht_printf("is idle!\n");
		} else {
//			ht_printf("state not idle bla !\n");
//			data |= SR_INT;
		}
		cuda_update_T1();
		IO_CUDA_TRACE("%d IFR->(%02x/%02x)\n", gCUDA.state, gCUDA.rIFR, data);
		break;
	case IER:
		IO_CUDA_TRACE("IER->\n");
		data = gCUDA.rIER;
		break;
	case ANH:
		IO_CUDA_TRACE("ANH->\n");
		data = gCUDA.rANH;
		break;
	default:
		IO_CUDA_ERR("unknown service\n");
	}

	sys_unlock_mutex(gCUDAMutex);
}

bool cuda_interrupt()
{
	return false;
}

static sys_semaphore	gCUDAEventSem;
static Queue		gCUDAEvents(true);

static bool cudaEventHandler(const SystemEvent &ev)
{
	sys_lock_semaphore(gCUDAEventSem);
//	ht_printf("queue  %d\n", ev.key.pressed);
	gCUDAEvents.enQueue(new SystemEventObject(ev));
	sys_unlock_semaphore(gCUDAEventSem);
	sys_signal_semaphore(gCUDAEventSem);
	return true;
}

static bool doProcessCudaEvent(const SystemEvent &ev)
{
	switch (ev.type) {
	case sysevKey: {
		uint8 k = ev.key.keycode;
		if (!ev.key.pressed) {
			k |= 0x80;
		}
		cuda_send_packet(ADB_PACKET, 4, 0x40, 0x2c, k, 0xff);
		return true;
	}
	case sysevMouse: {
		int dx = ev.mouse.relx; //* 256 / gDisplay->mClientChar.width;
		int dy = ev.mouse.rely; //* 256 / gDisplay->mClientChar.height;
		if (dx < 0) {
			if (dx < -63) {
				dx = 127;
			} else {
				dx += 128;
			}
		} else if (dx > 63) {
			dx = 63;
		}
		if (dy < 0) {
			if (dy < -63) {
				dy = 127;
			} else {
				dy += 128;
			}
		} else if (dy > 63) {
			dy = 63;
		}
		if (!ev.mouse.button2) dx |= 0x80;
		if (!ev.mouse.button1) dy |= 0x80;
//		ht_printf("adb mouse: cur: %d, %d d: %d, %d\n", ev.mouseEvent.x, ev.mouseEvent.y, dx, dy);
		cuda_send_packet(ADB_PACKET, 4, 0x40, 0x3c, dy, dx);
		return true;
	}
	default:
		return false;
	}
}

static bool tryProcessCudaEvent(const SystemEvent &ev)
{
	uint timeout_msec = 100;
	uint64 time_end = sys_get_hiresclk_ticks() + sys_get_hiresclk_ticks_per_second()
		* timeout_msec / 1000;
//	ht_printf("process  %d\n", ev.key.pressed);
	while (sys_get_hiresclk_ticks() < time_end) {
		sys_lock_mutex(gCUDAMutex);
		static int lockuphack = 0;
		if (gCUDA.state == cuda_idle) {
			if (!gCUDA.left /*&& !(gCUDA.rIFR & SR_INT)*/) {
				lockuphack = 0;
				bool k = doProcessCudaEvent(ev);
				sys_unlock_mutex(gCUDAMutex);
//				IO_CUDA_WARN("Tried to process event: %d.\n", k);
				return k;
			} else {
				IO_CUDA_TRACE2("left: %d\n", gCUDA.left);
				if (lockuphack++ == 20) {
					gCUDA.left = 0;
					lockuphack = 0;
					IO_CUDA_WARN("lock-up parachute\n");
				}
			}
		} else {
			IO_CUDA_TRACE2("cuda not idle (%d)!\n", gCUDA.state);
		}
		sys_unlock_mutex(gCUDAMutex);
		sys_lock_mutex(gCUDA.idle_sem);
		sys_wait_semaphore_bounded(gCUDA.idle_sem, 20);
		sys_unlock_mutex(gCUDA.idle_sem);
	}
	IO_CUDA_WARN("Event processing timed out. Event dropped.\n");
	return false;
}

static void *cudaEventLoop(void *arg)
{
	if (sys_create_semaphore(&gCUDAEventSem)) {
		IO_CUDA_ERR("Can't create semaphore\n");
	}
	gKeyboard->attachEventHandler(cudaEventHandler);
	gMouse->attachEventHandler(cudaEventHandler);
	sys_lock_semaphore(gCUDAEventSem);
	while (1) {
//		IO_CUDA_WARN("waiting on semaphore\n");
		sys_wait_semaphore(gCUDAEventSem);
//		IO_CUDA_WARN("semaphore signalled\n");
		SystemEventObject *seo;
		while ((seo = (SystemEventObject*)gCUDAEvents.deQueue())) {
			tryProcessCudaEvent(seo->mEv);
			delete seo;
		}
	}
}

void cuda_init()
{
	memset(&gCUDA, 0, sizeof gCUDA);
	gCUDA.state = cuda_idle;
	gCUDA.keybid = ADB_KEYBOARD;
	gCUDA.keybhandler = 1;
	gCUDA.mouseid = ADB_MOUSE;
	gCUDA.mousehandler = 2;
	gCUDA.T1_end = 0;
	gCUDA.rT1LL = 0xff;
	gCUDA.rT1LH = 0xff;

	if (sys_create_mutex(&gCUDAMutex)) {
		IO_CUDA_ERR("Can't create mutex\n");
	}

	if (sys_create_semaphore(&gCUDA.idle_sem)) {
		IO_CUDA_ERR("Can't create semaphore\n");
	}

	sys_thread cudaEventLoopThread;
	sys_create_thread(&cudaEventLoopThread, 0, cudaEventLoop, NULL);
}

void cuda_init_config()
{
}
