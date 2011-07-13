/*
 *	PearPC
 *	ppc_mmu.cc
 *
 *	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
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

#include "jitc_asm.h"

byte *gMemory = NULL;
uint32 gMemorySize;

#undef TLB

static int ppc_pte_protection[] = {
	// read(0)/write(1) key pp
	
	// read
	1, // r/w
	1, // r/w
	1, // r/w
	1, // r
	0, // -
	1, // r
	1, // r/w
	1, // r
	
	// write
	1, // r/w
	1, // r/w
	1, // r/w
	0, // r
	0, // -
	0, // r
	1, // r/w
	0, // r
};

int FASTCALL ppc_effective_to_physical(PPC_CPU_State &aCPU, uint32 addr, int flags, uint32 &result)
{
	if (flags & PPC_MMU_CODE) {
		if (!(aCPU.msr & MSR_IR)) {
			result = addr;
			return PPC_MMU_OK;
		}
		/*
		 * BAT translation .329
		 */
		for (int i=0; i<4; i++) {
			if ((addr & aCPU.ibat_bl[i]) == aCPU.ibat_bepi[i]) {
				// bat applies to this address
				if (((aCPU.ibatu[i] & BATU_Vs) && !(aCPU.msr & MSR_PR))
				 || ((aCPU.ibatu[i] & BATU_Vp) &&  (aCPU.msr & MSR_PR))) {
					// bat entry valid
					addr &= aCPU.ibat_nbl[i];
					addr |= aCPU.ibat_brpn[i];
					result = addr;
					// FIXME: check access rights
					return PPC_MMU_OK;
				}
			}
		}
	} else {
		if (!(aCPU.msr & MSR_DR)) {
			result = addr;
			return PPC_MMU_OK;
		}
		/*
		 * BAT translation .329
		 */
		for (int i=0; i<4; i++) {
			if ((addr & aCPU.dbat_bl[i]) == aCPU.dbat_bepi[i]) {
				// bat applies to this address
				if (((aCPU.dbatu[i] & BATU_Vs) && !(aCPU.msr & MSR_PR))
				 || ((aCPU.dbatu[i] & BATU_Vp) &&  (aCPU.msr & MSR_PR))) {
					// bat entry valid
					addr &= aCPU.dbat_nbl[i];
					addr |= aCPU.dbat_brpn[i];
					result = addr;
					// FIXME: check access rights
					return PPC_MMU_OK;
				}
			}
		}
	}
	
	/*
	 * Address translation with segment register
	 */
	uint32 sr = aCPU.sr[EA_SR(addr)];

	if (sr & SR_T) {
		// woea
		// FIXME: implement me
		PPC_MMU_ERR("sr & T\n");
	} else {
#ifdef TLB	
		for (int i=0; i<4; i++) {
			if ((addr & ~0xfff) == (aCPU.tlb_va[i])) {
				aCPU.tlb_last = i;
//				ht_printf("TLB: %d: %08x -> %08x\n", i, addr, aCPU.tlb_pa[i] | (addr & 0xfff));
				result = aCPU.tlb_pa[i] | (addr & 0xfff);
				return PPC_MMU_OK;
			}
		}
#endif
		// page address translation
		if ((flags & PPC_MMU_CODE) && (sr & SR_N)) {
			// segment isnt executable
			return PPC_MMU_FATAL;
		}
		uint32 offset = EA_Offset(addr);	 // 12 bit
		uint32 page_index = EA_PageIndex(addr);  // 16 bit
		uint32 VSID = SR_VSID(sr);	       // 24 bit
		uint32 api = EA_API(addr);	       //  6 bit (part of page_index)
		// VSID.page_index = Virtual Page Number (VPN)

		// Hashfunction no 1 "xor" .360
		uint32 hash1 = (VSID ^ page_index);
		uint32 pteg_addr = ((hash1 & aCPU.pagetable_hashmask)<<6) | aCPU.pagetable_base;
		for (int i=0; i<8; i++) {
			uint32 pte;
			if (ppc_read_physical_word(pteg_addr, pte)) {
				if (!(flags & PPC_MMU_NO_EXC)) {
					PPC_MMU_ERR("read physical in address translate failed\n");
					return PPC_MMU_EXC;
				}
				return PPC_MMU_FATAL;
			}
			if ((pte & PTE1_V) && (!(pte & PTE1_H))) {
				if (VSID == PTE1_VSID(pte) && (api == PTE1_API(pte))) {
					// page found
					if (ppc_read_physical_word(pteg_addr+4, pte)) {
						return PPC_MMU_FATAL;
					}
					// check accessmode .346
					int key;
					if (aCPU.msr & MSR_PR) {
						key = (sr & SR_Kp) ? 4 : 0;
					} else {
						key = (sr & SR_Ks) ? 4 : 0;
					}
					if (!ppc_pte_protection[((flags&PPC_MMU_WRITE)?8:0) + key + PTE2_PP(pte)]) {
						return PPC_MMU_FATAL;
					}
					// ok..
					uint32 pap = PTE2_RPN(pte);
					result = pap | offset;
#ifdef TLB
					aCPU.tlb_last++;
					aCPU.tlb_last &= 3;
					aCPU.tlb_pa[aCPU.tlb_last] = pap;
					aCPU.tlb_va[aCPU.tlb_last] = addr & ~0xfff;					
//					ht_printf("TLB: STORE %d: %08x -> %08x\n", aCPU.tlb_last, addr, pap);
#endif
					// update access bits
					// FIXME: is someone actually using this?
					if (flags & PPC_MMU_WRITE) {
						pte |= PTE2_C | PTE2_R;
					} else {
						pte |= PTE2_R;
					}
					ppc_write_physical_word(pteg_addr+4, pte);
					return PPC_MMU_OK;
				}
			}
			pteg_addr+=8;
		}
		
		// Hashfunction no 2 "not" .360
		hash1 = ~hash1;
		pteg_addr = ((hash1 & aCPU.pagetable_hashmask)<<6) | aCPU.pagetable_base;
		for (int i=0; i<8; i++) {
			uint32 pte;
			if (ppc_read_physical_word(pteg_addr, pte)) {
				if (!(flags & PPC_MMU_NO_EXC)) {
					PPC_MMU_ERR("read physical in address translate failed\n");
					return PPC_MMU_EXC;
				}
				return PPC_MMU_FATAL;
			}
			if ((pte & PTE1_V) && (pte & PTE1_H)) {
				if (VSID == PTE1_VSID(pte) && (api == PTE1_API(pte))) {
					// page found
					if (ppc_read_physical_word(pteg_addr+4, pte)) {
						if (!(flags & PPC_MMU_NO_EXC)) {
							PPC_MMU_ERR("read physical in address translate failed\n");
							return PPC_MMU_EXC;
						}
						return PPC_MMU_FATAL;
					}
					// check accessmode
					int key;
					if (aCPU.msr & MSR_PR) {
						key = (sr & SR_Kp) ? 4 : 0;
					} else {
						key = (sr & SR_Ks) ? 4 : 0;
					}
					if (!ppc_pte_protection[((flags&PPC_MMU_WRITE)?8:0) + key + PTE2_PP(pte)]) {
						return PPC_MMU_FATAL;
					}
					// ok..
					result = PTE2_RPN(pte) | offset;
					
					// update access bits
					// FIXME: is someone actually using this?
					if (flags & PPC_MMU_WRITE) {
						pte |= PTE2_C | PTE2_R;
					} else {
						pte |= PTE2_R;
					}
					ppc_write_physical_word(pteg_addr+4, pte);
//					PPC_MMU_WARN("hash function 2 used!\n");
//					gSinglestep = true;
					return PPC_MMU_OK;
				}
			}
			pteg_addr+=8;
		}
	}
	// page fault
	return PPC_MMU_FATAL;
}

int FASTCALL ppc_effective_to_physical_vm(PPC_CPU_State &aCPU, uint32 addr, int flags, uint32 &result)
{
	if (!(aCPU.msr & MSR_DR)) {
		result = addr;
		return PPC_MMU_READ | PPC_MMU_WRITE;
	}
	/*
	 * BAT translation .329
	 */
	for (int i=0; i<4; i++) {
		if ((addr & aCPU.dbat_bl[i]) == aCPU.dbat_bepi[i]) {
			// bat applies to this address
			if (((aCPU.dbatu[i] & BATU_Vs) && !(aCPU.msr & MSR_PR))
			 || ((aCPU.dbatu[i] & BATU_Vp) &&  (aCPU.msr & MSR_PR))) {
				// bat entry valid
				addr &= aCPU.dbat_nbl[i];
				addr |= aCPU.dbat_brpn[i];
				result = addr;
				// FIXME: check access rights
				return PPC_MMU_OK;
			}
		}
	}
	
	/*
	 * Address translation with segment register
	 */
	uint32 sr = aCPU.sr[EA_SR(addr)];

	if (sr & SR_T) {
		// woea
		// FIXME: implement me
		PPC_MMU_ERR("sr & T\n");
	} else {
		// page address translation
		uint32 offset = EA_Offset(addr);         // 12 bit
		uint32 page_index = EA_PageIndex(addr);  // 16 bit
		uint32 VSID = SR_VSID(sr);               // 24 bit
		uint32 api = EA_API(addr);               //  6 bit (part of page_index)
		// VSID.page_index = Virtual Page Number (VPN)

		// Hashfunction no 1 "xor" .360
		uint32 hash1 = (VSID ^ page_index);
		uint32 pteg_addr = ((hash1 & aCPU.pagetable_hashmask)<<6) | aCPU.pagetable_base;
		for (int i=0; i<8; i++) {
			uint32 pte;
			if (ppc_read_physical_word(pteg_addr, pte)) {
				return PPC_MMU_FATAL;
			}
			if ((pte & PTE1_V) && (!(pte & PTE1_H))) {
				if (VSID == PTE1_VSID(pte) && (api == PTE1_API(pte))) {
					// page found
					if (ppc_read_physical_word(pteg_addr+4, pte)) {
						return 0;
					}
					// check accessmode .346
					int key;
					if (aCPU.msr & MSR_PR) {
						key = (sr & SR_Kp) ? 4 : 0;
					} else {
						key = (sr & SR_Ks) ? 4 : 0;
					}
					int ret = PPC_MMU_WRITE | PPC_MMU_READ;
					if (!ppc_pte_protection[8 + key + PTE2_PP(pte)]) {
						if (!(flags & PPC_MMU_NO_EXC)) {
							if (flags & PPC_MMU_WRITE) {
								aCPU.dsisr = PPC_EXC_DSISR_PROT | PPC_EXC_DSISR_STORE;
							}
						}
						ret &= ~PPC_MMU_WRITE;
					}
					if (!ppc_pte_protection[key + PTE2_PP(pte)]) {
						if (!(flags & PPC_MMU_NO_EXC)) {
							if (!(flags & PPC_MMU_WRITE)) {
								aCPU.dsisr = PPC_EXC_DSISR_PROT;
							}
						}
						return PPC_MMU_OK;
					}
					// ok..
					uint32 pap = PTE2_RPN(pte);
					result = pap | offset;
					// update access bits
					if (ret & PPC_MMU_WRITE) {
						pte |= PTE2_C | PTE2_R;
					} else {
						pte |= PTE2_R;
					}
					ppc_write_physical_word(pteg_addr+4, pte);
					return ret;
				}
			}
			pteg_addr+=8;
		}
		
		// Hashfunction no 2 "not" .360
		hash1 = ~hash1;
		pteg_addr = ((hash1 & aCPU.pagetable_hashmask)<<6) | aCPU.pagetable_base;
		for (int i=0; i<8; i++) {
			uint32 pte;
			if (ppc_read_physical_word(pteg_addr, pte)) {
				return PPC_MMU_FATAL;
			}
			if ((pte & PTE1_V) && (pte & PTE1_H)) {
				if (VSID == PTE1_VSID(pte) && (api == PTE1_API(pte))) {
					// page found
					if (ppc_read_physical_word(pteg_addr+4, pte)) {
						return 0;
					}
					// check accessmode
					int key;
					if (aCPU.msr & MSR_PR) {
						key = (sr & SR_Kp) ? 4 : 0;
					} else {
						key = (sr & SR_Ks) ? 4 : 0;
					}
					int ret = PPC_MMU_WRITE | PPC_MMU_READ;
					if (!ppc_pte_protection[8 + key + PTE2_PP(pte)]) {
						if (!(flags & PPC_MMU_NO_EXC)) {
							if (flags & PPC_MMU_WRITE) {
								aCPU.dsisr = PPC_EXC_DSISR_PROT | PPC_EXC_DSISR_STORE;
							}
						}
						ret &= ~PPC_MMU_WRITE;
					}
					if (!ppc_pte_protection[key + PTE2_PP(pte)]) {
						if (!(flags & PPC_MMU_NO_EXC)) {
							if (!(flags & PPC_MMU_WRITE)) {
								aCPU.dsisr = PPC_EXC_DSISR_PROT;
							}
						}
						return PPC_MMU_OK;
					}
					// ok..
					result = PTE2_RPN(pte) | offset;
					
					// update access bits
					if (ret & PPC_MMU_WRITE) {
						pte |= PTE2_C | PTE2_R;
					} else {
						pte |= PTE2_R;
					}
					ppc_write_physical_word(pteg_addr+4, pte);
					return ret;
				}
			}
			pteg_addr+=8;
		}
	}
	// page fault
	if (!(flags & PPC_MMU_NO_EXC)) {
		if (flags & PPC_MMU_WRITE) {
			aCPU.dsisr = PPC_EXC_DSISR_PAGE | PPC_EXC_DSISR_STORE;
		} else {
			aCPU.dsisr = PPC_EXC_DSISR_PAGE;
		}
	}
	return PPC_MMU_OK;
}

void ppc_mmu_tlb_invalidate(PPC_CPU_State &aCPU)
{
	ppc_mmu_tlb_invalidate_all_asm(&aCPU);
}

/*
pagetable:
min. 2^10 (64k) PTEGs
PTEG = 64byte
The page table can be any size 2^n where 16 <= n <= 25.

A PTEG contains eight
PTEs of eight bytes each; therefore, each PTEG is 64 bytes long.
*/

