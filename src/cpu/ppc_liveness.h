/*
 *	PearPC
 *	ppc_liveness.h
 *
 *	Copyright (C) 2003-2006 Sebastian Biallas (sb@biallas.net)
 *
 *	Backward liveness analysis over a basic block of PPC instructions.
 *	Given per-instruction InsnEffect, computes which outputs are dead
 *	(written but not read before the next write or block exit).
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

#ifndef __PPC_LIVENESS_H__
#define __PPC_LIVENESS_H__

#include "cpu/ppc_liveness_sem.h"

struct LivenessState {
    uint32 gpr; // bitmask: which GPRs are live
    uint32 cr;  // bitmask: which CR bits are live
    uint8 xer;  // which XER sub-fields are live
};

struct LivenessInfo {
    uint32 dead_gpr; // subset of InsnEffect.gpr_write that is dead
    uint32 dead_cr;  // subset of InsnEffect.cr_write that is dead
    uint8 dead_xer;  // subset of InsnEffect.xer_write that is dead

    bool is_dead_gpr(int r) const
    {
        return dead_gpr & (1u << r);
    }
    bool is_dead_cr_field(int f) const
    {
        uint32 mask = 0xfu << ((7 - f) * 4);
        return (dead_cr & mask) == mask;
    }
    bool is_dead_xer_ca() const
    {
        return dead_xer & XER_BIT_CA;
    }
};

// Run backward liveness on a basic block.
// insns[0..count-1] are InsnEffect in program order.
// out[0..count-1] receives per-instruction dead output masks.
// At block exit, everything is conservatively live.
static inline void ppc_compute_liveness(const InsnEffect *insns, int count, LivenessInfo *out)
{
    // Conservative: everything live at block exit
    LivenessState live;
    live.gpr = 0xffffffff;
    live.cr = 0xffffffff;
    live.xer = XER_BIT_ALL;

    for (int i = count - 1; i >= 0; i--) {
        const InsnEffect &fx = insns[i];

        if (fx.is_everything) {
            // Worst case: everything becomes live, nothing is dead
            out[i].dead_gpr = 0;
            out[i].dead_cr = 0;
            out[i].dead_xer = 0;
            live.gpr = 0xffffffff;
            live.cr = 0xffffffff;
            live.xer = XER_BIT_ALL;
            continue;
        }

        // Determine which writes are dead (written but not live)
        out[i].dead_gpr = fx.gpr_write & ~live.gpr;
        out[i].dead_cr = fx.cr_write & ~live.cr;
        out[i].dead_xer = fx.xer_write & ~live.xer;

        // Update liveness: kill writes, then gen reads
        live.gpr = (live.gpr & ~fx.gpr_write) | fx.gpr_read;
        live.cr = (live.cr & ~fx.cr_write) | fx.cr_read;
        live.xer = (live.xer & ~fx.xer_write) | fx.xer_read;
    }
}

#endif
