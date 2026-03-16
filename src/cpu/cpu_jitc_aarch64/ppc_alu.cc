/*
 *  PearPC
 *  ppc_alu.cc - AArch64 JIT ALU instruction generation (stub)
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
 *  Stub: ALU opcode generation functions are not yet implemented
 *  for AArch64. All gen functions are routed through ppc_opc_gen_invalid
 *  in ppc_dec.cc, which returns flowEndBlockUnreachable.
 */

#include <cstdlib>
#include "tools/snprintf.h"
#include "jitc.h"
#include "jitc_asm.h"
#include "ppc_cpu.h"

/*
 *  Stub interpreter functions for PPC opcodes not yet implemented.
 *  Each will print the opcode name and abort, so we can track which
 *  opcodes need to be ported next.
 */
void ppc_opc_addcx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_addcx not implemented\n");
    exit(1);
}
void ppc_opc_addex(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_addex not implemented\n");
    exit(1);
}
void ppc_opc_addi(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_addi not implemented\n");
    exit(1);
}
void ppc_opc_addic_(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_addic_ not implemented\n");
    exit(1);
}
void ppc_opc_addic(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_addic not implemented\n");
    exit(1);
}
void ppc_opc_addis(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_addis not implemented\n");
    exit(1);
}
void ppc_opc_addmex(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_addmex not implemented\n");
    exit(1);
}
void ppc_opc_addx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_addx not implemented\n");
    exit(1);
}
void ppc_opc_addzex(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_addzex not implemented\n");
    exit(1);
}
void ppc_opc_andcx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_andcx not implemented\n");
    exit(1);
}
void ppc_opc_andi_(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_andi_ not implemented\n");
    exit(1);
}
void ppc_opc_andis_(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_andis_ not implemented\n");
    exit(1);
}
void ppc_opc_andx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_andx not implemented\n");
    exit(1);
}
void ppc_opc_bcctrx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_bcctrx not implemented\n");
    exit(1);
}
void ppc_opc_bclrx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_bclrx not implemented\n");
    exit(1);
}
void ppc_opc_bcx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_bcx not implemented\n");
    exit(1);
}
void ppc_opc_bx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_bx not implemented\n");
    exit(1);
}
void ppc_opc_cmp(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_cmp not implemented\n");
    exit(1);
}
void ppc_opc_cmpi(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_cmpi not implemented\n");
    exit(1);
}
void ppc_opc_cmpl(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_cmpl not implemented\n");
    exit(1);
}
void ppc_opc_cmpli(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_cmpli not implemented\n");
    exit(1);
}
void ppc_opc_cntlzwx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_cntlzwx not implemented\n");
    exit(1);
}
void ppc_opc_crand(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_crand not implemented\n");
    exit(1);
}
void ppc_opc_crandc(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_crandc not implemented\n");
    exit(1);
}
void ppc_opc_creqv(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_creqv not implemented\n");
    exit(1);
}
void ppc_opc_crnand(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_crnand not implemented\n");
    exit(1);
}
void ppc_opc_crnor(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_crnor not implemented\n");
    exit(1);
}
void ppc_opc_cror(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_cror not implemented\n");
    exit(1);
}
void ppc_opc_crorc(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_crorc not implemented\n");
    exit(1);
}
void ppc_opc_crxor(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_crxor not implemented\n");
    exit(1);
}
void ppc_opc_divwux(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_divwux not implemented\n");
    exit(1);
}
void ppc_opc_divwx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_divwx not implemented\n");
    exit(1);
}
void ppc_opc_eciwx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_eciwx not implemented\n");
    exit(1);
}
void ppc_opc_ecowx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_ecowx not implemented\n");
    exit(1);
}
void ppc_opc_eieio(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_eieio not implemented\n");
    exit(1);
}
void ppc_opc_eqvx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_eqvx not implemented\n");
    exit(1);
}
void ppc_opc_extsbx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_extsbx not implemented\n");
    exit(1);
}
void ppc_opc_extshx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_extshx not implemented\n");
    exit(1);
}
void ppc_opc_fabsx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fabsx not implemented\n");
    exit(1);
}
void ppc_opc_faddsx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_faddsx not implemented\n");
    exit(1);
}
void ppc_opc_faddx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_faddx not implemented\n");
    exit(1);
}
void ppc_opc_fcmpo(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fcmpo not implemented\n");
    exit(1);
}
void ppc_opc_fcmpu(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fcmpu not implemented\n");
    exit(1);
}
void ppc_opc_fctiwx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fctiwx not implemented\n");
    exit(1);
}
void ppc_opc_fctiwzx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fctiwzx not implemented\n");
    exit(1);
}
void ppc_opc_fdivsx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fdivsx not implemented\n");
    exit(1);
}
void ppc_opc_fdivx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fdivx not implemented\n");
    exit(1);
}
void ppc_opc_fmaddsx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fmaddsx not implemented\n");
    exit(1);
}
void ppc_opc_fmaddx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fmaddx not implemented\n");
    exit(1);
}
void ppc_opc_fmrx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fmrx not implemented\n");
    exit(1);
}
void ppc_opc_fmsubsx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fmsubsx not implemented\n");
    exit(1);
}
void ppc_opc_fmsubx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fmsubx not implemented\n");
    exit(1);
}
void ppc_opc_fmulsx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fmulsx not implemented\n");
    exit(1);
}
void ppc_opc_fmulx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fmulx not implemented\n");
    exit(1);
}
void ppc_opc_fnabsx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fnabsx not implemented\n");
    exit(1);
}
void ppc_opc_fnegx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fnegx not implemented\n");
    exit(1);
}
void ppc_opc_fnmaddsx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fnmaddsx not implemented\n");
    exit(1);
}
void ppc_opc_fnmaddx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fnmaddx not implemented\n");
    exit(1);
}
void ppc_opc_fnmsubsx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fnmsubsx not implemented\n");
    exit(1);
}
void ppc_opc_fnmsubx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fnmsubx not implemented\n");
    exit(1);
}
void ppc_opc_fresx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fresx not implemented\n");
    exit(1);
}
void ppc_opc_frspx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_frspx not implemented\n");
    exit(1);
}
void ppc_opc_frsqrtex(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_frsqrtex not implemented\n");
    exit(1);
}
void ppc_opc_fselx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fselx not implemented\n");
    exit(1);
}
void ppc_opc_fsqrtsx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fsqrtsx not implemented\n");
    exit(1);
}
void ppc_opc_fsqrtx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fsqrtx not implemented\n");
    exit(1);
}
void ppc_opc_fsubsx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fsubsx not implemented\n");
    exit(1);
}
void ppc_opc_fsubx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_fsubx not implemented\n");
    exit(1);
}
void ppc_opc_icbi(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_icbi not implemented\n");
    exit(1);
}
void ppc_opc_isync(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_isync not implemented\n");
    exit(1);
}
void ppc_opc_mcrf(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mcrf not implemented\n");
    exit(1);
}
void ppc_opc_mcrfs(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mcrfs not implemented\n");
    exit(1);
}
void ppc_opc_mcrxr(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mcrxr not implemented\n");
    exit(1);
}
void ppc_opc_mfcr(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mfcr not implemented\n");
    exit(1);
}
void ppc_opc_mffsx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mffsx not implemented\n");
    exit(1);
}
void ppc_opc_mfmsr(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mfmsr not implemented\n");
    exit(1);
}
void ppc_opc_mfspr(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mfspr not implemented\n");
    exit(1);
}
void ppc_opc_mfsr(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mfsr not implemented\n");
    exit(1);
}
void ppc_opc_mfsrin(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mfsrin not implemented\n");
    exit(1);
}
void ppc_opc_mftb(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mftb not implemented\n");
    exit(1);
}
void ppc_opc_mtcrf(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mtcrf not implemented\n");
    exit(1);
}
void ppc_opc_mtfsb0x(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mtfsb0x not implemented\n");
    exit(1);
}
void ppc_opc_mtfsb1x(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mtfsb1x not implemented\n");
    exit(1);
}
void ppc_opc_mtfsfix(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mtfsfix not implemented\n");
    exit(1);
}
void ppc_opc_mtfsfx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mtfsfx not implemented\n");
    exit(1);
}
void ppc_opc_mtmsr(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mtmsr not implemented\n");
    exit(1);
}
void ppc_opc_mtspr(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mtspr not implemented\n");
    exit(1);
}
void ppc_opc_mtsr(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mtsr not implemented\n");
    exit(1);
}
void ppc_opc_mtsrin(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mtsrin not implemented\n");
    exit(1);
}
void ppc_opc_mulhwux(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mulhwux not implemented\n");
    exit(1);
}
void ppc_opc_mulhwx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mulhwx not implemented\n");
    exit(1);
}
void ppc_opc_mulli(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mulli not implemented\n");
    exit(1);
}
void ppc_opc_mullwx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_mullwx not implemented\n");
    exit(1);
}
void ppc_opc_nandx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_nandx not implemented\n");
    exit(1);
}
void ppc_opc_negx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_negx not implemented\n");
    exit(1);
}
void ppc_opc_norx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_norx not implemented\n");
    exit(1);
}
void ppc_opc_orcx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_orcx not implemented\n");
    exit(1);
}
void ppc_opc_ori(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_ori not implemented\n");
    exit(1);
}
void ppc_opc_oris(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_oris not implemented\n");
    exit(1);
}
void ppc_opc_orx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_orx not implemented\n");
    exit(1);
}
void ppc_opc_rfi(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_rfi not implemented\n");
    exit(1);
}
void ppc_opc_rlwimix(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_rlwimix not implemented\n");
    exit(1);
}
void ppc_opc_rlwinmx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_rlwinmx not implemented\n");
    exit(1);
}
void ppc_opc_rlwnmx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_rlwnmx not implemented\n");
    exit(1);
}
void ppc_opc_sc(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_sc not implemented\n");
    exit(1);
}
void ppc_opc_slwx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_slwx not implemented\n");
    exit(1);
}
void ppc_opc_srawix(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_srawix not implemented\n");
    exit(1);
}
void ppc_opc_srawx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_srawx not implemented\n");
    exit(1);
}
void ppc_opc_srwx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_srwx not implemented\n");
    exit(1);
}
void ppc_opc_subfcx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_subfcx not implemented\n");
    exit(1);
}
void ppc_opc_subfex(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_subfex not implemented\n");
    exit(1);
}
void ppc_opc_subfic(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_subfic not implemented\n");
    exit(1);
}
void ppc_opc_subfmex(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_subfmex not implemented\n");
    exit(1);
}
void ppc_opc_subfx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_subfx not implemented\n");
    exit(1);
}
void ppc_opc_subfzex(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_subfzex not implemented\n");
    exit(1);
}
void ppc_opc_sync(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_sync not implemented\n");
    exit(1);
}
void ppc_opc_tlbia(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_tlbia not implemented\n");
    exit(1);
}
void ppc_opc_tlbie(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_tlbie not implemented\n");
    exit(1);
}
void ppc_opc_tlbsync(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_tlbsync not implemented\n");
    exit(1);
}
void ppc_opc_tw(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_tw not implemented\n");
    exit(1);
}
void ppc_opc_twi(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_twi not implemented\n");
    exit(1);
}
void ppc_opc_xori(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_xori not implemented\n");
    exit(1);
}
void ppc_opc_xoris(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_xoris not implemented\n");
    exit(1);
}
void ppc_opc_xorx(PPC_CPU_State &aCPU)
{
    ht_printf("FATAL: ppc_opc_xorx not implemented\n");
    exit(1);
}
