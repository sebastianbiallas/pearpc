/*
 *	PearPC
 *	ppc_vec.h
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

#ifndef __PPC_VEC_H__
#define __PPC_VEC_H__

#define PPC_OPC_VRc	(1<<10)

/* Rather than write each function to be endianless, we're writing these
 *   defines to do an endianless access to elements of the vector.
 *
 * These are for ADDRESSED vector elements.  Usually, most vector operations
 *   can be performed in either direction without care, so most of the
 *   for-loops should not use these, as it will introduce unneeded code
 *   for little-endian systems.
 */
#if HOST_ENDIANESS == HOST_ENDIANESS_LE

#define VECT_B(reg, index)	((reg).b[15 - (index)])
#define VECT_SB(reg, index)	((reg).sb[15 - (index)])
#define VECT_H(reg, index)	((reg).h[7 - (index)])
#define VECT_SH(reg, index)	((reg).sh[7 - (index)])
#define VECT_W(reg, index)	((reg).w[3 - (index)])
#define VECT_SW(reg, index)	((reg).sw[3 - (index)])
#define VECT_D(reg, index)	((reg).d[1 - (index)])
#define VECT_SD(reg, index)	((reg).sd[1 - (index)])
#define VECT_FLOAT(reg, index)	((reg).f[3 - (index)])

#define VECT_EVEN(index)	(((index) << 1) + 1)
#define VECT_ODD(index)		(((index) << 1) + 0)

#elif HOST_ENDIANESS == HOST_ENDIANESS_BE

#define VECT_B(reg, index)	((reg).b[(index)])
#define VECT_SB(reg, index)	((reg).sb[(index)])
#define VECT_H(reg, index)	((reg).h[(index)])
#define VECT_SH(reg, index)	((reg).sh[(index)])
#define VECT_W(reg, index)	((reg).w[(index)])
#define VECT_SW(reg, index)	((reg).sw[(index)])
#define VECT_D(reg, index)	((reg).d[(index)])
#define VECT_SD(reg, index)	((reg).sd[(index)])
#define VECT_FLOAT(reg, index)	((reg).f[(index)])

#define VECT_EVEN(index)	(((index) << 1) + 0)
#define VECT_ODD(index)		(((index) << 1) + 1)

#else
#error Endianess not supported!
#endif

//#define VECTOR_DEBUG	fprintf(stderr, "[PPC/VEC] %s\n", __FUNCTION__)
#define VECTOR_DEBUG

//#define VECTOR_DEBUG_COMMON	fprintf(stderr, "[PPC/VEC] %s\n", __FUNCTION__)
#define VECTOR_DEBUG_COMMON

/* Undefine this to turn of the MSR_VEC check for vector instructions. */
//#define __VEC_EXC_OFF__

#include "system/types.h"

#include "tools/snprintf.h"

#include "jitc_types.h"

struct PPC_CPU_State;

