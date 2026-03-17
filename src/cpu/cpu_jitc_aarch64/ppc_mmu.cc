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
    // ISI exception — set exception_pending so the caller can detect it.
    // The caller (ppc_new_pc_asm in jitc_tools.S) will check and jump to
    // ppc_isi_exception_asm which handles it properly (SRR0/SRR1 setup,
    // MSR clear, TLB invalidate, dispatch to vector 0x400).
    PPC_MMU_WARN("ISI for EA %08x (r=%d)\n", ea, r);
    cpu->exception_pending = true;
    cpu->srr[1] = (r == PPC_MMU_FATAL) ? (1 << 30) : 0; // stash fault type in srr1 temporarily
    return ea; // return EA — caller will handle the exception
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
 *  On MMU failure: sets DAR, DSISR and returns with cpu->exception_pending
 *  set. The asm stub checks exception_pending and jumps to the DSI handler.
 */

#define DSISR_PAGE  (1<<30)
#define DSISR_STORE (1<<25)
#define DSISR_PROT  (1<<27)

static inline void raise_dsi(PPC_CPU_State *cpu, uint32 ea, bool isWrite)
{
	cpu->dar = ea;
	cpu->dsisr = DSISR_PAGE | (isWrite ? DSISR_STORE : 0);
	cpu->exception_pending = true;
}

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

extern "C" uint32 ppc_read_effective_byte_slow(PPC_CPU_State *cpu, uint32 ea)
{
	uint32 pa;
	if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_READ, pa) == PPC_MMU_OK) {
		tlb_fill_data_read(cpu, ea, pa);
		uint8 result;
		ppc_read_physical_byte(pa, result);
		return result;
	}
	raise_dsi(cpu, ea, false);
	return 0;
}

extern "C" uint32 ppc_read_effective_half_z_slow(PPC_CPU_State *cpu, uint32 ea)
{
	uint32 pa;
	if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_READ, pa) == PPC_MMU_OK) {
		tlb_fill_data_read(cpu, ea, pa);
		uint16 result;
		ppc_read_physical_half(pa, result);
		return result;
	}
	raise_dsi(cpu, ea, false);
	return 0;
}

extern "C" uint32 ppc_read_effective_word_slow(PPC_CPU_State *cpu, uint32 ea)
{
	uint32 pa;
	if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_READ, pa) == PPC_MMU_OK) {
		tlb_fill_data_read(cpu, ea, pa);
		uint32 result;
		ppc_read_physical_word(pa, result);
		return result;
	}
	raise_dsi(cpu, ea, false);
	return 0;
}

extern "C" uint64 ppc_read_effective_dword_slow(PPC_CPU_State *cpu, uint32 ea)
{
	uint32 pa;
	if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_READ, pa) == PPC_MMU_OK) {
		tlb_fill_data_read(cpu, ea, pa);
		uint64 result;
		ppc_read_physical_dword(pa, result);
		return result;
	}
	raise_dsi(cpu, ea, false);
	return 0;
}

extern "C" void ppc_write_effective_byte_slow(PPC_CPU_State *cpu, uint32 ea, uint32 data)
{
	uint32 pa;
	if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_WRITE, pa) == PPC_MMU_OK) {
		tlb_fill_data_write(cpu, ea, pa);
		ppc_write_physical_byte(pa, data);
		return;
	}
	raise_dsi(cpu, ea, true);
}

extern "C" void ppc_write_effective_half_slow(PPC_CPU_State *cpu, uint32 ea, uint32 data)
{
	uint32 pa;
	if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_WRITE, pa) == PPC_MMU_OK) {
		tlb_fill_data_write(cpu, ea, pa);
		ppc_write_physical_half(pa, data);
		return;
	}
	raise_dsi(cpu, ea, true);
}

extern "C" void ppc_write_effective_word_slow(PPC_CPU_State *cpu, uint32 ea, uint32 data)
{
	uint32 pa;
	if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_WRITE, pa) == PPC_MMU_OK) {
		tlb_fill_data_write(cpu, ea, pa);
		ppc_write_physical_word(pa, data);
		return;
	}
	raise_dsi(cpu, ea, true);
}

extern "C" void ppc_write_effective_dword_slow(PPC_CPU_State *cpu, uint32 ea, uint64 data)
{
	uint32 pa;
	if (ppc_effective_to_physical(*cpu, ea, PPC_MMU_WRITE, pa) == PPC_MMU_OK) {
		tlb_fill_data_write(cpu, ea, pa);
		ppc_write_physical_dword(pa, data);
		return;
	}
	raise_dsi(cpu, ea, true);
}

