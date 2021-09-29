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
#define PPC_FPR_TYPE2(a,b) (((a)<<8)|(b))

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
		if (diff<0) {
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
			if (res.m & (1ULL<<56)) {
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
	uint64 t = q.m0 & ((1ULL<<exp)-1);
	q.m0 >>= exp;
	q.m1 >>= exp;
	q.m1 |= t<<(64-exp);
}

inline void ppc_fpu_quadro_mshl(ppc_quadro &q, int exp)
{
	if (exp >= 64) {
		q.m0 = q.m1;
		q.m1 = 0;
		exp -= 64;
	}
	uint64 t = (q.m1 >> (64-exp)) & ((1ULL<<exp)-1);
	q.m0 <<= exp;
	q.m1 <<= exp;
	q.m0 |= t;
}

inline void ppc_fpu_add_quadro_m(ppc_quadro &res, const ppc_quadro &a, const ppc_quadro &b)
{
	res.m1 = a.m1+b.m1;
	if (res.m1 < a.m1) {
		res.m0 = a.m0+b.m0+1;
	} else {
		res.m0 = a.m0+b.m0;
	}
}

inline void ppc_fpu_sub_quadro_m(ppc_quadro &res, const ppc_quadro &a, const ppc_quadro &b)
{
	res.m1 = a.m1-b.m1;
	if (a.m1 < b.m1) {
		res.m0 = a.m0-b.m0-1;
	} else {
		res.m0 = a.m0-b.m0;
	}
}

// res has 107 significant bits. a, b have 106 significant bits each.
inline void ppc_fpu_add_quadro(uint32 fpscr, ppc_quadro &res, ppc_quadro &a, ppc_quadro &b)
{
	// treat as 107 bit mantissa
	if (a.type == ppc_fpr_norm) ppc_fpu_quadro_mshl(a, 1);
	if (b.type == ppc_fpr_norm) ppc_fpu_quadro_mshl(b, 1);
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
			if (res.m0 & (1ULL<<(107-64))) {
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
				diff = ppc_fpu_normalize_quadro(res) - (128-107);
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
	carry = (a+b < a) ? 1 : 0;
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
		fL = (a.m & 0xffffffff) * (b.m & 0xffffffff);	// [32] * [32] = [63,64]
		fM1 = (a.m >> 32) * (b.m & 0xffffffff);		// [24] * [32] = [55,56]
		fM2 = (a.m & 0xffffffff) * (b.m >> 32);		// [32] * [24] = [55,56]
		fH = (a.m >> 32) * (b.m >> 32);			// [24] * [24] = [47,48]
//		ht_printf("fH: %qx fM1: %qx fM2: %qx fL: %qx\n", fH, fM1, fM2, fL);

		// calulate fH * 2^64 + (fM1 + fM2) * 2^32 + fL
		uint64 rL, rH;
		rL = fL;					// rL = rH = [63,64]
		rH = fH;					// rH = fH = [47,48]
		uint64 split;
		split = fM1 + fM2;
		uint64 carry;
		ppc_fpu_add_uint64_carry(rL, (split & 0xffffffff) << 32, carry); // rL = [63,64]
		rH += carry;					// rH = [0 .. 2^48]
		rH += split >> 32;				// rH = [0:48], where 46, 47 or 48 set

		// res.m = [0   0  .. 0  | rH_48 rH_47 .. rH_0 | rL_63 rL_62 .. rL_55]
		//         [---------------------------------------------------------]
		// bit   = [63  62 .. 58 | 57    56    .. 9    | 8     7        0    ]
		//         [---------------------------------------------------------]
		//         [15 bits zero |      49 bits rH     | 8 most sign.bits rL ]
		res.m = rH << 9;
		res.m |= rL >> (64-9);
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
	case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_zero): 
		res.type = a.type;
		break;
	case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_NaN): 
	case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_NaN): 
	case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_NaN): 
		res.s = b.s;
		// fall-thru
	case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_Inf): 
	case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_zero): 
		res.type = b.type;
		break;
	case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_Inf): 
	case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_zero): 
		res.type = ppc_fpr_NaN;
		break;
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
		fL = (a.m & 0xffffffff) * (b.m & 0xffffffff);	// [32] * [32] = [63,64]
		fM1 = (a.m >> 32) * (b.m & 0xffffffff);		// [24] * [32] = [55,56]
		fM2 = (a.m & 0xffffffff) * (b.m >> 32);		// [32] * [24] = [55,56]
		fH = (a.m >> 32) * (b.m >> 32);			// [24] * [24] = [47,48]
