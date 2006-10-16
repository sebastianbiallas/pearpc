/*
 *	PearPC
 *	ppc_dec.cc
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


#include <cstring>

#include "system/types.h"
#include "debug/tracers.h"
#include "cpu/cpu.h"
#include "ppc_alu.h"
#include "ppc_cpu.h"
#include "ppc_dec.h"
#include "ppc_esc.h"
#include "ppc_exc.h"
#include "ppc_fpu.h"
#include "ppc_vec.h"
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
	jitcClobberAll();
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
	ppc_opc_table_group2[758] = ppc_opc_dcba;
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
	ppc_opc_table_gen_group2[758] = ppc_opc_gen_dcba;
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

	if ((ppc_cpu_get_pvr(0) & 0xffff0000) == 0x000c0000) {
		/* Added for Altivec support */
		ppc_opc_table_group2[6] = ppc_opc_lvsl;
		ppc_opc_table_group2[7] = ppc_opc_lvebx;
		ppc_opc_table_group2[38] = ppc_opc_lvsr;
		ppc_opc_table_group2[39] = ppc_opc_lvehx;
		ppc_opc_table_group2[71] = ppc_opc_lvewx;
		ppc_opc_table_group2[103] = ppc_opc_lvx;
		ppc_opc_table_group2[135] = ppc_opc_stvebx;
		ppc_opc_table_group2[167] = ppc_opc_stvehx;
		ppc_opc_table_group2[199] = ppc_opc_stvewx;
		ppc_opc_table_group2[231] = ppc_opc_stvx;
		ppc_opc_table_group2[342] = ppc_opc_dst;
		ppc_opc_table_group2[359] = ppc_opc_lvxl;
		ppc_opc_table_group2[374] = ppc_opc_dstst;
		ppc_opc_table_group2[487] = ppc_opc_stvxl;
		ppc_opc_table_group2[822] = ppc_opc_dss;

		ppc_opc_table_gen_group2[6] = ppc_opc_gen_lvsl;
		ppc_opc_table_gen_group2[7] = ppc_opc_gen_lvebx;
		ppc_opc_table_gen_group2[38] = ppc_opc_gen_lvsr;
		ppc_opc_table_gen_group2[39] = ppc_opc_gen_lvehx;
		ppc_opc_table_gen_group2[71] = ppc_opc_gen_lvewx;
		ppc_opc_table_gen_group2[103] = ppc_opc_gen_lvx;
		ppc_opc_table_gen_group2[135] = ppc_opc_gen_stvebx;
		ppc_opc_table_gen_group2[167] = ppc_opc_gen_stvehx;
		ppc_opc_table_gen_group2[199] = ppc_opc_gen_stvewx;
		ppc_opc_table_gen_group2[231] = ppc_opc_gen_stvx;
		ppc_opc_table_gen_group2[342] = ppc_opc_gen_dst;
		ppc_opc_table_gen_group2[359] = ppc_opc_gen_lvxl;
		ppc_opc_table_gen_group2[374] = ppc_opc_gen_dstst;
		ppc_opc_table_gen_group2[487] = ppc_opc_gen_stvxl;
		ppc_opc_table_gen_group2[822] = ppc_opc_gen_dss;
	}
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

ppc_opc_function ppc_opc_table_groupv[965];
ppc_opc_gen_function ppc_opc_table_gen_groupv[965];