#undef TLB

static int ppc_pte_protection[] = {
    1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0,
};

int FASTCALL ppc_effective_to_physical(PPC_CPU_State &aCPU, uint32 addr, int flags, uint32 &result)
{
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
                    return PPC_MMU_OK;
                }
            }
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
    if (sr & SR_T) {
        PPC_MMU_ERR("sr & T\n");
    } else {
        if ((flags & PPC_MMU_CODE) && (sr & SR_N)) {
            return PPC_MMU_FATAL;
        }
        uint32 offset = EA_Offset(addr);
        uint32 page_index = EA_PageIndex(addr);
        uint32 VSID = SR_VSID(sr);
        uint32 api = EA_API(addr);

        uint32 hash1 = (VSID ^ page_index);
        uint32 pteg_addr = ((hash1 & aCPU.pagetable_hashmask) << 6) | aCPU.pagetable_base;
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
        return ppc_read_physical_dword(p, result);
    }
    return r;
}

int FASTCALL ppc_read_effective_word(PPC_CPU_State &aCPU, uint32 addr, uint32 &result)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ, p)))) {
        return ppc_read_physical_word(p, result);
    }
    return r;
}

int FASTCALL ppc_read_effective_half(PPC_CPU_State &aCPU, uint32 addr, uint16 &result)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ, p)))) {
        return ppc_read_physical_half(p, result);
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
        return ppc_write_physical_dword(p, data);
    }
    raise_dsi(&aCPU, addr, true);
    return r;
}

int FASTCALL ppc_write_effective_word(PPC_CPU_State &aCPU, uint32 addr, uint32 data)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_WRITE, p)))) {
        return ppc_write_physical_word(p, data);
    }
    raise_dsi(&aCPU, addr, true);
    return r;
}

int FASTCALL ppc_write_effective_half(PPC_CPU_State &aCPU, uint32 addr, uint16 data)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_WRITE, p)))) {
        return ppc_write_physical_half(p, data);
    }
    raise_dsi(&aCPU, addr, true);
    return r;
}

int FASTCALL ppc_write_effective_byte(PPC_CPU_State &aCPU, uint32 addr, uint8 data)
{
    uint32 p;
    int r;
    if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_WRITE, p)))) {
        return ppc_write_physical_byte(p, data);
    }
    raise_dsi(&aCPU, addr, true);
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
    if (dest > gMemorySize || (dest + size) > gMemorySize)
        return false;
    byte *ptr;
    ppc_direct_physical_memory_handle(dest, ptr);
    memcpy(ptr, src, size);
    // Also write to validation reference memory if active
    extern byte *gValidateRefMemory;
    if (gValidateRefMemory) {
        memcpy(gValidateRefMemory + dest, src, size);
    }
    return true;
}

bool ppc_dma_read(void *dest, uint32 src, uint32 size)
{
    if (src > gMemorySize || (src + size) > gMemorySize)
        return false;
    byte *ptr;
    ppc_direct_physical_memory_handle(src, ptr);
    memcpy(dest, ptr, size);
    return true;
}

bool ppc_dma_set(uint32 dest, int c, uint32 size)
{
    if (dest > gMemorySize || (dest + size) > gMemorySize)
        return false;
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

void ppc_opc_dcbz(PPC_CPU_State &aCPU) {}
void ppc_opc_dcba(PPC_CPU_State &aCPU) {}
void ppc_opc_dcbf(PPC_CPU_State &aCPU) {}
void ppc_opc_dcbi(PPC_CPU_State &aCPU) {}
void ppc_opc_dcbst(PPC_CPU_State &aCPU) {}
void ppc_opc_dcbt(PPC_CPU_State &aCPU) {}
void ppc_opc_dcbtst(PPC_CPU_State &aCPU) {}

void ppc_opc_lbz(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint8 r;
    if (ppc_read_effective_byte(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, r) == PPC_MMU_OK) {
        aCPU.gpr[rD] = r;
    }
}

void ppc_opc_lbzu(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint8 r;
    if (ppc_read_effective_byte(aCPU, aCPU.gpr[rA] + imm, r) == PPC_MMU_OK) {
        aCPU.gpr[rA] += imm;
        aCPU.gpr[rD] = r;
    }
}

void ppc_opc_lbzux(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint8 r;
    if (ppc_read_effective_byte(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], r) == PPC_MMU_OK) {
        aCPU.gpr[rA] += aCPU.gpr[rB];
        aCPU.gpr[rD] = r;
    }
}

