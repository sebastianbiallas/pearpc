/*
 *	PearPC
 *	ppc_fpu.cc
 *
 *	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
 *	Copyright (C) 2003, 2004 Stefan Weyergraf
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

#include "debug/tracers.h"
#include "ppc_cpu.h"
#include "ppc_dec.h"
#include "ppc_fpu.h"
#include "ppc_opc.h"

// .121
#define PPC_FPR_TYPE2(a, b) (((a) << 8) | (b))

const char *ppc_fpu_get_fpr_type(ppc_fpr_type t)
{
    switch (t) {
    case ppc_fpr_norm: return "norm";
    case ppc_fpr_zero: return "zero";
    case ppc_fpr_NaN: return "NaN";
    case ppc_fpr_Inf: return "Inf";
    default: return "???";
    }
}

inline void ppc_fpu_add(uint32 fpscr, ppc_double &res, ppc_double &a, ppc_double &b)
{
    switch (PPC_FPR_TYPE2(a.type, b.type)) {
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_norm): {
        int diff = a.e - b.e;
        if (diff < 0) {
            diff = -diff;
            if (diff <= 56) {
                a.m >>= diff;
            } else if (a.m != 0) {
                a.m = 1;
            } else {
                a.m = 0;
            }
            res.e = b.e;
        } else {
            if (diff <= 56) {
                b.m >>= diff;
            } else if (b.m != 0) {
                b.m = 1;
            } else {
                b.m = 0;
            }
            res.e = a.e;
        }
        res.type = ppc_fpr_norm;
        if (a.s == b.s) {
            res.s = a.s;
            res.m = a.m + b.m;
            if (res.m & (1ULL << 56)) {
                res.m >>= 1;
                res.e++;
            }
        } else {
            res.s = a.s;
            res.m = a.m - b.m;
            if (!res.m) {
                if (FPSCR_RN(fpscr) == FPSCR_RN_MINF) {
                    res.s |= b.s;
                } else {
                    res.s &= b.s;
                }
                res.type = ppc_fpr_zero;
            } else {
                if ((sint64)res.m < 0) {
                    res.m = b.m - a.m;
                    res.s = b.s;
                }
                diff = ppc_fpu_normalize(res) - 8;
                res.e -= diff;
                res.m <<= diff;
            }
        }
        break;
    }
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_NaN):
        res.s = a.s;
        res.type = ppc_fpr_NaN;
        break;
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_zero):
        res.e = a.e;
        // fall-thru
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_norm):
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_zero):
        res.s = a.s;
        res.m = a.m;
        res.type = a.type;
        break;
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_norm):
        res.e = b.e;
        // fall-thru
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_NaN):
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_NaN):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_NaN):
        res.s = b.s;
        res.m = b.m;
        res.type = b.type;
        break;
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_Inf):
        if (a.s != b.s) {
            // +oo + -oo == NaN
            res.s = a.s ^ b.s;
            res.type = ppc_fpr_NaN;
            break;
        }
        // fall-thru
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_norm):
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_zero):
        res.s = a.s;
        res.type = a.type;
        break;
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_Inf):
        res.s = b.s;
        res.type = b.type;
        break;
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_zero):
        // round bla
        res.type = ppc_fpr_zero;
        res.s = a.s && b.s;
        break;
    }
}

inline void ppc_fpu_quadro_mshr(ppc_quadro &q, int exp)
{
    if (exp >= 64) {
        q.m1 = q.m0;
        q.m0 = 0;
        exp -= 64;
    }
    uint64 t = q.m0 & ((1ULL << exp) - 1);
    q.m0 >>= exp;
    q.m1 >>= exp;
    q.m1 |= t << (64 - exp);
}

inline void ppc_fpu_quadro_mshl(ppc_quadro &q, int exp)
{
    if (exp >= 64) {
        q.m0 = q.m1;
        q.m1 = 0;
        exp -= 64;
    }
    uint64 t = (q.m1 >> (64 - exp)) & ((1ULL << exp) - 1);
    q.m0 <<= exp;
    q.m1 <<= exp;
    q.m0 |= t;
}

inline void ppc_fpu_add_quadro_m(ppc_quadro &res, const ppc_quadro &a, const ppc_quadro &b)
{
    res.m1 = a.m1 + b.m1;
    if (res.m1 < a.m1) {
        res.m0 = a.m0 + b.m0 + 1;
    } else {
        res.m0 = a.m0 + b.m0;
    }
}

inline void ppc_fpu_sub_quadro_m(ppc_quadro &res, const ppc_quadro &a, const ppc_quadro &b)
{
    res.m1 = a.m1 - b.m1;
    if (a.m1 < b.m1) {
        res.m0 = a.m0 - b.m0 - 1;
    } else {
        res.m0 = a.m0 - b.m0;
    }
}

// res has 107 significant bits. a, b have 106 significant bits each.
inline void ppc_fpu_add_quadro(uint32 fpscr, ppc_quadro &res, ppc_quadro &a, ppc_quadro &b)
{
    // treat as 107 bit mantissa
    if (a.type == ppc_fpr_norm)
        ppc_fpu_quadro_mshl(a, 1);
    if (b.type == ppc_fpr_norm)
        ppc_fpu_quadro_mshl(b, 1);
    switch (PPC_FPR_TYPE2(a.type, b.type)) {
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_norm): {
        int diff = a.e - b.e;
        if (diff < 0) {
            diff = -diff;
            if (diff <= 107) {
                // FIXME: may set x_prime
                ppc_fpu_quadro_mshr(a, diff);
            } else if (a.m0 || a.m1) {
                a.m0 = 0;
                a.m1 = 1;
            } else {
                a.m0 = 0;
                a.m1 = 0;
            }
            res.e = b.e;
        } else {
            if (diff <= 107) {
                // FIXME: may set x_prime
                ppc_fpu_quadro_mshr(b, diff);
            } else if (b.m0 || b.m1) {
                b.m0 = 0;
                b.m1 = 1;
            } else {
                b.m0 = 0;
                b.m1 = 0;
            }
            res.e = a.e;
        }
        res.type = ppc_fpr_norm;
        if (a.s == b.s) {
            res.s = a.s;
            ppc_fpu_add_quadro_m(res, a, b);
            int X_prime = res.m1 & 1;
            if (res.m0 & (1ULL << (107 - 64))) {
                ppc_fpu_quadro_mshr(res, 1);
                res.e++;
            }
            // res = [107]
            res.m1 = (res.m1 & 0xfffffffffffffffeULL) | X_prime;
        } else {
            res.s = a.s;
            int cmp;
            if (a.m0 < b.m0) {
                cmp = -1;
            } else if (a.m0 > b.m0) {
                cmp = +1;
            } else {
                if (a.m1 < b.m1) {
                    cmp = -1;
                } else if (a.m1 > b.m1) {
                    cmp = +1;
                } else {
                    cmp = 0;
                }
            }
            if (!cmp) {
                if (FPSCR_RN(fpscr) == FPSCR_RN_MINF) {
                    res.s |= b.s;
                } else {
                    res.s &= b.s;
                }
                res.type = ppc_fpr_zero;
            } else {
                if (cmp < 0) {
                    ppc_fpu_sub_quadro_m(res, b, a);
                    res.s = b.s;
                } else {
                    ppc_fpu_sub_quadro_m(res, a, b);
                }
                diff = ppc_fpu_normalize_quadro(res) - (128 - 107);
                int X_prime = res.m1 & 1;
                res.m1 &= 0xfffffffffffffffeULL;
                ppc_fpu_quadro_mshl(res, diff);
                res.e -= diff;
                res.m1 |= X_prime;
            }
            // res = [107]
        }
        break;
    }
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_NaN):
        res.s = a.s;
        res.type = ppc_fpr_NaN;
        break;
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_zero):
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_norm):
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_zero):
        res.e = a.e;
        res.s = a.s;
        res.m0 = a.m0;
        res.m1 = a.m1;
        res.type = a.type;
        break;
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_norm):
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_NaN):
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_NaN):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_NaN):
        res.e = b.e;
        res.s = b.s;
        res.m0 = b.m0;
        res.m1 = b.m1;
        res.type = b.type;
        break;
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_Inf):
        if (a.s != b.s) {
            // +oo + -oo == NaN
            res.s = a.s ^ b.s;
            res.type = ppc_fpr_NaN;
            break;
        }
        // fall-thru
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_norm):
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_zero):
        res.s = a.s;
        res.type = a.type;
        break;
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_Inf):
        res.s = b.s;
        res.type = b.type;
        break;
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_zero):
        // round bla
        res.type = ppc_fpr_zero;
        res.s = a.s && b.s;
        break;
    }
}

inline void ppc_fpu_add_uint64_carry(uint64 &a, uint64 b, uint64 &carry)
{
    carry = (a + b < a) ? 1 : 0;
    a += b;
}

// 'res' has 56 significant bits on return, a + b have 56 significant bits each
inline void ppc_fpu_mul(ppc_double &res, const ppc_double &a, const ppc_double &b)
{
    res.s = a.s ^ b.s;
    switch (PPC_FPR_TYPE2(a.type, b.type)) {
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_norm): {
        res.type = ppc_fpr_norm;
        res.e = a.e + b.e;
        //		printf("new exp: %d\n", res.e);
        //		ht_printf("MUL:\na.m: %qb\nb.m: %qb\n", a.m, b.m);
        uint64 fH, fM1, fM2, fL;
        fL = (a.m & 0xffffffff) * (b.m & 0xffffffff); // [32] * [32] = [63,64]
        fM1 = (a.m >> 32) * (b.m & 0xffffffff);       // [24] * [32] = [55,56]
        fM2 = (a.m & 0xffffffff) * (b.m >> 32);       // [32] * [24] = [55,56]
        fH = (a.m >> 32) * (b.m >> 32);               // [24] * [24] = [47,48]
        //		ht_printf("fH: %qx fM1: %qx fM2: %qx fL: %qx\n", fH, fM1, fM2, fL);

        // calulate fH * 2^64 + (fM1 + fM2) * 2^32 + fL
        uint64 rL, rH;
        rL = fL; // rL = rH = [63,64]
        rH = fH; // rH = fH = [47,48]
        uint64 split;
        split = fM1 + fM2;
        uint64 carry;
        ppc_fpu_add_uint64_carry(rL, (split & 0xffffffff) << 32, carry); // rL = [63,64]
        rH += carry;                                                     // rH = [0 .. 2^48]
        rH += split >> 32;                                               // rH = [0:48], where 46, 47 or 48 set

        // res.m = [0   0  .. 0  | rH_48 rH_47 .. rH_0 | rL_63 rL_62 .. rL_55]
        //         [---------------------------------------------------------]
        // bit   = [63  62 .. 58 | 57    56    .. 9    | 8     7        0    ]
        //         [---------------------------------------------------------]
        //         [15 bits zero |      49 bits rH     | 8 most sign.bits rL ]
        res.m = rH << 9;
        res.m |= rL >> (64 - 9);
        // res.m = [58]

        //		ht_printf("fH: %qx fM1: %qx fM2: %qx fL: %qx\n", fH, fM1, fM2, fL);
        if (res.m & (1ULL << 57)) {
            res.m >>= 2;
            res.e += 2;
        } else if (res.m & (1ULL << 56)) {
            res.m >>= 1;
            res.e++;
        }
        // res.m = [56]
        break;
    }
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_NaN):
        res.type = a.type;
        res.e = a.e;
        break;
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_norm):
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_zero):
        res.s = a.s;
        // fall-thru
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_norm):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_norm):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_zero): res.type = a.type; break;
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_NaN):
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_NaN):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_NaN):
        res.s = b.s;
        // fall-thru
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_zero): res.type = b.type; break;
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_zero): res.type = ppc_fpr_NaN; break;
    }
}

// 'res' has 'prec' significant bits on return, a + b have 56 significant bits each
// for 111 >= prec >= 64
inline void ppc_fpu_mul_quadro(ppc_quadro &res, ppc_double &a, ppc_double &b, int prec)
{
    res.s = a.s ^ b.s;
    switch (PPC_FPR_TYPE2(a.type, b.type)) {
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_norm): {
        res.type = ppc_fpr_norm;
        res.e = a.e + b.e;
        //		printf("new exp: %d\n", res.e);
        //		ht_printf("MUL:\na.m: %016qx\nb.m: %016qx\n", a.m, b.m);
        uint64 fH, fM1, fM2, fL;
        fL = (a.m & 0xffffffff) * (b.m & 0xffffffff); // [32] * [32] = [63,64]
        fM1 = (a.m >> 32) * (b.m & 0xffffffff);       // [24] * [32] = [55,56]
        fM2 = (a.m & 0xffffffff) * (b.m >> 32);       // [32] * [24] = [55,56]
        fH = (a.m >> 32) * (b.m >> 32);               // [24] * [24] = [47,48]
        //		ht_printf("fH: %016qx fM1: %016qx fM2: %016qx fL: %016qx\n", fH, fM1, fM2, fL);

        // calulate fH * 2^64 + (fM1 + fM2) * 2^32 + fL
        uint64 rL, rH;
        rL = fL; // rL = rH = [63,64]
        rH = fH; // rH = fH = [47,48]
        uint64 split;
        split = fM1 + fM2;
        uint64 carry;
        ppc_fpu_add_uint64_carry(rL, (split & 0xffffffff) << 32, carry); // rL = [63,64]
        rH += carry;                                                     // rH = [0 .. 2^48]
        rH += split >> 32;                                               // rH = [0:48], where 46, 47 or 48 set

        // res.m0 = [0    0   .. 0   | rH_48 rH_47 .. rH_0 | rL_63 rL_62 .. rL_0]
        //          [-----------------------------------------------------------]
        // log.bit= [127  126 .. 113 | 112            64   | 63    62       0   ]
        //          [-----------------------------------------------------------]
        //          [ 15 bits zero   |      49 bits rH     |      64 bits rL    ]
        res.m0 = rH;
        res.m1 = rL;
        // res.m0|res.m1 = [111,112,113]

        //		ht_printf("res = %016qx%016qx\n", res.m0, res.m1);
        if (res.m0 & (1ULL << 48)) {
            ppc_fpu_quadro_mshr(res, 2 + (111 - prec));
            res.e += 2;
        } else if (res.m0 & (1ULL << 47)) {
            ppc_fpu_quadro_mshr(res, 1 + (111 - prec));
            res.e += 1;
        } else {
            ppc_fpu_quadro_mshr(res, 111 - prec);
        }
        // res.m0|res.m1 = [prec]
        break;
    }
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_NaN):
        res.type = a.type;
        res.e = a.e;
        break;
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_norm):
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_zero):
        res.s = a.s;
        // fall-thru
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_norm):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_norm):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_zero): res.type = a.type; break;
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_NaN):
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_NaN):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_NaN):
        res.s = b.s;
        // fall-thru
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_zero): res.type = b.type; break;
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_zero): res.type = ppc_fpr_NaN; break;
    }
}

// calculate one of these:
// + m1 * m2 + s
// + m1 * m2 - s
// - m1 * m2 + s
// - m1 * m2 - s
// using a 106 bit accumulator
//
// .752
//
inline void ppc_fpu_mul_add(uint32 fpscr, ppc_double &res, ppc_double &m1, ppc_double &m2, ppc_double &s)
{
    ppc_quadro p;
    /*	ht_printf("m1 = %d * %016qx * 2^%d, %s\n", m1.s, m1.m, m1.e,
		ppc_fpu_get_fpr_type(m1.type));
	ht_printf("m2 = %d * %016qx * 2^%d, %s\n", m2.s, m2.m, m2.e,
		ppc_fpu_get_fpr_type(m2.type));*/
    // create product with 106 significant bits
    ppc_fpu_mul_quadro(p, m1, m2, 106);
    /*	ht_printf("p = %d * %016qx%016qx * 2^%d, %s\n", p.s, p.m0, p.m1, p.e,
		ppc_fpu_get_fpr_type(p.type));*/
    // convert s into ppc_quadro
    /*	ht_printf("s = %d * %016qx * 2^%d %s\n", s.s, s.m, s.e,
		ppc_fpu_get_fpr_type(s.type));*/
    ppc_quadro q;
    q.e = s.e;
    q.s = s.s;
    q.type = s.type;
    q.m0 = 0;
    q.m1 = s.m;
    // .. with 106 significant bits
    ppc_fpu_quadro_mshl(q, 106 - 56);
    /*	ht_printf("q = %d * %016qx%016qx * 2^%d %s\n", q.s, q.m0, q.m1, q.e,
		ppc_fpu_get_fpr_type(q.type));*/
    // now we must add p, q.
    ppc_quadro x;
    ppc_fpu_add_quadro(fpscr, x, p, q);
    // x = [107]
    /*	ht_printf("x = %d * %016qx%016qx * 2^%d %s\n", x.s, x.m0, x.m1, x.e,
		ppc_fpu_get_fpr_type(x.type));*/
    res.type = x.type;
    res.s = x.s;
    res.e = x.e;
    if (x.type == ppc_fpr_norm) {
        res.m = x.m0 << 13;                // 43 bits from m0
        res.m |= (x.m1 >> (64 - 12)) << 1; // 12 bits from m1
        res.m |= x.m1 & 1;                 // X' bit from m1
    }
    /*	ht_printf("res = %d * %016qx * 2^%d %s\n", res.s, res.m, res.e,
		ppc_fpu_get_fpr_type(res.type));*/
}