static void ppc_opc_init_groupv()
{
	for (uint i=0; i<(sizeof ppc_opc_table_groupv / sizeof ppc_opc_table_groupv[0]);i++) {
		ppc_opc_table_groupv[i] = ppc_opc_invalid;
		ppc_opc_table_gen_groupv[i] = ppc_opc_gen_invalid;
	}
	ppc_opc_table_groupv[0] = ppc_opc_vaddubm;
	ppc_opc_table_groupv[1] = ppc_opc_vmaxub;
	ppc_opc_table_groupv[2] = ppc_opc_vrlb;
	ppc_opc_table_groupv[4] = ppc_opc_vmuloub;
	ppc_opc_table_groupv[5] = ppc_opc_vaddfp;
	ppc_opc_table_groupv[6] = ppc_opc_vmrghb;
	ppc_opc_table_groupv[7] = ppc_opc_vpkuhum;
	ppc_opc_table_groupv[32] = ppc_opc_vadduhm;
	ppc_opc_table_groupv[33] = ppc_opc_vmaxuh;
	ppc_opc_table_groupv[34] = ppc_opc_vrlh;
	ppc_opc_table_groupv[36] = ppc_opc_vmulouh;
	ppc_opc_table_groupv[37] = ppc_opc_vsubfp;
	ppc_opc_table_groupv[38] = ppc_opc_vmrghh;
	ppc_opc_table_groupv[39] = ppc_opc_vpkuwum;
	ppc_opc_table_groupv[42] = ppc_opc_vpkpx;
	ppc_opc_table_groupv[64] = ppc_opc_vadduwm;
	ppc_opc_table_groupv[65] = ppc_opc_vmaxuw;
	ppc_opc_table_groupv[66] = ppc_opc_vrlw;
	ppc_opc_table_groupv[70] = ppc_opc_vmrghw;
	ppc_opc_table_groupv[71] = ppc_opc_vpkuhus;
	ppc_opc_table_groupv[103] = ppc_opc_vpkuwus;
	ppc_opc_table_groupv[129] = ppc_opc_vmaxsb;
	ppc_opc_table_groupv[130] = ppc_opc_vslb;
	ppc_opc_table_groupv[132] = ppc_opc_vmulosb;
	ppc_opc_table_groupv[133] = ppc_opc_vrefp;
	ppc_opc_table_groupv[134] = ppc_opc_vmrglb;
	ppc_opc_table_groupv[135] = ppc_opc_vpkshus;
	ppc_opc_table_groupv[161] = ppc_opc_vmaxsh;
	ppc_opc_table_groupv[162] = ppc_opc_vslh;
	ppc_opc_table_groupv[164] = ppc_opc_vmulosh;
	ppc_opc_table_groupv[165] = ppc_opc_vrsqrtefp;
	ppc_opc_table_groupv[166] = ppc_opc_vmrglh;
	ppc_opc_table_groupv[167] = ppc_opc_vpkswus;
	ppc_opc_table_groupv[192] = ppc_opc_vaddcuw;
	ppc_opc_table_groupv[193] = ppc_opc_vmaxsw;
	ppc_opc_table_groupv[194] = ppc_opc_vslw;
	ppc_opc_table_groupv[197] = ppc_opc_vexptefp;
	ppc_opc_table_groupv[198] = ppc_opc_vmrglw;
	ppc_opc_table_groupv[199] = ppc_opc_vpkshss;
	ppc_opc_table_groupv[226] = ppc_opc_vsl;
	ppc_opc_table_groupv[229] = ppc_opc_vlogefp;
	ppc_opc_table_groupv[231] = ppc_opc_vpkswss;
	ppc_opc_table_groupv[256] = ppc_opc_vaddubs;
	ppc_opc_table_groupv[257] = ppc_opc_vminub;
	ppc_opc_table_groupv[258] = ppc_opc_vsrb;
	ppc_opc_table_groupv[260] = ppc_opc_vmuleub;
	ppc_opc_table_groupv[261] = ppc_opc_vrfin;
	ppc_opc_table_groupv[262] = ppc_opc_vspltb;
	ppc_opc_table_groupv[263] = ppc_opc_vupkhsb;
	ppc_opc_table_groupv[288] = ppc_opc_vadduhs;
	ppc_opc_table_groupv[289] = ppc_opc_vminuh;
	ppc_opc_table_groupv[290] = ppc_opc_vsrh;
	ppc_opc_table_groupv[292] = ppc_opc_vmuleuh;
	ppc_opc_table_groupv[293] = ppc_opc_vrfiz;
	ppc_opc_table_groupv[294] = ppc_opc_vsplth;
	ppc_opc_table_groupv[295] = ppc_opc_vupkhsh;
	ppc_opc_table_groupv[320] = ppc_opc_vadduws;
	ppc_opc_table_groupv[321] = ppc_opc_vminuw;
	ppc_opc_table_groupv[322] = ppc_opc_vsrw;
	ppc_opc_table_groupv[325] = ppc_opc_vrfip;
	ppc_opc_table_groupv[326] = ppc_opc_vspltw;
	ppc_opc_table_groupv[327] = ppc_opc_vupklsb;
	ppc_opc_table_groupv[354] = ppc_opc_vsr;
	ppc_opc_table_groupv[357] = ppc_opc_vrfim;
	ppc_opc_table_groupv[359] = ppc_opc_vupklsh;
	ppc_opc_table_groupv[384] = ppc_opc_vaddsbs;
	ppc_opc_table_groupv[385] = ppc_opc_vminsb;
	ppc_opc_table_groupv[386] = ppc_opc_vsrab;
	ppc_opc_table_groupv[388] = ppc_opc_vmulesb;
	ppc_opc_table_groupv[389] = ppc_opc_vcfux;
	ppc_opc_table_groupv[390] = ppc_opc_vspltisb;
	ppc_opc_table_groupv[391] = ppc_opc_vpkpx;
	ppc_opc_table_groupv[416] = ppc_opc_vaddshs;
	ppc_opc_table_groupv[417] = ppc_opc_vminsh;
	ppc_opc_table_groupv[418] = ppc_opc_vsrah;
	ppc_opc_table_groupv[420] = ppc_opc_vmulesh;
	ppc_opc_table_groupv[421] = ppc_opc_vcfsx;
	ppc_opc_table_groupv[422] = ppc_opc_vspltish;
	ppc_opc_table_groupv[423] = ppc_opc_vupkhpx;
	ppc_opc_table_groupv[448] = ppc_opc_vaddsws;
	ppc_opc_table_groupv[449] = ppc_opc_vminsw;
	ppc_opc_table_groupv[450] = ppc_opc_vsraw;
	ppc_opc_table_groupv[453] = ppc_opc_vctuxs;
	ppc_opc_table_groupv[454] = ppc_opc_vspltisw;
	ppc_opc_table_groupv[485] = ppc_opc_vctsxs;
	ppc_opc_table_groupv[487] = ppc_opc_vupklpx;
	ppc_opc_table_groupv[512] = ppc_opc_vsububm;
	ppc_opc_table_groupv[513] = ppc_opc_vavgub;
	ppc_opc_table_groupv[514] = ppc_opc_vand;
	ppc_opc_table_groupv[517] = ppc_opc_vmaxfp;
	ppc_opc_table_groupv[518] = ppc_opc_vslo;
	ppc_opc_table_groupv[544] = ppc_opc_vsubuhm;
	ppc_opc_table_groupv[545] = ppc_opc_vavguh;
	ppc_opc_table_groupv[546] = ppc_opc_vandc;
	ppc_opc_table_groupv[549] = ppc_opc_vminfp;
	ppc_opc_table_groupv[550] = ppc_opc_vsro;
	ppc_opc_table_groupv[576] = ppc_opc_vsubuwm;
	ppc_opc_table_groupv[577] = ppc_opc_vavguw;
	ppc_opc_table_groupv[578] = ppc_opc_vor;
	ppc_opc_table_groupv[610] = ppc_opc_vxor;
	ppc_opc_table_groupv[641] = ppc_opc_vavgsb;
	ppc_opc_table_groupv[642] = ppc_opc_vnor;
	ppc_opc_table_groupv[673] = ppc_opc_vavgsh;
	ppc_opc_table_groupv[704] = ppc_opc_vsubcuw;
	ppc_opc_table_groupv[705] = ppc_opc_vavgsw;
	ppc_opc_table_groupv[768] = ppc_opc_vsububs;
	ppc_opc_table_groupv[770] = ppc_opc_mfvscr;
	ppc_opc_table_groupv[772] = ppc_opc_vsum4ubs;
	ppc_opc_table_groupv[800] = ppc_opc_vsubuhs;
	ppc_opc_table_groupv[802] = ppc_opc_mtvscr;
	ppc_opc_table_groupv[804] = ppc_opc_vsum4shs;
	ppc_opc_table_groupv[832] = ppc_opc_vsubuws;
	ppc_opc_table_groupv[836] = ppc_opc_vsum2sws;
	ppc_opc_table_groupv[896] = ppc_opc_vsubsbs;
	ppc_opc_table_groupv[900] = ppc_opc_vsum4sbs;
	ppc_opc_table_groupv[928] = ppc_opc_vsubshs;
	ppc_opc_table_groupv[960] = ppc_opc_vsubsws;
	ppc_opc_table_groupv[964] = ppc_opc_vsumsws;

	ppc_opc_table_gen_groupv[0] = ppc_opc_gen_vaddubm;
	ppc_opc_table_gen_groupv[1] = ppc_opc_gen_vmaxub;
	ppc_opc_table_gen_groupv[2] = ppc_opc_gen_vrlb;
	ppc_opc_table_gen_groupv[4] = ppc_opc_gen_vmuloub;
	ppc_opc_table_gen_groupv[5] = ppc_opc_gen_vaddfp;
	ppc_opc_table_gen_groupv[6] = ppc_opc_gen_vmrghb;
	ppc_opc_table_gen_groupv[7] = ppc_opc_gen_vpkuhum;
	ppc_opc_table_gen_groupv[32] = ppc_opc_gen_vadduhm;
	ppc_opc_table_gen_groupv[33] = ppc_opc_gen_vmaxuh;
	ppc_opc_table_gen_groupv[34] = ppc_opc_gen_vrlh;
	ppc_opc_table_gen_groupv[36] = ppc_opc_gen_vmulouh;
	ppc_opc_table_gen_groupv[37] = ppc_opc_gen_vsubfp;
	ppc_opc_table_gen_groupv[38] = ppc_opc_gen_vmrghh;
	ppc_opc_table_gen_groupv[39] = ppc_opc_gen_vpkuwum;
	ppc_opc_table_gen_groupv[42] = ppc_opc_gen_vpkpx;
	ppc_opc_table_gen_groupv[64] = ppc_opc_gen_vadduwm;
	ppc_opc_table_gen_groupv[65] = ppc_opc_gen_vmaxuw;
	ppc_opc_table_gen_groupv[66] = ppc_opc_gen_vrlw;
	ppc_opc_table_gen_groupv[70] = ppc_opc_gen_vmrghw;
	ppc_opc_table_gen_groupv[71] = ppc_opc_gen_vpkuhus;
	ppc_opc_table_gen_groupv[103] = ppc_opc_gen_vpkuwus;
	ppc_opc_table_gen_groupv[129] = ppc_opc_gen_vmaxsb;
	ppc_opc_table_gen_groupv[130] = ppc_opc_gen_vslb;
	ppc_opc_table_gen_groupv[132] = ppc_opc_gen_vmulosb;
	ppc_opc_table_gen_groupv[133] = ppc_opc_gen_vrefp;
	ppc_opc_table_gen_groupv[134] = ppc_opc_gen_vmrglb;
	ppc_opc_table_gen_groupv[135] = ppc_opc_gen_vpkshus;
	ppc_opc_table_gen_groupv[161] = ppc_opc_gen_vmaxsh;
	ppc_opc_table_gen_groupv[162] = ppc_opc_gen_vslh;
	ppc_opc_table_gen_groupv[164] = ppc_opc_gen_vmulosh;
	ppc_opc_table_gen_groupv[165] = ppc_opc_gen_vrsqrtefp;
	ppc_opc_table_gen_groupv[166] = ppc_opc_gen_vmrglh;
	ppc_opc_table_gen_groupv[167] = ppc_opc_gen_vpkswus;
	ppc_opc_table_gen_groupv[192] = ppc_opc_gen_vaddcuw;
	ppc_opc_table_gen_groupv[193] = ppc_opc_gen_vmaxsw;
	ppc_opc_table_gen_groupv[194] = ppc_opc_gen_vslw;
	ppc_opc_table_gen_groupv[197] = ppc_opc_gen_vexptefp;
	ppc_opc_table_gen_groupv[198] = ppc_opc_gen_vmrglw;
	ppc_opc_table_gen_groupv[199] = ppc_opc_gen_vpkshss;
	ppc_opc_table_gen_groupv[226] = ppc_opc_gen_vsl;
	ppc_opc_table_gen_groupv[229] = ppc_opc_gen_vlogefp;
	ppc_opc_table_gen_groupv[231] = ppc_opc_gen_vpkswss;
	ppc_opc_table_gen_groupv[256] = ppc_opc_gen_vaddubs;
	ppc_opc_table_gen_groupv[257] = ppc_opc_gen_vminub;
	ppc_opc_table_gen_groupv[258] = ppc_opc_gen_vsrb;
	ppc_opc_table_gen_groupv[260] = ppc_opc_gen_vmuleub;
	ppc_opc_table_gen_groupv[261] = ppc_opc_gen_vrfin;
	ppc_opc_table_gen_groupv[262] = ppc_opc_gen_vspltb;
	ppc_opc_table_gen_groupv[263] = ppc_opc_gen_vupkhsb;
	ppc_opc_table_gen_groupv[288] = ppc_opc_gen_vadduhs;
	ppc_opc_table_gen_groupv[289] = ppc_opc_gen_vminuh;
	ppc_opc_table_gen_groupv[290] = ppc_opc_gen_vsrh;
	ppc_opc_table_gen_groupv[292] = ppc_opc_gen_vmuleuh;
	ppc_opc_table_gen_groupv[293] = ppc_opc_gen_vrfiz;
	ppc_opc_table_gen_groupv[294] = ppc_opc_gen_vsplth;
	ppc_opc_table_gen_groupv[295] = ppc_opc_gen_vupkhsh;
	ppc_opc_table_gen_groupv[320] = ppc_opc_gen_vadduws;
	ppc_opc_table_gen_groupv[321] = ppc_opc_gen_vminuw;
	ppc_opc_table_gen_groupv[322] = ppc_opc_gen_vsrw;
	ppc_opc_table_gen_groupv[325] = ppc_opc_gen_vrfip;
	ppc_opc_table_gen_groupv[326] = ppc_opc_gen_vspltw;
	ppc_opc_table_gen_groupv[327] = ppc_opc_gen_vupklsb;
	ppc_opc_table_gen_groupv[354] = ppc_opc_gen_vsr;
	ppc_opc_table_gen_groupv[357] = ppc_opc_gen_vrfim;
	ppc_opc_table_gen_groupv[359] = ppc_opc_gen_vupklsh;
	ppc_opc_table_gen_groupv[384] = ppc_opc_gen_vaddsbs;
	ppc_opc_table_gen_groupv[385] = ppc_opc_gen_vminsb;
	ppc_opc_table_gen_groupv[386] = ppc_opc_gen_vsrab;
	ppc_opc_table_gen_groupv[388] = ppc_opc_gen_vmulesb;
	ppc_opc_table_gen_groupv[389] = ppc_opc_gen_vcfux;
	ppc_opc_table_gen_groupv[390] = ppc_opc_gen_vspltisb;
	ppc_opc_table_gen_groupv[391] = ppc_opc_gen_vpkpx;
	ppc_opc_table_gen_groupv[416] = ppc_opc_gen_vaddshs;
	ppc_opc_table_gen_groupv[417] = ppc_opc_gen_vminsh;
	ppc_opc_table_gen_groupv[418] = ppc_opc_gen_vsrah;
	ppc_opc_table_gen_groupv[420] = ppc_opc_gen_vmulesh;
	ppc_opc_table_gen_groupv[421] = ppc_opc_gen_vcfsx;
	ppc_opc_table_gen_groupv[422] = ppc_opc_gen_vspltish;
	ppc_opc_table_gen_groupv[423] = ppc_opc_gen_vupkhpx;
	ppc_opc_table_gen_groupv[448] = ppc_opc_gen_vaddsws;
	ppc_opc_table_gen_groupv[449] = ppc_opc_gen_vminsw;
	ppc_opc_table_gen_groupv[450] = ppc_opc_gen_vsraw;
	ppc_opc_table_gen_groupv[453] = ppc_opc_gen_vctuxs;
	ppc_opc_table_gen_groupv[454] = ppc_opc_gen_vspltisw;
	ppc_opc_table_gen_groupv[485] = ppc_opc_gen_vctsxs;
	ppc_opc_table_gen_groupv[487] = ppc_opc_gen_vupklpx;
	ppc_opc_table_gen_groupv[512] = ppc_opc_gen_vsububm;
	ppc_opc_table_gen_groupv[513] = ppc_opc_gen_vavgub;
	ppc_opc_table_gen_groupv[514] = ppc_opc_gen_vand;
	ppc_opc_table_gen_groupv[517] = ppc_opc_gen_vmaxfp;
	ppc_opc_table_gen_groupv[518] = ppc_opc_gen_vslo;
	ppc_opc_table_gen_groupv[544] = ppc_opc_gen_vsubuhm;
	ppc_opc_table_gen_groupv[545] = ppc_opc_gen_vavguh;
	ppc_opc_table_gen_groupv[546] = ppc_opc_gen_vandc;
	ppc_opc_table_gen_groupv[549] = ppc_opc_gen_vminfp;
	ppc_opc_table_gen_groupv[550] = ppc_opc_gen_vsro;
	ppc_opc_table_gen_groupv[576] = ppc_opc_gen_vsubuwm;
	ppc_opc_table_gen_groupv[577] = ppc_opc_gen_vavguw;
	ppc_opc_table_gen_groupv[578] = ppc_opc_gen_vor;
	ppc_opc_table_gen_groupv[610] = ppc_opc_gen_vxor;
	ppc_opc_table_gen_groupv[641] = ppc_opc_gen_vavgsb;
	ppc_opc_table_gen_groupv[642] = ppc_opc_gen_vnor;
	ppc_opc_table_gen_groupv[673] = ppc_opc_gen_vavgsh;
	ppc_opc_table_gen_groupv[704] = ppc_opc_gen_vsubcuw;
	ppc_opc_table_gen_groupv[705] = ppc_opc_gen_vavgsw;
	ppc_opc_table_gen_groupv[768] = ppc_opc_gen_vsububs;
	ppc_opc_table_gen_groupv[770] = ppc_opc_gen_mfvscr;
	ppc_opc_table_gen_groupv[772] = ppc_opc_gen_vsum4ubs;
	ppc_opc_table_gen_groupv[800] = ppc_opc_gen_vsubuhs;
	ppc_opc_table_gen_groupv[802] = ppc_opc_gen_mtvscr;
	ppc_opc_table_gen_groupv[804] = ppc_opc_gen_vsum4shs;
	ppc_opc_table_gen_groupv[832] = ppc_opc_gen_vsubuws;
	ppc_opc_table_gen_groupv[836] = ppc_opc_gen_vsum2sws;
	ppc_opc_table_gen_groupv[896] = ppc_opc_gen_vsubsbs;
	ppc_opc_table_gen_groupv[900] = ppc_opc_gen_vsum4sbs;
	ppc_opc_table_gen_groupv[928] = ppc_opc_gen_vsubshs;
	ppc_opc_table_gen_groupv[960] = ppc_opc_gen_vsubsws;
	ppc_opc_table_gen_groupv[964] = ppc_opc_gen_vsumsws;

	/* Put any MMX/SSE/SSE2 optimizations here under conditional
	 *   of a CPU caps.
	 */
}

