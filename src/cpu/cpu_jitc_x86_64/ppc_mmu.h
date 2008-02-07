/*
 *	PearPC
 *	ppc_mmu.h
 *
 *	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
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

#ifndef __PPC_MMU_H__
#define __PPC_MMU_H__

#include "system/types.h"

extern byte *gMemory;
extern uint32 gMemorySize;

#define PPC_MMU_READ  1
#define PPC_MMU_WRITE 2
#define PPC_MMU_CODE  4
#define PPC_MMU_SV    8
#define PPC_MMU_NO_EXC 16

#define PPC_MMU_OK 0
#define PPC_MMU_EXC 1
#define PPC_MMU_FATAL 2

int FASTCALL ppc_effective_to_physical(PPC_CPU_State &aCPU, uint32 addr, int flags, uint32 &result);
int FASTCALL ppc_effective_to_physical_vm(PPC_CPU_State &aCPU, uint32 addr, int flags, uint32 &result);
bool FASTCALL ppc_mmu_set_sdr1(PPC_CPU_State &aCPU, uint32 newval, bool quiesce);
void ppc_mmu_tlb_invalidate(PPC_CPU_State &aCPU);

int FASTCALL ppc_read_physical_dword(uint32 addr, uint64 &result);
int FASTCALL ppc_read_physical_word(uint32 addr, uint32 &result);
int FASTCALL ppc_read_physical_half(uint32 addr, uint16 &result);
int FASTCALL ppc_read_physical_byte(uint32 addr, uint8 &result);
 
int FASTCALL ppc_read_effective_code(PPC_CPU_State &aCPU, uint32 addr, uint32 &result);
int FASTCALL ppc_read_effective_dword(PPC_CPU_State &aCPU, uint32 addr, uint64 &result);
int FASTCALL ppc_read_effective_word(PPC_CPU_State &aCPU, uint32 addr, uint32 &result);
int FASTCALL ppc_read_effective_half(PPC_CPU_State &aCPU, uint32 addr, uint16 &result);
int FASTCALL ppc_read_effective_byte(PPC_CPU_State &aCPU, uint32 addr, uint8 &result);

int FASTCALL ppc_write_physical_dword(uint32 addr, uint64 data);
int FASTCALL ppc_write_physical_word(uint32 addr, uint32 data);
int FASTCALL ppc_write_physical_half(uint32 addr, uint16 data);
int FASTCALL ppc_write_physical_byte(uint32 addr, uint8 data);

int FASTCALL ppc_write_effective_dword(PPC_CPU_State &aCPU, uint32 addr, uint64 data);
int FASTCALL ppc_write_effective_word(PPC_CPU_State &aCPU, uint32 addr, uint32 data);
int FASTCALL ppc_write_effective_half(PPC_CPU_State &aCPU, uint32 addr, uint16 data);
int FASTCALL ppc_write_effective_byte(PPC_CPU_State &aCPU, uint32 addr, uint8 data);

int FASTCALL ppc_direct_physical_memory_handle(uint32 addr, byte *&ptr);
int FASTCALL ppc_direct_effective_memory_handle(PPC_CPU_State &aCPU, uint32 addr, byte *&ptr);
int FASTCALL ppc_direct_effective_memory_handle_code(PPC_CPU_State &aCPU, uint32 addr, byte *&ptr);
bool FASTCALL ppc_mmu_page_create(PPC_CPU_State &aCPU, uint32 ea, uint32 pa);
bool FASTCALL ppc_mmu_page_free(PPC_CPU_State &aCPU, uint32 ea);
bool FASTCALL ppc_init_physical_memory(uint size);

/*
pte: (page table entry)
1st word:
0     V    Valid
1-24  VSID Virtual Segment ID
25    H    Hash function
26-31 API  Abbreviated page index
2nd word:
0-19  RPN  Physical page number
20-22 res
23    R    Referenced bit
24    C    Changed bit
25-28 WIMG Memory/cache control bits
29    res
30-31 PP   Page protection bits
*/

/*
 *	MMU Opcodes
 */
void ppc_opc_dcbz(PPC_CPU_State &aCPU);