inline void ppc_fpu_div(ppc_double &res, const ppc_double &a, const ppc_double &b)
{
    res.s = a.s ^ b.s;
    switch (PPC_FPR_TYPE2(a.type, b.type)) {
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_norm): {
        res.type = ppc_fpr_norm;
        res.e = a.e - b.e;
        res.m = 0;
        uint64 am = a.m, bm = b.m;
        uint i = 0;
        //		printf("DIV:\nam=%llx, bm=%llx, rm=%llx\n", am, bm, res.m);
        while (am && (i < 56)) {
            res.m <<= 1;
            if (am >= bm) {
                res.m |= 1;
                am -= bm;
            }
            am <<= 1;
            //			printf("am=%llx, bm=%llx, rm=%llx\n", am, bm, res.m);
            i++;
        }
        res.m <<= 57 - i;
        if (res.m & (1ULL << 56)) {
            res.m >>= 1;
        } else {
            res.e--;
        }
        //		printf("final: am=%llx, bm=%llx, rm=%llx\n", am, bm, res.m);
        break;
    }
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_NaN):
        res.e = a.e;
        // fall-thru
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_norm):
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_NaN, ppc_fpr_zero):
        res.s = a.s;
        // fall-thru
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_norm):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_norm): res.type = a.type; break;
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_NaN):
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_NaN):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_NaN):
        res.s = b.s;
        res.type = b.type;
        break;
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_Inf): res.type = ppc_fpr_zero; break;
    case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_zero): res.type = ppc_fpr_Inf; break;
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_zero):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_Inf):
    case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_zero): res.type = ppc_fpr_NaN; break;
    }
}

