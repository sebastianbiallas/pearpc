/*
 *	PearPC
 *	ppc_vec.cc
 *
 *	Copyright (C) 2004 Daniel Foesch (dfoesch@cs.nmsu.edu)
 *	Copyright (C) 2026 AArch64 port
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
 
/*	Pages marked: v.???
 *	From: IBM PowerPC MicroProcessor Family: Altivec(tm) Technology...
 *		Programming Environments Manual
 */

#include <math.h>

/*
 *	FIXME: put somewhere appropriate
 */
#ifndef HAS_LOG2
#define log2(x) log(x)/log(2)
#endif /* HAS_LOG2 */ 

#ifndef HAS_EXP2
#define exp2(x)	pow(2, x)
#endif /* HAS_EXP2 */

#include "debug/tracers.h"
#include "ppc_cpu.h"
#include "ppc_dec.h"
#include "ppc_fpu.h"
#include "ppc_vec.h"

#define	SIGN32 0x80000000

/*	PACK_PIXEL	Packs a uint32 pixel to uint16 pixel
 *	v.219
 */
static inline uint16 PACK_PIXEL(uint32 clr)
{
	return	(((clr & 0x000000f8) >> 3) | \
		 ((clr & 0x0000f800) >> 6) | \
		 ((clr & 0x01f80000) >> 9));
}

/*	UNPACK_PIXEL	Unpacks a uint16 pixel to uint32 pixel
 *	v.276 & v.279
 */
static inline uint32 UNPACK_PIXEL(uint16 clr)
{
	return	(((uint32)(clr & 0x001f)) | \
		 ((uint32)(clr & 0x03E0) << 3) | \
		 ((uint32)(clr & 0x7c00) << 6) | \
		 (((clr) & 0x8000) ? 0xff000000 : 0));
}

static inline uint8 SATURATE_UB(PPC_CPU_State &aCPU, uint16 val)
{
	if (val & 0xff00) {
		aCPU.vscr |= VSCR_SAT;
		return 0xff;
	}
	return val;
}
static inline uint8 SATURATE_0B(PPC_CPU_State &aCPU, uint16 val)
{
	if (val & 0xff00) {
		aCPU.vscr |= VSCR_SAT;
		return 0;
	}
	return val;
}

static inline uint16 SATURATE_UH(PPC_CPU_State &aCPU, uint32 val)
{
	if (val & 0xffff0000) {
		aCPU.vscr |= VSCR_SAT;
		return 0xffff;
	}
	return val;
}

static inline uint16 SATURATE_0H(PPC_CPU_State &aCPU, uint32 val)
{
	if (val & 0xffff0000) {
		aCPU.vscr |= VSCR_SAT;
		return 0;
	}
	return val;
}

static inline sint8 SATURATE_SB(PPC_CPU_State &aCPU, sint16 val)
{
	if (val > 127) {			// 0x7F
		aCPU.vscr |= VSCR_SAT;
		return 127;
	} else if (val < -128) {		// 0x80
		aCPU.vscr |= VSCR_SAT;
		return -128;
	}
	return val;
}

static inline uint8 SATURATE_USB(PPC_CPU_State &aCPU, sint16 val)
{
	if (val > 0xff) {
		aCPU.vscr |= VSCR_SAT;
		return 0xff;
	} else if (val < 0) {
		aCPU.vscr |= VSCR_SAT;
		return 0;
	}
	return (uint8)val;
}

static inline sint16 SATURATE_SH(PPC_CPU_State &aCPU, sint32 val)
{
	if (val > 32767) {			// 0x7fff
		aCPU.vscr |= VSCR_SAT;
		return 32767;
	} else if (val < -32768) {		// 0x8000
		aCPU.vscr |= VSCR_SAT;
		return -32768;
	}
	return val;
}

static inline uint16 SATURATE_USH(PPC_CPU_State &aCPU, sint32 val)
{
	if (val > 0xffff) {
		aCPU.vscr |= VSCR_SAT;
		return 0xffff;
	} else if (val < 0) {
		aCPU.vscr |= VSCR_SAT;
		return 0;
	}
	return (uint16)val;
}

static inline sint32 SATURATE_UW(PPC_CPU_State &aCPU, sint64 val)
{
	if (val > 0xffffffffLL) {
		aCPU.vscr |= VSCR_SAT;
		return 0xffffffffLL;
	}
	return val;
}

static inline sint32 SATURATE_SW(PPC_CPU_State &aCPU, sint64 val)
{
	if (val > 2147483647LL) {			// 0x7fffffff
		aCPU.vscr |= VSCR_SAT;
		return 2147483647LL;
	} else if (val < -2147483648LL) {		// 0x80000000
		aCPU.vscr |= VSCR_SAT;
		return -2147483648LL;
	}
	return val;
}

/*	vperm		Vector Permutation
 *	v.218
 */
int ppc_opc_vperm(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrA, vrB, vrC;
	int sel;
	Vector_t r;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, vrC);
	for (int i=0; i<16; i++) {
		sel = aCPU.vr[vrC].b[i];
		if (sel & 0x10)
			r.b[i] = VECT_B(aCPU.vr[vrB], sel & 0xf);
		else
			r.b[i] = VECT_B(aCPU.vr[vrA], sel & 0xf);
	}

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vsel		Vector Select
 *	v.238
 */
int ppc_opc_vsel(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	uint64 mask, val;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, vrC);

	mask = aCPU.vr[vrC].d[0];
	val = aCPU.vr[vrB].d[0] & mask;
	val |= aCPU.vr[vrA].d[0] & ~mask;
	aCPU.vr[vrD].d[0] = val;

	mask = aCPU.vr[vrC].d[1];
	val = aCPU.vr[vrB].d[1] & mask;
	val |= aCPU.vr[vrA].d[1] & ~mask;
	aCPU.vr[vrD].d[1] = val;
	return 0;
}

/*	vsrb		Vector Shift Right Byte
 *	v.256
 */
int ppc_opc_vsrb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<16; i++) {
		aCPU.vr[vrD].b[i] = aCPU.vr[vrA].b[i] >> (aCPU.vr[vrB].b[i] & 0x7);
	}
	return 0;
}

/*	vsrh		Vector Shift Right Half Word
 *	v.257
 */
int ppc_opc_vsrh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<8; i++) {
		aCPU.vr[vrD].h[i] = aCPU.vr[vrA].h[i] >> (aCPU.vr[vrB].h[i] & 0xf);
	}
	return 0;
}

/*	vsrw		Vector Shift Right Word
 *	v.259
 */
int ppc_opc_vsrw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<4; i++) {
		aCPU.vr[vrD].w[i] = aCPU.vr[vrA].w[i] >> (aCPU.vr[vrB].w[i] & 0x1f);
	}
	return 0;
}

/*	vsrab		Vector Shift Right Arithmetic Byte
 *	v.253
 */
int ppc_opc_vsrab(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<16; i++) {
		aCPU.vr[vrD].sb[i] = aCPU.vr[vrA].sb[i] >> (aCPU.vr[vrB].b[i] & 0x7);
	}
	return 0;
}

/*	vsrah		Vector Shift Right Arithmetic Half Word
 *	v.254
 */
int ppc_opc_vsrah(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<8; i++) {
		aCPU.vr[vrD].sh[i] = aCPU.vr[vrA].sh[i] >> (aCPU.vr[vrB].h[i] & 0xf);
	}
	return 0;
}

/*	vsraw		Vector Shift Right Arithmetic Word
 *	v.255
 */
int ppc_opc_vsraw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<4; i++) {
		aCPU.vr[vrD].sw[i] = aCPU.vr[vrA].sw[i] >> (aCPU.vr[vrB].w[i] & 0x1f);
	}
	return 0;
}

/*	vslb		Vector Shift Left Byte
 *	v.240
 */
int ppc_opc_vslb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<16; i++) {
		aCPU.vr[vrD].b[i] = aCPU.vr[vrA].b[i] << (aCPU.vr[vrB].b[i] & 0x7);
	}
	return 0;
}

/*	vslh		Vector Shift Left Half Word
 *	v.242
 */
int ppc_opc_vslh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<8; i++) {
		aCPU.vr[vrD].h[i] = aCPU.vr[vrA].h[i] << (aCPU.vr[vrB].h[i] & 0xf);
	}
	return 0;
}

/*	vslw		Vector Shift Left Word
 *	v.244
 */
int ppc_opc_vslw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<4; i++) {
		aCPU.vr[vrD].w[i] = aCPU.vr[vrA].w[i] << (aCPU.vr[vrB].w[i] & 0x1f);
	}
	return 0;
}

/*	vsr		Vector Shift Right
 *	v.251
 */
