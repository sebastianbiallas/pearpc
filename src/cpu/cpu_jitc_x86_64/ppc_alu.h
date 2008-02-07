/*
 *	PearPC
 *	ppc_alu.h
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

#ifndef __PPC_ALU_H__
#define __PPC_ALU_H__

#include "jitc_types.h"

void ppc_opc_addx(PPC_CPU_State &aCPU);
void ppc_opc_addcx(PPC_CPU_State &aCPU);
void ppc_opc_addex(PPC_CPU_State &aCPU);
void ppc_opc_addi(PPC_CPU_State &aCPU);
void ppc_opc_addic(PPC_CPU_State &aCPU);
void ppc_opc_addic_(PPC_CPU_State &aCPU);
void ppc_opc_addis(PPC_CPU_State &aCPU);
void ppc_opc_addmex(PPC_CPU_State &aCPU);
void ppc_opc_addzex(PPC_CPU_State &aCPU);

void ppc_opc_andx(PPC_CPU_State &aCPU);
void ppc_opc_andcx(PPC_CPU_State &aCPU);
void ppc_opc_andi_(PPC_CPU_State &aCPU);
void ppc_opc_andis_(PPC_CPU_State &aCPU);

void ppc_opc_cmp(PPC_CPU_State &aCPU);
void ppc_opc_cmpi(PPC_CPU_State &aCPU);
void ppc_opc_cmpl(PPC_CPU_State &aCPU);
void ppc_opc_cmpli(PPC_CPU_State &aCPU);

void ppc_opc_cntlzwx(PPC_CPU_State &aCPU);

void ppc_opc_crand(PPC_CPU_State &aCPU);
void ppc_opc_crandc(PPC_CPU_State &aCPU);
void ppc_opc_creqv(PPC_CPU_State &aCPU);
void ppc_opc_crnand(PPC_CPU_State &aCPU);
void ppc_opc_crnor(PPC_CPU_State &aCPU);
void ppc_opc_cror(PPC_CPU_State &aCPU);
void ppc_opc_crorc(PPC_CPU_State &aCPU);
void ppc_opc_crxor(PPC_CPU_State &aCPU);

void ppc_opc_divwx(PPC_CPU_State &aCPU);
void ppc_opc_divwux(PPC_CPU_State &aCPU);

void ppc_opc_eqvx(PPC_CPU_State &aCPU);

void ppc_opc_extsbx(PPC_CPU_State &aCPU);
void ppc_opc_extshx(PPC_CPU_State &aCPU);

void ppc_opc_mulhwx(PPC_CPU_State &aCPU);
void ppc_opc_mulhwux(PPC_CPU_State &aCPU);
void ppc_opc_mulli(PPC_CPU_State &aCPU);
void ppc_opc_mullwx(PPC_CPU_State &aCPU);

void ppc_opc_nandx(PPC_CPU_State &aCPU);

void ppc_opc_negx(PPC_CPU_State &aCPU);
void ppc_opc_norx(PPC_CPU_State &aCPU);

void ppc_opc_orx(PPC_CPU_State &aCPU);
void ppc_opc_orcx(PPC_CPU_State &aCPU);
void ppc_opc_ori(PPC_CPU_State &aCPU);
void ppc_opc_oris(PPC_CPU_State &aCPU);

void ppc_opc_rlwimix(PPC_CPU_State &aCPU);
void ppc_opc_rlwinmx(PPC_CPU_State &aCPU);
void ppc_opc_rlwnmx(PPC_CPU_State &aCPU);

void ppc_opc_slwx(PPC_CPU_State &aCPU);
void ppc_opc_srawx(PPC_CPU_State &aCPU);
void ppc_opc_srawix(PPC_CPU_State &aCPU);
void ppc_opc_srwx(PPC_CPU_State &aCPU);

void ppc_opc_subfx(PPC_CPU_State &aCPU);
void ppc_opc_subfcx(PPC_CPU_State &aCPU);
void ppc_opc_subfex(PPC_CPU_State &aCPU);
void ppc_opc_subfic(PPC_CPU_State &aCPU);
void ppc_opc_subfmex(PPC_CPU_State &aCPU);
void ppc_opc_subfzex(PPC_CPU_State &aCPU);

void ppc_opc_xorx(PPC_CPU_State &aCPU);
void ppc_opc_xori(PPC_CPU_State &aCPU);
void ppc_opc_xoris(PPC_CPU_State &aCPU);

JITCFlow ppc_opc_gen_addx(JITC &aJITC);
JITCFlow ppc_opc_gen_addcx(JITC &aJITC);
JITCFlow ppc_opc_gen_addex(JITC &aJITC);
JITCFlow ppc_opc_gen_addi(JITC &aJITC);
JITCFlow ppc_opc_gen_addic(JITC &aJITC);
JITCFlow ppc_opc_gen_addic_(JITC &aJITC);
JITCFlow ppc_opc_gen_addis(JITC &aJITC);
JITCFlow ppc_opc_gen_addmex(JITC &aJITC);
JITCFlow ppc_opc_gen_addzex(JITC &aJITC);

JITCFlow ppc_opc_gen_andx(JITC &aJITC);
JITCFlow ppc_opc_gen_andcx(JITC &aJITC);
JITCFlow ppc_opc_gen_andi_(JITC &aJITC);
JITCFlow ppc_opc_gen_andis_(JITC &aJITC);

JITCFlow ppc_opc_gen_cmp(JITC &aJITC);
JITCFlow ppc_opc_gen_cmpi(JITC &aJITC);
JITCFlow ppc_opc_gen_cmpl(JITC &aJITC);
JITCFlow ppc_opc_gen_cmpli(JITC &aJITC);

JITCFlow ppc_opc_gen_cntlzwx(JITC &aJITC);

JITCFlow ppc_opc_gen_crand(JITC &aJITC);
JITCFlow ppc_opc_gen_crandc(JITC &aJITC);
JITCFlow ppc_opc_gen_creqv(JITC &aJITC);
JITCFlow ppc_opc_gen_crnand(JITC &aJITC);
JITCFlow ppc_opc_gen_crnor(JITC &aJITC);
JITCFlow ppc_opc_gen_cror(JITC &aJITC);
JITCFlow ppc_opc_gen_crorc(JITC &aJITC);
JITCFlow ppc_opc_gen_crxor(JITC &aJITC);

JITCFlow ppc_opc_gen_divwx(JITC &aJITC);
JITCFlow ppc_opc_gen_divwux(JITC &aJITC);

JITCFlow ppc_opc_gen_eqvx(JITC &aJITC);

JITCFlow ppc_opc_gen_extsbx(JITC &aJITC);
JITCFlow ppc_opc_gen_extshx(JITC &aJITC);

JITCFlow ppc_opc_gen_mulhwx(JITC &aJITC);
JITCFlow ppc_opc_gen_mulhwux(JITC &aJITC);
JITCFlow ppc_opc_gen_mulli(JITC &aJITC);
JITCFlow ppc_opc_gen_mullwx(JITC &aJITC);

JITCFlow ppc_opc_gen_nandx(JITC &aJITC);

JITCFlow ppc_opc_gen_negx(JITC &aJITC);
JITCFlow ppc_opc_gen_norx(JITC &aJITC);

JITCFlow ppc_opc_gen_orx(JITC &aJITC);
JITCFlow ppc_opc_gen_orcx(JITC &aJITC);
JITCFlow ppc_opc_gen_ori(JITC &aJITC);
JITCFlow ppc_opc_gen_oris(JITC &aJITC);

JITCFlow ppc_opc_gen_rlwimix(JITC &aJITC);
JITCFlow ppc_opc_gen_rlwinmx(JITC &aJITC);
JITCFlow ppc_opc_gen_rlwnmx(JITC &aJITC);

JITCFlow ppc_opc_gen_slwx(JITC &aJITC);
JITCFlow ppc_opc_gen_srawx(JITC &aJITC);
JITCFlow ppc_opc_gen_srawix(JITC &aJITC);
JITCFlow ppc_opc_gen_srwx(JITC &aJITC);

JITCFlow ppc_opc_gen_subfx(JITC &aJITC);
JITCFlow ppc_opc_gen_subfcx(JITC &aJITC);
JITCFlow ppc_opc_gen_subfex(JITC &aJITC);
JITCFlow ppc_opc_gen_subfic(JITC &aJITC);
JITCFlow ppc_opc_gen_subfmex(JITC &aJITC);
JITCFlow ppc_opc_gen_subfzex(JITC &aJITC);

JITCFlow ppc_opc_gen_xorx(JITC &aJITC);
JITCFlow ppc_opc_gen_xori(JITC &aJITC);
JITCFlow ppc_opc_gen_xoris(JITC &aJITC);

#endif
