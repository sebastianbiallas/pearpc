/*
 *	PearPC
 *	ppc_mmu.cc
 *
 *	Copyright (C) 2026 Sebastian Biallas (sb@biallas.net)
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

/*
 *	AArch64 JIT MMU support.
 *
 *	The interpreter MMU functions are platform-independent and identical
 *	to the x86_64 version. JIT code generation functions (_gen_) are
 *	stubbed out - they are routed through ppc_opc_gen_invalid in ppc_dec.cc
 *	which returns flowEndBlockUnreachable.
 */

#include <cstdlib>
#include <cstring>
#include "tools/snprintf.h"
#include "debug/tracers.h"
#include "io/prom/prom.h"
#include "io/io.h"
#include "ppc_cpu.h"
#include "ppc_fpu.h"
#include "ppc_vec.h"
#include "ppc_mmu.h"
#include "ppc_exc.h"
#include "ppc_tools.h"

#include "jitc.h"
#include "jitc_asm.h"
#include "aarch64asm.h"
#include "ppc_dec.h"
#include "ppc_opc.h"

extern PPC_CPU_State *gCPU;
extern FILE *gTraceLog;

extern "C" void crash_dump_cpu_state()
{
    if (gCPU) {
        fprintf(stderr, "  PPC state: pc=%08x npc=%08x lr=%08x ctr=%08x cr=%08x msr=%08x\n", gCPU->pc, gCPU->npc,
                gCPU->lr, gCPU->ctr, gCPU->cr, gCPU->msr);
        fprintf(stderr, "  srr0=%08x srr1=%08x current_opc=%08x code_base=%08x\n", gCPU->srr[0], gCPU->srr[1],
                gCPU->current_opc, gCPU->current_code_base);
        fprintf(stderr, "  gpr: r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x r9=%08x\n", gCPU->gpr[0], gCPU->gpr[1],
                gCPU->gpr[2], gCPU->gpr[3], gCPU->gpr[4], gCPU->gpr[5], gCPU->gpr[9]);
    }
}

void jitc_dump_and_exit(int code)
{
    extern byte *gMemory;
    extern uint32 gMemorySize;
    if (gTraceLog) {
        fflush(gTraceLog);
    }
    FILE *df = fopen("memdump_jit.bin", "wb");
    if (df) {
        fwrite(gMemory, 1, gMemorySize, df);
        fclose(df);
        fprintf(stderr, "[DUMP] wrote memdump_jit.bin (%u bytes)\n", gMemorySize);
    }
    exit(code);
}

extern "C" void jitc_fatal_gpr9_corrupt(PPC_CPU_State *cpu)
{
    fprintf(stderr, "[FATAL-GPR9] gpr[9]=%08x at pc=%08x ccb=%08x pc_ofs=%08x opc=%08x\n", cpu->gpr[9], cpu->pc,
            cpu->current_code_base, cpu->pc_ofs, cpu->current_opc);
    fprintf(stderr, "  npc=%08x lr=%08x msr=%08x r0=%08x r1=%08x r10=%08x\n", cpu->npc, cpu->lr, cpu->msr, cpu->gpr[0],
            cpu->gpr[1], cpu->gpr[10]);
    fprintf(stderr, "  temp=%08x temp2=%08x\n", cpu->temp, cpu->temp2);
    if (gTraceLog) {
        fflush(gTraceLog);
    }
    PPC_CPU_ERR("gpr[9] corrupt: %08x at pc=%08x\n", cpu->gpr[9], cpu->pc);
}


/*
 *  C wrapper for ppc_effective_to_physical_code.
 *  Called from jitc_mmu.S assembly.
 *  Returns the physical address, or aborts on fatal MMU error.
 */
extern "C" uint32 ppc_effective_to_physical_code_c(PPC_CPU_State *cpu, uint32 ea)
{
    uint32 pa;
    int r = ppc_effective_to_physical(*cpu, ea, PPC_MMU_READ | PPC_MMU_CODE, pa);
    if (r == PPC_MMU_OK) {
        return pa;
    }
    if (r == PPC_MMU_EXC) {
        // ppc_exception() already set up SRR0/SRR1, cleared MSR,
        // invalidated TLB, and set npc = ISI vector.
        // Return 0 to signal ISI to the caller.
        return 0;
    }
    // PPC_MMU_FATAL — should not happen since ppc_effective_to_physical
    // now calls ppc_exception, but handle defensively.
    PPC_MMU_ERR("ISI FATAL for EA %08x (r=%d)\n", ea, r);
    return 0;
}

byte *gMemory = NULL;
uint32 gMemorySize;

/*
 *  TLB slow-path helpers for JIT memory access stubs (jitc_mmu.S).
 *
 *  Called on TLB miss. Each helper:
 *  1. Translates EA → PA via ppc_effective_to_physical()
 *  2. Fills the TLB for the accessed page (RAM only, not IO)
 *  3. Performs the actual read/write
 *
 *  On MMU failure: returns nonzero. ppc_exception() has already set
 *  DAR, DSISR, SRR0/1, MSR, npc. The asm stub dispatches to npc.
 */


static inline void tlb_fill_data_read(PPC_CPU_State *cpu, uint32 ea, uint32 pa)
{
    uint32 pa_page = pa & 0xFFFFF000;
    if (pa_page + 0x1000 <= gMemorySize) {
        uint32 idx = (ea >> 12) & (TLB_ENTRIES - 1);
        cpu->tlb_data_read_eff[idx] = ea & 0xFFFFF000;
        cpu->tlb_data_read_phys[idx] = (uint64)(gMemory + pa_page);
    }
}

static inline void tlb_fill_data_write(PPC_CPU_State *cpu, uint32 ea, uint32 pa)
{
    uint32 pa_page = pa & 0xFFFFF000;
    if (pa_page + 0x1000 <= gMemorySize) {
        uint32 idx = (ea >> 12) & (TLB_ENTRIES - 1);
        cpu->tlb_data_write_eff[idx] = ea & 0xFFFFF000;
        cpu->tlb_data_write_phys[idx] = (uint64)(gMemory + pa_page);
    }
}

extern "C" int ppc_read_effective_byte_slow(PPC_CPU_State *cpu, uint32 ea)
{
    uint32 pa;
    if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_READ, pa) == PPC_MMU_OK) {
        tlb_fill_data_read(cpu, ea, pa);
        uint8 result;
        ppc_read_physical_byte(pa, result);
        cpu->temp = result;
        return 0;
    }
    // ppc_exception() already set up DSI (DAR, DSISR, SRR0/1, MSR, npc)
    return 1;
}

extern "C" int ppc_read_effective_half_z_slow(PPC_CPU_State *cpu, uint32 ea)
{
    if ((ea & 0xFFF) > 0xFFE) {
        // Access spans two pages — read byte-by-byte
        uint32 result = 0;
        for (int i = 0; i < 2; i++) {
            uint32 pa;
            if (ppc_effective_to_physical(*cpu, ea + i, PPC_MMU_READ, pa) != PPC_MMU_OK) {
                return 1;
            }
            uint8 b;
            ppc_read_physical_byte(pa, b);
            result = (result << 8) | b;
        }
        cpu->temp = result;
        return 0;
    }
    uint32 pa;
    if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_READ, pa) == PPC_MMU_OK) {
        tlb_fill_data_read(cpu, ea, pa);
        uint16 result;
        ppc_read_physical_half(pa, result);
        cpu->temp = result;
        return 0;
    }
    return 1;
}

extern "C" int ppc_read_effective_word_slow(PPC_CPU_State *cpu, uint32 ea)
{
    if ((ea & 0xFFF) > 0xFFC) {
        // Access spans two pages — read byte-by-byte
        uint32 result = 0;
        for (int i = 0; i < 4; i++) {
            uint32 pa;
            if (ppc_effective_to_physical(*cpu, ea + i, PPC_MMU_READ, pa) != PPC_MMU_OK) {
                return 1;
            }
            uint8 b;
            ppc_read_physical_byte(pa, b);
            result = (result << 8) | b;
        }
        cpu->temp = result;
        return 0;
    }
    uint32 pa;
    if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_READ, pa) == PPC_MMU_OK) {
        tlb_fill_data_read(cpu, ea, pa);
        uint32 result;
        ppc_read_physical_word(pa, result);
        cpu->temp = result;
        return 0;
    }
    return 1;
}

extern "C" int ppc_read_effective_dword_slow(PPC_CPU_State *cpu, uint32 ea)
{
    if ((ea & 0xFFF) > 0xFF8) {
        // Access spans two pages — read byte-by-byte
        uint64 result = 0;
        for (int i = 0; i < 8; i++) {
            uint32 pa;
            if (ppc_effective_to_physical(*cpu, ea + i, PPC_MMU_READ, pa) != PPC_MMU_OK) {
                return 1;
            }
            uint8 b;
            ppc_read_physical_byte(pa, b);
            result = (result << 8) | b;
        }
        cpu->temp = (uint32)(result >> 32);
        cpu->temp2 = (uint32)(result);
        return 0;
    }
    uint32 pa;
    if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_READ, pa) == PPC_MMU_OK) {
        tlb_fill_data_read(cpu, ea, pa);
        uint64 result;
        ppc_read_physical_dword(pa, result);
        cpu->temp = (uint32)(result >> 32);
        cpu->temp2 = (uint32)(result);
        return 0;
    }
    return 1;
}

extern "C" int ppc_write_effective_byte_slow(PPC_CPU_State *cpu, uint32 ea, uint32 data)
{
    uint32 pa;
    if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_WRITE, pa) == PPC_MMU_OK) {
        tlb_fill_data_write(cpu, ea, pa);
        ppc_write_physical_byte(pa, data);
        return 0;
    }
    // ppc_exception() already set up DSI (DAR, DSISR, SRR0/1, MSR, npc)
    return 1;
}

