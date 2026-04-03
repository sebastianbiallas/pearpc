/*
 *	PearPC
 *	ppc_concrete_sem.h
 *
 *	Copyright (C) 2003-2006 Sebastian Biallas (sb@biallas.net)
 *
 *	Concrete (interpreter) semantics for PPC instructions.
 *	Parameterized on CPU state type so that both the generic
 *	interpreter and the JITC interpreter fallback can use it.
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

#ifndef __PPC_CONCRETE_SEM_H__
#define __PPC_CONCRETE_SEM_H__

#include "system/types.h"
#include "cpu/common.h"

template <typename CPU> struct ConcreteSemantics {
    CPU &cpu;
    using Value = uint32;

    // --- State access ---

    Value read_gpr(int r)
    {
        return cpu.gpr[r];
    }
    void write_gpr(int r, Value v)
    {
        cpu.gpr[r] = v;
    }

    // Full-register SPR access
    Value read_lr()
    {
        return cpu.lr;
    }
    void write_lr(Value v)
    {
        cpu.lr = v;
    }
    Value read_ctr()
    {
        return cpu.ctr;
    }
    void write_ctr(Value v)
    {
        cpu.ctr = v;
    }
    Value read_xer()
    {
        return cpu.xer;
    }
    void write_xer(Value v)
    {
        cpu.xer = v;
    }

    // Full CR register access
    Value read_cr()
    {
        return cpu.cr;
    }
    void write_cr(Value v)
    {
        cpu.cr = v;
    }
    void write_cr_masked(Value src, uint32 mask)
    {
        cpu.cr = (src & mask) | (cpu.cr & ~mask);
    }
    Value read_cr_field(int f)
    {
        return (cpu.cr >> ((7 - f) * 4)) & 0xf;
    }
    void write_cr_field(int f, Value nibble)
    {
        int shift = (7 - f) * 4;
        cpu.cr = (cpu.cr & ~(0xfu << shift)) | ((nibble & 0xf) << shift);
    }

    // FPR access
    uint64 read_fpr(int r)
    {
        return cpu.fpr[r];
    }
    void write_fpr(int r, uint64 v)
    {
        cpu.fpr[r] = v;
    }

    // Fallback for unmodeled instructions
    void everything() {}

    Value read_xer_ca()
    {
        return (cpu.xer & XER_CA) ? 1 : 0;
    }

    void write_xer_ca(bool carry)
    {
        if (carry) {
            cpu.xer |= XER_CA;
        } else {
            cpu.xer &= ~XER_CA;
        }
    }

    bool read_xer_so()
    {
        return (cpu.xer & XER_SO) != 0;
    }

    void write_cr0_from_result(Value r)
    {
        cpu.cr &= 0x0fffffff;
        if (!r) {
            cpu.cr |= CR_CR0_EQ;
        } else if (r & 0x80000000) {
            cpu.cr |= CR_CR0_LT;
        } else {
            cpu.cr |= CR_CR0_GT;
        }
        if (cpu.xer & XER_SO) {
            cpu.cr |= CR_CR0_SO;
        }
    }

    void write_cr_field_signed(int field, Value a, Value b)
    {
        sint32 sa = (sint32)a;
        sint32 sb = (sint32)b;
        uint32 c;
        if (sa < sb) {
            c = 8;
        } else if (sa > sb) {
            c = 4;
        } else {
            c = 2;
        }
        if (cpu.xer & XER_SO) {
            c |= 1;
        }
        int shift = (7 - field) * 4;
        cpu.cr = (cpu.cr & ~(0xfu << shift)) | (c << shift);
    }

    void write_cr_field_unsigned(int field, Value a, Value b)
    {
        uint32 c;
        if (a < b) {
            c = 8;
        } else if (a > b) {
            c = 4;
        } else {
            c = 2;
        }
        if (cpu.xer & XER_SO) {
            c |= 1;
        }
        int shift = (7 - field) * 4;
        cpu.cr = (cpu.cr & ~(0xfu << shift)) | (c << shift);
    }

    void read_cr_bit(int) {}
    void write_cr_bit(int bit, bool val)
    {
        if (val) {
            cpu.cr |= (1u << (31 - bit));
        } else {
            cpu.cr &= ~(1u << (31 - bit));
        }
    }

    bool get_cr_bit(int bit)
    {
        return (cpu.cr & (1u << (31 - bit))) != 0;
    }

    // --- Computation ---

    Value imm(uint32 v)
    {
        return v;
    }
    Value add(Value a, Value b)
    {
        return a + b;
    }
    Value sub(Value a, Value b)
    {
        return a - b;
    }
    Value and_(Value a, Value b)
    {
        return a & b;
    }
    Value or_(Value a, Value b)
    {
        return a | b;
    }
    Value xor_(Value a, Value b)
    {
        return a ^ b;
    }
    Value not_(Value a)
    {
        return ~a;
    }
    Value neg(Value a)
    {
        return (uint32)(-(sint32)a);
    }
    Value nand_(Value a, Value b)
    {
        return ~(a & b);
    }
    Value nor_(Value a, Value b)
    {
        return ~(a | b);
    }
    Value andc(Value a, Value b)
    {
        return a & ~b;
    }
    Value orc(Value a, Value b)
    {
        return a | ~b;
    }
    Value eqv(Value a, Value b)
    {
        return ~(a ^ b);
    }

    Value mul(Value a, Value b)
    {
        return a * b;
    }

    Value mulhs(Value a, Value b)
    {
        sint64 r = (sint64)(sint32)a * (sint64)(sint32)b;
        return (uint32)((uint64)r >> 32);
    }

    Value mulhu(Value a, Value b)
    {
        uint64 r = (uint64)a * (uint64)b;
        return (uint32)(r >> 32);
    }

    Value div_s(Value a, Value b)
    {
        return (uint32)((sint32)a / (sint32)b);
    }
    Value div_u(Value a, Value b)
    {
        return a / b;
    }

    Value shl(Value a, Value sh)
    {
        uint32 s = sh & 0x3f;
        return (s > 31) ? 0 : (a << s);
    }

    Value shr(Value a, Value sh)
    {
        uint32 s = sh & 0x3f;
        return (s > 31) ? 0 : (a >> s);
    }

    Value sraw(Value a, Value sh, bool &carry_out)
    {
        uint32 s = sh & 0x3f;
        carry_out = false;
        if (a & 0x80000000) {
            uint32 result = a;
            for (uint32 i = 0; i < s; i++) {
                if (result & 1) {
                    carry_out = true;
                }
                result >>= 1;
                result |= 0x80000000;
            }
            return result;
        } else {
            return (s > 31) ? 0 : (a >> s);
        }
    }

    Value rotl(Value a, int n)
    {
        n &= 0x1f;
        return (a << n) | (a >> (32 - n));
    }

    static Value mask(int MB, int ME)
    {
        if (MB <= ME) {
            if (ME - MB == 31) {
                return 0xffffffff;
            }
            return ((1u << (ME - MB + 1)) - 1) << (31 - ME);
        }
        uint32 m = (1u << (32 - MB + ME + 1)) - 1;
        int n = (31 - ME) & 0x1f;
        return (m << n) | (m >> (32 - n));
    }

    Value extend_sign_byte(Value a)
    {
        return (a & 0x80) ? (a | 0xffffff00) : (a & 0xff);
    }

    Value extend_sign_half(Value a)
    {
        return (a & 0x8000) ? (a | 0xffff0000) : (a & 0xffff);
    }

    Value cntlzw(Value v)
    {
        uint32 n = 0;
        uint32 x = 0x80000000;
        while (!(v & x)) {
            n++;
            if (n == 32) {
                break;
            }
            x >>= 1;
        }
        return n;
    }

    static bool carry_add(uint32 a, uint32 b)
    {
        return (a + b) < a;
    }

    static bool carry_3(uint32 a, uint32 b, uint32 c)
    {
        if ((a + b) < a) {
            return true;
        }
        if ((a + b + c) < c) {
            return true;
        }
        return false;
    }
};

#endif