int ppc_opc_vsr(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	int shift;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	/* Specs say that the low-order 3 bits of all byte elements in vB
	 *   must be the same, or the result is undefined.  So we can just
	 *   use the same low-order 3 bits for all of our shifts.
	 */
	shift = aCPU.vr[vrB].w[0] & 0x7;

	r.d[0] = aCPU.vr[vrA].d[0] >> shift;
	r.d[1] = aCPU.vr[vrA].d[1] >> shift;

	VECT_D(r, 1) |= VECT_D(aCPU.vr[vrA], 0) << (64 - shift);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vsro		Vector Shift Right Octet
 *	v.258
 */
int ppc_opc_vsro(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	int shift, i;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	shift = (aCPU.vr[vrB].w[0] >> 3) & 0xf;
#if HOST_ENDIANESS == HOST_ENDIANESS_LE
	for (i=0; i<(16-shift); i++) {
		r.b[i] = aCPU.vr[vrA].b[i+shift];
	}

	for (; i<16; i++) {
		r.b[i] = 0;
	}
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
	for (i=0; i<shift; i++) {
		r.b[i] = 0;
	}

	for (; i<16; i++) {
		r.b[i] = aCPU.vr[vrA].b[i-shift];
	}
#else
#error Endianess not supported!
#endif

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vsl		Vector Shift Left
 *	v.239
 */
int ppc_opc_vsl(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	int shift;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	/* Specs say that the low-order 3 bits of all byte elements in vB
	 *   must be the same, or the result is undefined.  So we can just
	 *   use the same low-order 3 bits for all of our shifts.
	 */
	shift = aCPU.vr[vrB].w[0] & 0x7;

	r.d[0] = aCPU.vr[vrA].d[0] << shift;
	r.d[1] = aCPU.vr[vrA].d[1] << shift;

	VECT_D(r, 0) |= VECT_D(aCPU.vr[vrA], 1) >> (64 - shift);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vslo		Vector Shift Left Octet
 *	v.243
 */
int ppc_opc_vslo(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	int shift, i;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	shift = (aCPU.vr[vrB].w[0] >> 3) & 0xf;
#if HOST_ENDIANESS == HOST_ENDIANESS_LE
	for (i=0; i<shift; i++) {
		r.b[i] = 0;
	}

	for (; i<16; i++) {
		r.b[i] = aCPU.vr[vrA].b[i-shift];
	}
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
	for (i=0; i<(16-shift); i++) {
		r.b[i] = aCPU.vr[vrA].b[i+shift];
	}

	for (; i<16; i++) {
		r.b[i] = 0;
	}
#else
#error Endianess not supported!
#endif

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vsldoi		Vector Shift Left Double by Octet Immediate
 *	v.241
 */
int ppc_opc_vsldoi(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrA, vrB, shift, ashift;
	int i;
	Vector_t r;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, shift);

	shift &= 0xf;
	ashift = 16 - shift;

#if HOST_ENDIANESS == HOST_ENDIANESS_LE
	for (i=0; i<shift; i++) {
		r.b[i] = aCPU.vr[vrB].b[i+ashift];
	}

	for (; i<16; i++) {
		r.b[i] = aCPU.vr[vrA].b[i-shift];
	}
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
	for (i=0; i<ashift; i++) {
		r.b[i] = aCPU.vr[vrA].b[i+shift];
	}

	for (; i<16; i++) {
		r.b[i] = aCPU.vr[vrB].b[i-ashift];
	}
#else
#error Endianess not supported!
#endif

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vrlb		Vector Rotate Left Byte
 *	v.234
 */
int ppc_opc_vrlb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, shift;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		shift = (aCPU.vr[vrB].b[i] & 0x7);

		r.b[i] = aCPU.vr[vrA].b[i] << shift;
		r.b[i] |= aCPU.vr[vrA].b[i] >> (8 - shift);
	}

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vrlh		Vector Rotate Left Half Word
 *	v.235
 */
int ppc_opc_vrlh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, shift;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		shift = (aCPU.vr[vrB].h[i] & 0xf);

		r.h[i] = aCPU.vr[vrA].h[i] << shift;
		r.h[i] |= aCPU.vr[vrA].h[i] >> (16 - shift);
	}

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vrlw		Vector Rotate Left Word
 *	v.236
 */
int ppc_opc_vrlw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, shift;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		shift = (aCPU.vr[vrB].w[i] & 0x1F);

		r.w[i] = aCPU.vr[vrA].w[i] << shift;
		r.w[i] |= aCPU.vr[vrA].w[i] >> (32 - shift);
	}

	aCPU.vr[vrD] = r;
	return 0;
}

/* With the merges, I just don't see any point in risking that a compiler
 *   might generate actual alu code to calculate anything when it's
 *   compile-time known.  Plus, it's easier to validate it like this.
 */

/*	vmrghb		Vector Merge High Byte
 *	v.195
 */
int ppc_opc_vmrghb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_B(r, 0) = VECT_B(aCPU.vr[vrA], 0);
	VECT_B(r, 1) = VECT_B(aCPU.vr[vrB], 0);
	VECT_B(r, 2) = VECT_B(aCPU.vr[vrA], 1);
	VECT_B(r, 3) = VECT_B(aCPU.vr[vrB], 1);
	VECT_B(r, 4) = VECT_B(aCPU.vr[vrA], 2);
	VECT_B(r, 5) = VECT_B(aCPU.vr[vrB], 2);
	VECT_B(r, 6) = VECT_B(aCPU.vr[vrA], 3);
	VECT_B(r, 7) = VECT_B(aCPU.vr[vrB], 3);
	VECT_B(r, 8) = VECT_B(aCPU.vr[vrA], 4);
	VECT_B(r, 9) = VECT_B(aCPU.vr[vrB], 4);
	VECT_B(r,10) = VECT_B(aCPU.vr[vrA], 5);
	VECT_B(r,11) = VECT_B(aCPU.vr[vrB], 5);
	VECT_B(r,12) = VECT_B(aCPU.vr[vrA], 6);
	VECT_B(r,13) = VECT_B(aCPU.vr[vrB], 6);
	VECT_B(r,14) = VECT_B(aCPU.vr[vrA], 7);
	VECT_B(r,15) = VECT_B(aCPU.vr[vrB], 7);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vmrghh		Vector Merge High Half Word
 *	v.196
 */
int ppc_opc_vmrghh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = VECT_H(aCPU.vr[vrA], 0);
	VECT_H(r, 1) = VECT_H(aCPU.vr[vrB], 0);
	VECT_H(r, 2) = VECT_H(aCPU.vr[vrA], 1);
	VECT_H(r, 3) = VECT_H(aCPU.vr[vrB], 1);
	VECT_H(r, 4) = VECT_H(aCPU.vr[vrA], 2);
	VECT_H(r, 5) = VECT_H(aCPU.vr[vrB], 2);
	VECT_H(r, 6) = VECT_H(aCPU.vr[vrA], 3);
	VECT_H(r, 7) = VECT_H(aCPU.vr[vrB], 3);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vmrghw		Vector Merge High Word
 *	v.197
 */
int ppc_opc_vmrghw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_W(r, 0) = VECT_W(aCPU.vr[vrA], 0);
	VECT_W(r, 1) = VECT_W(aCPU.vr[vrB], 0);
	VECT_W(r, 2) = VECT_W(aCPU.vr[vrA], 1);
	VECT_W(r, 3) = VECT_W(aCPU.vr[vrB], 1);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vmrglb		Vector Merge Low Byte
 *	v.198
 */
int ppc_opc_vmrglb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_B(r, 0) = VECT_B(aCPU.vr[vrA], 8);
	VECT_B(r, 1) = VECT_B(aCPU.vr[vrB], 8);
	VECT_B(r, 2) = VECT_B(aCPU.vr[vrA], 9);
	VECT_B(r, 3) = VECT_B(aCPU.vr[vrB], 9);
	VECT_B(r, 4) = VECT_B(aCPU.vr[vrA],10);
	VECT_B(r, 5) = VECT_B(aCPU.vr[vrB],10);
	VECT_B(r, 6) = VECT_B(aCPU.vr[vrA],11);
	VECT_B(r, 7) = VECT_B(aCPU.vr[vrB],11);
	VECT_B(r, 8) = VECT_B(aCPU.vr[vrA],12);
	VECT_B(r, 9) = VECT_B(aCPU.vr[vrB],12);
	VECT_B(r,10) = VECT_B(aCPU.vr[vrA],13);
	VECT_B(r,11) = VECT_B(aCPU.vr[vrB],13);
	VECT_B(r,12) = VECT_B(aCPU.vr[vrA],14);
	VECT_B(r,13) = VECT_B(aCPU.vr[vrB],14);
	VECT_B(r,14) = VECT_B(aCPU.vr[vrA],15);
	VECT_B(r,15) = VECT_B(aCPU.vr[vrB],15);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vmrglh		Vector Merge Low Half Word
 *	v.199
 */
int ppc_opc_vmrglh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = VECT_H(aCPU.vr[vrA], 4);
	VECT_H(r, 1) = VECT_H(aCPU.vr[vrB], 4);
	VECT_H(r, 2) = VECT_H(aCPU.vr[vrA], 5);
	VECT_H(r, 3) = VECT_H(aCPU.vr[vrB], 5);
	VECT_H(r, 4) = VECT_H(aCPU.vr[vrA], 6);
	VECT_H(r, 5) = VECT_H(aCPU.vr[vrB], 6);
	VECT_H(r, 6) = VECT_H(aCPU.vr[vrA], 7);
	VECT_H(r, 7) = VECT_H(aCPU.vr[vrB], 7);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vmrglw		Vector Merge Low Word
 *	v.200
 */
int ppc_opc_vmrglw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_W(r, 0) = VECT_W(aCPU.vr[vrA], 2);
	VECT_W(r, 1) = VECT_W(aCPU.vr[vrB], 2);
	VECT_W(r, 2) = VECT_W(aCPU.vr[vrA], 3);
	VECT_W(r, 3) = VECT_W(aCPU.vr[vrB], 3);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vspltb		Vector Splat Byte
 *	v.245
 */
int ppc_opc_vspltb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 uimm;
	uint64 val;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, uimm, vrB);

	/* The documentation doesn't stipulate what a value higher than 0xf
	 *   will do.  Thus, this is by default an undefined value.  We
	 *   are thus doing this the fastest way that won't crash us.
	 */
	val = VECT_B(aCPU.vr[vrB], uimm & 0xf);
	val |= (val << 8);
	val |= (val << 16);
	val |= (val << 32);

	aCPU.vr[vrD].d[0] = val;
	aCPU.vr[vrD].d[1] = val;
	return 0;
}

/*	vsplth		Vector Splat Half Word
 *	v.246
 */
int ppc_opc_vsplth(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 uimm;
	uint64 val;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, uimm, vrB);

	/* The documentation doesn't stipulate what a value higher than 0x7
	 *   will do.  Thus, this is by default an undefined value.  We
	 *   are thus doing this the fastest way that won't crash us.
	 */
	val = VECT_H(aCPU.vr[vrB], uimm & 0x7);
	val |= (val << 16);
	val |= (val << 32);

	aCPU.vr[vrD].d[0] = val;
	aCPU.vr[vrD].d[1] = val;
	return 0;
}

/*	vspltw		Vector Splat Word
 *	v.250
 */
int ppc_opc_vspltw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 uimm;
	uint64 val;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, uimm, vrB);

	/* The documentation doesn't stipulate what a value higher than 0x3
	 *   will do.  Thus, this is by default an undefined value.  We
	 *   are thus doing this the fastest way that won't crash us.
	 */
	val = VECT_W(aCPU.vr[vrB], uimm & 0x3);
	val |= (val << 32);

	aCPU.vr[vrD].d[0] = val;
	aCPU.vr[vrD].d[1] = val;
	return 0;
}

