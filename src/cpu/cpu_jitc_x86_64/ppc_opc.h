/*
 *	PearPC
 *	ppc_opc.h
 *
 *	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
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

#ifndef __PPC_OPC_H__
#define __PPC_OPC_H__

#include "system/types.h"
#include "jitc_types.h"
#include "jitc.h"

static inline void ppc_update_cr0(PPC_CPU_State &aCPU, uint32 r)
{
	aCPU.cr &= 0x0fffffff;
	if (!r) {
		aCPU.cr |= CR_CR0_EQ;
	} else if (r & 0x80000000) {
		aCPU.cr |= CR_CR0_LT;
	} else {
		aCPU.cr |= CR_CR0_GT;
	}
	if (aCPU.xer & XER_SO) aCPU.cr |= CR_CR0_SO;
}

static UNUSED void ppc_opc_gen_interpret(JITC &jitc, ppc_opc_function func) 
{
	jitc.clobberAll();
	
	jitc.asmALU64(X86_LEA, RDI, curCPU(all));
	jitc.asmALU32(X86_MOV, curCPU(current_opc), jitc.current_opc);
	jitc.asmCALL((NativeAddress)func);
}


void ppc_opc_bx(PPC_CPU_State &aCPU);
void ppc_opc_bcx(PPC_CPU_State &aCPU);
void ppc_opc_bcctrx(PPC_CPU_State &aCPU);
void ppc_opc_bclrx(PPC_CPU_State &aCPU);

void ppc_opc_dcba(PPC_CPU_State &aCPU);
void ppc_opc_dcbf(PPC_CPU_State &aCPU);
void ppc_opc_dcbi(PPC_CPU_State &aCPU);
void ppc_opc_dcbst(PPC_CPU_State &aCPU);
void ppc_opc_dcbt(PPC_CPU_State &aCPU);
void ppc_opc_dcbtst(PPC_CPU_State &aCPU);

void ppc_opc_eciwx(PPC_CPU_State &aCPU);
void ppc_opc_ecowx(PPC_CPU_State &aCPU);
void ppc_opc_eieio(PPC_CPU_State &aCPU);

void ppc_opc_icbi(PPC_CPU_State &aCPU);
void ppc_opc_isync(PPC_CPU_State &aCPU);

void ppc_opc_mcrf(PPC_CPU_State &aCPU);
void ppc_opc_mcrfs(PPC_CPU_State &aCPU);
void ppc_opc_mcrxr(PPC_CPU_State &aCPU);
void ppc_opc_mfcr(PPC_CPU_State &aCPU);
void ppc_opc_mffsx(PPC_CPU_State &aCPU);
void ppc_opc_mfmsr(PPC_CPU_State &aCPU);
void ppc_opc_mfspr(PPC_CPU_State &aCPU);
void ppc_opc_mfsr(PPC_CPU_State &aCPU);
void ppc_opc_mfsrin(PPC_CPU_State &aCPU);
void ppc_opc_mftb(PPC_CPU_State &aCPU);
void ppc_opc_mtcrf(PPC_CPU_State &aCPU);
void ppc_opc_mtfsb0x(PPC_CPU_State &aCPU);
void ppc_opc_mtfsb1x(PPC_CPU_State &aCPU);
void ppc_opc_mtfsfx(PPC_CPU_State &aCPU);
void ppc_opc_mtfsfix(PPC_CPU_State &aCPU);
void ppc_opc_mtmsr(PPC_CPU_State &aCPU);
void ppc_opc_mtspr(PPC_CPU_State &aCPU);
void ppc_opc_mtsr(PPC_CPU_State &aCPU);
void ppc_opc_mtsrin(PPC_CPU_State &aCPU);

void ppc_opc_rfi(PPC_CPU_State &aCPU);
void ppc_opc_sc(PPC_CPU_State &aCPU);
void ppc_opc_sync(PPC_CPU_State &aCPU);
void ppc_opc_tlbia(PPC_CPU_State &aCPU);
void ppc_opc_tlbie(PPC_CPU_State &aCPU);
void ppc_opc_tlbsync(PPC_CPU_State &aCPU);
void ppc_opc_tw(PPC_CPU_State &aCPU);
void ppc_opc_twi(PPC_CPU_State &aCPU);

JITCFlow ppc_opc_gen_bx(JITC &aJITC);
JITCFlow ppc_opc_gen_bcx(JITC &aJITC);
JITCFlow ppc_opc_gen_bcctrx(JITC &aJITC);
JITCFlow ppc_opc_gen_bclrx(JITC &aJITC);

JITCFlow ppc_opc_gen_dcba(JITC &aJITC);
JITCFlow ppc_opc_gen_dcbf(JITC &aJITC);
JITCFlow ppc_opc_gen_dcbi(JITC &aJITC);
JITCFlow ppc_opc_gen_dcbst(JITC &aJITC);
JITCFlow ppc_opc_gen_dcbt(JITC &aJITC);
JITCFlow ppc_opc_gen_dcbtst(JITC &aJITC);

JITCFlow ppc_opc_gen_eciwx(JITC &aJITC);
JITCFlow ppc_opc_gen_ecowx(JITC &aJITC);
JITCFlow ppc_opc_gen_eieio(JITC &aJITC);

JITCFlow ppc_opc_gen_icbi(JITC &aJITC);
JITCFlow ppc_opc_gen_isync(JITC &aJITC);

JITCFlow ppc_opc_gen_mcrf(JITC &aJITC);
JITCFlow ppc_opc_gen_mcrfs(JITC &aJITC);
JITCFlow ppc_opc_gen_mcrxr(JITC &aJITC);
JITCFlow ppc_opc_gen_mfcr(JITC &aJITC);
JITCFlow ppc_opc_gen_mffsx(JITC &aJITC);
JITCFlow ppc_opc_gen_mfmsr(JITC &aJITC);
JITCFlow ppc_opc_gen_mfspr(JITC &aJITC);
JITCFlow ppc_opc_gen_mfsr(JITC &aJITC);
JITCFlow ppc_opc_gen_mfsrin(JITC &aJITC);
JITCFlow ppc_opc_gen_mftb(JITC &aJITC);
JITCFlow ppc_opc_gen_mtcrf(JITC &aJITC);
JITCFlow ppc_opc_gen_mtfsb0x(JITC &aJITC);
JITCFlow ppc_opc_gen_mtfsb1x(JITC &aJITC);
JITCFlow ppc_opc_gen_mtfsfx(JITC &aJITC);
JITCFlow ppc_opc_gen_mtfsfix(JITC &aJITC);
JITCFlow ppc_opc_gen_mtmsr(JITC &aJITC);
JITCFlow ppc_opc_gen_mtspr(JITC &aJITC);
JITCFlow ppc_opc_gen_mtsr(JITC &aJITC);
JITCFlow ppc_opc_gen_mtsrin(JITC &aJITC);

JITCFlow ppc_opc_gen_rfi(JITC &aJITC);
JITCFlow ppc_opc_gen_sc(JITC &aJITC);
JITCFlow ppc_opc_gen_sync(JITC &aJITC);
JITCFlow ppc_opc_gen_tlbia(JITC &aJITC);
JITCFlow ppc_opc_gen_tlbie(JITC &aJITC);
JITCFlow ppc_opc_gen_tlbsync(JITC &aJITC);
JITCFlow ppc_opc_gen_tw(JITC &aJITC);
JITCFlow ppc_opc_gen_twi(JITC &aJITC);


#endif