inline uint32 ppc_fpu_sqrt(uint32 fpscr, ppc_double &D, const ppc_double &B)
{
    switch (B.type) {
    case ppc_fpr_norm:
        if (B.s) {
            D.type = ppc_fpr_NaN;
            return FPSCR_VXSQRT;
        }
        // D := 1/2(D_old + B/D_old)
        D = B;
        D.e /= 2;
        for (int i = 0; i < 6; i++) {
            ppc_double D_old = D;
            ppc_double B_div_D_old;
            ppc_fpu_div(B_div_D_old, B, D_old);
            ppc_fpu_add(fpscr, D, D_old, B_div_D_old);
            D.e--;

            /*			uint64 e;
			ppc_double E = D;
			ppc_fpu_pack_double(E, e);
			printf("%.20f\n", *(double *)&e);*/
        }
        return 0;
    case ppc_fpr_zero:
        D.type = ppc_fpr_zero;
        D.s = B.s;
        return 0;
    case ppc_fpr_Inf:
        if (B.s) {
            D.type = ppc_fpr_NaN;
            return FPSCR_VXSQRT;
        } else {
            D.type = ppc_fpr_Inf;
            D.s = 0;
        }
        return 0;
    case ppc_fpr_NaN: D.type = ppc_fpr_NaN; return 0;
    }
    return 0;
}