extern "C" int ppc_write_effective_half_slow(PPC_CPU_State *cpu, uint32 ea, uint32 data)
{
    if ((ea & 0xFFF) > 0xFFE) {
        // Access spans two pages — write byte-by-byte (big-endian order)
        for (int i = 0; i < 2; i++) {
            uint32 pa;
            if (ppc_effective_to_physical(*cpu, ea + i, PPC_MMU_WRITE, pa) != PPC_MMU_OK) {
                return 1;
            }
            ppc_write_physical_byte(pa, (data >> (8 * (1 - i))) & 0xFF);
        }
        return 0;
    }
    uint32 pa;
    if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_WRITE, pa) == PPC_MMU_OK) {
        tlb_fill_data_write(cpu, ea, pa);
        ppc_write_physical_half(pa, data);
        return 0;
    }
    return 1;
}

extern "C" int ppc_write_effective_word_slow(PPC_CPU_State *cpu, uint32 ea, uint32 data)
{
    if ((ea & 0xFFF) > 0xFFC) {
        // Access spans two pages — write byte-by-byte (big-endian order)
        for (int i = 0; i < 4; i++) {
            uint32 pa;
            if (ppc_effective_to_physical(*cpu, ea + i, PPC_MMU_WRITE, pa) != PPC_MMU_OK) {
                return 1;
            }
            ppc_write_physical_byte(pa, (data >> (8 * (3 - i))) & 0xFF);
        }
        return 0;
    }
    uint32 pa;
    if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_WRITE, pa) == PPC_MMU_OK) {
        tlb_fill_data_write(cpu, ea, pa);
        ppc_write_physical_word(pa, data);
        return 0;
    }
    return 1;
}

extern "C" int ppc_write_effective_dword_slow(PPC_CPU_State *cpu, uint32 ea, uint64 data)
{
    if ((ea & 0xFFF) > 0xFF8) {
        // Access spans two pages — write byte-by-byte (big-endian order)
        for (int i = 0; i < 8; i++) {
            uint32 pa;
            if (ppc_effective_to_physical(*cpu, ea + i, PPC_MMU_WRITE, pa) != PPC_MMU_OK) {
                return 1;
            }
            ppc_write_physical_byte(pa, (data >> (8 * (7 - i))) & 0xFF);
        }
        return 0;
    }
    uint32 pa;
    if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_WRITE, pa) == PPC_MMU_OK) {
        tlb_fill_data_write(cpu, ea, pa);
        ppc_write_physical_dword(pa, data);
        return 0;
    }
    return 1;
}

#undef TLB

static int ppc_pte_protection[] = {
    1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0,
};

static int e2p_trace = 0;

int FASTCALL ppc_effective_to_physical(PPC_CPU_State &aCPU, uint32 addr, int flags, uint32 &result)
{
    bool trace = false;

    if (flags & PPC_MMU_CODE) {
        if (!(aCPU.msr & MSR_IR)) {
            result = addr;
            return PPC_MMU_OK;
        }
        for (int i = 0; i < 4; i++) {
            if ((addr & aCPU.ibat_bl[i]) == aCPU.ibat_bepi[i]) {
                if (((aCPU.ibatu[i] & BATU_Vs) && !(aCPU.msr & MSR_PR)) ||
                    ((aCPU.ibatu[i] & BATU_Vp) && (aCPU.msr & MSR_PR))) {
                    addr &= aCPU.ibat_nbl[i];
                    addr |= aCPU.ibat_brpn[i];
                    result = addr;
                    if (trace) {
                        e2p_trace++;
                        fprintf(stderr, "[E2P] ea=%08x → IBAT%d → pa=%08x\n", result, i, result);
                    }
                    return PPC_MMU_OK;
                }
            }
        }
        if (trace) {
            fprintf(stderr, "[E2P] ea=%08x CODE: no IBAT match, trying page table (msr=%08x)\n", addr, aCPU.msr);
        }
    } else {
        if (!(aCPU.msr & MSR_DR)) {
            result = addr;
            return PPC_MMU_OK;
        }
        for (int i = 0; i < 4; i++) {
            if ((addr & aCPU.dbat_bl[i]) == aCPU.dbat_bepi[i]) {
                if (((aCPU.dbatu[i] & BATU_Vs) && !(aCPU.msr & MSR_PR)) ||
                    ((aCPU.dbatu[i] & BATU_Vp) && (aCPU.msr & MSR_PR))) {
                    addr &= aCPU.dbat_nbl[i];
                    addr |= aCPU.dbat_brpn[i];
                    result = addr;
                    return PPC_MMU_OK;
                }
            }
        }
    }

    uint32 sr = aCPU.sr[EA_SR(addr)];
    if (trace) {
        e2p_trace++;
        fprintf(stderr, "[E2P] ea=%08x sr[%d]=%08x VSID=%06x flags=%x pagetable=%08x mask=%08x\n", addr, EA_SR(addr),
                sr, SR_VSID(sr), flags, aCPU.pagetable_base, aCPU.pagetable_hashmask);
    }
    if (sr & SR_T) {
        PPC_MMU_ERR("sr & T\n");
    } else {
        if ((flags & PPC_MMU_CODE) && (sr & SR_N)) {
            if (trace) {
                fprintf(stderr, "[E2P] ea=%08x FATAL: SR_N set\n", addr);
            }
            return PPC_MMU_FATAL;
        }
        uint32 offset = EA_Offset(addr);
        uint32 page_index = EA_PageIndex(addr);
        uint32 VSID = SR_VSID(sr);
        uint32 api = EA_API(addr);

        uint32 hash1 = (VSID ^ page_index);
        uint32 pteg_addr = ((hash1 & aCPU.pagetable_hashmask) << 6) | aCPU.pagetable_base;
        if (trace) {
            fprintf(stderr, "[E2P] hash1=%08x pteg=%08x api=%x\n", hash1, pteg_addr, api);
        }
        for (int i = 0; i < 8; i++) {
            uint32 pte;
            if (ppc_read_physical_word(pteg_addr, pte)) {
                if (!(flags & PPC_MMU_NO_EXC)) {
                    return PPC_MMU_EXC;
                }
                return PPC_MMU_FATAL;
            }
            if ((pte & PTE1_V) && (!(pte & PTE1_H))) {
                if (VSID == PTE1_VSID(pte) && (api == PTE1_API(pte))) {
                    if (ppc_read_physical_word(pteg_addr + 4, pte)) {
                        return PPC_MMU_FATAL;
                    }
                    int key;
                    if (aCPU.msr & MSR_PR) {
                        key = (sr & SR_Kp) ? 4 : 0;
                    } else {
                        key = (sr & SR_Ks) ? 4 : 0;
                    }
                    if (!ppc_pte_protection[((flags & PPC_MMU_WRITE) ? 8 : 0) + key + PTE2_PP(pte)]) {
                        if (!(flags & PPC_MMU_NO_EXC)) {
                            if (flags & PPC_MMU_CODE) {
                                ppc_exception(aCPU, PPC_EXC_ISI, PPC_EXC_SRR1_PROT, addr);
                            } else if (flags & PPC_MMU_WRITE) {
                                ppc_exception(aCPU, PPC_EXC_DSI, PPC_EXC_DSISR_PROT | PPC_EXC_DSISR_STORE, addr);
                            } else {
                                ppc_exception(aCPU, PPC_EXC_DSI, PPC_EXC_DSISR_PROT, addr);
                            }
                            return PPC_MMU_EXC;
                        }
                        return PPC_MMU_FATAL;
                    }
                    uint32 pap = PTE2_RPN(pte);
                    result = pap | offset;
                    if (flags & PPC_MMU_WRITE) {
                        pte |= PTE2_C | PTE2_R;
                    } else {
                        pte |= PTE2_R;
                    }
                    ppc_write_physical_word(pteg_addr + 4, pte);
                    return PPC_MMU_OK;
                }
            }
            pteg_addr += 8;
        }

        hash1 = ~hash1;
        pteg_addr = ((hash1 & aCPU.pagetable_hashmask) << 6) | aCPU.pagetable_base;
        for (int i = 0; i < 8; i++) {
            uint32 pte;
            if (ppc_read_physical_word(pteg_addr, pte)) {
                if (!(flags & PPC_MMU_NO_EXC)) {
                    return PPC_MMU_EXC;
                }
                return PPC_MMU_FATAL;
            }
            if ((pte & PTE1_V) && (pte & PTE1_H)) {
                if (VSID == PTE1_VSID(pte) && (api == PTE1_API(pte))) {
                    if (ppc_read_physical_word(pteg_addr + 4, pte)) {
                        return PPC_MMU_FATAL;
                    }
                    int key;
                    if (aCPU.msr & MSR_PR) {
                        key = (sr & SR_Kp) ? 4 : 0;
                    } else {
                        key = (sr & SR_Ks) ? 4 : 0;
                    }
                    if (!ppc_pte_protection[((flags & PPC_MMU_WRITE) ? 8 : 0) + key + PTE2_PP(pte)]) {
                        if (!(flags & PPC_MMU_NO_EXC)) {
                            if (flags & PPC_MMU_CODE) {
                                ppc_exception(aCPU, PPC_EXC_ISI, PPC_EXC_SRR1_PROT, addr);
                            } else if (flags & PPC_MMU_WRITE) {
                                ppc_exception(aCPU, PPC_EXC_DSI, PPC_EXC_DSISR_PROT | PPC_EXC_DSISR_STORE, addr);
                            } else {
                                ppc_exception(aCPU, PPC_EXC_DSI, PPC_EXC_DSISR_PROT, addr);
                            }
                            return PPC_MMU_EXC;
                        }
                        return PPC_MMU_FATAL;
                    }
                    uint32 pap = PTE2_RPN(pte);
                    result = pap | offset;
                    if (flags & PPC_MMU_WRITE) {
                        pte |= PTE2_C | PTE2_R;
                    } else {
                        pte |= PTE2_R;
                    }
                    ppc_write_physical_word(pteg_addr + 4, pte);
                    return PPC_MMU_OK;
                }
            }
            pteg_addr += 8;
        }
    }
    // Page not found — raise exception (matching generic CPU behavior)
    if (!(flags & PPC_MMU_NO_EXC)) {
        if (flags & PPC_MMU_CODE) {
            ppc_exception(aCPU, PPC_EXC_ISI, PPC_EXC_SRR1_PAGE);
        } else {
            if (flags & PPC_MMU_WRITE) {
                ppc_exception(aCPU, PPC_EXC_DSI, PPC_EXC_DSISR_PAGE | PPC_EXC_DSISR_STORE, addr);
            } else {
                ppc_exception(aCPU, PPC_EXC_DSI, PPC_EXC_DSISR_PAGE, addr);
            }
        }
        return PPC_MMU_EXC;
    }
    return PPC_MMU_FATAL;
}