void ppc_opc_lbz(PPC_CPU_State &aCPU);
void ppc_opc_lbzu(PPC_CPU_State &aCPU);
void ppc_opc_lbzux(PPC_CPU_State &aCPU);
void ppc_opc_lbzx(PPC_CPU_State &aCPU);
void ppc_opc_lfd(PPC_CPU_State &aCPU);
void ppc_opc_lfdu(PPC_CPU_State &aCPU);
void ppc_opc_lfdux(PPC_CPU_State &aCPU);
void ppc_opc_lfdx(PPC_CPU_State &aCPU);
void ppc_opc_lfs(PPC_CPU_State &aCPU);
void ppc_opc_lfsu(PPC_CPU_State &aCPU);
void ppc_opc_lfsux(PPC_CPU_State &aCPU);
void ppc_opc_lfsx(PPC_CPU_State &aCPU);
void ppc_opc_lha(PPC_CPU_State &aCPU);
void ppc_opc_lhau(PPC_CPU_State &aCPU);
void ppc_opc_lhaux(PPC_CPU_State &aCPU);
void ppc_opc_lhax(PPC_CPU_State &aCPU);
void ppc_opc_lhbrx(PPC_CPU_State &aCPU);
void ppc_opc_lhz(PPC_CPU_State &aCPU);
void ppc_opc_lhzu(PPC_CPU_State &aCPU);
void ppc_opc_lhzux(PPC_CPU_State &aCPU);
void ppc_opc_lhzx(PPC_CPU_State &aCPU);
void ppc_opc_lmw(PPC_CPU_State &aCPU);
void ppc_opc_lswi(PPC_CPU_State &aCPU);
void ppc_opc_lswx(PPC_CPU_State &aCPU);
void ppc_opc_lwarx(PPC_CPU_State &aCPU);
void ppc_opc_lwbrx(PPC_CPU_State &aCPU);
void ppc_opc_lwz(PPC_CPU_State &aCPU);
void ppc_opc_lwzu(PPC_CPU_State &aCPU);
void ppc_opc_lwzux(PPC_CPU_State &aCPU);
void ppc_opc_lwzx(PPC_CPU_State &aCPU);
void ppc_opc_lvx(PPC_CPU_State &aCPU);             /* for altivec support */
void ppc_opc_lvxl(PPC_CPU_State &aCPU);
void ppc_opc_lvebx(PPC_CPU_State &aCPU);
void ppc_opc_lvehx(PPC_CPU_State &aCPU);
void ppc_opc_lvewx(PPC_CPU_State &aCPU);
void ppc_opc_lvsl(PPC_CPU_State &aCPU);
void ppc_opc_lvsr(PPC_CPU_State &aCPU);
void ppc_opc_dst(PPC_CPU_State &aCPU);

void ppc_opc_stb(PPC_CPU_State &aCPU);
void ppc_opc_stbu(PPC_CPU_State &aCPU);
void ppc_opc_stbux(PPC_CPU_State &aCPU);
void ppc_opc_stbx(PPC_CPU_State &aCPU);
void ppc_opc_stfd(PPC_CPU_State &aCPU);
void ppc_opc_stfdu(PPC_CPU_State &aCPU);
void ppc_opc_stfdux(PPC_CPU_State &aCPU);
void ppc_opc_stfdx(PPC_CPU_State &aCPU);
void ppc_opc_stfiwx(PPC_CPU_State &aCPU);
void ppc_opc_stfs(PPC_CPU_State &aCPU);
void ppc_opc_stfsu(PPC_CPU_State &aCPU);
void ppc_opc_stfsux(PPC_CPU_State &aCPU);
void ppc_opc_stfsx(PPC_CPU_State &aCPU);
void ppc_opc_sth(PPC_CPU_State &aCPU);
void ppc_opc_sthbrx(PPC_CPU_State &aCPU);
void ppc_opc_sthu(PPC_CPU_State &aCPU);
void ppc_opc_sthux(PPC_CPU_State &aCPU);
void ppc_opc_sthx(PPC_CPU_State &aCPU);
void ppc_opc_stmw(PPC_CPU_State &aCPU);
void ppc_opc_stswi(PPC_CPU_State &aCPU);
void ppc_opc_stswx(PPC_CPU_State &aCPU);
void ppc_opc_stw(PPC_CPU_State &aCPU);
void ppc_opc_stwbrx(PPC_CPU_State &aCPU);
void ppc_opc_stwcx_(PPC_CPU_State &aCPU);
void ppc_opc_stwu(PPC_CPU_State &aCPU);
void ppc_opc_stwux(PPC_CPU_State &aCPU);
void ppc_opc_stwx(PPC_CPU_State &aCPU);
void ppc_opc_stvx(PPC_CPU_State &aCPU);            /* for altivec support */
void ppc_opc_stvxl(PPC_CPU_State &aCPU);
void ppc_opc_stvebx(PPC_CPU_State &aCPU);
void ppc_opc_stvehx(PPC_CPU_State &aCPU);
void ppc_opc_stvewx(PPC_CPU_State &aCPU);
void ppc_opc_dstst(PPC_CPU_State &aCPU);
void ppc_opc_dss(PPC_CPU_State &aCPU);

#include "jitc_types.h"

JITCFlow ppc_opc_gen_dcbz(JITC &aJITC);

