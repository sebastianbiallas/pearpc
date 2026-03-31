/*
 *	PearPC
 *	ppc_alu.cc
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

#include "debug/tracers.h"
#include "cpu/debug.h"
#include "ppc_alu.h"
#include "ppc_dec.h"
#include "ppc_exc.h"
#include "ppc_cpu.h"
#include "ppc_opc.h"
#include "ppc_tools.h"
#include "cpu/ppc_concrete_sem.h"
#include "cpu/ppc_semantics_alu.h"

#define SEM                                                                                                            \
    ConcreteSemantics<PPC_CPU_State> s                                                                                 \
    {                                                                                                                  \
        gCPU                                                                                                           \
    }

// ---- Add instructions ----

void ppc_opc_addx()
{
    SEM;
    ppc_sem_addx(s, gCPU.current_opc);
}

void ppc_opc_addox()
{
    SEM;
    ppc_sem_addx(s, gCPU.current_opc);
    // update XER flags
    PPC_ALU_ERR("addox unimplemented\n");
}

void ppc_opc_addcx()
{
    SEM;
    ppc_sem_addcx(s, gCPU.current_opc);
}

void ppc_opc_addcox()
{
    SEM;
    ppc_sem_addcx(s, gCPU.current_opc);
    // update XER flags
    PPC_ALU_ERR("addcox unimplemented\n");
}

void ppc_opc_addex()
{
    SEM;
    ppc_sem_addex(s, gCPU.current_opc);
}

void ppc_opc_addeox()
{
    SEM;
    ppc_sem_addex(s, gCPU.current_opc);
    // update XER flags
    PPC_ALU_ERR("addeox unimplemented\n");
}

void ppc_opc_addi()
{
    SEM;
    ppc_sem_addi(s, gCPU.current_opc);
}

void ppc_opc_addic()
{
    SEM;
    ppc_sem_addic(s, gCPU.current_opc);
}

void ppc_opc_addic_()
{
    SEM;
    ppc_sem_addic_(s, gCPU.current_opc);
}

void ppc_opc_addis()
{
    SEM;
    ppc_sem_addis(s, gCPU.current_opc);
}

void ppc_opc_addmex()
{
    SEM;
    ppc_sem_addmex(s, gCPU.current_opc);
}

void ppc_opc_addmeox()
{
    SEM;
    ppc_sem_addmex(s, gCPU.current_opc);
    // update XER flags
    PPC_ALU_ERR("addmeox unimplemented\n");
}

void ppc_opc_addzex()
{
    SEM;
    ppc_sem_addzex(s, gCPU.current_opc);
}

void ppc_opc_addzeox()
{
    SEM;
    ppc_sem_addzex(s, gCPU.current_opc);
    // update XER flags
    PPC_ALU_ERR("addzeox unimplemented\n");
}

// ---- Logic instructions ----

void ppc_opc_andx()
{
    SEM;
    ppc_sem_andx(s, gCPU.current_opc);
}

void ppc_opc_andcx()
{
    SEM;
    ppc_sem_andcx(s, gCPU.current_opc);
}

void ppc_opc_andi_()
{
    SEM;
    ppc_sem_andi_(s, gCPU.current_opc);
}

void ppc_opc_andis_()
{
    SEM;
    ppc_sem_andis_(s, gCPU.current_opc);
}

// ---- Compare instructions ----

void ppc_opc_cmp()
{
    SEM;
    ppc_sem_cmp(s, gCPU.current_opc);
}

void ppc_opc_cmpi()
{
    SEM;
    ppc_sem_cmpi(s, gCPU.current_opc);
}

void ppc_opc_cmpl()
{
    SEM;
    ppc_sem_cmpl(s, gCPU.current_opc);
}

void ppc_opc_cmpli()
{
    SEM;
    ppc_sem_cmpli(s, gCPU.current_opc);
}

// ---- Count leading zeros ----

void ppc_opc_cntlzwx()
{
    SEM;
    ppc_sem_cntlzwx(s, gCPU.current_opc);
}

// ---- CR logical instructions ----

void ppc_opc_crand()
{
    SEM;
    ppc_sem_crand(s, gCPU.current_opc);
}

void ppc_opc_crandc()
{
    SEM;
    ppc_sem_crandc(s, gCPU.current_opc);
}

void ppc_opc_creqv()
{
    SEM;
    ppc_sem_creqv(s, gCPU.current_opc);
}

void ppc_opc_crnand()
{
    SEM;
    ppc_sem_crnand(s, gCPU.current_opc);
}

void ppc_opc_crnor()
{
    SEM;
    ppc_sem_crnor(s, gCPU.current_opc);
}

void ppc_opc_cror()
{
    SEM;
    ppc_sem_cror(s, gCPU.current_opc);
}

void ppc_opc_crorc()
{
    SEM;
    ppc_sem_crorc(s, gCPU.current_opc);
}

void ppc_opc_crxor()
{
    SEM;
    ppc_sem_crxor(s, gCPU.current_opc);
}

// ---- Divide instructions ----

void ppc_opc_divwx()
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
    if (!gCPU.gpr[rB]) {
        PPC_ALU_WARN("division by zero @%08x\n", gCPU.pc);
        SINGLESTEP("");
    }
    SEM;
    ppc_sem_divwx(s, gCPU.current_opc);
}

void ppc_opc_divwox()
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
    if (!gCPU.gpr[rB]) {
        PPC_ALU_ERR("division by zero\n");
    }
    SEM;
    ppc_sem_divwx(s, gCPU.current_opc);
    // update XER flags
    PPC_ALU_ERR("divwox unimplemented\n");
}

void ppc_opc_divwux()
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
    if (!gCPU.gpr[rB]) {
        PPC_ALU_WARN("division by zero @%08x\n", gCPU.pc);
        SINGLESTEP("");
    }
    SEM;
    ppc_sem_divwux(s, gCPU.current_opc);
}

void ppc_opc_divwuox()
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
    if (!gCPU.gpr[rB]) {
        //		PPC_ALU_ERR("division by zero\n");
    }
    SEM;
    ppc_sem_divwux(s, gCPU.current_opc);
    // update XER flags
    PPC_ALU_ERR("divwuox unimplemented\n");
}

// ---- Equivalent ----

void ppc_opc_eqvx()
{
    SEM;
    ppc_sem_eqvx(s, gCPU.current_opc);
}

// ---- Sign extension ----

void ppc_opc_extsbx()
{
    SEM;
    ppc_sem_extsbx(s, gCPU.current_opc);
}

void ppc_opc_extshx()
{
    SEM;
    ppc_sem_extshx(s, gCPU.current_opc);
}

// ---- Multiply instructions ----

void ppc_opc_mulhwx()
{
    SEM;
    ppc_sem_mulhwx(s, gCPU.current_opc);
}

void ppc_opc_mulhwux()
{
    SEM;
    ppc_sem_mulhwux(s, gCPU.current_opc);
}

void ppc_opc_mulli()
{
    SEM;
    ppc_sem_mulli(s, gCPU.current_opc);
}

void ppc_opc_mullwx()
{
    SEM;
    ppc_sem_mullwx(s, gCPU.current_opc);
}

// ---- NAND ----

void ppc_opc_nandx()
{
    SEM;
    ppc_sem_nandx(s, gCPU.current_opc);
}

// ---- Negate ----

void ppc_opc_negx()
{
    SEM;
    ppc_sem_negx(s, gCPU.current_opc);
}

void ppc_opc_negox()
{
    SEM;
    ppc_sem_negx(s, gCPU.current_opc);
    // update XER flags
    PPC_ALU_ERR("negox unimplemented\n");
}

// ---- NOR ----

void ppc_opc_norx()
{
    SEM;
    ppc_sem_norx(s, gCPU.current_opc);
}

// ---- OR instructions ----

void ppc_opc_orx()
{
    SEM;
    ppc_sem_orx(s, gCPU.current_opc);
}

void ppc_opc_orcx()
{
    SEM;
    ppc_sem_orcx(s, gCPU.current_opc);
}

void ppc_opc_ori()
{
    SEM;
    ppc_sem_ori(s, gCPU.current_opc);
}

void ppc_opc_oris()
{
    SEM;
    ppc_sem_oris(s, gCPU.current_opc);
}

// ---- Rotate and mask ----

void ppc_opc_rlwimix()
{
    SEM;
    ppc_sem_rlwimix(s, gCPU.current_opc);
}

void ppc_opc_rlwinmx()
{
    SEM;
    ppc_sem_rlwinmx(s, gCPU.current_opc);
}

void ppc_opc_rlwnmx()
{
    SEM;
    ppc_sem_rlwnmx(s, gCPU.current_opc);
}

// ---- Shift instructions ----

void ppc_opc_slwx()
{
    SEM;
    ppc_sem_slwx(s, gCPU.current_opc);
}

void ppc_opc_srawx()
{
    SEM;
    ppc_sem_srawx(s, gCPU.current_opc);
}

void ppc_opc_srawix()
{
    SEM;
    ppc_sem_srawix(s, gCPU.current_opc);
}

void ppc_opc_srwx()
{
    SEM;
    ppc_sem_srwx(s, gCPU.current_opc);
}

// ---- Subtract instructions ----

void ppc_opc_subfx()
{
    SEM;
    ppc_sem_subfx(s, gCPU.current_opc);
}

void ppc_opc_subfox()
{
    SEM;
    ppc_sem_subfx(s, gCPU.current_opc);
    // update XER flags
    PPC_ALU_ERR("subfox unimplemented\n");
}

void ppc_opc_subfcx()
{
    SEM;
    ppc_sem_subfcx(s, gCPU.current_opc);
}

void ppc_opc_subfcox()
{
    SEM;
    ppc_sem_subfcx(s, gCPU.current_opc);
    // update XER flags
    PPC_ALU_ERR("subfcox unimplemented\n");
}

void ppc_opc_subfex()
{
    SEM;
    ppc_sem_subfex(s, gCPU.current_opc);
}

void ppc_opc_subfeox()
{
    SEM;
    ppc_sem_subfex(s, gCPU.current_opc);
    // update XER flags
    PPC_ALU_ERR("subfeox unimplemented\n");
}

void ppc_opc_subfic()
{
    SEM;
    ppc_sem_subfic(s, gCPU.current_opc);
}

void ppc_opc_subfmex()
{
    SEM;
    ppc_sem_subfmex(s, gCPU.current_opc);
}

void ppc_opc_subfmeox()
{
    SEM;
    ppc_sem_subfmex(s, gCPU.current_opc);
    // update XER flags
    PPC_ALU_ERR("subfmeox unimplemented\n");
}

void ppc_opc_subfzex()
{
    SEM;
    ppc_sem_subfzex(s, gCPU.current_opc);
}

void ppc_opc_subfzeox()
{
    SEM;
    ppc_sem_subfzex(s, gCPU.current_opc);
    // update XER flags
    PPC_ALU_ERR("subfzeox unimplemented\n");
}

// ---- XOR instructions ----

void ppc_opc_xorx()
{
    SEM;
    ppc_sem_xorx(s, gCPU.current_opc);
}

void ppc_opc_xori()
{
    SEM;
    ppc_sem_xori(s, gCPU.current_opc);
}

void ppc_opc_xoris()
{
    SEM;
    ppc_sem_xoris(s, gCPU.current_opc);
}