int FASTCALL ppc_effective_to_physical_vm(PPC_CPU_State &aCPU, uint32 addr, int flags, uint32 &result)
{
    return ppc_effective_to_physical(aCPU, addr, flags, result);
}

void ppc_mmu_tlb_invalidate(PPC_CPU_State &aCPU)
{
    ppc_mmu_tlb_invalidate_all_asm(&aCPU);
}

bool FASTCALL ppc_mmu_set_sdr1(PPC_CPU_State &aCPU, uint32 newval, bool quiesce)
{
    aCPU.sdr1 = newval;
    aCPU.pagetable_base = newval & 0xffff0000;
    aCPU.pagetable_hashmask = ((newval & 0x1ff) << 10) | 0x3ff;
    ppc_mmu_tlb_invalidate(aCPU);
    return true;
}

int FASTCALL ppc_direct_physical_memory_handle(uint32 addr, byte *&ptr)
{
    if (addr < gMemorySize) {
        ptr = &gMemory[addr];
        return PPC_MMU_OK;
    }
    return PPC_MMU_FATAL;
}

int FASTCALL ppc_direct_effective_memory_handle(PPC_CPU_State &aCPU, uint32 addr, byte *&ptr)
{
    uint32 ea;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ, ea)))) {
        return ppc_direct_physical_memory_handle(ea, ptr);
    }
    return r;
}

int FASTCALL ppc_direct_effective_memory_handle_code(PPC_CPU_State &aCPU, uint32 addr, byte *&ptr)
{
    uint32 ea;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ | PPC_MMU_CODE, ea)))) {
        return ppc_direct_physical_memory_handle(ea, ptr);
    }
    return r;
}

int FASTCALL ppc_read_physical_dword(uint32 addr, uint64 &result)
{
    if (addr < gMemorySize - 7) {
        result = ppc_dword_from_BE(*((uint64 *)&gMemory[addr]));
        return PPC_MMU_OK;
    }
    return PPC_MMU_FATAL;
}

int FASTCALL ppc_read_physical_word(uint32 addr, uint32 &result)
{
    if (addr < gMemorySize) {
        result = ppc_word_from_BE(*((uint32 *)(gMemory + addr)));
        // Trace reads from jiffies (PA 0025abe4)
        if (addr >= 0x0025abe0 && addr <= 0x0025abe8) {
            extern PPC_CPU_State *gCPU;
            static int readTraceCount = 0;
            if (readTraceCount < 200) {
                fprintf(stderr, "[READ-TRACE] PA=%08x result=%08x raw_BE=%08x pc=%08x lr=%08x\n", addr, result,
                        *((uint32 *)(gMemory + addr)), gCPU->pc, gCPU->lr);
                readTraceCount++;
            }
        }
        return PPC_MMU_OK;
    }
    int ret = io_mem_read(addr, result, 4);
    result = ppc_bswap_word(result);
    return ret;
}

int FASTCALL ppc_read_physical_half(uint32 addr, uint16 &result)
{
    if (addr < gMemorySize) {
        result = ppc_half_from_BE(*((uint16 *)(gMemory + addr)));
        return PPC_MMU_OK;
    }
    uint32 r;
    int ret = io_mem_read(addr, r, 2);
    result = ppc_bswap_half(r);
    return ret;
}

int FASTCALL ppc_read_physical_byte(uint32 addr, uint8 &result)
{
    if (addr < gMemorySize) {
        result = gMemory[addr];
        return PPC_MMU_OK;
    }
    uint32 r;
    int ret = io_mem_read(addr, r, 1);
    result = r;
    return ret;
}

int FASTCALL ppc_read_effective_code(PPC_CPU_State &aCPU, uint32 addr, uint32 &result)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ | PPC_MMU_CODE, p)))) {
        return ppc_read_physical_word(p, result);
    }
    return r;
}

int FASTCALL ppc_read_effective_dword(PPC_CPU_State &aCPU, uint32 addr, uint64 &result)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ, p)))) {
        if ((addr & 0xFFF) > 0xFF8) {
            // Access spans two pages — read byte-by-byte
            result = 0;
            for (int i = 0; i < 8; i++) {
                uint32 pa;
                if (ppc_effective_to_physical(aCPU, addr + i, PPC_MMU_READ, pa) != PPC_MMU_OK) {
                    return PPC_MMU_EXC;
                }
                uint8 b;
                ppc_read_physical_byte(pa, b);
                result = (result << 8) | b;
            }
            return PPC_MMU_OK;
        }
        return ppc_read_physical_dword(p, result);
    }
    if (r == PPC_MMU_FATAL) {
        PPC_MMU_ERR("fatal at ea=%08x pc=%08x msr=%08x\n", addr, aCPU.pc, aCPU.msr);
    }
    return r;
}

int FASTCALL ppc_read_effective_word(PPC_CPU_State &aCPU, uint32 addr, uint32 &result)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ, p)))) {
        if ((addr & 0xFFF) > 0xFFC) {
            // Access spans two pages — read byte-by-byte
            result = 0;
            for (int i = 0; i < 4; i++) {
                uint32 pa;
                if (ppc_effective_to_physical(aCPU, addr + i, PPC_MMU_READ, pa) != PPC_MMU_OK) {
                    return PPC_MMU_EXC;
                }
                uint8 b;
                ppc_read_physical_byte(pa, b);
                result = (result << 8) | b;
            }
            return PPC_MMU_OK;
        }
        return ppc_read_physical_word(p, result);
    }
    if (r == PPC_MMU_FATAL) {
        PPC_MMU_ERR("fatal at ea=%08x pc=%08x msr=%08x\n", addr, aCPU.pc, aCPU.msr);
    }
    return r;
}

int FASTCALL ppc_read_effective_half(PPC_CPU_State &aCPU, uint32 addr, uint16 &result)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ, p)))) {
        if ((addr & 0xFFF) > 0xFFE) {
            // Access spans two pages — read byte-by-byte
            uint32 tmp = 0;
            for (int i = 0; i < 2; i++) {
                uint32 pa;
                if (ppc_effective_to_physical(aCPU, addr + i, PPC_MMU_READ, pa) != PPC_MMU_OK) {
                    return PPC_MMU_EXC;
                }
                uint8 b;
                ppc_read_physical_byte(pa, b);
                tmp = (tmp << 8) | b;
            }
            result = (uint16)tmp;
            return PPC_MMU_OK;
        }
        return ppc_read_physical_half(p, result);
    }
    if (r == PPC_MMU_FATAL) {
        PPC_MMU_ERR("fatal at ea=%08x pc=%08x msr=%08x\n", addr, aCPU.pc, aCPU.msr);
    }
    return r;
}

int FASTCALL ppc_read_effective_byte(PPC_CPU_State &aCPU, uint32 addr, uint8 &result)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ, p)))) {
        return ppc_read_physical_byte(p, result);
    }
    if (r == PPC_MMU_FATAL) {
        PPC_MMU_ERR("fatal at ea=%08x pc=%08x msr=%08x\n", addr, aCPU.pc, aCPU.msr);
    }
    return r;
}

int FASTCALL ppc_write_physical_dword(uint32 addr, uint64 data)
{
    if (addr < gMemorySize - 7) {
        *((uint64 *)&gMemory[addr]) = ppc_dword_to_BE(data);
        return PPC_MMU_OK;
    }
    return PPC_MMU_FATAL;
}

int FASTCALL ppc_write_physical_word(uint32 addr, uint32 data)
{
    if (addr < gMemorySize) {
        *((uint32 *)(gMemory + addr)) = ppc_word_to_BE(data);
        return PPC_MMU_OK;
    }
    return io_mem_write(addr, ppc_bswap_word(data), 4);
}

int FASTCALL ppc_write_physical_half(uint32 addr, uint16 data)
{
    if (addr < gMemorySize) {
        *((uint16 *)(gMemory + addr)) = ppc_half_to_BE(data);
        return PPC_MMU_OK;
    }
    return io_mem_write(addr, ppc_bswap_half(data), 2);
}

int FASTCALL ppc_write_physical_byte(uint32 addr, uint8 data)
{
    if (addr < gMemorySize) {
        gMemory[addr] = data;
        return PPC_MMU_OK;
    }
    return io_mem_write(addr, data, 1);
}

