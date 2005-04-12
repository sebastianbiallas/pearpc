/*
 *	PearPC
 *	ppc_dec.cc
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

#include <cstring>

#include "system/types.h"
#include "debug/tracers.h"
#include "ppc_alu.h"
#include "ppc_cpu.h"
#include "ppc_dec.h"
#include "ppc_esc.h"
#include "ppc_exc.h"
#include "ppc_fpu.h"
#include "ppc_mmu.h"
#include "ppc_opc.h"
#include "jitc_asm.h"
#include "x86asm.h"

#include "io/prom/promosi.h"

static void ppc_opc_invalid()
{
	SINGLESTEP("unknown instruction\n");
}

static JITCFlow ppc_opc_gen_invalid()
{
//	PPC_DEC_WARN("invalid instruction 0x%08x\n", gJITC.current_opc);
	asmALURegImm(X86_MOV, ESI, gJITC.pc);
	asmALURegImm(X86_MOV, EDX, gJITC.current_opc);
	asmALURegImm(X86_MOV, ECX, PPC_EXC_PROGRAM_ILL);
	asmJMP((NativeAddress)ppc_program_exception_asm);
	return flowEndBlockUnreachable;
}

static void ppc_opc_special()
{
	if (gCPU.pc == gPromOSIEntry && gCPU.current_opc == PROM_MAGIC_OPCODE) {
		call_prom_osi();
		return;
	}
	if (gCPU.current_opc == 0x00333301) {
		// memset(r3, r4, r5)
		uint32 dest = gCPU.gpr[3];
		uint32 c = gCPU.gpr[4];
		uint32 size = gCPU.gpr[5];
		if (dest & 0xfff) {
			byte *dst;
			ppc_direct_effective_memory_handle(dest, dst);
			uint32 a = 4096 - (dest & 0xfff);
			memset(dst, c, a);
			size -= a;
			dest += a;
		}
		while (size >= 4096) {
			byte *dst;
			ppc_direct_effective_memory_handle(dest, dst);
			memset(dst, c, 4096);
			dest += 4096;
			size -= 4096;
		}
		if (size) {
			byte *dst;
			ppc_direct_effective_memory_handle(dest, dst);
			memset(dst, c, size);
		}
		gCPU.pc = gCPU.npc;
		return;
	}
	if (gCPU.current_opc == 0x00333302) {
		// memcpy
		uint32 dest = gCPU.gpr[3];
		uint32 src = gCPU.gpr[4];
		uint32 size = gCPU.gpr[5];
		byte *d, *s;
		ppc_direct_effective_memory_handle(dest, d);
		ppc_direct_effective_memory_handle(src, s);
		while (size--) {
			if (!(dest & 0xfff)) ppc_direct_effective_memory_handle(dest, d);
			if (!(src & 0xfff)) ppc_direct_effective_memory_handle(src, s);
			*d = *s;
			src++; dest++; d++; s++;
		}
		gCPU.pc = gCPU.npc;
		return;
	}
	ppc_opc_invalid();
}

static JITCFlow ppc_opc_gen_special()
{
	if (gJITC.current_opc == PPC_OPC_ESCAPE_VM) {
		jitcGetClientRegister(PPC_GPR(3), NATIVE_REG | EAX);
		jitcClobberAll();
		asmALURegReg(X86_MOV, EDX, ESP);
		asmALURegImm(X86_MOV, ECX, gJITC.pc);
		PPC_ESC_TRACE("pc = %08x\n", gJITC.pc);
		asmCALL((NativeAddress)&ppc_escape_vm);
		return flowEndBlock;
	}
	if (gJITC.pc == (gPromOSIEntry&0xfff) && gJITC.current_opc == PROM_MAGIC_OPCODE) {
		jitcClobberAll();
		asmMOVRegDMem(EAX, (uint32)&gCPU.current_code_base);
		asmALURegImm(X86_ADD, EAX, gJITC.pc);
		asmMOVDMemReg((uint32)&gCPU.pc, EAX);
		asmCALL((NativeAddress)&call_prom_osi);
		return flowEndBlock;
	}
	return ppc_opc_gen_invalid();
}

// main opcode 19
static void ppc_opc_group_1()
{
	uint32 ext = PPC_OPC_EXT(gCPU.current_opc);
	if (ext & 1) {
		// crxxx
		if (ext <= 225) {
			switch (ext) {
				case 33: ppc_opc_crnor(); return;
				case 129: ppc_opc_crandc(); return;
				case 193: ppc_opc_crxor(); return;
				case 225: ppc_opc_crnand(); return;
			}
		} else {
			switch (ext) {
				case 257: ppc_opc_crand(); return;
				case 289: ppc_opc_creqv(); return;
				case 417: ppc_opc_crorc(); return;
				case 449: ppc_opc_cror(); return;
			}
		}
	} else if (ext & (1<<9)) {
		// bcctrx
		if (ext == 528) {
			ppc_opc_bcctrx(); 
			return;
		}
	} else {
		switch (ext) {
			case 16: ppc_opc_bclrx(); return;
			case 0: ppc_opc_mcrf(); return;
			case 50: ppc_opc_rfi(); return;
			case 150: ppc_opc_isync(); return;
		}
	}
	return ppc_opc_invalid();
}
static JITCFlow ppc_opc_gen_group_1()
{
	uint32 ext = PPC_OPC_EXT(gJITC.current_opc);
	if (ext & 1) {
		// crxxx
		if (ext <= 225) {
			switch (ext) {
				case 33: return ppc_opc_gen_crnor();
				case 129: return ppc_opc_gen_crandc();
				case 193: return ppc_opc_gen_crxor();
				case 225: return ppc_opc_gen_crnand();
			}
		} else {
			switch (ext) {
				case 257: return ppc_opc_gen_crand();
				case 289: return ppc_opc_gen_creqv();
				case 417: return ppc_opc_gen_crorc();
				case 449: return ppc_opc_gen_cror();
			}
		}
	} else if (ext & (1<<9)) {
		// bcctrx
		if (ext == 528) {
			return ppc_opc_gen_bcctrx(); 
		}
	} else {
		switch (ext) {
			case 16: return ppc_opc_gen_bclrx();
			case 0: return ppc_opc_gen_mcrf();
			case 50: return ppc_opc_gen_rfi();
			case 150: return ppc_opc_gen_isync();
		}
	}
	return ppc_opc_gen_invalid();
}

ppc_opc_function ppc_opc_table_group2[1015];
ppc_opc_gen_function ppc_opc_table_gen_group2[1015];

// main opcode 31
static void ppc_opc_init_group2()
{
	for (uint i=0; i<(sizeof ppc_opc_table_group2 / sizeof ppc_opc_table_group2[0]); i++) {
		ppc_opc_table_group2[i] = ppc_opc_invalid;
		ppc_opc_table_gen_group2[i] = ppc_opc_gen_invalid;
	}
	ppc_opc_table_group2[0] = ppc_opc_cmp;
	ppc_opc_table_group2[4] = ppc_opc_tw;
	ppc_opc_table_group2[8] = ppc_opc_subfcx;//+
	ppc_opc_table_group2[10] = ppc_opc_addcx;//+
	ppc_opc_table_group2[11] = ppc_opc_mulhwux;
	ppc_opc_table_group2[19] = ppc_opc_mfcr;
	ppc_opc_table_group2[20] = ppc_opc_lwarx;
	ppc_opc_table_group2[23] = ppc_opc_lwzx;
	ppc_opc_table_group2[24] = ppc_opc_slwx;
	ppc_opc_table_group2[26] = ppc_opc_cntlzwx;
	ppc_opc_table_group2[28] = ppc_opc_andx;
	ppc_opc_table_group2[32] = ppc_opc_cmpl;
	ppc_opc_table_group2[40] = ppc_opc_subfx;
	ppc_opc_table_group2[54] = ppc_opc_dcbst;
	ppc_opc_table_group2[55] = ppc_opc_lwzux;
	ppc_opc_table_group2[60] = ppc_opc_andcx;
	ppc_opc_table_group2[75] = ppc_opc_mulhwx;
	ppc_opc_table_group2[83] = ppc_opc_mfmsr;
	ppc_opc_table_group2[86] = ppc_opc_dcbf;
	ppc_opc_table_group2[87] = ppc_opc_lbzx;
	ppc_opc_table_group2[104] = ppc_opc_negx;
	ppc_opc_table_group2[119] = ppc_opc_lbzux;
	ppc_opc_table_group2[124] = ppc_opc_norx;
	ppc_opc_table_group2[136] = ppc_opc_subfex;//+
	ppc_opc_table_group2[138] = ppc_opc_addex;//+
	ppc_opc_table_group2[144] = ppc_opc_mtcrf;
	ppc_opc_table_group2[146] = ppc_opc_mtmsr;
	ppc_opc_table_group2[150] = ppc_opc_stwcx_;
	ppc_opc_table_group2[151] = ppc_opc_stwx;
	ppc_opc_table_group2[183] = ppc_opc_stwux;
	ppc_opc_table_group2[200] = ppc_opc_subfzex;//+
	ppc_opc_table_group2[202] = ppc_opc_addzex;//+
	ppc_opc_table_group2[210] = ppc_opc_mtsr;
	ppc_opc_table_group2[215] = ppc_opc_stbx;
	ppc_opc_table_group2[232] = ppc_opc_subfmex;//+
	ppc_opc_table_group2[234] = ppc_opc_addmex;
	ppc_opc_table_group2[235] = ppc_opc_mullwx;//+
	ppc_opc_table_group2[242] = ppc_opc_mtsrin;
	ppc_opc_table_group2[246] = ppc_opc_dcbtst;
	ppc_opc_table_group2[247] = ppc_opc_stbux;
	ppc_opc_table_group2[266] = ppc_opc_addx;//+
	ppc_opc_table_group2[278] = ppc_opc_dcbt;
	ppc_opc_table_group2[279] = ppc_opc_lhzx;
	ppc_opc_table_group2[284] = ppc_opc_eqvx;
	ppc_opc_table_group2[306] = ppc_opc_tlbie;
	ppc_opc_table_group2[310] = ppc_opc_eciwx;
	ppc_opc_table_group2[311] = ppc_opc_lhzux;
	ppc_opc_table_group2[316] = ppc_opc_xorx;
	ppc_opc_table_group2[339] = ppc_opc_mfspr;
	ppc_opc_table_group2[343] = ppc_opc_lhax;
	ppc_opc_table_group2[370] = ppc_opc_tlbia;
	ppc_opc_table_group2[371] = ppc_opc_mftb;
	ppc_opc_table_group2[375] = ppc_opc_lhaux;
	ppc_opc_table_group2[407] = ppc_opc_sthx;
	ppc_opc_table_group2[412] = ppc_opc_orcx;
	ppc_opc_table_group2[438] = ppc_opc_ecowx;
	ppc_opc_table_group2[439] = ppc_opc_sthux;
	ppc_opc_table_group2[444] = ppc_opc_orx;
	ppc_opc_table_group2[459] = ppc_opc_divwux;//+
	ppc_opc_table_group2[467] = ppc_opc_mtspr;
	ppc_opc_table_group2[470] = ppc_opc_dcbi;
	ppc_opc_table_group2[476] = ppc_opc_nandx;
	ppc_opc_table_group2[491] = ppc_opc_divwx;//+
	ppc_opc_table_group2[512] = ppc_opc_mcrxr;
	ppc_opc_table_group2[533] = ppc_opc_lswx;
	ppc_opc_table_group2[534] = ppc_opc_lwbrx;
	ppc_opc_table_group2[535] = ppc_opc_lfsx;
	ppc_opc_table_group2[536] = ppc_opc_srwx;
	ppc_opc_table_group2[566] = ppc_opc_tlbsync;
	ppc_opc_table_group2[567] = ppc_opc_lfsux;
	ppc_opc_table_group2[595] = ppc_opc_mfsr;
	ppc_opc_table_group2[597] = ppc_opc_lswi;
	ppc_opc_table_group2[598] = ppc_opc_sync;
	ppc_opc_table_group2[599] = ppc_opc_lfdx;
	ppc_opc_table_group2[631] = ppc_opc_lfdux;
	ppc_opc_table_group2[659] = ppc_opc_mfsrin;
	ppc_opc_table_group2[661] = ppc_opc_stswx;
	ppc_opc_table_group2[662] = ppc_opc_stwbrx;
	ppc_opc_table_group2[663] = ppc_opc_stfsx;
	ppc_opc_table_group2[695] = ppc_opc_stfsux;
	ppc_opc_table_group2[725] = ppc_opc_stswi;
	ppc_opc_table_group2[727] = ppc_opc_stfdx;
	ppc_opc_table_group2[759] = ppc_opc_stfdux;
	ppc_opc_table_group2[790] = ppc_opc_lhbrx;
	ppc_opc_table_group2[792] = ppc_opc_srawx;
	ppc_opc_table_group2[824] = ppc_opc_srawix;
	ppc_opc_table_group2[854] = ppc_opc_eieio;
	ppc_opc_table_group2[918] = ppc_opc_sthbrx;
	ppc_opc_table_group2[922] = ppc_opc_extshx;
	ppc_opc_table_group2[954] = ppc_opc_extsbx;
	ppc_opc_table_group2[982] = ppc_opc_icbi;
	ppc_opc_table_group2[983] = ppc_opc_stfiwx;
	ppc_opc_table_group2[1014] = ppc_opc_dcbz;
	ppc_opc_table_gen_group2[0] = ppc_opc_gen_cmp;
	ppc_opc_table_gen_group2[4] = ppc_opc_gen_tw;
	ppc_opc_table_gen_group2[8] = ppc_opc_gen_subfcx;//+
	ppc_opc_table_gen_group2[10] = ppc_opc_gen_addcx;//+
	ppc_opc_table_gen_group2[11] = ppc_opc_gen_mulhwux;
	ppc_opc_table_gen_group2[19] = ppc_opc_gen_mfcr;
	ppc_opc_table_gen_group2[20] = ppc_opc_gen_lwarx;
	ppc_opc_table_gen_group2[23] = ppc_opc_gen_lwzx;
	ppc_opc_table_gen_group2[24] = ppc_opc_gen_slwx;
	ppc_opc_table_gen_group2[26] = ppc_opc_gen_cntlzwx;
	ppc_opc_table_gen_group2[28] = ppc_opc_gen_andx;
	ppc_opc_table_gen_group2[32] = ppc_opc_gen_cmpl;
	ppc_opc_table_gen_group2[40] = ppc_opc_gen_subfx;
	ppc_opc_table_gen_group2[54] = ppc_opc_gen_dcbst;
	ppc_opc_table_gen_group2[55] = ppc_opc_gen_lwzux;
	ppc_opc_table_gen_group2[60] = ppc_opc_gen_andcx;
	ppc_opc_table_gen_group2[75] = ppc_opc_gen_mulhwx;
	ppc_opc_table_gen_group2[83] = ppc_opc_gen_mfmsr;
	ppc_opc_table_gen_group2[86] = ppc_opc_gen_dcbf;
	ppc_opc_table_gen_group2[87] = ppc_opc_gen_lbzx;
	ppc_opc_table_gen_group2[104] = ppc_opc_gen_negx;
	ppc_opc_table_gen_group2[119] = ppc_opc_gen_lbzux;
	ppc_opc_table_gen_group2[124] = ppc_opc_gen_norx;
	ppc_opc_table_gen_group2[136] = ppc_opc_gen_subfex;//+
	ppc_opc_table_gen_group2[138] = ppc_opc_gen_addex;//+
	ppc_opc_table_gen_group2[144] = ppc_opc_gen_mtcrf;
	ppc_opc_table_gen_group2[146] = ppc_opc_gen_mtmsr;
	ppc_opc_table_gen_group2[150] = ppc_opc_gen_stwcx_;
	ppc_opc_table_gen_group2[151] = ppc_opc_gen_stwx;
	ppc_opc_table_gen_group2[183] = ppc_opc_gen_stwux;
	ppc_opc_table_gen_group2[200] = ppc_opc_gen_subfzex;//+
	ppc_opc_table_gen_group2[202] = ppc_opc_gen_addzex;//+
	ppc_opc_table_gen_group2[210] = ppc_opc_gen_mtsr;
	ppc_opc_table_gen_group2[215] = ppc_opc_gen_stbx;
	ppc_opc_table_gen_group2[232] = ppc_opc_gen_subfmex;//+
	ppc_opc_table_gen_group2[234] = ppc_opc_gen_addmex;
	ppc_opc_table_gen_group2[235] = ppc_opc_gen_mullwx;//+
	ppc_opc_table_gen_group2[242] = ppc_opc_gen_mtsrin;
	ppc_opc_table_gen_group2[246] = ppc_opc_gen_dcbtst;
	ppc_opc_table_gen_group2[247] = ppc_opc_gen_stbux;
	ppc_opc_table_gen_group2[266] = ppc_opc_gen_addx;//+
	ppc_opc_table_gen_group2[278] = ppc_opc_gen_dcbt;
	ppc_opc_table_gen_group2[279] = ppc_opc_gen_lhzx;
	ppc_opc_table_gen_group2[284] = ppc_opc_gen_eqvx;
	ppc_opc_table_gen_group2[306] = ppc_opc_gen_tlbie;
	ppc_opc_table_gen_group2[310] = ppc_opc_gen_eciwx;
	ppc_opc_table_gen_group2[311] = ppc_opc_gen_lhzux;
	ppc_opc_table_gen_group2[316] = ppc_opc_gen_xorx;
	ppc_opc_table_gen_group2[339] = ppc_opc_gen_mfspr;
	ppc_opc_table_gen_group2[343] = ppc_opc_gen_lhax;
	ppc_opc_table_gen_group2[370] = ppc_opc_gen_tlbia;
	ppc_opc_table_gen_group2[371] = ppc_opc_gen_mftb;
	ppc_opc_table_gen_group2[375] = ppc_opc_gen_lhaux;
	ppc_opc_table_gen_group2[407] = ppc_opc_gen_sthx;
	ppc_opc_table_gen_group2[412] = ppc_opc_gen_orcx;
	ppc_opc_table_gen_group2[438] = ppc_opc_gen_ecowx;
	ppc_opc_table_gen_group2[439] = ppc_opc_gen_sthux;
	ppc_opc_table_gen_group2[444] = ppc_opc_gen_orx;
	ppc_opc_table_gen_group2[459] = ppc_opc_gen_divwux;//+
	ppc_opc_table_gen_group2[467] = ppc_opc_gen_mtspr;
	ppc_opc_table_gen_group2[470] = ppc_opc_gen_dcbi;
	ppc_opc_table_gen_group2[476] = ppc_opc_gen_nandx;
	ppc_opc_table_gen_group2[491] = ppc_opc_gen_divwx;//+
	ppc_opc_table_gen_group2[512] = ppc_opc_gen_mcrxr;
	ppc_opc_table_gen_group2[533] = ppc_opc_gen_lswx;
	ppc_opc_table_gen_group2[534] = ppc_opc_gen_lwbrx;
	ppc_opc_table_gen_group2[535] = ppc_opc_gen_lfsx;
	ppc_opc_table_gen_group2[536] = ppc_opc_gen_srwx;
	ppc_opc_table_gen_group2[566] = ppc_opc_gen_tlbsync;
	ppc_opc_table_gen_group2[567] = ppc_opc_gen_lfsux;
	ppc_opc_table_gen_group2[595] = ppc_opc_gen_mfsr;
	ppc_opc_table_gen_group2[597] = ppc_opc_gen_lswi;
	ppc_opc_table_gen_group2[598] = ppc_opc_gen_sync;
	ppc_opc_table_gen_group2[599] = ppc_opc_gen_lfdx;
	ppc_opc_table_gen_group2[631] = ppc_opc_gen_lfdux;
	ppc_opc_table_gen_group2[659] = ppc_opc_gen_mfsrin;
	ppc_opc_table_gen_group2[661] = ppc_opc_gen_stswx;
	ppc_opc_table_gen_group2[662] = ppc_opc_gen_stwbrx;
	ppc_opc_table_gen_group2[663] = ppc_opc_gen_stfsx;
	ppc_opc_table_gen_group2[695] = ppc_opc_gen_stfsux;
	ppc_opc_table_gen_group2[725] = ppc_opc_gen_stswi;
	ppc_opc_table_gen_group2[727] = ppc_opc_gen_stfdx;
	ppc_opc_table_gen_group2[759] = ppc_opc_gen_stfdux;
	ppc_opc_table_gen_group2[790] = ppc_opc_gen_lhbrx;
	ppc_opc_table_gen_group2[792] = ppc_opc_gen_srawx;
	ppc_opc_table_gen_group2[824] = ppc_opc_gen_srawix;
	ppc_opc_table_gen_group2[854] = ppc_opc_gen_eieio;
	ppc_opc_table_gen_group2[918] = ppc_opc_gen_sthbrx;
	ppc_opc_table_gen_group2[922] = ppc_opc_gen_extshx;
	ppc_opc_table_gen_group2[954] = ppc_opc_gen_extsbx;
	ppc_opc_table_gen_group2[982] = ppc_opc_gen_icbi;
	ppc_opc_table_gen_group2[983] = ppc_opc_gen_stfiwx;
	ppc_opc_table_gen_group2[1014] = ppc_opc_gen_dcbz;
}

// main opcode 31
static void ppc_opc_group_2()
{
	uint32 ext = PPC_OPC_EXT(gCPU.current_opc);
	if (ext >= (sizeof ppc_opc_table_group2 / sizeof ppc_opc_table_group2[0])) {
		ppc_opc_invalid();
	}
	ppc_opc_table_group2[ext]();
}
static JITCFlow ppc_opc_gen_group_2()
{
	uint32 ext = PPC_OPC_EXT(gJITC.current_opc);
	if (ext >= (sizeof ppc_opc_table_group2 / sizeof ppc_opc_table_group2[0])) {
		return ppc_opc_gen_invalid();
	}
	return ppc_opc_table_gen_group2[ext]();
}

// main opcode 59
static void ppc_opc_group_f1()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	uint32 ext = PPC_OPC_EXT(gCPU.current_opc);
	switch (ext & 0x1f) {
		case 18: ppc_opc_fdivsx(); return;
		case 20: ppc_opc_fsubsx(); return;
		case 21: ppc_opc_faddsx(); return;
		case 22: ppc_opc_fsqrtsx(); return;
		case 24: ppc_opc_fresx(); return;
		case 25: ppc_opc_fmulsx(); return;
		case 28: ppc_opc_fmsubsx(); return;
		case 29: ppc_opc_fmaddsx(); return;
		case 30: ppc_opc_fnmsubsx(); return;
		case 31: ppc_opc_fnmaddsx(); return;
	}
	ppc_opc_invalid();
}
static JITCFlow ppc_opc_gen_group_f1()
{
	ppc_opc_gen_check_fpu();
	uint32 ext = PPC_OPC_EXT(gJITC.current_opc);
	switch (ext & 0x1f) {
		case 18: return ppc_opc_gen_fdivsx();
		case 20: return ppc_opc_gen_fsubsx();
		case 21: return ppc_opc_gen_faddsx();
		case 22: return ppc_opc_gen_fsqrtsx();
		case 24: return ppc_opc_gen_fresx();
		case 25: return ppc_opc_gen_fmulsx();
		case 28: return ppc_opc_gen_fmsubsx();
		case 29: return ppc_opc_gen_fmaddsx();
		case 30: return ppc_opc_gen_fnmsubsx();
		case 31: return ppc_opc_gen_fnmaddsx();
	}
	return ppc_opc_gen_invalid();
}

// main opcode 63
static void ppc_opc_group_f2()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	uint32 ext = PPC_OPC_EXT(gCPU.current_opc);
	if (ext & 16) {
		switch (ext & 0x1f) {
		case 18: ppc_opc_fdivx(); return;
		case 20: ppc_opc_fsubx(); return;
		case 21: ppc_opc_faddx(); return;
		case 22: ppc_opc_fsqrtx(); return;
		case 23: ppc_opc_fselx(); return;
		case 25: ppc_opc_fmulx(); return;
		case 26: ppc_opc_frsqrtex(); return;
		case 28: ppc_opc_fmsubx(); return;
		case 29: ppc_opc_fmaddx(); return;
		case 30: ppc_opc_fnmsubx(); return;
		case 31: ppc_opc_fnmaddx(); return;
		}
	} else {
		switch (ext) {
		case 0: ppc_opc_fcmpu(); return;
		case 12: ppc_opc_frspx(); return;
		case 14: ppc_opc_fctiwx(); return;
		case 15: ppc_opc_fctiwzx(); return;
		//--
		case 32: ppc_opc_fcmpo(); return;
		case 38: ppc_opc_mtfsb1x(); return;
		case 40: ppc_opc_fnegx(); return;
		case 64: ppc_opc_mcrfs(); return;
		case 70: ppc_opc_mtfsb0x(); return;
		case 72: ppc_opc_fmrx(); return;
		case 134: ppc_opc_mtfsfix(); return;
		case 136: ppc_opc_fnabsx(); return;
		case 264: ppc_opc_fabsx(); return;
		case 583: ppc_opc_mffsx(); return;
		case 711: ppc_opc_mtfsfx(); return;
		}
	}
	ppc_opc_invalid();
}
static JITCFlow ppc_opc_gen_group_f2()
{
	ppc_opc_gen_check_fpu();
	uint32 ext = PPC_OPC_EXT(gJITC.current_opc);
	if (ext & 16) {
		switch (ext & 0x1f) {
		case 18: return ppc_opc_gen_fdivx();
		case 20: return ppc_opc_gen_fsubx();
		case 21: return ppc_opc_gen_faddx();
		case 22: return ppc_opc_gen_fsqrtx();
		case 23: return ppc_opc_gen_fselx();
		case 25: return ppc_opc_gen_fmulx();
		case 26: return ppc_opc_gen_frsqrtex();
		case 28: return ppc_opc_gen_fmsubx();
		case 29: return ppc_opc_gen_fmaddx();
		case 30: return ppc_opc_gen_fnmsubx();
		case 31: return ppc_opc_gen_fnmaddx();
		}
	} else {
		switch (ext) {
		case 0: return ppc_opc_gen_fcmpu();
		case 12: return ppc_opc_gen_frspx();
		case 14: return ppc_opc_gen_fctiwx();
		case 15: return ppc_opc_gen_fctiwzx();
		//--
		case 32: return ppc_opc_gen_fcmpo();
		case 38: return ppc_opc_gen_mtfsb1x();
		case 40: return ppc_opc_gen_fnegx();
		case 64: return ppc_opc_gen_mcrfs();
		case 70: return ppc_opc_gen_mtfsb0x();
		case 72: return ppc_opc_gen_fmrx();
		case 134: return ppc_opc_gen_mtfsfix();
		case 136: return ppc_opc_gen_fnabsx();
		case 264: return ppc_opc_gen_fabsx();
		case 583: return ppc_opc_gen_mffsx();
		case 711: return ppc_opc_gen_mtfsfx();
		}
	}
	return ppc_opc_gen_invalid();
}

static ppc_opc_function ppc_opc_table_main[64] = {
	&ppc_opc_special,	//  0
	&ppc_opc_invalid,	//  1
	&ppc_opc_invalid,	//  2  (tdi on 64 bit platforms)
	&ppc_opc_twi,		//  3
	&ppc_opc_invalid,	//  4
	&ppc_opc_invalid,	//  5
	&ppc_opc_invalid,	//  6
	&ppc_opc_mulli,		//  7
	&ppc_opc_subfic,	//  8
	&ppc_opc_invalid,	//  9
	&ppc_opc_cmpli,		// 10
	&ppc_opc_cmpi,		// 11
	&ppc_opc_addic,		// 12
	&ppc_opc_addic_,	// 13
	&ppc_opc_addi,		// 14
	&ppc_opc_addis,		// 15
	&ppc_opc_bcx,		// 16
	&ppc_opc_sc,		// 17
	&ppc_opc_bx,		// 18
	&ppc_opc_group_1,	// 19
	&ppc_opc_rlwimix,	// 20
	&ppc_opc_rlwinmx,	// 21
	&ppc_opc_invalid,	// 22
	&ppc_opc_rlwnmx,	// 23
	&ppc_opc_ori,		// 24
	&ppc_opc_oris,		// 25
	&ppc_opc_xori,		// 26
	&ppc_opc_xoris,		// 27
	&ppc_opc_andi_,		// 28
	&ppc_opc_andis_,	// 29
	&ppc_opc_invalid,	// 30  (group_rld on 64 bit platforms)
	&ppc_opc_group_2,	// 31
	&ppc_opc_lwz,		// 32
	&ppc_opc_lwzu,		// 33
	&ppc_opc_lbz,		// 34
	&ppc_opc_lbzu,		// 35
	&ppc_opc_stw,		// 36
	&ppc_opc_stwu,		// 37
	&ppc_opc_stb,		// 38
	&ppc_opc_stbu,		// 39
	&ppc_opc_lhz,		// 40
	&ppc_opc_lhzu,		// 41
	&ppc_opc_lha,		// 42
	&ppc_opc_lhau,		// 43
	&ppc_opc_sth,		// 44
	&ppc_opc_sthu,		// 45
	&ppc_opc_lmw,		// 46
	&ppc_opc_stmw,		// 47
	&ppc_opc_lfs,		// 48
	&ppc_opc_lfsu,		// 49
	&ppc_opc_lfd,		// 50
	&ppc_opc_lfdu,		// 51
	&ppc_opc_stfs,		// 52
	&ppc_opc_stfsu,		// 53
	&ppc_opc_stfd,		// 54
	&ppc_opc_stfdu,		// 55
	&ppc_opc_invalid,	// 56
	&ppc_opc_invalid,	// 57
	&ppc_opc_invalid,	// 58  (ld on 64 bit platforms)
	&ppc_opc_group_f1,	// 59
	&ppc_opc_invalid,	// 60
	&ppc_opc_invalid,	// 61
	&ppc_opc_invalid,	// 62
	&ppc_opc_group_f2,	// 63
};
static ppc_opc_gen_function ppc_opc_table_gen_main[64] = {
	&ppc_opc_gen_special,	//  0
	&ppc_opc_gen_invalid,	//  1
	&ppc_opc_gen_invalid,	//  2  (tdi on 64 bit platforms)
	&ppc_opc_gen_twi,	//  3
	&ppc_opc_gen_invalid,	//  4
	&ppc_opc_gen_invalid,	//  5
	&ppc_opc_gen_invalid,	//  6
	&ppc_opc_gen_mulli,	//  7
	&ppc_opc_gen_subfic,	//  8
	&ppc_opc_gen_invalid,	//  9
	&ppc_opc_gen_cmpli,	// 10
	&ppc_opc_gen_cmpi,	// 11
	&ppc_opc_gen_addic,	// 12
	&ppc_opc_gen_addic_,	// 13
	&ppc_opc_gen_addi,	// 14
	&ppc_opc_gen_addis,	// 15
	&ppc_opc_gen_bcx,	// 16
	&ppc_opc_gen_sc,	// 17
	&ppc_opc_gen_bx,	// 18
	&ppc_opc_gen_group_1,	// 19
	&ppc_opc_gen_rlwimix,	// 20
	&ppc_opc_gen_rlwinmx,	// 21
	&ppc_opc_gen_invalid,	// 22
	&ppc_opc_gen_rlwnmx,	// 23
	&ppc_opc_gen_ori,	// 24
	&ppc_opc_gen_oris,	// 25
	&ppc_opc_gen_xori,	// 26
	&ppc_opc_gen_xoris,	// 27
	&ppc_opc_gen_andi_,	// 28
	&ppc_opc_gen_andis_,	// 29
	&ppc_opc_gen_invalid,	// 30  (group_rld on 64 bit platforms)
	&ppc_opc_gen_group_2,	// 31
	&ppc_opc_gen_lwz,	// 32
	&ppc_opc_gen_lwzu,	// 33
	&ppc_opc_gen_lbz,	// 34
	&ppc_opc_gen_lbzu,	// 35
	&ppc_opc_gen_stw,	// 36
	&ppc_opc_gen_stwu,	// 37
	&ppc_opc_gen_stb,	// 38
	&ppc_opc_gen_stbu,	// 39
	&ppc_opc_gen_lhz,	// 40
	&ppc_opc_gen_lhzu,	// 41
	&ppc_opc_gen_lha,	// 42
	&ppc_opc_gen_lhau,	// 43
	&ppc_opc_gen_sth,	// 44
	&ppc_opc_gen_sthu,	// 45
	&ppc_opc_gen_lmw,	// 46
	&ppc_opc_gen_stmw,	// 47
	&ppc_opc_gen_lfs,	// 48
	&ppc_opc_gen_lfsu,	// 49
	&ppc_opc_gen_lfd,	// 50
	&ppc_opc_gen_lfdu,	// 51
	&ppc_opc_gen_stfs,	// 52
	&ppc_opc_gen_stfsu,	// 53
	&ppc_opc_gen_stfd,	// 54
	&ppc_opc_gen_stfdu,	// 55
	&ppc_opc_gen_invalid,	// 56
	&ppc_opc_gen_invalid,	// 57
	&ppc_opc_gen_invalid,	// 58  (ld on 64 bit platforms)
	&ppc_opc_gen_group_f1,	// 59
	&ppc_opc_gen_invalid,	// 60
	&ppc_opc_gen_invalid,	// 61
	&ppc_opc_gen_invalid,	// 62
	&ppc_opc_gen_group_f2,	// 63
};

void FASTCALL ppc_exec_opc()
{
	uint32 mainopc = PPC_OPC_MAIN(gCPU.current_opc);
	ppc_opc_table_main[mainopc]();
}

JITCFlow FASTCALL ppc_gen_opc()
{
	uint32 mainopc = PPC_OPC_MAIN(gJITC.current_opc);
	return ppc_opc_table_gen_main[mainopc]();
}

void ppc_dec_init()
{
	ppc_opc_init_group2();
}