bool FASTCALL ppc_mmu_set_sdr1(PPC_CPU_State &aCPU, uint32 newval, bool quiesce)
{
	/* if (newval == aCPU.sdr1)*/ quiesce = false;
	PPC_MMU_TRACE("new pagetable: sdr1 = 0x%08x\n", newval);
	uint32 htabmask = SDR1_HTABMASK(newval);
	uint32 x = 1;
	uint32 xx = 0;
	int n = 0;
	while ((htabmask & x) && (n < 9)) {
		n++;
		xx|=x;
		x<<=1;
	}
	if (htabmask & ~xx) {
		PPC_MMU_WARN("new pagetable: broken htabmask (%05x)\n", htabmask);
		return false;
	}
	uint32 htaborg = SDR1_HTABORG(newval);
	if (htaborg & xx) {
		PPC_MMU_WARN("new pagetable: broken htaborg (%05x)\n", htaborg);
		return false;
	}
	aCPU.pagetable_base = htaborg<<16;
	aCPU.sdr1 = newval;
	aCPU.pagetable_hashmask = ((xx<<10)|0x3ff);
	uint a = (0xffffffff & aCPU.pagetable_hashmask) | aCPU.pagetable_base;
	if (a > gMemorySize) {
		PPC_MMU_WARN("new pagetable: not in memory (%08x)\n", a);
		return false;
	}	
	PPC_MMU_TRACE("new pagetable: sdr1 accepted\n");
	PPC_MMU_TRACE("number of pages: 2^%d pagetable_start: 0x%08x size: 2^%d\n", n+13, aCPU.pagetable_base, n+16);
	if (quiesce) {
		prom_quiesce();
	}
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

int FASTCALL ppc_read_physical_qword(uint32 addr, Vector_t &result)
{
	if (addr < gMemorySize) {
		// big endian
		VECT_D(result,0) = ppc_dword_from_BE(*((uint64*)(gMemory+addr)));
		VECT_D(result,1) = ppc_dword_from_BE(*((uint64*)(gMemory+addr+8)));
		return PPC_MMU_OK;
	}
	return io_mem_read128(addr, (uint128 *)&result);
}

int FASTCALL ppc_read_physical_dword(uint32 addr, uint64 &result)
{
	if (addr < gMemorySize) {
		// big endian
		result = ppc_dword_from_BE(*((uint64*)(gMemory+addr)));
		return PPC_MMU_OK;
	}
	int ret = io_mem_read64(addr, result);
	result = ppc_bswap_dword(result);
	return ret;
}

int FASTCALL ppc_read_physical_word(uint32 addr, uint32 &result)
{
	if (addr < gMemorySize) {
		// big endian
		result = ppc_word_from_BE(*((uint32*)(gMemory+addr)));
		return PPC_MMU_OK;
	}
	int ret = io_mem_read(addr, result, 4);
	result = ppc_bswap_word(result);
	return ret;
}

int FASTCALL ppc_read_physical_half(uint32 addr, uint16 &result)
{
	if (addr < gMemorySize) {
		// big endian
		result = ppc_half_from_BE(*((uint16*)(gMemory+addr)));
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
		// big endian
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
	if (addr & 3) {
		// EXC..bla
		return PPC_MMU_FATAL;
	}
	uint32 p;
	int r;
	if (!((r=ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ | PPC_MMU_CODE, p)))) {
		return ppc_read_physical_word(p, result);
	}
	return r;
}

int FASTCALL ppc_read_effective_qword(PPC_CPU_State &aCPU, uint32 addr, Vector_t &result)
{
	uint32 p;
	int r;

	addr &= ~0x0f;

	if (!(r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ, p))) {
		return ppc_read_physical_qword(p, result);
	}

	return r;
}

int FASTCALL ppc_read_effective_dword(PPC_CPU_State &aCPU, uint32 addr, uint64 &result)
{
	uint32 p;
	int r;
	if (!(r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ, p))) {
		if (EA_Offset(addr) > 4088) {
			// read overlaps two pages.. tricky
			byte *r1, *r2;
			byte b[14];
			ppc_effective_to_physical(aCPU, (addr & ~0xfff)+4089, PPC_MMU_READ, p);
			if ((r = ppc_direct_physical_memory_handle(p, r1))) return r;
			if ((r = ppc_effective_to_physical(aCPU, (addr & ~0xfff)+4096, PPC_MMU_READ, p))) return r;
			if ((r = ppc_direct_physical_memory_handle(p, r2))) return r;
			memmove(&b[0], r1, 7);
			memmove(&b[7], r2, 7);
			memmove(&result, &b[EA_Offset(addr)-4089], 8);
			result = ppc_dword_from_BE(result);
			return PPC_MMU_OK;
		} else {
			return ppc_read_physical_dword(p, result);
		}
	}
	return r;
}

int FASTCALL ppc_read_effective_word(PPC_CPU_State &aCPU, uint32 addr, uint32 &result)
{
	uint32 p;
	int r;
	if (!(r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ, p))) {
		if (EA_Offset(addr) > 4092) {
			// read overlaps two pages.. tricky
			byte *r1, *r2;
			byte b[6];
			ppc_effective_to_physical(aCPU, (addr & ~0xfff)+4093, PPC_MMU_READ, p);
			if ((r = ppc_direct_physical_memory_handle(p, r1))) return r;
			if ((r = ppc_effective_to_physical(aCPU, (addr & ~0xfff)+4096, PPC_MMU_READ, p))) return r;
			if ((r = ppc_direct_physical_memory_handle(p, r2))) return r;
			memmove(&b[0], r1, 3);
			memmove(&b[3], r2, 3);
			memmove(&result, &b[EA_Offset(addr)-4093], 4);
			result = ppc_word_from_BE(result);
			return PPC_MMU_OK;
		} else {
			return ppc_read_physical_word(p, result);
		}
	}
	return r;
}