int FASTCALL ppc_write_effective_dword(PPC_CPU_State &aCPU, uint32 addr, uint64 data)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_WRITE, p)))) {
        if ((addr & 0xFFF) > 0xFF8) {
            // Access spans two pages — write byte-by-byte (big-endian order)
            for (int i = 0; i < 8; i++) {
                uint32 pa;
                if (ppc_effective_to_physical(aCPU, addr + i, PPC_MMU_WRITE, pa) != PPC_MMU_OK) {
                    return PPC_MMU_EXC;
                }
                ppc_write_physical_byte(pa, (data >> (8 * (7 - i))) & 0xFF);
            }
            return PPC_MMU_OK;
        }
        return ppc_write_physical_dword(p, data);
    }
    if (r == PPC_MMU_FATAL) {
        PPC_MMU_ERR("fatal at ea=%08x pc=%08x msr=%08x\n", addr, aCPU.pc, aCPU.msr);
    }
    return r;
}

int FASTCALL ppc_write_effective_word(PPC_CPU_State &aCPU, uint32 addr, uint32 data)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_WRITE, p)))) {
        if ((addr & 0xFFF) > 0xFFC) {
            // Access spans two pages — write byte-by-byte (big-endian order)
            for (int i = 0; i < 4; i++) {
                uint32 pa;
                if (ppc_effective_to_physical(aCPU, addr + i, PPC_MMU_WRITE, pa) != PPC_MMU_OK) {
                    return PPC_MMU_EXC;
                }
                ppc_write_physical_byte(pa, (data >> (8 * (3 - i))) & 0xFF);
            }
            return PPC_MMU_OK;
        }
        return ppc_write_physical_word(p, data);
    }
    if (r == PPC_MMU_FATAL) {
        PPC_MMU_ERR("fatal at ea=%08x pc=%08x msr=%08x\n", addr, aCPU.pc, aCPU.msr);
    }
    return r;
}

int FASTCALL ppc_write_effective_half(PPC_CPU_State &aCPU, uint32 addr, uint16 data)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_WRITE, p)))) {
        if ((addr & 0xFFF) > 0xFFE) {
            // Access spans two pages — write byte-by-byte (big-endian order)
            for (int i = 0; i < 2; i++) {
                uint32 pa;
                if (ppc_effective_to_physical(aCPU, addr + i, PPC_MMU_WRITE, pa) != PPC_MMU_OK) {
                    return PPC_MMU_EXC;
                }
                ppc_write_physical_byte(pa, (data >> (8 * (1 - i))) & 0xFF);
            }
            return PPC_MMU_OK;
        }
        return ppc_write_physical_half(p, data);
    }
    if (r == PPC_MMU_FATAL) {
        PPC_MMU_ERR("fatal at ea=%08x pc=%08x msr=%08x\n", addr, aCPU.pc, aCPU.msr);
    }
    return r;
}

int FASTCALL ppc_write_effective_byte(PPC_CPU_State &aCPU, uint32 addr, uint8 data)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_WRITE, p)))) {
        return ppc_write_physical_byte(p, data);
    }
    if (r == PPC_MMU_FATAL) {
        PPC_MMU_ERR("fatal at ea=%08x pc=%08x msr=%08x\n", addr, aCPU.pc, aCPU.msr);
    }
    return r;
}

bool FASTCALL ppc_init_physical_memory(uint size)
{
    if (size < 64 * 1024 * 1024) {
        PPC_MMU_ERR("Main memory size must >= 64MB!\n");
    }
    gMemory = (byte *)malloc(size + 4095);
    if ((uint64)gMemory & 0x0fff) {
        gMemory += 4096 - ((uint64)gMemory & 0x0fff);
    }
    printf("&gMemory: %p\n", gMemory);
    if (gMemory == 0) {
        PPC_MMU_ERR("Cannot allocate memory!\n");
    }
    gMemorySize = size;
    return gMemory != NULL;
}

uint32 ppc_get_memory_size()
{
    return gMemorySize;
}

bool ppc_dma_write(uint32 dest, const void *src, uint32 size)
{
    if (dest > gMemorySize || (dest + size) > gMemorySize) {
        return false;
    }
    byte *ptr;
    ppc_direct_physical_memory_handle(dest, ptr);
    memcpy(ptr, src, size);
    return true;
}

bool ppc_dma_read(void *dest, uint32 src, uint32 size)
{
    if (src > gMemorySize || (src + size) > gMemorySize) {
        return false;
    }
    byte *ptr;
    ppc_direct_physical_memory_handle(src, ptr);
    memcpy(dest, ptr, size);
    return true;
}

bool ppc_dma_set(uint32 dest, int c, uint32 size)
{
    if (dest > gMemorySize || (dest + size) > gMemorySize) {
        return false;
    }
    byte *ptr;
    ppc_direct_physical_memory_handle(dest, ptr);
    memset(ptr, c, size);
    return true;
}

extern PPC_CPU_State *gCPU;

bool ppc_prom_set_sdr1(uint32 newval, bool quiesce)
{
    return ppc_mmu_set_sdr1(*gCPU, newval, quiesce);
}

bool ppc_prom_effective_to_physical(uint32 &result, uint32 ea)
{
    return ppc_effective_to_physical(*gCPU, ea, PPC_MMU_READ | PPC_MMU_SV | PPC_MMU_NO_EXC, result) == PPC_MMU_OK;
}

bool ppc_prom_page_create(uint32 ea, uint32 pa)
{
    PPC_CPU_State &aCPU = *gCPU;
    uint32 sr = aCPU.sr[EA_SR(ea)];
    uint32 page_index = EA_PageIndex(ea);
    uint32 VSID = SR_VSID(sr);
    uint32 api = EA_API(ea);
    uint32 hash1 = (VSID ^ page_index);
    uint32 pte, pte2;
    uint32 h = 0;
    for (int j = 0; j < 2; j++) {
        uint32 pteg_addr = ((hash1 & aCPU.pagetable_hashmask) << 6) | aCPU.pagetable_base;
        for (int i = 0; i < 8; i++) {
            if (ppc_read_physical_word(pteg_addr, pte)) {
                return false;
            }
            if (!(pte & PTE1_V)) {
                pte = PTE1_V | (VSID << 7) | h | api;
                pte2 = (PA_RPN(pa) << 12) | 0;
                if (ppc_write_physical_word(pteg_addr, pte) || ppc_write_physical_word(pteg_addr + 4, pte2)) {
                    return false;
                } else {
                    return true;
                }
            }
            pteg_addr += 8;
        }
        hash1 = ~hash1;
        h = PTE1_H;
    }
    return false;
}

bool ppc_prom_page_free(uint32 ea)
{
    return true;
}

/*
 *	MMU Load/Store Opcodes (interpreter)
 *
 *	These are platform-independent interpreter functions.
 *	The JIT code generation functions (ppc_opc_gen_*) for load/store
 *	are all routed through ppc_opc_gen_invalid in ppc_dec.cc.
 *
 *	TODO: Copy the full interpreter load/store opcode implementations
 *	from the x86_64 version. For now, minimal stubs are provided.
 */

int ppc_opc_dcbz(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint32 a = (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB];
    a &= ~0x1f; // align to 32-byte cache line
    if (ppc_write_effective_dword(aCPU, a, 0)) {
        return PPC_MMU_EXC;
    }
    if (ppc_write_effective_dword(aCPU, a + 8, 0)) {
        return PPC_MMU_EXC;
    }
    if (ppc_write_effective_dword(aCPU, a + 16, 0)) {
        return PPC_MMU_EXC;
    }
    if (ppc_write_effective_dword(aCPU, a + 24, 0)) {
        return PPC_MMU_EXC;
    }
    return PPC_MMU_OK;
}
int ppc_opc_dcba(PPC_CPU_State &aCPU)
{
    return 0;
}
int ppc_opc_dcbf(PPC_CPU_State &aCPU)
{
    return 0;
}
int ppc_opc_dcbi(PPC_CPU_State &aCPU)
{
    return 0;
}
int ppc_opc_dcbst(PPC_CPU_State &aCPU)
{
    return 0;
}
int ppc_opc_dcbt(PPC_CPU_State &aCPU)
{
    return 0;
}
int ppc_opc_dcbtst(PPC_CPU_State &aCPU)
{
    return 0;
}

int ppc_opc_lbz(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint8 r;
    int ret = ppc_read_effective_byte(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rD] = r;
    }
    return ret;
}

int ppc_opc_lbzu(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint8 r;
    int ret = ppc_read_effective_byte(aCPU, aCPU.gpr[rA] + imm, r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += imm;
        aCPU.gpr[rD] = r;
    }
    return ret;
}

int ppc_opc_lbzux(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint8 r;
    int ret = ppc_read_effective_byte(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += aCPU.gpr[rB];
        aCPU.gpr[rD] = r;
    }
    return ret;
}

int ppc_opc_lbzx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint8 r;
    int ret = ppc_read_effective_byte(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rD] = r;
    }
    return ret;
}

int ppc_opc_lwz(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint32 r;
    int ret = ppc_read_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rD] = r;
    }
    return ret;
}

int ppc_opc_lwzu(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint32 r;
    int ret = ppc_read_effective_word(aCPU, aCPU.gpr[rA] + imm, r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += imm;
        aCPU.gpr[rD] = r;
    }
    return ret;
}

int ppc_opc_lwzux(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint32 r;
    int ret = ppc_read_effective_word(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += aCPU.gpr[rB];
        aCPU.gpr[rD] = r;
    }
    return ret;
}

int ppc_opc_lwzx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint32 r;
    int ret = ppc_read_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rD] = r;
    }
    return ret;
}

int ppc_opc_lhz(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint16 r;
    int ret = ppc_read_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rD] = r;
    }
    return ret;
}