//		ht_printf("fH: %016qx fM1: %016qx fM2: %016qx fL: %016qx\n", fH, fM1, fM2, fL);

		// calulate fH * 2^64 + (fM1 + fM2) * 2^32 + fL
		uint64 rL, rH;
		rL = fL;					// rL = rH = [63,64]
		rH = fH;					// rH = fH = [47,48]
		uint64 split;
		split = fM1 + fM2;
		uint64 carry;
		ppc_fpu_add_uint64_carry(rL, (split & 0xffffffff) << 32, carry); // rL = [63,64]
		rH += carry;					// rH = [0 .. 2^48]
		rH += split >> 32;				// rH = [0:48], where 46, 47 or 48 set

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
			ppc_fpu_quadro_mshr(res, 2+(111-prec));
			res.e += 2;
		} else if (res.m0 & (1ULL << 47)) {
			ppc_fpu_quadro_mshr(res, 1+(111-prec));
			res.e += 1;
		} else {
			ppc_fpu_quadro_mshr(res, 111-prec);
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
	case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_zero): 
		res.type = a.type;
		break;
	case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_NaN): 
	case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_NaN): 
	case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_NaN): 
		res.s = b.s;
		// fall-thru
	case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_Inf): 
	case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_zero): 
		res.type = b.type;
		break;
	case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_Inf): 
	case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_zero): 
		res.type = ppc_fpr_NaN;
		break;
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
inline void ppc_fpu_mul_add(uint32 fpscr, ppc_double &res, ppc_double &m1, ppc_double &m2,
	ppc_double &s)
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
	ppc_fpu_quadro_mshl(q, 106-56);
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
		res.m = x.m0 << 13;			// 43 bits from m0
		res.m |= (x.m1 >> (64-12)) << 1;	// 12 bits from m1
		res.m |= x.m1 & 1;			// X' bit from m1
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
		while (am && (i<56)) {
			res.m <<= 1;
			if (am >= bm) {
				res.m |= 1;
				am -= bm;
			}
			am <<= 1;
//			printf("am=%llx, bm=%llx, rm=%llx\n", am, bm, res.m);
			i++;
		}
		res.m <<= 57-i;
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
	case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_norm):
		res.type = a.type;
		break;
	case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_NaN):
	case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_NaN):
	case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_NaN):
		res.s = b.s;
		res.type = b.type;
		break;
	case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_Inf):
		res.type = ppc_fpr_zero;
		break;
	case PPC_FPR_TYPE2(ppc_fpr_norm, ppc_fpr_zero):
		res.type = ppc_fpr_Inf;
		break;
	case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_Inf):
	case PPC_FPR_TYPE2(ppc_fpr_Inf, ppc_fpr_zero):
	case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_Inf):
	case PPC_FPR_TYPE2(ppc_fpr_zero, ppc_fpr_zero):
		res.type = ppc_fpr_NaN;
		break;
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
		for (int i=0; i<6; i++) {
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
	case ppc_fpr_NaN:
		D.type = ppc_fpr_NaN;
		return 0;
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
		if (b.type == ppc_fpr_zero) return 2;
		return (b.s) ? 4: 8;
	}
	if (b.type == ppc_fpr_zero) return (a.s) ? 8: 4;
	if (a.s != b.s) return (a.s) ? 8: 4;
	if (a.e > b.e) return (a.s) ? 8: 4;
	if (a.e < b.e) return (a.s) ? 4: 8;
	if (a.m > b.m) return (a.s) ? 8: 4;
	if (a.m < b.m) return (a.s) ? 4: 8;
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
		for (int i=0; i<55; i++) {
			r = r / 2.0;
		}
		if (d.e < 0) {
			for (int i=0; i>d.e; i--) {
				r = r / 2.0;
			}
		} else if (d.e > 0) {
			for (int i=0; i<d.e; i++) {
				r = r * 2.0;
			}
		}
		if (d.s) r = -r;
		return r;
	} else {
		return 0.0;
	}
}

/***********************************************************************************
 *
 */

#define SWAP do {                                                    \
	int tmp = frA; frA = frB; frB = tmp;                         \
	tmp = a; a = b; b = tmp;                                     \
	X86FloatArithOp tmpop = op; op = rop; rop = tmpop;           \
} while(0);

