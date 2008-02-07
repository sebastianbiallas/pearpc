/*
 *	PearPC
 *	ppc_vec.h
 *
 *	Copyright (C) 2004 Daniel Foesch (dfoesch@cs.nmsu.edu)
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

#define VECT_EVEN(index)	(((index) << 1) + 1)
#define VECT_ODD(index)		(((index) << 1) + 0)

#define VECT_BUILD(x,y,z,w)	w, z, y, x

#elif HOST_ENDIANESS == HOST_ENDIANESS_BE

#define VECT_B(reg, index)	((reg).b[(index)])
#define VECT_SB(reg, index)	((reg).sb[(index)])
#define VECT_H(reg, index)	((reg).h[(index)])
#define VECT_SH(reg, index)	((reg).sh[(index)])
#define VECT_W(reg, index)	((reg).w[(index)])
#define VECT_SW(reg, index)	((reg).sw[(index)])
#define VECT_D(reg, index)	((reg).d[(index)])
#define VECT_SD(reg, index)	((reg).sd[(index)])

#define VECT_EVEN(index)	(((index) << 1) + 0)
#define VECT_ODD(index)		(((index) << 1) + 1)

#define VECT_BUILD(x,y,z,w)	x, y, z, w
#else
#error Endianess not supported!
#endif

//#define VECTOR_DEBUG	fprintf(stderr, "[PPC/VEC] %s\n", __FUNCTION__)
//#define VECTOR_DEBUG	jitcAssertFlushedVectorRegisters()
#define VECTOR_DEBUG

#define VECTOR_DEBUG_COMMON	VECTOR_DEBUG
//#define VECTOR_DEBUG_COMMON

/* Undefine this to turn of the MSR_VEC check for vector instructions. */
//#define __VEC_EXC_OFF__

#include "system/types.h"

#include "tools/snprintf.h"

#include "jitc.h"
#include "jitc_asm.h"
#include "ppc_exc.h"

static UNUSED void ppc_opc_gen_check_vec(JITC &jitc)
{
	if (!jitc.checkedVector) {
//		jitcFloatRegisterClobberAll(); FIXME64
//		jitc.flushVectorRegister();
		jitc.clobberCarryAndFlags();

		NativeReg r1 = jitc.getClientRegister(PPC_MSR);
		jitc.asmALU32(X86_TEST, r1, MSR_VEC);
		NativeAddress fixup = jitc.asmJxxFixup(X86_NZ);

		jitc.flushRegisterDirty();
		jitc.asmALU32(X86_MOV, RSI, jitc.pc);
		jitc.asmJMP((NativeAddress)ppc_no_vec_exception_asm);

		jitc.asmResolveFixup(fixup);
		jitc.checkedVector = true;
	}
}