int ppc_opc_lhzu(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint16 r;
    int ret = ppc_read_effective_half(aCPU, aCPU.gpr[rA] + imm, r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += imm;
        aCPU.gpr[rD] = r;
    }
    return ret;
}
int ppc_opc_lhzux(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint16 r;
    int ret = ppc_read_effective_half(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += aCPU.gpr[rB];
        aCPU.gpr[rD] = r;
    }
    return ret;
}
int ppc_opc_lhzx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint16 r;
    int ret = ppc_read_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rD] = r;
    }
    return ret;
}
int ppc_opc_lha(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint16 r;
    int ret = ppc_read_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rD] = (sint32)(sint16)r;
    }
    return ret;
}
int ppc_opc_lhau(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint16 r;
    int ret = ppc_read_effective_half(aCPU, aCPU.gpr[rA] + imm, r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += imm;
        aCPU.gpr[rD] = (sint32)(sint16)r;
    }
    return ret;
}
int ppc_opc_lhaux(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint16 r;
    int ret = ppc_read_effective_half(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += aCPU.gpr[rB];
        aCPU.gpr[rD] = (sint32)(sint16)r;
    }
    return ret;
}
int ppc_opc_lhax(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint16 r;
    int ret = ppc_read_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rD] = (sint32)(sint16)r;
    }
    return ret;
}
int ppc_opc_lhbrx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint16 r;
    int ret = ppc_read_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rD] = ppc_bswap_half(r);
    }
    return ret;
}
int ppc_opc_lwbrx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint32 r;
    int ret = ppc_read_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rD] = ppc_bswap_word(r);
    }
    return ret;
}
int ppc_opc_lwarx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint32 r;
    int ret = ppc_read_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rD] = r;
        aCPU.reserve = r;
        aCPU.have_reservation = 1;
    }
    return ret;
}
int ppc_opc_lmw(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint32 ea = (rA ? aCPU.gpr[rA] : 0) + imm;
    for (int i = rD; i <= 31; i++) {
        uint32 val;
        int ret = ppc_read_effective_word(aCPU, ea, val);
        if (ret != PPC_MMU_OK) {
            return ret;
        }
        aCPU.gpr[i] = val;
        ea += 4;
    }
    return PPC_MMU_OK;
}
int ppc_opc_lswi(PPC_CPU_State &aCPU)
{
    int rA, rD, NB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, NB);
    if (NB == 0) {
        NB = 32;
    }
    uint32 ea = rA ? aCPU.gpr[rA] : 0;
    uint32 r = 0;
    int i = 4;
    uint8 v;
    while (NB > 0) {
        if (!i) {
            i = 4;
            aCPU.gpr[rD] = r;
            rD++;
            rD %= 32;
            r = 0;
        }
        int ret = ppc_read_effective_byte(aCPU, ea, v);
        if (ret) {
            return ret;
        }
        r <<= 8;
        r |= v;
        ea++;
        i--;
        NB--;
    }
    while (i) {
        r <<= 8;
        i--;
    }
    aCPU.gpr[rD] = r;
    return PPC_MMU_OK;
}
int ppc_opc_lswx(PPC_CPU_State &aCPU)
{
    int rA, rD, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    int NB = XER_n(aCPU.xer);
    uint32 ea = aCPU.gpr[rB] + (rA ? aCPU.gpr[rA] : 0);
    uint32 r = 0;
    int i = 4;
    uint8 v;
    while (NB > 0) {
        if (!i) {
            i = 4;
            aCPU.gpr[rD] = r;
            rD++;
            rD %= 32;
            r = 0;
        }
        int ret = ppc_read_effective_byte(aCPU, ea, v);
        if (ret) {
            return ret;
        }
        r <<= 8;
        r |= v;
        ea++;
        i--;
        NB--;
    }
    while (i) {
        r <<= 8;
        i--;
    }
    aCPU.gpr[rD] = r;
    return PPC_MMU_OK;
}

int ppc_opc_stb(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
    return ppc_write_effective_byte(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, (uint8)aCPU.gpr[rS]);
}

int ppc_opc_stbu(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
    int ret = ppc_write_effective_byte(aCPU, aCPU.gpr[rA] + imm, (uint8)aCPU.gpr[rS]);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += imm;
    }
    return ret;
}
int ppc_opc_stbux(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    int ret = ppc_write_effective_byte(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], (uint8)aCPU.gpr[rS]);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += aCPU.gpr[rB];
    }
    return ret;
}
int ppc_opc_stbx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    return ppc_write_effective_byte(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], (uint8)aCPU.gpr[rS]);
}

int ppc_opc_stw(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
    return ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, aCPU.gpr[rS]);
}

int ppc_opc_stwu(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
    int ret = ppc_write_effective_word(aCPU, aCPU.gpr[rA] + imm, aCPU.gpr[rS]);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += imm;
    }
    return ret;
}
int ppc_opc_stwux(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    int ret = ppc_write_effective_word(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], aCPU.gpr[rS]);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += aCPU.gpr[rB];
    }
    return ret;
}
int ppc_opc_stwx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    return ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], aCPU.gpr[rS]);
}
int ppc_opc_stwbrx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    return ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], ppc_bswap_word(aCPU.gpr[rS]));
}
int ppc_opc_stwcx_(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.cr &= 0x0fffffff;
    if (aCPU.have_reservation) {
        aCPU.have_reservation = false;
        uint32 v;
        int ret = ppc_read_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], v);
        if (ret) {
            return ret;
        }
        if (v == aCPU.reserve) {
            ret = ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], aCPU.gpr[rS]);
            if (ret) {
                return ret;
            }
            aCPU.cr |= CR_CR0_EQ;
        }
        if (aCPU.xer & XER_SO) {
            aCPU.cr |= CR_CR0_SO;
        }
    }
    return PPC_MMU_OK;
}

int ppc_opc_sth(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
    return ppc_write_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, (uint16)aCPU.gpr[rS]);
}

int ppc_opc_sthbrx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    return ppc_write_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], ppc_bswap_half(aCPU.gpr[rS]));
}
int ppc_opc_sthu(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
    int ret = ppc_write_effective_half(aCPU, aCPU.gpr[rA] + imm, (uint16)aCPU.gpr[rS]);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += imm;
    }
    return ret;
}
int ppc_opc_sthux(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    int ret = ppc_write_effective_half(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], (uint16)aCPU.gpr[rS]);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += aCPU.gpr[rB];
    }
    return ret;
}
int ppc_opc_sthx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    return ppc_write_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], (uint16)aCPU.gpr[rS]);
}
int ppc_opc_stmw(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
    uint32 ea = (rA ? aCPU.gpr[rA] : 0) + imm;
    for (int i = rS; i <= 31; i++) {
        int ret = ppc_write_effective_word(aCPU, ea, aCPU.gpr[i]);
        if (ret != PPC_MMU_OK) {
            return ret;
        }
        ea += 4;
    }
    return PPC_MMU_OK;
}
int ppc_opc_stswi(PPC_CPU_State &aCPU)
{
    int rA, rS, NB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, NB);
    if (NB == 0) {
        NB = 32;
    }
    uint32 ea = rA ? aCPU.gpr[rA] : 0;
    uint32 r = 0;
    int i = 0;
    while (NB > 0) {
        if (!i) {
            r = aCPU.gpr[rS];
            rS++;
            rS %= 32;
            i = 4;
        }
        int ret = ppc_write_effective_byte(aCPU, ea, (r >> 24));
        if (ret) {
            return ret;
        }
        r <<= 8;
        ea++;
        i--;
        NB--;
    }
    return PPC_MMU_OK;
}
int ppc_opc_stswx(PPC_CPU_State &aCPU)
{
    int rA, rS, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    int NB = XER_n(aCPU.xer);
    uint32 ea = aCPU.gpr[rB] + (rA ? aCPU.gpr[rA] : 0);
    uint32 r = 0;
    int i = 0;
    while (NB > 0) {
        if (!i) {
            r = aCPU.gpr[rS];
            rS++;
            rS %= 32;
            i = 4;
        }
        int ret = ppc_write_effective_byte(aCPU, ea, (r >> 24));
        if (ret) {
            return ret;
        }
        r <<= 8;
        ea++;
        i--;
        NB--;
    }
    return PPC_MMU_OK;
}

/*
 * FPU load/store opcodes
 * These just transfer data between fpr[] and memory.
 * Double: 8 bytes transferred as-is (raw IEEE 754 bits)
 * Single: 4 bytes with double↔single conversion
 */

#define FPU_CHECK(cpu)                                                                                                 \
    if (!(cpu.msr & MSR_FP)) {                                                                                         \
        ppc_exception(cpu, PPC_EXC_NO_FPU, 0, 0);                                                                      \
        return PPC_MMU_EXC;                                                                                            \
    }