void ppc_fpu_test(uint32 fpscr)
{
#if 0
	double bb = 1.0;
	uint64 b = *(uint64 *)&bb;
	ppc_double B;
	ppc_double D;
	ppc_fpu_unpack_double(B, b);
	ht_printf("%d\n", B.e);
	ppc_fpu_sqrt(fpscr, D, B);
	uint64 d;
	fpscr |= ppc_fpu_pack_double(fpscr, D, d);
	printf("%f\n", *(double *)&d);
/*	ppc_double A, B, C, D, E;
	ppc_fpu_unpack_double(A, 0xc00fafcd6c40e500ULL);
	ppc_fpu_unpack_double(B, 0xc00fafcd6c40e4beULL);
	B.s ^= 1;
	ppc_fpu_add(E, A, B);
	uint64 e;
	ppc_fpu_pack_double(E, e);
	ht_printf("%qx\n", e);
	ppc_fpu_add(D, E, B);*/

/*	ppc_double A, B, C;
	double a, b, c;
	A.type = B.type = ppc_fpr_norm;
	A.s = 1;
	A.e = 0;
	A.m = 0;
	A.m = ((1ULL<<56)-1)-((1ULL<<10)-1);
	ht_printf("%qb\n", A.m);
	B.s = 1;
	B.e = 0;
	B.m = 0;
	B.m = ((1ULL<<56)-1)-((1ULL<<50)-1);
	a = ppc_fpu_get_double(A);
	b = ppc_fpu_get_double(B);
	printf("%f + %f = \n", a, b);
	ppc_fpu_add(C, A, B);
	uint64 d;
	uint32 s;
	ppc_fpu_pack_double_as_single(C, d);
	ht_printf("%064qb\n", d);
	ppc_fpu_unpack_double(C, d);
	ppc_fpu_pack_single(C, s);
	ht_printf("single: %032b\n", s);
	ppc_single Cs;
	ppc_fpu_unpack_single(Cs, s);
	ppc_fpu_single_to_double(Cs, C);
//	ht_printf("%d\n", ppc_fpu_double_to_int(C));
	c = ppc_fpu_get_double(C);
	printf("%f\n", c);*/
#endif
}

