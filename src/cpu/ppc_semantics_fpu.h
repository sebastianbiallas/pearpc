/*
 *	PearPC
 *	ppc_semantics_fpu.h
 *
 *	Copyright (C) 2003-2006 Sebastian Biallas (sb@biallas.net)
 *
 *	Abstract PPC FPU instruction semantics, parameterized
 *	on a semantics backend (ConcreteSemantics, LivenessSemantics, etc.)
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

#ifndef __PPC_SEMANTICS_FPU_H__
#define __PPC_SEMANTICS_FPU_H__

#include "system/types.h"
#include "cpu/ppc_opc_decode.h"

/*
 *	fcmpu / fcmpo		Floating Compare (Unordered / Ordered)
 *	Reads two FPRs, writes a CR field. Writes FPSCR (not tracked).
 *	The CR field write is the liveness-relevant effect.
 */
template <typename S> void ppc_sem_fcmpx(S &s, uint32 opc)
{
    int crfD, frA, frB;
    PPC_OPC_TEMPL_X(opc, crfD, frA, frB);
    crfD >>= 2;
    s.read_fpr(frA);
    s.read_fpr(frB);
    // Writes 4 CR bits in the target field + reads XER.SO for the SO bit
    s.write_cr_field_signed(crfD, s.imm(0), s.imm(0));
}

/*
 *	Generic FPU arithmetic — covers all FPU ops that only touch FPRs
 *	and FPSCR. For liveness, the only relevant effect is:
 *	  - Rc=1 writes CR1 (from FPSCR exception bits)
 *
 *	This covers: fadd, fsub, fmul, fdiv, fsqrt, fmadd, fmsub,
 *	fnmadd, fnmsub, frsp, fctiwx, fctiwzx, fsel, fres, frsqrte,
 *	fmr, fabs, fneg, fnabs, and their single-precision variants.
 */
template <typename S> void ppc_sem_fpu_arith(S &s, uint32 opc)
{
    if (opc & PPC_OPC_Rc) {
        // Rc=1: CR1 ← FPSCR exception summary (FX, FEX, VX, OX)
        uint32 cr1_mask = 0xfu << ((7 - 1) * 4);
        s.write_cr_masked(s.imm(0), cr1_mask);
    }
}

// ---- FPU Load/Store (D-form) ----

template <typename S> void ppc_sem_lfs(S &s, uint32 opc)
{
    int frD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, frD, rA, imm);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.imm(imm));
    s.write_fpr(frD, s.read_mem(addr, 4));
}

template <typename S> void ppc_sem_lfsu(S &s, uint32 opc)
{
    int frD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, frD, rA, imm);
    auto addr = s.add(s.read_gpr(rA), s.imm(imm));
    s.write_fpr(frD, s.read_mem(addr, 4));
    s.write_gpr(rA, addr);
}

template <typename S> void ppc_sem_lfd(S &s, uint32 opc)
{
    int frD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, frD, rA, imm);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.imm(imm));
    s.write_fpr(frD, s.read_mem(addr, 8));
}

template <typename S> void ppc_sem_lfdu(S &s, uint32 opc)
{
    int frD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, frD, rA, imm);
    auto addr = s.add(s.read_gpr(rA), s.imm(imm));
    s.write_fpr(frD, s.read_mem(addr, 8));
    s.write_gpr(rA, addr);
}

template <typename S> void ppc_sem_stfs(S &s, uint32 opc)
{
    int frS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, frS, rA, imm);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.imm(imm));
    s.read_fpr(frS);
    s.write_mem(addr, s.imm(0), 4);
}

template <typename S> void ppc_sem_stfsu(S &s, uint32 opc)
{
    int frS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, frS, rA, imm);
    auto addr = s.add(s.read_gpr(rA), s.imm(imm));
    s.read_fpr(frS);
    s.write_mem(addr, s.imm(0), 4);
    s.write_gpr(rA, addr);
}

template <typename S> void ppc_sem_stfd(S &s, uint32 opc)
{
    int frS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, frS, rA, imm);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.imm(imm));
    s.read_fpr(frS);
    s.write_mem(addr, s.imm(0), 8);
}

template <typename S> void ppc_sem_stfdu(S &s, uint32 opc)
{
    int frS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, frS, rA, imm);
    auto addr = s.add(s.read_gpr(rA), s.imm(imm));
    s.read_fpr(frS);
    s.write_mem(addr, s.imm(0), 8);
    s.write_gpr(rA, addr);
}

// ---- FPU Load/Store (X-form indexed) ----

template <typename S> void ppc_sem_lfsx(S &s, uint32 opc)
{
    int frD, rA, rB;
    PPC_OPC_TEMPL_X(opc, frD, rA, rB);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.read_gpr(rB));
    s.write_fpr(frD, s.read_mem(addr, 4));
}

template <typename S> void ppc_sem_lfsux(S &s, uint32 opc)
{
    int frD, rA, rB;
    PPC_OPC_TEMPL_X(opc, frD, rA, rB);
    auto addr = s.add(s.read_gpr(rA), s.read_gpr(rB));
    s.write_fpr(frD, s.read_mem(addr, 4));
    s.write_gpr(rA, addr);
}

template <typename S> void ppc_sem_lfdx(S &s, uint32 opc)
{
    int frD, rA, rB;
    PPC_OPC_TEMPL_X(opc, frD, rA, rB);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.read_gpr(rB));
    s.write_fpr(frD, s.read_mem(addr, 8));
}

template <typename S> void ppc_sem_lfdux(S &s, uint32 opc)
{
    int frD, rA, rB;
    PPC_OPC_TEMPL_X(opc, frD, rA, rB);
    auto addr = s.add(s.read_gpr(rA), s.read_gpr(rB));
    s.write_fpr(frD, s.read_mem(addr, 8));
    s.write_gpr(rA, addr);
}

template <typename S> void ppc_sem_stfsx(S &s, uint32 opc)
{
    int frS, rA, rB;
    PPC_OPC_TEMPL_X(opc, frS, rA, rB);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.read_gpr(rB));
    s.read_fpr(frS);
    s.write_mem(addr, s.imm(0), 4);
}

template <typename S> void ppc_sem_stfsux(S &s, uint32 opc)
{
    int frS, rA, rB;
    PPC_OPC_TEMPL_X(opc, frS, rA, rB);
    auto addr = s.add(s.read_gpr(rA), s.read_gpr(rB));
    s.read_fpr(frS);
    s.write_mem(addr, s.imm(0), 4);
    s.write_gpr(rA, addr);
}

template <typename S> void ppc_sem_stfdx(S &s, uint32 opc)
{
    int frS, rA, rB;
    PPC_OPC_TEMPL_X(opc, frS, rA, rB);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.read_gpr(rB));
    s.read_fpr(frS);
    s.write_mem(addr, s.imm(0), 8);
}

template <typename S> void ppc_sem_stfdux(S &s, uint32 opc)
{
    int frS, rA, rB;
    PPC_OPC_TEMPL_X(opc, frS, rA, rB);
    auto addr = s.add(s.read_gpr(rA), s.read_gpr(rB));
    s.read_fpr(frS);
    s.write_mem(addr, s.imm(0), 8);
    s.write_gpr(rA, addr);
}

#endif