int ppc_opc_lfd(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frD;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frD, rA, imm);
    uint64 r;
    int ret = ppc_read_effective_dword(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, r);
    if (ret == PPC_MMU_OK) {
        aCPU.fpr[frD] = r;
    }
    return ret;
}
int ppc_opc_lfdu(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frD;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frD, rA, imm);
    uint64 r;
    int ret = ppc_read_effective_dword(aCPU, aCPU.gpr[rA] + imm, r);
    if (ret == PPC_MMU_OK) {
        aCPU.fpr[frD] = r;
        aCPU.gpr[rA] += imm;
    }
    return ret;
}
int ppc_opc_lfdux(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frD, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
    uint64 r;
    int ret = ppc_read_effective_dword(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += aCPU.gpr[rB];
        aCPU.fpr[frD] = r;
    }
    return ret;
}
int ppc_opc_lfdx(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frD, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
    uint64 r;
    int ret = ppc_read_effective_dword(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        aCPU.fpr[frD] = r;
    }
    return ret;
}
int ppc_opc_lfs(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frD;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frD, rA, imm);
    uint32 r;
    int ret = ppc_read_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, r);
    if (ret == PPC_MMU_OK) {
        ppc_single s;
        ppc_double d;
        ppc_fpu_unpack_single(s, r);
        ppc_fpu_single_to_double(s, d);
        ppc_fpu_pack_double(aCPU.fpscr, d, aCPU.fpr[frD]);
    }
    return ret;
}
int ppc_opc_lfsu(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frD;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frD, rA, imm);
    uint32 r;
    int ret = ppc_read_effective_word(aCPU, aCPU.gpr[rA] + imm, r);
    if (ret == PPC_MMU_OK) {
        ppc_single s;
        ppc_double d;
        ppc_fpu_unpack_single(s, r);
        ppc_fpu_single_to_double(s, d);
        ppc_fpu_pack_double(aCPU.fpscr, d, aCPU.fpr[frD]);
        aCPU.gpr[rA] += imm;
    }
    return ret;
}
int ppc_opc_lfsux(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frD, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
    uint32 r;
    int ret = ppc_read_effective_word(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        ppc_single s;
        ppc_double d;
        ppc_fpu_unpack_single(s, r);
        ppc_fpu_single_to_double(s, d);
        ppc_fpu_pack_double(aCPU.fpscr, d, aCPU.fpr[frD]);
        aCPU.gpr[rA] += aCPU.gpr[rB];
    }
    return ret;
}
int ppc_opc_lfsx(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frD, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
    uint32 r;
    int ret = ppc_read_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r);
    if (ret == PPC_MMU_OK) {
        ppc_single s;
        ppc_double d;
        ppc_fpu_unpack_single(s, r);
        ppc_fpu_single_to_double(s, d);
        ppc_fpu_pack_double(aCPU.fpscr, d, aCPU.fpr[frD]);
    }
    return ret;
}
int ppc_opc_stfd(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frS;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frS, rA, imm);
    return ppc_write_effective_dword(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, aCPU.fpr[frS]);
}
int ppc_opc_stfdu(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frS;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frS, rA, imm);
    int ret = ppc_write_effective_dword(aCPU, aCPU.gpr[rA] + imm, aCPU.fpr[frS]);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += imm;
    }
    return ret;
}
int ppc_opc_stfdux(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frS, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
    int ret = ppc_write_effective_dword(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], aCPU.fpr[frS]);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += aCPU.gpr[rB];
    }
    return ret;
}
int ppc_opc_stfdx(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frS, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
    return ppc_write_effective_dword(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], aCPU.fpr[frS]);
}
int ppc_opc_stfiwx(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frS, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
    return ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], (uint32)aCPU.fpr[frS]);
}
int ppc_opc_stfs(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frS;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frS, rA, imm);
    ppc_double d;
    ppc_fpu_unpack_double(d, aCPU.fpr[frS]);
    uint32 s;
    ppc_fpu_pack_single(aCPU.fpscr, d, s);
    return ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, s);
}
int ppc_opc_stfsu(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frS;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frS, rA, imm);
    ppc_double d;
    ppc_fpu_unpack_double(d, aCPU.fpr[frS]);
    uint32 s;
    ppc_fpu_pack_single(aCPU.fpscr, d, s);
    int ret = ppc_write_effective_word(aCPU, aCPU.gpr[rA] + imm, s);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += imm;
    }
    return ret;
}
int ppc_opc_stfsux(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frS, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
    ppc_double d;
    ppc_fpu_unpack_double(d, aCPU.fpr[frS]);
    uint32 s;
    ppc_fpu_pack_single(aCPU.fpscr, d, s);
    int ret = ppc_write_effective_word(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], s);
    if (ret == PPC_MMU_OK) {
        aCPU.gpr[rA] += aCPU.gpr[rB];
    }
    return ret;
}
int ppc_opc_stfsx(PPC_CPU_State &aCPU)
{
    FPU_CHECK(aCPU);
    int rA, frS, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
    ppc_double d;
    ppc_fpu_unpack_double(d, aCPU.fpr[frS]);
    uint32 s;
    ppc_fpu_pack_single(aCPU.fpscr, d, s);
    return ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], s);
}

/*
 *	AltiVec load/store operations
 */

#if HOST_ENDIANESS == HOST_ENDIANESS_LE
static byte lvsl_helper[] = {0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15,
                             0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A,
                             0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
static byte lvsl_helper[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
                             0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
                             0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
#else
#error Endianess not supported!
#endif

/*      lvx         Load Vector Indexed
 *      v.127
 */
int ppc_opc_lvx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
    if ((aCPU.msr & MSR_VEC) == 0) {
        ppc_exception(aCPU, PPC_EXC_NO_VEC);
        return 1;
    }
#endif
    VECTOR_DEBUG;
    int rA, vrD, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, rA, rB);

    uint32 ea = ((rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB]) & ~0x0f;

    uint64 hi, lo;
    int ret = ppc_read_effective_dword(aCPU, ea, hi);
    if (ret) {
        return ret;
    }
    ret = ppc_read_effective_dword(aCPU, ea + 8, lo);
    if (ret) {
        return ret;
    }
    VECT_D(aCPU.vr[vrD], 0) = hi;
    VECT_D(aCPU.vr[vrD], 1) = lo;
    return 0;
}

/*      lvxl        Load Vector Index LRU
 *      v.128
 */
int ppc_opc_lvxl(PPC_CPU_State &aCPU)
{
    return ppc_opc_lvx(aCPU);
    /* This instruction should hint to the cache that the value won't be
	 *   needed again in memory anytime soon.  We don't emulate the cache,
	 *   so this is effectively exactly the same as lvx.
	 */
}

/*      lvebx       Load Vector Element Byte Indexed
 *      v.119
 */
int ppc_opc_lvebx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
    if ((aCPU.msr & MSR_VEC) == 0) {
        ppc_exception(aCPU, PPC_EXC_NO_VEC);
        return 1;
    }
#endif
    VECTOR_DEBUG;
    int rA, vrD, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, rA, rB);
    uint32 ea;
    uint8 r;
    ea = (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB];
    int ret = ppc_read_effective_byte(aCPU, ea, r);
    if (ret == PPC_MMU_OK) {
        VECT_B(aCPU.vr[vrD], ea & 0xf) = r;
    }
    return ret;
}

/*      lvehx       Load Vector Element Half Word Indexed
 *      v.121
 */
int ppc_opc_lvehx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
    if ((aCPU.msr & MSR_VEC) == 0) {
        ppc_exception(aCPU, PPC_EXC_NO_VEC);
        return 1;
    }
#endif
    VECTOR_DEBUG;
    int rA, vrD, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, rA, rB);
    uint32 ea;
    uint16 r;
    ea = ((rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB]) & ~1;
    int ret = ppc_read_effective_half(aCPU, ea, r);
    if (ret == PPC_MMU_OK) {
        VECT_H(aCPU.vr[vrD], (ea & 0xf) >> 1) = r;
    }
    return ret;
}

/*      lvewx       Load Vector Element Word Indexed
 *      v.122
 */
int ppc_opc_lvewx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
    if ((aCPU.msr & MSR_VEC) == 0) {
        ppc_exception(aCPU, PPC_EXC_NO_VEC);
        return 1;
    }
#endif
    VECTOR_DEBUG;
    int rA, vrD, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, rA, rB);
    uint32 ea;
    uint32 r;
    ea = ((rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB]) & ~3;
    int ret = ppc_read_effective_word(aCPU, ea, r);
    if (ret == PPC_MMU_OK) {
        VECT_W(aCPU.vr[vrD], (ea & 0xf) >> 2) = r;
    }
    return ret;
}

/*      lvsl        Load Vector for Shift Left
 *      v.123
 */
int ppc_opc_lvsl(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
    if ((aCPU.msr & MSR_VEC) == 0) {
        ppc_exception(aCPU, PPC_EXC_NO_VEC);
        return 1;
    }
#endif
    VECTOR_DEBUG;
    int rA, vrD, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, rA, rB);
    uint32 ea;
    ea = ((rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB]);
#if HOST_ENDIANESS == HOST_ENDIANESS_LE
    memmove(&aCPU.vr[vrD], lvsl_helper + 0x10 - (ea & 0xf), 16);
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
    memmove(&aCPU.vr[vrD], lvsl_helper + (ea & 0xf), 16);
#else
#error Endianess not supported!
#endif
    return 0;
}

/*      lvsr        Load Vector for Shift Right
 *      v.125
 */
int ppc_opc_lvsr(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
    if ((aCPU.msr & MSR_VEC) == 0) {
        ppc_exception(aCPU, PPC_EXC_NO_VEC);
        return 1;
    }
#endif
    VECTOR_DEBUG;
    int rA, vrD, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, rA, rB);
    uint32 ea;
    ea = ((rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB]);
#if HOST_ENDIANESS == HOST_ENDIANESS_LE
    memmove(&aCPU.vr[vrD], lvsl_helper + (ea & 0xf), 16);
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
    memmove(&aCPU.vr[vrD], lvsl_helper + 0x10 - (ea & 0xf), 16);
#else
#error Endianess not supported!
#endif
    return 0;
}

/*      dst         Data Stream Touch
 *      v.115
 */
int ppc_opc_dst(PPC_CPU_State &aCPU)
{
    VECTOR_DEBUG;
    /* Since we are not emulating the cache, this is a nop */
    return 0;
}

/*      stvx        Store Vector Indexed
 *      v.134
 */
int ppc_opc_stvx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
    if ((aCPU.msr & MSR_VEC) == 0) {
        ppc_exception(aCPU, PPC_EXC_NO_VEC);
        return 1;
    }
