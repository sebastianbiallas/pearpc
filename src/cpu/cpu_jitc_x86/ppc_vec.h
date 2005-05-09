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
#include "x86asm.h"
#include "ppc_exc.h"

static UNUSED void ppc_opc_gen_check_vec()
{
#ifndef __VEC_EXC_OFF__
	if (!gJITC.checkedVector) {
		jitcFloatRegisterClobberAll();
		jitcFlushVectorRegister();
		jitcClobberCarryAndFlags();

		NativeReg r1 = jitcGetClientRegister(PPC_MSR);
		asmALU(X86_TEST, r1, MSR_VEC);
		NativeAddress fixup = asmJxxFixup(X86_NZ);

		jitcFlushRegisterDirty();
		asmALU(X86_MOV, ESI, gJITC.pc);
		asmJMP((NativeAddress)ppc_no_vec_exception_asm);

		asmResolveFixup(fixup);
		gJITC.checkedVector = true;
	}
#endif
}

void ppc_opc_vperm();
void ppc_opc_vsel();
void ppc_opc_vsrb();
void ppc_opc_vsrh();
void ppc_opc_vsrw();
void ppc_opc_vsrab();
void ppc_opc_vsrah();
void ppc_opc_vsraw();
void ppc_opc_vsr();
void ppc_opc_vsro();
void ppc_opc_vslb();
void ppc_opc_vslh();
void ppc_opc_vslw();
void ppc_opc_vsl();
void ppc_opc_vslo();
void ppc_opc_vsldoi();
void ppc_opc_vrlb();
void ppc_opc_vrlh();
void ppc_opc_vrlw();
void ppc_opc_vmrghb();
void ppc_opc_vmrghh();
void ppc_opc_vmrghw();
void ppc_opc_vmrglb();
void ppc_opc_vmrglh();
void ppc_opc_vmrglw();
void ppc_opc_vspltb();
void ppc_opc_vsplth();
void ppc_opc_vspltw();
void ppc_opc_vspltisb();
void ppc_opc_vspltish();
void ppc_opc_vspltisw();
void ppc_opc_mfvscr();
void ppc_opc_mtvscr();
void ppc_opc_vpkuhum();
void ppc_opc_vpkuwum();
void ppc_opc_vpkpx();
void ppc_opc_vpkuhus();
void ppc_opc_vpkshss();
void ppc_opc_vpkuwus();
void ppc_opc_vpkswss();
void ppc_opc_vpkuhus();
void ppc_opc_vpkshus();
void ppc_opc_vpkuwus();
void ppc_opc_vpkswus();
void ppc_opc_vupkhsb();
void ppc_opc_vupkhpx();
void ppc_opc_vupkhsh();
void ppc_opc_vupklsb();
void ppc_opc_vupklpx();
void ppc_opc_vupklsh();
void ppc_opc_vaddubm();
void ppc_opc_vadduhm();
void ppc_opc_vadduwm();
void ppc_opc_vaddfp();
void ppc_opc_vaddcuw();
void ppc_opc_vaddubs();
void ppc_opc_vaddsbs();
void ppc_opc_vadduhs();
void ppc_opc_vaddshs();
void ppc_opc_vadduws();
void ppc_opc_vaddsws();
void ppc_opc_vsububm();
void ppc_opc_vsubuhm();
void ppc_opc_vsubuwm();
void ppc_opc_vsubfp();
void ppc_opc_vsubcuw();
void ppc_opc_vsububs();
void ppc_opc_vsubsbs();
void ppc_opc_vsubuhs();
void ppc_opc_vsubshs();
void ppc_opc_vsubuws();
void ppc_opc_vsubsws();
void ppc_opc_vmuleub();
void ppc_opc_vmulesb();
void ppc_opc_vmuleuh();
void ppc_opc_vmulesh();
void ppc_opc_vmuloub();
void ppc_opc_vmulosb();
void ppc_opc_vmulouh();
void ppc_opc_vmulosh();
void ppc_opc_vmaddfp();
void ppc_opc_vmhaddshs();
void ppc_opc_vmladduhm();
void ppc_opc_vmhraddshs();
void ppc_opc_vmsumubm();
void ppc_opc_vmsumuhm();
void ppc_opc_vmsummbm();
void ppc_opc_vmsumshm();
void ppc_opc_vmsumuhs();
void ppc_opc_vmsumshs();
void ppc_opc_vsum4ubs();
void ppc_opc_vsum4sbs();
void ppc_opc_vsum4shs();
void ppc_opc_vsum2sws();
void ppc_opc_vsumsws();
void ppc_opc_vnmsubfp();
void ppc_opc_vavgub();
void ppc_opc_vavgsb();
void ppc_opc_vavguh();
void ppc_opc_vavgsh();
void ppc_opc_vavguw();
void ppc_opc_vavgsw();
void ppc_opc_vmaxub();
void ppc_opc_vmaxsb();
void ppc_opc_vmaxuh();
void ppc_opc_vmaxsh();
void ppc_opc_vmaxuw();
void ppc_opc_vmaxsw();
void ppc_opc_vmaxfp();
void ppc_opc_vminub();
void ppc_opc_vminsb();
void ppc_opc_vminuh();
void ppc_opc_vminsh();
void ppc_opc_vminuw();
void ppc_opc_vminsw();
void ppc_opc_vminfp();
void ppc_opc_vrfin();
void ppc_opc_vrfip();
void ppc_opc_vrfim();
void ppc_opc_vrfiz();
void ppc_opc_vrefp();
void ppc_opc_vrsqrtefp();
void ppc_opc_vlogefp();
void ppc_opc_vexptefp();
void ppc_opc_vcfux();
void ppc_opc_vcfsx();
void ppc_opc_vctsxs();
void ppc_opc_vctuxs();
void ppc_opc_vand();
void ppc_opc_vandc();
void ppc_opc_vor();
void ppc_opc_vnor();
void ppc_opc_vxor();
void ppc_opc_vcmpequbx();
void ppc_opc_vcmpequhx();
void ppc_opc_vcmpequwx();
void ppc_opc_vcmpeqfpx();
void ppc_opc_vcmpgtubx();
void ppc_opc_vcmpgtsbx();
void ppc_opc_vcmpgtuhx();
void ppc_opc_vcmpgtshx();
void ppc_opc_vcmpgtuwx();
void ppc_opc_vcmpgtswx();
void ppc_opc_vcmpgtfpx();
void ppc_opc_vcmpgefpx();
void ppc_opc_vcmpbfpx();