/*	vspltisb	Vector Splat Immediate Signed Byte
 *	v.247
 */
int ppc_opc_vspltisb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrB;
	uint32 simm;
	uint64 val;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, simm, vrB);
	PPC_OPC_ASSERT(vrB==0);

	val = (simm & 0x10) ? (simm | 0xE0) : simm;
	val |= (val << 8);
	val |= (val << 16);
	val |= (val << 32);

	aCPU.vr[vrD].d[0] = val;
	aCPU.vr[vrD].d[1] = val;
	return 0;
}

/*	vspltish	Vector Splat Immediate Signed Half Word
 *	v.248
 */
int ppc_opc_vspltish(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrB;
	uint32 simm;
	uint64 val;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, simm, vrB);
	PPC_OPC_ASSERT(vrB==0);

	val = (simm & 0x10) ? (simm | 0xFFE0) : simm;
	val |= (val << 16);
	val |= (val << 32);

	aCPU.vr[vrD].d[0] = val;
	aCPU.vr[vrD].d[1] = val;
	return 0;
}

/*	vspltisw	Vector Splat Immediate Signed Word
 *	v.249
 */
int ppc_opc_vspltisw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrB;
	uint32 simm;
	uint64 val;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, simm, vrB);
	PPC_OPC_ASSERT(vrB==0);

	val = (simm & 0x10) ? (simm | 0xFFFFFFE0) : simm;
	val |= (val << 32);

	aCPU.vr[vrD].d[0] = val;
	aCPU.vr[vrD].d[1] = val;
	return 0;
}

/*	mfvscr		Move from Vector Status and Control Register
 *	v.129
 */
int ppc_opc_mfvscr(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);
	PPC_OPC_ASSERT(vrB==0);

	VECT_W(aCPU.vr[vrD], 3) = aCPU.vscr;
	VECT_W(aCPU.vr[vrD], 2) = 0;
	VECT_D(aCPU.vr[vrD], 0) = 0;
	return 0;
}

/*	mtvscr		Move to Vector Status and Control Register
 *	v.130
 */
int ppc_opc_mtvscr(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);
	PPC_OPC_ASSERT(vrD==0);

	aCPU.vscr = VECT_W(aCPU.vr[vrB], 3);
	return 0;
}

/*	vpkuhum		Vector Pack Unsigned Half Word Unsigned Modulo
 *	v.224
 */
int ppc_opc_vpkuhum(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_B(r, 0) = VECT_B(aCPU.vr[vrA], 1);
	VECT_B(r, 1) = VECT_B(aCPU.vr[vrA], 3);
	VECT_B(r, 2) = VECT_B(aCPU.vr[vrA], 5);
	VECT_B(r, 3) = VECT_B(aCPU.vr[vrA], 7);
	VECT_B(r, 4) = VECT_B(aCPU.vr[vrA], 9);
	VECT_B(r, 5) = VECT_B(aCPU.vr[vrA],11);
	VECT_B(r, 6) = VECT_B(aCPU.vr[vrA],13);
	VECT_B(r, 7) = VECT_B(aCPU.vr[vrA],15);

	VECT_B(r, 8) = VECT_B(aCPU.vr[vrB], 1);
	VECT_B(r, 9) = VECT_B(aCPU.vr[vrB], 3);
	VECT_B(r,10) = VECT_B(aCPU.vr[vrB], 5);
	VECT_B(r,11) = VECT_B(aCPU.vr[vrB], 7);
	VECT_B(r,12) = VECT_B(aCPU.vr[vrB], 9);
	VECT_B(r,13) = VECT_B(aCPU.vr[vrB],11);
	VECT_B(r,14) = VECT_B(aCPU.vr[vrB],13);
	VECT_B(r,15) = VECT_B(aCPU.vr[vrB],15);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vpkuwum		Vector Pack Unsigned Word Unsigned Modulo
 *	v.226
 */
int ppc_opc_vpkuwum(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = VECT_H(aCPU.vr[vrA], 1);
	VECT_H(r, 1) = VECT_H(aCPU.vr[vrA], 3);
	VECT_H(r, 2) = VECT_H(aCPU.vr[vrA], 5);
	VECT_H(r, 3) = VECT_H(aCPU.vr[vrA], 7);

	VECT_H(r, 4) = VECT_H(aCPU.vr[vrB], 1);
	VECT_H(r, 5) = VECT_H(aCPU.vr[vrB], 3);
	VECT_H(r, 6) = VECT_H(aCPU.vr[vrB], 5);
	VECT_H(r, 7) = VECT_H(aCPU.vr[vrB], 7);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vpkpx		Vector Pack Pixel32
 *	v.219
 */
int ppc_opc_vpkpx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = PACK_PIXEL(VECT_W(aCPU.vr[vrA], 0));
	VECT_H(r, 1) = PACK_PIXEL(VECT_W(aCPU.vr[vrA], 1));
	VECT_H(r, 2) = PACK_PIXEL(VECT_W(aCPU.vr[vrA], 2));
	VECT_H(r, 3) = PACK_PIXEL(VECT_W(aCPU.vr[vrA], 3));

	VECT_H(r, 4) = PACK_PIXEL(VECT_W(aCPU.vr[vrB], 0));
	VECT_H(r, 5) = PACK_PIXEL(VECT_W(aCPU.vr[vrB], 1));
	VECT_H(r, 6) = PACK_PIXEL(VECT_W(aCPU.vr[vrB], 2));
	VECT_H(r, 7) = PACK_PIXEL(VECT_W(aCPU.vr[vrB], 3));

	aCPU.vr[vrD] = r;
	return 0;
}


/*	vpkuhus		Vector Pack Unsigned Half Word Unsigned Saturate
 *	v.225
 */
int ppc_opc_vpkuhus(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_B(r, 0) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrA], 0));
	VECT_B(r, 1) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrA], 1));
	VECT_B(r, 2) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrA], 2));
	VECT_B(r, 3) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrA], 3));
	VECT_B(r, 4) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrA], 4));
	VECT_B(r, 5) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrA], 5));
	VECT_B(r, 6) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrA], 6));
	VECT_B(r, 7) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrA], 7));

	VECT_B(r, 8) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrB], 0));
	VECT_B(r, 9) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrB], 1));
	VECT_B(r,10) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrB], 2));
	VECT_B(r,11) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrB], 3));
	VECT_B(r,12) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrB], 4));
	VECT_B(r,13) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrB], 5));
	VECT_B(r,14) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrB], 6));
	VECT_B(r,15) = SATURATE_UB(aCPU, VECT_H(aCPU.vr[vrB], 7));

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vpkshss		Vector Pack Signed Half Word Signed Saturate
 *	v.220
 */
int ppc_opc_vpkshss(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_B(r, 0) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrA], 0));
	VECT_B(r, 1) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrA], 1));
	VECT_B(r, 2) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrA], 2));
	VECT_B(r, 3) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrA], 3));
	VECT_B(r, 4) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrA], 4));
	VECT_B(r, 5) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrA], 5));
	VECT_B(r, 6) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrA], 6));
	VECT_B(r, 7) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrA], 7));

	VECT_B(r, 8) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrB], 0));
	VECT_B(r, 9) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrB], 1));
	VECT_B(r,10) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrB], 2));
	VECT_B(r,11) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrB], 3));
	VECT_B(r,12) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrB], 4));
	VECT_B(r,13) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrB], 5));
	VECT_B(r,14) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrB], 6));
	VECT_B(r,15) = SATURATE_SB(aCPU, VECT_H(aCPU.vr[vrB], 7));

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vpkuwus		Vector Pack Unsigned Word Unsigned Saturate
 *	v.227
 */
int ppc_opc_vpkuwus(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = SATURATE_UH(aCPU, VECT_W(aCPU.vr[vrA], 0));
	VECT_H(r, 1) = SATURATE_UH(aCPU, VECT_W(aCPU.vr[vrA], 1));
	VECT_H(r, 2) = SATURATE_UH(aCPU, VECT_W(aCPU.vr[vrA], 2));
	VECT_H(r, 3) = SATURATE_UH(aCPU, VECT_W(aCPU.vr[vrA], 3));

	VECT_H(r, 4) = SATURATE_UH(aCPU, VECT_W(aCPU.vr[vrB], 0));
	VECT_H(r, 5) = SATURATE_UH(aCPU, VECT_W(aCPU.vr[vrB], 1));
	VECT_H(r, 6) = SATURATE_UH(aCPU, VECT_W(aCPU.vr[vrB], 2));
	VECT_H(r, 7) = SATURATE_UH(aCPU, VECT_W(aCPU.vr[vrB], 3));

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vpkswss		Vector Pack Signed Word Signed Saturate
 *	v.222
 */
int ppc_opc_vpkswss(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = SATURATE_SH(aCPU, VECT_W(aCPU.vr[vrA], 0));
	VECT_H(r, 1) = SATURATE_SH(aCPU, VECT_W(aCPU.vr[vrA], 1));
	VECT_H(r, 2) = SATURATE_SH(aCPU, VECT_W(aCPU.vr[vrA], 2));
	VECT_H(r, 3) = SATURATE_SH(aCPU, VECT_W(aCPU.vr[vrA], 3));

	VECT_H(r, 4) = SATURATE_SH(aCPU, VECT_W(aCPU.vr[vrB], 0));
	VECT_H(r, 5) = SATURATE_SH(aCPU, VECT_W(aCPU.vr[vrB], 1));
	VECT_H(r, 6) = SATURATE_SH(aCPU, VECT_W(aCPU.vr[vrB], 2));
	VECT_H(r, 7) = SATURATE_SH(aCPU, VECT_W(aCPU.vr[vrB], 3));

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vpkshus		Vector Pack Signed Half Word Unsigned Saturate
 *	v.221
 */
int ppc_opc_vpkshus(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_B(r, 0) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrA], 0));
	VECT_B(r, 1) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrA], 1));
	VECT_B(r, 2) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrA], 2));
	VECT_B(r, 3) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrA], 3));
	VECT_B(r, 4) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrA], 4));
	VECT_B(r, 5) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrA], 5));
	VECT_B(r, 6) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrA], 6));
	VECT_B(r, 7) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrA], 7));

	VECT_B(r, 8) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrB], 0));
	VECT_B(r, 9) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrB], 1));
	VECT_B(r,10) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrB], 2));
	VECT_B(r,11) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrB], 3));
	VECT_B(r,12) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrB], 4));
	VECT_B(r,13) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrB], 5));
	VECT_B(r,14) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrB], 6));
	VECT_B(r,15) = SATURATE_USB(aCPU, VECT_H(aCPU.vr[vrB], 7));

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vpkswus		Vector Pack Signed Word Unsigned Saturate
 *	v.223
 */