int FASTCALL ppc_read_effective_half(PPC_CPU_State &aCPU, uint32 addr, uint16 &result)
{
	uint32 p;
	int r;
	if (!((r = ppc_effective_to_physical(aCPU, addr, PPC_MMU_READ, p)))) {
		if (EA_Offset(addr) > 4094) {
			// read overlaps two pages.. tricky
			byte b1, b2;
			ppc_effective_to_physical(aCPU, (addr & ~0xfff)+4095, PPC_MMU_READ, p);
			if ((r = ppc_read_physical_byte(p, b1))) return r;
			if ((r = ppc_effective_to_physical(aCPU, (addr & ~0xfff)+4096, PPC_MMU_READ, p))) return r;
			if ((r = ppc_read_physical_byte(p, b2))) return r;
			result = (b1<<8)|b2;
			return PPC_MMU_OK;
		} else {
			return ppc_read_physical_half(p, result);
		}
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

int FASTCALL ppc_write_physical_qword(uint32 addr, Vector_t data)
{
	if (addr < gMemorySize) {
		// big endian
		*((uint64*)(gMemory+addr)) = ppc_dword_to_BE(VECT_D(data,0));
		*((uint64*)(gMemory+addr+8)) = ppc_dword_to_BE(VECT_D(data,1));
		return PPC_MMU_OK;
	}
	if (io_mem_write128(addr, (uint128 *)&data) == IO_MEM_ACCESS_OK) {
		return PPC_MMU_OK;
	} else {
		return PPC_MMU_FATAL;
	}
}

int FASTCALL ppc_write_physical_dword(uint32 addr, uint64 data)
{
	if (addr < gMemorySize) {
		// big endian
		*((uint64*)(gMemory+addr)) = ppc_dword_to_BE(data);
		return PPC_MMU_OK;
	}
	if (io_mem_write64(addr, ppc_bswap_dword(data)) == IO_MEM_ACCESS_OK) {
		return PPC_MMU_OK;
	} else {
		return PPC_MMU_FATAL;
	}
}

int FASTCALL ppc_write_physical_word(uint32 addr, uint32 data)
{
	if (addr < gMemorySize) {
		// big endian
		*((uint32*)(gMemory+addr)) = ppc_word_to_BE(data);
		return PPC_MMU_OK;
	}
	return io_mem_write(addr, ppc_bswap_word(data), 4);
}

int FASTCALL ppc_write_physical_half(uint32 addr, uint16 data)
{
	if (addr < gMemorySize) {
		// big endian
		*((uint16*)(gMemory+addr)) = ppc_half_to_BE(data);
		return PPC_MMU_OK;
	}
	return io_mem_write(addr, ppc_bswap_half(data), 2);
}

int FASTCALL ppc_write_physical_byte(uint32 addr, uint8 data)
{
	if (addr < gMemorySize) {
		// big endian
		gMemory[addr] = data;
		return PPC_MMU_OK;
	}
	return io_mem_write(addr, data, 1);
}

int FASTCALL ppc_write_effective_qword(PPC_CPU_State &aCPU, uint32 addr, Vector_t data)
{
	uint32 p;
	int r;

	addr &= ~0x0f;

	if (!((r=ppc_effective_to_physical(aCPU, addr, PPC_MMU_WRITE, p)))) {
		return ppc_write_physical_qword(p, data);
	}
	return r;
}

int FASTCALL ppc_write_effective_dword(PPC_CPU_State &aCPU, uint32 addr, uint64 data)
{
	uint32 p;
	int r;
	if (!((r=ppc_effective_to_physical(aCPU, addr, PPC_MMU_WRITE, p)))) {
		if (EA_Offset(addr) > 4088) {
			// write overlaps two pages.. tricky
			byte *r1, *r2;
			byte b[14];
			ppc_effective_to_physical(aCPU, (addr & ~0xfff)+4089, PPC_MMU_WRITE, p);
			if ((r = ppc_direct_physical_memory_handle(p, r1))) return r;
			if ((r = ppc_effective_to_physical(aCPU, (addr & ~0xfff)+4096, PPC_MMU_WRITE, p))) return r;
			if ((r = ppc_direct_physical_memory_handle(p, r2))) return r;
			data = ppc_dword_to_BE(data);
			memmove(&b[0], r1, 7);
			memmove(&b[7], r2, 7);
			memmove(&b[EA_Offset(addr)-4089], &data, 8);
			memmove(r1, &b[0], 7);
			memmove(r2, &b[7], 7);
			return PPC_MMU_OK;
		} else {
			return ppc_write_physical_dword(p, data);
		}
	}
	return r;
}

int FASTCALL ppc_write_effective_word(PPC_CPU_State &aCPU, uint32 addr, uint32 data)
{
	uint32 p;
	int r;
	if (!((r=ppc_effective_to_physical(aCPU, addr, PPC_MMU_WRITE, p)))) {
		if (EA_Offset(addr) > 4092) {
			// write overlaps two pages.. tricky
			byte *r1, *r2;
			byte b[6];
			ppc_effective_to_physical(aCPU, (addr & ~0xfff)+4093, PPC_MMU_WRITE, p);
			if ((r = ppc_direct_physical_memory_handle(p, r1))) return r;
			if ((r = ppc_effective_to_physical(aCPU, (addr & ~0xfff)+4096, PPC_MMU_WRITE, p))) return r;
			if ((r = ppc_direct_physical_memory_handle(p, r2))) return r;
			data = ppc_word_to_BE(data);
			memmove(&b[0], r1, 3);
			memmove(&b[3], r2, 3);
			memmove(&b[EA_Offset(addr)-4093], &data, 4);
			memmove(r1, &b[0], 3);
			memmove(r2, &b[3], 3);
			return PPC_MMU_OK;
		} else {
			return ppc_write_physical_word(p, data);
		}
	}
	return r;
}

int FASTCALL ppc_write_effective_half(PPC_CPU_State &aCPU, uint32 addr, uint16 data)
{
	uint32 p;
	int r;
	if (!((r=ppc_effective_to_physical(aCPU, addr, PPC_MMU_WRITE, p)))) {
		if (EA_Offset(addr) > 4094) {
			// write overlaps two pages.. tricky
			ppc_effective_to_physical(aCPU, (addr & ~0xfff)+4095, PPC_MMU_WRITE, p);
			if ((r = ppc_write_physical_byte(p, data>>8))) return r;
			if ((r = ppc_effective_to_physical(aCPU, (addr & ~0xfff)+4096, PPC_MMU_WRITE, p))) return r;
			if ((r = ppc_write_physical_byte(p, data))) return r;
			return PPC_MMU_OK;
		} else {
			return ppc_write_physical_half(p, data);
		}
	}
	return r;
}

int FASTCALL ppc_write_effective_byte(PPC_CPU_State &aCPU, uint32 addr, uint8 data)
{
	uint32 p;
	int r;
	if (!((r=ppc_effective_to_physical(aCPU, addr, PPC_MMU_WRITE, p)))) {
		return ppc_write_physical_byte(p, data);
	}
	return r;
}

bool FASTCALL ppc_init_physical_memory(uint size)
{
	if (size < 64*1024*1024) {
		PPC_MMU_ERR("Main memory size must >= 64MB!\n");
	}
	gMemory = (byte*)malloc(size+4095);
//	gMemory = (byte*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED | MAP_32BIT, -1, 0);
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

uint32  ppc_get_memory_size()
{
	return gMemorySize;
}

/***************************************************************************
 *	DMA Interface
 */

bool	ppc_dma_write(uint32 dest, const void *src, uint32 size)
{
	if (dest > gMemorySize || (dest+size) > gMemorySize) return false;
	
	byte *ptr;
	ppc_direct_physical_memory_handle(dest, ptr);
	
	memcpy(ptr, src, size);
	return true;
}

bool	ppc_dma_read(void *dest, uint32 src, uint32 size)
{
	if (src > gMemorySize || (src+size) > gMemorySize) return false;
	
	byte *ptr;
	ppc_direct_physical_memory_handle(src, ptr);
	
	memcpy(dest, ptr, size);
	return true;
}

bool	ppc_dma_set(uint32 dest, int c, uint32 size)
{
	if (dest > gMemorySize || (dest+size) > gMemorySize) return false;
	
	byte *ptr;
	ppc_direct_physical_memory_handle(dest, ptr);
	
	memset(ptr, c, size);
	return true;
}


/***************************************************************************
 *	DEPRECATED prom interface
 */
 
extern PPC_CPU_State *gCPU;
bool ppc_prom_set_sdr1(uint32 newval, bool quiesce)
{
	return ppc_mmu_set_sdr1(*gCPU, newval, quiesce);
}

bool ppc_prom_effective_to_physical(uint32 &result, uint32 ea)
{
	return ppc_effective_to_physical(*gCPU, ea, PPC_MMU_READ|PPC_MMU_SV|PPC_MMU_NO_EXC, result) == PPC_MMU_OK;
}

bool ppc_prom_page_create(uint32 ea, uint32 pa)
{
	PPC_CPU_State &aCPU = *gCPU;
	uint32 sr = aCPU.sr[EA_SR(ea)];
	uint32 page_index = EA_PageIndex(ea);  // 16 bit
	uint32 VSID = SR_VSID(sr);	     // 24 bit
	uint32 api = EA_API(ea);	       //  6 bit (part of page_index)
	uint32 hash1 = (VSID ^ page_index);
	uint32 pte, pte2;
	uint32 h = 0;
	for (int j=0; j<2; j++) {
		uint32 pteg_addr = ((hash1 & aCPU.pagetable_hashmask)<<6) | aCPU.pagetable_base;
		for (int i=0; i<8; i++) {
			if (ppc_read_physical_word(pteg_addr, pte)) {
				PPC_MMU_ERR("read physical in address translate failed\n");
				return false;
			}
			if (!(pte & PTE1_V)) {
				// free pagetable entry found
				pte = PTE1_V | (VSID << 7) | h | api;
				pte2 = (PA_RPN(pa) << 12) | 0;
				if (ppc_write_physical_word(pteg_addr, pte)
				 || ppc_write_physical_word(pteg_addr+4, pte2)) {
					return false;
				} else {
					// ok
					return true;
				}
			}
			pteg_addr+=8;
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

/***************************************************************************
 *	MMU Opcodes
 */

#include "ppc_dec.h"

/* 
 *	puts the sum of cr1 and cr2 into RAX 
 *	(in the most clever way)
 */
static void getRAXRsum(JITC &jitc, PPC_Register cr1, PPC_Register cr2)
{
	NativeReg r1 = jitc.getClientRegisterMapping(cr1);
	NativeReg r2 = jitc.getClientRegisterMapping(cr2);
	if (r1 == RAX) {
		/* intentional left empty */
	} else if (r2 == RAX) {
		if (r1 == REG_NO) {
			jitc.asmALU32(X86_ADD, RAX, curCPUreg(cr1));
		} else {
			jitc.asmALU32(X86_ADD, RAX, r1);
		}
		return;
	} else {
		/*
		 *	We load cr1 into RAX but have to clobber it since
		 *	we're going to modify RAX.
		 */
		jitc.getClientRegister(cr1, NATIVE_REG | RAX);
	}
	jitc.clobberRegister(NATIVE_REG | RAX);
	r2 = jitc.getClientRegisterMapping(cr2);
	if (r2 == REG_NO) {
		jitc.asmALU32(X86_ADD, RAX, curCPUreg(cr2));
	} else {
		jitc.asmALU32(X86_ADD, RAX, r2);
	}
}

static void getRAX_0_Rsum(JITC &jitc, PPC_Register cr1, PPC_Register cr2)
{
	if (cr1 == PPC_GPR(0)) {
		jitc.getClientRegister(cr2, NATIVE_REG | RAX);
	} else {
		getRAXRsum(jitc, cr1, cr2);
	}
}

static void getRAXRsumAndEDX(JITC &jitc, PPC_Register cr1, PPC_Register cr2, PPC_Register cr3)
{
	NativeReg r1 = jitc.getClientRegisterMapping(cr1);
	NativeReg r2 = jitc.getClientRegisterMapping(cr2);
	if (r1 == RAX) {
		jitc.touchRegister(RAX);
		jitc.clobberRegister(NATIVE_REG | RDX);
		if (cr1 == cr3) {
			jitc.asmALU32(X86_MOV, RDX, RAX);
		} else {
			jitc.getClientRegister(cr3, NATIVE_REG | RDX);
		}
		r2 = jitc.getClientRegisterMapping(cr2);
		if (r2 == REG_NO) {
			jitc.asmALU32(X86_ADD, RAX, curCPUreg(cr2));
			return;
		} else {
			jitc.asmALU32(X86_ADD, RAX, r2);
			return;
		}
	} else if (r2 == RAX) {
		jitc.touchRegister(RAX);
		jitc.clobberRegister(NATIVE_REG | RDX);
		if (cr2 == cr3) {
			jitc.asmALU32(X86_MOV, RDX, RAX);
		} else {
			jitc.getClientRegister(cr3, NATIVE_REG | RDX);
		}
		r1 = jitc.getClientRegisterMapping(cr1);
		if (r1 == REG_NO) {
			jitc.asmALU32(X86_ADD, RAX, curCPUreg(cr1));
			return;
		}
		jitc.asmALU32(X86_ADD, RAX, r1);
		return;
	} else {
		jitc.getClientRegister(cr1, NATIVE_REG | RAX);
		jitc.clobberRegister(NATIVE_REG | RDX);
		if (cr1 == cr3) {
			jitc.asmALU32(X86_MOV, RDX, RAX);
		} else {
			jitc.getClientRegister(cr3, NATIVE_REG | RDX);
		}
	}
	// FIXME: what if mapping of cr3==RDX?
	r2 = jitc.getClientRegisterMapping(cr2);
	if (r2 == REG_NO) {
		jitc.asmALU32(X86_ADD, RAX, curCPUreg(cr2));
	} else {
		jitc.asmALU32(X86_ADD, RAX, r2);
	}
}

static void getRAX_0_RsumAndEDX(JITC &jitc, PPC_Register cr1, PPC_Register cr2, PPC_Register cr3)
{
	if (cr1 == PPC_GPR(0)) {
		if (jitc.getClientRegisterMapping(cr2) == RDX) jitc.touchRegister(RDX);
		jitc.getClientRegister(cr2, NATIVE_REG | RAX);
		if (cr2 == cr3) {
			jitc.asmALU32(X86_MOV, RDX, RAX);
		} else {
			jitc.getClientRegister(cr3, NATIVE_REG | RDX);
		}
	} else {
		getRAXRsumAndEDX(jitc, cr1, cr2, cr3);
	}
}

/* 
 *	puts the sum of cr1 and imm into RAX 
 *	(in the most clever way)
 */
static void getRAXIsum(JITC &jitc, PPC_Register cr1, uint32 imm)
{
	jitc.getClientRegister(cr1, NATIVE_REG | RAX);
	if (imm) {
		jitc.asmALU32(X86_ADD, RAX, imm);
	}
}

static void getRAX_0_Isum(JITC &jitc, PPC_Register cr1, uint32 imm)
{
	if (cr1 == PPC_GPR(0)) {
		jitc.asmALU32(X86_MOV, RAX, imm);
	} else {
		getRAXIsum(jitc, cr1, imm);
	}
}

static void getRAXIsumAndEDX(JITC &jitc, PPC_Register cr1, uint32 imm, PPC_Register cr2)
{
	if (jitc.getClientRegisterMapping(cr2) == RDX) jitc.touchRegister(RDX);
	jitc.getClientRegister(cr1, NATIVE_REG | RAX);
	if (cr1 == cr2) {
		jitc.asmALU32(X86_MOV, RDX, RAX);
	} else {
		jitc.getClientRegister(cr2, NATIVE_REG | RDX);
	}
	jitc.clobberRegister(NATIVE_REG | RAX);
	if (imm) {
		jitc.asmALU32(X86_ADD, RAX, imm);
	}
}

static void getRAX_0_IsumAndEDX(JITC &jitc, PPC_Register cr1, uint32 imm, PPC_Register cr2)
{
	if (cr1 == PPC_GPR(0)) {
		jitc.getClientRegister(cr2, NATIVE_REG | RDX);
		jitc.clobberRegister(NATIVE_REG | RAX);
		jitc.asmALU32(X86_MOV, RAX, imm);
	} else {
		getRAXIsumAndEDX(jitc, cr1, imm, cr2);
	}
}


static void ppc_opc_gen_helper_l(JITC &jitc, PPC_Register cr1, uint32 imm)
{
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	getRAX_0_Isum(jitc, cr1, imm);
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
}

static void ppc_opc_gen_helper_lu(JITC &jitc, PPC_Register cr1, uint32 imm)
{
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	getRAXIsum(jitc, cr1, imm);
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
}

static void ppc_opc_gen_helper_lux(JITC &jitc, PPC_Register cr1, PPC_Register cr2)
{
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	getRAXRsum(jitc, cr1, cr2);
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
}

static void ppc_opc_gen_helper_lx(JITC &jitc, PPC_Register cr1, PPC_Register cr2)
{
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	getRAX_0_Rsum(jitc, cr1, cr2);
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
}

static void ppc_opc_gen_helper_st(JITC &jitc, PPC_Register cr1, uint32 imm, PPC_Register cr2)
{
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	getRAX_0_IsumAndEDX(jitc, cr1, imm, cr2);
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
}

static void ppc_opc_gen_helper_stu(JITC &jitc, PPC_Register cr1, uint32 imm, PPC_Register cr2)
{
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	getRAXIsumAndEDX(jitc, cr1, imm, cr2);
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
}

static void ppc_opc_gen_helper_stux(JITC &jitc, PPC_Register cr1, PPC_Register cr2, PPC_Register cr3)
{
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	getRAXRsumAndEDX(jitc,cr1, cr2, cr3);
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
}

static void ppc_opc_gen_helper_stx(JITC &jitc, PPC_Register cr1, PPC_Register cr2, PPC_Register cr3)
{
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	getRAX_0_RsumAndEDX(jitc, cr1, cr2, cr3);
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
}

static void ppc_opc_gen_helper_tlb(JITC &jitc, NativeReg src, NativeReg d1, NativeReg d2)
{
	jitc.asmALU32(X86_MOV, d1, src);
	jitc.asmALU32(X86_MOV, d2, src);
	jitc.asmShift32(X86_SHR, d1, 12);
	jitc.asmALU32(X86_AND, d2, 0xfffff000);
	jitc.asmALU32(X86_AND, d1, TLB_ENTRIES-1);
}

static uint64 FASTCALL ppc_opc_single_to_double(uint32 fpscr, uint32 r)
{
	ppc_single s;
	ppc_double d;
	uint64 ret;
	ppc_fpu_unpack_single(s, r);
	ppc_fpu_single_to_double(s, d);
	ppc_fpu_pack_double(fpscr, d, ret);
	return ret;
}

static uint32 FASTCALL ppc_opc_double_to_single(uint32 fpscr, uint64 r)
{
	uint32 s;
	ppc_double d;
	ppc_fpu_unpack_double(d, r);
	ppc_fpu_pack_single(fpscr, d, s);
	return s;
}


/*
 *	dcbz		Data Cache Clear to Zero
 *	.464
 */
void ppc_opc_dcbz(PPC_CPU_State &aCPU)
{
	//PPC_L1_CACHE_LINE_SIZE
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	// assert rD=0
	uint32 a = (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB];
	// BAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	ppc_write_effective_dword(aCPU, a, 0)
	|| ppc_write_effective_dword(aCPU, a+8, 0)
	|| ppc_write_effective_dword(aCPU, a+16, 0)
	|| ppc_write_effective_dword(aCPU, a+24, 0);
}
JITCFlow ppc_opc_gen_dcbz(JITC &jitc)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	getRAX_0_Rsum(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.clobberRegister();
	jitc.asmALU32(X86_XOR, RDX, RDX);
	jitc.asmALU32(X86_MOV, curCPU(temp), RAX);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	jitc.asmALU32(X86_MOV, RAX, curCPU(temp));
	jitc.asmALU32(X86_XOR, RDX, RDX);
	jitc.asmALU32(X86_ADD, RAX, 8);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	jitc.asmALU32(X86_MOV, RAX, curCPU(temp));
	jitc.asmALU32(X86_XOR, RDX, RDX);
	jitc.asmALU32(X86_ADD, RAX, 16);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	jitc.asmALU32(X86_MOV, RAX, curCPU(temp));
	jitc.asmALU32(X86_XOR, RDX, RDX);
	jitc.asmALU32(X86_ADD, RAX, 24);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	return flowEndBlock;
}

/*
 *	lbz		Load Byte and Zero
 *	.521
 */
void ppc_opc_lbz(PPC_CPU_State &aCPU)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint8 r;
	int ret = ppc_read_effective_byte(aCPU, (rA?aCPU.gpr[rA]:0)+imm, r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lbz(JITC &jitc)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_l(jitc, PPC_GPR(rA), imm);
#if 0
	ppc_opc_gen_helper_tlb(jitc, RAX, RDX, RCX);
	
	jitc.asmALU32(X86_CMP, RCX, curCPUsib(tlb_data_read_eff, 4, RDX));
	NativeAddress fixup1 = jitc.asmJxxFixup(X86_E);
	jitc.asmCALL((NativeAddress)ppc_read_effective_byte_asm);
	NativeAddress fixup2 = jitc.asmJMPFixup();
	
	jitc.asmResolveFixup(fixup1, jitc.asmHERE());
	jitc.asmALU64(X86_ADD, RAX, curCPUsib(tlb_data_read_phys, 8, RDX));
	jitc.asmMOVxx32_8(X86_MOVZX, RDX, RAX, 0u);
	
	jitc.asmResolveFixup(fixup2, jitc.asmHERE());
#else
	jitc.asmCALL((NativeAddress)ppc_read_effective_byte_asm);
#endif
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	return flowContinue;
}
/*
 *	lbzu		Load Byte and Zero with Update
 *	.522
 */
void ppc_opc_lbzu(PPC_CPU_State &aCPU)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	// FIXME: check rA!=0 && rA!=rD
	uint8 r;
	int ret = ppc_read_effective_byte(aCPU, aCPU.gpr[rA]+imm, r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
		aCPU.gpr[rD] = r;
	}	
}
JITCFlow ppc_opc_gen_lbzu(JITC &jitc)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_lu(jitc, PPC_GPR(rA), imm);
#if 0
	ppc_opc_gen_helper_tlb(jitc, RAX, RDX, RCX);
	
	jitc.asmALU32(X86_CMP, RCX, curCPUsib(tlb_data_read_eff, 4, RDX));
	NativeAddress fixup1 = jitc.asmJxxFixup(X86_E);
	jitc.asmCALL((NativeAddress)ppc_read_effective_byte_asm);
	NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | RAX);
	if (imm) {
		jitc.asmALU32(X86_ADD, a, imm);
	}
	NativeAddress fixup2 = jitc.asmJMPFixup();
	
	jitc.asmResolveFixup(fixup1, jitc.asmHERE());
	jitc.asmALU64(X86_MOV, RDX, curCPUsib(tlb_data_read_phys, 8, RDX));
	jitc.asmALU32(X86_ADD, RDX, RAX);
	jitc.asmMOVxx32_8(X86_MOVZX, RDX, RDX, 0u);
	
	jitc.asmResolveFixup(fixup2, jitc.asmHERE());
#else
	jitc.asmCALL((NativeAddress)ppc_read_effective_byte_asm);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	if (imm) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_ADD, a, imm);
	}
#endif
	return flowContinue;
}
/*
 *	lbzux		Load Byte and Zero with Update Indexed
 *	.523
 */
void ppc_opc_lbzux(PPC_CPU_State &aCPU)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	// FIXME: check rA!=0 && rA!=rD
	uint8 r;
	int ret = ppc_read_effective_byte(aCPU, aCPU.gpr[rA]+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
		aCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lbzux(JITC &jitc)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lux(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.asmCALL((NativeAddress)ppc_read_effective_byte_asm);
	if (rD == rB) {
		// don't ask...
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | RAX);
		jitc.asmALU32(X86_ADD, a, curCPUreg(PPC_GPR(rB)));
		jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	} else {
		jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		jitc.asmALU32(X86_ADD, a, b);
	}
	return flowContinue;
}
/*
 *	lbzx		Load Byte and Zero Indexed
 *	.524
 */
void ppc_opc_lbzx(PPC_CPU_State &aCPU)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint8 r;
	int ret = ppc_read_effective_byte(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lbzx(JITC &jitc)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.asmCALL((NativeAddress)ppc_read_effective_byte_asm);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	return flowContinue;
}
/*
 *	lfd		Load Floating-Point Double
 *	.530
 */
void ppc_opc_lfd(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frD, rA, imm);
	uint64 r;
	int ret = ppc_read_effective_dword(aCPU, (rA?aCPU.gpr[rA]:0)+imm, r);
	if (ret == PPC_MMU_OK) {
		aCPU.fpr[frD] = r;
	}	
}
JITCFlow ppc_opc_gen_lfd(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, frD, rA, imm);
	jitc.floatRegisterClobberAll();
	ppc_opc_gen_helper_l(jitc, PPC_GPR(rA), imm);