/*
 *	a and b must not be NaNs
 */
inline uint32 ppc_fpu_compare(ppc_double &a, ppc_double &b)
{
    if (a.type == ppc_fpr_zero) {
        if (b.type == ppc_fpr_zero)
            return 2;
        return (b.s) ? 4 : 8;
    }
    if (b.type == ppc_fpr_zero)
        return (a.s) ? 8 : 4;
    if (a.s != b.s)
        return (a.s) ? 8 : 4;
    if (a.e > b.e)
        return (a.s) ? 8 : 4;
    if (a.e < b.e)
        return (a.s) ? 4 : 8;
    if (a.m > b.m)
        return (a.s) ? 8 : 4;
    if (a.m < b.m)
        return (a.s) ? 4 : 8;
    return 2;
}

double ppc_fpu_get_double(uint64 d)
{
    ppc_double dd;
    ppc_fpu_unpack_double(dd, d);
    return ppc_fpu_get_double(dd);
}

double ppc_fpu_get_double(ppc_double &d)
{
    if (d.type == ppc_fpr_norm) {
        double r = d.m;
        for (int i = 0; i < 55; i++) {
            r = r / 2.0;
        }
        if (d.e < 0) {
            for (int i = 0; i > d.e; i--) {
                r = r / 2.0;
            }
        } else if (d.e > 0) {
            for (int i = 0; i < d.e; i++) {
                r = r * 2.0;
            }
        }
        if (d.s)
            r = -r;
        return r;
    } else {
        return 0.0;
    }
}

/***********************************************************************************
 *
 */


/*
 *	fabsx		Floating Absolute Value
 *	.484
 */
int ppc_opc_fabsx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frD, frA, frB);
    PPC_OPC_ASSERT(frA == 0);
    aCPU.fpr[frD] = aCPU.fpr[frB] & ~FPU_SIGN_BIT;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fabs.\n");
    }
	return 0;
}
/*
 *	faddx		Floating Add (Double-Precision)
 *	.485
 */
int ppc_opc_faddx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    PPC_OPC_ASSERT(frC == 0);
    ppc_double A, B, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    if (A.s != B.s && A.type == ppc_fpr_Inf && B.type == ppc_fpr_Inf) {
        aCPU.fpscr |= FPSCR_VXISI;
    }
    ppc_fpu_add(aCPU.fpscr, D, A, B);
    aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fadd.\n");
    }
	return 0;
}
/*
 *	faddsx		Floating Add Single
 *	.486
 */
