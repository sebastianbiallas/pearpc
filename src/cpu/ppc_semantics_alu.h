/*
 *	PearPC
 *	ppc_semantics_alu.h
 *
 *	Copyright (C) 2003-2006 Sebastian Biallas (sb@biallas.net)
 *
 *	Abstract PPC ALU instruction semantics, parameterized
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

#ifndef __PPC_SEMANTICS_ALU_H__
#define __PPC_SEMANTICS_ALU_H__

#include "system/types.h"
#include "cpu/ppc_opc_decode.h"

// ---- Add instructions ----

template <typename S> void ppc_sem_addx(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto val = s.add(s.read_gpr(rA), s.read_gpr(rB));
    s.write_gpr(rD, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_addcx(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto a = s.read_gpr(rA);
    auto b = s.read_gpr(rB);
    auto val = s.add(a, b);
    s.write_gpr(rD, val);
    s.write_xer_ca(S::carry_add(a, b));
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_addex(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto a = s.read_gpr(rA);
    auto b = s.read_gpr(rB);
    auto ca = s.read_xer_ca();
    auto val = s.add(s.add(a, b), s.imm(ca));
    s.write_gpr(rD, val);
    s.write_xer_ca(S::carry_3(a, b, ca));
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_addi(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);
    auto val = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    val = s.add(val, s.imm(imm));
    s.write_gpr(rD, val);
}

template <typename S> void ppc_sem_addic(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);
    auto a = s.read_gpr(rA);
    auto val = s.add(a, s.imm(imm));
    s.write_gpr(rD, val);
    s.write_xer_ca(S::carry_add(a, imm));
}

template <typename S> void ppc_sem_addic_(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);
    auto a = s.read_gpr(rA);
    auto val = s.add(a, s.imm(imm));
    s.write_gpr(rD, val);
    s.write_xer_ca(S::carry_add(a, imm));
    s.write_cr0_from_result(val);
}

template <typename S> void ppc_sem_addis(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_Shift16(opc, rD, rA, imm);
    auto val = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    val = s.add(val, s.imm(imm));
    s.write_gpr(rD, val);
}

template <typename S> void ppc_sem_addmex(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto a = s.read_gpr(rA);
    auto ca = s.read_xer_ca();
    auto val = s.add(s.add(a, s.imm(ca)), s.imm(0xffffffff));
    s.write_gpr(rD, val);
    s.write_xer_ca(a || ca);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_addzex(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto a = s.read_gpr(rA);
    auto ca = s.read_xer_ca();
    auto val = s.add(a, s.imm(ca));
    s.write_gpr(rD, val);
    s.write_xer_ca((a == 0xffffffff) && ca);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

// ---- Logic instructions ----

template <typename S> void ppc_sem_andx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto val = s.and_(s.read_gpr(rS), s.read_gpr(rB));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_andcx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto val = s.andc(s.read_gpr(rS), s.read_gpr(rB));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_andi_(S &s, uint32 opc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(opc, rS, rA, imm);
    auto val = s.and_(s.read_gpr(rS), s.imm(imm));
    s.write_gpr(rA, val);
    s.write_cr0_from_result(val);
}

template <typename S> void ppc_sem_andis_(S &s, uint32 opc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_Shift16(opc, rS, rA, imm);
    auto val = s.and_(s.read_gpr(rS), s.imm(imm));
    s.write_gpr(rA, val);
    s.write_cr0_from_result(val);
}

template <typename S> void ppc_sem_orx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto val = s.or_(s.read_gpr(rS), s.read_gpr(rB));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_orcx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto val = s.orc(s.read_gpr(rS), s.read_gpr(rB));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_ori(S &s, uint32 opc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(opc, rS, rA, imm);
    auto val = s.or_(s.read_gpr(rS), s.imm(imm));
    s.write_gpr(rA, val);
}

template <typename S> void ppc_sem_oris(S &s, uint32 opc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_Shift16(opc, rS, rA, imm);
    auto val = s.or_(s.read_gpr(rS), s.imm(imm));
    s.write_gpr(rA, val);
}

template <typename S> void ppc_sem_xorx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto val = s.xor_(s.read_gpr(rS), s.read_gpr(rB));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_xori(S &s, uint32 opc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(opc, rS, rA, imm);
    auto val = s.xor_(s.read_gpr(rS), s.imm(imm));
    s.write_gpr(rA, val);
}

template <typename S> void ppc_sem_xoris(S &s, uint32 opc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_Shift16(opc, rS, rA, imm);
    auto val = s.xor_(s.read_gpr(rS), s.imm(imm));
    s.write_gpr(rA, val);
}

template <typename S> void ppc_sem_nandx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto val = s.nand_(s.read_gpr(rS), s.read_gpr(rB));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_norx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto val = s.nor_(s.read_gpr(rS), s.read_gpr(rB));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_eqvx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto val = s.eqv(s.read_gpr(rS), s.read_gpr(rB));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

// ---- Compare instructions ----

template <typename S> void ppc_sem_cmp(S &s, uint32 opc)
{
    uint32 cr;
    int rA, rB;
    PPC_OPC_TEMPL_X(opc, cr, rA, rB);
    cr >>= 2;
    s.write_cr_field_signed(cr, s.read_gpr(rA), s.read_gpr(rB));
}

template <typename S> void ppc_sem_cmpi(S &s, uint32 opc)
{
    uint32 cr;
    int rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, cr, rA, imm);
    cr >>= 2;
    s.write_cr_field_signed(cr, s.read_gpr(rA), s.imm(imm));
}

template <typename S> void ppc_sem_cmpl(S &s, uint32 opc)
{
    uint32 cr;
    int rA, rB;
    PPC_OPC_TEMPL_X(opc, cr, rA, rB);
    cr >>= 2;
    s.write_cr_field_unsigned(cr, s.read_gpr(rA), s.read_gpr(rB));
}

template <typename S> void ppc_sem_cmpli(S &s, uint32 opc)
{
    uint32 cr;
    int rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(opc, cr, rA, imm);
    cr >>= 2;
    s.write_cr_field_unsigned(cr, s.read_gpr(rA), s.imm(imm));
}

// ---- Subtract instructions ----

template <typename S> void ppc_sem_subfx(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto val = s.add(s.add(s.not_(s.read_gpr(rA)), s.read_gpr(rB)), s.imm(1));
    s.write_gpr(rD, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_subfcx(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto a = s.read_gpr(rA);
    auto b = s.read_gpr(rB);
    auto na = s.not_(a);
    auto val = s.add(s.add(na, b), s.imm(1));
    s.write_gpr(rD, val);
    s.write_xer_ca(S::carry_3(~a, b, 1));
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_subfex(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto a = s.read_gpr(rA);
    auto b = s.read_gpr(rB);
    auto ca = s.read_xer_ca();
    auto na = s.not_(a);
    auto val = s.add(s.add(na, b), s.imm(ca));
    s.write_gpr(rD, val);
    s.write_xer_ca(S::carry_3(~a, b, ca));
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_subfic(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);
    auto a = s.read_gpr(rA);
    auto val = s.add(s.add(s.not_(a), s.imm(imm)), s.imm(1));
    s.write_gpr(rD, val);
    s.write_xer_ca(S::carry_3(~a, imm, 1));
}

template <typename S> void ppc_sem_subfmex(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto a = s.read_gpr(rA);
    auto ca = s.read_xer_ca();
    auto na = s.not_(a);
    auto val = s.add(s.add(na, s.imm(ca)), s.imm(0xffffffff));
    s.write_gpr(rD, val);
    s.write_xer_ca((a != 0xffffffff) || ca);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_subfzex(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto a = s.read_gpr(rA);
    auto ca = s.read_xer_ca();
    auto na = s.not_(a);
    auto val = s.add(na, s.imm(ca));
    s.write_gpr(rD, val);
    s.write_xer_ca(!a && ca);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

// ---- Negate ----

template <typename S> void ppc_sem_negx(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto val = s.neg(s.read_gpr(rA));
    s.write_gpr(rD, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

// ---- Multiply instructions ----

template <typename S> void ppc_sem_mulli(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);
    auto val = s.mul(s.read_gpr(rA), s.imm(imm));
    s.write_gpr(rD, val);
}

template <typename S> void ppc_sem_mullwx(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto val = s.mul(s.read_gpr(rA), s.read_gpr(rB));
    s.write_gpr(rD, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_mulhwx(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto val = s.mulhs(s.read_gpr(rA), s.read_gpr(rB));
    s.write_gpr(rD, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_mulhwux(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto val = s.mulhu(s.read_gpr(rA), s.read_gpr(rB));
    s.write_gpr(rD, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

// ---- Divide instructions ----

template <typename S> void ppc_sem_divwx(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto val = s.div_s(s.read_gpr(rA), s.read_gpr(rB));
    s.write_gpr(rD, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_divwux(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(opc, rD, rA, rB);
    auto val = s.div_u(s.read_gpr(rA), s.read_gpr(rB));
    s.write_gpr(rD, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

// ---- Shift instructions ----

template <typename S> void ppc_sem_slwx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto val = s.shl(s.read_gpr(rS), s.read_gpr(rB));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_srwx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto val = s.shr(s.read_gpr(rS), s.read_gpr(rB));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_srawx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    bool carry;
    auto val = s.sraw(s.read_gpr(rS), s.read_gpr(rB), carry);
    s.write_gpr(rA, val);
    s.write_xer_ca(carry);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_srawix(S &s, uint32 opc)
{
    int rS, rA;
    uint32 SH;
    PPC_OPC_TEMPL_X(opc, rS, rA, SH);
    bool carry;
    auto val = s.sraw(s.read_gpr(rS), s.imm(SH), carry);
    s.write_gpr(rA, val);
    s.write_xer_ca(carry);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

// ---- Rotate and mask instructions ----

template <typename S> void ppc_sem_rlwinmx(S &s, uint32 opc)
{
    int rS, rA, SH, MB, ME;
    PPC_OPC_TEMPL_M(opc, rS, rA, SH, MB, ME);
    auto v = s.rotl(s.read_gpr(rS), SH);
    auto m = S::mask(MB, ME);
    auto val = s.and_(v, s.imm(m));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_rlwnmx(S &s, uint32 opc)
{
    int rS, rA, rB, MB, ME;
    PPC_OPC_TEMPL_M(opc, rS, rA, rB, MB, ME);
    auto v = s.rotl(s.read_gpr(rS), s.read_gpr(rB));
    auto m = S::mask(MB, ME);
    auto val = s.and_(v, s.imm(m));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_rlwimix(S &s, uint32 opc)
{
    int rS, rA, SH, MB, ME;
    PPC_OPC_TEMPL_M(opc, rS, rA, SH, MB, ME);
    auto v = s.rotl(s.read_gpr(rS), SH);
    auto m = S::mask(MB, ME);
    auto val = s.or_(s.and_(v, s.imm(m)), s.and_(s.read_gpr(rA), s.imm(~m)));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

// ---- Sign extension ----

template <typename S> void ppc_sem_extsbx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto val = s.extend_sign_byte(s.read_gpr(rS));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

template <typename S> void ppc_sem_extshx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto val = s.extend_sign_half(s.read_gpr(rS));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

// ---- Count leading zeros ----

template <typename S> void ppc_sem_cntlzwx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto val = s.cntlzw(s.read_gpr(rS));
    s.write_gpr(rA, val);
    if (opc & PPC_OPC_Rc) {
        s.write_cr0_from_result(val);
    }
}

// ---- CR logical instructions ----

template <typename S> void ppc_sem_crand(S &s, uint32 opc)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(opc, crD, crA, crB);
    bool a = s.get_cr_bit(crA);
    bool b = s.get_cr_bit(crB);
    s.write_cr_bit(crD, a && b);
}

template <typename S> void ppc_sem_crandc(S &s, uint32 opc)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(opc, crD, crA, crB);
    bool a = s.get_cr_bit(crA);
    bool b = s.get_cr_bit(crB);
    s.write_cr_bit(crD, a && !b);
}

template <typename S> void ppc_sem_creqv(S &s, uint32 opc)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(opc, crD, crA, crB);
    bool a = s.get_cr_bit(crA);
    bool b = s.get_cr_bit(crB);
    s.write_cr_bit(crD, a == b);
}

template <typename S> void ppc_sem_crnand(S &s, uint32 opc)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(opc, crD, crA, crB);
    bool a = s.get_cr_bit(crA);
    bool b = s.get_cr_bit(crB);
    s.write_cr_bit(crD, !(a && b));
}

template <typename S> void ppc_sem_crnor(S &s, uint32 opc)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(opc, crD, crA, crB);
    bool a = s.get_cr_bit(crA);
    bool b = s.get_cr_bit(crB);
    s.write_cr_bit(crD, !(a || b));
}

template <typename S> void ppc_sem_cror(S &s, uint32 opc)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(opc, crD, crA, crB);
    bool a = s.get_cr_bit(crA);
    bool b = s.get_cr_bit(crB);
    s.write_cr_bit(crD, a || b);
}

template <typename S> void ppc_sem_crorc(S &s, uint32 opc)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(opc, crD, crA, crB);
    bool a = s.get_cr_bit(crA);
    bool b = s.get_cr_bit(crB);
    s.write_cr_bit(crD, a || !b);
}

template <typename S> void ppc_sem_crxor(S &s, uint32 opc)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(opc, crD, crA, crB);
    bool a = s.get_cr_bit(crA);
    bool b = s.get_cr_bit(crB);
    s.write_cr_bit(crD, a != b);
}

#endif
