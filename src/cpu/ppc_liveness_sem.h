/*
 *	PearPC
 *	ppc_liveness_sem.h
 *
 *	Copyright (C) 2003-2006 Sebastian Biallas (sb@biallas.net)
 *
 *	Liveness analysis semantics for PPC instructions.
 *	Records which architectural resources each instruction reads
 *	and writes, without executing it.
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

#ifndef __PPC_LIVENESS_SEM_H__
#define __PPC_LIVENESS_SEM_H__

#include "system/types.h"

// XER sub-field bits for InsnEffect.xer_read / xer_write
enum {
    XER_BIT_CA = 1,
    XER_BIT_OV = 2,
    XER_BIT_SO = 4,
    XER_BIT_ALL = 7,
};

struct InsnEffect {
    uint32 gpr_read;
    uint32 gpr_write;
    uint32 cr_read;  // per-bit bitmask (bits 0-31)
    uint32 cr_write; // per-bit bitmask (bits 0-31)
    uint8 xer_read;
    uint8 xer_write;
    bool lr_read, lr_write;
    bool ctr_read, ctr_write;
    bool reads_memory, writes_memory;
    bool is_branch;
    bool is_everything;

    bool reads_gpr(int r) const
    {
        return gpr_read & (1u << r);
    }
    bool writes_gpr(int r) const
    {
        return gpr_write & (1u << r);
    }
    bool reads_cr_bit(int bit) const
    {
        return cr_read & (1u << (31 - bit));
    }
    bool writes_cr_bit(int bit) const
    {
        return cr_write & (1u << (31 - bit));
    }
    bool writes_cr_field(int f) const
    {
        uint32 mask = 0xfu << ((7 - f) * 4);
        return (cr_write & mask) == mask;
    }
    bool reads_xer_ca() const
    {
        return xer_read & XER_BIT_CA;
    }
    bool reads_xer_so() const
    {
        return xer_read & XER_BIT_SO;
    }
    bool writes_xer_ca() const
    {
        return xer_write & XER_BIT_CA;
    }

    static InsnEffect everything()
    {
        InsnEffect e = {};
        e.is_everything = true;
        e.gpr_read = 0xffffffff;
        e.gpr_write = 0xffffffff;
        e.cr_read = 0xffffffff;
        e.cr_write = 0xffffffff;
        e.xer_read = XER_BIT_ALL;
        e.xer_write = XER_BIT_ALL;
        e.lr_read = e.lr_write = true;
        e.ctr_read = e.ctr_write = true;
        e.reads_memory = e.writes_memory = true;
        e.is_branch = false;
        return e;
    }
};

struct LivenessSemantics {
    InsnEffect fx;
    using Value = int; // dummy, never inspected

    LivenessSemantics() : fx{} {}

    // --- State access ---

    Value read_gpr(int r)
    {
        fx.gpr_read |= (1u << r);
        return 0;
    }
    void write_gpr(int r, Value)
    {
        fx.gpr_write |= (1u << r);
    }

    Value read_xer_ca()
    {
        fx.xer_read |= XER_BIT_CA;
        return 0;
    }
    void write_xer_ca(bool)
    {
        fx.xer_write |= XER_BIT_CA;
    }
    bool read_xer_so()
    {
        fx.xer_read |= XER_BIT_SO;
        return false;
    }

    void write_cr0_from_result(Value)
    {
        fx.cr_write |= 0xfu << 28; // CR field 0 = bits 28-31
        fx.xer_read |= XER_BIT_SO; // CR0.SO copies from XER.SO
    }

    void write_cr_field_signed(int field, Value, Value)
    {
        fx.cr_write |= 0xfu << ((7 - field) * 4);
        fx.xer_read |= XER_BIT_SO;
    }

    void write_cr_field_unsigned(int field, Value, Value)
    {
        fx.cr_write |= 0xfu << ((7 - field) * 4);
        fx.xer_read |= XER_BIT_SO;
    }

    void read_cr_bit(int bit)
    {
        fx.cr_read |= (1u << (31 - bit));
    }
    void write_cr_bit(int bit, bool)
    {
        fx.cr_write |= (1u << (31 - bit));
    }

    bool get_cr_bit(int bit)
    {
        fx.cr_read |= (1u << (31 - bit));
        return false;
    }

    // --- LR / CTR ---

    Value read_lr()
    {
        fx.lr_read = true;
        return 0;
    }
    void write_lr(Value = 0)
    {
        fx.lr_write = true;
    }
    Value read_ctr()
    {
        fx.ctr_read = true;
        return 0;
    }
    void write_ctr(Value = 0)
    {
        fx.ctr_write = true;
    }

    // --- XER (full register) ---

    Value read_xer()
    {
        fx.xer_read |= XER_BIT_ALL;
        return 0;
    }
    void write_xer(Value)
    {
        fx.xer_write |= XER_BIT_ALL;
    }

    // --- CR (full register) ---

    Value read_cr()
    {
        fx.cr_read = 0xffffffff;
        return 0;
    }
    void write_cr(Value)
    {
        fx.cr_write = 0xffffffff;
    }
    void write_cr_masked(Value, uint32 mask)
    {
        fx.cr_write |= mask;
    }
    Value read_cr_field(int f)
    {
        fx.cr_read |= (0xfu << ((7 - f) * 4));
        return 0;
    }
    void write_cr_field(int f, Value)
    {
        fx.cr_write |= (0xfu << ((7 - f) * 4));
    }

    // --- Fallback ---

    void everything()
    {
        fx = InsnEffect::everything();
    }

    // --- Memory ---

    Value read_mem(Value, int)
    {
        fx.reads_memory = true;
        return 0;
    }
    Value read_mem_sign_extend(Value, int)
    {
        fx.reads_memory = true;
        return 0;
    }
    void write_mem(Value, Value, int)
    {
        fx.writes_memory = true;
    }

    // --- Control flow ---

    void branch()
    {
        fx.is_branch = true;
    }
    void branch_cond()
    {
        fx.is_branch = true;
    }

    // --- Computation (all return dummy 0) ---

    Value imm(uint32)
    {
        return 0;
    }
    Value add(Value, Value)
    {
        return 0;
    }
    Value sub(Value, Value)
    {
        return 0;
    }
    Value and_(Value, Value)
    {
        return 0;
    }
    Value or_(Value, Value)
    {
        return 0;
    }
    Value xor_(Value, Value)
    {
        return 0;
    }
    Value not_(Value)
    {
        return 0;
    }
    Value neg(Value)
    {
        return 0;
    }
    Value nand_(Value, Value)
    {
        return 0;
    }
    Value nor_(Value, Value)
    {
        return 0;
    }
    Value andc(Value, Value)
    {
        return 0;
    }
    Value orc(Value, Value)
    {
        return 0;
    }
    Value eqv(Value, Value)
    {
        return 0;
    }

    Value mul(Value, Value)
    {
        return 0;
    }
    Value mulhs(Value, Value)
    {
        return 0;
    }
    Value mulhu(Value, Value)
    {
        return 0;
    }
    Value div_s(Value, Value)
    {
        return 0;
    }
    Value div_u(Value, Value)
    {
        return 0;
    }

    Value shl(Value, Value)
    {
        return 0;
    }
    Value shr(Value, Value)
    {
        return 0;
    }

    Value sraw(Value, Value, bool &carry_out)
    {
        carry_out = true; // conservative: always report carry
        fx.xer_write |= XER_BIT_CA;
        return 0;
    }

    Value rotl(Value, int)
    {
        return 0;
    }
    static Value mask(int, int)
    {
        return 0;
    }

    Value extend_sign_byte(Value)
    {
        return 0;
    }
    Value extend_sign_half(Value)
    {
        return 0;
    }
    Value cntlzw(Value)
    {
        return 0;
    }

    static bool carry_add(uint32, uint32)
    {
        return false;
    }
    static bool carry_3(uint32, uint32, uint32)
    {
        return false;
    }
};

#endif