int ppc_opc_faddsx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    PPC_OPC_ASSERT(frC == 0);
    ppc_double A, B, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    if (A.s != B.s && A.type == ppc_fpr_Inf && B.type == ppc_fpr_Inf) {
        aCPU.fpscr |= FPSCR_VXISI;
    }
    ppc_fpu_add(aCPU.fpscr, D, A, B);
    aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fadds.\n");
    }
	return 0;
}
/*
 *	fcmpo		Floating Compare Ordered
 *	.488
 */
static uint32 ppc_fpu_cmp_and_mask[8] = {
    0xfffffff0, 0xffffff0f, 0xfffff0ff, 0xffff0fff, 0xfff0ffff, 0xff0fffff, 0xf0ffffff, 0x0fffffff,
};
int ppc_opc_fcmpo(PPC_CPU_State &aCPU)
{
    int crfD, frA, frB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crfD, frA, frB);
    crfD >>= 2;
    ppc_double A, B;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    uint32 cmp;
    if (A.type == ppc_fpr_NaN || B.type == ppc_fpr_NaN) {
        aCPU.fpscr |= FPSCR_VXSNAN;
        /*if (bla)*/ aCPU.fpscr |= FPSCR_VXVC;
        cmp = 1;
    } else {
        cmp = ppc_fpu_compare(A, B);
    }
    crfD = 7 - crfD;
    aCPU.fpscr &= ~0x1f000;
    aCPU.fpscr |= (cmp << 12);
    aCPU.cr &= ppc_fpu_cmp_and_mask[crfD];
    aCPU.cr |= (cmp << (crfD * 4));
	return 0;
}
/*
 *	fcmpu		Floating Compare Unordered
 *	.489
 */
int ppc_opc_fcmpu(PPC_CPU_State &aCPU)
{
    int crfD, frA, frB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crfD, frA, frB);
    crfD >>= 2;
    ppc_double A, B;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    uint32 cmp;
    if (A.type == ppc_fpr_NaN || B.type == ppc_fpr_NaN) {
        aCPU.fpscr |= FPSCR_VXSNAN;
        cmp = 1;
    } else {
        cmp = ppc_fpu_compare(A, B);
    }
    crfD = 7 - crfD;
    aCPU.fpscr &= ~0x1f000;
    aCPU.fpscr |= (cmp << 12);
    aCPU.cr &= ppc_fpu_cmp_and_mask[crfD];
    aCPU.cr |= (cmp << (crfD * 4));
	return 0;
}
/*
 *	fctiwx		Floating Convert to Integer Word
 *	.492
 */
int ppc_opc_fctiwx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frD, frA, frB);
    PPC_OPC_ASSERT(frA == 0);
    ppc_double B;
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    aCPU.fpr[frD] = ppc_fpu_double_to_int(aCPU.fpscr, B);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fctiw.\n");
    }
	return 0;
}
/*
 *	fctiwzx		Floating Convert to Integer Word with Round toward Zero
 *	.493
 */
int ppc_opc_fctiwzx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frD, frA, frB);
    PPC_OPC_ASSERT(frA == 0);
    uint32 oldfpscr = aCPU.fpscr;
    aCPU.fpscr &= ~3;
    aCPU.fpscr |= 1;
    ppc_double B;
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    aCPU.fpr[frD] = ppc_fpu_double_to_int(aCPU.fpscr, B);
    aCPU.fpscr = oldfpscr;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fctiwz.\n");
    }
	return 0;
}
/*
 *	fdivx		Floating Divide (Double-Precision)
 *	.494
 */
int ppc_opc_fdivx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    PPC_OPC_ASSERT(frC == 0);
    ppc_double A, B, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    if (A.type == ppc_fpr_zero && B.type == ppc_fpr_zero) {
        aCPU.fpscr |= FPSCR_VXZDZ;
    }
    if (A.type == ppc_fpr_Inf && B.type == ppc_fpr_Inf) {
        aCPU.fpscr |= FPSCR_VXIDI;
    }
    if (B.type == ppc_fpr_zero && A.type != ppc_fpr_zero) {
        // FIXME::
        aCPU.fpscr |= FPSCR_VXIDI;
    }
    ppc_fpu_div(D, A, B);
    aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fdiv.\n");
    }
	return 0;
}
/*
 *	fdivsx		Floating Divide Single
 *	.495
 */
int ppc_opc_fdivsx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    PPC_OPC_ASSERT(frC == 0);
    ppc_double A, B, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    if (A.type == ppc_fpr_zero && B.type == ppc_fpr_zero) {
        aCPU.fpscr |= FPSCR_VXZDZ;
    }
    if (A.type == ppc_fpr_Inf && B.type == ppc_fpr_Inf) {
        aCPU.fpscr |= FPSCR_VXIDI;
    }
    if (B.type == ppc_fpr_zero && A.type != ppc_fpr_zero) {
        // FIXME::
        aCPU.fpscr |= FPSCR_VXIDI;
    }
    ppc_fpu_div(D, A, B);
    aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fdivs.\n");
    }
	return 0;
}
/*
 *	fmaddx		Floating Multiply-Add (Double-Precision)
 *	.496
 */
