/*
 *	PearPC
 *	ppc_semantics_spr.h
 *
 *	Copyright (C) 2003-2006 Sebastian Biallas (sb@biallas.net)
 *
 *	Abstract PPC SPR/CR move instruction semantics, parameterized
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

#ifndef __PPC_SEMANTICS_SPR_H__
#define __PPC_SEMANTICS_SPR_H__

#include "system/types.h"
#include "cpu/ppc_opc_decode.h"

/*
 *	mfspr		Move from Special-Purpose Register
 *
 *	SPR number is encoded in the opcode (spr1:spr2 fields).
 *	Unprivileged SPRs (XER, LR, CTR) are modeled precisely.
 *	Privileged SPRs fall back to everything().
 */
template <typename S> void ppc_sem_mfspr(S &s, uint32 opc)
{
    int rD, spr1, spr2;
    PPC_OPC_TEMPL_XO(opc, rD, spr1, spr2);

    if (spr2 == 0) {
        switch (spr1) {
        case 1: s.write_gpr(rD, s.read_xer()); return;
        case 8: s.write_gpr(rD, s.read_lr()); return;
        case 9: s.write_gpr(rD, s.read_ctr()); return;
        }
    }
    // Privileged or uncommon SPR — conservative
    s.everything();
}

/*
 *	mtspr		Move to Special-Purpose Register
 *
 *	Unprivileged SPRs (XER, LR, CTR) are modeled precisely.
 *	Privileged SPRs fall back to everything().
 */
template <typename S> void ppc_sem_mtspr(S &s, uint32 opc)
{
    int rS, spr1, spr2;
    PPC_OPC_TEMPL_XO(opc, rS, spr1, spr2);

    if (spr2 == 0) {
        switch (spr1) {
        case 1: s.write_xer(s.read_gpr(rS)); return;
        case 8: s.write_lr(s.read_gpr(rS)); return;
        case 9: s.write_ctr(s.read_gpr(rS)); return;
        }
    }
    // Privileged or uncommon SPR — conservative
    s.everything();
}

/*
 *	mfcr		Move from Condition Register
 *	gpr[rD] = CR
 */
template <typename S> void ppc_sem_mfcr(S &s, uint32 opc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(opc, rD, rA, rB);
    s.write_gpr(rD, s.read_cr());
}

/*
 *	mtcrf		Move to Condition Register Fields
 *	Selected CR fields (via CRM mask) are replaced from gpr[rS].
 */
template <typename S> void ppc_sem_mtcrf(S &s, uint32 opc)
{
    int rS;
    uint32 crm;
    PPC_OPC_TEMPL_XFX(opc, rS, crm);

    auto val = s.read_gpr(rS);

    // Expand CRM (8-bit, one bit per CR field) to 32-bit mask
    uint32 mask = 0;
    for (int i = 0; i < 8; i++) {
        if (crm & (1 << (7 - i))) {
            mask |= 0xfu << ((7 - i) * 4);
        }
    }
    s.write_cr_masked(val, mask);
}

/*
 *	mcrf		Move Condition Register Field
 *	CR[crD] = CR[crS]
 */
template <typename S> void ppc_sem_mcrf(S &s, uint32 opc)
{
    int crD, crS, bla;
    PPC_OPC_TEMPL_X(opc, crD, crS, bla);
    crD >>= 2;
    crS >>= 2;
    s.write_cr_field(crD, s.read_cr_field(crS));
}

#endif
