/*
 *  PearPC
 *  jitc_debug.cc
 *
 *  Copyright (C) 2004 Sebastian Biallas (sb@biallas.net)
 *  Copyright (C) 2026 AArch64 port
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "tools/data.h"
#include "tools/str.h"
#include "tools/endianess.h"

#include "debug/ppcdis.h"
#include "tools/snprintf.h"
#include "jitc.h"
#include "jitc_asm.h"
#include "jitc_debug.h"

#ifdef JITC_DEBUG

#include "io/prom/promosi.h"

static FILE *gDebugLog;
static AVLTree *symbols;

/*
 *  MOVZ/MOVK tracker for resolving BLR targets.
 *  The JIT emits MOVZ X16, #lo; MOVK X16, #mid, lsl#16; ... BLR X16.
 *  We accumulate the 64-bit value and look it up on BLR.
 */
static uint64 movTrackValue;
static int movTrackReg = -1;

static const char *symbolLookup(uint64 addr)
{
    KeyValue tmp(new UInt64(addr), NULL);
    ObjHandle oh = symbols->find(&tmp);
    if (oh != InvObjHandle) {
        KeyValue *kv = (KeyValue *)symbols->get(oh);
        return ((String *)kv->mValue)->contentChar();
    }
    return NULL;
}

/*
 *  CPU state field name lookup.
 *  Maps [X20, #offset] to a human-readable PPC register name.
 */
static const char *cpuFieldName(int offset, char *buf, int buflen)
{
    // Use offsetof-style knowledge from jitc_common.h / ppc_cpu.h
    // jitc pointer at 0 (8 bytes)
    if (offset == 0) return "jitc";

    // gpr[32] at offset 8, each 4 bytes
    if (offset >= 8 && offset < 8 + 32 * 4) {
        int idx = (offset - 8) / 4;
        snprintf(buf, buflen, "r%d", idx);
        return buf;
    }

    // fpr[32] at offset 136, each 8 bytes
    int fpr_base = 8 + 32 * 4;
    if (offset >= fpr_base && offset < fpr_base + 32 * 8) {
        int idx = (offset - fpr_base) / 8;
        int rem = (offset - fpr_base) % 8;
        if (rem == 0)
            snprintf(buf, buflen, "fr%d", idx);
        else
            snprintf(buf, buflen, "fr%d+%d", idx, rem);
        return buf;
    }

    int cr_ofs = fpr_base + 32 * 8;          // 392
    if (offset == cr_ofs) return "cr";

    int fpscr_ofs = cr_ofs + 4;              // 396
    if (offset == fpscr_ofs) return "fpscr";

    int xer_ofs = fpscr_ofs + 4;             // 400
    if (offset == xer_ofs) return "xer";

    int xer_ca_ofs = xer_ofs + 4;            // 404
    if (offset == xer_ca_ofs) return "xer_ca";

    int lr_ofs = xer_ca_ofs + 4;             // 408
    if (offset == lr_ofs) return "lr";

    int ctr_ofs = lr_ofs + 4;                // 412
    if (offset == ctr_ofs) return "ctr";

    int msr_ofs = ctr_ofs + 4;               // 416
    if (offset == msr_ofs) return "msr";

    int pvr_ofs = msr_ofs + 4;               // 420
    if (offset == pvr_ofs) return "pvr";

    // BATs: ibatu[4], ibatl[4], ibat_bl[4], ibat_nbl[4], ibat_bepi[4], ibat_brpn[4]
    int ibatu_ofs = pvr_ofs + 4;             // 424
    if (offset >= ibatu_ofs && offset < ibatu_ofs + 16) {
        snprintf(buf, buflen, "ibatu%d", (offset - ibatu_ofs) / 4);
        return buf;
    }
    int ibatl_ofs = ibatu_ofs + 16;
    if (offset >= ibatl_ofs && offset < ibatl_ofs + 16) {
        snprintf(buf, buflen, "ibatl%d", (offset - ibatl_ofs) / 4);
        return buf;
    }
    int ibat_brpn_end = ibatl_ofs + 4 * 16;  // skip bl, nbl, bepi, brpn

    // dbatu[4] ... dbat_brpn[4]
    int dbatu_ofs = ibat_brpn_end;
    int dbatl_ofs = dbatu_ofs + 16;
    if (offset >= dbatu_ofs && offset < dbatu_ofs + 16) {
        snprintf(buf, buflen, "dbatu%d", (offset - dbatu_ofs) / 4);
        return buf;
    }
    if (offset >= dbatl_ofs && offset < dbatl_ofs + 16) {
        snprintf(buf, buflen, "dbatl%d", (offset - dbatl_ofs) / 4);
        return buf;
    }
    int dbat_brpn_end = dbatl_ofs + 4 * 16;

    int sdr1_ofs = dbat_brpn_end;
    if (offset == sdr1_ofs) return "sdr1";

    int sr_ofs = sdr1_ofs + 4;
    if (offset >= sr_ofs && offset < sr_ofs + 64) {
        snprintf(buf, buflen, "sr%d", (offset - sr_ofs) / 4);
        return buf;
    }

    int dar_ofs = sr_ofs + 64;
    if (offset == dar_ofs) return "dar";

    int dsisr_ofs = dar_ofs + 4;
    if (offset == dsisr_ofs) return "dsisr";

    int sprg_ofs = dsisr_ofs + 4;
    if (offset >= sprg_ofs && offset < sprg_ofs + 16) {
        snprintf(buf, buflen, "sprg%d", (offset - sprg_ofs) / 4);
        return buf;
    }

    int srr0_ofs = sprg_ofs + 16;
    if (offset == srr0_ofs) return "srr0";
    if (offset == srr0_ofs + 4) return "srr1";

    int dec_ofs = srr0_ofs + 8;
    if (offset == dec_ofs) return "dec";

    int ear_ofs = dec_ofs + 4;
    if (offset == ear_ofs) return "ear";

    int pir_ofs = ear_ofs + 4;
    if (offset == pir_ofs) return "pir";

    int tb_ofs = pir_ofs + 4;
    if (offset == tb_ofs) return "tb";
    if (offset == tb_ofs + 4) return "tb+4";

    int hid_ofs = tb_ofs + 8;
    if (offset >= hid_ofs && offset < hid_ofs + 64) {
        snprintf(buf, buflen, "hid%d", (offset - hid_ofs) / 4);
        return buf;
    }

    int pc_ofs = hid_ofs + 64;
    if (offset == pc_ofs) return "pc";

    int npc_ofs = pc_ofs + 4;
    if (offset == npc_ofs) return "npc";

    int current_opc_ofs = npc_ofs + 4;
    if (offset == current_opc_ofs) return "current_opc";

    int exc_pending_ofs = current_opc_ofs + 4;
    if (offset == exc_pending_ofs) return "exception_pending";

    // Skip to jitc internal fields
    // pagetable_base, hashmask, reserve, have_reservation, tlb_last, etc.
    // pc_ofs (the field), current_code_base
    int pt_base_ofs = exc_pending_ofs + 8;
    if (offset == pt_base_ofs) return "pagetable_base";
    if (offset == pt_base_ofs + 4) return "pagetable_hashmask";
    if (offset == pt_base_ofs + 8) return "reserve";

    return NULL;
}