#if 0
	jitc.asmALU32(X86_TEST, RAX, 7);
	NativeAddress fixup3 = jitc.asmJxxFixup(X86_NZ);
	ppc_opc_gen_helper_tlb(jitc, RAX, RDX, RCX);
	
	jitc.asmALU32(X86_CMP, RCX, curCPUsib(tlb_data_read_eff, 4, RDX));
	NativeAddress fixup1 = jitc.asmJxxFixup(X86_E);
	jitc.asmResolveFixup(fixup3, jitc.asmHERE());
	jitc.asmCALL((NativeAddress)ppc_read_effective_dword_asm);
	NativeAddress fixup2 = jitc.asmJMPFixup();
	
	jitc.asmResolveFixup(fixup1, jitc.asmHERE());
	jitc.asmALU64(X86_ADD, RAX, curCPUsib(tlb_data_read_phys, 8, RDX));
	jitc.asmALU64(X86_MOV, RDX, RAX, 0u);
	jitc.asmBSWAP64(RDX);

	jitc.asmResolveFixup(fixup2, jitc.asmHERE());
#else
	jitc.asmCALL((NativeAddress)ppc_read_effective_dword_asm);
#endif
	jitc.mapClientRegisterDirty(PPC_FPR(frD), NATIVE_REG | RDX);
	return flowContinue;
}
/*
 *	lfdu		Load Floating-Point Double with Update
 *	.531
 */
void ppc_opc_lfdu(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frD, rA, imm);
	// FIXME: check rA!=0
	uint64 r;
	int ret = ppc_read_effective_dword(aCPU, aCPU.gpr[rA]+imm, r);
	if (ret == PPC_MMU_OK) {
		aCPU.fpr[frD] = r;
		aCPU.gpr[rA] += imm;
	}	
}
JITCFlow ppc_opc_gen_lfdu(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, frD, rA, imm);
	jitc.floatRegisterClobberAll();
	ppc_opc_gen_helper_lu(jitc, PPC_GPR(rA), imm);
	jitc.asmCALL((NativeAddress)ppc_read_effective_dword_asm);
	jitc.mapClientRegisterDirty(PPC_FPR(frD), NATIVE_REG | RDX);
	if (imm) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_ADD, a, imm);
	}
	return flowContinue;
}
/*
 *	lfdux		Load Floating-Point Double with Update Indexed
 *	.532
 */
void ppc_opc_lfdux(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
	// FIXME: check rA!=0
	uint64 r;
	int ret = ppc_read_effective_dword(aCPU, aCPU.gpr[rA]+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
		aCPU.fpr[frD] = r;
	}	
}
JITCFlow ppc_opc_gen_lfdux(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frD, rA, rB);
	ppc_opc_gen_helper_lux(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.floatRegisterClobberAll();
	jitc.asmCALL((NativeAddress)ppc_read_effective_dword_asm);
	jitc.mapClientRegisterDirty(PPC_FPR(frD), NATIVE_REG | RDX);
	NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	jitc.asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	lfdx		Load Floating-Point Double Indexed
 *	.533
 */
void ppc_opc_lfdx(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
	uint64 r;
	int ret = ppc_read_effective_dword(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		aCPU.fpr[frD] = r;
	}	
}
JITCFlow ppc_opc_gen_lfdx(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frD, rA, rB);
	jitc.floatRegisterClobberAll();
	ppc_opc_gen_helper_lx(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.asmCALL((NativeAddress)ppc_read_effective_dword_asm);
	jitc.mapClientRegisterDirty(PPC_FPR(frD), NATIVE_REG | RDX);
	return flowContinue;
}
/*
 *	lfs		Load Floating-Point Single
 *	.534
 */
void ppc_opc_lfs(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frD, rA, imm);
	uint32 r;
	int ret = ppc_read_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+imm, r);
	if (ret == PPC_MMU_OK) {
		ppc_single s;
		ppc_double d;
		ppc_fpu_unpack_single(s, r);
		ppc_fpu_single_to_double(s, d);
		ppc_fpu_pack_double(aCPU.fpscr, d, aCPU.fpr[frD]);
	}	
}
JITCFlow ppc_opc_gen_lfs(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, frD, rA, imm);
	jitc.floatRegisterClobberAll();
	ppc_opc_gen_helper_l(jitc, PPC_GPR(rA), imm);
	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
	jitc.asmALU32(X86_MOV, RDI, curCPU(fpscr));	
	jitc.asmALU32(X86_MOV, RSI, RDX);
	jitc.asmCALL((NativeAddress)ppc_opc_single_to_double);
	jitc.mapClientRegisterDirty(PPC_FPR(frD), NATIVE_REG | RAX);
	return flowContinue;
}
/*
 *	lfsu		Load Floating-Point Single with Update
 *	.535
 */
void ppc_opc_lfsu(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frD, rA, imm);
	// FIXME: check rA!=0
	uint32 r;
	int ret = ppc_read_effective_word(aCPU, aCPU.gpr[rA]+imm, r);
	if (ret == PPC_MMU_OK) {
		ppc_single s;
		ppc_double d;
		ppc_fpu_unpack_single(s, r);
		ppc_fpu_single_to_double(s, d);
		ppc_fpu_pack_double(aCPU.fpscr, d, aCPU.fpr[frD]);
		aCPU.gpr[rA] += imm;
	}	
}
JITCFlow ppc_opc_gen_lfsu(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, frD, rA, imm);
	jitc.floatRegisterClobberAll();
	ppc_opc_gen_helper_lu(jitc, PPC_GPR(rA), imm);
	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
	jitc.asmALU32(X86_MOV, RSI, RDX);
	jitc.asmALU32(X86_MOV, RDI, curCPU(fpscr));	
	jitc.asmCALL((NativeAddress)ppc_opc_single_to_double);
	jitc.mapClientRegisterDirty(PPC_FPR(frD), NATIVE_REG | RAX);
	if (imm) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_ADD, a, imm);
	}
	return flowContinue;
}
/*
 *	lfsux		Load Floating-Point Single with Update Indexed
 *	.536
 */
void ppc_opc_lfsux(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
	// FIXME: check rA!=0
	uint32 r;
	int ret = ppc_read_effective_word(aCPU, aCPU.gpr[rA]+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) { 
		aCPU.gpr[rA] += aCPU.gpr[rB];
		ppc_single s;
		ppc_double d;
		ppc_fpu_unpack_single(s, r);
		ppc_fpu_single_to_double(s, d);
		ppc_fpu_pack_double(aCPU.fpscr, d, aCPU.fpr[frD]);
	}	
}
JITCFlow ppc_opc_gen_lfsux(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frD, rA, rB);
	jitc.floatRegisterClobberAll();
	ppc_opc_gen_helper_lux(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
	jitc.asmALU32(X86_MOV, RSI, RDX);
	jitc.asmALU32(X86_MOV, RDI, curCPU(fpscr));	
	jitc.asmCALL((NativeAddress)ppc_opc_single_to_double);
	jitc.mapClientRegisterDirty(PPC_FPR(frD), NATIVE_REG | RAX);
	NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	jitc.asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	lfsx		Load Floating-Point Single Indexed
 *	.537
 */
void ppc_opc_lfsx(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
////		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
	uint32 r;
	int ret = ppc_read_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		ppc_single s;
		ppc_double d;
		ppc_fpu_unpack_single(s, r);
		ppc_fpu_single_to_double(s, d);
		ppc_fpu_pack_double(aCPU.fpscr, d, aCPU.fpr[frD]);
	}	
}
JITCFlow ppc_opc_gen_lfsx(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frD, rA, rB);
	jitc.floatRegisterClobberAll();
	ppc_opc_gen_helper_lx(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
	jitc.asmALU32(X86_MOV, RSI, RDX);
	jitc.asmALU32(X86_MOV, RDI, curCPU(fpscr));	
	jitc.asmCALL((NativeAddress)ppc_opc_single_to_double);
	jitc.mapClientRegisterDirty(PPC_FPR(frD), NATIVE_REG | RAX);
	return flowContinue;
}
/*
 *	lha		Load Half Word Algebraic
 *	.538
 */
void ppc_opc_lha(PPC_CPU_State &aCPU)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint16 r;
	int ret = ppc_read_effective_half(aCPU, (rA?aCPU.gpr[rA]:0)+imm, r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rD] = (r&0x8000)?(r|0xffff0000):r;
	}
}
JITCFlow ppc_opc_gen_lha(JITC &jitc)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_l(jitc, PPC_GPR(rA), imm);
	jitc.asmCALL((NativeAddress)ppc_read_effective_half_s_asm);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	return flowContinue;
}
/*
 *	lhau		Load Half Word Algebraic with Update
 *	.539
 */
void ppc_opc_lhau(PPC_CPU_State &aCPU)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint16 r;
	// FIXME: rA != 0
	int ret = ppc_read_effective_half(aCPU, aCPU.gpr[rA]+imm, r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
		aCPU.gpr[rD] = (r&0x8000)?(r|0xffff0000):r;
	}
}
JITCFlow ppc_opc_gen_lhau(JITC &jitc)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_lu(jitc, PPC_GPR(rA), imm);
	jitc.asmCALL((NativeAddress)ppc_read_effective_half_s_asm);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	if (imm) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_ADD, a, imm);
	}
	return flowContinue;
}
/*
 *	lhaux		Load Half Word Algebraic with Update Indexed
 *	.540
 */
void ppc_opc_lhaux(PPC_CPU_State &aCPU)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint16 r;
	// FIXME: rA != 0
	int ret = ppc_read_effective_half(aCPU, aCPU.gpr[rA]+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
		aCPU.gpr[rD] = (r&0x8000)?(r|0xffff0000):r;
	}
}
JITCFlow ppc_opc_gen_lhaux(JITC &jitc)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lux(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.asmCALL((NativeAddress)ppc_read_effective_half_s_asm);
	if (rD == rB) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | RAX);
		jitc.asmALU32(X86_ADD, a, curCPUreg(PPC_GPR(rB)));
		jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	} else {
		jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		jitc.asmALU32(X86_ADD, a, b);
	}
	return flowContinue;
}
/*
 *	lhax		Load Half Word Algebraic Indexed
 *	.541
 */
void ppc_opc_lhax(PPC_CPU_State &aCPU)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint16 r;
	// FIXME: rA != 0
	int ret = ppc_read_effective_half(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rD] = (r&0x8000) ? (r|0xffff0000):r;
	}
}
JITCFlow ppc_opc_gen_lhax(JITC &jitc)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.asmCALL((NativeAddress)ppc_read_effective_half_s_asm);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	return flowContinue;
}
/*
 *	lhbrx		Load Half Word Byte-Reverse Indexed
 *	.542
 */
void ppc_opc_lhbrx(PPC_CPU_State &aCPU)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint16 r;
	int ret = ppc_read_effective_half(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rD] = ppc_bswap_half(r);
	}
}
JITCFlow ppc_opc_gen_lhbrx(JITC &jitc)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.asmCALL((NativeAddress)ppc_read_effective_half_z_asm);
	jitc.asmShift16(X86_ROL, RDX, 8);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	return flowContinue;
}
/*
 *	lhz		Load Half Word and Zero
 *	.543
 */
void ppc_opc_lhz(PPC_CPU_State &aCPU)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint16 r;
	int ret = ppc_read_effective_half(aCPU, (rA?aCPU.gpr[rA]:0)+imm, r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lhz(JITC &jitc)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_l(jitc, PPC_GPR(rA), imm);
	jitc.asmCALL((NativeAddress)ppc_read_effective_half_z_asm);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	return flowContinue;
}
/*
 *	lhzu		Load Half Word and Zero with Update
 *	.544
 */
void ppc_opc_lhzu(PPC_CPU_State &aCPU)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint16 r;
	// FIXME: rA!=0
	int ret = ppc_read_effective_half(aCPU, aCPU.gpr[rA]+imm, r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rD] = r;
		aCPU.gpr[rA] += imm;
	}
}
JITCFlow ppc_opc_gen_lhzu(JITC &jitc)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_lu(jitc, PPC_GPR(rA), imm);
	jitc.asmCALL((NativeAddress)ppc_read_effective_half_z_asm);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	if (imm) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_ADD, a, imm);
	}
	return flowContinue;
}
/*
 *	lhzux		Load Half Word and Zero with Update Indexed
 *	.545
 */
void ppc_opc_lhzux(PPC_CPU_State &aCPU)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint16 r;
	// FIXME: rA != 0
	int ret = ppc_read_effective_half(aCPU, aCPU.gpr[rA]+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
		aCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lhzux(JITC &jitc)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lux(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.asmCALL((NativeAddress)ppc_read_effective_half_z_asm);
	if (rD == rB) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | RAX);
		jitc.asmALU32(X86_ADD, a, curCPUreg(PPC_GPR(rB)));
		jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	} else {
		jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		jitc.asmALU32(X86_ADD, a, b);
	}
	return flowContinue;
}
/*
 *	lhzx		Load Half Word and Zero Indexed
 *	.546
 */
void ppc_opc_lhzx(PPC_CPU_State &aCPU)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint16 r;
	int ret = ppc_read_effective_half(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lhzx(JITC &jitc)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.asmCALL((NativeAddress)ppc_read_effective_half_z_asm);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	return flowContinue;
}
/*
 *	lmw		Load Multiple Word
 *	.547
 */
void ppc_opc_lmw(PPC_CPU_State &aCPU)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint32 ea = (rA ? aCPU.gpr[rA] : 0) + imm;
	while (rD <= 31) {
		if (ppc_read_effective_word(aCPU, ea, aCPU.gpr[rD])) {
			return;
		}
		rD++;
		ea += 4;
	}
}
JITCFlow ppc_opc_gen_lmw(JITC &jitc)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	
	while (rD <= 31) {
		ppc_opc_gen_helper_l(jitc, PPC_GPR(rA), imm);
		jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
		jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
		rD += 1;
		imm += 4;
	}
	return flowContinue;
}
/*
 *	lswi		Load String Word Immediate
 *	.548
 */
void ppc_opc_lswi(PPC_CPU_State &aCPU)
{
	int rA, rD, NB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, NB);
	if (NB==0) NB=32;
	uint32 ea = rA ? aCPU.gpr[rA] : 0;
	uint32 r = 0;
	int i = 4;
	uint8 v;
	while (NB > 0) {
		if (!i) {
			i = 4;
			aCPU.gpr[rD] = r;
			rD++;
			rD%=32;
			r = 0;
		}
		if (ppc_read_effective_byte(aCPU, ea, v)) {
			return;
		}
		r<<=8;
		r|=v;
		ea++;
		i--;
		NB--;
	}
	while (i) { r<<=8; i--; }
	aCPU.gpr[rD] = r;
}
JITCFlow ppc_opc_gen_lswi(JITC &jitc)
{
	int rA, rD, NB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, NB);
	if (NB==0) NB=32;
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	if (rA) {
		jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
	} else {
		jitc.asmALU32(X86_MOV, RAX, 0);
	}
	jitc.asmALU32(X86_MOV, RCX, NB);
	jitc.asmALU32(X86_MOV, RBX, rD);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.clobberAll();
	jitc.asmCALL((NativeAddress)ppc_opc_lswi_asm);
	return flowEndBlock;
}
/*
 *	lswx		Load String Word Indexed
 *	.550
 */
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
			rD%=32;
			r = 0;
		}
		if (ppc_read_effective_byte(aCPU, ea, v)) {
			return;
		}
		r<<=8;
		r|=v;
		ea++;
		i--;
		NB--;
	}
	while (i) { r<<=8; i--; }
	aCPU.gpr[rD] = r;
}
JITCFlow ppc_opc_gen_lswx(JITC &jitc)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	jitc.getClientRegister(PPC_XER, NATIVE_REG | RCX);
	if (rA) {
		jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
		jitc.asmALU32(X86_ADD, RAX, curCPUreg(PPC_GPR(rB)));
	} else {
		jitc.getClientRegister(PPC_GPR(rB), NATIVE_REG | RAX);
	}
	jitc.asmALU32(X86_AND, RCX, 0x7f);
	jitc.clobberAll();
	NativeAddress fixup = jitc.asmJxxFixup(X86_Z);
	jitc.asmALU32(X86_MOV, RBX, rD);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_opc_lswi_asm);
	jitc.asmResolveFixup(fixup, jitc.asmHERE());
	return flowEndBlock;
}
/*
 *	lwarx		Load Word and Reserve Indexed
 *	.553
 */