int ppc_opc_vpkswus(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = SATURATE_USH(aCPU, VECT_W(aCPU.vr[vrA], 0));
	VECT_H(r, 1) = SATURATE_USH(aCPU, VECT_W(aCPU.vr[vrA], 1));
	VECT_H(r, 2) = SATURATE_USH(aCPU, VECT_W(aCPU.vr[vrA], 2));
	VECT_H(r, 3) = SATURATE_USH(aCPU, VECT_W(aCPU.vr[vrA], 3));

	VECT_H(r, 4) = SATURATE_USH(aCPU, VECT_W(aCPU.vr[vrB], 0));
	VECT_H(r, 5) = SATURATE_USH(aCPU, VECT_W(aCPU.vr[vrB], 1));
	VECT_H(r, 6) = SATURATE_USH(aCPU, VECT_W(aCPU.vr[vrB], 2));
	VECT_H(r, 7) = SATURATE_USH(aCPU, VECT_W(aCPU.vr[vrB], 3));

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vupkhsb		Vector Unpack High Signed Byte
 *	v.277
 */
int ppc_opc_vupkhsb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	VECT_SH(r, 0) = VECT_SB(aCPU.vr[vrB], 0);
	VECT_SH(r, 1) = VECT_SB(aCPU.vr[vrB], 1);
	VECT_SH(r, 2) = VECT_SB(aCPU.vr[vrB], 2);
	VECT_SH(r, 3) = VECT_SB(aCPU.vr[vrB], 3);
	VECT_SH(r, 4) = VECT_SB(aCPU.vr[vrB], 4);
	VECT_SH(r, 5) = VECT_SB(aCPU.vr[vrB], 5);
	VECT_SH(r, 6) = VECT_SB(aCPU.vr[vrB], 6);
	VECT_SH(r, 7) = VECT_SB(aCPU.vr[vrB], 7);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vupkhpx		Vector Unpack High Pixel32
 *	v.279
 */
int ppc_opc_vupkhpx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	VECT_W(r, 0) = UNPACK_PIXEL(VECT_H(aCPU.vr[vrB], 0));
	VECT_W(r, 1) = UNPACK_PIXEL(VECT_H(aCPU.vr[vrB], 1));
	VECT_W(r, 2) = UNPACK_PIXEL(VECT_H(aCPU.vr[vrB], 2));
	VECT_W(r, 3) = UNPACK_PIXEL(VECT_H(aCPU.vr[vrB], 3));
	
	aCPU.vr[vrD] = r;
	return 0;
}

/*	vupkhsh		Vector Unpack High Signed Half Word
 *	v.278
 */
int ppc_opc_vupkhsh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	VECT_SW(r, 0) = VECT_SH(aCPU.vr[vrB], 0);
	VECT_SW(r, 1) = VECT_SH(aCPU.vr[vrB], 1);
	VECT_SW(r, 2) = VECT_SH(aCPU.vr[vrB], 2);
	VECT_SW(r, 3) = VECT_SH(aCPU.vr[vrB], 3);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vupklsb		Vector Unpack Low Signed Byte
 *	v.280
 */
int ppc_opc_vupklsb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	VECT_SH(r, 0) = VECT_SB(aCPU.vr[vrB], 8);
	VECT_SH(r, 1) = VECT_SB(aCPU.vr[vrB], 9);
	VECT_SH(r, 2) = VECT_SB(aCPU.vr[vrB],10);
	VECT_SH(r, 3) = VECT_SB(aCPU.vr[vrB],11);
	VECT_SH(r, 4) = VECT_SB(aCPU.vr[vrB],12);
	VECT_SH(r, 5) = VECT_SB(aCPU.vr[vrB],13);
	VECT_SH(r, 6) = VECT_SB(aCPU.vr[vrB],14);
	VECT_SH(r, 7) = VECT_SB(aCPU.vr[vrB],15);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vupklpx		Vector Unpack Low Pixel32
 *	v.279
 */
int ppc_opc_vupklpx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	VECT_W(r, 0) = UNPACK_PIXEL(VECT_H(aCPU.vr[vrB], 4));
	VECT_W(r, 1) = UNPACK_PIXEL(VECT_H(aCPU.vr[vrB], 5));
	VECT_W(r, 2) = UNPACK_PIXEL(VECT_H(aCPU.vr[vrB], 6));
	VECT_W(r, 3) = UNPACK_PIXEL(VECT_H(aCPU.vr[vrB], 7));

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vupklsh		Vector Unpack Low Signed Half Word
 *	v.281
 */
int ppc_opc_vupklsh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	VECT_SW(r, 0) = VECT_SH(aCPU.vr[vrB], 4);
	VECT_SW(r, 1) = VECT_SH(aCPU.vr[vrB], 5);
	VECT_SW(r, 2) = VECT_SH(aCPU.vr[vrB], 6);
	VECT_SW(r, 3) = VECT_SH(aCPU.vr[vrB], 7);

	aCPU.vr[vrD] = r;
	return 0;
}

/*	vaddubm		Vector Add Unsigned Byte Modulo
 *	v.141
 */
int ppc_opc_vaddubm(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint8 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = aCPU.vr[vrA].b[i] + aCPU.vr[vrB].b[i];
		aCPU.vr[vrD].b[i] = res;
	}
	return 0;
}

/*	vadduhm		Vector Add Unsigned Half Word Modulo
 *	v.143
 */
int ppc_opc_vadduhm(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = aCPU.vr[vrA].h[i] + aCPU.vr[vrB].h[i];
		aCPU.vr[vrD].h[i] = res;
	}
	return 0;
}

/*	vadduwm		Vector Add Unsigned Word Modulo
 *	v.145
 */
int ppc_opc_vadduwm(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = aCPU.vr[vrA].w[i] + aCPU.vr[vrB].w[i];
		aCPU.vr[vrD].w[i] = res;
	}
	return 0;
}

/*	vaddfp		Vector Add Float Point
 *	v.137
 */
int ppc_opc_vaddfp(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	float res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		res = aCPU.vr[vrA].f[i] + aCPU.vr[vrB].f[i];
		aCPU.vr[vrD].f[i] = res;
	}
	return 0;
}

/*	vaddcuw		Vector Add Carryout Unsigned Word
 *	v.136
 */
int ppc_opc_vaddcuw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = aCPU.vr[vrA].w[i] + aCPU.vr[vrB].w[i];
		aCPU.vr[vrD].w[i] = (res < aCPU.vr[vrA].w[i]) ? 1 : 0;
	}
	return 0;
}

/*	vaddubs		Vector Add Unsigned Byte Saturate
 *	v.142
 */
int ppc_opc_vaddubs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = (uint16)aCPU.vr[vrA].b[i] + (uint16)aCPU.vr[vrB].b[i];
		aCPU.vr[vrD].b[i] = SATURATE_UB(aCPU, res);
	}
	return 0;
}

/*	vaddsbs		Vector Add Signed Byte Saturate
 *	v.138
 */
int ppc_opc_vaddsbs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = (sint16)aCPU.vr[vrA].sb[i] + (sint16)aCPU.vr[vrB].sb[i];
		aCPU.vr[vrD].b[i] = SATURATE_SB(aCPU, res);
	}
	return 0;
}

/*	vadduhs		Vector Add Unsigned Half Word Saturate
 *	v.144
 */
int ppc_opc_vadduhs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (uint32)aCPU.vr[vrA].h[i] + (uint32)aCPU.vr[vrB].h[i];
		aCPU.vr[vrD].h[i] = SATURATE_UH(aCPU, res);
	}
	return 0;
}

/*	vaddshs		Vector Add Signed Half Word Saturate
 *	v.139
 */
int ppc_opc_vaddshs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (sint32)aCPU.vr[vrA].sh[i] + (sint32)aCPU.vr[vrB].sh[i];
		aCPU.vr[vrD].h[i] = SATURATE_SH(aCPU, res);
	}
	return 0;
}

/*	vadduws		Vector Add Unsigned Word Saturate
 *	v.146
 */
int ppc_opc_vadduws(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = aCPU.vr[vrA].w[i] + aCPU.vr[vrB].w[i];

		// We do this to prevent us from having to do 64-bit math
		if (res < aCPU.vr[vrA].w[i]) {
			res = 0xFFFFFFFF;
			aCPU.vscr |= VSCR_SAT;
		}

	/*	64-bit math		|	32-bit hack
	 *	------------------------+-------------------------------------
	 *	add, addc	(a+b)	|	add		(a+b)
	 *	sub, subb 	(r>ub)	|	sub		(r<a)
	 */

		aCPU.vr[vrD].w[i] = res;
	}
	return 0;
}

/*	vaddsws		Vector Add Signed Word Saturate
 *	v.140
 */
int ppc_opc_vaddsws(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = aCPU.vr[vrA].w[i] + aCPU.vr[vrB].w[i];

		// We do this to prevent us from having to do 64-bit math
		if (((aCPU.vr[vrA].w[i] ^ aCPU.vr[vrB].w[i]) & SIGN32) == 0) {
			// the signs of both operands are the same

			if (((res ^ aCPU.vr[vrA].w[i]) & SIGN32) != 0) {
				// sign of result != sign of operands

				// if res is negative, should have been positive
				res = (res & SIGN32) ? (SIGN32 - 1) : SIGN32;
				aCPU.vscr |= VSCR_SAT;
			}
		}

	/*	64-bit math		|	32-bit hack
	 *	------------------------+-------------------------------------
	 *	add, addc	(a+b)	|	add		(a+b)
	 *	sub, subb 	(r>ub)	|	xor, and	(sign == sign)
	 *	sub, subb	(r<lb)	|	xor, and	(sign != sign)
	 *				|	and		(which)
	 */

		aCPU.vr[vrD].w[i] = res;
	}
	return 0;
}