#if 0 
static void ppc_opc_gen_binary_floatop(X86FloatArithOp op, X86FloatArithOp rop, int frD, int frA, int frB)
{
	jitcFloatRegisterClobberAll();
//	jitcSetFPUPrecision(53);
	
//	ht_printf("binfloatop: %x: %d: %d, %d, %d\n", jitc.pc, op, frD, frA, frB);
	// op == st(i)/st(0)
	// rop == st(0)/st(i)

	// op == st(0)/mem
	// rop == mem/st(0)

	// frD := frA (op) frB = frB (rop) frA

	// make sure client float register aren't mapped to integer registers
	jitcClobberClientRegisterForFloat(frA);
	jitcClobberClientRegisterForFloat(frB);
	jitcInvalidateClientRegisterForFloat(frD);

	JitcFloatReg a = jitcGetClientFloatRegisterMapping(frA);
	JitcFloatReg b = jitcGetClientFloatRegisterMapping(frB);
//	ht_printf("%d -> %d\n", frA, a);
//	ht_printf("%d -> %d\n", frB, b);
	if (a == JITC_FLOAT_REG_NONE && b != JITC_FLOAT_REG_NONE) {
		// b is mapped but not a, swap them
		SWAP;
	}
	if (a != JITC_FLOAT_REG_NONE) {
		// a is mapped
		if (frB == frD && frA != frD) {
			// b = st(a) (op) b
//			ht_printf("case a\n");
			b = jitcGetClientFloatRegister(frB, a);
			if (jitcFloatRegisterIsTOP(b)) {
				asmFArith_ST0(op, jitcFloatRegisterToNative(a));
			} else {
				jitcFloatRegisterXCHGToFront(a);
				asmFArith_STi(rop, jitcFloatRegisterToNative(b));
			}
			jitcFloatRegisterDirty(b);
		} else if (frA == frD) {
			// st(a) = st(a) (op) b
//			ht_printf("case b\n");
			b = jitcGetClientFloatRegister(frB, a);
			if (jitcFloatRegisterIsTOP(b)) {
				asmFArith_STi(op, jitcFloatRegisterToNative(a));
			} else {
				jitcFloatRegisterXCHGToFront(a);
				asmFArith_ST0(rop, jitcFloatRegisterToNative(b));
			}
			jitcFloatRegisterDirty(a);
		} else {
//			ht_printf("case c\n");
			// frA != frD != frB (and frA is mapped, frD isn't mapped)
			a = jitcFloatRegisterDup(a, b);
//			ht_printf("%d\n", b);
			// now a is TOP
			if (b != JITC_FLOAT_REG_NONE) {
				asmFArith_ST0(rop, jitcFloatRegisterToNative(b));
			} else {
				modrm_o modrm;
				asmFArith(op, x86_mem2(modrm, &aCPU.fpr[frB]));
			}
			JitcFloatReg d = jitcGetClientFloatRegisterMapping(frD);
			if (d == JITC_FLOAT_REG_NONE) {
				jitcMapClientFloatRegisterDirty(frD, a);
			} else {
				jitcFloatRegisterStoreAndPopTOP(d);
				jitcFloatRegisterDirty(d);
			}
		}
	} else {
//			ht_printf("case d\n");
		// neither a nor b is mapped
		if (frB == frD && frA != frD) {
			// frB = frA (op) frB, none of them is mapped
			b = jitcGetClientFloatRegister(frB);
			jitcFloatRegisterDirty(b);
			modrm_o modrm;
			asmFArith(rop, x86_mem2(modrm, &aCPU.fpr[frA]));
			return;
		}
		if (frA == frD) {
			// frA = frA (op) frB, none of them is mapped
			a = jitcGetClientFloatRegister(frA);
			jitcFloatRegisterDirty(a);
			modrm_o modrm;
			asmFArith(op, x86_mem2(modrm, &aCPU.fpr[frB]));
		} else {
			// frA != frD != frB (and frA, frB aren't mapped)
			a = jitcGetClientFloatRegisterUnmapped(frA);
			modrm_o modrm;
			asmFArith(op, x86_mem2(modrm, &aCPU.fpr[frB]));
			JitcFloatReg d = jitcGetClientFloatRegisterMapping(frD);
			if (d == JITC_FLOAT_REG_NONE) {
				jitcMapClientFloatRegisterDirty(frD, a);
			} else {
				jitcFloatRegisterStoreAndPopTOP(d);
				jitcFloatRegisterDirty(d);
			}
		}
	}
}

static inline void ppc_opc_gen_unary_floatop(X86FloatOp op, int frD, int frA)
{
	jitcClobberClientRegisterForFloat(frA);
	jitcInvalidateClientRegisterForFloat(frD);
	if (frD == frA) {
		JitcFloatReg a = jitcGetClientFloatRegister(frA);
		jitcFloatRegisterDirty(a);
		jitcFloatRegisterXCHGToFront(a);
	} else {
		JitcFloatReg a = jitcGetClientFloatRegisterMapping(frA);
		if (a == JITC_FLOAT_REG_NONE) {
			a = jitcGetClientFloatRegisterUnmapped(frA);
		} else {
			a = jitcFloatRegisterDup(a);
		}
		JitcFloatReg d = jitcGetClientFloatRegisterMapping(frD);
		if (d == JITC_FLOAT_REG_NONE) {
			jitcMapClientFloatRegisterDirty(frD, a);
		} else {
			asmFSimple(op);
			jitcFloatRegisterStoreAndPopTOP(d);
			jitcFloatRegisterDirty(d);
			return;
		}
	}
	asmFSimple(op);
}


/*
 fmadd     FADD  false
 fmsub     FSUB  false
 fnmadd    FADD  true
 fnmsub    FSUBR false
*/

static void ppc_opc_gen_ternary_floatop(X86FloatArithOp op, X86FloatArithOp rop, bool chs, int frD, int frA, int frC, int frB)
{
	jitcFloatRegisterClobberAll();
//	jitcSetFPUPrecision(64);
	jitcClobberClientRegisterForFloat(frA);
	jitcClobberClientRegisterForFloat(frC);
	jitcClobberClientRegisterForFloat(frB);
	jitcInvalidateClientRegisterForFloat(frD);

	JitcFloatReg a = jitcGetClientFloatRegisterMapping(frA);
	if (a != JITC_FLOAT_REG_NONE) {
		ht_printf("askf lsa flsd\n");
		a = jitcFloatRegisterDup(a, jitcGetClientFloatRegisterMapping(frC));
	} else {
		a = jitcGetClientFloatRegisterUnmapped(frA, jitcGetClientFloatRegisterMapping(frC), jitcGetClientFloatRegisterMapping(frB));
	}
	// a is TOP now
	JitcFloatReg c = jitcGetClientFloatRegisterMapping(frC);
	if (c != JITC_FLOAT_REG_NONE) {
		asmFArith_ST0(X86_FMUL, jitcFloatRegisterToNative(c));
	} else {
		modrm_o modrm;
		asmFArith(X86_FMUL, x86_mem2(modrm, &aCPU.fpr[frC]));
	}
	JitcFloatReg b = jitcGetClientFloatRegisterMapping(frB);
	if (b != JITC_FLOAT_REG_NONE) {
		asmFArith_ST0(rop, jitcFloatRegisterToNative(b));
	} else {
		modrm_o modrm;
		asmFArith(op, x86_mem2(modrm, &aCPU.fpr[frB]));
	}
	if (chs) {
		asmFSimple(FCHS);
	}
	JitcFloatReg d = jitcGetClientFloatRegisterMapping(frD);
	if (d == JITC_FLOAT_REG_NONE) {
		jitcMapClientFloatRegisterDirty(frD, a);
	} else {
		jitcFloatRegisterStoreAndPopTOP(d);
		jitcFloatRegisterDirty(d);
	}		
}

