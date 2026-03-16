/*
 *  PearPC
 *  ppc_opc.cc
 *
 *  Copyright (C) 2026 Sebastian Biallas (sb@biallas.net)
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

/*
 *  AArch64 JIT stub for PPC opcode generation.
 *
 *  All _gen_ functions currently return flowEndBlockUnreachable,
 *  falling back to the interpreter for every opcode.
 *  Actual AArch64 code generation will be implemented incrementally.
 */

#include "debug/tracers.h"
#include "io/pic/pic.h"
#include "info.h"
#include "ppc_cpu.h"
#include "ppc_exc.h"
#include "ppc_mmu.h"
#include "ppc_opc.h"
#include "ppc_dec.h"

#include "jitc.h"
#include "jitc_asm.h"

static uint64 gDECwriteITB;
static uint64 gDECwriteValue;

static void readDEC(PPC_CPU_State &aCPU)
{
    uint64 itb = ppc_get_cpu_ideal_timebase() - gDECwriteITB;
    aCPU.dec = gDECwriteValue - itb;
}

void FASTCALL writeDEC(PPC_CPU_State &aCPU, uint32 newdec)
{
    if (!(aCPU.dec & 0x80000000) && (newdec & 0x80000000)) {
        aCPU.dec = newdec;
        sys_set_timer(gDECtimer, 0, 0, false);
    } else {
        aCPU.dec = newdec;
        /*
         *  1000000000ULL and aCPU.dec are both smaller than 2^32
         *  so this expression can't overflow
         */
        uint64 q = 1000000000ULL * aCPU.dec / gClientTimeBaseFrequency;

        if (q > 20 * 1000 * 1000) {
            PPC_OPC_WARN("write dec > 20 millisec := %08x (%qu)\n", aCPU.dec, q);
            q = 10 * 1000 * 1000;
        }
        sys_set_timer(gDECtimer, 0, q, false);
    }
    gDECwriteValue = aCPU.dec;
    gDECwriteITB = ppc_get_cpu_ideal_timebase();
}

void FASTCALL writeTBL(PPC_CPU_State &aCPU, uint32 newtbl)
{
    uint64 tbBase = ppc_get_cpu_timebase();
    aCPU.tb = (tbBase & 0xffffffff00000000ULL) | (uint64)newtbl;
}

void FASTCALL writeTBU(PPC_CPU_State &aCPU, uint32 newtbu)
{
    uint64 tbBase = ppc_get_cpu_timebase();
    aCPU.tb = ((uint64)newtbu << 32) | (tbBase & 0xffffffff);
}

void ppc_set_msr(PPC_CPU_State &aCPU, uint32 newmsr)
{
    ppc_mmu_tlb_invalidate(aCPU);
#ifndef PPC_CPU_ENABLE_SINGLESTEP
    if (newmsr & MSR_SE) {
        SINGLESTEP("");
        PPC_CPU_WARN("MSR[SE] (singlestep enable) set, but compiled w/o SE support.\n");
    }
#else
    aCPU.singlestep_ignore = true;
#endif
    if (newmsr & PPC_CPU_UNSUPPORTED_MSR_BITS) {
        jitc_error_msr_unsupported_bits(newmsr);
    }
    aCPU.msr = newmsr;
}