void ppc_opc_lbzx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint8 r;
    if (ppc_read_effective_byte(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r) == PPC_MMU_OK) {
        aCPU.gpr[rD] = r;
    }
}

void ppc_opc_lwz(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint32 r;
    if (ppc_read_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, r) == PPC_MMU_OK) {
        aCPU.gpr[rD] = r;
    }
}

void ppc_opc_lwzu(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint32 r;
    if (ppc_read_effective_word(aCPU, aCPU.gpr[rA] + imm, r) == PPC_MMU_OK) {
        aCPU.gpr[rA] += imm;
        aCPU.gpr[rD] = r;
    }
}

void ppc_opc_lwzux(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint32 r;
    if (ppc_read_effective_word(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], r) == PPC_MMU_OK) {
        aCPU.gpr[rA] += aCPU.gpr[rB];
        aCPU.gpr[rD] = r;
    }
}

void ppc_opc_lwzx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint32 r;
    if (ppc_read_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r) == PPC_MMU_OK) {
        aCPU.gpr[rD] = r;
    }
}

void ppc_opc_lhz(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint16 r;
    if (ppc_read_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, r) == PPC_MMU_OK) {
        aCPU.gpr[rD] = r;
    }
}

void ppc_opc_lhzu(PPC_CPU_State &aCPU)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint16 r;
	if (ppc_read_effective_half(aCPU, aCPU.gpr[rA] + imm, r) == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
		aCPU.gpr[rD] = r;
	}
}
void ppc_opc_lhzux(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint16 r;
	if (ppc_read_effective_half(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], r) == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
		aCPU.gpr[rD] = r;
	}
}
void ppc_opc_lhzx(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint16 r;
	if (ppc_read_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r) == PPC_MMU_OK) {
		aCPU.gpr[rD] = r;
	}
}
void ppc_opc_lha(PPC_CPU_State &aCPU)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint16 r;
	if (ppc_read_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, r) == PPC_MMU_OK) {
		aCPU.gpr[rD] = (sint32)(sint16)r;
	}
}
void ppc_opc_lhau(PPC_CPU_State &aCPU)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint16 r;
	if (ppc_read_effective_half(aCPU, aCPU.gpr[rA] + imm, r) == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
		aCPU.gpr[rD] = (sint32)(sint16)r;
	}
}
void ppc_opc_lhaux(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint16 r;
	if (ppc_read_effective_half(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], r) == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
		aCPU.gpr[rD] = (sint32)(sint16)r;
	}
}
void ppc_opc_lhax(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint16 r;
	if (ppc_read_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r) == PPC_MMU_OK) {
		aCPU.gpr[rD] = (sint32)(sint16)r;
	}
}
void ppc_opc_lhbrx(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint16 r;
	if (ppc_read_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r) == PPC_MMU_OK) {
		aCPU.gpr[rD] = ppc_bswap_half(r);
	}
}
void ppc_opc_lwbrx(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint32 r;
	if (ppc_read_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r) == PPC_MMU_OK) {
		aCPU.gpr[rD] = ppc_bswap_word(r);
	}
}
void ppc_opc_lwarx(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint32 r;
	if (ppc_read_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r) == PPC_MMU_OK) {
		aCPU.gpr[rD] = r;
		aCPU.have_reservation = true;
		aCPU.reserve = (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB];
	}
}
void ppc_opc_lmw(PPC_CPU_State &aCPU)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint32 ea = (rA ? aCPU.gpr[rA] : 0) + imm;
	for (int i = rD; i <= 31; i++) {
		uint32 val;
		if (ppc_read_effective_word(aCPU, ea, val) != PPC_MMU_OK) return;
		aCPU.gpr[i] = val;
		ea += 4;
	}
}
void ppc_opc_lswi(PPC_CPU_State &aCPU)
{
	int rA, rD, NB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, NB);
	if (NB == 0) NB = 32;
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
		if (ppc_read_effective_byte(aCPU, ea, v)) return;
		r <<= 8;
		r |= v;
		ea++;
		i--;
		NB--;
	}
	while (i) { r <<= 8; i--; }
	aCPU.gpr[rD] = r;
}
void ppc_opc_lswx(PPC_CPU_State &aCPU)
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
		if (ppc_read_effective_byte(aCPU, ea, v)) return;
		r <<= 8;
		r |= v;
		ea++;
		i--;
		NB--;
	}
	while (i) { r <<= 8; i--; }
	aCPU.gpr[rD] = r;
}