//#define JITC
 
static void FASTCALL ppc_opc_gen_update_cr1_output_err(const char *err)
{
	PPC_FPU_ERR("%s\n", err);
}

static void ppc_opc_gen_update_cr1(const char *err)
{
	asmALU(X86_MOV, EAX, (uint32)err);
	asmCALL((NativeAddress)ppc_opc_gen_update_cr1_output_err);
}
#endif

/*
 *	fabsx		Floating Absolute Value
 *	.484
 */
void ppc_opc_fabsx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, frA, frB);
	PPC_OPC_ASSERT(frA==0);
	aCPU.fpr[frD] = aCPU.fpr[frB] & ~FPU_SIGN_BIT;
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fabs.\n");
	}
}
JITCFlow ppc_opc_gen_fabsx(JITC &jitc)
{
#if 0
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frD, frA, frB);
	if (jitcGetClientFloatRegisterMapping(frB) == JITC_FLOAT_REG_NONE) {
		JitcFloatReg d = jitcGetClientFloatRegisterMapping(frD);
		if (d != JITC_FLOAT_REG_NONE) jitcFloatRegisterInvalidate(d);
		jitcClobberCarryAndFlags();	
		if (frD != frB) {
			NativeReg bh = jitcGetClientRegister(PPC_FPR_U(frB));
			NativeReg bl = jitcGetClientRegister(PPC_FPR_L(frB));
			NativeReg dh = jitcMapClientRegisterDirty(PPC_FPR_U(frD));
			NativeReg dl = jitcMapClientRegisterDirty(PPC_FPR_L(frD));
			asmALU(X86_MOV, dh, bh);
			asmALU(X86_MOV, dl, bl);
			asmALU(X86_AND, dh, 0x7fffffff);
		} else {
			NativeReg b = jitcGetClientRegisterDirty(PPC_FPR_U(frB));
			asmALU(X86_AND, b, 0x7fffffff);
		}
	} else {
		ppc_opc_gen_unary_floatop(FABS, frD, frB);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fabs.\n");
	}	
	return flowContinue;
#endif
	ppc_opc_gen_interpret(jitc, ppc_opc_fabsx);
	return flowEndBlock;
}
/*
 *	faddx		Floating Add (Double-Precision)
 *	.485
 */
void ppc_opc_faddx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frC==0);
	ppc_double A, B, D;
	ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
	ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
	if (A.s != B.s && A.type == ppc_fpr_Inf && B.type == ppc_fpr_Inf) {
		aCPU.fpscr |= FPSCR_VXISI;
	}
	ppc_fpu_add(aCPU.fpscr, D, A, B);
	aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, (uint64&)(aCPU.fpr[frD]));
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fadd.\n");
	}
}
JITCFlow ppc_opc_gen_faddx(JITC &jitc)
{
#if 0
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(jitc.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frC==0);
	ppc_opc_gen_binary_floatop(X86_FADD, X86_FADD, frD, frA, frB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fadd.\n");
	}
	return flowContinue;
#else
	ppc_opc_gen_interpret(jitc, ppc_opc_faddx);
	return flowEndBlock;
#endif
}
/*
 *	faddsx		Floating Add Single
 *	.486
 */
void ppc_opc_faddsx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frC==0);
	ppc_double A, B, D;
	ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
	ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
	if (A.s != B.s && A.type == ppc_fpr_Inf && B.type == ppc_fpr_Inf) {
		aCPU.fpscr |= FPSCR_VXISI;
	}
	ppc_fpu_add(aCPU.fpscr, D, A, B);
	aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fadds.\n");
	}
}
JITCFlow ppc_opc_gen_faddsx(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_faddsx);
	return flowEndBlock;
}
/*
 *	fcmpo		Floating Compare Ordered
 *	.488
 */
static uint32 ppc_fpu_cmp_and_mask[8] = {
	0xfffffff0,
	0xffffff0f,
	0xfffff0ff,
	0xffff0fff,
	0xfff0ffff,
	0xff0fffff,
	0xf0ffffff,
	0x0fffffff,
};
void ppc_opc_fcmpo(PPC_CPU_State &aCPU)
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
	crfD = 7-crfD;
	aCPU.fpscr &= ~0x1f000;
	aCPU.fpscr |= (cmp << 12);
	aCPU.cr &= ppc_fpu_cmp_and_mask[crfD];
	aCPU.cr |= (cmp << (crfD * 4));
}
JITCFlow ppc_opc_gen_fcmpo(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_fcmpo);
	return flowEndBlock;
}
/*
 *	fcmpu		Floating Compare Unordered
 *	.489
 */
void ppc_opc_fcmpu(PPC_CPU_State &aCPU)
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
	crfD = 7-crfD;
	aCPU.fpscr &= ~0x1f000;
	aCPU.fpscr |= (cmp << 12);
	aCPU.cr &= ppc_fpu_cmp_and_mask[crfD];
	aCPU.cr |= (cmp << (crfD * 4));
}
JITCFlow ppc_opc_gen_fcmpu(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_fcmpu);
	return flowEndBlock;
}
/*
 *	fctiwx		Floating Convert to Integer Word
 *	.492
 */
