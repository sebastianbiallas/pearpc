/*
 *	PearPC
 *	ppc_semantics_mem.h
 *
 *	Copyright (C) 2003-2006 Sebastian Biallas (sb@biallas.net)
 *
 *	Abstract PPC load/store instruction semantics, parameterized
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

#ifndef __PPC_SEMANTICS_MEM_H__
#define __PPC_SEMANTICS_MEM_H__

#include "system/types.h"
#include "cpu/ppc_opc_decode.h"

// ---- Load Word ----

template <typename S> void ppc_sem_lwz(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.imm(imm));
    auto val = s.read_mem(addr, 4);
    s.write_gpr(rD, val);
}

template <typename S> void ppc_sem_lwzu(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);
    auto addr = s.add(s.read_gpr(rA), s.imm(imm));
    auto val = s.read_mem(addr, 4);
    s.write_gpr(rD, val);
    s.write_gpr(rA, addr);
}

template <typename S> void ppc_sem_lwzx(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(opc, rD, rA, rB);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.read_gpr(rB));
    auto val = s.read_mem(addr, 4);
    s.write_gpr(rD, val);
}

template <typename S> void ppc_sem_lwzux(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(opc, rD, rA, rB);
    auto addr = s.add(s.read_gpr(rA), s.read_gpr(rB));
    auto val = s.read_mem(addr, 4);
    s.write_gpr(rD, val);
    s.write_gpr(rA, addr);
}

// ---- Load Word Indexed (already above: lwzx, lwzux) ----

// ---- Load Half Word ----

template <typename S> void ppc_sem_lhz(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.imm(imm));
    auto val = s.read_mem(addr, 2);
    s.write_gpr(rD, val);
}

template <typename S> void ppc_sem_lhzu(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);
    auto addr = s.add(s.read_gpr(rA), s.imm(imm));
    auto val = s.read_mem(addr, 2);
    s.write_gpr(rD, val);
    s.write_gpr(rA, addr);
}

template <typename S> void ppc_sem_lha(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.imm(imm));
    auto val = s.read_mem_sign_extend(addr, 2);
    s.write_gpr(rD, val);
}

template <typename S> void ppc_sem_lhau(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);
    auto addr = s.add(s.read_gpr(rA), s.imm(imm));
    auto val = s.read_mem_sign_extend(addr, 2);
    s.write_gpr(rD, val);
    s.write_gpr(rA, addr);
}

// ---- Load Byte ----

template <typename S> void ppc_sem_lbz(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.imm(imm));
    auto val = s.read_mem(addr, 1);
    s.write_gpr(rD, val);
}

template <typename S> void ppc_sem_lbzu(S &s, uint32 opc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rD, rA, imm);
    auto addr = s.add(s.read_gpr(rA), s.imm(imm));
    auto val = s.read_mem(addr, 1);
    s.write_gpr(rD, val);
    s.write_gpr(rA, addr);
}

// ---- Store Word ----

template <typename S> void ppc_sem_stw(S &s, uint32 opc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rS, rA, imm);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.imm(imm));
    s.write_mem(addr, s.read_gpr(rS), 4);
}

template <typename S> void ppc_sem_stwu(S &s, uint32 opc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rS, rA, imm);
    auto addr = s.add(s.read_gpr(rA), s.imm(imm));
    s.write_mem(addr, s.read_gpr(rS), 4);
    s.write_gpr(rA, addr);
}

template <typename S> void ppc_sem_stwx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.read_gpr(rB));
    s.write_mem(addr, s.read_gpr(rS), 4);
}

template <typename S> void ppc_sem_stwux(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto addr = s.add(s.read_gpr(rA), s.read_gpr(rB));
    s.write_mem(addr, s.read_gpr(rS), 4);
    s.write_gpr(rA, addr);
}

// ---- Store Half Word ----

template <typename S> void ppc_sem_sth(S &s, uint32 opc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rS, rA, imm);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.imm(imm));
    s.write_mem(addr, s.read_gpr(rS), 2);
}

template <typename S> void ppc_sem_sthu(S &s, uint32 opc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rS, rA, imm);
    auto addr = s.add(s.read_gpr(rA), s.imm(imm));
    s.write_mem(addr, s.read_gpr(rS), 2);
    s.write_gpr(rA, addr);
}

// ---- Store Byte ----

template <typename S> void ppc_sem_stb(S &s, uint32 opc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rS, rA, imm);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.imm(imm));
    s.write_mem(addr, s.read_gpr(rS), 1);
}

template <typename S> void ppc_sem_stbu(S &s, uint32 opc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(opc, rS, rA, imm);
    auto addr = s.add(s.read_gpr(rA), s.imm(imm));
    s.write_mem(addr, s.read_gpr(rS), 1);
    s.write_gpr(rA, addr);
}

// ---- Load Byte Indexed ----

template <typename S> void ppc_sem_lbzx(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(opc, rD, rA, rB);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.read_gpr(rB));
    auto val = s.read_mem(addr, 1);
    s.write_gpr(rD, val);
}

template <typename S> void ppc_sem_lbzux(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(opc, rD, rA, rB);
    auto addr = s.add(s.read_gpr(rA), s.read_gpr(rB));
    auto val = s.read_mem(addr, 1);
    s.write_gpr(rD, val);
    s.write_gpr(rA, addr);
}

// ---- Load Half Word Indexed ----

template <typename S> void ppc_sem_lhzx(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(opc, rD, rA, rB);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.read_gpr(rB));
    auto val = s.read_mem(addr, 2);
    s.write_gpr(rD, val);
}

template <typename S> void ppc_sem_lhzux(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(opc, rD, rA, rB);
    auto addr = s.add(s.read_gpr(rA), s.read_gpr(rB));
    auto val = s.read_mem(addr, 2);
    s.write_gpr(rD, val);
    s.write_gpr(rA, addr);
}

template <typename S> void ppc_sem_lhax(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(opc, rD, rA, rB);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.read_gpr(rB));
    auto val = s.read_mem_sign_extend(addr, 2);
    s.write_gpr(rD, val);
}

template <typename S> void ppc_sem_lhaux(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(opc, rD, rA, rB);
    auto addr = s.add(s.read_gpr(rA), s.read_gpr(rB));
    auto val = s.read_mem_sign_extend(addr, 2);
    s.write_gpr(rD, val);
    s.write_gpr(rA, addr);
}

// ---- Store Word Indexed (already above: stwx, stwux) ----

// ---- Store Byte Indexed ----

template <typename S> void ppc_sem_stbx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.read_gpr(rB));
    s.write_mem(addr, s.read_gpr(rS), 1);
}

template <typename S> void ppc_sem_stbux(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto addr = s.add(s.read_gpr(rA), s.read_gpr(rB));
    s.write_mem(addr, s.read_gpr(rS), 1);
    s.write_gpr(rA, addr);
}

// ---- Store Half Word Indexed ----

template <typename S> void ppc_sem_sthx(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto addr = (rA == 0) ? s.imm(0) : s.read_gpr(rA);
    addr = s.add(addr, s.read_gpr(rB));
    s.write_mem(addr, s.read_gpr(rS), 2);
}

template <typename S> void ppc_sem_sthux(S &s, uint32 opc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(opc, rS, rA, rB);
    auto addr = s.add(s.read_gpr(rA), s.read_gpr(rB));
    s.write_mem(addr, s.read_gpr(rS), 2);
    s.write_gpr(rA, addr);
}

#endif