/*	vsububm		Vector Subtract Unsigned Byte Modulo
 *	v.265
 */
int ppc_opc_vsububm(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint8 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = aCPU.vr[vrA].b[i] - aCPU.vr[vrB].b[i];
		aCPU.vr[vrD].b[i] = res;
	}
	return 0;
}

/*	vsubuhm		Vector Subtract Unsigned Half Word Modulo
 *	v.267
 */
int ppc_opc_vsubuhm(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = aCPU.vr[vrA].h[i] - aCPU.vr[vrB].h[i];
		aCPU.vr[vrD].h[i] = res;
	}
	return 0;
}

/*	vsubuwm		Vector Subtract Unsigned Word Modulo
 *	v.269
 */
int ppc_opc_vsubuwm(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = aCPU.vr[vrA].w[i] - aCPU.vr[vrB].w[i];
		aCPU.vr[vrD].w[i] = res;
	}
	return 0;
}

/*	vsubfp		Vector Subtract Float Point
 *	v.261
 */
int ppc_opc_vsubfp(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	float res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		res = aCPU.vr[vrA].f[i] - aCPU.vr[vrB].f[i];
		aCPU.vr[vrD].f[i] = res;
	}
	return 0;
}

/*	vsubcuw		Vector Subtract Carryout Unsigned Word
 *	v.260
 */
int ppc_opc_vsubcuw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = aCPU.vr[vrA].w[i] - aCPU.vr[vrB].w[i];
		aCPU.vr[vrD].w[i] = (res <= aCPU.vr[vrA].w[i]) ? 1 : 0;
	}
	return 0;
}

/*	vsububs		Vector Subtract Unsigned Byte Saturate
 *	v.266
 */
int ppc_opc_vsububs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = (uint16)aCPU.vr[vrA].b[i] - (uint16)aCPU.vr[vrB].b[i];

		aCPU.vr[vrD].b[i] = SATURATE_0B(aCPU, res);
	}
	return 0;
}

/*	vsubsbs		Vector Subtract Signed Byte Saturate
 *	v.262
 */
int ppc_opc_vsubsbs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = (sint16)aCPU.vr[vrA].sb[i] - (sint16)aCPU.vr[vrB].sb[i];

		aCPU.vr[vrD].sb[i] = SATURATE_SB(aCPU, res);
	}
	return 0;
}

/*	vsubuhs		Vector Subtract Unsigned Half Word Saturate
 *	v.268
 */
int ppc_opc_vsubuhs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (uint32)aCPU.vr[vrA].h[i] - (uint32)aCPU.vr[vrB].h[i];

		aCPU.vr[vrD].h[i] = SATURATE_0H(aCPU, res);
	}
	return 0;
}

/*	vsubshs		Vector Subtract Signed Half Word Saturate
 *	v.263
 */
int ppc_opc_vsubshs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (sint32)aCPU.vr[vrA].sh[i] - (sint32)aCPU.vr[vrB].sh[i];

		aCPU.vr[vrD].sh[i] = SATURATE_SH(aCPU, res);
	}
	return 0;
}

/*	vsubuws		Vector Subtract Unsigned Word Saturate
 *	v.270
 */
int ppc_opc_vsubuws(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = aCPU.vr[vrA].w[i] - aCPU.vr[vrB].w[i];

		// We do this to prevent us from having to do 64-bit math
		if (res > aCPU.vr[vrA].w[i]) {
			res = 0;
			aCPU.vscr |= VSCR_SAT;
		}

	/*	64-bit math		|	32-bit hack
	 *	------------------------+-------------------------------------
	 *	sub, subb	(a+b)	|	sub		(a+b)
	 *	sub, subb 	(r>ub)	|	sub		(r<a)
	 */

		aCPU.vr[vrD].w[i] = res;
	}
	return 0;
}

/*	vsubsws		Vector Subtract Signed Word Saturate
 *	v.264
 */
int ppc_opc_vsubsws(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res, tmp;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		tmp = -aCPU.vr[vrB].w[i];
		res = aCPU.vr[vrA].w[i] + tmp;

		// We do this to prevent us from having to do 64-bit math
		if (((aCPU.vr[vrA].w[i] ^ tmp) & SIGN32) == 0) {
			// the signs of both operands are the same

			if (((res ^ tmp) & SIGN32) != 0) {
				// sign of result != sign of operands

				// if res is negative, should have been positive
				res = (res & SIGN32) ? (SIGN32 - 1) : SIGN32;
				aCPU.vscr |= VSCR_SAT;
			}
		}

	/*	64-bit math		|	32-bit hack
	 *	------------------------+-------------------------------------
	 *	sub, subc	(a+b)	|	neg, add	(a-b)
	 *	sub, subb 	(r>ub)	|	xor, and	(sign == sign)
	 *	sub, subb	(r<lb)	|	xor, and	(sign != sign)
	 *				|	and		(which)
	 */

		aCPU.vr[vrD].w[i] = res;
	}
	return 0;
}

/*	vmuleub		Vector Multiply Even Unsigned Byte
 *	v.209
 */
int ppc_opc_vmuleub(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (uint16)aCPU.vr[vrA].b[VECT_EVEN(i)] *
			 (uint16)aCPU.vr[vrB].b[VECT_EVEN(i)];

		aCPU.vr[vrD].h[i] = res;
	}
	return 0;
}

/*	vmulesb		Vector Multiply Even Signed Byte
 *	v.207
 */
int ppc_opc_vmulesb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (sint16)aCPU.vr[vrA].sb[VECT_EVEN(i)] *
			 (sint16)aCPU.vr[vrB].sb[VECT_EVEN(i)];

		aCPU.vr[vrD].sh[i] = res;
	}
	return 0;
}

/*	vmuleuh		Vector Multiply Even Unsigned Half Word
 *	v.210
 */
int ppc_opc_vmuleuh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (uint32)aCPU.vr[vrA].h[VECT_EVEN(i)] *
			 (uint32)aCPU.vr[vrB].h[VECT_EVEN(i)];

		aCPU.vr[vrD].w[i] = res;
	}
	return 0;
}

/*	vmulesh		Vector Multiply Even Signed Half Word
 *	v.208
 */
int ppc_opc_vmulesh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (sint32)aCPU.vr[vrA].sh[VECT_EVEN(i)] *
			 (sint32)aCPU.vr[vrB].sh[VECT_EVEN(i)];

		aCPU.vr[vrD].sw[i] = res;
	}
	return 0;
}

/*	vmuloub		Vector Multiply Odd Unsigned Byte
 *	v.213
 */
int ppc_opc_vmuloub(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (uint16)aCPU.vr[vrA].b[VECT_ODD(i)] *
			 (uint16)aCPU.vr[vrB].b[VECT_ODD(i)];

		aCPU.vr[vrD].h[i] = res;
	}
	return 0;
}

/*	vmulosb		Vector Multiply Odd Signed Byte
 *	v.211
 */
int ppc_opc_vmulosb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (sint16)aCPU.vr[vrA].sb[VECT_ODD(i)] *
			 (sint16)aCPU.vr[vrB].sb[VECT_ODD(i)];

		aCPU.vr[vrD].sh[i] = res;
	}
	return 0;
}

/*	vmulouh		Vector Multiply Odd Unsigned Half Word
 *	v.214
 */
int ppc_opc_vmulouh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (uint32)aCPU.vr[vrA].h[VECT_ODD(i)] *
			 (uint32)aCPU.vr[vrB].h[VECT_ODD(i)];

		aCPU.vr[vrD].w[i] = res;
	}
	return 0;
}

/*	vmulosh		Vector Multiply Odd Signed Half Word
 *	v.212
 */
int ppc_opc_vmulosh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (sint32)aCPU.vr[vrA].sh[VECT_ODD(i)] *
			 (sint32)aCPU.vr[vrB].sh[VECT_ODD(i)];

		aCPU.vr[vrD].sw[i] = res;
	}
	return 0;
}

/*	vmaddfp		Vector Multiply Add Floating Point
 *	v.177
 */
int ppc_opc_vmaddfp(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	double res;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		res = (double)aCPU.vr[vrA].f[i] * (double)aCPU.vr[vrC].f[i];

		res = (double)aCPU.vr[vrB].f[i] + res;

		aCPU.vr[vrD].f[i] = (float)res;
	}
	return 0;
}

/*	vmhaddshs	Vector Multiply High and Add Signed Half Word Saturate
 *	v.185
 */
int ppc_opc_vmhaddshs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	sint32 prod;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<8; i++) {
		prod = (sint32)aCPU.vr[vrA].sh[i] * (sint32)aCPU.vr[vrB].sh[i];

		prod = (prod >> 15) + (sint32)aCPU.vr[vrC].sh[i];

		aCPU.vr[vrD].sh[i] = SATURATE_SH(aCPU, prod);
	}
	return 0;
}

/*	vmladduhm	Vector Multiply Low and Add Unsigned Half Word Modulo
 *	v.194
 */
int ppc_opc_vmladduhm(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	uint32 prod;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<8; i++) {
		prod = (uint32)aCPU.vr[vrA].h[i] * (uint32)aCPU.vr[vrB].h[i];

		prod = prod + (uint32)aCPU.vr[vrC].h[i];

		aCPU.vr[vrD].h[i] = prod;
	}
	return 0;
}

/*	vmhraddshs	Vector Multiply High Round and Add Signed Half Word Saturate
 *	v.186
 */
int ppc_opc_vmhraddshs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	sint32 prod;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<8; i++) {
		prod = (sint32)aCPU.vr[vrA].sh[i] * (sint32)aCPU.vr[vrB].sh[i];

		prod += 0x4000;
		prod = (prod >> 15) + (sint32)aCPU.vr[vrC].sh[i];

		aCPU.vr[vrD].sh[i] = SATURATE_SH(aCPU, prod);
	}
	return 0;
}