void ppc_opc_fctiwx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, frA, frB);
	PPC_OPC_ASSERT(frA==0);
	ppc_double B;
	ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
	aCPU.fpr[frD] = ppc_fpu_double_to_int(aCPU.fpscr, B);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fctiw.\n");
	}
}
JITCFlow ppc_opc_gen_fctiwx(JITC &jitc)
{
#if 0
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frD, frA, frB);
	PPC_OPC_ASSERT(frA==0);
	jitcClobberClientRegisterForFloat(frB);
	jitcInvalidateClientRegisterForFloat(frD);

	JitcFloatReg d = jitcGetClientFloatRegisterMapping(frD);
	if (frB != frD && d != JITC_FLOAT_REG_NONE) {
		jitcFloatRegisterXCHGToFront(d);
		asmFFREEP(Float_ST0);
		jitc.nativeFloatRegState[d] = rsUnused;
		jitc.clientFloatReg[frD] = JITC_FLOAT_REG_NONE;
		jitc.nativeFloatTOP--;	
	}

	modrm_o modrm;
	JitcFloatReg b = jitcGetClientFloatRegisterUnmapped(frB);
	asmFISTP_D(x86_mem2(modrm, &aCPU.fpr[frD]));
	jitc.nativeFloatRegState[b] = rsUnused;
	jitc.clientFloatReg[frB] = JITC_FLOAT_REG_NONE;
	jitc.nativeFloatTOP--;
	
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fctiw.\n");
	}
	return flowContinue;
#endif
	ppc_opc_gen_interpret(jitc, ppc_opc_fctiwx);
	return flowEndBlock;
}
/*
 *	fctiwzx		Floating Convert to Integer Word with Round toward Zero
 *	.493
 */
void ppc_opc_fctiwzx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, frA, frB);
	PPC_OPC_ASSERT(frA==0);
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
}
JITCFlow ppc_opc_gen_fctiwzx(JITC &jitc)
{
#if 0
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frD, frA, frB);
	PPC_OPC_ASSERT(frA==0);
	
	static uint16 cw = 0xfff;
	
	modrm_o modrm;
	if (!jitc.hostCPUCaps.sse3) {
		asmFLDCW(x86_mem2(modrm, &cw));
	}
	
	jitcClobberClientRegisterForFloat(frB);
	jitcInvalidateClientRegisterForFloat(frD);

	JitcFloatReg d = jitcGetClientFloatRegisterMapping(frD);
	if (frB != frD && d != JITC_FLOAT_REG_NONE) {
		jitcFloatRegisterXCHGToFront(d);
		asmFFREEP(Float_ST0);
		jitc.nativeFloatRegState[d] = rsUnused;
		jitc.clientFloatReg[frD] = JITC_FLOAT_REG_NONE;
		jitc.nativeFloatTOP--;	
	}

	JitcFloatReg b = jitcGetClientFloatRegisterUnmapped(frB);
	if (jitc.hostCPUCaps.sse3) {
		asmFISTTP(x86_mem2(modrm, &aCPU.fpr[frD]));
	} else {
		asmFISTP_D(x86_mem2(modrm, &aCPU.fpr[frD]));
	}
	jitc.nativeFloatRegState[b] = rsUnused;
	jitc.clientFloatReg[frB] = JITC_FLOAT_REG_NONE;
	jitc.nativeFloatTOP--;
	
	if (!jitc.hostCPUCaps.sse3) {
		asmFLDCW(x86_mem2(modrm, &aCPU.x87cw));
	}
	
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fctiwz.\n");
	}
	return flowContinue;
#endif
	ppc_opc_gen_interpret(jitc, ppc_opc_fctiwzx);
	return flowEndBlock;
}
/*
 *	fdivx		Floating Divide (Double-Precision)
 *	.494
 */
void ppc_opc_fdivx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frC==0);
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
	aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fdiv.\n");
	}
}
JITCFlow ppc_opc_gen_fdivx(JITC &jitc)
{
#if 1
	ppc_opc_gen_interpret(jitc, ppc_opc_fdivx);
	return flowEndBlock;
#else
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(jitc.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frC==0);
	ppc_opc_gen_binary_floatop(X86_FDIV, X86_FDIVR, frD, frA, frB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fdiv.\n");
	}
	return flowContinue;
#endif
}
/*
 *	fdivsx		Floating Divide Single
 *	.495
 */
void ppc_opc_fdivsx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frC==0);
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
	aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fdivs.\n");
	}
}
JITCFlow ppc_opc_gen_fdivsx(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_fdivsx);
	return flowEndBlock;
}
/*
 *	fmaddx		Floating Multiply-Add (Double-Precision)
 *	.496
 */
void ppc_opc_fmaddx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	ppc_double A, B, C, D;
	ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
	ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
	ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
	ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
	aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fmadd.\n");
	}
}
JITCFlow ppc_opc_gen_fmaddx(JITC &jitc)
{
#if 0
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(jitc.current_opc, frD, frA, frB, frC);
	ppc_opc_gen_ternary_floatop(X86_FADD, X86_FADD, false, frD, frA, frC, frB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fmadd.\n");
	}
	return flowContinue;
#endif
	ppc_opc_gen_interpret(jitc, ppc_opc_fmaddx);
	return flowEndBlock;
}
/*
 *	fmaddx		Floating Multiply-Add Single
 *	.497
 */