void ppc_opc_lwarx(PPC_CPU_State &aCPU)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint32 r;
	int ret = ppc_read_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rD] = r;
		aCPU.reserve = r;
		aCPU.have_reservation = 1;
	}
}
JITCFlow ppc_opc_gen_lwarx(JITC &jitc)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	jitc.asmALU32(X86_MOV, curCPU(reserve), RDX);
	jitc.asmALU8(X86_MOV, curCPU(have_reservation), 1);
	return flowContinue;
}
/*
 *	lwbrx		Load Word Byte-Reverse Indexed
 *	.556
 */
void ppc_opc_lwbrx(PPC_CPU_State &aCPU)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint32 r;
	int ret = ppc_read_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rD] = ppc_bswap_word(r);
	}
}
JITCFlow ppc_opc_gen_lwbrx(JITC &jitc)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	jitc.asmBSWAP32(RDX);
	return flowContinue;
}
/*
 *	lwz		Load Word and Zero
 *	.557
 */
void ppc_opc_lwz(PPC_CPU_State &aCPU)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint32 r;
	int ret = ppc_read_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+imm, r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rD] = r;
	}	
}
JITCFlow ppc_opc_gen_lwz(JITC &jitc)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_l(jitc, PPC_GPR(rA), imm);
#if 0
	jitc.asmALU32(X86_TEST, RAX, 3);
	NativeAddress fixup3 = jitc.asmJxxFixup(X86_NZ);
	ppc_opc_gen_helper_tlb(jitc, RAX, RDX, RCX);
	
	jitc.asmALU32(X86_CMP, RCX, curCPUsib(tlb_data_read_eff, 4, RDX));
	NativeAddress fixup1 = jitc.asmJxxFixup(X86_E);
	jitc.asmResolveFixup(fixup3, jitc.asmHERE());
	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
	NativeAddress fixup2 = jitc.asmJMPFixup();
	
	jitc.asmResolveFixup(fixup1, jitc.asmHERE());
	jitc.asmALU64(X86_ADD, RAX, curCPUsib(tlb_data_read_phys, 8, RDX));
	jitc.asmALU32(X86_MOV, RDX, RAX, 0u);
	jitc.asmBSWAP32(RDX);

	jitc.asmResolveFixup(fixup2, jitc.asmHERE());
#else
	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
#endif
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	return flowContinue;
}
/*
 *	lbzu		Load Word and Zero with Update
 *	.558
 */
void ppc_opc_lwzu(PPC_CPU_State &aCPU)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	// FIXME: check rA!=0 && rA!=rD
	uint32 r;
	int ret = ppc_read_effective_word(aCPU, aCPU.gpr[rA]+imm, r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
		aCPU.gpr[rD] = r;
	}	
}
JITCFlow ppc_opc_gen_lwzu(JITC &jitc)
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_lu(jitc, PPC_GPR(rA), imm);
#if 0
	jitc.asmALU32(X86_TEST, RAX, 3);
	NativeAddress fixup3 = jitc.asmJxxFixup(X86_NZ);
	ppc_opc_gen_helper_tlb(jitc, RAX, RDX, RCX);
	
	jitc.asmALU32(X86_CMP, RCX, curCPUsib(tlb_data_read_eff, 4, RDX));
	NativeAddress fixup1 = jitc.asmJxxFixup(X86_E);
	jitc.asmResolveFixup(fixup3, jitc.asmHERE());
	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
	NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | RAX);
	if (imm) {
		jitc.asmALU32(X86_ADD, a, imm);
	}
	NativeAddress fixup2 = jitc.asmJMPFixup();
	
	jitc.asmResolveFixup(fixup1, jitc.asmHERE());
	jitc.asmALU64(X86_MOV, RDX, curCPUsib(tlb_data_read_phys, 8, RDX));
	jitc.asmALU32(X86_ADD, RDX, RAX);
	jitc.asmALU32(X86_MOV, RDX, RDX, 0u);
	jitc.asmBSWAP32(RDX);
	
	jitc.asmResolveFixup(fixup2, jitc.asmHERE());
#else
	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	if (imm) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_ADD, a, imm);
	}
#endif
	return flowContinue;
}
/*
 *	lwzux		Load Word and Zero with Update Indexed
 *	.559
 */
void ppc_opc_lwzux(PPC_CPU_State &aCPU)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	// FIXME: check rA!=0 && rA!=rD
	uint32 r;
	int ret = ppc_read_effective_word(aCPU, aCPU.gpr[rA]+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
		aCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lwzux(JITC &jitc)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lux(jitc, PPC_GPR(rA), PPC_GPR(rB));
	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
	if (rD == rB) {
		// don't ask...
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | RAX);
		jitc.asmALU32(X86_ADD, a, curCPUreg(PPC_GPR(rB)));
		jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	} else {
		jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		jitc.asmALU32(X86_ADD, a, b);
	}
	return flowContinue;
}
/*
 *	lwzx		Load Word and Zero Indexed
 *	.560
 */
void ppc_opc_lwzx(PPC_CPU_State &aCPU)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	uint32 r;
	int ret = ppc_read_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lwzx(JITC &jitc)
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(jitc, PPC_GPR(rA), PPC_GPR(rB));
#if 0
	jitc.asmALU32(X86_TEST, RAX, 3);
	NativeAddress fixup3 = jitc.asmJxxFixup(X86_NZ);	
	ppc_opc_gen_helper_tlb(jitc, RAX, RDX, RCX);
	
	jitc.asmALU32(X86_CMP, RCX, curCPUsib(tlb_data_read_eff, 4, RDX));
	NativeAddress fixup1 = jitc.asmJxxFixup(X86_E);
	jitc.asmResolveFixup(fixup3, jitc.asmHERE());
	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
	NativeAddress fixup2 = jitc.asmJMPFixup();
	
	jitc.asmResolveFixup(fixup1, jitc.asmHERE());
	jitc.asmALU64(X86_ADD, RAX, curCPUsib(tlb_data_read_phys, 8, RDX));
	jitc.asmALU32(X86_MOV, RDX, RAX, 0u);
	jitc.asmBSWAP32(RDX);

	jitc.asmResolveFixup(fixup2, jitc.asmHERE());
#else
	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
#endif
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	return flowContinue;
}

#if 0
static inline NativeReg FASTCALL ppc_opc_gen_helper_lvx_hint(int rA, int rB, int hint)
{
	NativeReg ret = REG_NO;
	byte modrm[6];

	NativeReg reg1 = jitc.getClientRegisterMapping(PPC_GPR(rA));
	NativeReg reg2 = jitc.getClientRegisterMapping(PPC_GPR(rB));

	if (reg1 == hint) {
		jitc.clobberCarryAndFlags();
		jitc.clobberRegister(NATIVE_REG | reg1);
		ret = reg1;
		jitc.touchRegister(ret);

		if (reg2 != REG_NO) {
			jitc.asmALU32(X86_ADD, ret, reg2);
		} else {
			asmALURegMem(X86_ADD, ret, modrm,
				x86_mem(modrm, REG_NO, (uint32)&aCPU.gpr[rB]));
		}
	} else if (reg2 == hint) {
		jitc.clobberCarryAndFlags();
		jitc.clobberRegister(NATIVE_REG | reg2);
		ret = reg2;
		jitc.touchRegister(ret);

		if (reg1 != REG_NO) {
			jitc.asmALU32(X86_ADD, ret, reg1);
		} else {
			asmALURegMem(X86_ADD, ret, modrm,
				x86_mem(modrm, REG_NO, (uint32)&aCPU.gpr[rA]));
		}
	} else if ((reg1 != REG_NO) && (reg2 != REG_NO)) {
		/* If both are in register space, and not the hint we're best
		 *   off clobbering the hint, then using leal as a 3-operand
		 *   ADD.
		 * This gives us the performance of an ADD, and removes the
		 *   need for a later MOV into the hint.
		 */
		jitc.clobberRegister(NATIVE_REG | hint);
		ret = (NativeReg)hint;
		jitc.touchRegister(ret);

		asmLEA(ret, modrm, x86_mem_sib(modrm, reg1, 1, reg2, 0));
	}

	return ret;
}

static inline NativeReg FASTCALL ppc_opc_gen_helper_lvx(int rA, int rB, int hint=0)
{
	NativeReg ret = REG_NO;
	byte modrm[6];

	if (!rA) {
		ret = jitc.getClientRegisterMapping(PPC_GPR(rB));

		if (ret == REG_NO) {
			ret = jitc.getClientRegister(PPC_GPR(rB), hint);
		}

		jitc.clobberRegister(NATIVE_REG | ret);
		jitc.touchRegister(ret);

		return ret;
	}

	if (hint & NATIVE_REG) {
		ret = ppc_opc_gen_helper_lvx_hint(rA, rB, hint & 0x0f);

		if (ret != REG_NO)
			return ret;
	}

	jitc.clobberCarryAndFlags();

	NativeReg reg1 = jitc.getClientRegisterMapping(PPC_GPR(rA));
	NativeReg reg2 = jitc.getClientRegisterMapping(PPC_GPR(rB));

	if (reg2 == REG_NO) {
		ret = jitc.getClientRegister(PPC_GPR(rA));
		jitc.clobberRegister(NATIVE_REG | ret);

		asmALURegMem(X86_ADD, ret, modrm,
			x86_mem(modrm, REG_NO, (uint32)&aCPU.gpr[rB]));
	} else {
		jitc.clobberRegister(NATIVE_REG | reg2);
		ret = reg2;

		if (reg1 != REG_NO) {
			jitc.asmALU32(X86_ADD, ret, reg1);
		} else {
			asmALURegMem(X86_ADD, ret, modrm,
				x86_mem(modrm, REG_NO, (uint32)&aCPU.gpr[rA]));
		}
	}

	jitc.touchRegister(ret);
	return ret;
}

/*      lvx	     Load Vector Indexed
 *      v.127
 */
void ppc_opc_lvx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
	if ((aCPU.msr & MSR_VEC) == 0) {
//		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, rA, rB);
	Vector_t r;

	int ea = ((rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB]);

	int ret = ppc_read_effective_qword(ea, r);
	if (ret == PPC_MMU_OK) {
		aCPU.vr[vrD] = r;
	}
}
JITCFlow ppc_opc_gen_lvx()
{
	ppc_opc_gen_check_vec();
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, vrD, rA, rB);
	jitcDropClientVectorRegister(vrD);
	jitcAssertFlushedVectorRegister(vrD);

	jitc.clobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB, NATIVE_REG | RAX);
#if 1
	jitc.clobberAll();
	if (regA != RAX) {
		//printf("*** hint miss r%u != r0\n", regA);
		jitc.asmALU32(X86_MOV, RAX, regA);
	}
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmALU32(X86_MOV, EDX, (uint32)&(aCPU.vr[vrD]));

	if (0 && jitc.hostCPUCaps.sse) {
		jitc.asmCALL((NativeAddress)ppc_read_effective_qword_sse_asm);
		jitc.nativeVectorReg = vrD;
	} else {
		jitc.asmCALL((NativeAddress)ppc_read_effective_qword_asm);
	}
#else
	jitc.asmALU32(X86_AND, regA, ~0x0f);
	asmMOVDMemReg((uint32)&aCPU.vtemp, regA);

	jitc.clobberAll();
	if (regA != RAX) {
		//printf("*** hint miss r%u != r0\n", regA);
		jitc.asmALU32(X86_MOV, RAX, regA);
	}
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);

	jitc.asmCALL((NativeAddress)ppc_read_effective_dword_asm);
	asmMOVDMemReg((uint32)&(aCPU.vr[vrD].w[3]), RCX);
	asmMOVDMemReg((uint32)&(aCPU.vr[vrD].w[2]), EDX);

	asmMOVRegDMem(RAX, (uint32)&aCPU.vtemp);
	jitc.asmALU32(X86_OR, RAX, 8);

	jitc.asmCALL((NativeAddress)ppc_read_effective_dword_asm);
	asmMOVDMemReg((uint32)&(aCPU.vr[vrD].w[1]), RCX);
	asmMOVDMemReg((uint32)&(aCPU.vr[vrD].w[0]), EDX);
#endif

	return flowContinue;
}


/*      lvxl	    Load Vector Index LRU
 *      v.128
 */
void ppc_opc_lvxl(PPC_CPU_State &aCPU)
{
	ppc_opc_lvx(aCPU);
	/* This instruction should hint to the cache that the value won't be
	 *   needed again in memory anytime soon.  We don't emulate the cache,
	 *   so this is effectively exactly the same as lvx.
	 */
}
JITCFlow ppc_opc_gen_lvxl()
{
	return ppc_opc_gen_lvx();
}


/*      lvRBX	   Load Vector Element Byte Indexed
 *      v.119
 */
void ppc_opc_lvebx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
	if ((aCPU.msr & MSR_VEC) == 0) {
//		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, rA, rB);
	uint32 ea;
	uint8 r;
	ea = (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB];
	int ret = ppc_read_effective_byte(ea, r);
	if (ret == PPC_MMU_OK) {
		VECT_B(aCPU.vr[vrD], ea & 0x0f) = r;
	}
}
JITCFlow ppc_opc_gen_lvebx()
{
	ppc_opc_gen_check_vec();
	int rA, vrD, rB;
	byte modrm[6];
	PPC_OPC_TEMPL_X(jitc.current_opc, vrD, rA, rB);
	jitcDropClientVectorRegister(vrD);
	jitcAssertFlushedVectorRegister(vrD);

	if (vrD == jitc.nativeVectorReg) {
		jitc.nativeVectorReg = VECTREG_NO;
	}

	jitc.clobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB, NATIVE_REG | RAX);
	asmMOVDMemReg((uint32)&aCPU.vtemp, regA);

	jitc.clobberAll();
	if (regA != RAX)	jitc.asmALU32(X86_MOV, RAX, regA);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);

	jitc.asmCALL((NativeAddress)ppc_read_effective_byte_asm);
	asmMOVRegDMem(RAX, (uint32)&aCPU.vtemp);
	jitc.asmALU32(X86_AND, RAX, 0x0f);
	asmALUReg(X86_NOT, RAX);

	asmALUMemReg8(X86_MOV, modrm,
		x86_mem(modrm, RAX, ((uint32)&aCPU.vr[vrD])+16), DL);

	return flowContinue;
}

/*      lvehx	   Load Vector Element Half Word Indexed
 *      v.121
 */
