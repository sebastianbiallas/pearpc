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

static inline void ppc_update_cr0(uint32 r)
{
	gCPU.cr &= 0x0fffffff;
	if (!r) {
		gCPU.cr |= CR_CR0_EQ;
	} else if (r & 0x80000000) {
		gCPU.cr |= CR_CR0_LT;
	} else {
		gCPU.cr |= CR_CR0_GT;
	}
	if (gCPU.xer & XER_SO) gCPU.cr |= CR_CR0_SO;
}

void ppc_opc_bx();
void ppc_opc_bcx();
void ppc_opc_bcctrx();
void ppc_opc_bclrx();

void ppc_opc_dcba();
void ppc_opc_dcbf();
void ppc_opc_dcbi();
void ppc_opc_dcbst();
void ppc_opc_dcbt();
void ppc_opc_dcbtst();

void ppc_opc_eciwx();
void ppc_opc_ecowx();
void ppc_opc_eieio();

void ppc_opc_icbi();
void ppc_opc_isync();

void ppc_opc_mcrf();
void ppc_opc_mcrfs();
void ppc_opc_mcrxr();
void ppc_opc_mfcr();
void ppc_opc_mffsx();
void ppc_opc_mfmsr();
void ppc_opc_mfspr();
void ppc_opc_mfsr();
void ppc_opc_mfsrin();
void ppc_opc_mftb();
void ppc_opc_mtcrf();
void ppc_opc_mtfsb0x();
void ppc_opc_mtfsb1x();
void ppc_opc_mtfsfx();
void ppc_opc_mtfsfix();
void ppc_opc_mtmsr();
void ppc_opc_mtspr();
void ppc_opc_mtsr();
void ppc_opc_mtsrin();

void ppc_opc_rfi();
void ppc_opc_sc();
void ppc_opc_sync();
void ppc_opc_tlbia();
void ppc_opc_tlbie();
void ppc_opc_tlbsync();
void ppc_opc_tw();
void ppc_opc_twi();

JITCFlow ppc_opc_gen_bx();
JITCFlow ppc_opc_gen_bcx();
JITCFlow ppc_opc_gen_bcctrx();
JITCFlow ppc_opc_gen_bclrx();

JITCFlow ppc_opc_gen_dcba();
JITCFlow ppc_opc_gen_dcbf();
JITCFlow ppc_opc_gen_dcbi();
JITCFlow ppc_opc_gen_dcbst();
JITCFlow ppc_opc_gen_dcbt();
JITCFlow ppc_opc_gen_dcbtst();

JITCFlow ppc_opc_gen_eciwx();
JITCFlow ppc_opc_gen_ecowx();
JITCFlow ppc_opc_gen_eieio();

JITCFlow ppc_opc_gen_icbi();
JITCFlow ppc_opc_gen_isync();

JITCFlow ppc_opc_gen_mcrf();
JITCFlow ppc_opc_gen_mcrfs();
JITCFlow ppc_opc_gen_mcrxr();
JITCFlow ppc_opc_gen_mfcr();
JITCFlow ppc_opc_gen_mffsx();
JITCFlow ppc_opc_gen_mfmsr();
JITCFlow ppc_opc_gen_mfspr();
JITCFlow ppc_opc_gen_mfsr();
JITCFlow ppc_opc_gen_mfsrin();
JITCFlow ppc_opc_gen_mftb();
JITCFlow ppc_opc_gen_mtcrf();
JITCFlow ppc_opc_gen_mtfsb0x();
JITCFlow ppc_opc_gen_mtfsb1x();
JITCFlow ppc_opc_gen_mtfsfx();
JITCFlow ppc_opc_gen_mtfsfix();
JITCFlow ppc_opc_gen_mtmsr();
JITCFlow ppc_opc_gen_mtspr();
JITCFlow ppc_opc_gen_mtsr();
JITCFlow ppc_opc_gen_mtsrin();

JITCFlow ppc_opc_gen_rfi();
JITCFlow ppc_opc_gen_sc();
JITCFlow ppc_opc_gen_sync();
JITCFlow ppc_opc_gen_tlbia();
JITCFlow ppc_opc_gen_tlbie();
JITCFlow ppc_opc_gen_tlbsync();
JITCFlow ppc_opc_gen_tw();
JITCFlow ppc_opc_gen_twi();


#endif