void ppc_opc_stb(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
    ppc_write_effective_byte(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, (uint8)aCPU.gpr[rS]);
}

void ppc_opc_stbu(PPC_CPU_State &aCPU)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
	if (ppc_write_effective_byte(aCPU, aCPU.gpr[rA] + imm, (uint8)aCPU.gpr[rS]) == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
	}
}
void ppc_opc_stbux(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	if (ppc_write_effective_byte(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], (uint8)aCPU.gpr[rS]) == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
	}
}
void ppc_opc_stbx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	ppc_write_effective_byte(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], (uint8)aCPU.gpr[rS]);
}

void ppc_opc_stw(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
    ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, aCPU.gpr[rS]);
}

void ppc_opc_stwu(PPC_CPU_State &aCPU)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
	if (ppc_write_effective_word(aCPU, aCPU.gpr[rA] + imm, aCPU.gpr[rS]) == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
	}
}
void ppc_opc_stwux(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	if (ppc_write_effective_word(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], aCPU.gpr[rS]) == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
	}
}
void ppc_opc_stwx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], aCPU.gpr[rS]);
}
void ppc_opc_stwbrx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], ppc_bswap_word(aCPU.gpr[rS]));
}
void ppc_opc_stwcx_(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	aCPU.cr &= 0x0fffffff;
	if (aCPU.have_reservation) {
		aCPU.have_reservation = false;
		uint32 ea = (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB];
		if (ea == aCPU.reserve) {
			if (ppc_write_effective_word(aCPU, ea, aCPU.gpr[rS]) == PPC_MMU_OK) {
				aCPU.cr |= CR_CR0_EQ;
			}
		}
	}
	if (aCPU.xer & XER_SO) aCPU.cr |= CR_CR0_SO;
}

void ppc_opc_sth(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
    ppc_write_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, (uint16)aCPU.gpr[rS]);
}

void ppc_opc_sthbrx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	ppc_write_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], ppc_bswap_half(aCPU.gpr[rS]));
}
void ppc_opc_sthu(PPC_CPU_State &aCPU)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
	if (ppc_write_effective_half(aCPU, aCPU.gpr[rA] + imm, (uint16)aCPU.gpr[rS]) == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
	}
}
void ppc_opc_sthux(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	if (ppc_write_effective_half(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], (uint16)aCPU.gpr[rS]) == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
	}
}
void ppc_opc_sthx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	ppc_write_effective_half(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], (uint16)aCPU.gpr[rS]);
}
void ppc_opc_stmw(PPC_CPU_State &aCPU)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
	uint32 ea = (rA ? aCPU.gpr[rA] : 0) + imm;
	for (int i = rS; i <= 31; i++) {
		if (ppc_write_effective_word(aCPU, ea, aCPU.gpr[i]) != PPC_MMU_OK) return;
		ea += 4;
	}
}
void ppc_opc_stswi(PPC_CPU_State &aCPU)
{
	int rA, rS, NB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, NB);
	if (NB == 0) NB = 32;
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
		if (ppc_write_effective_byte(aCPU, ea, (r >> 24))) return;
		r <<= 8;
		ea++;
		i--;
		NB--;
	}
}
void ppc_opc_stswx(PPC_CPU_State &aCPU)
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
		if (ppc_write_effective_byte(aCPU, ea, (r >> 24))) return;
		r <<= 8;
		ea++;
		i--;
		NB--;
	}
}

/*
 * FPU load/store opcodes
 * These just transfer data between fpr[] and memory.
 * Double: 8 bytes transferred as-is (raw IEEE 754 bits)
 * Single: 4 bytes with double↔single conversion
 */

#define FPU_CHECK(cpu) \
	if (!(cpu.msr & MSR_FP)) { \
		return; \
	}