void ppc_opc_lvehx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
	if ((aCPU.msr & MSR_VEC) == 0) {
//		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, rA, rB);
	uint32 ea;
	uint16 r;
	ea = ((rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB]) & ~1;
	int ret = ppc_read_effective_half(ea, r);
	if (ret == PPC_MMU_OK) {
		VECT_H(aCPU.vr[vrD], (ea & 0x0f) >> 1) = r;
	}
}
JITCFlow ppc_opc_gen_lvehx()
{
	ppc_opc_gen_check_vec();
	int rA, vrD, rB;
	byte modrm[6];
	PPC_OPC_TEMPL_X(jitc.current_opc, vrD, rA, rB);
	jitcDropClientVectorRegister(vrD);
	jitcAssertFlushedVectorRegister(vrD);

	if (vrD == jitc.nativeVectorReg) {
		jitc.nativeVectorReg = VECTREG_NO;
	}

	jitc.clobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB, NATIVE_REG | RAX);
	asmMOVDMemReg((uint32)&aCPU.vtemp, regA);
	jitc.asmALU32(X86_AND, regA, ~0x01);

	jitc.clobberAll();
	if (regA != RAX)	jitc.asmALU32(X86_MOV, RAX, regA);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);

	jitc.asmCALL((NativeAddress)ppc_read_effective_half_z_asm);
	asmMOVRegDMem(RAX, (uint32)&aCPU.vtemp);
	jitc.asmALU32(X86_AND, RAX, 0x0e);
	asmALUReg(X86_NOT, RAX);

	asmALUMemReg8(X86_MOV, modrm,
		x86_mem(modrm, RAX, ((uint32)&aCPU.vr[vrD])+15), DL);
	asmALUMemReg8(X86_MOV, modrm,
		x86_mem(modrm, RAX, ((uint32)&aCPU.vr[vrD])+16), DH);

	return flowContinue;
}

/*      lvewx	   Load Vector Element Word Indexed
 *      v.122
 */
void ppc_opc_lvewx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
	if ((aCPU.msr & MSR_VEC) == 0) {
//		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, rA, rB);
	uint32 ea;
	uint32 r;
	ea = ((rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB]) & ~3;
	int ret = ppc_read_effective_word(ea, r);
	if (ret == PPC_MMU_OK) {
		VECT_W(aCPU.vr[vrD], (ea & 0xf) >> 2) = r;
	}
}
JITCFlow ppc_opc_gen_lvewx()
{
	ppc_opc_gen_check_vec();
	int rA, vrD, rB;
	byte modrm[6];
	PPC_OPC_TEMPL_X(jitc.current_opc, vrD, rA, rB);
	jitcDropClientVectorRegister(vrD);
	jitcAssertFlushedVectorRegister(vrD);

	if (vrD == jitc.nativeVectorReg) {
		jitc.nativeVectorReg = VECTREG_NO;
	}

	jitc.clobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB, NATIVE_REG | RAX);
	asmMOVDMemReg((uint32)&aCPU.vtemp, regA);
	jitc.asmALU32(X86_AND, regA, ~0x03);

	jitc.clobberAll();
	if (regA != RAX)	jitc.asmALU32(X86_MOV, RAX, regA);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);

	jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
	asmMOVRegDMem(RAX, (uint32)&aCPU.vtemp);
	jitc.asmALU32(X86_AND, RAX, 0x0c);
	asmALUReg(X86_NOT, RAX);

	asmALUMemReg(X86_MOV, modrm,
		x86_mem(modrm, RAX, ((uint32)&aCPU.vr[vrD])+13), EDX);
	return flowContinue;
}

const static byte lvsl_helper[] = {
#if HOST_ENDIANESS == HOST_ENDIANESS_LE
	0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18,
	0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10,
	0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
	0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
#else
#error Endianess not supported!
#endif
};

const static uint32 lvsl_helper_full[16*4] = {
	VECT_BUILD(0x00010203, 0x04050607, 0x08090A0B, 0x0C0D0E0F),
	VECT_BUILD(0x01020304, 0x05060708, 0x090A0B0C, 0x0D0E0F10),
	VECT_BUILD(0x02030405, 0x06070809, 0x0A0B0C0D, 0x0E0F1011),
	VECT_BUILD(0x03040506, 0x0708090A, 0x0B0C0D0E, 0x0F101112),
	VECT_BUILD(0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213),
	VECT_BUILD(0x05060708, 0x090A0B0C, 0x0D0E0F10, 0x11121314),
	VECT_BUILD(0x06070809, 0x0A0B0C0D, 0x0E0F1011, 0x12131415),
	VECT_BUILD(0x0708090A, 0x0B0C0D0E, 0x0F101112, 0x13141516),
	VECT_BUILD(0x08090A0B, 0x0C0D0E0F, 0x10111213, 0x14151617),
	VECT_BUILD(0x090A0B0C, 0x0D0E0F10, 0x11121314, 0x15161718),
	VECT_BUILD(0x0A0B0C0D, 0x0E0F1011, 0x12131415, 0x16171819),
	VECT_BUILD(0x0B0C0D0E, 0x0F101112, 0x13141516, 0x1718191A),
	VECT_BUILD(0x0C0D0E0F, 0x10111213, 0x14151617, 0x18191A1B),
	VECT_BUILD(0x0D0E0F10, 0x11121314, 0x15161718, 0x191A1B1C),
	VECT_BUILD(0x0E0F1011, 0x12131415, 0x16171819, 0x1A1B1C1D),
	VECT_BUILD(0x0F101112, 0x13141516, 0x1718191A, 0x1B1C1D1E),
};

const static uint32 lvsr_helper_full[16*4] = {
	VECT_BUILD(0x10111213, 0x14151617, 0x18191A1B, 0x1C1D1E1F),
	VECT_BUILD(0x0F101112, 0x13141516, 0x1718191A, 0x1B1C1D1E),
	VECT_BUILD(0x0E0F1011, 0x12131415, 0x16171819, 0x1A1B1C1D),
	VECT_BUILD(0x0D0E0F10, 0x11121314, 0x15161718, 0x191A1B1C),
	VECT_BUILD(0x0C0D0E0F, 0x10111213, 0x14151617, 0x18191A1B),
	VECT_BUILD(0x0B0C0D0E, 0x0F101112, 0x13141516, 0x1718191A),
	VECT_BUILD(0x0A0B0C0D, 0x0E0F1011, 0x12131415, 0x16171819),
	VECT_BUILD(0x090A0B0C, 0x0D0E0F10, 0x11121314, 0x15161718),
	VECT_BUILD(0x08090A0B, 0x0C0D0E0F, 0x10111213, 0x14151617),
	VECT_BUILD(0x0708090A, 0x0B0C0D0E, 0x0F101112, 0x13141516),
	VECT_BUILD(0x06070809, 0x0A0B0C0D, 0x0E0F1011, 0x12131415),
	VECT_BUILD(0x05060708, 0x090A0B0C, 0x0D0E0F10, 0x11121314),
	VECT_BUILD(0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213),
	VECT_BUILD(0x03040506, 0x0708090A, 0x0B0C0D0E, 0x0F101112),
	VECT_BUILD(0x02030405, 0x06070809, 0x0A0B0C0D, 0x0E0F1011),
	VECT_BUILD(0x01020304, 0x05060708, 0x090A0B0C, 0x0D0E0F10),
};

/*
 *      lvsl	    Load Vector for Shift Left
 *      v.123
 */
void ppc_opc_lvsl(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
	if ((aCPU.msr & MSR_VEC) == 0) {
//		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, rA, rB);
	uint32 ea;
	ea = ((rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB]);
#if HOST_ENDIANESS == HOST_ENDIANESS_LE
	memmove(&aCPU.vr[vrD], lvsl_helper+0x10-(ea & 0xf), 16);
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
	memmove(&aCPU.vr[vrD], lvsl_helper+(ea & 0xf), 16);
#else
#error Endianess not supported!
#endif
}
JITCFlow ppc_opc_gen_lvsl()
{
	ppc_opc_gen_check_vec();
	int rA, vrD, rB;
	byte modrm[6];
	PPC_OPC_TEMPL_X(jitc.current_opc, vrD, rA, rB);

	if (vrD == jitc.nativeVectorReg) {
		jitc.nativeVectorReg = VECTREG_NO;
	}

	jitc.clobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB);
	jitc.asmALU32(X86_AND, regA, 0x0f);

	if (jitc.hostCPUCaps.sse) {
		asmShiftRegImm(X86_SHL, regA, 4);

		NativeVectorReg reg1 = jitc.mapClientVectorRegisterDirty(vrD);

		asmALUPS(X86_MOVAPS, reg1,
			x86_mem2(modrm, regA, (uint32)&lvsl_helper_full));
	} else {
		asmALUReg(X86_NOT, regA);
		jitcDropClientVectorRegister(vrD);

		NativeReg reg1 = jitcAllocRegister();
		NativeReg reg2 = jitcAllocRegister();
		NativeReg reg3 = jitcAllocRegister();

		asmALURegMem(X86_MOV, reg1, modrm,
			x86_mem(modrm, regA, (uint32)&lvsl_helper+0x11));
		asmALURegMem(X86_MOV, reg2, modrm,
			x86_mem(modrm, regA, (uint32)&lvsl_helper+0x15));
		asmALURegMem(X86_MOV, reg3, modrm,
			x86_mem(modrm, regA, (uint32)&lvsl_helper+0x19));
		asmALURegMem(X86_MOV, regA, modrm,
			x86_mem(modrm, regA, (uint32)&lvsl_helper+0x1d));

		asmMOVDMemReg((uint32)&(aCPU.vr[vrD].w[0]), reg1);
		asmMOVDMemReg((uint32)&(aCPU.vr[vrD].w[1]), reg2);
		asmMOVDMemReg((uint32)&(aCPU.vr[vrD].w[2]), reg3);
		asmMOVDMemReg((uint32)&(aCPU.vr[vrD].w[3]), regA);
	}

	return flowContinue;
}

/*
 *      lvsr	    Load Vector for Shift Right
 *      v.125
 */
void ppc_opc_lvsr(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
	if ((aCPU.msr & MSR_VEC) == 0) {
//		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrD, rA, rB);
	uint32 ea;
	ea = ((rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB]);
#if HOST_ENDIANESS == HOST_ENDIANESS_LE
	memmove(&aCPU.vr[vrD], lvsl_helper+(ea & 0xf), 16);
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
	memmove(&aCPU.vr[vrD], lvsl_helper+0x10-(ea & 0xf), 16);
#else
#error Endianess not supported!
#endif
}
JITCFlow ppc_opc_gen_lvsr()
{
	ppc_opc_gen_check_vec();
	int rA, vrD, rB;
	byte modrm[6];
	PPC_OPC_TEMPL_X(jitc.current_opc, vrD, rA, rB);

	if (vrD == jitc.nativeVectorReg) {
		jitc.nativeVectorReg = VECTREG_NO;
	}

	jitc.clobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB);
	jitc.asmALU32(X86_AND, regA, 0x0f);

	if (jitc.hostCPUCaps.sse) {
		asmShiftRegImm(X86_SHL, regA, 4);

		NativeVectorReg reg1 = jitc.mapClientVectorRegisterDirty(vrD);

		asmALUPS(X86_MOVAPS, reg1,
			x86_mem2(modrm, regA, (uint32)&lvsr_helper_full));
	} else {
		jitcDropClientVectorRegister(vrD);
		jitcAssertFlushedVectorRegister(vrD);

		NativeReg reg1 = jitcAllocRegister();
		NativeReg reg2 = jitcAllocRegister();
		NativeReg reg3 = jitcAllocRegister();

		asmALURegMem(X86_MOV, reg1, modrm,
			x86_mem(modrm, regA, (uint32)&lvsl_helper));
		asmALURegMem(X86_MOV, reg2, modrm,
			x86_mem(modrm, regA, (uint32)&lvsl_helper+4));
		asmALURegMem(X86_MOV, reg3, modrm,
			x86_mem(modrm, regA, (uint32)&lvsl_helper+8));
		asmALURegMem(X86_MOV, regA, modrm,
			x86_mem(modrm, regA, (uint32)&lvsl_helper+12));

		asmMOVDMemReg((uint32)&(aCPU.vr[vrD].w[0]), reg1);
		asmMOVDMemReg((uint32)&(aCPU.vr[vrD].w[1]), reg2);
		asmMOVDMemReg((uint32)&(aCPU.vr[vrD].w[2]), reg3);
		asmMOVDMemReg((uint32)&(aCPU.vr[vrD].w[3]), regA);
	}

	return flowContinue;
}

/*
 *      dst	     Data Stream Touch
 *      v.115
 */
void ppc_opc_dst(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	/* Since we are not emulating the cache, this is a nop */
}
JITCFlow ppc_opc_gen_dst()
{
	/* Since we are not emulating the cache, this is a nop */
	return flowContinue;
}
#endif

/*
 *	stb		Store Byte
 *	.632
 */
void ppc_opc_stb(PPC_CPU_State &aCPU)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
	ppc_write_effective_byte(aCPU, (rA?aCPU.gpr[rA]:0)+imm, (uint8)aCPU.gpr[rS]);
}
JITCFlow ppc_opc_gen_stb(JITC &jitc)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
	ppc_opc_gen_helper_st(jitc, PPC_GPR(rA), imm, PPC_GPR(rS));
#if 0
	jitc.asmALU32(X86_MOV, RBX, RAX);
	jitc.asmALU32(X86_MOV, RCX, RAX);
	jitc.asmShift32(X86_SHR, RBX, 12);
	jitc.asmALU32(X86_AND, RCX, 0xfffff000);
	jitc.asmALU32(X86_AND, RBX, TLB_ENTRIES-1);
	
	jitc.asmALU32(X86_CMP, RCX, curCPUsib(tlb_data_write_eff, 4, RBX));
	NativeAddress fixup1 = jitc.asmJxxFixup(X86_E);
	jitc.asmCALL((NativeAddress)ppc_write_effective_byte_asm);
	NativeAddress fixup2 = jitc.asmJMPFixup();
	
	jitc.asmResolveFixup(fixup1, jitc.asmHERE());
	jitc.asmALU64(X86_ADD, RAX, curCPUsib(tlb_data_write_phys, 8, RBX));
	jitc.asmALU8(X86_MOV, RAX, 0u, RDX);
	
	jitc.asmResolveFixup(fixup2, jitc.asmHERE());
#else
	jitc.asmCALL((NativeAddress)ppc_write_effective_byte_asm);
#endif
	return flowEndBlock;
}
/*
 *	stbu		Store Byte with Update
 *	.633
 */
void ppc_opc_stbu(PPC_CPU_State &aCPU)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_byte(aCPU, aCPU.gpr[rA]+imm, (uint8)aCPU.gpr[rS]);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
	}
}
JITCFlow ppc_opc_gen_stbu(JITC &jitc)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
	// FIXME: check rA!=0
	ppc_opc_gen_helper_stu(jitc, PPC_GPR(rA), imm, PPC_GPR(rS));
	jitc.asmCALL((NativeAddress)ppc_write_effective_byte_asm);
	if (imm) {
		NativeReg r = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_ADD, r, imm);
	}
	return flowContinue;
}
/*
 *	stbux		Store Byte with Update Indexed
 *	.634
 */