/*	vmsumubm	Vector Multiply Sum Unsigned Byte Modulo
 *	v.204
 */
int ppc_opc_vmsumubm(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	uint32 temp;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<4; i++) {
		temp = aCPU.vr[vrC].w[i];

		temp += (uint16)aCPU.vr[vrA].b[i<<2] *
			(uint16)aCPU.vr[vrB].b[i<<2];

		temp += (uint16)aCPU.vr[vrA].b[(i<<2)+1] *
			(uint16)aCPU.vr[vrB].b[(i<<2)+1];

		temp += (uint16)aCPU.vr[vrA].b[(i<<2)+2] *
			(uint16)aCPU.vr[vrB].b[(i<<2)+2];

		temp += (uint16)aCPU.vr[vrA].b[(i<<2)+3] *
			(uint16)aCPU.vr[vrB].b[(i<<2)+3];

		aCPU.vr[vrD].w[i] = temp;
	}
	return 0;
}

/*	vmsumuhm	Vector Multiply Sum Unsigned Half Word Modulo
 *	v.205
 */
int ppc_opc_vmsumuhm(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	uint32 temp;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<4; i++) {
		temp = aCPU.vr[vrC].w[i];

		temp += (uint32)aCPU.vr[vrA].h[i<<1] *
			(uint32)aCPU.vr[vrB].h[i<<1];
		temp += (uint32)aCPU.vr[vrA].h[(i<<1)+1] *
			(uint32)aCPU.vr[vrB].h[(i<<1)+1];

		aCPU.vr[vrD].w[i] = temp;
	}
	return 0;
}

/*	vmsummbm	Vector Multiply Sum Mixed-Sign Byte Modulo
 *	v.201
 */
int ppc_opc_vmsummbm(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	sint32 temp;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<4; i++) {
		temp = aCPU.vr[vrC].sw[i];

		temp += (sint16)aCPU.vr[vrA].sb[i<<2] *
			(uint16)aCPU.vr[vrB].b[i<<2];
		temp += (sint16)aCPU.vr[vrA].sb[(i<<2)+1] *
			(uint16)aCPU.vr[vrB].b[(i<<2)+1];
		temp += (sint16)aCPU.vr[vrA].sb[(i<<2)+2] *
			(uint16)aCPU.vr[vrB].b[(i<<2)+2];
		temp += (sint16)aCPU.vr[vrA].sb[(i<<2)+3] *
			(uint16)aCPU.vr[vrB].b[(i<<2)+3];

		aCPU.vr[vrD].sw[i] = temp;
	}
	return 0;
}

/*	vmsumshm	Vector Multiply Sum Signed Half Word Modulo
 *	v.202
 */
int ppc_opc_vmsumshm(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	sint32 temp;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<4; i++) {
		temp = aCPU.vr[vrC].sw[i];

		temp += (sint32)aCPU.vr[vrA].sh[i<<1] *
			(sint32)aCPU.vr[vrB].sh[i<<1];
		temp += (sint32)aCPU.vr[vrA].sh[(i<<1)+1] *
			(sint32)aCPU.vr[vrB].sh[(i<<1)+1];

		aCPU.vr[vrD].sw[i] = temp;
	}
	return 0;
}

/*	vmsumuhs	Vector Multiply Sum Unsigned Half Word Saturate
 *	v.206
 */
int ppc_opc_vmsumuhs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	uint64 temp;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, vrC);

	/* For this, there's no way to get around 64-bit math.  If we use
	 *   the hacks used before, then we have to do it so often, that
	 *   we'll outpace the 64-bit math in execution time.
	 */
	for (int i=0; i<4; i++) {
		temp = aCPU.vr[vrC].w[i];

		temp += (uint32)aCPU.vr[vrA].h[i<<1] *
			(uint32)aCPU.vr[vrB].h[i<<1];

		temp += (uint32)aCPU.vr[vrA].h[(i<<1)+1] *
			(uint32)aCPU.vr[vrB].h[(i<<1)+1];

		aCPU.vr[vrD].w[i] = SATURATE_UW(aCPU, temp);
	}
	return 0;
}

/*	vmsumshs	Vector Multiply Sum Signed Half Word Saturate
 *	v.203
 */
int ppc_opc_vmsumshs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	sint64 temp;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, vrC);

	/* For this, there's no way to get around 64-bit math.  If we use
	 *   the hacks used before, then we have to do it so often, that
	 *   we'll outpace the 64-bit math in execution time.
	 */

	for (int i=0; i<4; i++) {
		temp = aCPU.vr[vrC].sw[i];

		temp += (sint32)aCPU.vr[vrA].sh[i<<1] *
			(sint32)aCPU.vr[vrB].sh[i<<1];
		temp += (sint32)aCPU.vr[vrA].sh[(i<<1)+1] *
			(sint32)aCPU.vr[vrB].sh[(i<<1)+1];

		aCPU.vr[vrD].sw[i] = SATURATE_SW(aCPU, temp);
	}
	return 0;
}

/*	vsum4ubs	Vector Sum Across Partial (1/4) Unsigned Byte Saturate
 *	v.275
 */
int ppc_opc_vsum4ubs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint64 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	/* For this, there's no way to get around 64-bit math.  If we use
	 *   the hacks used before, then we have to do it so often, that
	 *   we'll outpace the 64-bit math in execution time.
	 */

	for (int i=0; i<4; i++) {
		res = (uint64)aCPU.vr[vrB].w[i];

		res += (uint64)aCPU.vr[vrA].b[(i<<2)];
		res += (uint64)aCPU.vr[vrA].b[(i<<2)+1];
		res += (uint64)aCPU.vr[vrA].b[(i<<2)+2];
		res += (uint64)aCPU.vr[vrA].b[(i<<2)+3];

		aCPU.vr[vrD].w[i] = SATURATE_UW(aCPU, res);
	}
	return 0;
}

/*	vsum4sbs	Vector Sum Across Partial (1/4) Signed Byte Saturate
 *	v.273
 */
int ppc_opc_vsum4sbs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint64 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (sint64)aCPU.vr[vrB].sw[i];

		res += (sint64)aCPU.vr[vrA].sb[(i<<2)];
		res += (sint64)aCPU.vr[vrA].sb[(i<<2)+1];
		res += (sint64)aCPU.vr[vrA].sb[(i<<2)+2];
		res += (sint64)aCPU.vr[vrA].sb[(i<<2)+3];

		aCPU.vr[vrD].sw[i] = SATURATE_SW(aCPU, res);
	}
	return 0;
}

/*	vsum4shs	Vector Sum Across Partial (1/4) Signed Half Word Saturate
 *	v.274
 */
int ppc_opc_vsum4shs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint64 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (sint64)aCPU.vr[vrB].sw[i];

		res += (sint64)aCPU.vr[vrA].sh[(i<<1)];
		res += (sint64)aCPU.vr[vrA].sh[(i<<1)+1];

		aCPU.vr[vrD].sw[i] = SATURATE_SW(aCPU, res);
	}
	return 0;
}

/*	vsum2sws	Vector Sum Across Partial (1/2) Signed Word Saturate
 *	v.272
 */
int ppc_opc_vsum2sws(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint64 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	res = (sint64)aCPU.vr[vrA].sw[0] + (sint64)aCPU.vr[vrA].sw[1];
	res += (sint64)aCPU.vr[vrB].sw[VECT_ODD(0)];

	aCPU.vr[vrD].w[VECT_ODD(0)] = SATURATE_SW(aCPU, res);
	aCPU.vr[vrD].w[VECT_EVEN(0)] = 0;

	res = (sint64)aCPU.vr[vrA].sw[2] + (sint64)aCPU.vr[vrA].sw[3];
	res += (sint64)aCPU.vr[vrB].sw[VECT_ODD(1)];

	aCPU.vr[vrD].w[VECT_ODD(1)] = SATURATE_SW(aCPU, res);
	aCPU.vr[vrD].w[VECT_EVEN(1)] = 0;
	return 0;
}

/*	vsumsws		Vector Sum Across Signed Word Saturate
 *	v.271
 */
int ppc_opc_vsumsws(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint64 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	res = (sint64)aCPU.vr[vrA].sw[0] + (sint64)aCPU.vr[vrA].sw[1];
	res += (sint64)aCPU.vr[vrA].sw[2] + (sint64)aCPU.vr[vrA].sw[3];

	res += (sint64)VECT_W(aCPU.vr[vrB], 3);

	VECT_W(aCPU.vr[vrD], 3) = SATURATE_SW(aCPU, res);
	VECT_W(aCPU.vr[vrD], 2) = 0;
	VECT_W(aCPU.vr[vrD], 1) = 0;
	VECT_W(aCPU.vr[vrD], 0) = 0;
	return 0;
}

/*	vnmsubfp	Vector Negative Multiply-Subtract Floating Point
 *	v.215
 */
int ppc_opc_vnmsubfp(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	double res;
	PPC_OPC_TEMPL_A(aCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		res = (double)aCPU.vr[vrA].f[i] * (double)aCPU.vr[vrC].f[i];

		res = (double)aCPU.vr[vrB].f[i] - res;

		aCPU.vr[vrD].f[i] = (float)res;
	}
	return 0;
}

/*	vavgub		Vector Average Unsigned Byte
 *	v.152
 */
int ppc_opc_vavgub(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = (uint16)aCPU.vr[vrA].b[i] +
			(uint16)aCPU.vr[vrB].b[i] + 1;

		aCPU.vr[vrD].b[i] = (res >> 1);
	}
	return 0;
}

/*	vavguh		Vector Average Unsigned Half Word
 *	v.153
 */
int ppc_opc_vavguh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (uint32)aCPU.vr[vrA].h[i] +
			(uint32)aCPU.vr[vrB].h[i] + 1;

		aCPU.vr[vrD].h[i] = (res >> 1);
	}
	return 0;
}

/*	vavguw		Vector Average Unsigned Word
 *	v.154
 */
