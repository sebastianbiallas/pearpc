/*
 *  PearPC
 *  ppc_fpu.h
 *
 *  Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
 *  Copyright (C) 2003, 2004 Stefan Weyergraf
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

#ifndef __PPC_FPU_H__
#define __PPC_FPU_H__

/*
 *  This header is a minimal stub for AArch64.
 *  The full FPU math routines are in the shared ppc_fpu.h from x86_64.
 *  For now, we just forward-declare the interpreter and gen functions.
 */

#include "jitc.h"
#include "jitc_asm.h"
#include "ppc_exc.h"

static UNUSED void ppc_opc_gen_check_fpu(JITC &jitc)
{
    if (!jitc.checkedFloat) {
        jitc.clobberCarryAndFlags();

        NativeReg r1 = jitc.getClientRegister(PPC_MSR);
        // TODO: emit TST r1, #MSR_FP and conditional branch
        // For now, just mark as checked (stub)
        jitc.checkedFloat = true;
    }
}

static UNUSED void ppc_opc_gen_check_vec(JITC &jitc)
{
    if (!jitc.checkedVector) {
        // TODO: check MSR_VEC
        jitc.checkedVector = true;
    }
}

void ppc_opc_fabsx(PPC_CPU_State &aCPU);
void ppc_opc_faddx(PPC_CPU_State &aCPU);
void ppc_opc_faddsx(PPC_CPU_State &aCPU);
void ppc_opc_fcmpo(PPC_CPU_State &aCPU);
void ppc_opc_fcmpu(PPC_CPU_State &aCPU);
void ppc_opc_fctiwx(PPC_CPU_State &aCPU);
void ppc_opc_fctiwzx(PPC_CPU_State &aCPU);
void ppc_opc_fdivx(PPC_CPU_State &aCPU);
void ppc_opc_fdivsx(PPC_CPU_State &aCPU);
void ppc_opc_fmaddx(PPC_CPU_State &aCPU);
void ppc_opc_fmaddsx(PPC_CPU_State &aCPU);
void ppc_opc_fmrx(PPC_CPU_State &aCPU);
void ppc_opc_fmsubx(PPC_CPU_State &aCPU);
void ppc_opc_fmsubsx(PPC_CPU_State &aCPU);
void ppc_opc_fmulx(PPC_CPU_State &aCPU);
void ppc_opc_fmulsx(PPC_CPU_State &aCPU);
void ppc_opc_fnabsx(PPC_CPU_State &aCPU);
void ppc_opc_fnegx(PPC_CPU_State &aCPU);
void ppc_opc_fnmaddx(PPC_CPU_State &aCPU);
void ppc_opc_fnmaddsx(PPC_CPU_State &aCPU);
void ppc_opc_fnmsubx(PPC_CPU_State &aCPU);
void ppc_opc_fnmsubsx(PPC_CPU_State &aCPU);
void ppc_opc_fresx(PPC_CPU_State &aCPU);
void ppc_opc_frspx(PPC_CPU_State &aCPU);
void ppc_opc_frsqrtex(PPC_CPU_State &aCPU);
void ppc_opc_fselx(PPC_CPU_State &aCPU);
void ppc_opc_fsqrtx(PPC_CPU_State &aCPU);
void ppc_opc_fsqrtsx(PPC_CPU_State &aCPU);
void ppc_opc_fsubx(PPC_CPU_State &aCPU);
void ppc_opc_fsubsx(PPC_CPU_State &aCPU);

JITCFlow ppc_opc_gen_fabsx(JITC &aJITC);
JITCFlow ppc_opc_gen_faddx(JITC &aJITC);
JITCFlow ppc_opc_gen_faddsx(JITC &aJITC);
JITCFlow ppc_opc_gen_fcmpo(JITC &aJITC);
JITCFlow ppc_opc_gen_fcmpu(JITC &aJITC);
JITCFlow ppc_opc_gen_fctiwx(JITC &aJITC);
JITCFlow ppc_opc_gen_fctiwzx(JITC &aJITC);
JITCFlow ppc_opc_gen_fdivx(JITC &aJITC);
JITCFlow ppc_opc_gen_fdivsx(JITC &aJITC);
JITCFlow ppc_opc_gen_fmaddx(JITC &aJITC);
JITCFlow ppc_opc_gen_fmaddsx(JITC &aJITC);
JITCFlow ppc_opc_gen_fmrx(JITC &aJITC);
JITCFlow ppc_opc_gen_fmsubx(JITC &aJITC);
JITCFlow ppc_opc_gen_fmsubsx(JITC &aJITC);
JITCFlow ppc_opc_gen_fmulx(JITC &aJITC);
JITCFlow ppc_opc_gen_fmulsx(JITC &aJITC);
JITCFlow ppc_opc_gen_fnabsx(JITC &aJITC);
JITCFlow ppc_opc_gen_fnegx(JITC &aJITC);
JITCFlow ppc_opc_gen_fnmaddx(JITC &aJITC);
JITCFlow ppc_opc_gen_fnmaddsx(JITC &aJITC);
JITCFlow ppc_opc_gen_fnmsubx(JITC &aJITC);
JITCFlow ppc_opc_gen_fnmsubsx(JITC &aJITC);
JITCFlow ppc_opc_gen_fresx(JITC &aJITC);
JITCFlow ppc_opc_gen_frspx(JITC &aJITC);
JITCFlow ppc_opc_gen_frsqrtex(JITC &aJITC);
JITCFlow ppc_opc_gen_fselx(JITC &aJITC);
JITCFlow ppc_opc_gen_fsqrtx(JITC &aJITC);
JITCFlow ppc_opc_gen_fsqrtsx(JITC &aJITC);
JITCFlow ppc_opc_gen_fsubx(JITC &aJITC);
JITCFlow ppc_opc_gen_fsubsx(JITC &aJITC);

#endif
