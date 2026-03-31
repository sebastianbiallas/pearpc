/*
 *	PearPC
 *	ppc_semantics_branch.h
 *
 *	Copyright (C) 2003-2006 Sebastian Biallas (sb@biallas.net)
 *
 *	Abstract PPC branch instruction semantics, parameterized
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

#ifndef __PPC_SEMANTICS_BRANCH_H__
#define __PPC_SEMANTICS_BRANCH_H__

#include "system/types.h"
#include "cpu/ppc_opc_decode.h"

/*
 *	bx		Branch
 *	Reads: (nothing from GPR/CR)
 *	Writes: LR (if LK=1)
 *	Always a branch (unconditional)
 */
template <typename S> void ppc_sem_bx(S &s, uint32 opc)
{
    if (opc & PPC_OPC_LK) {
        s.write_lr();
    }
    s.branch();
}

/*
 *	bcx		Branch Conditional
 *	Reads: CTR (if BO[2]=0), CR bit BI (if BO[4]=0)
 *	Writes: CTR (if BO[2]=0, decremented), LR (if LK=1)
 */
template <typename S> void ppc_sem_bcx(S &s, uint32 opc)
{
    uint32 BO, BI, BD;
    PPC_OPC_TEMPL_B(opc, BO, BI, BD);

    if (!(BO & 4)) {
        s.read_ctr();
        s.write_ctr();
    }
    if (!(BO & 16)) {
        s.read_cr_bit(BI);
    }
    if (opc & PPC_OPC_LK) {
        s.write_lr();
    }
    s.branch_cond();
}

/*
 *	bcctrx		Branch Conditional to Count Register
 *	Reads: CTR (target), CR bit BI (if BO[4]=0)
 *	Writes: LR (if LK=1)
 */
template <typename S> void ppc_sem_bcctrx(S &s, uint32 opc)
{
    uint32 BO, BI, BD;
    PPC_OPC_TEMPL_XL(opc, BO, BI, BD);

    s.read_ctr();
    if (!(BO & 16)) {
        s.read_cr_bit(BI);
    }
    if (opc & PPC_OPC_LK) {
        s.write_lr();
    }
    s.branch_cond();
}

/*
 *	bclrx		Branch Conditional to Link Register
 *	Reads: LR (target), CTR (if BO[2]=0), CR bit BI (if BO[4]=0)
 *	Writes: CTR (if BO[2]=0), LR (if LK=1)
 */
template <typename S> void ppc_sem_bclrx(S &s, uint32 opc)
{
    uint32 BO, BI, BD;
    PPC_OPC_TEMPL_XL(opc, BO, BI, BD);

    s.read_lr();
    if (!(BO & 4)) {
        s.read_ctr();
        s.write_ctr();
    }
    if (!(BO & 16)) {
        s.read_cr_bit(BI);
    }
    if (opc & PPC_OPC_LK) {
        s.write_lr();
    }
    s.branch_cond();
}

#endif