int ppc_opc_vavguw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint64 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (uint64)aCPU.vr[vrA].w[i] +
			(uint64)aCPU.vr[vrB].w[i] + 1;

		aCPU.vr[vrD].w[i] = (res >> 1);
	}
	return 0;
}

/*	vavgsb		Vector Average Signed Byte
 *	v.149
 */
int ppc_opc_vavgsb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = (sint16)aCPU.vr[vrA].sb[i] +
			(sint16)aCPU.vr[vrB].sb[i] + 1;

		aCPU.vr[vrD].sb[i] = (res >> 1);
	}
	return 0;
}

/*	vavgsh		Vector Average Signed Half Word
 *	v.150
 */
int ppc_opc_vavgsh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (sint32)aCPU.vr[vrA].sh[i] +
			(sint32)aCPU.vr[vrB].sh[i] + 1;

		aCPU.vr[vrD].sh[i] = (res >> 1);
	}
	return 0;
}

/*	vavgsw		Vector Average Signed Word
 *	v.151
 */
int ppc_opc_vavgsw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint64 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (sint64)aCPU.vr[vrA].sw[i] +
			(sint64)aCPU.vr[vrB].sw[i] + 1;

		aCPU.vr[vrD].sw[i] = (res >> 1);
	}
	return 0;
}

/*	vmaxub		Vector Maximum Unsigned Byte
 *	v.182
 */
int ppc_opc_vmaxub(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint8 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = aCPU.vr[vrA].b[i];

		if (res < aCPU.vr[vrB].b[i])
			res = aCPU.vr[vrB].b[i];

		aCPU.vr[vrD].b[i] = res;
	}
	return 0;
}

/*	vmaxuh		Vector Maximum Unsigned Half Word
 *	v.183
 */
int ppc_opc_vmaxuh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = aCPU.vr[vrA].h[i];

		if (res < aCPU.vr[vrB].h[i])
			res = aCPU.vr[vrB].h[i];

		aCPU.vr[vrD].h[i] = res;
	}
	return 0;
}

/*	vmaxuw		Vector Maximum Unsigned Word
 *	v.184
 */
int ppc_opc_vmaxuw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = aCPU.vr[vrA].w[i];

		if (res < aCPU.vr[vrB].w[i])
			res = aCPU.vr[vrB].w[i];

		aCPU.vr[vrD].w[i] = res;
	}
	return 0;
}

/*	vmaxsb		Vector Maximum Signed Byte
 *	v.179
 */
int ppc_opc_vmaxsb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint8 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = aCPU.vr[vrA].sb[i];

		if (res < aCPU.vr[vrB].sb[i])
			res = aCPU.vr[vrB].sb[i];

		aCPU.vr[vrD].sb[i] = res;
	}
	return 0;
}

/*	vmaxsh		Vector Maximum Signed Half Word
 *	v.180
 */
int ppc_opc_vmaxsh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = aCPU.vr[vrA].sh[i];

		if (res < aCPU.vr[vrB].sh[i])
			res = aCPU.vr[vrB].sh[i];

		aCPU.vr[vrD].sh[i] = res;
	}
	return 0;
}

/*	vmaxsw		Vector Maximum Signed Word
 *	v.181
 */
int ppc_opc_vmaxsw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = aCPU.vr[vrA].sw[i];

		if (res < aCPU.vr[vrB].sw[i])
			res = aCPU.vr[vrB].sw[i];

		aCPU.vr[vrD].sw[i] = res;
	}
	return 0;
}

/*	vmaxfp		Vector Maximum Floating Point
 *	v.178
 */
int ppc_opc_vmaxfp(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	float res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		res = aCPU.vr[vrA].f[i];

		if (res < aCPU.vr[vrB].f[i])
			res = aCPU.vr[vrB].f[i];

		aCPU.vr[vrD].f[i] = res;
	}
	return 0;
}

/*	vminub		Vector Minimum Unsigned Byte
 *	v.191
 */
int ppc_opc_vminub(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint8 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = aCPU.vr[vrA].b[i];

		if (res > aCPU.vr[vrB].b[i])
			res = aCPU.vr[vrB].b[i];

		aCPU.vr[vrD].b[i] = res;
	}
	return 0;
}

/*	vminuh		Vector Minimum Unsigned Half Word
 *	v.192
 */
int ppc_opc_vminuh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = aCPU.vr[vrA].h[i];

		if (res > aCPU.vr[vrB].h[i])
			res = aCPU.vr[vrB].h[i];

		aCPU.vr[vrD].h[i] = res;
	}
	return 0;
}

/*	vminuw		Vector Minimum Unsigned Word
 *	v.193
 */
int ppc_opc_vminuw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = aCPU.vr[vrA].w[i];

		if (res > aCPU.vr[vrB].w[i])
			res = aCPU.vr[vrB].w[i];

		aCPU.vr[vrD].w[i] = res;
	}
	return 0;
}

/*	vminsb		Vector Minimum Signed Byte
 *	v.188
 */
int ppc_opc_vminsb(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint8 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = aCPU.vr[vrA].sb[i];

		if (res > aCPU.vr[vrB].sb[i])
			res = aCPU.vr[vrB].sb[i];

		aCPU.vr[vrD].sb[i] = res;
	}
	return 0;
}

/*	vminsh		Vector Minimum Signed Half Word
 *	v.189
 */
int ppc_opc_vminsh(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = aCPU.vr[vrA].sh[i];

		if (res > aCPU.vr[vrB].sh[i])
			res = aCPU.vr[vrB].sh[i];

		aCPU.vr[vrD].sh[i] = res;
	}
	return 0;
}

/*	vminsw		Vector Minimum Signed Word
 *	v.190
 */
int ppc_opc_vminsw(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = aCPU.vr[vrA].sw[i];

		if (res > aCPU.vr[vrB].sw[i])
			res = aCPU.vr[vrB].sw[i];

		aCPU.vr[vrD].sw[i] = res;
	}
	return 0;
}

/*	vminfp		Vector Minimum Floating Point
 *	v.187
 */
int ppc_opc_vminfp(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	float res;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		res = aCPU.vr[vrA].f[i];

		if (res > aCPU.vr[vrB].f[i])
			res = aCPU.vr[vrB].f[i];

		aCPU.vr[vrD].f[i] = res;
	}
	return 0;
}

/*	vrfin		Vector Round to Floating-Point Integer Nearest
 *	v.231
 */
int ppc_opc_vrfin(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	/* Documentation doesn't dictate how this instruction should
	 *   round from a middle point.  With a test on a real G4, it was
	 *   found to be round to nearest, with bias to even if equidistant.
	 *
	 * This is covered by the function rint()
	 */
	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		aCPU.vr[vrD].f[i] = rintf(aCPU.vr[vrB].f[i]);
	}
	return 0;
}

/*	vrfip		Vector Round to Floating-Point Integer toward Plus Infinity
 *	v.232
 */
int ppc_opc_vrfip(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		aCPU.vr[vrD].f[i] = ceilf(aCPU.vr[vrB].f[i]);
	}
	return 0;
}

/*	vrfim		Vector Round to Floating-Point Integer toward Minus Infinity
 *	v.230
 */
int ppc_opc_vrfim(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		aCPU.vr[vrD].f[i] = floorf(aCPU.vr[vrB].f[i]);
	}
	return 0;
}

/*	vrfiz	Vector Round to Floating-Point Integer toward Zero
 *	v.233
 */
int ppc_opc_vrfiz(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		aCPU.vr[vrD].f[i] = truncf(aCPU.vr[vrD].f[i]);
	}
	return 0;
}

/*	vrefp		Vector Reciprocal Estimate Floating Point
 *	v.228
 */
int ppc_opc_vrefp(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	/* This emulation generates an exact value, instead of an estimate.
	 *   This is technically within specs, but some test-suites expect the
	 *   exact estimate value returned by G4s.  These anomolous failures
	 *   should be ignored.
	 */

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		aCPU.vr[vrD].f[i] = 1 / aCPU.vr[vrB].f[i];
	}
	return 0;
}

/*	vrsqrtefp	Vector Reciprocal Square Root Estimate Floating Point
 *	v.237
 */
int ppc_opc_vrsqrtefp(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	/* This emulation generates an exact value, instead of an estimate.
	 *   This is technically within specs, but some test-suites expect the
	 *   exact estimate value returned by G4s.  These anomolous failures
	 *   should be ignored.
	 */

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		aCPU.vr[vrD].f[i] = 1 / sqrt(aCPU.vr[vrB].f[i]);
	}
	return 0;
}

/*	vlogefp		Vector Log2 Estimate Floating Point
 *	v.175
 */
int ppc_opc_vlogefp(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	/* This emulation generates an exact value, instead of an estimate.
	 *   This is technically within specs, but some test-suites expect the
	 *   exact estimate value returned by G4s.  These anomolous failures
	 *   should be ignored.
	 */

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		aCPU.vr[vrD].f[i] = log2(aCPU.vr[vrB].f[i]);
	}
	return 0;
}

/*	vexptefp	Vector 2 Raised to the Exponent Estimate Floating Point
 *	v.173
 */
int ppc_opc_vexptefp(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	/* This emulation generates an exact value, instead of an estimate.
	 *   This is technically within specs, but some test-suites expect the
	 *   exact estimate value returned by G4s.  These anomolous failures
	 *   should be ignored.
	 */

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		aCPU.vr[vrD].f[i] = exp2(aCPU.vr[vrB].f[i]);
	}
	return 0;
}

/*	vcfux		Vector Convert from Unsigned Fixed-Point Word
 *	v.156
 */
int ppc_opc_vcfux(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 uimm;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, uimm, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		aCPU.vr[vrD].f[i] = ((float)aCPU.vr[vrB].w[i]) / (1 << uimm);
 	}
	return 0;
}

/*	vcfsx		Vector Convert from Signed Fixed-Point Word
 *	v.155
 */
int ppc_opc_vcfsx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 uimm;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, uimm, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		aCPU.vr[vrD].f[i] = ((float)aCPU.vr[vrB].sw[i]) / (1 << uimm);
 	}
	return 0;
}

/*	vctsxs		Vector Convert To Signed Fixed-Point Word Saturate
 *	v.171
 */