int ppc_opc_fmaddx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    ppc_double A, B, C, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
    ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
    aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fmadd.\n");
    }
	return 0;
}
/*
 *	fmaddx		Floating Multiply-Add Single
 *	.497
 */
int ppc_opc_fmaddsx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    ppc_double A, B, C, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
    ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
    aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fmadds.\n");
    }
	return 0;
}
/*
 *	fmrx		Floating Move Register
 *	.498
 */
int ppc_opc_fmrx(PPC_CPU_State &aCPU)
{
    int frD, rA, frB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, frB);
    PPC_OPC_ASSERT(rA == 0);
    aCPU.fpr[frD] = aCPU.fpr[frB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fmr.\n");
    }
	return 0;
}
/*
 *	fmsubx		Floating Multiply-Subtract (Double-Precision)
 *	.499
 */
int ppc_opc_fmsubx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    ppc_double A, B, C, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
    B.s ^= 1;
    ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
    aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fmsub.\n");
    }
	return 0;
}
/*
 *	fmsubsx		Floating Multiply-Subtract Single
 *	.500
 */
int ppc_opc_fmsubsx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    ppc_double A, B, C, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
    B.s ^= 1;
    ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
    aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fmsubs.\n");
    }
	return 0;
}
/*
 *	fmulx		Floating Multiply (Double-Precision)
 *	.501
 */
int ppc_opc_fmulx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    PPC_OPC_ASSERT(frB == 0);
    ppc_double A, C, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
    if ((A.type == ppc_fpr_Inf && C.type == ppc_fpr_zero) || (A.type == ppc_fpr_zero && C.type == ppc_fpr_Inf)) {
        aCPU.fpscr |= FPSCR_VXIMZ;
    }
    ppc_fpu_mul(D, A, C);
    aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fmul.\n");
    }
	return 0;
}
/*
 *	fmulsx		Floating Multiply Single
 *	.502
 */
int ppc_opc_fmulsx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    PPC_OPC_ASSERT(frB == 0);
    ppc_double A, C, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
    if ((A.type == ppc_fpr_Inf && C.type == ppc_fpr_zero) || (A.type == ppc_fpr_zero && C.type == ppc_fpr_Inf)) {
        aCPU.fpscr |= FPSCR_VXIMZ;
    }
    ppc_fpu_mul(D, A, C);
    aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fmuls.\n");
    }
	return 0;
}
/*
 *	fnabsx		Floating Negative Absolute Value
 *	.503
 */
int ppc_opc_fnabsx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frD, frA, frB);
    PPC_OPC_ASSERT(frA == 0);
    aCPU.fpr[frD] = aCPU.fpr[frB] | FPU_SIGN_BIT;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fnabs.\n");
    }
	return 0;
}
/*
 *	fnegx		Floating Negate
 *	.504
 */
int ppc_opc_fnegx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frD, frA, frB);
    PPC_OPC_ASSERT(frA == 0);
    aCPU.fpr[frD] = aCPU.fpr[frB] ^ FPU_SIGN_BIT;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fneg.\n");
    }
	return 0;
}
/*
 *	fnmaddx		Floating Negative Multiply-Add (Double-Precision) 
 *	.505
 */
int ppc_opc_fnmaddx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    ppc_double A, B, C, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
    ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
    D.s ^= 1;
    aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fnmadd.\n");
    }
	return 0;
}
/*
 *	fnmaddsx	Floating Negative Multiply-Add Single
 *	.506
 */
int ppc_opc_fnmaddsx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    ppc_double A, B, C, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
    ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
    D.s ^= 1;
    aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fnmadds.\n");
    }
	return 0;
}
/*
 *	fnmsubx		Floating Negative Multiply-Subtract (Double-Precision)
 *	.507
 */
int ppc_opc_fnmsubx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    ppc_double A, B, C, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
    B.s ^= 1;
    ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
    D.s ^= 1;
    aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fnmsub.\n");
    }
	return 0;
}
/*
 *	fnmsubsx	Floating Negative Multiply-Subtract Single
 *	.508
 */
int ppc_opc_fnmsubsx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    ppc_double A, B, C, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
    B.s ^= 1;
    ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
    D.s ^= 1;
    aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fnmsubs.\n");
    }
	return 0;
}
/*
 *	fresx		Floating Reciprocal Estimate Single
 *	.509
 */
int ppc_opc_fresx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    PPC_OPC_ASSERT(frA == 0 && frC == 0);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fres.\n");
    }
    PPC_FPU_ERR("fres\n");
	return 0;
}
/*
 *	frspx		Floating Round to Single
 *	.511
 */
int ppc_opc_frspx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frD, frA, frB);
    PPC_OPC_ASSERT(frA == 0);
    ppc_double B;
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, B, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("frsp.\n");
    }
	return 0;
}
/*
 *	frsqrtex	Floating Reciprocal Square Root Estimate
 *	.512
 */