void ppc_opc_lfd(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frD, rA, imm);
	uint64 r;
	if (ppc_read_effective_dword(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, r) == PPC_MMU_OK) {
		aCPU.fpr[frD] = r;
	}
}
void ppc_opc_lfdu(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frD, rA, imm);
	uint64 r;
	if (ppc_read_effective_dword(aCPU, aCPU.gpr[rA] + imm, r) == PPC_MMU_OK) {
		aCPU.fpr[frD] = r;
		aCPU.gpr[rA] += imm;
	}
}
void ppc_opc_lfdux(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
	uint64 r;
	if (ppc_read_effective_dword(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], r) == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
		aCPU.fpr[frD] = r;
	}
}
void ppc_opc_lfdx(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
	uint64 r;
	if (ppc_read_effective_dword(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r) == PPC_MMU_OK) {
		aCPU.fpr[frD] = r;
	}
}
void ppc_opc_lfs(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frD, rA, imm);
	uint32 r;
	if (ppc_read_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, r) == PPC_MMU_OK) {
		ppc_single s;
		ppc_double d;
		ppc_fpu_unpack_single(s, r);
		ppc_fpu_single_to_double(s, d);
		ppc_fpu_pack_double(aCPU.fpscr, d, aCPU.fpr[frD]);
	}
}
void ppc_opc_lfsu(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frD, rA, imm);
	uint32 r;
	if (ppc_read_effective_word(aCPU, aCPU.gpr[rA] + imm, r) == PPC_MMU_OK) {
		ppc_single s;
		ppc_double d;
		ppc_fpu_unpack_single(s, r);
		ppc_fpu_single_to_double(s, d);
		ppc_fpu_pack_double(aCPU.fpscr, d, aCPU.fpr[frD]);
		aCPU.gpr[rA] += imm;
	}
}
void ppc_opc_lfsux(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
	uint32 r;
	if (ppc_read_effective_word(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], r) == PPC_MMU_OK) {
		ppc_single s;
		ppc_double d;
		ppc_fpu_unpack_single(s, r);
		ppc_fpu_single_to_double(s, d);
		ppc_fpu_pack_double(aCPU.fpscr, d, aCPU.fpr[frD]);
		aCPU.gpr[rA] += aCPU.gpr[rB];
	}
}
void ppc_opc_lfsx(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
	uint32 r;
	if (ppc_read_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], r) == PPC_MMU_OK) {
		ppc_single s;
		ppc_double d;
		ppc_fpu_unpack_single(s, r);
		ppc_fpu_single_to_double(s, d);
		ppc_fpu_pack_double(aCPU.fpscr, d, aCPU.fpr[frD]);
	}
}
void ppc_opc_stfd(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frS, rA, imm);
	ppc_write_effective_dword(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, aCPU.fpr[frS]);
}
void ppc_opc_stfdu(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frS, rA, imm);
	if (ppc_write_effective_dword(aCPU, aCPU.gpr[rA] + imm, aCPU.fpr[frS]) == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
	}
}
void ppc_opc_stfdux(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
	if (ppc_write_effective_dword(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], aCPU.fpr[frS]) == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
	}
}
void ppc_opc_stfdx(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
	ppc_write_effective_dword(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], aCPU.fpr[frS]);
}
void ppc_opc_stfiwx(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
	ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], (uint32)aCPU.fpr[frS]);
}
void ppc_opc_stfs(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frS, rA, imm);
	ppc_double d;
	ppc_fpu_unpack_double(d, aCPU.fpr[frS]);
	uint32 s;
	ppc_fpu_pack_single(aCPU.fpscr, d, s);
	ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + imm, s);
}
void ppc_opc_stfsu(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frS, rA, imm);
	ppc_double d;
	ppc_fpu_unpack_double(d, aCPU.fpr[frS]);
	uint32 s;
	ppc_fpu_pack_single(aCPU.fpscr, d, s);
	if (ppc_write_effective_word(aCPU, aCPU.gpr[rA] + imm, s) == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
	}
}
void ppc_opc_stfsux(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
	ppc_double d;
	ppc_fpu_unpack_double(d, aCPU.fpr[frS]);
	uint32 s;
	ppc_fpu_pack_single(aCPU.fpscr, d, s);
	if (ppc_write_effective_word(aCPU, aCPU.gpr[rA] + aCPU.gpr[rB], s) == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
	}
}
void ppc_opc_stfsx(PPC_CPU_State &aCPU)
{
	FPU_CHECK(aCPU);
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
	ppc_double d;
	ppc_fpu_unpack_double(d, aCPU.fpr[frS]);
	uint32 s;
	ppc_fpu_pack_single(aCPU.fpscr, d, s);
	ppc_write_effective_word(aCPU, (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB], s);
}