#endif
    VECTOR_DEBUG;
    int rA, vrS, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, vrS, rA, rB);

    uint32 ea = ((rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB]) & ~0x0f;

    int ret = ppc_write_effective_dword(aCPU, ea, VECT_D(aCPU.vr[vrS], 0));
    if (ret) {
        return ret;
    }
    ret = ppc_write_effective_dword(aCPU, ea + 8, VECT_D(aCPU.vr[vrS], 1));
    return ret;
}

/*      stvxl       Store Vector Indexed LRU
 *      v.135
 */
int ppc_opc_stvxl(PPC_CPU_State &aCPU)
{
    return ppc_opc_stvx(aCPU);
    /* This instruction should hint to the cache that the value won't be
	 *   needed again in memory anytime soon.  We don't emulate the cache,
	 *   so this is effectively exactly the same as stvx.
	 */
}

/*      stvebx      Store Vector Element Byte Indexed
 *      v.131
 */
int ppc_opc_stvebx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
    if ((aCPU.msr & MSR_VEC) == 0) {
        ppc_exception(aCPU, PPC_EXC_NO_VEC);
        return 1;
    }
#endif
    VECTOR_DEBUG;
    int rA, vrS, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, vrS, rA, rB);
    uint32 ea;
    ea = (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB];
    return ppc_write_effective_byte(aCPU, ea, VECT_B(aCPU.vr[vrS], ea & 0xf));
}

/*      stvehx      Store Vector Element Half Word Indexed
 *      v.132
 */
int ppc_opc_stvehx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
    if ((aCPU.msr & MSR_VEC) == 0) {
        ppc_exception(aCPU, PPC_EXC_NO_VEC);
        return 1;
    }
#endif
    VECTOR_DEBUG;
    int rA, vrS, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, vrS, rA, rB);
    uint32 ea;
    ea = ((rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB]) & ~1;
    return ppc_write_effective_half(aCPU, ea, VECT_H(aCPU.vr[vrS], (ea & 0xf) >> 1));
}

/*      stvewx      Store Vector Element Word Indexed
 *      v.133
 */
int ppc_opc_stvewx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
    if ((aCPU.msr & MSR_VEC) == 0) {
        ppc_exception(aCPU, PPC_EXC_NO_VEC);
        return 1;
    }
#endif
    VECTOR_DEBUG;
    int rA, vrS, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, vrS, rA, rB);
    uint32 ea;
    ea = ((rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB]) & ~3;
    return ppc_write_effective_word(aCPU, ea, VECT_W(aCPU.vr[vrS], (ea & 0xf) >> 2));
}
int ppc_opc_dstst(PPC_CPU_State &aCPU)
{
    /* Since we are not emulating the cache, this is a nop */
    return 0;
}
int ppc_opc_dss(PPC_CPU_State &aCPU)
{
    /* Since we are not emulating the cache, this is a nop */
    return 0;
}

/*
 *  ========================================================
 *  Native AArch64 code generation for load/store opcodes
 *  ========================================================
 *
 *  These emit code that:
 *  1. Computes the effective address (rA + SIMM, or rA + rB)
 *  2. Calls the asm TLB stub (ppc_read/write_effective_*_asm)
 *     which has an inline TLB fast path
 *  3. Stores/uses the result
 *
 *  Convention:
 *    X20 = PPC_CPU_State pointer
 *    W16, W17 = scratch (IP0/IP1)
 *    W0 = effective address for asm stubs
 *    W0 = return value from read stubs
 *    W1 = value for write stubs
 */

#define GPR_OFS(n) (offsetof(PPC_CPU_State, gpr) + (n) * 4)

/*
 *  Helper: emit code to compute EA = (rA ? gpr[rA] : 0) + SIMM into W0
 */
static void gen_ea_D(JITC &jitc, int rA, sint32 simm)
{
    if (rA == 0) {
        jitc.asmMOV(W0, (uint32)simm);
    } else {
        jitc.asmLDRw_cpu(W0, GPR_OFS(rA));
        if (simm > 0 && simm < 4096) {
            jitc.asmADDw(W0, W0, (uint32)simm);
        } else if (simm < 0 && (-simm) < 4096) {
            jitc.asmSUBw(W0, W0, (uint32)(-simm));
        } else if (simm != 0) {
            jitc.asmMOV(W17, (uint32)simm);
            jitc.asmADDw(W0, W0, W17);
        }
    }
}


/*
 *  Helpers for load/store codegen.
 */

// Pass pc_ofs to asm stub in W9.  The stub stores pc_ofs and computes
// pc = current_code_base + pc_ofs only on the slow path (TLB miss/DSI).
static void gen_prologue(JITC &jitc)
{
    jitc.asmMOV(W9, jitc.pc);
}

// Compute EA = (rA ? gpr[rA] : 0) + gpr[rB] into W0  (X-form)
static void gen_ea_X(JITC &jitc, int rA, int rB)
{
    if (rA == 0) {
        jitc.asmLDRw_cpu(W0, GPR_OFS(rB));
    } else {
        jitc.asmLDRw_cpu(W0, GPR_OFS(rA));
        jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
        jitc.asmADDw(W0, W0, W17);
    }
}

/*
 *  === D-form loads ===
 */

JITCFlow ppc_opc_gen_lwz(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmCALL_cpu(PPC_STUB_READ_WORD);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lbz(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmCALL_cpu(PPC_STUB_READ_BYTE);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lhz(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmCALL_cpu(PPC_STUB_READ_HALF_Z);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lha(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmCALL_cpu(PPC_STUB_READ_HALF_S);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    return flowContinue;
}

/*
 *  === D-form stores ===
 */

JITCFlow ppc_opc_gen_stw(JITC &jitc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_WORD);
    return flowContinue;
}

JITCFlow ppc_opc_gen_stb(JITC &jitc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_BYTE);
    return flowContinue;
}

JITCFlow ppc_opc_gen_sth(JITC &jitc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_HALF);
    return flowContinue;
}

/*
 *  === D-form loads with update ===
 *  EA = gpr[rA] + d, rD = MEM[EA], rA = EA
 *  EA is saved in W6 across the stub call (W6 is not clobbered by stubs).
 *  If DSI occurs, the stub never returns, so rA is not modified.
 */