#if 0
void ppc_opc_vperm(PPC_CPU_State &aCPU);
void ppc_opc_vsel(PPC_CPU_State &aCPU);
void ppc_opc_vsrb(PPC_CPU_State &aCPU);
void ppc_opc_vsrh(PPC_CPU_State &aCPU);
void ppc_opc_vsrw(PPC_CPU_State &aCPU);
void ppc_opc_vsrab(PPC_CPU_State &aCPU);
void ppc_opc_vsrah(PPC_CPU_State &aCPU);
void ppc_opc_vsraw(PPC_CPU_State &aCPU);
void ppc_opc_vsr(PPC_CPU_State &aCPU);
void ppc_opc_vsro(PPC_CPU_State &aCPU);
void ppc_opc_vslb(PPC_CPU_State &aCPU);
void ppc_opc_vslh(PPC_CPU_State &aCPU);
void ppc_opc_vslw(PPC_CPU_State &aCPU);
void ppc_opc_vsl(PPC_CPU_State &aCPU);
void ppc_opc_vslo(PPC_CPU_State &aCPU);
void ppc_opc_vsldoi(PPC_CPU_State &aCPU);
void ppc_opc_vrlb(PPC_CPU_State &aCPU);
void ppc_opc_vrlh(PPC_CPU_State &aCPU);
void ppc_opc_vrlw(PPC_CPU_State &aCPU);
void ppc_opc_vmrghb(PPC_CPU_State &aCPU);
void ppc_opc_vmrghh(PPC_CPU_State &aCPU);
void ppc_opc_vmrghw(PPC_CPU_State &aCPU);
void ppc_opc_vmrglb(PPC_CPU_State &aCPU);
void ppc_opc_vmrglh(PPC_CPU_State &aCPU);
void ppc_opc_vmrglw(PPC_CPU_State &aCPU);
void ppc_opc_vspltb(PPC_CPU_State &aCPU);
void ppc_opc_vsplth(PPC_CPU_State &aCPU);
void ppc_opc_vspltw(PPC_CPU_State &aCPU);
void ppc_opc_vspltisb(PPC_CPU_State &aCPU);
void ppc_opc_vspltish(PPC_CPU_State &aCPU);
void ppc_opc_vspltisw(PPC_CPU_State &aCPU);
void ppc_opc_mfvscr(PPC_CPU_State &aCPU);
void ppc_opc_mtvscr(PPC_CPU_State &aCPU);
void ppc_opc_vpkuhum(PPC_CPU_State &aCPU);
void ppc_opc_vpkuwum(PPC_CPU_State &aCPU);
void ppc_opc_vpkpx(PPC_CPU_State &aCPU);
void ppc_opc_vpkuhus(PPC_CPU_State &aCPU);
void ppc_opc_vpkshss(PPC_CPU_State &aCPU);
void ppc_opc_vpkuwus(PPC_CPU_State &aCPU);
void ppc_opc_vpkswss(PPC_CPU_State &aCPU);
void ppc_opc_vpkuhus(PPC_CPU_State &aCPU);
void ppc_opc_vpkshus(PPC_CPU_State &aCPU);
void ppc_opc_vpkuwus(PPC_CPU_State &aCPU);
void ppc_opc_vpkswus(PPC_CPU_State &aCPU);
void ppc_opc_vupkhsb(PPC_CPU_State &aCPU);
void ppc_opc_vupkhpx(PPC_CPU_State &aCPU);
void ppc_opc_vupkhsh(PPC_CPU_State &aCPU);
void ppc_opc_vupklsb(PPC_CPU_State &aCPU);
void ppc_opc_vupklpx(PPC_CPU_State &aCPU);
void ppc_opc_vupklsh(PPC_CPU_State &aCPU);
void ppc_opc_vaddubm(PPC_CPU_State &aCPU);
void ppc_opc_vadduhm(PPC_CPU_State &aCPU);
void ppc_opc_vadduwm(PPC_CPU_State &aCPU);
void ppc_opc_vaddfp(PPC_CPU_State &aCPU);
void ppc_opc_vaddcuw(PPC_CPU_State &aCPU);
void ppc_opc_vaddubs(PPC_CPU_State &aCPU);
void ppc_opc_vaddsbs(PPC_CPU_State &aCPU);
void ppc_opc_vadduhs(PPC_CPU_State &aCPU);
void ppc_opc_vaddshs(PPC_CPU_State &aCPU);
void ppc_opc_vadduws(PPC_CPU_State &aCPU);
void ppc_opc_vaddsws(PPC_CPU_State &aCPU);
void ppc_opc_vsububm(PPC_CPU_State &aCPU);
void ppc_opc_vsubuhm(PPC_CPU_State &aCPU);
void ppc_opc_vsubuwm(PPC_CPU_State &aCPU);
void ppc_opc_vsubfp(PPC_CPU_State &aCPU);
void ppc_opc_vsubcuw(PPC_CPU_State &aCPU);
void ppc_opc_vsububs(PPC_CPU_State &aCPU);
void ppc_opc_vsubsbs(PPC_CPU_State &aCPU);
void ppc_opc_vsubuhs(PPC_CPU_State &aCPU);
void ppc_opc_vsubshs(PPC_CPU_State &aCPU);
void ppc_opc_vsubuws(PPC_CPU_State &aCPU);
void ppc_opc_vsubsws(PPC_CPU_State &aCPU);
void ppc_opc_vmuleub(PPC_CPU_State &aCPU);
void ppc_opc_vmulesb(PPC_CPU_State &aCPU);
void ppc_opc_vmuleuh(PPC_CPU_State &aCPU);
void ppc_opc_vmulesh(PPC_CPU_State &aCPU);
void ppc_opc_vmuloub(PPC_CPU_State &aCPU);
void ppc_opc_vmulosb(PPC_CPU_State &aCPU);
void ppc_opc_vmulouh(PPC_CPU_State &aCPU);
void ppc_opc_vmulosh(PPC_CPU_State &aCPU);
void ppc_opc_vmaddfp(PPC_CPU_State &aCPU);
void ppc_opc_vmhaddshs(PPC_CPU_State &aCPU);
void ppc_opc_vmladduhm(PPC_CPU_State &aCPU);
void ppc_opc_vmhraddshs(PPC_CPU_State &aCPU);
void ppc_opc_vmsumubm(PPC_CPU_State &aCPU);
void ppc_opc_vmsumuhm(PPC_CPU_State &aCPU);
void ppc_opc_vmsummbm(PPC_CPU_State &aCPU);
void ppc_opc_vmsumshm(PPC_CPU_State &aCPU);
void ppc_opc_vmsumuhs(PPC_CPU_State &aCPU);
void ppc_opc_vmsumshs(PPC_CPU_State &aCPU);
void ppc_opc_vsum4ubs(PPC_CPU_State &aCPU);
void ppc_opc_vsum4sbs(PPC_CPU_State &aCPU);
void ppc_opc_vsum4shs(PPC_CPU_State &aCPU);
void ppc_opc_vsum2sws(PPC_CPU_State &aCPU);
void ppc_opc_vsumsws(PPC_CPU_State &aCPU);
void ppc_opc_vnmsubfp(PPC_CPU_State &aCPU);
void ppc_opc_vavgub(PPC_CPU_State &aCPU);
void ppc_opc_vavgsb(PPC_CPU_State &aCPU);
void ppc_opc_vavguh(PPC_CPU_State &aCPU);
void ppc_opc_vavgsh(PPC_CPU_State &aCPU);
void ppc_opc_vavguw(PPC_CPU_State &aCPU);
void ppc_opc_vavgsw(PPC_CPU_State &aCPU);
void ppc_opc_vmaxub(PPC_CPU_State &aCPU);
void ppc_opc_vmaxsb(PPC_CPU_State &aCPU);
void ppc_opc_vmaxuh(PPC_CPU_State &aCPU);
void ppc_opc_vmaxsh(PPC_CPU_State &aCPU);
void ppc_opc_vmaxuw(PPC_CPU_State &aCPU);
void ppc_opc_vmaxsw(PPC_CPU_State &aCPU);
void ppc_opc_vmaxfp(PPC_CPU_State &aCPU);
void ppc_opc_vminub(PPC_CPU_State &aCPU);
void ppc_opc_vminsb(PPC_CPU_State &aCPU);
void ppc_opc_vminuh(PPC_CPU_State &aCPU);
void ppc_opc_vminsh(PPC_CPU_State &aCPU);
void ppc_opc_vminuw(PPC_CPU_State &aCPU);
void ppc_opc_vminsw(PPC_CPU_State &aCPU);
void ppc_opc_vminfp(PPC_CPU_State &aCPU);
void ppc_opc_vrfin(PPC_CPU_State &aCPU);
void ppc_opc_vrfip(PPC_CPU_State &aCPU);
void ppc_opc_vrfim(PPC_CPU_State &aCPU);
void ppc_opc_vrfiz(PPC_CPU_State &aCPU);
void ppc_opc_vrefp(PPC_CPU_State &aCPU);
void ppc_opc_vrsqrtefp(PPC_CPU_State &aCPU);
void ppc_opc_vlogefp(PPC_CPU_State &aCPU);
void ppc_opc_vexptefp(PPC_CPU_State &aCPU);
void ppc_opc_vcfux(PPC_CPU_State &aCPU);
void ppc_opc_vcfsx(PPC_CPU_State &aCPU);
void ppc_opc_vctsxs(PPC_CPU_State &aCPU);
void ppc_opc_vctuxs(PPC_CPU_State &aCPU);
void ppc_opc_vand(PPC_CPU_State &aCPU);
void ppc_opc_vandc(PPC_CPU_State &aCPU);
void ppc_opc_vor(PPC_CPU_State &aCPU);
void ppc_opc_vnor(PPC_CPU_State &aCPU);
void ppc_opc_vxor(PPC_CPU_State &aCPU);
void ppc_opc_vcmpequbx(PPC_CPU_State &aCPU);
void ppc_opc_vcmpequhx(PPC_CPU_State &aCPU);
void ppc_opc_vcmpequwx(PPC_CPU_State &aCPU);
void ppc_opc_vcmpeqfpx(PPC_CPU_State &aCPU);
void ppc_opc_vcmpgtubx(PPC_CPU_State &aCPU);
void ppc_opc_vcmpgtsbx(PPC_CPU_State &aCPU);
void ppc_opc_vcmpgtuhx(PPC_CPU_State &aCPU);
void ppc_opc_vcmpgtshx(PPC_CPU_State &aCPU);
void ppc_opc_vcmpgtuwx(PPC_CPU_State &aCPU);
void ppc_opc_vcmpgtswx(PPC_CPU_State &aCPU);
void ppc_opc_vcmpgtfpx(PPC_CPU_State &aCPU);
void ppc_opc_vcmpgefpx(PPC_CPU_State &aCPU);
void ppc_opc_vcmpbfpx(PPC_CPU_State &aCPU);