/*
 *  AArch64 register names
 */
static const char *xreg(int r)
{
    static const char *names[] = {
        "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
        "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
        "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
        "x24", "x25", "x26", "x27", "x28", "x29", "x30", "sp"
    };
    return names[r & 31];
}

static const char *xreg_or_zr(int r)
{
    if (r == 31) return "xzr";
    return xreg(r);
}

static const char *wreg(int r)
{
    static const char *names[] = {
        "w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7",
        "w8", "w9", "w10", "w11", "w12", "w13", "w14", "w15",
        "w16", "w17", "w18", "w19", "w20", "w21", "w22", "w23",
        "w24", "w25", "w26", "w27", "w28", "w29", "w30", "wsp"
    };
    return names[r & 31];
}

static const char *wreg_or_zr(int r)
{
    if (r == 31) return "wzr";
    return wreg(r);
}

static const char *condName(int cond)
{
    static const char *names[] = {
        "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
        "hi", "ls", "ge", "lt", "gt", "le", "al", "nv"
    };
    return names[cond & 15];
}

/*
 *  Sign-extend a bitfield
 */
static sint64 signExtend(uint64 val, int bits)
{
    uint64 mask = 1ULL << (bits - 1);
    return (sint64)((val ^ mask) - mask);
}

/*
 *  Disassemble one AArch64 instruction.
 *  Returns the disassembly string in 'result' (must be >= 128 bytes).
 *  'pc' is the address of the instruction (for PC-relative calculations).
 */