void ppc_opc_fmaddsx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	ppc_double A, B, C, D;
	ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
	ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
	ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
	ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
	aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fmadds.\n");
	}
}
JITCFlow ppc_opc_gen_fmaddsx(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_fmaddsx);
	return flowEndBlock;
}
/*
 *	fmrx		Floating Move Register
 *	.498
 */
void ppc_opc_fmrx(PPC_CPU_State &aCPU)
{
	int frD, rA, frB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, frB);
	PPC_OPC_ASSERT(rA==0);
	aCPU.fpr[frD] = aCPU.fpr[frB];
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fmr.\n");
	}
}
JITCFlow ppc_opc_gen_fmrx(JITC &jitc)
{
#if 0
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frD, frA, frB);
	if (frD != frB) {
		JitcFloatReg a = jitcGetClientFloatRegisterMapping(frB);
		if (a == JITC_FLOAT_REG_NONE) {
			JitcFloatReg d = jitcGetClientFloatRegisterMapping(frD);
			if (d != JITC_FLOAT_REG_NONE) jitcFloatRegisterInvalidate(d);
			NativeReg bu = jitcGetClientRegister(PPC_FPR_U(frB));
			NativeReg bl = jitcGetClientRegister(PPC_FPR_L(frB));
			NativeReg du = jitcMapClientRegisterDirty(PPC_FPR_U(frD));
			NativeReg dl = jitcMapClientRegisterDirty(PPC_FPR_L(frD));
			asmALU(X86_MOV, du, bu);
        		asmALU(X86_MOV, dl, bl);
		} else {
			jitcInvalidateClientRegisterForFloat(frD);
			JitcFloatReg d = jitcGetClientFloatRegisterMapping(frD);
			if (d == JITC_FLOAT_REG_NONE) {
				d = jitcFloatRegisterDup(a);
				jitcMapClientFloatRegisterDirty(frD, d);
			} else {
				jitcFloatRegisterXCHGToFront(a);
				asmFST(jitcFloatRegisterToNative(d));
				jitcFloatRegisterDirty(d);
			}
		}
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fabs.\n");
	}	
	return flowContinue;
#endif
	ppc_opc_gen_interpret(jitc, ppc_opc_fmrx);
	return flowEndBlock;
}
/*
 *	fmsubx		Floating Multiply-Subtract (Double-Precision)
 *	.499
 */
void ppc_opc_fmsubx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	ppc_double A, B, C, D;
	ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
	ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
	ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
	B.s ^= 1;
	ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
	aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fmsub.\n");
	}
}
JITCFlow ppc_opc_gen_fmsubx(JITC &jitc)
{
#if 0
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(jitc.current_opc, frD, frA, frB, frC);
	ppc_opc_gen_ternary_floatop(X86_FSUB, X86_FSUBR, false, frD, frA, frC, frB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fmsub.\n");
	}
	return flowContinue;
#endif
	ppc_opc_gen_interpret(jitc, ppc_opc_fmsubx);
	return flowEndBlock;
}
/*
 *	fmsubsx		Floating Multiply-Subtract Single
 *	.500
 */
void ppc_opc_fmsubsx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	ppc_double A, B, C, D;
	ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
	ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
	ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
	B.s ^= 1;
	ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
	aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fmsubs.\n");
	}
}
JITCFlow ppc_opc_gen_fmsubsx(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_fmsubsx);
	return flowEndBlock;
}
/*
 *	fmulx		Floating Multiply (Double-Precision)
 *	.501
 */
void ppc_opc_fmulx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frB==0);
	ppc_double A, C, D;
	ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
	ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
	if ((A.type == ppc_fpr_Inf && C.type == ppc_fpr_zero)
	 || (A.type == ppc_fpr_zero && C.type == ppc_fpr_Inf)) {
		aCPU.fpscr |= FPSCR_VXIMZ;
	}
	ppc_fpu_mul(D, A, C);
	aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fmul.\n");
	}
}
JITCFlow ppc_opc_gen_fmulx(JITC &jitc)
{
#if 0
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(jitc.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frB==0);
	ppc_opc_gen_binary_floatop(X86_FMUL, X86_FMUL, frD, frA, frC);
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fmul.\n");
	}
	return flowContinue;
#else
	ppc_opc_gen_interpret(jitc, ppc_opc_fmulx);
	return flowEndBlock;
#endif
}
/*
 *	fmulsx		Floating Multiply Single
 *	.502
 */
void ppc_opc_fmulsx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frB==0);
	ppc_double A, C, D;
	ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
	ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
	if ((A.type == ppc_fpr_Inf && C.type == ppc_fpr_zero)
	 || (A.type == ppc_fpr_zero && C.type == ppc_fpr_Inf)) {
		aCPU.fpscr |= FPSCR_VXIMZ;
	}
	ppc_fpu_mul(D, A, C);
	aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fmuls.\n");
	}
}
JITCFlow ppc_opc_gen_fmulsx(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_fmulsx);
	return flowEndBlock;
}
/*
 *	fnabsx		Floating Negative Absolute Value
 *	.503
 */