JITCFlow ppc_opc_gen_vperm();
JITCFlow ppc_opc_gen_vsel();
JITCFlow ppc_opc_gen_vsrb();
JITCFlow ppc_opc_gen_vsrh();
JITCFlow ppc_opc_gen_vsrw();
JITCFlow ppc_opc_gen_vsrab();
JITCFlow ppc_opc_gen_vsrah();
JITCFlow ppc_opc_gen_vsraw();
JITCFlow ppc_opc_gen_vsr();
JITCFlow ppc_opc_gen_vsro();
JITCFlow ppc_opc_gen_vslb();
JITCFlow ppc_opc_gen_vslh();
JITCFlow ppc_opc_gen_vslw();
JITCFlow ppc_opc_gen_vsl();
JITCFlow ppc_opc_gen_vslo();
JITCFlow ppc_opc_gen_vsldoi();
JITCFlow ppc_opc_gen_vrlb();
JITCFlow ppc_opc_gen_vrlh();
JITCFlow ppc_opc_gen_vrlw();
JITCFlow ppc_opc_gen_vmrghb();
JITCFlow ppc_opc_gen_vmrghh();
JITCFlow ppc_opc_gen_vmrghw();
JITCFlow ppc_opc_gen_vmrglb();
JITCFlow ppc_opc_gen_vmrglh();
JITCFlow ppc_opc_gen_vmrglw();
JITCFlow ppc_opc_gen_vspltb();
JITCFlow ppc_opc_gen_vsplth();
JITCFlow ppc_opc_gen_vspltw();
JITCFlow ppc_opc_gen_vspltisb();
JITCFlow ppc_opc_gen_vspltish();
JITCFlow ppc_opc_gen_vspltisw();
JITCFlow ppc_opc_gen_mfvscr();
JITCFlow ppc_opc_gen_mtvscr();
JITCFlow ppc_opc_gen_vpkuhum();
JITCFlow ppc_opc_gen_vpkuwum();
JITCFlow ppc_opc_gen_vpkpx();
JITCFlow ppc_opc_gen_vpkuhus();
JITCFlow ppc_opc_gen_vpkshss();
JITCFlow ppc_opc_gen_vpkuwus();
JITCFlow ppc_opc_gen_vpkswss();
JITCFlow ppc_opc_gen_vpkuhus();
JITCFlow ppc_opc_gen_vpkshus();
JITCFlow ppc_opc_gen_vpkuwus();
JITCFlow ppc_opc_gen_vpkswus();
JITCFlow ppc_opc_gen_vupkhsb();
JITCFlow ppc_opc_gen_vupkhpx();
JITCFlow ppc_opc_gen_vupkhsh();
JITCFlow ppc_opc_gen_vupklsb();
JITCFlow ppc_opc_gen_vupklpx();
JITCFlow ppc_opc_gen_vupklsh();
JITCFlow ppc_opc_gen_vaddubm();
JITCFlow ppc_opc_gen_vadduhm();
JITCFlow ppc_opc_gen_vadduwm();
JITCFlow ppc_opc_gen_vaddfp();
JITCFlow ppc_opc_gen_vaddcuw();
JITCFlow ppc_opc_gen_vaddubs();
JITCFlow ppc_opc_gen_vaddsbs();
JITCFlow ppc_opc_gen_vadduhs();
JITCFlow ppc_opc_gen_vaddshs();
JITCFlow ppc_opc_gen_vadduws();
JITCFlow ppc_opc_gen_vaddsws();
JITCFlow ppc_opc_gen_vsububm();
JITCFlow ppc_opc_gen_vsubuhm();
JITCFlow ppc_opc_gen_vsubuwm();
JITCFlow ppc_opc_gen_vsubfp();
JITCFlow ppc_opc_gen_vsubcuw();
JITCFlow ppc_opc_gen_vsububs();
JITCFlow ppc_opc_gen_vsubsbs();
JITCFlow ppc_opc_gen_vsubuhs();
JITCFlow ppc_opc_gen_vsubshs();
JITCFlow ppc_opc_gen_vsubuws();
JITCFlow ppc_opc_gen_vsubsws();
JITCFlow ppc_opc_gen_vmuleub();
JITCFlow ppc_opc_gen_vmulesb();
JITCFlow ppc_opc_gen_vmuleuh();
JITCFlow ppc_opc_gen_vmulesh();
JITCFlow ppc_opc_gen_vmuloub();
JITCFlow ppc_opc_gen_vmulosb();
JITCFlow ppc_opc_gen_vmulouh();
JITCFlow ppc_opc_gen_vmulosh();
JITCFlow ppc_opc_gen_vmaddfp();
JITCFlow ppc_opc_gen_vmhaddshs();
JITCFlow ppc_opc_gen_vmladduhm();
JITCFlow ppc_opc_gen_vmhraddshs();
JITCFlow ppc_opc_gen_vmsumubm();
JITCFlow ppc_opc_gen_vmsumuhm();
JITCFlow ppc_opc_gen_vmsummbm();
JITCFlow ppc_opc_gen_vmsumshm();
JITCFlow ppc_opc_gen_vmsumuhs();
JITCFlow ppc_opc_gen_vmsumshs();
JITCFlow ppc_opc_gen_vsum4ubs();
JITCFlow ppc_opc_gen_vsum4sbs();
JITCFlow ppc_opc_gen_vsum4shs();
JITCFlow ppc_opc_gen_vsum2sws();
JITCFlow ppc_opc_gen_vsumsws();
JITCFlow ppc_opc_gen_vnmsubfp();
JITCFlow ppc_opc_gen_vavgub();
JITCFlow ppc_opc_gen_vavgsb();
JITCFlow ppc_opc_gen_vavguh();
JITCFlow ppc_opc_gen_vavgsh();
JITCFlow ppc_opc_gen_vavguw();
JITCFlow ppc_opc_gen_vavgsw();
JITCFlow ppc_opc_gen_vmaxub();
JITCFlow ppc_opc_gen_vmaxsb();
JITCFlow ppc_opc_gen_vmaxuh();
JITCFlow ppc_opc_gen_vmaxsh();
JITCFlow ppc_opc_gen_vmaxuw();
JITCFlow ppc_opc_gen_vmaxsw();
JITCFlow ppc_opc_gen_vmaxfp();
JITCFlow ppc_opc_gen_vminub();
JITCFlow ppc_opc_gen_vminsb();
JITCFlow ppc_opc_gen_vminuh();
JITCFlow ppc_opc_gen_vminsh();
JITCFlow ppc_opc_gen_vminuw();
JITCFlow ppc_opc_gen_vminsw();
JITCFlow ppc_opc_gen_vminfp();
JITCFlow ppc_opc_gen_vrfin();
JITCFlow ppc_opc_gen_vrfip();
JITCFlow ppc_opc_gen_vrfim();
JITCFlow ppc_opc_gen_vrfiz();
JITCFlow ppc_opc_gen_vrefp();
JITCFlow ppc_opc_gen_vrsqrtefp();
JITCFlow ppc_opc_gen_vlogefp();
JITCFlow ppc_opc_gen_vexptefp();
JITCFlow ppc_opc_gen_vcfux();
JITCFlow ppc_opc_gen_vcfsx();
JITCFlow ppc_opc_gen_vctsxs();
JITCFlow ppc_opc_gen_vctuxs();
JITCFlow ppc_opc_gen_vand();
JITCFlow ppc_opc_gen_vandc();
JITCFlow ppc_opc_gen_vor();
JITCFlow ppc_opc_gen_vnor();
JITCFlow ppc_opc_gen_vxor();
JITCFlow ppc_opc_gen_vcmpequbx();
JITCFlow ppc_opc_gen_vcmpequhx();
JITCFlow ppc_opc_gen_vcmpequwx();
JITCFlow ppc_opc_gen_vcmpeqfpx();
JITCFlow ppc_opc_gen_vcmpgtubx();
JITCFlow ppc_opc_gen_vcmpgtsbx();
JITCFlow ppc_opc_gen_vcmpgtuhx();
JITCFlow ppc_opc_gen_vcmpgtshx();
JITCFlow ppc_opc_gen_vcmpgtuwx();
JITCFlow ppc_opc_gen_vcmpgtswx();
JITCFlow ppc_opc_gen_vcmpgtfpx();
JITCFlow ppc_opc_gen_vcmpgefpx();
JITCFlow ppc_opc_gen_vcmpbfpx();

#endif