static void disasmA64(uint32 insn, uint64 pc, char *result)
{
    int rd = insn & 0x1F;
    int rn = (insn >> 5) & 0x1F;
    int rm = (insn >> 16) & 0x1F;
    int rt2 = (insn >> 10) & 0x1F;
    char fieldbuf[32];

    // NOP
    if (insn == 0xD503201F) {
        strcpy(result, "nop");
        return;
    }

    // Unconditional branch: B / BL
    // B:  0001 01ii iiii iiii iiii iiii iiii iiii
    // BL: 1001 01ii iiii iiii iiii iiii iiii iiii
    if ((insn & 0x7C000000) == 0x14000000) {
        const char *op = (insn & 0x80000000) ? "bl" : "b";
        sint64 imm26 = signExtend(insn & 0x03FFFFFF, 26);
        if (imm26 == 0) {
            snprintf(result, 256, "%s <fixup>", op);
            return;
        }
        uint64 target = pc + imm26 * 4;
        const char *sym = symbolLookup(target);
        if (sym)
            snprintf(result, 256, "%s %s", op, sym);
        else
            snprintf(result, 256, "%s 0x%llx", op, (unsigned long long)target);
        return;
    }

    // Conditional branch: B.cond
    // 0101 0100 iiii iiii iiii iiii iii0 cccc
    if ((insn & 0xFF000010) == 0x54000000) {
        int cond = insn & 0xF;
        sint64 imm19 = signExtend((insn >> 5) & 0x7FFFF, 19);
        if (imm19 == 0) {
            snprintf(result, 256, "b.%s <fixup>", condName(cond));
            return;
        }
        uint64 target = pc + imm19 * 4;
        const char *sym = symbolLookup(target);
        if (sym)
            snprintf(result, 256, "b.%s %s", condName(cond), sym);
        else
            snprintf(result, 256, "b.%s 0x%llx", condName(cond), (unsigned long long)target);
        return;
    }

    // CBZ / CBNZ (32 and 64 bit)
    // sf 011 010 0 imm19 rt  (CBZ)
    // sf 011 010 1 imm19 rt  (CBNZ)
    if ((insn & 0x7E000000) == 0x34000000) {
        int sf = (insn >> 31) & 1;
        int nz = (insn >> 24) & 1;
        sint64 imm19 = signExtend((insn >> 5) & 0x7FFFF, 19);
        uint64 target = pc + imm19 * 4;
        const char *op = nz ? "cbnz" : "cbz";
        const char *rname = sf ? xreg(rd) : wreg(rd);
        const char *sym = symbolLookup(target);
        if (sym)
            snprintf(result, 256, "%s %s, %s", op, rname, sym);
        else
            snprintf(result, 256, "%s %s, 0x%llx", op, rname, (unsigned long long)target);
        return;
    }

    // TBZ / TBNZ
    // b5 011 011 0 b40 imm14 rt  (TBZ)
    // b5 011 011 1 b40 imm14 rt  (TBNZ)
    if ((insn & 0x7E000000) == 0x36000000) {
        int nz = (insn >> 24) & 1;
        int b5 = (insn >> 31) & 1;
        int b40 = (insn >> 19) & 0x1F;
        int bit = (b5 << 5) | b40;
        sint64 imm14 = signExtend((insn >> 5) & 0x3FFF, 14);
        uint64 target = pc + imm14 * 4;
        const char *op = nz ? "tbnz" : "tbz";
        // Use x reg if bit >= 32
        const char *rname = (bit >= 32) ? xreg(rd) : wreg(rd);
        const char *sym = symbolLookup(target);
        if (sym)
            snprintf(result, 256, "%s %s, #%d, %s", op, rname, bit, sym);
        else
            snprintf(result, 256, "%s %s, #%d, 0x%llx", op, rname, bit, (unsigned long long)target);
        return;
    }

    // Unconditional branch to register: BR / BLR / RET
    // 1101 0110 0001 1111 0000 00nn nnn0 0000  BR
    // 1101 0110 0011 1111 0000 00nn nnn0 0000  BLR
    // 1101 0110 0101 1111 0000 00nn nnn0 0000  RET
    if ((insn & 0xFE1FFC1F) == 0xD61F0000) {
        int opc = (insn >> 21) & 3;
        if (opc == 0) {
            snprintf(result, 256, "br %s", xreg(rn));
            // Check MOVZ/MOVK tracker
            if (movTrackReg == rn) {
                const char *sym = symbolLookup(movTrackValue);
                if (sym)
                    snprintf(result + strlen(result), 256 - strlen(result), "  ; %s", sym);
            }
        } else if (opc == 1) {
            snprintf(result, 256, "blr %s", xreg(rn));
            if (movTrackReg == rn) {
                const char *sym = symbolLookup(movTrackValue);
                if (sym)
                    snprintf(result + strlen(result), 256 - strlen(result), "  ; %s", sym);
            }
        } else if (opc == 2) {
            if (rn == 30)
                strcpy(result, "ret");
            else
                snprintf(result, 256, "ret %s", xreg(rn));
        } else {
            snprintf(result, 256, "??? 0x%08x", insn);
        }
        movTrackReg = -1;
        return;
    }

    // Move wide: MOVZ / MOVK / MOVN
    // sf opc 10 0101 hw imm16 rd
    // opc: 00=MOVN, 10=MOVZ, 11=MOVK
    if ((insn & 0x1F800000) == 0x12800000) {
        int sf = (insn >> 31) & 1;
        int opc = (insn >> 29) & 3;
        int hw = (insn >> 21) & 3;
        uint32 imm16 = (insn >> 5) & 0xFFFF;
        int shift = hw * 16;
        const char *rname = sf ? xreg(rd) : wreg(rd);
        const char *op;
        switch (opc) {
        case 0: op = "movn"; break;
        case 2: op = "movz"; break;
        case 3: op = "movk"; break;
        default: op = "???"; break;
        }
        if (shift)
            snprintf(result, 256, "%s %s, #0x%x, lsl #%d", op, rname, imm16, shift);
        else
            snprintf(result, 256, "%s %s, #0x%x", op, rname, imm16);

        // Track MOVZ/MOVK for BLR resolution
        if (opc == 2) {
            // MOVZ: start fresh
            movTrackValue = (uint64)imm16 << shift;
            movTrackReg = rd;
        } else if (opc == 3 && movTrackReg == rd) {
            // MOVK: accumulate
            movTrackValue &= ~((uint64)0xFFFF << shift);
            movTrackValue |= (uint64)imm16 << shift;
        }
        return;
    }

    // Add/Sub immediate
    // sf op S 1000 1 sh imm12 rn rd
    // op=0: ADD/ADDS, op=1: SUB/SUBS
    if ((insn & 0x1F000000) == 0x11000000) {
        int sf = (insn >> 31) & 1;
        int op = (insn >> 30) & 1;
        int S = (insn >> 29) & 1;
        int sh = (insn >> 22) & 1;
        uint32 imm12 = (insn >> 10) & 0xFFF;
        if (sh) imm12 <<= 12;
        // CMP alias: SUBS with rd=xzr/wzr
        if (S && op && rd == 31) {
            snprintf(result, 256, "cmp %s, #0x%x",
                    sf ? xreg(rn) : wreg(rn), imm12);
        }
        // CMN alias: ADDS with rd=xzr/wzr
        else if (S && !op && rd == 31) {
            snprintf(result, 256, "cmn %s, #0x%x",
                    sf ? xreg(rn) : wreg(rn), imm12);
        }
        // MOV to/from SP alias: ADD with imm=0, one operand is SP
        else if (!S && !op && imm12 == 0 && (rd == 31 || rn == 31)) {
            snprintf(result, 256, "mov %s, %s",
                    sf ? xreg(rd) : wreg(rd),
                    sf ? xreg(rn) : wreg(rn));
        } else {
            const char *mn;
            if (op) mn = S ? "subs" : "sub";
            else    mn = S ? "adds" : "add";
            const char *rdn = sf ? xreg(rd) : wreg(rd);
            // For ADDS/SUBS, rd can be XZR
            if (S) rdn = sf ? xreg_or_zr(rd) : wreg_or_zr(rd);
            snprintf(result, 256, "%s %s, %s, #0x%x", mn,
                    rdn, sf ? xreg(rn) : wreg(rn), imm12);
        }
        movTrackReg = -1;
        return;
    }

    // Logical/Add/Sub (shifted register)
    // sf opc 01011 sh 0 rm imm6 rn rd  (Add/Sub)
    // sf opc 01010 sh 0 rm imm6 rn rd  (Logical)
    if ((insn & 0x1F000000) == 0x0A000000) {
        // Logical shifted register
        int sf = (insn >> 31) & 1;
        int opc = (insn >> 29) & 3;
        int shift_type = (insn >> 22) & 3;
        int N = (insn >> 21) & 1;
        int imm6 = (insn >> 10) & 0x3F;
        const char *op;
        // opc: 00=AND, 01=ORR, 10=EOR, 11=ANDS
        // N inverts rm
        if (N) {
            switch (opc) {
            case 0: op = "bic"; break;
            case 1: op = "orn"; break;
            case 2: op = "eon"; break;
            case 3: op = "bics"; break;
            default: op = "???"; break;
            }
        } else {
            switch (opc) {
            case 0: op = "and"; break;
            case 1: op = "orr"; break;
            case 2: op = "eor"; break;
            case 3: op = "ands"; break;
            default: op = "???"; break;
            }
        }
        // MOV alias: ORR Xd, XZR, Xm (no shift)
        if (opc == 1 && !N && rn == 31 && imm6 == 0 && shift_type == 0) {
            snprintf(result, 256, "mov %s, %s",
                    sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                    sf ? xreg_or_zr(rm) : wreg_or_zr(rm));
            movTrackReg = -1;
            return;
        }
        // TST alias: ANDS with rd=XZR
        if (opc == 3 && !N && rd == 31) {
            op = "tst";
            if (imm6)
                snprintf(result, 256, "tst %s, %s, %s #%d",
                        sf ? xreg_or_zr(rn) : wreg_or_zr(rn),
                        sf ? xreg_or_zr(rm) : wreg_or_zr(rm),
                        (shift_type == 0) ? "lsl" : (shift_type == 1) ? "lsr" : (shift_type == 2) ? "asr" : "ror",
                        imm6);
            else
                snprintf(result, 256, "tst %s, %s",
                        sf ? xreg_or_zr(rn) : wreg_or_zr(rn),
                        sf ? xreg_or_zr(rm) : wreg_or_zr(rm));
            movTrackReg = -1;
            return;
        }
        // MVN alias: ORN Xd, XZR, Xm
        if (opc == 1 && N && rn == 31) {
            if (imm6)
                snprintf(result, 256, "mvn %s, %s, %s #%d",
                        sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                        sf ? xreg_or_zr(rm) : wreg_or_zr(rm),
                        (shift_type == 0) ? "lsl" : (shift_type == 1) ? "lsr" : (shift_type == 2) ? "asr" : "ror",
                        imm6);
            else
                snprintf(result, 256, "mvn %s, %s",
                        sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                        sf ? xreg_or_zr(rm) : wreg_or_zr(rm));
            movTrackReg = -1;
            return;
        }

        if (imm6) {
            const char *sh;
            switch (shift_type) {
            case 0: sh = "lsl"; break;
            case 1: sh = "lsr"; break;
            case 2: sh = "asr"; break;
            default: sh = "ror"; break;
            }
            snprintf(result, 256, "%s %s, %s, %s, %s #%d", op,
                    sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                    sf ? xreg_or_zr(rn) : wreg_or_zr(rn),
                    sf ? xreg_or_zr(rm) : wreg_or_zr(rm),
                    sh, imm6);
        } else {
            snprintf(result, 256, "%s %s, %s, %s", op,
                    sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                    sf ? xreg_or_zr(rn) : wreg_or_zr(rn),
                    sf ? xreg_or_zr(rm) : wreg_or_zr(rm));
        }
        movTrackReg = -1;
        return;
    }

    if ((insn & 0x1F000000) == 0x0B000000) {
        // Add/Sub shifted register
        int sf = (insn >> 31) & 1;
        int op = (insn >> 30) & 1;
        int S = (insn >> 29) & 1;
        int shift_type = (insn >> 22) & 3;
        int imm6 = (insn >> 10) & 0x3F;

        // CMP alias: SUBS rd=XZR
        if (S && op && rd == 31) {
            if (imm6) {
                const char *sh = (shift_type == 0) ? "lsl" : (shift_type == 1) ? "lsr" : "asr";
                snprintf(result, 256, "cmp %s, %s, %s #%d",
                        sf ? xreg(rn) : wreg(rn),
                        sf ? xreg_or_zr(rm) : wreg_or_zr(rm),
                        sh, imm6);
            } else {
                snprintf(result, 256, "cmp %s, %s",
                        sf ? xreg(rn) : wreg(rn),
                        sf ? xreg_or_zr(rm) : wreg_or_zr(rm));
            }
        }
        // NEG alias: SUB rd, XZR, rm
        else if (!S && op && rn == 31) {
            snprintf(result, 256, "neg %s, %s",
                    sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                    sf ? xreg_or_zr(rm) : wreg_or_zr(rm));
        } else {
            const char *mn;
            if (op) mn = S ? "subs" : "sub";
            else    mn = S ? "adds" : "add";
            const char *rdn = sf ? (S ? xreg_or_zr(rd) : xreg(rd))
                                 : (S ? wreg_or_zr(rd) : wreg(rd));
            if (imm6) {
                const char *sh = (shift_type == 0) ? "lsl" : (shift_type == 1) ? "lsr" : "asr";
                snprintf(result, 256, "%s %s, %s, %s, %s #%d", mn,
                        rdn,
                        sf ? xreg(rn) : wreg(rn),
                        sf ? xreg_or_zr(rm) : wreg_or_zr(rm),
                        sh, imm6);
            } else {
                snprintf(result, 256, "%s %s, %s, %s", mn,
                        rdn,
                        sf ? xreg(rn) : wreg(rn),
                        sf ? xreg_or_zr(rm) : wreg_or_zr(rm));
            }
        }
        movTrackReg = -1;
        return;
    }

    // Bitfield: SBFM / BFM / UBFM
    // sf opc 100110 N immr imms rn rd
    if ((insn & 0x1F800000) == 0x13000000) {
        int sf = (insn >> 31) & 1;
        int opc = (insn >> 29) & 3;
        int immr = (insn >> 16) & 0x3F;
        int imms = (insn >> 10) & 0x3F;
        int width = sf ? 64 : 32;

        // Decode aliases
        if (opc == 2) {
            // UBFM
            if (imms == width - 1) {
                // LSR alias: UBFM Rd, Rn, #shift, #(width-1)
                snprintf(result, 256, "lsr %s, %s, #%d",
                        sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                        sf ? xreg_or_zr(rn) : wreg_or_zr(rn),
                        immr);
            } else if (imms + 1 == immr) {
                // LSL alias: UBFM Rd, Rn, #(-shift MOD width), #(width-1-shift)
                int shift = width - immr;
                snprintf(result, 256, "lsl %s, %s, #%d",
                        sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                        sf ? xreg_or_zr(rn) : wreg_or_zr(rn),
                        shift);
            } else if (immr == 0 && imms == 7) {
                snprintf(result, 256, "uxtb %s, %s",
                        sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                        wreg_or_zr(rn));
            } else if (immr == 0 && imms == 15) {
                snprintf(result, 256, "uxth %s, %s",
                        sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                        wreg_or_zr(rn));
            } else {
                snprintf(result, 256, "ubfm %s, %s, #%d, #%d",
                        sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                        sf ? xreg_or_zr(rn) : wreg_or_zr(rn),
                        immr, imms);
            }
        } else if (opc == 0) {
            // SBFM
            if (imms == width - 1) {
                snprintf(result, 256, "asr %s, %s, #%d",
                        sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                        sf ? xreg_or_zr(rn) : wreg_or_zr(rn),
                        immr);
            } else if (immr == 0 && imms == 7) {
                snprintf(result, 256, "sxtb %s, %s",
                        sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                        wreg_or_zr(rn));
            } else if (immr == 0 && imms == 15) {
                snprintf(result, 256, "sxth %s, %s",
                        sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                        wreg_or_zr(rn));
            } else if (immr == 0 && imms == 31 && sf) {
                snprintf(result, 256, "sxtw %s, %s",
                        xreg_or_zr(rd), wreg_or_zr(rn));
            } else {
                snprintf(result, 256, "sbfm %s, %s, #%d, #%d",
                        sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                        sf ? xreg_or_zr(rn) : wreg_or_zr(rn),
                        immr, imms);
            }
        } else if (opc == 1) {
            // BFM
            snprintf(result, 256, "bfm %s, %s, #%d, #%d",
                    sf ? xreg_or_zr(rd) : wreg_or_zr(rd),
                    sf ? xreg_or_zr(rn) : wreg_or_zr(rn),
                    immr, imms);
        } else {
            snprintf(result, 256, "??? 0x%08x", insn);
        }
        movTrackReg = -1;
        return;
    }

    // Load/Store unsigned offset
    // size opc 11 1 0 01 opc2 imm12 rn rt
    // size: 00=byte, 01=half, 10=word, 11=dword
    // opc2: 01=load, 00=store
    if ((insn & 0x3B000000) == 0x39000000) {
        int size = (insn >> 30) & 3;
        int V = (insn >> 26) & 1;
        int opc2 = (insn >> 22) & 3;
        uint32 imm12 = (insn >> 10) & 0xFFF;
        int scale = size;
        int offset = imm12 << scale;

        if (V) {
            // SIMD/FP load/store — simplified
            snprintf(result, 256, "%s (SIMD) [%s, #%d]",
                    (opc2 & 1) ? "ldr" : "str", xreg(rn), offset);
            movTrackReg = -1;
            return;
        }

        const char *op;
        const char *rname;
        switch (opc2) {
        case 0: // STR
            op = (size == 0) ? "strb" : (size == 1) ? "strh" : "str";
            rname = (size >= 3) ? xreg_or_zr(rd) : wreg_or_zr(rd);
            break;
        case 1: // LDR
            op = (size == 0) ? "ldrb" : (size == 1) ? "ldrh" : "ldr";
            rname = (size >= 3) ? xreg_or_zr(rd) : wreg_or_zr(rd);
            break;
        case 2: // LDRSW / STR (SIMD)
            if (size == 2) { op = "ldrsw"; rname = xreg_or_zr(rd); }
            else { op = "ldr?"; rname = wreg_or_zr(rd); }
            break;
        case 3: // LDRSH / LDRSB
            if (size == 0) { op = "ldrsb"; rname = wreg_or_zr(rd); }
            else if (size == 1) { op = "ldrsh"; rname = wreg_or_zr(rd); }
            else { op = "ldr?"; rname = wreg_or_zr(rd); }
            break;
        default:
            op = "???"; rname = "?";
        }

        // Annotate CPU state field if base is X20
        if (rn == 20) {
            const char *field = cpuFieldName(offset, fieldbuf, sizeof(fieldbuf));
            if (field)
                snprintf(result, 256, "%s %s, [x20, #%d]  ; %s", op, rname, offset, field);
            else
                snprintf(result, 256, "%s %s, [x20, #%d]", op, rname, offset);
        } else if (offset) {
            snprintf(result, 256, "%s %s, [%s, #%d]", op, rname, xreg(rn), offset);
        } else {
            snprintf(result, 256, "%s %s, [%s]", op, rname, xreg(rn));
        }
        movTrackReg = -1;
        return;
    }

    // Load/Store pair (pre-index and post-index, 64-bit)
    // opc 10 1 V 0 type imm7 rt2 rn rt
    // type: 001=STP post, 011=STP pre, 101=LDP post, 111=LDP pre
    if ((insn & 0x3E000000) == 0x28000000) {
        int opc = (insn >> 30) & 3;
        int V = (insn >> 26) & 1;
        int L = (insn >> 22) & 1;
        int wb = (insn >> 23) & 1;  // writeback (pre/post)
        int pre = (insn >> 24) & 1; // pre-index vs post-index
        sint32 imm7 = (sint32)signExtend((insn >> 15) & 0x7F, 7);
        int scale = (opc == 2) ? 3 : 2; // 64-bit pairs use scale 3
        if (opc >= 2) scale = 3;
        sint32 offset = imm7 << scale;

        const char *op = L ? "ldp" : "stp";
        const char *r1, *r2;
        if (opc >= 2) {
            r1 = xreg(rd);
            r2 = xreg(rt2);
        } else {
            r1 = wreg(rd);
            r2 = wreg(rt2);
        }

        if (V) {
            snprintf(result, 256, "%s (SIMD) ...", op);
        } else if (wb && pre) {
            snprintf(result, 256, "%s %s, %s, [%s, #%d]!", op, r1, r2, xreg(rn), offset);
        } else if (wb && !pre) {
            snprintf(result, 256, "%s %s, %s, [%s], #%d", op, r1, r2, xreg(rn), offset);
        } else {
            if (offset)
                snprintf(result, 256, "%s %s, %s, [%s, #%d]", op, r1, r2, xreg(rn), offset);
            else
                snprintf(result, 256, "%s %s, %s, [%s]", op, r1, r2, xreg(rn));
        }
        movTrackReg = -1;
        return;
    }

    // REV (32-bit): 0101 1010 1100 0000 0000 10nn nnnd dddd = 5AC00800
    if ((insn & 0xFFFFFC00) == 0x5AC00800) {
        snprintf(result, 256, "rev %s, %s", wreg(rd), wreg(rn));
        movTrackReg = -1;
        return;
    }

    // REV (64-bit): 1101 1010 1100 0000 0000 11nn nnnd dddd = DAC00C00
    if ((insn & 0xFFFFFC00) == 0xDAC00C00) {
        snprintf(result, 256, "rev %s, %s", xreg(rd), xreg(rn));
        movTrackReg = -1;
        return;
    }

    // REV16 (32-bit): 5AC00400
    if ((insn & 0xFFFFFC00) == 0x5AC00400) {
        snprintf(result, 256, "rev16 %s, %s", wreg(rd), wreg(rn));
        movTrackReg = -1;
        return;
    }

    // ADRP: 1 immlo 10000 immhi rd
    if ((insn & 0x9F000000) == 0x90000000) {
        uint32 immlo = (insn >> 29) & 3;
        uint32 immhi = (insn >> 5) & 0x7FFFF;
        sint64 imm = signExtend((immhi << 2) | immlo, 21);
        uint64 base = pc & ~0xFFFULL;
        uint64 target = base + (imm << 12);
        snprintf(result, 256, "adrp %s, 0x%llx", xreg(rd), (unsigned long long)target);
        movTrackReg = -1;
        return;
    }

    // ADR: 0 immlo 10000 immhi rd
    if ((insn & 0x9F000000) == 0x10000000) {
        uint32 immlo = (insn >> 29) & 3;
        uint32 immhi = (insn >> 5) & 0x7FFFF;
        sint64 imm = signExtend((immhi << 2) | immlo, 21);
        uint64 target = pc + imm;
        snprintf(result, 256, "adr %s, 0x%llx", xreg(rd), (unsigned long long)target);
        movTrackReg = -1;
        return;
    }

    // Fallback: unknown instruction
    snprintf(result, 256, "??? 0x%08x", insn);
    movTrackReg = -1;
}