JITCFlow ppc_opc_gen_lwzu(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmMOV(W6, W0);
    jitc.asmCALL_cpu(PPC_STUB_READ_WORD);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lbzu(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmMOV(W6, W0);
    jitc.asmCALL_cpu(PPC_STUB_READ_BYTE);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lhzu(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmMOV(W6, W0);
    jitc.asmCALL_cpu(PPC_STUB_READ_HALF_Z);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lhau(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmMOV(W6, W0);
    jitc.asmCALL_cpu(PPC_STUB_READ_HALF_S);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  === D-form stores with update ===
 *  EA = gpr[rA] + d, MEM[EA] = rS, rA = EA
 */

JITCFlow ppc_opc_gen_stwu(JITC &jitc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmMOV(W6, W0);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_WORD);
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

JITCFlow ppc_opc_gen_stbu(JITC &jitc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmMOV(W6, W0);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_BYTE);
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

JITCFlow ppc_opc_gen_sthu(JITC &jitc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmMOV(W6, W0);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_HALF);
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  === X-form indexed loads ===
 *  EA = (rA|0) + gpr[rB], rD = MEM[EA]
 */

JITCFlow ppc_opc_gen_lwzx(JITC &jitc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmCALL_cpu(PPC_STUB_READ_WORD);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lbzx(JITC &jitc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmCALL_cpu(PPC_STUB_READ_BYTE);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lhzx(JITC &jitc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmCALL_cpu(PPC_STUB_READ_HALF_Z);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lhax(JITC &jitc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmCALL_cpu(PPC_STUB_READ_HALF_S);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    return flowContinue;
}

/*
 *  === X-form indexed stores ===
 *  EA = (rA|0) + gpr[rB], MEM[EA] = rS
 */

JITCFlow ppc_opc_gen_stwx(JITC &jitc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_WORD);
    return flowContinue;
}

JITCFlow ppc_opc_gen_stbx(JITC &jitc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_BYTE);
    return flowContinue;
}

JITCFlow ppc_opc_gen_sthx(JITC &jitc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_HALF);
    return flowContinue;
}

/*
 *  === lwarx / stwcx. ===
 */

JITCFlow ppc_opc_gen_lwarx(JITC &jitc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmCALL_cpu(PPC_STUB_READ_WORD);
    // rD = result
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    // reserve = result
    jitc.asmSTRw_cpu(W0, offsetof(PPC_CPU_State, reserve));
    // have_reservation = 1
    jitc.asmMOV(W16, 1);
    jitc.asmSTRw_cpu(W16, offsetof(PPC_CPU_State, have_reservation));
    return flowContinue;
}

JITCFlow ppc_opc_gen_stwcx_(JITC &jitc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.clobberAll();
    // Reserve enough space for the entire codegen to avoid fragment
    // boundaries between forward-branch fixups and their targets.
    jitc.emitAssure(128);
    gen_prologue(jitc);

    // cr &= 0x0FFFFFFF  (clear CR0)
    // 32-bit logical imm: 0x0FFFFFFF = 28 ones from bit 0, N=0, immr=0, imms=27
    jitc.asmLDRw_cpu(W16, offsetof(PPC_CPU_State, cr));
    jitc.asmANDw_imm(W16, W16, 0, 27);
    jitc.asmSTRw_cpu(W16, offsetof(PPC_CPU_State, cr));

    // if (!have_reservation) goto done
    jitc.asmLDRw_cpu(W16, offsetof(PPC_CPU_State, have_reservation));
    NativeAddress cbz_fixup = jitc.asmCBZwFixup(W16);

    // have_reservation = 0
    jitc.asmMOV(W16, 0);
    jitc.asmSTRw_cpu(W16, offsetof(PPC_CPU_State, have_reservation));

    // EA into W0, save in temp2 (asm stub clobbers W0-W18)
    gen_ea_X(jitc, rA, rB);
    jitc.asmSTRw_cpu(W0, offsetof(PPC_CPU_State, temp2));

    // Read current value at EA
    jitc.asmCALL_cpu(PPC_STUB_READ_WORD);

    // Compare with reserve
    jitc.asmLDRw_cpu(W16, offsetof(PPC_CPU_State, reserve));
    jitc.asmCMPw(W0, W16);

    // if (value != reserve) goto skip_write
    NativeAddress bne_fixup = jitc.asmBccFixup(A64_NE);

    // Store gpr[rS] to EA (reload EA from temp2, reload W9 = pc_ofs)
    gen_prologue(jitc);
    jitc.asmLDRw_cpu(W0, offsetof(PPC_CPU_State, temp2));
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_WORD);

    // cr |= CR_CR0_EQ  (bit 29 = 0x20000000)
    jitc.asmLDRw_cpu(W16, offsetof(PPC_CPU_State, cr));
    jitc.asmMOV(W17, (uint32)CR_CR0_EQ);
    jitc.asmORRw(W16, W16, W17);
    jitc.asmSTRw_cpu(W16, offsetof(PPC_CPU_State, cr));

    // skip_write:
    jitc.asmResolveFixup(bne_fixup);

    // if (xer & XER_SO) cr |= CR_CR0_SO
    jitc.asmLDRw_cpu(W16, offsetof(PPC_CPU_State, xer));
    uint so_body = 4 /* LDR cr */ + a64_movw_size(CR_CR0_SO) + 4 /* ORR */ + 4 /* STR */;
    NativeAddress so_done = jitc.asmHERE() + 4 + so_body;
    jitc.asmTBZ(W16, 31, 4 + so_body); // skip body
    jitc.asmLDRw_cpu(W16, offsetof(PPC_CPU_State, cr));
    jitc.asmMOV(W17, (uint32)CR_CR0_SO);
    jitc.asmORRw(W16, W16, W17);
    jitc.asmSTRw_cpu(W16, offsetof(PPC_CPU_State, cr));
    jitc.asmAssertHERE(so_done, "stwcx_ SO skip");

    // done:
    jitc.asmResolveFixup(cbz_fixup);

    return flowContinue;
}

/*
 *  === X-form indexed loads with update ===
 *  EA = gpr[rA] + gpr[rB], rD = MEM[EA], rA = EA
 */

JITCFlow ppc_opc_gen_lwzux(JITC &jitc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmMOV(W6, W0);
    jitc.asmCALL_cpu(PPC_STUB_READ_WORD);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lbzux(JITC &jitc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmMOV(W6, W0);
    jitc.asmCALL_cpu(PPC_STUB_READ_BYTE);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lhzux(JITC &jitc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmMOV(W6, W0);
    jitc.asmCALL_cpu(PPC_STUB_READ_HALF_Z);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lhaux(JITC &jitc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmMOV(W6, W0);
    jitc.asmCALL_cpu(PPC_STUB_READ_HALF_S);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  === X-form indexed stores with update ===
 *  EA = gpr[rA] + gpr[rB], MEM[EA] = rS, rA = EA
 */

JITCFlow ppc_opc_gen_stwux(JITC &jitc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmMOV(W6, W0);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_WORD);
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

JITCFlow ppc_opc_gen_stbux(JITC &jitc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmMOV(W6, W0);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_BYTE);
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

JITCFlow ppc_opc_gen_sthux(JITC &jitc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmMOV(W6, W0);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_HALF);
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  === Byte-reversed load/store ===
 *  Call the normal asm stub (which byte-swaps BE→LE), then REV the result
 *  to get the byte-reversed value (or REV before storing).
 */

JITCFlow ppc_opc_gen_lwbrx(JITC &jitc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmCALL_cpu(PPC_STUB_READ_WORD);
    jitc.asmREVw(W0, W0);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lhbrx(JITC &jitc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmCALL_cpu(PPC_STUB_READ_HALF_Z);
    // Half stub returns zero-extended 16-bit value, byte-swap the low 16 bits
    jitc.asmREV16w(W0, W0);
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    return flowContinue;
}

JITCFlow ppc_opc_gen_stwbrx(JITC &jitc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmREVw(W1, W1);
    jitc.asmCALL_cpu(PPC_STUB_WRITE_WORD);
    return flowContinue;
}

JITCFlow ppc_opc_gen_sthbrx(JITC &jitc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmLDRw_cpu(W1, GPR_OFS(rS));
    jitc.asmREV16w(W1, W1);
    jitc.asmCALL_cpu(PPC_STUB_WRITE_HALF);
    return flowContinue;
}

/*
 *  === lmw / stmw — Load/Store Multiple Word ===
 *  For small register counts (≤4), unroll as individual lwz/stw calls.
 *  For larger counts, fall back to interpreter.
 */

#define LMW_STMW_INLINE_MAX 4

JITCFlow ppc_opc_gen_lmw(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    int count = 32 - rD;
    if (count > LMW_STMW_INLINE_MAX) {
        ppc_opc_gen_interpret_loadstore(jitc, ppc_opc_lmw);
        return flowEndBlock;
    }
    jitc.clobberAll();
    gen_prologue(jitc);
    for (int i = 0; i < count; i++) {
        gen_ea_D(jitc, rA, (sint32)imm + i * 4);
        jitc.asmCALL_cpu(PPC_STUB_READ_WORD);
        jitc.asmSTRw_cpu(W0, GPR_OFS(rD + i));
    }
    return flowContinue;
}

JITCFlow ppc_opc_gen_stmw(JITC &jitc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
    int count = 32 - rS;
    if (count > LMW_STMW_INLINE_MAX) {
        ppc_opc_gen_interpret_loadstore(jitc, ppc_opc_stmw);
        return flowEndBlock;
    }
    jitc.clobberAll();
    gen_prologue(jitc);
    for (int i = 0; i < count; i++) {
        gen_ea_D(jitc, rA, (sint32)imm + i * 4);
        jitc.asmLDRw_cpu(W1, GPR_OFS(rS + i));
        jitc.asmCALL_cpu(PPC_STUB_WRITE_WORD);
    }
    return flowContinue;
}

/*
 *  === FP double-word loads ===
 *  lfd frD, d(rA): frD = MEM64[(rA|0) + d]
 *  Dword stub: W0 = EA, W9 = pc_ofs → returns X0 = 64-bit value
 */

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
        jitc.asmBccForward(A64_NE, body); // B.NE skip (FP enabled)

        jitc.asmMOV(W0, jitc.pc);
        jitc.asmCALL_cpu(PPC_STUB_NO_FPU_EXC);

        jitc.asmAssertHERE(target, "check_fpu_mmu");
        jitc.checkedFloat = true;
    }
}

JITCFlow ppc_opc_gen_lfd(JITC &jitc)
{
    int frD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, frD, rA, imm);
    gen_check_fpu(jitc);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmCALL_cpu(PPC_STUB_READ_DWORD);
    jitc.asmSTR_cpu(X0, FPR_OFS(frD));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lfdu(JITC &jitc)
{
    int frD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, frD, rA, imm);
    gen_check_fpu(jitc);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmMOV(W6, W0);
    jitc.asmCALL_cpu(PPC_STUB_READ_DWORD);
    jitc.asmSTR_cpu(X0, FPR_OFS(frD));
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lfdx(JITC &jitc)
{
    int frD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, frD, rA, rB);
    gen_check_fpu(jitc);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmCALL_cpu(PPC_STUB_READ_DWORD);
    jitc.asmSTR_cpu(X0, FPR_OFS(frD));
    return flowContinue;
}

JITCFlow ppc_opc_gen_lfdux(JITC &jitc)
{
    int frD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, frD, rA, rB);
    gen_check_fpu(jitc);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmMOV(W6, W0);
    jitc.asmCALL_cpu(PPC_STUB_READ_DWORD);
    jitc.asmSTR_cpu(X0, FPR_OFS(frD));
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  === FP double-word stores ===
 *  stfd frS, d(rA): MEM64[(rA|0) + d] = frS
 *  Dword stub: W0 = EA, X1 = 64-bit value, W9 = pc_ofs
 */

JITCFlow ppc_opc_gen_stfd(JITC &jitc)
{
    int frS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, frS, rA, imm);
    gen_check_fpu(jitc);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmLDR_cpu(X1, FPR_OFS(frS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_DWORD);
    return flowContinue;
}

JITCFlow ppc_opc_gen_stfdu(JITC &jitc)
{
    int frS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, frS, rA, imm);
    gen_check_fpu(jitc);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_D(jitc, rA, (sint32)imm);
    jitc.asmMOV(W6, W0);
    jitc.asmLDR_cpu(X1, FPR_OFS(frS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_DWORD);
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}

JITCFlow ppc_opc_gen_stfdx(JITC &jitc)
{
    int frS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, frS, rA, rB);
    gen_check_fpu(jitc);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmLDR_cpu(X1, FPR_OFS(frS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_DWORD);
    return flowContinue;
}

JITCFlow ppc_opc_gen_stfdux(JITC &jitc)
{
    int frS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, frS, rA, rB);
    gen_check_fpu(jitc);
    jitc.clobberAll();
    gen_prologue(jitc);
    gen_ea_X(jitc, rA, rB);
    jitc.asmMOV(W6, W0);
    jitc.asmLDR_cpu(X1, FPR_OFS(frS));
    jitc.asmCALL_cpu(PPC_STUB_WRITE_DWORD);
    jitc.asmSTRw_cpu(W6, GPR_OFS(rA));
    return flowContinue;
}