void ppc_opc_fnabsx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, frA, frB);
	PPC_OPC_ASSERT(frA==0);
	aCPU.fpr[frD] = aCPU.fpr[frB] | FPU_SIGN_BIT;
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fnabs.\n");
	}
}
JITCFlow ppc_opc_gen_fnabsx(JITC &jitc)
{
#if 0
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frD, frA, frB);
	if (jitcGetClientFloatRegisterMapping(frB) == JITC_FLOAT_REG_NONE) {
		JitcFloatReg d = jitcGetClientFloatRegisterMapping(frD);
		if (d != JITC_FLOAT_REG_NONE) jitcFloatRegisterInvalidate(d);
		jitcClobberCarryAndFlags();
		if (frD != frB) {
			NativeReg bh = jitcGetClientRegister(PPC_FPR_U(frB));
			NativeReg bl = jitcGetClientRegister(PPC_FPR_L(frB));
			NativeReg dh = jitcMapClientRegisterDirty(PPC_FPR_U(frD));
			NativeReg dl = jitcMapClientRegisterDirty(PPC_FPR_L(frD));
			asmALU(X86_MOV, dh, bh);
			asmALU(X86_MOV, dl, bl);
			asmALU(X86_OR, dh, 0x80000000);
		} else {
			NativeReg b = jitcGetClientRegisterDirty(PPC_FPR_U(frB));
			asmALU(X86_OR, b, 0x80000000);
		}
	} else {
		ppc_opc_gen_unary_floatop(FABS, frD, frB);
		asmFSimple(FCHS);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fnabs.\n");
	}	
	return flowContinue;
#endif
	ppc_opc_gen_interpret(jitc, ppc_opc_fnabsx);
	return flowEndBlock;
}
/*
 *	fnegx		Floating Negate
 *	.504
 */
void ppc_opc_fnegx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, frA, frB);
	PPC_OPC_ASSERT(frA==0);
	aCPU.fpr[frD] = aCPU.fpr[frB] ^ FPU_SIGN_BIT;
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fneg.\n");
	}
}
JITCFlow ppc_opc_gen_fnegx(JITC &jitc)
{
#if 0
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frD, frA, frB);
	if (jitcGetClientFloatRegisterMapping(frB) == JITC_FLOAT_REG_NONE) {
		JitcFloatReg d = jitcGetClientFloatRegisterMapping(frD);
		if (d != JITC_FLOAT_REG_NONE) jitcFloatRegisterInvalidate(d);
		jitcClobberCarryAndFlags();
		if (frD != frB) {
			NativeReg bh = jitcGetClientRegister(PPC_FPR_U(frB));
			NativeReg bl = jitcGetClientRegister(PPC_FPR_L(frB));
			NativeReg dh = jitcMapClientRegisterDirty(PPC_FPR_U(frD));
			NativeReg dl = jitcMapClientRegisterDirty(PPC_FPR_L(frD));
			asmALU(X86_MOV, dh, bh);
			asmALU(X86_MOV, dl, bl);
			asmALU(X86_XOR, dh, 0x80000000);
		} else {
			NativeReg b = jitcGetClientRegisterDirty(PPC_FPR_U(frB));
			asmALU(X86_XOR, b, 0x80000000);
		}
	} else {
		ppc_opc_gen_unary_floatop(FCHS, frD, frB);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fneg.\n");
	}	
	return flowContinue;
#endif
	ppc_opc_gen_interpret(jitc, ppc_opc_fnegx);
	return flowEndBlock;
}
/*
 *	fnmaddx		Floating Negative Multiply-Add (Double-Precision) 
 *	.505
 */
void ppc_opc_fnmaddx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	ppc_double A, B, C, D;
	ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
	ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
	ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
	ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
	D.s ^= 1;
	aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fnmadd.\n");
	}
}
JITCFlow ppc_opc_gen_fnmaddx(JITC &jitc)
{
#if 0
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(jitc.current_opc, frD, frA, frB, frC);
	ppc_opc_gen_ternary_floatop(X86_FADD, X86_FADD, true, frD, frA, frC, frB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fnmadd.\n");
	}
	return flowContinue;
#endif
	ppc_opc_gen_interpret(jitc, ppc_opc_fnmaddx);
	return flowEndBlock;
}
/*
 *	fnmaddsx	Floating Negative Multiply-Add Single
 *	.506
 */
void ppc_opc_fnmaddsx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	ppc_double A, B, C, D;
	ppc_fpu_unpack_double(A, aCPU.fpr[frA]);
	ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
	ppc_fpu_unpack_double(C, aCPU.fpr[frC]);
	ppc_fpu_mul_add(aCPU.fpscr, D, A, C, B);
	D.s ^= 1;
	aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fnmadds.\n");
	}
}
JITCFlow ppc_opc_gen_fnmaddsx(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_fnmaddsx);
	return flowEndBlock;
}
/*
 *	fnmsubx		Floating Negative Multiply-Subtract (Double-Precision)
 *	.507
 */
void ppc_opc_fnmsubx(PPC_CPU_State &aCPU)
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
	aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fnmsub.\n");
	}
}
JITCFlow ppc_opc_gen_fnmsubx(JITC &jitc)
{
#if 0
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(jitc.current_opc, frD, frA, frB, frC);
	ppc_opc_gen_ternary_floatop(X86_FSUBR, X86_FSUB, false, frD, frA, frC, frB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fnmsub.\n");
	}
	return flowContinue;
#endif
	ppc_opc_gen_interpret(jitc, ppc_opc_fnmsubx);
	return flowEndBlock;
}
/*
 *	fnmsubsx	Floating Negative Multiply-Subtract Single
 *	.508
 */