inline static void disasmPPC(uint32 code, uint32 ea, char *result)
{
    PPCDisassembler dis(PPC_MODE_32);
    CPU_ADDR addr;
    addr.addr32.offset = ea;
    addr_sym_func = NULL;
    byte code_buf[4];
    createForeignInt(code_buf, code, 4, big_endian);
    strcpy(result, dis.str(dis.decode(code_buf, 4, addr), 0));
}

void jitcDebugLogAdd(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    ht_vfprintf(gDebugLog, fmt, ap);
    va_end(ap);
    fflush(gDebugLog);
}

void jitcDebugLogNewInstruction(JITC &jitc)
{
    char str[128];
    disasmPPC(jitc.current_opc, jitc.pc, str);
    jitcDebugLogAdd("%08x   %08x  %s\n", jitc.pc, jitc.current_opc, str);
    // Reset MOVZ/MOVK tracker at PPC instruction boundary
    movTrackReg = -1;
}

void jitcDebugLogEmit(JITC &jitc, const byte *insn, int size)
{
    for (int ofs = 0; ofs < size; ofs += 4) {
        uint32 word = *(uint32 *)(insn + ofs);
        char disasm[256];
        uint64 addr = (uint64)((byte *)jitc.currentPage->tcp + ofs);
        disasmA64(word, addr, disasm);
        jitcDebugLogAdd("  %p  %08x  %s\n", (void *)addr, word, disasm);
    }
}