int ppc_opc_frsqrtex(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    PPC_OPC_ASSERT(frA == 0 && frC == 0);
    ppc_double B;
    ppc_double D;
    ppc_double E;
    ppc_double Q;
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    aCPU.fpscr |= ppc_fpu_sqrt(aCPU.fpscr, Q, B);
    E.type = ppc_fpr_norm;
    E.s = 0;
    E.e = 0;
    E.m = 0x80000000000000ULL;
    ppc_fpu_div(D, E, Q);
    aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("frsqrte.\n");
    }
	return 0;
}
/*
 *	fselx		Floating Select
 *	.514
 */
int ppc_opc_fselx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    ppc_double A;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    if (A.type == ppc_fpr_NaN || (A.type != ppc_fpr_zero && A.s)) {
        aCPU.fpr[frD] = aCPU.fpr[frB];
    } else {
        aCPU.fpr[frD] = aCPU.fpr[frC];
    }
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fsel.\n");
    }
	return 0;
}
/*
 *	fsqrtx		Floating Square Root (Double-Precision)
 *	.515
 */
int ppc_opc_fsqrtx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    PPC_OPC_ASSERT(frA == 0 && frC == 0);
    ppc_double B;
    ppc_double D;
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    ppc_fpu_sqrt(aCPU.fpscr, D, B);
    aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fsqrt.\n");
    }
	return 0;
}
/*
 *	fsqrtsx		Floating Square Root Single
 *	.515
 */
int ppc_opc_fsqrtsx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    PPC_OPC_ASSERT(frA == 0 && frC == 0);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fsqrts.\n");
    }
    PPC_FPU_ERR("fsqrts\n");
	return 0;
}
/*
 *	fsubx		Floating Subtract (Double-Precision)
 *	.517
 */
int ppc_opc_fsubx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    PPC_OPC_ASSERT(frC == 0);
    ppc_double A, B, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    if (B.type != ppc_fpr_NaN) {
        B.s ^= 1;
    }
    if (A.s != B.s && A.type == ppc_fpr_Inf && B.type == ppc_fpr_Inf) {
        aCPU.fpscr |= FPSCR_VXISI;
    }
    ppc_fpu_add(aCPU.fpscr, D, A, B);
    aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fsub.\n");
    }
	return 0;
}
/*
 *	fsubsx		Floating Subtract Single
 *	.518
 */
int ppc_opc_fsubsx(PPC_CPU_State &aCPU)
{
    int frD, frA, frB, frC;
    PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
    PPC_OPC_ASSERT(frC == 0);
    ppc_double A, B, D;
    ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
    ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
    if (B.type != ppc_fpr_NaN) {
        B.s ^= 1;
    }
    if (A.s != B.s && A.type == ppc_fpr_Inf && B.type == ppc_fpr_Inf) {
        aCPU.fpscr |= FPSCR_VXISI;
    }
    ppc_fpu_add(aCPU.fpscr, D, A, B);
    aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, aCPU.fpr[frD]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_FPU_ERR("fsubs.\n");
    }
	return 0;
}

/*
 *  === Native JIT codegen for FP register operations ===
 */

#include "jitc.h"
#include "jitc_asm.h"

#define FPR_OFS(n) (offsetof(PPC_CPU_State, fpr) + (n) * sizeof(uint64))

static void gen_check_fpu(JITC &jitc)
{
	if (!jitc.checkedFloat) {
		jitc.clobberAll();
		jitc.asmLDRw_cpu(W0, offsetof(PPC_CPU_State, msr));
		jitc.asmTSTw(W0, 19, 0); // TST W0, #(1<<13) = MSR_FP

		uint body = a64_movw_size(jitc.pc) + JITC::asmCALL_cpu_size;
		jitc.emitAssure(4 + body);
		NativeAddress target = jitc.asmHERE() + 4 + body;
		jitc.asmBccForward(A64_NE, body);

		jitc.asmMOV(W0, jitc.pc);
		jitc.asmCALL_cpu(PPC_STUB_NO_FPU_EXC);

		jitc.asmAssertHERE(target, "check_fpu_fpu");
		jitc.checkedFloat = true;
	}
}

/* fmr frD, frB — copy 64-bit fpr */
JITCFlow ppc_opc_gen_fmrx(JITC &jitc)
{
	int frD, rA, frB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frD, rA, frB);
	(void)rA;
	gen_check_fpu(jitc);
	jitc.asmLDR_cpu(X16, FPR_OFS(frB));
	jitc.asmSTR_cpu(X16, FPR_OFS(frD));
	return flowContinue;
}

/* fneg frD, frB — negate (flip sign bit 63) */
JITCFlow ppc_opc_gen_fnegx(JITC &jitc)
{
	int frD, rA, frB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frD, rA, frB);
	(void)rA;
	gen_check_fpu(jitc);
	jitc.asmLDR_cpu(X16, FPR_OFS(frB));
	// EOR X16, X16, #0x8000000000000000 (N=1, immr=0, imms=0)
	jitc.asmEOR_imm(X16, X16, 1, 0, 0);
	jitc.asmSTR_cpu(X16, FPR_OFS(frD));
	return flowContinue;
}