int ppc_opc_vperm(PPC_CPU_State &aCPU);
int ppc_opc_vsel(PPC_CPU_State &aCPU);
int ppc_opc_vsrb(PPC_CPU_State &aCPU);
int ppc_opc_vsrh(PPC_CPU_State &aCPU);
int ppc_opc_vsrw(PPC_CPU_State &aCPU);
int ppc_opc_vsrab(PPC_CPU_State &aCPU);
int ppc_opc_vsrah(PPC_CPU_State &aCPU);
int ppc_opc_vsraw(PPC_CPU_State &aCPU);
int ppc_opc_vsr(PPC_CPU_State &aCPU);
int ppc_opc_vsro(PPC_CPU_State &aCPU);
int ppc_opc_vslb(PPC_CPU_State &aCPU);
int ppc_opc_vslh(PPC_CPU_State &aCPU);
int ppc_opc_vslw(PPC_CPU_State &aCPU);
int ppc_opc_vsl(PPC_CPU_State &aCPU);
int ppc_opc_vslo(PPC_CPU_State &aCPU);
int ppc_opc_vsldoi(PPC_CPU_State &aCPU);
int ppc_opc_vrlb(PPC_CPU_State &aCPU);
int ppc_opc_vrlh(PPC_CPU_State &aCPU);
int ppc_opc_vrlw(PPC_CPU_State &aCPU);
int ppc_opc_vmrghb(PPC_CPU_State &aCPU);
int ppc_opc_vmrghh(PPC_CPU_State &aCPU);
int ppc_opc_vmrghw(PPC_CPU_State &aCPU);
int ppc_opc_vmrglb(PPC_CPU_State &aCPU);
int ppc_opc_vmrglh(PPC_CPU_State &aCPU);
int ppc_opc_vmrglw(PPC_CPU_State &aCPU);
int ppc_opc_vspltb(PPC_CPU_State &aCPU);
int ppc_opc_vsplth(PPC_CPU_State &aCPU);
int ppc_opc_vspltw(PPC_CPU_State &aCPU);
int ppc_opc_vspltisb(PPC_CPU_State &aCPU);
int ppc_opc_vspltish(PPC_CPU_State &aCPU);
int ppc_opc_vspltisw(PPC_CPU_State &aCPU);
int ppc_opc_mfvscr(PPC_CPU_State &aCPU);
int ppc_opc_mtvscr(PPC_CPU_State &aCPU);
int ppc_opc_vpkuhum(PPC_CPU_State &aCPU);
int ppc_opc_vpkuwum(PPC_CPU_State &aCPU);
int ppc_opc_vpkpx(PPC_CPU_State &aCPU);
int ppc_opc_vpkuhus(PPC_CPU_State &aCPU);
int ppc_opc_vpkshss(PPC_CPU_State &aCPU);
int ppc_opc_vpkuwus(PPC_CPU_State &aCPU);
int ppc_opc_vpkswss(PPC_CPU_State &aCPU);
int ppc_opc_vpkshus(PPC_CPU_State &aCPU);
int ppc_opc_vpkswus(PPC_CPU_State &aCPU);
int ppc_opc_vupkhsb(PPC_CPU_State &aCPU);
int ppc_opc_vupkhpx(PPC_CPU_State &aCPU);
int ppc_opc_vupkhsh(PPC_CPU_State &aCPU);
int ppc_opc_vupklsb(PPC_CPU_State &aCPU);
int ppc_opc_vupklpx(PPC_CPU_State &aCPU);
int ppc_opc_vupklsh(PPC_CPU_State &aCPU);
int ppc_opc_vaddubm(PPC_CPU_State &aCPU);
int ppc_opc_vadduhm(PPC_CPU_State &aCPU);
int ppc_opc_vadduwm(PPC_CPU_State &aCPU);
int ppc_opc_vaddfp(PPC_CPU_State &aCPU);
int ppc_opc_vaddcuw(PPC_CPU_State &aCPU);
int ppc_opc_vaddubs(PPC_CPU_State &aCPU);
int ppc_opc_vaddsbs(PPC_CPU_State &aCPU);
int ppc_opc_vadduhs(PPC_CPU_State &aCPU);
int ppc_opc_vaddshs(PPC_CPU_State &aCPU);
int ppc_opc_vadduws(PPC_CPU_State &aCPU);
int ppc_opc_vaddsws(PPC_CPU_State &aCPU);
int ppc_opc_vsububm(PPC_CPU_State &aCPU);
int ppc_opc_vsubuhm(PPC_CPU_State &aCPU);
int ppc_opc_vsubuwm(PPC_CPU_State &aCPU);
int ppc_opc_vsubfp(PPC_CPU_State &aCPU);
int ppc_opc_vsubcuw(PPC_CPU_State &aCPU);
int ppc_opc_vsububs(PPC_CPU_State &aCPU);
int ppc_opc_vsubsbs(PPC_CPU_State &aCPU);
int ppc_opc_vsubuhs(PPC_CPU_State &aCPU);
int ppc_opc_vsubshs(PPC_CPU_State &aCPU);
int ppc_opc_vsubuws(PPC_CPU_State &aCPU);
int ppc_opc_vsubsws(PPC_CPU_State &aCPU);
int ppc_opc_vmuleub(PPC_CPU_State &aCPU);
int ppc_opc_vmulesb(PPC_CPU_State &aCPU);
int ppc_opc_vmuleuh(PPC_CPU_State &aCPU);
int ppc_opc_vmulesh(PPC_CPU_State &aCPU);
int ppc_opc_vmuloub(PPC_CPU_State &aCPU);
int ppc_opc_vmulosb(PPC_CPU_State &aCPU);
int ppc_opc_vmulouh(PPC_CPU_State &aCPU);
int ppc_opc_vmulosh(PPC_CPU_State &aCPU);
int ppc_opc_vmaddfp(PPC_CPU_State &aCPU);
int ppc_opc_vmhaddshs(PPC_CPU_State &aCPU);
int ppc_opc_vmladduhm(PPC_CPU_State &aCPU);
int ppc_opc_vmhraddshs(PPC_CPU_State &aCPU);
int ppc_opc_vmsumubm(PPC_CPU_State &aCPU);
int ppc_opc_vmsumuhm(PPC_CPU_State &aCPU);
int ppc_opc_vmsummbm(PPC_CPU_State &aCPU);
int ppc_opc_vmsumshm(PPC_CPU_State &aCPU);
int ppc_opc_vmsumuhs(PPC_CPU_State &aCPU);
int ppc_opc_vmsumshs(PPC_CPU_State &aCPU);
int ppc_opc_vsum4ubs(PPC_CPU_State &aCPU);
int ppc_opc_vsum4sbs(PPC_CPU_State &aCPU);
int ppc_opc_vsum4shs(PPC_CPU_State &aCPU);
int ppc_opc_vsum2sws(PPC_CPU_State &aCPU);
int ppc_opc_vsumsws(PPC_CPU_State &aCPU);
int ppc_opc_vnmsubfp(PPC_CPU_State &aCPU);
int ppc_opc_vavgub(PPC_CPU_State &aCPU);
int ppc_opc_vavgsb(PPC_CPU_State &aCPU);
int ppc_opc_vavguh(PPC_CPU_State &aCPU);
int ppc_opc_vavgsh(PPC_CPU_State &aCPU);
int ppc_opc_vavguw(PPC_CPU_State &aCPU);
int ppc_opc_vavgsw(PPC_CPU_State &aCPU);
int ppc_opc_vmaxub(PPC_CPU_State &aCPU);
int ppc_opc_vmaxsb(PPC_CPU_State &aCPU);
int ppc_opc_vmaxuh(PPC_CPU_State &aCPU);
int ppc_opc_vmaxsh(PPC_CPU_State &aCPU);
int ppc_opc_vmaxuw(PPC_CPU_State &aCPU);
int ppc_opc_vmaxsw(PPC_CPU_State &aCPU);
int ppc_opc_vmaxfp(PPC_CPU_State &aCPU);
int ppc_opc_vminub(PPC_CPU_State &aCPU);
int ppc_opc_vminsb(PPC_CPU_State &aCPU);
int ppc_opc_vminuh(PPC_CPU_State &aCPU);
int ppc_opc_vminsh(PPC_CPU_State &aCPU);
int ppc_opc_vminuw(PPC_CPU_State &aCPU);
int ppc_opc_vminsw(PPC_CPU_State &aCPU);
int ppc_opc_vminfp(PPC_CPU_State &aCPU);
int ppc_opc_vrfin(PPC_CPU_State &aCPU);
int ppc_opc_vrfip(PPC_CPU_State &aCPU);
int ppc_opc_vrfim(PPC_CPU_State &aCPU);
int ppc_opc_vrfiz(PPC_CPU_State &aCPU);
int ppc_opc_vrefp(PPC_CPU_State &aCPU);
int ppc_opc_vrsqrtefp(PPC_CPU_State &aCPU);
int ppc_opc_vlogefp(PPC_CPU_State &aCPU);
int ppc_opc_vexptefp(PPC_CPU_State &aCPU);
int ppc_opc_vcfux(PPC_CPU_State &aCPU);
int ppc_opc_vcfsx(PPC_CPU_State &aCPU);
int ppc_opc_vctsxs(PPC_CPU_State &aCPU);
int ppc_opc_vctuxs(PPC_CPU_State &aCPU);
int ppc_opc_vand(PPC_CPU_State &aCPU);
int ppc_opc_vandc(PPC_CPU_State &aCPU);
int ppc_opc_vor(PPC_CPU_State &aCPU);
int ppc_opc_vnor(PPC_CPU_State &aCPU);
int ppc_opc_vxor(PPC_CPU_State &aCPU);
int ppc_opc_vcmpequbx(PPC_CPU_State &aCPU);
int ppc_opc_vcmpequhx(PPC_CPU_State &aCPU);
int ppc_opc_vcmpequwx(PPC_CPU_State &aCPU);
int ppc_opc_vcmpeqfpx(PPC_CPU_State &aCPU);
int ppc_opc_vcmpgtubx(PPC_CPU_State &aCPU);
int ppc_opc_vcmpgtsbx(PPC_CPU_State &aCPU);
int ppc_opc_vcmpgtuhx(PPC_CPU_State &aCPU);
int ppc_opc_vcmpgtshx(PPC_CPU_State &aCPU);
int ppc_opc_vcmpgtuwx(PPC_CPU_State &aCPU);
int ppc_opc_vcmpgtswx(PPC_CPU_State &aCPU);
int ppc_opc_vcmpgtfpx(PPC_CPU_State &aCPU);
int ppc_opc_vcmpgefpx(PPC_CPU_State &aCPU);
int ppc_opc_vcmpbfpx(PPC_CPU_State &aCPU);

#endif