void ppc_opc_fnmsubsx(PPC_CPU_State &aCPU)
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
	aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fnmsubs.\n");
	}
}
JITCFlow ppc_opc_gen_fnmsubsx(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_fnmsubsx);
	return flowEndBlock;
}
/*
 *	fresx		Floating Reciprocal Estimate Single
 *	.509
 */
void ppc_opc_fresx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frA==0 && frC==0);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fres.\n");
	}
	PPC_FPU_ERR("fres\n");
}
JITCFlow ppc_opc_gen_fresx(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_fresx);
	return flowEndBlock;
}
/*
 *	frspx		Floating Round to Single
 *	.511
 */
void ppc_opc_frspx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, frA, frB);
	PPC_OPC_ASSERT(frA==0);
	ppc_double B;
	ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
	aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, B, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("frsp.\n");
	}
}
JITCFlow ppc_opc_gen_frspx(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_frspx);
	return flowEndBlock;
}
/*
 *	frsqrtex	Floating Reciprocal Square Root Estimate
 *	.512
 */
void ppc_opc_frsqrtex(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frA==0 && frC==0);
	ppc_double B;
	ppc_double D;
	ppc_double E;
	ppc_double Q;
	ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
	aCPU.fpscr |= ppc_fpu_sqrt(aCPU.fpscr, Q, B);
	E.type = ppc_fpr_norm; E.s = 0; E.e = 0; E.m = 0x80000000000000ULL;
	ppc_fpu_div(D, E, Q);
	aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);	
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("frsqrte.\n");
	}
}
JITCFlow ppc_opc_gen_frsqrtex(JITC &jitc)
{
#if 0
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(jitc.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frA==0 && frC==0);
	ppc_opc_gen_unary_floatop(FSQRT, frD, frB);
	if (jitc.nativeFloatTOP == 8) {
		jitcPopFloatStack(jitcGetClientFloatRegisterMapping(frD), JITC_FLOAT_REG_NONE);
	}
	jitc.nativeFloatTOP++;
	asmFSimple(FLD1);
	asmFArithP_STi(X86_FDIVR, jitcFloatRegisterToNative(jitcGetClientFloatRegisterMapping(frD)));
	jitc.nativeFloatTOP--;
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("frsqrte.\n");
	}
	return flowContinue;
#endif
	ppc_opc_gen_interpret(jitc, ppc_opc_frsqrtex);
	return flowEndBlock;
}
/*
 *	fselx		Floating Select
 *	.514
 */
void ppc_opc_fselx(PPC_CPU_State &aCPU)
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
}
JITCFlow ppc_opc_gen_fselx(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_fselx);
	return flowEndBlock;
}
/*
 *	fsqrtx		Floating Square Root (Double-Precision)
 *	.515
 */
void ppc_opc_fsqrtx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frA==0 && frC==0);
	ppc_double B;
	ppc_double D;
	ppc_fpu_unpack_double(B, aCPU.fpr[frB]);
	ppc_fpu_sqrt(aCPU.fpscr, D, B);
	aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fsqrt.\n");
	}
}
JITCFlow ppc_opc_gen_fsqrtx(JITC &jitc)
{
#if 0
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(jitc.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frA==0 && frC==0);
	ppc_opc_gen_unary_floatop(FSQRT, frD, frB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fsqrt.\n");
	}
	return flowContinue;
#endif
	ppc_opc_gen_interpret(jitc, ppc_opc_fsqrtx);
	return flowEndBlock;
}
/*
 *	fsqrtsx		Floating Square Root Single
 *	.515
 */
void ppc_opc_fsqrtsx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frA==0 && frC==0);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fsqrts.\n");
	}
	PPC_FPU_ERR("fsqrts\n");
}
JITCFlow ppc_opc_gen_fsqrtsx(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_fsqrtsx);
	return flowEndBlock;
}
/*
 *	fsubx		Floating Subtract (Double-Precision)
 *	.517
 */
void ppc_opc_fsubx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frC==0);
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
	aCPU.fpscr |= ppc_fpu_pack_double(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fsub.\n");
	}
}
JITCFlow ppc_opc_gen_fsubx(JITC &jitc)
{
#if 0
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(jitc.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frC==0);
	ppc_opc_gen_binary_floatop(X86_FSUB, X86_FSUBR, frD, frA, frB);
	/*
	 *	FIXME: This solves the a floating point bug.
	 *	I have no idea why.
	 */
	jitcFloatRegisterClobberAll();
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		ppc_opc_gen_update_cr1("fsub.\n");
	}
	return flowContinue;
#else
	ppc_opc_gen_interpret(jitc, ppc_opc_fsubx);
	return flowEndBlock;
#endif
}
/*
 *	fsubsx		Floating Subtract Single
 *	.518
 */
void ppc_opc_fsubsx(PPC_CPU_State &aCPU)
{
	int frD, frA, frB, frC;
	PPC_OPC_TEMPL_A(aCPU.current_opc, frD, frA, frB, frC);
	PPC_OPC_ASSERT(frC==0);
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
	aCPU.fpscr |= ppc_fpu_pack_double_as_single(aCPU.fpscr, D, (uint64&)aCPU.fpr[frD]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_FPU_ERR("fsubs.\n");
	}
}
JITCFlow ppc_opc_gen_fsubsx(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_fsubsx);
	return flowEndBlock;
}