void ppc_opc_stbux(PPC_CPU_State &aCPU)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_byte(aCPU, aCPU.gpr[rA]+aCPU.gpr[rB], (uint8)aCPU.gpr[rS]);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
	}
}
JITCFlow ppc_opc_gen_stbux(JITC &jitc)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stux(jitc, PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	jitc.asmCALL((NativeAddress)ppc_write_effective_byte_asm);
	NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	jitc.asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	stbx		Store Byte Indexed
 *	.635
 */
void ppc_opc_stbx(PPC_CPU_State &aCPU)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	ppc_write_effective_byte(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], (uint8)aCPU.gpr[rS]);
}
JITCFlow ppc_opc_gen_stbx(JITC &jitc)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stx(jitc, PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	jitc.asmCALL((NativeAddress)ppc_write_effective_byte_asm);
	return flowEndBlock;
}
/*
 *	stfd		Store Floating-Point Double
 *	.642
 */
void ppc_opc_stfd(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frS, rA, imm);
	ppc_write_effective_dword(aCPU, (rA?aCPU.gpr[rA]:0)+imm, aCPU.fpr[frS]);
}
JITCFlow ppc_opc_gen_stfd(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, frS, rA, imm);
	jitc.floatRegisterClobberAll();
	jitc.flushRegister();
	jitc.clobberCarryAndFlags();
	jitc.getClientRegister(PPC_FPR(frS), NATIVE_REG | RDX);
	if (rA) {
		jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
		if (imm) {
			jitc.asmALU32(X86_ADD, RAX, imm);
		}
	} else {
		jitc.asmALU32(X86_MOV, RAX, imm);
	}
	jitc.clobberAll();
#if 0
	jitc.asmALU32(X86_TEST, RAX, 7);
	NativeAddress fixup3 = jitc.asmJxxFixup(X86_NZ);
	
	ppc_opc_gen_helper_tlb(jitc, RAX, RBX, RCX);
	
	jitc.asmALU32(X86_CMP, RCX, curCPUsib(tlb_data_write_eff, 4, RBX));
	NativeAddress fixup1 = jitc.asmJxxFixup(X86_E);
	jitc.asmResolveFixup(fixup3, jitc.asmHERE());
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	NativeAddress fixup2 = jitc.asmJMPFixup();
	
	jitc.asmResolveFixup(fixup1, jitc.asmHERE());
	jitc.asmALU64(X86_ADD, RAX, curCPUsib(tlb_data_write_phys, 8, RBX));
	jitc.asmBSWAP64(RDX);
	jitc.asmALU64(X86_MOV, RAX, 0u, RDX);
	
	jitc.asmResolveFixup(fixup2, jitc.asmHERE());
#else
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_write_effective_dword_asm);
#endif
	return flowEndBlock;
}
/*
 *	stfdu		Store Floating-Point Double with Update
 *	.643
 */
void ppc_opc_stfdu(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frS, rA, imm);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_dword(aCPU, aCPU.gpr[rA]+imm, aCPU.fpr[frS]);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
	}
}
JITCFlow ppc_opc_gen_stfdu(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, frS, rA, imm);
	jitc.floatRegisterClobberAll();
	jitc.flushRegister();
	jitc.clobberCarryAndFlags();
	jitc.getClientRegister(PPC_FPR(frS), NATIVE_REG | RDX);
	// FIXME: check rA!=0
	jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
	if (imm) {
		jitc.asmALU32(X86_ADD, RAX, imm);
	}
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	if (imm) {
		NativeReg r = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_ADD, r, imm);
	}
	return flowContinue;
}
/*
 *	stfd		Store Floating-Point Double with Update Indexed
 *	.644
 */
void ppc_opc_stfdux(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_dword(aCPU, aCPU.gpr[rA]+aCPU.gpr[rB], aCPU.fpr[frS]);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
	}
}
JITCFlow ppc_opc_gen_stfdux(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frS, rA, rB);
	jitc.floatRegisterClobberAll();
	jitc.flushRegister();
	jitc.clobberCarryAndFlags();
	jitc.getClientRegister(PPC_FPR(frS), NATIVE_REG | RDX);
	// FIXME: check rA!=0
	jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
	jitc.asmALU32(X86_ADD, RAX, curCPUreg(PPC_GPR(rB)));
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	jitc.asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	stfdx		Store Floating-Point Double Indexed
 *	.645
 */
void ppc_opc_stfdx(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
	ppc_write_effective_dword(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], aCPU.fpr[frS]);
}
JITCFlow ppc_opc_gen_stfdx(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frS, rA, rB);
	jitc.floatRegisterClobberAll();
	jitc.flushRegister();
	jitc.clobberCarryAndFlags();
	jitc.getClientRegister(PPC_FPR(frS), NATIVE_REG | RDX);
	if (rA) {
		jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
		jitc.asmALU32(X86_ADD, RAX, curCPUreg(PPC_GPR(rB)));
	} else {
		jitc.getClientRegister(PPC_GPR(rB), NATIVE_REG | RAX);
	}
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	return flowEndBlock;
}
/*
 *	stfiwx		Store Floating-Point as Integer Word Indexed
 *	.646
 */
void ppc_opc_stfiwx(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
	ppc_write_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], (uint32)aCPU.fpr[frS]);
}
JITCFlow ppc_opc_gen_stfiwx(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frS, rA, rB);
	jitc.floatRegisterClobberAll();
	ppc_opc_gen_helper_stx(jitc, PPC_GPR(rA), PPC_GPR(rB), PPC_FPR(frS));
	// FIXME64: loading lower half would be enough
	jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
	return flowEndBlock;
}
/*
 *	stfs		Store Floating-Point Single
 *	.647
 */
void ppc_opc_stfs(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frS, rA, imm);
	uint32 s;
	ppc_double d;
	ppc_fpu_unpack_double(d, aCPU.fpr[frS]);
	ppc_fpu_pack_single(aCPU.fpscr, d, s);
	ppc_write_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+imm, s);
}
JITCFlow ppc_opc_gen_stfs(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, frS, rA, imm);
	jitc.floatRegisterClobberAll();
	jitc.getClientRegister(PPC_FPR(frS), NATIVE_REG | RSI);
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RDI, curCPU(fpscr));
	jitc.asmCALL((NativeAddress)ppc_opc_double_to_single);
	jitc.asmALU32(X86_MOV, RDX, RAX);
	if (rA) {
		jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
		if (imm) {
			jitc.asmALU32(X86_ADD, RAX, imm);
		}
	} else {
		jitc.asmALU32(X86_MOV, RAX, imm);
	}
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
	return flowEndBlock;
}
/*
 *	stfsu		Store Floating-Point Single with Update
 *	.648
 */
void ppc_opc_stfsu(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, frS, rA, imm);
	// FIXME: check rA!=0
	uint32 s;
	ppc_double d;
	ppc_fpu_unpack_double(d, aCPU.fpr[frS]);
	ppc_fpu_pack_single(aCPU.fpscr, d, s);
	int ret = ppc_write_effective_word(aCPU, aCPU.gpr[rA]+imm, s);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
	}
}
JITCFlow ppc_opc_gen_stfsu(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, frS, rA, imm);
	jitc.floatRegisterClobberAll();
	jitc.getClientRegister(PPC_FPR(frS), NATIVE_REG | RSI);
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RDI, curCPU(fpscr));
	jitc.asmCALL((NativeAddress)ppc_opc_double_to_single);
	jitc.asmALU32(X86_MOV, RDX, RAX);
	// FIXME: check rA!=0
	jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
	if (imm) {
		jitc.asmALU32(X86_ADD, RAX, imm);
	}
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
	if (imm) {
		NativeReg r = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_ADD, r, imm);
	}
	return flowContinue;
}
/*
 *	stfsux		Store Floating-Point Single with Update Indexed
 *	.649
 */
void ppc_opc_stfsux(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
	// FIXME: check rA!=0
	uint32 s;
	ppc_double d;
	ppc_fpu_unpack_double(d, aCPU.fpr[frS]);
	ppc_fpu_pack_single(aCPU.fpscr, d, s);
	int ret = ppc_write_effective_word(aCPU, aCPU.gpr[rA]+aCPU.gpr[rB], s);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
	}
}
JITCFlow ppc_opc_gen_stfsux(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frS, rA, rB);
	jitc.floatRegisterClobberAll();
	jitc.getClientRegister(PPC_FPR(frS), NATIVE_REG | RSI);
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RDI, curCPU(fpscr));
	jitc.asmCALL((NativeAddress)ppc_opc_double_to_single);
	jitc.asmALU32(X86_MOV, RDX, RAX);
	// FIXME: check rA!=0
	jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
	jitc.asmALU32(X86_ADD, RAX, curCPUreg(PPC_GPR(rB)));
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
	NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	jitc.asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	stfsx		Store Floating-Point Single Indexed
 *	.650
 */
void ppc_opc_stfsx(PPC_CPU_State &aCPU)
{
	if ((aCPU.msr & MSR_FP) == 0) {
//		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frS, rA, rB);
	uint32 s;
	ppc_double d;
	ppc_fpu_unpack_double(d, aCPU.fpr[frS]);
	ppc_fpu_pack_single(aCPU.fpscr, d, s);
	ppc_write_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], s);
}
JITCFlow ppc_opc_gen_stfsx(JITC &jitc)
{
	ppc_opc_gen_check_fpu(jitc);
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, frS, rA, rB);
	jitc.floatRegisterClobberAll();
	jitc.getClientRegister(PPC_FPR(frS), NATIVE_REG | RSI);
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RDI, curCPU(fpscr));
	jitc.asmCALL((NativeAddress)ppc_opc_double_to_single);
	jitc.asmALU32(X86_MOV, RDX, RAX);
	if (rA) {
		jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
		jitc.asmALU32(X86_ADD, RAX, curCPUreg(PPC_GPR(rB)));
	} else {
		jitc.getClientRegister(PPC_GPR(rB), NATIVE_REG | RAX);
	}
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
	return flowEndBlock;
}
/*
 *	sth		Store Half Word
 *	.651
 */
void ppc_opc_sth(PPC_CPU_State &aCPU)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
	ppc_write_effective_half(aCPU, (rA?aCPU.gpr[rA]:0)+imm, (uint16)aCPU.gpr[rS]);
}
JITCFlow ppc_opc_gen_sth(JITC &jitc)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
	ppc_opc_gen_helper_st(jitc, PPC_GPR(rA), imm, PPC_GPR(rS));
	jitc.asmCALL((NativeAddress)ppc_write_effective_half_asm);
	return flowEndBlock;
}
/*
 *	sthbrx		Store Half Word Byte-Reverse Indexed
 *	.652
 */
void ppc_opc_sthbrx(PPC_CPU_State &aCPU)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	ppc_write_effective_half(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], ppc_bswap_half((uint16)aCPU.gpr[rS]));
}
JITCFlow ppc_opc_gen_sthbrx(JITC &jitc)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stx(jitc, PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	jitc.asmShift16(X86_ROL, RDX, 8);
	jitc.asmCALL((NativeAddress)ppc_write_effective_half_asm);
	return flowEndBlock;
}
/*
 *	sthu		Store Half Word with Update
 *	.653
 */
void ppc_opc_sthu(PPC_CPU_State &aCPU)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_half(aCPU, aCPU.gpr[rA]+imm, (uint16)aCPU.gpr[rS]);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
	}
}
JITCFlow ppc_opc_gen_sthu(JITC &jitc)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
	// FIXME: check rA!=0
	ppc_opc_gen_helper_stu(jitc, PPC_GPR(rA), imm, PPC_GPR(rS));
	jitc.asmCALL((NativeAddress)ppc_write_effective_half_asm);
	if (imm) {
		NativeReg r = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_ADD, r, imm);
	}
	return flowContinue;
}
/*
 *	sthux		Store Half Word with Update Indexed
 *	.654
 */
void ppc_opc_sthux(PPC_CPU_State &aCPU)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_half(aCPU, aCPU.gpr[rA]+aCPU.gpr[rB], (uint16)aCPU.gpr[rS]);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
	}
}
JITCFlow ppc_opc_gen_sthux(JITC &jitc)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stux(jitc, PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	jitc.asmCALL((NativeAddress)ppc_write_effective_half_asm);
	NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	jitc.asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	sthx		Store Half Word Indexed
 *	.655
 */
void ppc_opc_sthx(PPC_CPU_State &aCPU)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	ppc_write_effective_half(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], (uint16)aCPU.gpr[rS]);
}
JITCFlow ppc_opc_gen_sthx(JITC &jitc)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stx(jitc, PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	jitc.asmCALL((NativeAddress)ppc_write_effective_half_asm);
	return flowEndBlock;
}
/*
 *	stmw		Store Multiple Word
 *	.656
 */
void ppc_opc_stmw(PPC_CPU_State &aCPU)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
	uint32 ea = (rA ? aCPU.gpr[rA] : 0) + imm;
	while (rS <= 31) {
		if (ppc_write_effective_word(aCPU, ea, aCPU.gpr[rS])) {
			return;
		}
		rS++;
		ea += 4;
	}
}
JITCFlow ppc_opc_gen_stmw(JITC &jitc)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
        while (rS <= 31) {
                ppc_opc_gen_helper_st(jitc, PPC_GPR(rA), imm, PPC_GPR(rS));
//                jitc.getClientRegister(PPC_GPR(rS), NATIVE_REG | RDX);
                jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
                rS += 1;
                imm += 4;
        } 
	return flowEndBlock;
}
/*
 *	stswi		Store String Word Immediate
 *	.657
 */
void ppc_opc_stswi(PPC_CPU_State &aCPU)
{
	int rA, rS, NB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, NB);
	if (NB==0) NB=32;
	uint32 ea = rA ? aCPU.gpr[rA] : 0;
	uint32 r = 0;
	int i = 0;
	
	while (NB > 0) {
		if (!i) {
			r = aCPU.gpr[rS];
			rS++;
			rS%=32;
			i = 4;
		}
		if (ppc_write_effective_byte(aCPU, ea, (r>>24))) {
			return;
		}
		r<<=8;
		ea++;
		i--;
		NB--;
	}
}
JITCFlow ppc_opc_gen_stswi(JITC &jitc)
{
	int rA, rS, NB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, NB);
	if (NB==0) NB=32;
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	if (rA) {
		jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
	} else {
		jitc.asmALU32(X86_MOV, RAX, 0);
	}
	jitc.asmALU32(X86_MOV, RCX, NB);
	jitc.asmALU32(X86_MOV, RBX, rS);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.clobberAll();
	jitc.asmCALL((NativeAddress)ppc_opc_stswi_asm);
	return flowEndBlock;
}
/*
 *	stswx		Store String Word Indexed
 *	.658
 */
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
			rS%=32;
			i = 4;
		}
		if (ppc_write_effective_byte(aCPU, ea, (r>>24))) {
			return;
		}
		r<<=8;
		ea++;
		i--;
		NB--;
	}
}
JITCFlow ppc_opc_gen_stswx(JITC &jitc)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	jitc.getClientRegister(PPC_XER, NATIVE_REG | RCX);
	if (rA) {
		jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
		jitc.asmALU32(X86_ADD, RAX, curCPUreg(PPC_GPR(rB)));
	} else {
		jitc.getClientRegister(PPC_GPR(rB), NATIVE_REG | RAX);
	}
	jitc.asmALU32(X86_AND, RCX, 0x7f);
	jitc.clobberAll();
	NativeAddress fixup = jitc.asmJxxFixup(X86_Z);
	jitc.asmALU32(X86_MOV, RBX, rS);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_opc_stswi_asm);
	jitc.asmResolveFixup(fixup, jitc.asmHERE());
	return flowEndBlock;
}
/*
 *	stw		Store Word
 *	.659
 */