int ppc_opc_vctsxs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 uimm;
	float ftmp;
	sint32 tmp;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, uimm, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		ftmp = aCPU.vr[vrB].f[i] * (float)(1 << uimm);
		ftmp = rintf(ftmp);

		tmp = (sint32)ftmp;

		if (ftmp > 2147483647.0) {
			tmp = 2147483647;		// 0x7fffffff
			aCPU.vscr |= VSCR_SAT;
		} else if (ftmp < -2147483648.0) {
			tmp = -2147483648LL;		// 0x80000000
			aCPU.vscr |= VSCR_SAT;
		}

		aCPU.vr[vrD].sw[i] = tmp;
 	}
	return 0;
}

/*	vctuxs		Vector Convert to Unsigned Fixed-Point Word Saturate
 *	v.172
 */
int ppc_opc_vctuxs(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 tmp, uimm;
	float ftmp;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, uimm, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		ftmp = aCPU.vr[vrB].f[i] * (float)(1 << uimm);
		ftmp = rintf(ftmp);

		tmp = (uint32)ftmp;

		if (ftmp > 4294967295.0) {
			tmp = 0xffffffff;
			aCPU.vscr |= VSCR_SAT;
		} else if (ftmp < 0) {
			tmp = 0;
			aCPU.vscr |= VSCR_SAT;
		}

		aCPU.vr[vrD].w[i] = tmp;
 	}
	return 0;
}

/*	vand		Vector Logical AND
 *	v.147
 */
int ppc_opc_vand(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	aCPU.vr[vrD].d[0] = aCPU.vr[vrA].d[0] & aCPU.vr[vrB].d[0];
	aCPU.vr[vrD].d[1] = aCPU.vr[vrA].d[1] & aCPU.vr[vrB].d[1];
	return 0;
}

/*	vandc		Vector Logical AND with Complement
 *	v.148
 */
int ppc_opc_vandc(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	aCPU.vr[vrD].d[0] = aCPU.vr[vrA].d[0] & ~aCPU.vr[vrB].d[0];
	aCPU.vr[vrD].d[1] = aCPU.vr[vrA].d[1] & ~aCPU.vr[vrB].d[1];
	return 0;
}

/*	vor		Vector Logical OR
 *	v.217
 */
int ppc_opc_vor(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	aCPU.vr[vrD].d[0] = aCPU.vr[vrA].d[0] | aCPU.vr[vrB].d[0];
	aCPU.vr[vrD].d[1] = aCPU.vr[vrA].d[1] | aCPU.vr[vrB].d[1];
	return 0;
}

/*	vnor		Vector Logical NOR
 *	v.216
 */
int ppc_opc_vnor(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	aCPU.vr[vrD].d[0] = ~(aCPU.vr[vrA].d[0] | aCPU.vr[vrB].d[0]);
	aCPU.vr[vrD].d[1] = ~(aCPU.vr[vrA].d[1] | aCPU.vr[vrB].d[1]);
	return 0;
}

/*	vxor		Vector Logical XOR
 *	v.282
 */
int ppc_opc_vxor(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	aCPU.vr[vrD].d[0] = aCPU.vr[vrA].d[0] ^ aCPU.vr[vrB].d[0];
	aCPU.vr[vrD].d[1] = aCPU.vr[vrA].d[1] ^ aCPU.vr[vrB].d[1];
	return 0;
}

#define CR_CR6		(0x00f0)
#define CR_CR6_EQ	(1<<7)
#define CR_CR6_NE_SOME	(1<<6)
#define CR_CR6_NE	(1<<5)
#define CR_CR6_EQ_SOME	(1<<4)

/*	vcmpequbx	Vector Compare Equal-to Unsigned Byte
 *	v.160
 */
int ppc_opc_vcmpequbx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		if (aCPU.vr[vrA].b[i] == aCPU.vr[vrB].b[i]) {
			aCPU.vr[vrD].b[i] = 0xff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			aCPU.vr[vrD].b[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & aCPU.current_opc) {
		aCPU.cr &= ~CR_CR6;
		aCPU.cr |= tf;
	}
	return 0;
}

/*	vcmpequhx	Vector Compare Equal-to Unsigned Half Word
 *	v.161
 */
int ppc_opc_vcmpequhx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		if (aCPU.vr[vrA].h[i] == aCPU.vr[vrB].h[i]) {
			aCPU.vr[vrD].h[i] = 0xffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			aCPU.vr[vrD].h[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & aCPU.current_opc) {
		aCPU.cr &= ~CR_CR6;
		aCPU.cr |= tf;
	}
	return 0;
}

/*	vcmpequwx	Vector Compare Equal-to Unsigned Word
 *	v.162
 */
int ppc_opc_vcmpequwx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		if (aCPU.vr[vrA].w[i] == aCPU.vr[vrB].w[i]) {
			aCPU.vr[vrD].w[i] = 0xffffffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			aCPU.vr[vrD].w[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & aCPU.current_opc) {
		aCPU.cr &= ~CR_CR6;
		aCPU.cr |= tf;
	}
	return 0;
}

/*	vcmpeqfpx	Vector Compare Equal-to-Floating Point
 *	v.159
 */
int ppc_opc_vcmpeqfpx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		if (aCPU.vr[vrA].f[i] == aCPU.vr[vrB].f[i]) {
			aCPU.vr[vrD].w[i] = 0xffffffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			aCPU.vr[vrD].w[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & aCPU.current_opc) {
		aCPU.cr &= ~CR_CR6;
		aCPU.cr |= tf;
	}
	return 0;
}

/*	vcmpgtubx	Vector Compare Greater-Than Unsigned Byte
 *	v.168
 */
int ppc_opc_vcmpgtubx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		if (aCPU.vr[vrA].b[i] > aCPU.vr[vrB].b[i]) {
			aCPU.vr[vrD].b[i] = 0xff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			aCPU.vr[vrD].b[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & aCPU.current_opc) {
		aCPU.cr &= ~CR_CR6;
		aCPU.cr |= tf;
	}
	return 0;
}

/*	vcmpgtsbx	Vector Compare Greater-Than Signed Byte
 *	v.165
 */
int ppc_opc_vcmpgtsbx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		if (aCPU.vr[vrA].sb[i] > aCPU.vr[vrB].sb[i]) {
			aCPU.vr[vrD].b[i] = 0xff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			aCPU.vr[vrD].b[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & aCPU.current_opc) {
		aCPU.cr &= ~CR_CR6;
		aCPU.cr |= tf;
	}
	return 0;
}

/*	vcmpgtuhx	Vector Compare Greater-Than Unsigned Half Word
 *	v.169
 */
int ppc_opc_vcmpgtuhx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		if (aCPU.vr[vrA].h[i] > aCPU.vr[vrB].h[i]) {
			aCPU.vr[vrD].h[i] = 0xffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			aCPU.vr[vrD].h[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & aCPU.current_opc) {
		aCPU.cr &= ~CR_CR6;
		aCPU.cr |= tf;
	}
	return 0;
}

/*	vcmpgtshx	Vector Compare Greater-Than Signed Half Word
 *	v.166
 */
int ppc_opc_vcmpgtshx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		if (aCPU.vr[vrA].sh[i] > aCPU.vr[vrB].sh[i]) {
			aCPU.vr[vrD].h[i] = 0xffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			aCPU.vr[vrD].h[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & aCPU.current_opc) {
		aCPU.cr &= ~CR_CR6;
		aCPU.cr |= tf;
	}
	return 0;
}

/*	vcmpgtuwx	Vector Compare Greater-Than Unsigned Word
 *	v.170
 */
int ppc_opc_vcmpgtuwx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		if (aCPU.vr[vrA].w[i] > aCPU.vr[vrB].w[i]) {
			aCPU.vr[vrD].w[i] = 0xffffffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			aCPU.vr[vrD].w[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & aCPU.current_opc) {
		aCPU.cr &= ~CR_CR6;
		aCPU.cr |= tf;
	}
	return 0;
}

/*	vcmpgtswx	Vector Compare Greater-Than Signed Word
 *	v.167
 */
int ppc_opc_vcmpgtswx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		if (aCPU.vr[vrA].sw[i] > aCPU.vr[vrB].sw[i]) {
			aCPU.vr[vrD].w[i] = 0xffffffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			aCPU.vr[vrD].w[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & aCPU.current_opc) {
		aCPU.cr &= ~CR_CR6;
		aCPU.cr |= tf;
	}
	return 0;
}

/*	vcmpgtfpx	Vector Compare Greater-Than Floating-Point
 *	v.164
 */
int ppc_opc_vcmpgtfpx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		if (aCPU.vr[vrA].f[i] > aCPU.vr[vrB].f[i]) {
			aCPU.vr[vrD].w[i] = 0xffffffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			aCPU.vr[vrD].w[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & aCPU.current_opc) {
		aCPU.cr &= ~CR_CR6;
		aCPU.cr |= tf;
	}
	return 0;
}

/*	vcmpgefpx	Vector Compare Greater-Than-or-Equal-to Floating Point
 *	v.163
 */
int ppc_opc_vcmpgefpx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		if (aCPU.vr[vrA].f[i] >= aCPU.vr[vrB].f[i]) {
			aCPU.vr[vrD].w[i] = 0xffffffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			aCPU.vr[vrD].w[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & aCPU.current_opc) {
		aCPU.cr &= ~CR_CR6;
		aCPU.cr |= tf;
	}
	return 0;
}

/*	vcmpbfpx	Vector Compare Bounds Floating Point
 *	v.157
 */
int ppc_opc_vcmpbfpx(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int le, ge;
	int ib=CR_CR6_NE;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		le = (aCPU.vr[vrA].f[i] <= aCPU.vr[vrB].f[i]) ? 0 : 0x80000000;
		ge = (aCPU.vr[vrA].f[i] >= -aCPU.vr[vrB].f[i]) ? 0 : 0x40000000;

		aCPU.vr[vrD].w[i] = le | ge;
		if (le | ge) {
			ib = 0;
 		}
	}

	if (PPC_OPC_VRc & aCPU.current_opc) {
		aCPU.cr &= ~CR_CR6;
		aCPU.cr |= ib;
	}
	return 0;
}
