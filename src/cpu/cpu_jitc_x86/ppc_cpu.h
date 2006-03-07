/*
 *	PearPC
 *	ppc_cpu.h
 *
 *	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
 *	Copyright (C) 2004 Daniel Foesch (dfoesch@cs.nmsu.edu)
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

#ifndef __PPC_CPU_H__
#define __PPC_CPU_H__

#include <stddef.h>
#include "system/types.h"
#include "cpu/common.h"

#define PPC_MHz(v) ((v)*1000*1000)

#define PPC_MODEL		"ppc_model"
#define PPC_CPU_MODEL		"ppc_cpu"
#define PPC_CLOCK_FREQUENCY	PPC_MHz(200)
#define PPC_BUS_FREQUENCY	(PPC_CLOCK_FREQUENCY/5)
#define PPC_TIMEBASE_FREQUENCY	(PPC_BUS_FREQUENCY/4)

struct PPC_CPU_State {
	// offsetof first entry of this structure must not be 0
	uint32 dummy;
	
	// * uisa
	uint32 gpr[32];
	uint64 fpr[32];
	uint32 cr;
	uint32 fpscr;
	uint32 xer;	// spr 1
	uint32 xer_ca;  // for jitc
	uint32 lr;	// spr 8
	uint32 ctr;	// spr 9
	// * oea
	uint32 msr;
	uint32 pvr;	// spr 287

	//    * memory managment
	uint32 ibatu[4];	// spr 528, 530, 532, 534
	uint32 ibatl[4];	// spr 529, 531, 533, 535
	uint32 ibat_bl[4];	// internal
	uint32 ibat_nbl[4];	// internal
	uint32 ibat_bepi[4];	// internal
	uint32 ibat_brpn[4];	// internal

	uint32 dbatu[4];	// spr 536, 538, 540, 542
	uint32 dbatl[4];	// spr 537, 539, 541, 543
	uint32 dbat_bl[4];	// internal
	uint32 dbat_nbl[4];	// internal
	uint32 dbat_bepi[4];	// internal
	uint32 dbat_brpn[4];	// internal

	uint32 sdr1;	// spr 25       (page table base address)

	uint32 sr[16];
	
	//    * exception handling
	uint32 dar;	// spr 19
	uint32 dsisr;	// spr 18
	uint32 sprg[4]; // spr 272-275
	uint32 srr[2];	// spr 26-27
	
	//    * misc
	uint32 dec; // spr 22
	uint32 ear; // spr 282 .101
	uint32 pir; // spr 1032
	uint64 tb; // .75 spr 284(l)/285(u)
	
	uint32 hid[16];
	// * internal
	// this is used for speeding things up
	
	uint32 pc;
	uint32 npc;
	uint32 current_opc;
	bool   exception_pending;
	bool   dec_exception;
	bool   ext_exception;
	bool   stop_exception;
	bool   singlestep_ignore;
	byte   align[3];	

	uint32 pagetable_base;
	int    pagetable_hashmask;
	uint32 reserve;
	bool   have_reservation;
	byte   align2[3];	
	
	uint32 tlb_last;
	uint32 tlb_pa[4];
	uint32 tlb_va[4];

	// for generic cpu core
	uint32 effective_code_page;
	byte  *physical_code_page;
	uint64 pdec;	// more precise version of dec
	uint64 ptb;	// more precise version of tb

	// for jitc
	uint32 temp;
	uint32 temp2;
	uint32 x87cw;
	uint32 pc_ofs;
	uint32 current_code_base;

	// for altivec
	uint32 vscr;
	uint32 vrsave;  // spr 256
	uint32 vtemp;
	uint64 vtemp64;
	uint32 vfcw;	// floating point control word store for vect unit
	uint32 vfcw_save;	// floating point control word save
	Vector_t vr[36] ALIGN_STRUCT(16);	// <-- this MUST be 16-byte aligned!
} PACKED;

enum PPC_Register {
	PPC_REG_NO = 0,
	PPC_GPR0 = offsetof(PPC_CPU_State, gpr),
	PPC_FPR1 = offsetof(PPC_CPU_State, fpr),
	PPC_VR = offsetof(PPC_CPU_State, vr),
	PPC_CR = offsetof(PPC_CPU_State, cr),
	PPC_FPSCR = offsetof(PPC_CPU_State, fpscr),
	PPC_VSCR = offsetof(PPC_CPU_State, vscr),
	PPC_VRSAVE = offsetof(PPC_CPU_State, vrsave),
	PPC_XER = offsetof(PPC_CPU_State, xer),
	PPC_LR = offsetof(PPC_CPU_State, lr),
	PPC_CTR = offsetof(PPC_CPU_State, ctr),
	PPC_MSR = offsetof(PPC_CPU_State, msr),
	PPC_SRR0 = offsetof(PPC_CPU_State, srr),
	PPC_SRR1 = offsetof(PPC_CPU_State, srr)+sizeof (uint32),
	PPC_DSISR = offsetof(PPC_CPU_State, dsisr),
	PPC_DAR = offsetof(PPC_CPU_State, dar),
	PPC_DEC = offsetof(PPC_CPU_State, dec),
	PPC_SDR1 = offsetof(PPC_CPU_State, sdr1),
	PPC_EAR = offsetof(PPC_CPU_State, ear),
	PPC_PVR = offsetof(PPC_CPU_State, pvr),
	PPC_HID0 = offsetof(PPC_CPU_State, hid),
	PPC_HID1 = offsetof(PPC_CPU_State, hid)+sizeof (uint32),
};

enum PPC_CRx {
	PPC_CR0=0,
	PPC_CR1=1,
	PPC_CR2=2,
	PPC_CR3=3,
	PPC_CR4=4,
	PPC_CR5=5,
	PPC_CR6=6,
	PPC_CR7=7,
	
	PPC_NO_CRx=0xffffffff,
};

#define PPC_GPR(n) ((PPC_Register)(offsetof(PPC_CPU_State, gpr)+(n)*sizeof (uint32)))
#define PPC_FPR(n) ((PPC_Register)(offsetof(PPC_CPU_State, fpr)+(n)*sizeof (uint64)))
#define PPC_FPR_U(n) ((PPC_Register)(offsetof(PPC_CPU_State, fpr)+4+(n)*sizeof (uint64)))
#define PPC_FPR_L(n) ((PPC_Register)(offsetof(PPC_CPU_State, fpr)+(n)*sizeof (uint64)))
#define PPC_VR(n) ((PPC_Register)((n)*sizeof (Vector_t)))
#define PPC_VR_3(n) ((PPC_Register)(offsetof(PPC_CPU_State, vr)+12+(n)*sizeof (Vector_t)))
#define PPC_VR_2(n) ((PPC_Register)(offsetof(PPC_CPU_State, vr)+8+(n)*sizeof (Vector_t)))
#define PPC_VR_1(n) ((PPC_Register)(offsetof(PPC_CPU_State, vr)+4+(n)*sizeof (Vector_t)))
#define PPC_VR_0(n) ((PPC_Register)(offsetof(PPC_CPU_State, vr)+(n)*sizeof (Vector_t)))
#define PPC_SR(n) ((PPC_Register)(offsetof(PPC_CPU_State, sr)+(n)*sizeof (uint32)))
#define PPC_SPRG(n) ((PPC_Register)(offsetof(PPC_CPU_State, sprg)+(n)*sizeof (uint32)))
#define PPC_IBATU(n) ((PPC_Register)(offsetof(PPC_CPU_State, ibatu)+(n)*sizeof (uint32)))
#define PPC_IBATL(n) ((PPC_Register)(offsetof(PPC_CPU_State, ibatl)+(n)*sizeof (uint32)))
#define PPC_DBATU(n) ((PPC_Register)(offsetof(PPC_CPU_State, dbatu)+(n)*sizeof (uint32)))
#define PPC_DBATL(n) ((PPC_Register)(offsetof(PPC_CPU_State, dbatl)+(n)*sizeof (uint32)))

#include "system/systimer.h"

extern PPC_CPU_State gCPU;
extern uint64 gClientClockFrequency;
extern uint64 gClientTimeBaseFrequency;
extern sys_timer gDECtimer;

uint64 ppc_get_cpu_timebase();
uint64 ppc_get_cpu_ideal_timebase();

void ppc_run();
void ppc_stop();

void ppc_set_singlestep_v(bool v, const char *file, int line, const char *infoformat, ...);
void ppc_set_singlestep_nonverbose(bool v);

extern "C" void ppc_cpu_atomic_raise_dec_exception();
extern "C" void ppc_cpu_atomic_raise_ext_exception();
extern "C" void ppc_cpu_atomic_cancel_ext_exception();

void cpu_wakeup();

bool cpu_init();
void cpu_init_config();


#define SINGLESTEP(info...)	ppc_set_singlestep_v(true, __FILE__, __LINE__, info)
extern uint32 gBreakpoint;
extern uint32 gBreakpoint2;

#endif
 