// main opcode 04
static void ppc_opc_group_v()
{
	uint32 ext = PPC_OPC_EXT(gCPU.current_opc);
#ifndef  __VEC_EXC_OFF__
	if ((gCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	switch(ext & 0x1f) {
		case 16:
			if (gCPU.current_opc & PPC_OPC_Rc)
				return ppc_opc_vmhraddshs();
			else
				return ppc_opc_vmhaddshs();
		case 17:	return ppc_opc_vmladduhm();
		case 18:
			if (gCPU.current_opc & PPC_OPC_Rc)
				return ppc_opc_vmsummbm();
			else
				return ppc_opc_vmsumubm();
		case 19:
			if (gCPU.current_opc & PPC_OPC_Rc)
				return ppc_opc_vmsumuhs();
			else
				return ppc_opc_vmsumuhm();
		case 20:
			if (gCPU.current_opc & PPC_OPC_Rc)
				return ppc_opc_vmsumshs();
			else
				return ppc_opc_vmsumshm();
		case 21:
			if (gCPU.current_opc & PPC_OPC_Rc)
				return ppc_opc_vperm();
			else
				return ppc_opc_vsel();
		case 22:	return ppc_opc_vsldoi();
		case 23:
			if (gCPU.current_opc & PPC_OPC_Rc)
				return ppc_opc_vnmsubfp();
			else
				return ppc_opc_vmaddfp();
	}
	switch(ext & 0x1ff)
	{
		case 3: return ppc_opc_vcmpequbx();
		case 35: return ppc_opc_vcmpequhx();
		case 67: return ppc_opc_vcmpequwx();
		case 99: return ppc_opc_vcmpeqfpx();
		case 227: return ppc_opc_vcmpgefpx();
		case 259: return ppc_opc_vcmpgtubx();
		case 291: return ppc_opc_vcmpgtuhx();
		case 323: return ppc_opc_vcmpgtuwx();
		case 355: return ppc_opc_vcmpgtfpx();
		case 387: return ppc_opc_vcmpgtsbx();
		case 419: return ppc_opc_vcmpgtshx();
		case 451: return ppc_opc_vcmpgtswx();
		case 483: return ppc_opc_vcmpbfpx();
	}

	if (ext >= (sizeof ppc_opc_table_groupv / sizeof ppc_opc_table_groupv[0])) {
		return ppc_opc_invalid();
	}
	return ppc_opc_table_groupv[ext]();
}

// main opcode 04
static JITCFlow ppc_opc_gen_group_v()
{
	uint32 ext = PPC_OPC_EXT(gJITC.current_opc);
	int vrS = (gJITC.current_opc >> 21) & 0x1f;

	ppc_opc_gen_check_vec();

	if (vrS == gJITC.nativeVectorReg) {
		gJITC.nativeVectorReg = VECTREG_NO;
	}

	switch(ext & 0x1f) {
		case 16:
			if (gJITC.current_opc & PPC_OPC_Rc)
				return ppc_opc_gen_vmhraddshs();
			else
				return ppc_opc_gen_vmhaddshs();
		case 17:	return ppc_opc_gen_vmladduhm();
		case 18:
			if (gJITC.current_opc & PPC_OPC_Rc)
				return ppc_opc_gen_vmsummbm();
			else
				return ppc_opc_gen_vmsumubm();
		case 19:
			if (gJITC.current_opc & PPC_OPC_Rc)
				return ppc_opc_gen_vmsumuhs();
			else
				return ppc_opc_gen_vmsumuhm();
		case 20:
			if (gJITC.current_opc & PPC_OPC_Rc)
				return ppc_opc_gen_vmsumshs();
			else
				return ppc_opc_gen_vmsumshm();
		case 21:
			if (gJITC.current_opc & PPC_OPC_Rc)
				return ppc_opc_gen_vperm();
			else
				return ppc_opc_gen_vsel();
		case 22:	return ppc_opc_gen_vsldoi();
		case 23:
			if (gJITC.current_opc & PPC_OPC_Rc)
				return ppc_opc_gen_vnmsubfp();
			else
				return ppc_opc_gen_vmaddfp();
	}
	switch(ext & 0x1ff)
	{
		case 3: return ppc_opc_gen_vcmpequbx();
		case 35: return ppc_opc_gen_vcmpequhx();
		case 67: return ppc_opc_gen_vcmpequwx();
		case 99: return ppc_opc_gen_vcmpeqfpx();
		case 227: return ppc_opc_gen_vcmpgefpx();
		case 259: return ppc_opc_gen_vcmpgtubx();
		case 291: return ppc_opc_gen_vcmpgtuhx();
		case 323: return ppc_opc_gen_vcmpgtuwx();
		case 355: return ppc_opc_gen_vcmpgtfpx();
		case 387: return ppc_opc_gen_vcmpgtsbx();
		case 419: return ppc_opc_gen_vcmpgtshx();
		case 451: return ppc_opc_gen_vcmpgtswx();
		case 483: return ppc_opc_gen_vcmpbfpx();
	}

	if (ext >= (sizeof ppc_opc_table_gen_groupv / sizeof ppc_opc_table_gen_groupv[0])) {
		return ppc_opc_gen_invalid();
	}
	return ppc_opc_table_gen_groupv[ext]();
}


static ppc_opc_function ppc_opc_table_main[64] = {
	&ppc_opc_special,	//  0
	&ppc_opc_invalid,	//  1
	&ppc_opc_invalid,	//  2  (tdi on 64 bit platforms)
	&ppc_opc_twi,		//  3
	&ppc_opc_invalid,	//  4  (altivec)
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
	&ppc_opc_gen_invalid,	//  4  (altivec)
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

static uint32 last_instr = 0, instr_report = 0, instr_count = 0;

JITCFlow FASTCALL ppc_gen_opc()
{
	uint32 mainopc = PPC_OPC_MAIN(gJITC.current_opc);

	/*if (instr_report) {
		uint32 ext = PPC_OPC_EXT(gJITC.current_opc);
		uint32 rD, rA, rB, rC;
		PPC_OPC_TEMPL_A(gJITC.current_opc, rD, rA, rB, rC);

		printf("*** (%08x) %08x (%i, %i) (%i, %i, %i, %i)\n", instr_count++, gJITC.current_opc, mainopc, ext, rD, rA, rB, rC);
	} else {
		if (last_instr == 0x7c00004c && gJITC.current_opc == 0x10e0038c) {
			instr_report = 1;
		}

		last_instr = gJITC.current_opc;
	}*/

	return ppc_opc_table_gen_main[mainopc]();
}

void ppc_dec_init()
{
	ppc_opc_init_group2();
	if ((ppc_cpu_get_pvr(0) & 0xffff0000) == 0x000c0000) {
		ht_printf("[PPC/VEC] Vector Address: %08x\n", (uint32)&gCPU.vr[0]);
		ppc_opc_table_main[4] = ppc_opc_group_v;
		ppc_opc_table_gen_main[4] = ppc_opc_gen_group_v;
		ppc_opc_init_groupv();
	}
}