#define ADD_SYM(func) \
    symbols->insert(new KeyValue(new UInt64(uint64(&func)), new String(#func)))

void jitcDebugInit()
{
    gDebugLog = fopen("jitc.log", "w");
    symbols = new AVLTree(true);

    // Memory access stubs
    ADD_SYM(ppc_write_effective_byte_asm);
    ADD_SYM(ppc_write_effective_half_asm);
    ADD_SYM(ppc_write_effective_word_asm);
    ADD_SYM(ppc_write_effective_dword_asm);
    ADD_SYM(ppc_read_effective_byte_asm);
    ADD_SYM(ppc_read_effective_half_z_asm);
    ADD_SYM(ppc_read_effective_half_s_asm);
    ADD_SYM(ppc_read_effective_word_asm);
    ADD_SYM(ppc_read_effective_dword_asm);

    // String/cache opcodes
    ADD_SYM(ppc_opc_icbi_asm);
    ADD_SYM(ppc_opc_stswi_asm);
    ADD_SYM(ppc_opc_lswi_asm);

    // Exception handlers
    ADD_SYM(ppc_isi_exception_asm);
    ADD_SYM(ppc_dsi_exception_asm);
    ADD_SYM(ppc_dsi_exception_special_asm);
    ADD_SYM(ppc_program_exception_asm);
    ADD_SYM(ppc_no_fpu_exception_asm);
    ADD_SYM(ppc_no_vec_exception_asm);
    ADD_SYM(ppc_sc_exception_asm);

    // Flag flush
    ADD_SYM(ppc_flush_flags_asm);
    ADD_SYM(ppc_flush_flags_signed_0_asm);
    ADD_SYM(ppc_flush_flags_signed_even_asm);
    ADD_SYM(ppc_flush_flags_signed_odd_asm);
    ADD_SYM(ppc_flush_flags_unsigned_0_asm);
    ADD_SYM(ppc_flush_flags_unsigned_even_asm);
    ADD_SYM(ppc_flush_flags_unsigned_odd_asm);

    // Branch/dispatch
    ADD_SYM(ppc_new_pc_asm);
    ADD_SYM(ppc_new_pc_rel_asm);
    ADD_SYM(ppc_new_pc_this_page_asm);
    ADD_SYM(ppc_heartbeat_ext_asm);
    ADD_SYM(ppc_heartbeat_ext_rel_asm);
    ADD_SYM(ppc_start_jitc_asm);

    // MSR / TLB
    ADD_SYM(ppc_set_msr_asm);
    ADD_SYM(ppc_mmu_tlb_invalidate_all_asm);
    ADD_SYM(ppc_mmu_tlb_invalidate_entry_asm);

    // PROM
    ADD_SYM(call_prom_osi);
}

#undef ADD_SYM

void jitcDebugDone()
{
    fclose(gDebugLog);
}

#endif