void ppc_opc_stw(PPC_CPU_State &aCPU)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
	ppc_write_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+imm, aCPU.gpr[rS]);
}
JITCFlow ppc_opc_gen_stw(JITC &jitc)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
	ppc_opc_gen_helper_st(jitc, PPC_GPR(rA), imm, PPC_GPR(rS));
#if 0
	jitc.asmALU32(X86_TEST, RAX, 3);
	NativeAddress fixup3 = jitc.asmJxxFixup(X86_NZ);
	
	ppc_opc_gen_helper_tlb(jitc, RAX, RBX, RCX);
	
	jitc.asmALU32(X86_CMP, RCX, curCPUsib(tlb_data_write_eff, 4, RBX));
	NativeAddress fixup1 = jitc.asmJxxFixup(X86_E);
	jitc.asmResolveFixup(fixup3, jitc.asmHERE());
	jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
	NativeAddress fixup2 = jitc.asmJMPFixup();
	
	jitc.asmResolveFixup(fixup1, jitc.asmHERE());
	jitc.asmALU64(X86_ADD, RAX, curCPUsib(tlb_data_write_phys, 8, RBX));
	jitc.asmBSWAP32(RDX);
	jitc.asmALU32(X86_MOV, RAX, 0u, RDX);
	
	jitc.asmResolveFixup(fixup2, jitc.asmHERE());
	return flowEndBlock;
#else
	jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
	return flowEndBlock;
#endif
}
/*
 *	stwbrx		Store Word Byte-Reverse Indexed
 *	.660
 */
void ppc_opc_stwbrx(PPC_CPU_State &aCPU)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	// FIXME: doppelt gemoppelt
	ppc_write_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], ppc_bswap_word(aCPU.gpr[rS]));
}
JITCFlow ppc_opc_gen_stwbrx(JITC &jitc)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stx(jitc, PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	jitc.asmBSWAP32(RDX);
	jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
	return flowEndBlock;
}
/*
 *	stwcx.		Store Word Conditional Indexed
 *	.661
 */
void ppc_opc_stwcx_(PPC_CPU_State &aCPU)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	aCPU.cr &= 0x0fffffff;
	if (aCPU.have_reservation) {
		aCPU.have_reservation = false;
		uint32 v;
		if (ppc_read_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], v)) {
			return;
		}
		if (v==aCPU.reserve) {
			if (ppc_write_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], aCPU.gpr[rS])) {
				return;
			}
			aCPU.cr |= CR_CR0_EQ;
		}
		if (aCPU.xer & XER_SO) {
			aCPU.cr |= CR_CR0_SO;
		}
	}
}
JITCFlow ppc_opc_gen_stwcx_(JITC &jitc)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	jitc.clobberCarryAndFlags();
	jitc.asmALU8(X86_AND, curCPU(cr)+3, 0x0f);
	jitc.asmBTx32(X86_BTR, curCPU(have_reservation), 0);
	NativeAddress no_reservation = jitc.asmJxxFixup(X86_NC);
		ppc_opc_gen_helper_lx(jitc, PPC_GPR(rA), PPC_GPR(rB));
		jitc.asmCALL((NativeAddress)ppc_read_effective_word_asm);
		jitc.asmALU32(X86_CMP, RDX, curCPU(reserve));
		// FIXME: mapFlags?
		NativeAddress fixup = jitc.asmJxxFixup(X86_NE);
		ppc_opc_gen_helper_stx(jitc, PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
		jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
		jitc.asmOR32(curCPU(cr), 0x20000000);  // CR_CR0_EQ
	
	jitc.asmResolveFixup(fixup, jitc.asmHERE());
	jitc.asmResolveFixup(no_reservation, jitc.asmHERE());
	
	jitc.asmTEST32(curCPU(xer), XER_SO);
	fixup = jitc.asmJxxFixup(X86_Z);
		jitc.asmOR32(curCPU(cr), 0x10000000);  // CR_CR0_SO
	jitc.asmResolveFixup(fixup, jitc.asmHERE());
	return flowEndBlock;
}
/*
 *	stwu		Store Word with Update
 *	.663
 */
void ppc_opc_stwu(PPC_CPU_State &aCPU)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rS, rA, imm);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_word(aCPU, aCPU.gpr[rA]+imm, aCPU.gpr[rS]);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += imm;
	}
}
JITCFlow ppc_opc_gen_stwu(JITC &jitc)
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rS, rA, imm);
	// FIXME: check rA!=0
	ppc_opc_gen_helper_stu(jitc, PPC_GPR(rA), imm, PPC_GPR(rS));
	jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
	if (imm) {
		NativeReg r = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_ADD, r, imm);
	}
	return flowContinue;
}
/*
 *	stwux		Store Word with Update Indexed
 *	.664
 */
void ppc_opc_stwux(PPC_CPU_State &aCPU)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_word(aCPU, aCPU.gpr[rA]+aCPU.gpr[rB], aCPU.gpr[rS]);
	if (ret == PPC_MMU_OK) {
		aCPU.gpr[rA] += aCPU.gpr[rB];
	}
}
JITCFlow ppc_opc_gen_stwux(JITC &jitc)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stux(jitc, PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
	NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	jitc.asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	stwx		Store Word Indexed
 *	.665
 */
void ppc_opc_stwx(PPC_CPU_State &aCPU)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	ppc_write_effective_word(aCPU, (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB], aCPU.gpr[rS]);
}
JITCFlow ppc_opc_gen_stwx(JITC &jitc)
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stx(jitc, PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
#if 0
	jitc.asmALU32(X86_TEST, RAX, 3);
	NativeAddress fixup3 = jitc.asmJxxFixup(X86_NZ);
	
	ppc_opc_gen_helper_tlb(jitc, RAX, RBX, RCX);
	
	jitc.asmALU32(X86_CMP, RCX, curCPUsib(tlb_data_write_eff, 4, RBX));
	NativeAddress fixup1 = jitc.asmJxxFixup(X86_E);
	jitc.asmResolveFixup(fixup3, jitc.asmHERE());
	jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
	NativeAddress fixup2 = jitc.asmJMPFixup();
	
	jitc.asmResolveFixup(fixup1, jitc.asmHERE());
	jitc.asmALU64(X86_ADD, RAX, curCPUsib(tlb_data_write_phys, 8, RBX));
	jitc.asmBSWAP32(RDX);
	jitc.asmALU32(X86_MOV, RAX, 0u, RDX);
	
	jitc.asmResolveFixup(fixup2, jitc.asmHERE());
#else
	jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
#endif
	return flowEndBlock;
}

#if 0
/*      stvx	    Store Vector Indexed
 *      v.134
 */
void ppc_opc_stvx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
	if ((aCPU.msr & MSR_VEC) == 0) {
//		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrS, rA, rB);

	int ea = ((rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB]);

	ppc_write_effective_qword(ea, aCPU.vr[vrS]);
}
JITCFlow ppc_opc_gen_stvx(JITC &jitc)
{
	ppc_opc_gen_check_vec(jitc);
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, vrS, rA, rB);

	jitc.flushClientVectorRegister(vrS);
	jitcAssertFlushedVectorRegister(vrS);

	jitc.clobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(jitc, rA, rB, NATIVE_REG | RAX);

#if 1
	jitc.clobberAll();
	if (regA != RAX)	jitc.asmALU32(X86_MOV, RAX, regA);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);

	if (0 && vrS == jitc.nativeVectorReg) {
		jitc.asmALU32(X86_MOV, EDX, (uint32)&(aCPU.vr[JITC_VECTOR_TEMP]));
		jitc.asmCALL((NativeAddress)ppc_write_effective_qword_sse_asm);
	} else {
		jitc.asmALU32(X86_MOV, EDX, (uint32)&(aCPU.vr[vrS]));
		jitc.asmCALL((NativeAddress)ppc_write_effective_qword_asm);
	}
#else
	jitc.asmALU32(X86_AND, regA, ~0x0f);
	asmMOVDMemReg((uint32)&aCPU.vtemp, regA);

	jitc.clobberAll();
	if (regA != RAX)	jitc.asmALU32(X86_MOV, RAX, regA);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);

	asmMOVRegDMem(RCX, (uint32)&(aCPU.vr[vrS])+12);
	asmMOVRegDMem(EDX, (uint32)&(aCPU.vr[vrS])+8);

	jitc.asmCALL((NativeAddress)ppc_write_effective_dword_asm);

	asmMOVRegDMem(RAX, (uint32)&aCPU.vtemp);
	jitc.asmALU32(X86_OR, RAX, 8);

	asmMOVRegDMem(RCX, (uint32)&(aCPU.vr[vrS])+4);
	asmMOVRegDMem(EDX, (uint32)&(aCPU.vr[vrS])+0);

	jitc.asmCALL((NativeAddress)ppc_write_effective_dword_asm);
#endif
	return flowEndBlock;
}

/*      stvxl	   Store Vector Indexed LRU
 *      v.135
 */
void ppc_opc_stvxl(PPC_CPU_State &aCPU)
{
	ppc_opc_stvx(aCPU);
	/* This instruction should hint to the cache that the value won't be
	 *   needed again in memory anytime soon.  We don't emulate the cache,
	 *   so this is effectively exactly the same as stvx.
	 */
}
JITCFlow ppc_opc_gen_stvxl(JITC &jitc)
{
	return ppc_opc_gen_stvx(jitc);
}

/*      stvRBX	  Store Vector Element Byte Indexed
 *      v.131
 */
void ppc_opc_stvebx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
	if ((aCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrS, rA, rB);
	uint32 ea;
	ea = (rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB];
	ppc_write_effective_byte(ea, VECT_B(aCPU.vr[vrS], ea & 0xf));
}
JITCFlow ppc_opc_gen_stvebx(JITC &jitc)
{
	ppc_opc_gen_check_vec(jitc);
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, vrS, rA, rB);

	jitc.flushClientVectorRegister(vrS);
	jitcAssertFlushedVectorRegister(vrS);

	jitc.clobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(jitc, rA, rB, NATIVE_REG | RAX);
	asmMOVDMemReg((uint32)&aCPU.vtemp, regA);
	jitc.asmALU32(X86_AND, regA, 0x0f);
	asmALUReg(X86_NOT, regA);

	jitc.clobberAll();
	if (regA != RAX)	jitc.asmALU32(X86_MOV, RAX, regA);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);

	asmALURegMem8(X86_MOV, DL, modrm,
		x86_mem(modrm, RAX, ((uint32)&aCPU.vr[vrS])+16));

	asmMOVRegDMem(RAX, (uint32)&aCPU.vtemp);

	jitc.asmCALL((NativeAddress)ppc_write_effective_byte_asm);
	return flowEndBlock;
}


/*      stvehx	  Store Vector Element Half Word Indexed
 *      v.132
 */
void ppc_opc_stvehx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
	if ((aCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrS, rA, rB);
	uint32 ea;
	ea = ((rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB]) & ~1;
	ppc_write_effective_half(ea, VECT_H(aCPU.vr[vrS], (ea & 0xf) >> 1));
}
JITCFlow ppc_opc_gen_stvehx(JITC &jitc)
{
	ppc_opc_gen_check_vec(jitc);
	int rA, vrS, rB;
	byte modrm[6];
	PPC_OPC_TEMPL_X(jitc.current_opc, vrS, rA, rB);

	jitc.flushClientVectorRegister(vrS);
	jitcAssertFlushedVectorRegister(vrS);

	jitc.clobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(jitc, rA, rB, NATIVE_REG | RAX);
	asmMOVDMemReg((uint32)&aCPU.vtemp, regA);
	jitc.asmALU32(X86_AND, regA, 0x0e);
	asmALUReg(X86_NOT, regA);

	jitc.clobberAll();
	if (regA != RAX)	jitc.asmALU32(X86_MOV, RAX, regA);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);

	asmALURegMem8(X86_MOV, DL, modrm,
		x86_mem(modrm, RAX, ((uint32)&aCPU.vr[vrS])+15));
	asmALURegMem8(X86_MOV, DH, modrm,
		x86_mem(modrm, RAX, ((uint32)&aCPU.vr[vrS])+16));

	asmMOVRegDMem(RAX, (uint32)&aCPU.vtemp);
	jitc.asmALU32(X86_AND, RAX, ~0x1);

	jitc.asmCALL((NativeAddress)ppc_write_effective_half_asm);
	return flowEndBlock;
}


/*      stvewx	  Store Vector Element Word Indexed
 *      v.133
 */
void ppc_opc_stvewx(PPC_CPU_State &aCPU)
{
#ifndef __VEC_EXC_OFF__
	if ((aCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, vrS, rA, rB);
	uint32 ea;
	ea = ((rA?aCPU.gpr[rA]:0)+aCPU.gpr[rB]) & ~3;
	ppc_write_effective_word(ea, VECT_W(aCPU.vr[vrS], (ea & 0xf) >> 2));
}
JITCFlow ppc_opc_gen_stvewx(JITC &jitc)
{
	ppc_opc_gen_check_vec(jitc);
	int rA, vrS, rB;
	byte modrm[6];
	PPC_OPC_TEMPL_X(jitc.current_opc, vrS, rA, rB);

	jitc.flushClientVectorRegister(vrS);
	jitcAssertFlushedVectorRegister(vrS);

	jitc.clobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(jitc, rA, rB, NATIVE_REG | RAX);
	asmMOVDMemReg((uint32)&aCPU.vtemp, regA);
	jitc.asmALU32(X86_AND, regA, 0x0c);
	asmALUReg(X86_NOT, regA);

	jitc.clobberAll();
	if (regA != RAX) jitc.asmALU32(X86_MOV, RAX, regA);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);

	asmALURegMem(X86_MOV, EDX, modrm,
		x86_mem(modrm, RAX, ((uint32)&aCPU.vr[vrS])+13));

	asmMOVRegDMem(RAX, (uint32)&aCPU.vtemp);
	jitc.asmALU32(X86_AND, RAX, ~0x3);

	jitc.asmCALL((NativeAddress)ppc_write_effective_word_asm);
	return flowEndBlock;
}

/*      dstst	   Data Stream Touch for Store
 *      v.117
 */
void ppc_opc_dstst(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	/* Since we are not emulating the cache, this is a nop */
}
JITCFlow ppc_opc_gen_dstst(JITC &jitc)
{
	/* Since we are not emulating the cache, this is a nop */
	return flowContinue;
}

/*      dss	     Data Stream Stop
 *      v.114
 */
void ppc_opc_dss(PPC_CPU_State &aCPU)
{
	VECTOR_DEBUG;
	/* Since we are not emulating the cache, this is a nop */
}
JITCFlow ppc_opc_gen_dss(JITC &jitc)
{
	/* Since we are not emulating the cache, this is a nop */
	return flowContinue;
}
#endif