JITCFlow ppc_opc_gen_lbz(JITC &aJITC);
JITCFlow ppc_opc_gen_lbzu(JITC &aJITC);
JITCFlow ppc_opc_gen_lbzux(JITC &aJITC);
JITCFlow ppc_opc_gen_lbzx(JITC &aJITC);
JITCFlow ppc_opc_gen_lfd(JITC &aJITC);
JITCFlow ppc_opc_gen_lfdu(JITC &aJITC);
JITCFlow ppc_opc_gen_lfdux(JITC &aJITC);
JITCFlow ppc_opc_gen_lfdx(JITC &aJITC);
JITCFlow ppc_opc_gen_lfs(JITC &aJITC);
JITCFlow ppc_opc_gen_lfsu(JITC &aJITC);
JITCFlow ppc_opc_gen_lfsux(JITC &aJITC);
JITCFlow ppc_opc_gen_lfsx(JITC &aJITC);
JITCFlow ppc_opc_gen_lha(JITC &aJITC);
JITCFlow ppc_opc_gen_lhau(JITC &aJITC);
JITCFlow ppc_opc_gen_lhaux(JITC &aJITC);
JITCFlow ppc_opc_gen_lhax(JITC &aJITC);
JITCFlow ppc_opc_gen_lhbrx(JITC &aJITC);
JITCFlow ppc_opc_gen_lhz(JITC &aJITC);
JITCFlow ppc_opc_gen_lhzu(JITC &aJITC);
JITCFlow ppc_opc_gen_lhzux(JITC &aJITC);
JITCFlow ppc_opc_gen_lhzx(JITC &aJITC);
JITCFlow ppc_opc_gen_lmw(JITC &aJITC);
JITCFlow ppc_opc_gen_lswi(JITC &aJITC);
JITCFlow ppc_opc_gen_lswx(JITC &aJITC);
JITCFlow ppc_opc_gen_lwarx(JITC &aJITC);
JITCFlow ppc_opc_gen_lwbrx(JITC &aJITC);
JITCFlow ppc_opc_gen_lwz(JITC &aJITC);
JITCFlow ppc_opc_gen_lwzu(JITC &aJITC);
JITCFlow ppc_opc_gen_lwzux(JITC &aJITC);
JITCFlow ppc_opc_gen_lwzx(JITC &aJITC);
JITCFlow ppc_opc_gen_lvx(JITC &aJITC);             /* for altivec support */
JITCFlow ppc_opc_gen_lvxl(JITC &aJITC);
JITCFlow ppc_opc_gen_lvebx(JITC &aJITC);
JITCFlow ppc_opc_gen_lvehx(JITC &aJITC);
JITCFlow ppc_opc_gen_lvewx(JITC &aJITC);
JITCFlow ppc_opc_gen_lvsl(JITC &aJITC);
JITCFlow ppc_opc_gen_lvsr(JITC &aJITC);
JITCFlow ppc_opc_gen_dst(JITC &aJITC);

JITCFlow ppc_opc_gen_stb(JITC &aJITC);
JITCFlow ppc_opc_gen_stbu(JITC &aJITC);
JITCFlow ppc_opc_gen_stbux(JITC &aJITC);
JITCFlow ppc_opc_gen_stbx(JITC &aJITC);
JITCFlow ppc_opc_gen_stfd(JITC &aJITC);
JITCFlow ppc_opc_gen_stfdu(JITC &aJITC);
JITCFlow ppc_opc_gen_stfdux(JITC &aJITC);
JITCFlow ppc_opc_gen_stfdx(JITC &aJITC);
JITCFlow ppc_opc_gen_stfiwx(JITC &aJITC);
JITCFlow ppc_opc_gen_stfs(JITC &aJITC);
JITCFlow ppc_opc_gen_stfsu(JITC &aJITC);
JITCFlow ppc_opc_gen_stfsux(JITC &aJITC);
JITCFlow ppc_opc_gen_stfsx(JITC &aJITC);
JITCFlow ppc_opc_gen_sth(JITC &aJITC);
JITCFlow ppc_opc_gen_sthbrx(JITC &aJITC);
JITCFlow ppc_opc_gen_sthu(JITC &aJITC);
JITCFlow ppc_opc_gen_sthux(JITC &aJITC);
JITCFlow ppc_opc_gen_sthx(JITC &aJITC);
JITCFlow ppc_opc_gen_stmw(JITC &aJITC);
JITCFlow ppc_opc_gen_stswi(JITC &aJITC);
JITCFlow ppc_opc_gen_stswx(JITC &aJITC);
JITCFlow ppc_opc_gen_stw(JITC &aJITC);
JITCFlow ppc_opc_gen_stwbrx(JITC &aJITC);
JITCFlow ppc_opc_gen_stwcx_(JITC &aJITC);
JITCFlow ppc_opc_gen_stwu(JITC &aJITC);
JITCFlow ppc_opc_gen_stwux(JITC &aJITC);
JITCFlow ppc_opc_gen_stwx(JITC &aJITC);
JITCFlow ppc_opc_gen_stvx(JITC &aJITC);            /* for altivec support */
JITCFlow ppc_opc_gen_stvxl(JITC &aJITC);
JITCFlow ppc_opc_gen_stvebx(JITC &aJITC);
JITCFlow ppc_opc_gen_stvehx(JITC &aJITC);
JITCFlow ppc_opc_gen_stvewx(JITC &aJITC);
JITCFlow ppc_opc_gen_dstst(JITC &aJITC);
JITCFlow ppc_opc_gen_dss(JITC &aJITC);

#endif