/* Altivec load/store stubs */
void ppc_opc_lvx(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_lvxl(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_lvebx(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_lvehx(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_lvewx(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_lvsl(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_lvsr(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_dst(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_stvx(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_stvxl(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_stvebx(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_stvehx(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_stvewx(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_dstst(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
}
void ppc_opc_dss(PPC_CPU_State &aCPU)
{
	PPC_MMU_ERR("UNIMPLEMENTED opcode %08x at pc=%08x\n", aCPU.current_opc, aCPU.pc);
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
		jitc.emitMOV32((NativeReg)0, (uint32)simm);
	} else {
		jitc.emitLDR32_cpu((NativeReg)0, GPR_OFS(rA));
		if (simm > 0 && simm < 4096) {
			jitc.emit32(a64_ADDw_imm(0, 0, simm));
		} else if (simm < 0 && (-simm) < 4096) {
			jitc.emit32(a64_SUBw_imm(0, 0, -simm));
		} else if (simm != 0) {
			jitc.emitMOV32((NativeReg)17, (uint32)simm);
			jitc.emit32(a64_ADDw_reg(0, 0, 17));
		}
	}
}

/*
 *  lwz rD, d(rA)
 *  rD = MEM[EA, 4]  where EA = (rA|0) + d
 */
JITCFlow ppc_opc_gen_lwz(JITC &jitc)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	jitc.clobberAll();

	// Store pc_ofs for exception handling
	jitc.emitMOV32((NativeReg)16, jitc.pc);
	jitc.emitSTR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, pc_ofs));

	gen_ea_D(jitc, rA, (sint32)imm);
	jitc.emitBLR((NativeAddress)ppc_read_effective_word_asm);
	// W0 = result
	jitc.emitSTR32_cpu((NativeReg)0, GPR_OFS(rD));
	return flowContinue;
}

/*
 *  stw rS, d(rA)
 *  MEM[EA, 4] = rS  where EA = (rA|0) + d
 */
JITCFlow ppc_opc_gen_stw(JITC &jitc)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
	jitc.clobberAll();

	jitc.emitMOV32((NativeReg)16, jitc.pc);
	jitc.emitSTR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, pc_ofs));

	gen_ea_D(jitc, rA, (sint32)imm);
	// W1 = value to store
	jitc.emitLDR32_cpu((NativeReg)1, GPR_OFS(rS));
	jitc.emitBLR((NativeAddress)ppc_write_effective_word_asm);
	return flowContinue;
}

/*
 *  lbz rD, d(rA)
 *  rD = MEM[EA, 1]  (zero-extended)
 */
JITCFlow ppc_opc_gen_lbz(JITC &jitc)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	jitc.clobberAll();

	jitc.emitMOV32((NativeReg)16, jitc.pc);
	jitc.emitSTR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, pc_ofs));

	gen_ea_D(jitc, rA, (sint32)imm);
	jitc.emitBLR((NativeAddress)ppc_read_effective_byte_asm);
	jitc.emitSTR32_cpu((NativeReg)0, GPR_OFS(rD));
	return flowContinue;
}

/*
 *  stb rS, d(rA)
 *  MEM[EA, 1] = rS[24:31]
 */
JITCFlow ppc_opc_gen_stb(JITC &jitc)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
	jitc.clobberAll();

	jitc.emitMOV32((NativeReg)16, jitc.pc);
	jitc.emitSTR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, pc_ofs));

	gen_ea_D(jitc, rA, (sint32)imm);
	jitc.emitLDR32_cpu((NativeReg)1, GPR_OFS(rS));
	jitc.emitBLR((NativeAddress)ppc_write_effective_byte_asm);
	return flowContinue;
}

/*
 *  lhz rD, d(rA)
 *  rD = MEM[EA, 2]  (zero-extended)
 */
JITCFlow ppc_opc_gen_lhz(JITC &jitc)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	jitc.clobberAll();

	jitc.emitMOV32((NativeReg)16, jitc.pc);
	jitc.emitSTR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, pc_ofs));

	gen_ea_D(jitc, rA, (sint32)imm);
	jitc.emitBLR((NativeAddress)ppc_read_effective_half_z_asm);
	jitc.emitSTR32_cpu((NativeReg)0, GPR_OFS(rD));
	return flowContinue;
}

/*
 *  sth rS, d(rA)
 *  MEM[EA, 2] = rS[16:31]
 */
JITCFlow ppc_opc_gen_sth(JITC &jitc)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
	jitc.clobberAll();

	jitc.emitMOV32((NativeReg)16, jitc.pc);
	jitc.emitSTR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, pc_ofs));

	gen_ea_D(jitc, rA, (sint32)imm);
	jitc.emitLDR32_cpu((NativeReg)1, GPR_OFS(rS));
	jitc.emitBLR((NativeAddress)ppc_write_effective_half_asm);
	return flowContinue;
}