JITCFlow ppc_opc_gen_vperm(JITC &aJITC);
JITCFlow ppc_opc_gen_vsel(JITC &aJITC);
JITCFlow ppc_opc_gen_vsrb(JITC &aJITC);
JITCFlow ppc_opc_gen_vsrh(JITC &aJITC);
JITCFlow ppc_opc_gen_vsrw(JITC &aJITC);
JITCFlow ppc_opc_gen_vsrab(JITC &aJITC);
JITCFlow ppc_opc_gen_vsrah(JITC &aJITC);
JITCFlow ppc_opc_gen_vsraw(JITC &aJITC);
JITCFlow ppc_opc_gen_vsr(JITC &aJITC);
JITCFlow ppc_opc_gen_vsro(JITC &aJITC);
JITCFlow ppc_opc_gen_vslb(JITC &aJITC);
JITCFlow ppc_opc_gen_vslh(JITC &aJITC);
JITCFlow ppc_opc_gen_vslw(JITC &aJITC);
JITCFlow ppc_opc_gen_vsl(JITC &aJITC);
JITCFlow ppc_opc_gen_vslo(JITC &aJITC);
JITCFlow ppc_opc_gen_vsldoi(JITC &aJITC);
JITCFlow ppc_opc_gen_vrlb(JITC &aJITC);
JITCFlow ppc_opc_gen_vrlh(JITC &aJITC);
JITCFlow ppc_opc_gen_vrlw(JITC &aJITC);
JITCFlow ppc_opc_gen_vmrghb(JITC &aJITC);
JITCFlow ppc_opc_gen_vmrghh(JITC &aJITC);
JITCFlow ppc_opc_gen_vmrghw(JITC &aJITC);
JITCFlow ppc_opc_gen_vmrglb(JITC &aJITC);
JITCFlow ppc_opc_gen_vmrglh(JITC &aJITC);
JITCFlow ppc_opc_gen_vmrglw(JITC &aJITC);
JITCFlow ppc_opc_gen_vspltb(JITC &aJITC);
JITCFlow ppc_opc_gen_vsplth(JITC &aJITC);
JITCFlow ppc_opc_gen_vspltw(JITC &aJITC);
JITCFlow ppc_opc_gen_vspltisb(JITC &aJITC);
JITCFlow ppc_opc_gen_vspltish(JITC &aJITC);
JITCFlow ppc_opc_gen_vspltisw(JITC &aJITC);
JITCFlow ppc_opc_gen_mfvscr(JITC &aJITC);
JITCFlow ppc_opc_gen_mtvscr(JITC &aJITC);
JITCFlow ppc_opc_gen_vpkuhum(JITC &aJITC);
JITCFlow ppc_opc_gen_vpkuwum(JITC &aJITC);
JITCFlow ppc_opc_gen_vpkpx(JITC &aJITC);
JITCFlow ppc_opc_gen_vpkuhus(JITC &aJITC);
JITCFlow ppc_opc_gen_vpkshss(JITC &aJITC);
JITCFlow ppc_opc_gen_vpkuwus(JITC &aJITC);
JITCFlow ppc_opc_gen_vpkswss(JITC &aJITC);
JITCFlow ppc_opc_gen_vpkuhus(JITC &aJITC);
JITCFlow ppc_opc_gen_vpkshus(JITC &aJITC);
JITCFlow ppc_opc_gen_vpkuwus(JITC &aJITC);
JITCFlow ppc_opc_gen_vpkswus(JITC &aJITC);
JITCFlow ppc_opc_gen_vupkhsb(JITC &aJITC);
JITCFlow ppc_opc_gen_vupkhpx(JITC &aJITC);
JITCFlow ppc_opc_gen_vupkhsh(JITC &aJITC);
JITCFlow ppc_opc_gen_vupklsb(JITC &aJITC);
JITCFlow ppc_opc_gen_vupklpx(JITC &aJITC);
JITCFlow ppc_opc_gen_vupklsh(JITC &aJITC);
JITCFlow ppc_opc_gen_vaddubm(JITC &aJITC);
JITCFlow ppc_opc_gen_vadduhm(JITC &aJITC);
JITCFlow ppc_opc_gen_vadduwm(JITC &aJITC);
JITCFlow ppc_opc_gen_vaddfp(JITC &aJITC);
JITCFlow ppc_opc_gen_vaddcuw(JITC &aJITC);
JITCFlow ppc_opc_gen_vaddubs(JITC &aJITC);
JITCFlow ppc_opc_gen_vaddsbs(JITC &aJITC);
JITCFlow ppc_opc_gen_vadduhs(JITC &aJITC);
JITCFlow ppc_opc_gen_vaddshs(JITC &aJITC);
JITCFlow ppc_opc_gen_vadduws(JITC &aJITC);
JITCFlow ppc_opc_gen_vaddsws(JITC &aJITC);
JITCFlow ppc_opc_gen_vsububm(JITC &aJITC);
JITCFlow ppc_opc_gen_vsubuhm(JITC &aJITC);
JITCFlow ppc_opc_gen_vsubuwm(JITC &aJITC);
JITCFlow ppc_opc_gen_vsubfp(JITC &aJITC);
JITCFlow ppc_opc_gen_vsubcuw(JITC &aJITC);
JITCFlow ppc_opc_gen_vsububs(JITC &aJITC);
JITCFlow ppc_opc_gen_vsubsbs(JITC &aJITC);
JITCFlow ppc_opc_gen_vsubuhs(JITC &aJITC);
JITCFlow ppc_opc_gen_vsubshs(JITC &aJITC);
JITCFlow ppc_opc_gen_vsubuws(JITC &aJITC);
JITCFlow ppc_opc_gen_vsubsws(JITC &aJITC);
JITCFlow ppc_opc_gen_vmuleub(JITC &aJITC);
JITCFlow ppc_opc_gen_vmulesb(JITC &aJITC);
JITCFlow ppc_opc_gen_vmuleuh(JITC &aJITC);
JITCFlow ppc_opc_gen_vmulesh(JITC &aJITC);
JITCFlow ppc_opc_gen_vmuloub(JITC &aJITC);
JITCFlow ppc_opc_gen_vmulosb(JITC &aJITC);
JITCFlow ppc_opc_gen_vmulouh(JITC &aJITC);
JITCFlow ppc_opc_gen_vmulosh(JITC &aJITC);
JITCFlow ppc_opc_gen_vmaddfp(JITC &aJITC);
JITCFlow ppc_opc_gen_vmhaddshs(JITC &aJITC);
JITCFlow ppc_opc_gen_vmladduhm(JITC &aJITC);
JITCFlow ppc_opc_gen_vmhraddshs(JITC &aJITC);
JITCFlow ppc_opc_gen_vmsumubm(JITC &aJITC);
JITCFlow ppc_opc_gen_vmsumuhm(JITC &aJITC);
JITCFlow ppc_opc_gen_vmsummbm(JITC &aJITC);
JITCFlow ppc_opc_gen_vmsumshm(JITC &aJITC);
JITCFlow ppc_opc_gen_vmsumuhs(JITC &aJITC);
JITCFlow ppc_opc_gen_vmsumshs(JITC &aJITC);
JITCFlow ppc_opc_gen_vsum4ubs(JITC &aJITC);
JITCFlow ppc_opc_gen_vsum4sbs(JITC &aJITC);
JITCFlow ppc_opc_gen_vsum4shs(JITC &aJITC);
JITCFlow ppc_opc_gen_vsum2sws(JITC &aJITC);
JITCFlow ppc_opc_gen_vsumsws(JITC &aJITC);
JITCFlow ppc_opc_gen_vnmsubfp(JITC &aJITC);
JITCFlow ppc_opc_gen_vavgub(JITC &aJITC);
JITCFlow ppc_opc_gen_vavgsb(JITC &aJITC);
JITCFlow ppc_opc_gen_vavguh(JITC &aJITC);
JITCFlow ppc_opc_gen_vavgsh(JITC &aJITC);
JITCFlow ppc_opc_gen_vavguw(JITC &aJITC);
JITCFlow ppc_opc_gen_vavgsw(JITC &aJITC);
JITCFlow ppc_opc_gen_vmaxub(JITC &aJITC);
JITCFlow ppc_opc_gen_vmaxsb(JITC &aJITC);
JITCFlow ppc_opc_gen_vmaxuh(JITC &aJITC);
JITCFlow ppc_opc_gen_vmaxsh(JITC &aJITC);
JITCFlow ppc_opc_gen_vmaxuw(JITC &aJITC);
JITCFlow ppc_opc_gen_vmaxsw(JITC &aJITC);
JITCFlow ppc_opc_gen_vmaxfp(JITC &aJITC);
JITCFlow ppc_opc_gen_vminub(JITC &aJITC);
JITCFlow ppc_opc_gen_vminsb(JITC &aJITC);
JITCFlow ppc_opc_gen_vminuh(JITC &aJITC);
JITCFlow ppc_opc_gen_vminsh(JITC &aJITC);
JITCFlow ppc_opc_gen_vminuw(JITC &aJITC);
JITCFlow ppc_opc_gen_vminsw(JITC &aJITC);
JITCFlow ppc_opc_gen_vminfp(JITC &aJITC);
JITCFlow ppc_opc_gen_vrfin(JITC &aJITC);
JITCFlow ppc_opc_gen_vrfip(JITC &aJITC);
JITCFlow ppc_opc_gen_vrfim(JITC &aJITC);
JITCFlow ppc_opc_gen_vrfiz(JITC &aJITC);
JITCFlow ppc_opc_gen_vrefp(JITC &aJITC);
JITCFlow ppc_opc_gen_vrsqrtefp(JITC &aJITC);
JITCFlow ppc_opc_gen_vlogefp(JITC &aJITC);
JITCFlow ppc_opc_gen_vexptefp(JITC &aJITC);
JITCFlow ppc_opc_gen_vcfux(JITC &aJITC);
JITCFlow ppc_opc_gen_vcfsx(JITC &aJITC);
JITCFlow ppc_opc_gen_vctsxs(JITC &aJITC);
JITCFlow ppc_opc_gen_vctuxs(JITC &aJITC);
JITCFlow ppc_opc_gen_vand(JITC &aJITC);
JITCFlow ppc_opc_gen_vandc(JITC &aJITC);
JITCFlow ppc_opc_gen_vor(JITC &aJITC);
JITCFlow ppc_opc_gen_vnor(JITC &aJITC);
JITCFlow ppc_opc_gen_vxor(JITC &aJITC);
JITCFlow ppc_opc_gen_vcmpequbx(JITC &aJITC);
JITCFlow ppc_opc_gen_vcmpequhx(JITC &aJITC);
JITCFlow ppc_opc_gen_vcmpequwx(JITC &aJITC);
JITCFlow ppc_opc_gen_vcmpeqfpx(JITC &aJITC);
JITCFlow ppc_opc_gen_vcmpgtubx(JITC &aJITC);
JITCFlow ppc_opc_gen_vcmpgtsbx(JITC &aJITC);
JITCFlow ppc_opc_gen_vcmpgtuhx(JITC &aJITC);
JITCFlow ppc_opc_gen_vcmpgtshx(JITC &aJITC);
JITCFlow ppc_opc_gen_vcmpgtuwx(JITC &aJITC);
JITCFlow ppc_opc_gen_vcmpgtswx(JITC &aJITC);
JITCFlow ppc_opc_gen_vcmpgtfpx(JITC &aJITC);
JITCFlow ppc_opc_gen_vcmpgefpx(JITC &aJITC);
JITCFlow ppc_opc_gen_vcmpbfpx(JITC &aJITC);

#endif
#endif
