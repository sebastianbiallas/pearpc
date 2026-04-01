/*
 *	PearPC
 *	ppc_liveness.h
 *
 *	Copyright (C) 2003-2006 Sebastian Biallas (sb@biallas.net)
 *
 *	Liveness analysis for PPC instructions.
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

#include "system/arch/sysendian.h"
#include "cpu/ppc_liveness_sem.h"
#include "cpu/ppc_opc_decode.h"

// ============================================================
// Intra-block liveness (existing, conservative at block exit)
// ============================================================

struct LivenessState {
    uint32 gpr;
    uint32 cr;
    uint8 xer;
};

struct LivenessInfo {
    uint32 dead_gpr;
    uint32 dead_cr;
    uint8 dead_xer;

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

static inline void ppc_compute_liveness(const InsnEffect *insns, int count, LivenessInfo *out)
{
    LivenessState live;
    live.gpr = 0xffffffff;
    live.cr = 0xffffffff;
    live.xer = XER_BIT_ALL;

    for (int i = count - 1; i >= 0; i--) {
        const InsnEffect &fx = insns[i];
        if (fx.is_everything) {
            out[i].dead_gpr = 0;
            out[i].dead_cr = 0;
            out[i].dead_xer = 0;
            live.gpr = 0xffffffff;
            live.cr = 0xffffffff;
            live.xer = XER_BIT_ALL;
            continue;
        }
        out[i].dead_gpr = fx.gpr_write & ~live.gpr;
        out[i].dead_cr = fx.cr_write & ~live.cr;
        out[i].dead_xer = fx.xer_write & ~live.xer;
        live.gpr = (live.gpr & ~fx.gpr_write) | fx.gpr_read;
        live.cr = (live.cr & ~fx.cr_write) | fx.cr_read;
        live.xer = (live.xer & ~fx.xer_write) | fx.xer_read;
    }
}

// ============================================================
// Page-level fixed-point liveness analysis
// ============================================================

struct LiveSet {
    uint32 gpr;
    uint32 cr;
    uint8 xer;
    bool lr;
    bool ctr;

    // Accessors
    bool is_gpr_dead(int r) const
    {
        return !(gpr & (1u << r));
    }
    bool is_gpr_live(int r) const
    {
        return gpr & (1u << r);
    }
    bool is_cr_field_dead(int f) const
    {
        uint32 mask = 0xfu << ((7 - f) * 4);
        return (cr & mask) == 0;
    }
    bool is_cr_field_live(int f) const
    {
        return !is_cr_field_dead(f);
    }
    bool is_cr_bit_dead(int bit) const
    {
        return !(cr & (1u << (31 - bit)));
    }
    bool is_xer_ca_dead() const
    {
        return !(xer & XER_BIT_CA);
    }
    bool is_lr_dead() const
    {
        return !lr;
    }
    bool is_ctr_dead() const
    {
        return !ctr;
    }

    // Factories
    static LiveSet all_live()
    {
        return {0xFFFFFFFF, 0xFFFFFFFF, XER_BIT_ALL, true, true};
    }
    static LiveSet none()
    {
        return {0, 0, 0, false, false};
    }

    // Operators for dataflow
    bool operator==(const LiveSet &o) const
    {
        return gpr == o.gpr && cr == o.cr && xer == o.xer && lr == o.lr && ctr == o.ctr;
    }
    bool operator!=(const LiveSet &o) const
    {
        return !(*this == o);
    }

    LiveSet operator|(const LiveSet &o) const
    {
        return {gpr | o.gpr, cr | o.cr, (uint8)(xer | o.xer), lr || o.lr, ctr || o.ctr};
    }
    LiveSet operator&(const LiveSet &o) const
    {
        return {gpr & o.gpr, cr & o.cr, (uint8)(xer & o.xer), lr && o.lr, ctr && o.ctr};
    }
    LiveSet operator~() const
    {
        return {~gpr, ~cr, (uint8)(~xer & XER_BIT_ALL), !lr, !ctr};
    }
};

enum { PAGE_SUCC_NONE = -1, PAGE_SUCC_OFF_PAGE = -2 };

struct PageBlock {
    uint16 startOfs;
    uint16 endOfs;    // past-the-end offset
    LiveSet gen;      // resources read before written
    LiveSet kill;     // resources written
    LiveSet live_in;  // result of dataflow
    LiveSet live_out; // result of dataflow
    sint16 succ[2];   // successor block indices
    uint8 numSucc;
};

struct PageCFG {
    PageBlock blocks[512];
    int numBlocks;
    sint16 blockAtOfs[1024]; // instruction slot → block index

    // Look up the block containing a given page offset
    int blockForOfs(uint32 ofs) const
    {
        if (ofs >= 4096) {
            return -1;
        }
        return blockAtOfs[ofs / 4];
    }
};

// Decode branch target for an instruction. Returns target page offset,
// or -1 if off-page / unknown.
static inline sint32 ppc_decode_branch_target(uint32 opc, uint32 instrOfs)
{
    uint32 mainopc = PPC_OPC_MAIN(opc);
    if (mainopc == 18) {
        // bx
        uint32 li = opc & 0x03FFFFFC;
        if (li & 0x02000000) li |= 0xFC000000;
        bool aa = opc & PPC_OPC_AA;
        sint32 target = aa ? (sint32)li : (sint32)(instrOfs + (sint32)li);
        if (target >= 0 && target < 4096) return target;
    } else if (mainopc == 16) {
        // bcx
        sint32 BD = opc & 0xfffc;
        if (BD & 0x8000) BD |= 0xffff0000;
        bool aa = opc & PPC_OPC_AA;
        sint32 target = aa ? BD : (sint32)(instrOfs + BD);
        if (target >= 0 && target < 4096) return target;
    }
    return -1;
}

static inline bool ppc_is_unconditional_branch(uint32 opc)
{
    uint32 mainopc = PPC_OPC_MAIN(opc);
    if (mainopc == 18) return true; // bx is always unconditional
    if (mainopc == 16) {
        // bcx: unconditional if BO[2]=1 (don't test CTR) and BO[4]=1 (don't test CR)
        uint32 BO = (opc >> 21) & 0x1f;
        return (BO & 0x14) == 0x14;
    }
    if (mainopc == 19) {
        // bclrx (ext 16) / bcctrx (ext 528): check BO for unconditional
        uint32 ext = PPC_OPC_EXT(opc);
        if (ext == 16 || ext == 528) {
            uint32 BO = (opc >> 21) & 0x1f;
            return (BO & 0x14) == 0x14;
        }
    }
    return false;
}

// Scan a single block starting at startOfs, compute gen/kill.
// Returns the past-the-end offset of the block.
static inline uint32 ppc_scan_block(const byte *physpage, uint32 startOfs,
                                     InsnEffect (*analyze)(uint32),
                                     PageCFG &cfg, int blockIdx)
{
    PageBlock &blk = cfg.blocks[blockIdx];
    blk.startOfs = startOfs;
    blk.gen = LiveSet::none();
    blk.kill = LiveSet::none();
    blk.numSucc = 0;
    blk.succ[0] = blk.succ[1] = PAGE_SUCC_NONE;

    uint32 ofs = startOfs;
    while (ofs < 4096) {
        // If this offset already belongs to another block, stop here
        // (we've reached a block that was discovered via a different path)
        if (cfg.blockAtOfs[ofs / 4] >= 0 && cfg.blockAtOfs[ofs / 4] != blockIdx) {
            break;
        }

        uint32 opc = ppc_word_from_BE(*(uint32 *)&physpage[ofs]);
        InsnEffect fx = analyze(opc);
        cfg.blockAtOfs[ofs / 4] = blockIdx;

        if (fx.is_everything) {
            blk.gen = LiveSet::all_live();
            blk.kill = LiveSet::all_live();
            ofs += 4;
            break;
        }

        blk.gen.gpr |= (fx.gpr_read & ~blk.kill.gpr);
        blk.gen.cr |= (fx.cr_read & ~blk.kill.cr);
        blk.gen.xer |= (fx.xer_read & ~blk.kill.xer);
        if (fx.lr_read && !blk.kill.lr) blk.gen.lr = true;
        if (fx.ctr_read && !blk.kill.ctr) blk.gen.ctr = true;

        blk.kill.gpr |= fx.gpr_write;
        blk.kill.cr |= fx.cr_write;
        blk.kill.xer |= fx.xer_write;
        if (fx.lr_write) blk.kill.lr = true;
        if (fx.ctr_write) blk.kill.ctr = true;

        ofs += 4;
        if (fx.is_branch) break;
    }
    blk.endOfs = ofs;
    return ofs;
}

// Build the CFG via DFS from the entry point, compute gen/kill, solve liveness.
static inline void ppc_build_page_cfg(const byte *physpage, uint32 startOfs,
                                      InsnEffect (*analyze)(uint32), PageCFG &cfg)
{
    cfg.numBlocks = 0;
    for (int i = 0; i < 1024; i++) cfg.blockAtOfs[i] = -1;

    // --- DFS worklist: offsets to explore ---
    uint32 worklist[512];
    int worklistSize = 0;
    worklist[worklistSize++] = startOfs;

    while (worklistSize > 0 && cfg.numBlocks < 512) {
        uint32 entryOfs = worklist[--worklistSize];

        // Already discovered?
        if (cfg.blockAtOfs[entryOfs / 4] >= 0) continue;
        if (entryOfs >= 4096) continue;

        // Scan the block
        int blockIdx = cfg.numBlocks++;
        uint32 endOfs = ppc_scan_block(physpage, entryOfs, analyze, cfg, blockIdx);
        PageBlock &blk = cfg.blocks[blockIdx];

        // Determine successors and add targets to worklist
        if (endOfs <= blk.startOfs) continue; // empty block safety

        uint32 lastOfs = endOfs - 4;
        uint32 lastOpc = ppc_word_from_BE(*(uint32 *)&physpage[lastOfs]);
        InsnEffect lastFx = analyze(lastOpc);

        if (lastFx.is_branch) {
            sint32 target = ppc_decode_branch_target(lastOpc, lastOfs);
            bool uncond = ppc_is_unconditional_branch(lastOpc);

            // Branch target
            if (target >= 0 && target < 4096) {
                worklist[worklistSize++] = (uint32)target;
            }

            // Fall-through (conditional branches only)
            if (!uncond && endOfs < 4096) {
                worklist[worklistSize++] = endOfs;
            }
        } else if (lastFx.is_everything) {
            // Fall through after everything()
            if (endOfs < 4096) {
                worklist[worklistSize++] = endOfs;
            }
        } else if (endOfs < 4096 && cfg.blockAtOfs[endOfs / 4] >= 0) {
            // Fell into an existing block (not a branch, not everything, not page end)
            // This is a fall-through to the next block
        }
        // else: page end, no successors
    }

    // --- Build edges (now that all blocks are discovered) ---
    for (int i = 0; i < cfg.numBlocks; i++) {
        PageBlock &blk = cfg.blocks[i];
        uint32 lastOfs = blk.endOfs - 4;
        uint32 lastOpc = ppc_word_from_BE(*(uint32 *)&physpage[lastOfs]);
        InsnEffect lastFx = analyze(lastOpc);

        if (lastFx.is_branch) {
            sint32 target = ppc_decode_branch_target(lastOpc, lastOfs);
            bool uncond = ppc_is_unconditional_branch(lastOpc);

            if (target >= 0 && target < 4096) {
                int tgt = cfg.blockForOfs((uint32)target);
                blk.succ[0] = (tgt >= 0) ? tgt : PAGE_SUCC_OFF_PAGE;
            } else {
                blk.succ[0] = PAGE_SUCC_OFF_PAGE;
            }

            if (uncond) {
                blk.numSucc = 1;
            } else {
                if (blk.endOfs < 4096) {
                    int ft = cfg.blockForOfs(blk.endOfs);
                    blk.succ[1] = (ft >= 0) ? ft : PAGE_SUCC_OFF_PAGE;
                } else {
                    blk.succ[1] = PAGE_SUCC_OFF_PAGE;
                }
                blk.numSucc = 2;
            }
        } else if (lastFx.is_everything) {
            if (blk.endOfs < 4096) {
                int ft = cfg.blockForOfs(blk.endOfs);
                blk.succ[0] = (ft >= 0) ? ft : PAGE_SUCC_OFF_PAGE;
            } else {
                blk.succ[0] = PAGE_SUCC_OFF_PAGE;
            }
            blk.numSucc = 1;
        } else if (blk.endOfs < 4096) {
            // Non-branch, non-everything fall-through
            int ft = cfg.blockForOfs(blk.endOfs);
            if (ft >= 0) {
                blk.succ[0] = ft;
                blk.numSucc = 1;
            } else {
                blk.numSucc = 0;
            }
        } else {
            blk.numSucc = 0;
        }
    }

    // --- Fixed-point liveness ---
    for (int i = 0; i < cfg.numBlocks; i++) {
        cfg.blocks[i].live_in = LiveSet::none();
        cfg.blocks[i].live_out = LiveSet::none();
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = cfg.numBlocks - 1; i >= 0; i--) {
            PageBlock &blk = cfg.blocks[i];

            LiveSet new_live_out = LiveSet::none();
            for (int j = 0; j < blk.numSucc; j++) {
                if (blk.succ[j] == PAGE_SUCC_OFF_PAGE) {
                    new_live_out = LiveSet::all_live();
                    break;
                } else if (blk.succ[j] >= 0) {
                    new_live_out = new_live_out | cfg.blocks[blk.succ[j]].live_in;
                }
            }
            if (blk.numSucc == 0) {
                new_live_out = LiveSet::all_live();
            }

            LiveSet new_live_in = blk.gen | (new_live_out & ~blk.kill);

            if (new_live_in != blk.live_in) {
                blk.live_in = new_live_in;
                changed = true;
            }
            blk.live_out = new_live_out;
        }
    }
}

#endif
