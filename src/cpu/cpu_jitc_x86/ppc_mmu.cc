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

#include "x86asm.h"
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

int FASTCALL ppc_effective_to_physical(uint32 addr, int flags, uint32 &result)
{
	if (flags & PPC_MMU_CODE) {
		if (!(gCPU.msr & MSR_IR)) {
			result = addr;
			return PPC_MMU_OK;
		}
		/*
		 * BAT translation .329
		 */
		for (int i=0; i<4; i++) {
			if ((addr & gCPU.ibat_bl[i]) == gCPU.ibat_bepi[i]) {
				// bat applies to this address
				if (((gCPU.ibatu[i] & BATU_Vs) && !(gCPU.msr & MSR_PR))
				 || ((gCPU.ibatu[i] & BATU_Vp) &&  (gCPU.msr & MSR_PR))) {
					// bat entry valid
					addr &= gCPU.ibat_nbl[i];
					addr |= gCPU.ibat_brpn[i];
					result = addr;
					// FIXME: check access rights
					return PPC_MMU_OK;
				}
			}
		}
	} else {
		if (!(gCPU.msr & MSR_DR)) {
			result = addr;
			return PPC_MMU_OK;
		}
		/*
		 * BAT translation .329
		 */
		for (int i=0; i<4; i++) {
			if ((addr & gCPU.dbat_bl[i]) == gCPU.dbat_bepi[i]) {
				// bat applies to this address
				if (((gCPU.dbatu[i] & BATU_Vs) && !(gCPU.msr & MSR_PR))
				 || ((gCPU.dbatu[i] & BATU_Vp) &&  (gCPU.msr & MSR_PR))) {
					// bat entry valid
					addr &= gCPU.dbat_nbl[i];
					addr |= gCPU.dbat_brpn[i];
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
	uint32 sr = gCPU.sr[EA_SR(addr)];

	if (sr & SR_T) {
		// woea
		// FIXME: implement me
		PPC_MMU_ERR("sr & T\n");
	} else {
#ifdef TLB	
		for (int i=0; i<4; i++) {
			if ((addr & ~0xfff) == (gCPU.tlb_va[i])) {
				gCPU.tlb_last = i;
//				ht_printf("TLB: %d: %08x -> %08x\n", i, addr, gCPU.tlb_pa[i] | (addr & 0xfff));
				result = gCPU.tlb_pa[i] | (addr & 0xfff);
				return PPC_MMU_OK;
			}
		}
#endif
		// page address translation
		if ((flags & PPC_MMU_CODE) && (sr & SR_N)) {
			// segment isnt executable
			if (!(flags & PPC_MMU_NO_EXC)) {
				ppc_exception(PPC_EXC_ISI, PPC_EXC_SRR1_GUARD);
				return PPC_MMU_EXC;
			}
			return PPC_MMU_FATAL;
		}
		uint32 offset = EA_Offset(addr);	 // 12 bit
		uint32 page_index = EA_PageIndex(addr);  // 16 bit
		uint32 VSID = SR_VSID(sr);	       // 24 bit
		uint32 api = EA_API(addr);	       //  6 bit (part of page_index)
		// VSID.page_index = Virtual Page Number (VPN)

		// Hashfunction no 1 "xor" .360
		uint32 hash1 = (VSID ^ page_index);
		uint32 pteg_addr = ((hash1 & gCPU.pagetable_hashmask)<<6) | gCPU.pagetable_base;
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
						if (!(flags & PPC_MMU_NO_EXC)) {
							PPC_MMU_ERR("read physical in address translate failed\n");
							return PPC_MMU_EXC;
						}
						return PPC_MMU_FATAL;
					}
					// check accessmode .346
					int key;
					if (gCPU.msr & MSR_PR) {
						key = (sr & SR_Kp) ? 4 : 0;
					} else {
						key = (sr & SR_Ks) ? 4 : 0;
					}
					if (!ppc_pte_protection[((flags&PPC_MMU_WRITE)?8:0) + key + PTE2_PP(pte)]) {
						if (!(flags & PPC_MMU_NO_EXC)) {
							if (flags & PPC_MMU_CODE) {
								PPC_MMU_WARN("correct impl? code + read protection\n");
								ppc_exception(PPC_EXC_ISI, PPC_EXC_SRR1_PROT, addr);
								return PPC_MMU_EXC;
							} else {
								if (flags & PPC_MMU_WRITE) {
									ppc_exception(PPC_EXC_DSI, PPC_EXC_DSISR_PROT | PPC_EXC_DSISR_STORE, addr);
								} else {
									ppc_exception(PPC_EXC_DSI, PPC_EXC_DSISR_PROT, addr);
								}
								return PPC_MMU_EXC;
							}
						}
						return PPC_MMU_FATAL;
					}
					// ok..
					uint32 pap = PTE2_RPN(pte);
					result = pap | offset;
#ifdef TLB
					gCPU.tlb_last++;
					gCPU.tlb_last &= 3;
					gCPU.tlb_pa[gCPU.tlb_last] = pap;
					gCPU.tlb_va[gCPU.tlb_last] = addr & ~0xfff;					
//					ht_printf("TLB: STORE %d: %08x -> %08x\n", gCPU.tlb_last, addr, pap);
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
		pteg_addr = ((hash1 & gCPU.pagetable_hashmask)<<6) | gCPU.pagetable_base;
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
					if (gCPU.msr & MSR_PR) {
						key = (sr & SR_Kp) ? 4 : 0;
					} else {
						key = (sr & SR_Ks) ? 4 : 0;
					}
					if (!ppc_pte_protection[((flags&PPC_MMU_WRITE)?8:0) + key + PTE2_PP(pte)]) {
						if (!(flags & PPC_MMU_NO_EXC)) {
							if (flags & PPC_MMU_CODE) {
								PPC_MMU_WARN("correct impl? code + read protection\n");
								ppc_exception(PPC_EXC_ISI, PPC_EXC_SRR1_PROT, addr);
								return PPC_MMU_EXC;
							} else {
								if (flags & PPC_MMU_WRITE) {
									ppc_exception(PPC_EXC_DSI, PPC_EXC_DSISR_PROT | PPC_EXC_DSISR_STORE, addr);
								} else {
									ppc_exception(PPC_EXC_DSI, PPC_EXC_DSISR_PROT, addr);
								}
								return PPC_MMU_EXC;
							}
						}
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
	if (!(flags & PPC_MMU_NO_EXC)) {
		if (flags & PPC_MMU_CODE) {
			ppc_exception(PPC_EXC_ISI, PPC_EXC_SRR1_PAGE);
		} else {
			if (flags & PPC_MMU_WRITE) {
				ppc_exception(PPC_EXC_DSI, PPC_EXC_DSISR_PAGE | PPC_EXC_DSISR_STORE, addr);
			} else {
				ppc_exception(PPC_EXC_DSI, PPC_EXC_DSISR_PAGE, addr);
			}
		}
		return PPC_MMU_EXC;
	}
	return PPC_MMU_FATAL;
}

int FASTCALL ppc_effective_to_physical_vm(uint32 addr, int flags, uint32 &result)
{
	if (!(gCPU.msr & MSR_DR)) {
		result = addr;
		return PPC_MMU_READ | PPC_MMU_WRITE;
	}
	/*
	 * BAT translation .329
	 */
	for (int i=0; i<4; i++) {
		if ((addr & gCPU.dbat_bl[i]) == gCPU.dbat_bepi[i]) {
			// bat applies to this address
			if (((gCPU.dbatu[i] & BATU_Vs) && !(gCPU.msr & MSR_PR))
			 || ((gCPU.dbatu[i] & BATU_Vp) &&  (gCPU.msr & MSR_PR))) {
				// bat entry valid
				addr &= gCPU.dbat_nbl[i];
				addr |= gCPU.dbat_brpn[i];
				result = addr;
				// FIXME: check access rights
				return PPC_MMU_OK;
			}
		}
	}
	
	/*
	 * Address translation with segment register
	 */
	uint32 sr = gCPU.sr[EA_SR(addr)];

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
		uint32 pteg_addr = ((hash1 & gCPU.pagetable_hashmask)<<6) | gCPU.pagetable_base;
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
					if (gCPU.msr & MSR_PR) {
						key = (sr & SR_Kp) ? 4 : 0;
					} else {
						key = (sr & SR_Ks) ? 4 : 0;
					}
					int ret = PPC_MMU_WRITE | PPC_MMU_READ;
					if (!ppc_pte_protection[8 + key + PTE2_PP(pte)]) {
						if (!(flags & PPC_MMU_NO_EXC)) {
							if (flags & PPC_MMU_WRITE) {
								gCPU.dsisr = PPC_EXC_DSISR_PROT | PPC_EXC_DSISR_STORE;
							}
						}
						ret &= ~PPC_MMU_WRITE;
					}
					if (!ppc_pte_protection[key + PTE2_PP(pte)]) {
						if (!(flags & PPC_MMU_NO_EXC)) {
							if (!(flags & PPC_MMU_WRITE)) {
								gCPU.dsisr = PPC_EXC_DSISR_PROT;
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
		pteg_addr = ((hash1 & gCPU.pagetable_hashmask)<<6) | gCPU.pagetable_base;
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
					if (gCPU.msr & MSR_PR) {
						key = (sr & SR_Kp) ? 4 : 0;
					} else {
						key = (sr & SR_Ks) ? 4 : 0;
					}
					int ret = PPC_MMU_WRITE | PPC_MMU_READ;
					if (!ppc_pte_protection[8 + key + PTE2_PP(pte)]) {
						if (!(flags & PPC_MMU_NO_EXC)) {
							if (flags & PPC_MMU_WRITE) {
								gCPU.dsisr = PPC_EXC_DSISR_PROT | PPC_EXC_DSISR_STORE;
							}
						}
						ret &= ~PPC_MMU_WRITE;
					}
					if (!ppc_pte_protection[key + PTE2_PP(pte)]) {
						if (!(flags & PPC_MMU_NO_EXC)) {
							if (!(flags & PPC_MMU_WRITE)) {
								gCPU.dsisr = PPC_EXC_DSISR_PROT;
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
			gCPU.dsisr = PPC_EXC_DSISR_PAGE | PPC_EXC_DSISR_STORE;
		} else {
			gCPU.dsisr = PPC_EXC_DSISR_PAGE;
		}
	}
	return PPC_MMU_OK;
}

void ppc_mmu_tlb_invalidate()
{
	gCPU.effective_code_page = 0xffffffff;
	ppc_mmu_tlb_invalidate_all_asm();
}

/*
pagetable:
min. 2^10 (64k) PTEGs
PTEG = 64byte
The page table can be any size 2^n where 16 <= n <= 25.

A PTEG contains eight
PTEs of eight bytes each; therefore, each PTEG is 64 bytes long.
*/

bool FASTCALL ppc_mmu_set_sdr1(uint32 newval, bool quiesce)
{
	/* if (newval == gCPU.sdr1)*/ quiesce = false;
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
	gCPU.pagetable_base = htaborg<<16;
	gCPU.sdr1 = newval;
	gCPU.pagetable_hashmask = ((xx<<10)|0x3ff);
	uint a = (0xffffffff & gCPU.pagetable_hashmask) | gCPU.pagetable_base;
	if (a > gMemorySize) {
		PPC_MMU_WARN("new pagetable: not in memory (%08x)\n", a);
		return false;
	}	
	PPC_MMU_TRACE("new pagetable: sdr1 accepted\n");
	PPC_MMU_TRACE("number of pages: 2^%d pagetable_start: 0x%08x size: 2^%d\n", n+13, gCPU.pagetable_base, n+16);
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

int FASTCALL ppc_direct_effective_memory_handle(uint32 addr, byte *&ptr)
{
	uint32 ea;
	int r;
	if (!((r = ppc_effective_to_physical(addr, PPC_MMU_READ, ea)))) {
		return ppc_direct_physical_memory_handle(ea, ptr);
	}
	return r;
}

int FASTCALL ppc_direct_effective_memory_handle_code(uint32 addr, byte *&ptr)
{
	uint32 ea;
	int r;
	if (!((r = ppc_effective_to_physical(addr, PPC_MMU_READ | PPC_MMU_CODE, ea)))) {
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

int FASTCALL ppc_read_effective_code(uint32 addr, uint32 &result)
{
	if (addr & 3) {
		// EXC..bla
		return PPC_MMU_FATAL;
	}
	uint32 p;
	int r;
	if (!((r=ppc_effective_to_physical(addr, PPC_MMU_READ | PPC_MMU_CODE, p)))) {
		return ppc_read_physical_word(p, result);
	}
	return r;
}

int FASTCALL ppc_read_effective_qword(uint32 addr, Vector_t &result)
{
	uint32 p;
	int r;

	addr &= ~0x0f;

	if (!(r = ppc_effective_to_physical(addr, PPC_MMU_READ, p))) {
		return ppc_read_physical_qword(p, result);
	}

	return r;
}

int FASTCALL ppc_read_effective_dword(uint32 addr, uint64 &result)
{
	uint32 p;
	int r;
	if (!(r = ppc_effective_to_physical(addr, PPC_MMU_READ, p))) {
		if (EA_Offset(addr) > 4088) {
			// read overlaps two pages.. tricky
			byte *r1, *r2;
			byte b[14];
			ppc_effective_to_physical((addr & ~0xfff)+4089, PPC_MMU_READ, p);
			if ((r = ppc_direct_physical_memory_handle(p, r1))) return r;
			if ((r = ppc_effective_to_physical((addr & ~0xfff)+4096, PPC_MMU_READ, p))) return r;
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

int FASTCALL ppc_read_effective_word(uint32 addr, uint32 &result)
{
	uint32 p;
	int r;
	if (!(r = ppc_effective_to_physical(addr, PPC_MMU_READ, p))) {
		if (EA_Offset(addr) > 4092) {
			// read overlaps two pages.. tricky
			byte *r1, *r2;
			byte b[6];
			ppc_effective_to_physical((addr & ~0xfff)+4093, PPC_MMU_READ, p);
			if ((r = ppc_direct_physical_memory_handle(p, r1))) return r;
			if ((r = ppc_effective_to_physical((addr & ~0xfff)+4096, PPC_MMU_READ, p))) return r;
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

int FASTCALL ppc_read_effective_half(uint32 addr, uint16 &result)
{
	uint32 p;
	int r;
	if (!((r = ppc_effective_to_physical(addr, PPC_MMU_READ, p)))) {
		if (EA_Offset(addr) > 4094) {
			// read overlaps two pages.. tricky
			byte b1, b2;
			ppc_effective_to_physical((addr & ~0xfff)+4095, PPC_MMU_READ, p);
			if ((r = ppc_read_physical_byte(p, b1))) return r;
			if ((r = ppc_effective_to_physical((addr & ~0xfff)+4096, PPC_MMU_READ, p))) return r;
			if ((r = ppc_read_physical_byte(p, b2))) return r;
			result = (b1<<8)|b2;
			return PPC_MMU_OK;
		} else {
			return ppc_read_physical_half(p, result);
		}
	}
	return r;
}

int FASTCALL ppc_read_effective_byte(uint32 addr, uint8 &result)
{
	uint32 p;
	int r;
	if (!((r = ppc_effective_to_physical(addr, PPC_MMU_READ, p)))) {
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

int FASTCALL ppc_write_effective_qword(uint32 addr, Vector_t data)
{
	uint32 p;
	int r;

	addr &= ~0x0f;

	if (!((r=ppc_effective_to_physical(addr, PPC_MMU_WRITE, p)))) {
		return ppc_write_physical_qword(p, data);
	}
	return r;
}

int FASTCALL ppc_write_effective_dword(uint32 addr, uint64 data)
{
	uint32 p;
	int r;
	if (!((r=ppc_effective_to_physical(addr, PPC_MMU_WRITE, p)))) {
		if (EA_Offset(addr) > 4088) {
			// write overlaps two pages.. tricky
			byte *r1, *r2;
			byte b[14];
			ppc_effective_to_physical((addr & ~0xfff)+4089, PPC_MMU_WRITE, p);
			if ((r = ppc_direct_physical_memory_handle(p, r1))) return r;
			if ((r = ppc_effective_to_physical((addr & ~0xfff)+4096, PPC_MMU_WRITE, p))) return r;
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

int FASTCALL ppc_write_effective_word(uint32 addr, uint32 data)
{
	uint32 p;
	int r;
	if (!((r=ppc_effective_to_physical(addr, PPC_MMU_WRITE, p)))) {
		if (EA_Offset(addr) > 4092) {
			// write overlaps two pages.. tricky
			byte *r1, *r2;
			byte b[6];
			ppc_effective_to_physical((addr & ~0xfff)+4093, PPC_MMU_WRITE, p);
			if ((r = ppc_direct_physical_memory_handle(p, r1))) return r;
			if ((r = ppc_effective_to_physical((addr & ~0xfff)+4096, PPC_MMU_WRITE, p))) return r;
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

int FASTCALL ppc_write_effective_half(uint32 addr, uint16 data)
{
	uint32 p;
	int r;
	if (!((r=ppc_effective_to_physical(addr, PPC_MMU_WRITE, p)))) {
		if (EA_Offset(addr) > 4094) {
			// write overlaps two pages.. tricky
			ppc_effective_to_physical((addr & ~0xfff)+4095, PPC_MMU_WRITE, p);
			if ((r = ppc_write_physical_byte(p, data>>8))) return r;
			if ((r = ppc_effective_to_physical((addr & ~0xfff)+4096, PPC_MMU_WRITE, p))) return r;
			if ((r = ppc_write_physical_byte(p, data))) return r;
			return PPC_MMU_OK;
		} else {
			return ppc_write_physical_half(p, data);
		}
	}
	return r;
}

int FASTCALL ppc_write_effective_byte(uint32 addr, uint8 data)
{
	uint32 p;
	int r;
	if (!((r=ppc_effective_to_physical(addr, PPC_MMU_WRITE, p)))) {
		return ppc_write_physical_byte(p, data);
	}
	return r;
}

bool ppc_init_physical_memory(uint size)
{
	if (size < 64*1024*1024) {
		PPC_MMU_ERR("Main memory size must >= 64MB!\n");
	}
	gMemory = (byte*)malloc(size+16);
	if ((uint32)gMemory & 0x0f) {
		gMemory += 16 - ((uint32)gMemory & 0x0f);
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
bool ppc_prom_set_sdr1(uint32 newval, bool quiesce)
{
	return ppc_mmu_set_sdr1(newval, quiesce);
}

bool ppc_prom_effective_to_physical(uint32 &result, uint32 ea)
{
	return ppc_effective_to_physical(ea, PPC_MMU_READ|PPC_MMU_SV|PPC_MMU_NO_EXC, result) == PPC_MMU_OK;
}

bool ppc_prom_page_create(uint32 ea, uint32 pa)
{
	uint32 sr = gCPU.sr[EA_SR(ea)];
	uint32 page_index = EA_PageIndex(ea);  // 16 bit
	uint32 VSID = SR_VSID(sr);	     // 24 bit
	uint32 api = EA_API(ea);	       //  6 bit (part of page_index)
	uint32 hash1 = (VSID ^ page_index);
	uint32 pte, pte2;
	uint32 h = 0;
	for (int j=0; j<2; j++) {
		uint32 pteg_addr = ((hash1 & gCPU.pagetable_hashmask)<<6) | gCPU.pagetable_base;
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
 *	puts the sum of cr1 and cr2 into EAX 
 *	(in the most clever way)
 */
static void getEAXRsum(PPC_Register cr1, PPC_Register cr2)
{
	NativeReg r1 = jitcGetClientRegisterMapping(cr1);
	NativeReg r2 = jitcGetClientRegisterMapping(cr2);
	if (r1 == EAX) {
		/* intentional left empty */
	} else if (r2 == EAX) {
		if (r1 == REG_NO) {
			asmALU32(X86_ADD, EAX, (byte*)&gCPU+cr1);
		} else {
			asmALU32(X86_ADD, EAX, r1);
		}
		return;
	} else {
		/*
		 *	We load cr1 into EAX but have to clobber it since
		 *	we're going to modify EAX.
		 */
		jitcGetClientRegister(cr1, NATIVE_REG | EAX);
	}
	jitcClobberRegister(NATIVE_REG | EAX);
	r2 = jitcGetClientRegisterMapping(cr2);
	if (r2 == REG_NO) {
		asmALU32(X86_ADD, EAX, (byte*)&gCPU+cr2);
	} else {
		asmALU32(X86_ADD, EAX, r2);
	}
}

static void getEAX_0_Rsum(PPC_Register cr1, PPC_Register cr2)
{
	if (cr1 == PPC_GPR(0)) {
		jitcGetClientRegister(cr2, NATIVE_REG | EAX);
	} else {
		getEAXRsum(cr1, cr2);
	}
}

static void getEAXRsumAndEDX(PPC_Register cr1, PPC_Register cr2, PPC_Register cr3)
{
	NativeReg r1 = jitcGetClientRegisterMapping(cr1);
	NativeReg r2 = jitcGetClientRegisterMapping(cr2);
	if (r1 == EAX) {
		jitcTouchRegister(EAX);
		jitcClobberRegister(NATIVE_REG | EDX);
		if (cr1 == cr3) {
			asmALU32(X86_MOV, EDX, EAX);
		} else {
			jitcGetClientRegister(cr3, NATIVE_REG | EDX);
		}
		r2 = jitcGetClientRegisterMapping(cr2);
		if (r2 == REG_NO) {
			asmALU32(X86_ADD, EAX, (byte*)&gCPU+cr2);
			return;
		} else {
			asmALU32(X86_ADD, EAX, r2);
			return;
		}
	} else if (r2 == EAX) {
		jitcTouchRegister(EAX);
		jitcClobberRegister(NATIVE_REG | EDX);
		if (cr2 == cr3) {
			asmALU32(X86_MOV, EDX, EAX);
		} else {
			jitcGetClientRegister(cr3, NATIVE_REG | EDX);
		}
		r1 = jitcGetClientRegisterMapping(cr1);
		if (r1 == REG_NO) {
			asmALU32(X86_ADD, EAX, (byte*)&gCPU+cr1);
			return;
		}
		asmALU32(X86_ADD, EAX, r1);
		return;
	} else {
		jitcGetClientRegister(cr1, NATIVE_REG | EAX);
		jitcClobberRegister(NATIVE_REG | EDX);
		if (cr1 == cr3) {
			asmALU32(X86_MOV, EDX, EAX);
		} else {
			jitcGetClientRegister(cr3, NATIVE_REG | EDX);
		}
	}
	// FIXME: what if mapping of cr3==EDX?
	r2 = jitcGetClientRegisterMapping(cr2);
	if (r2 == REG_NO) {
		asmALU32(X86_ADD, EAX, (byte*)&gCPU+cr2);
	} else {
		asmALU32(X86_ADD, EAX, r2);
	}
}

static void getEAX_0_RsumAndEDX(PPC_Register cr1, PPC_Register cr2, PPC_Register cr3)
{
	if (cr1 == PPC_GPR(0)) {
		if (jitcGetClientRegisterMapping(cr2) == EDX) jitcTouchRegister(EDX);
		jitcGetClientRegister(cr2, NATIVE_REG | EAX);
		if (cr2 == cr3) {
			asmALU32(X86_MOV, EDX, EAX);
		} else {
			jitcGetClientRegister(cr3, NATIVE_REG | EDX);
		}
	} else {
		getEAXRsumAndEDX(cr1, cr2, cr3);
	}
}

/* 
 *	puts the sum of cr1 and imm into EAX 
 *	(in the most clever way)
 */
static void getEAXIsum(PPC_Register cr1, uint32 imm)
{
	jitcGetClientRegister(cr1, NATIVE_REG | EAX);
	if (imm) {
		asmALU32(X86_ADD, EAX, imm);
	}
}

static void getEAX_0_Isum(PPC_Register cr1, uint32 imm)
{
	if (cr1 == PPC_GPR(0)) {
		asmALU32(X86_MOV, EAX, imm);
	} else {
		getEAXIsum(cr1, imm);
	}
}

static void getEAXIsumAndEDX(PPC_Register cr1, uint32 imm, PPC_Register cr2)
{
	if (jitcGetClientRegisterMapping(cr2) == EDX) jitcTouchRegister(EDX);
	jitcGetClientRegister(cr1, NATIVE_REG | EAX);
	if (cr1 == cr2) {
		asmALU32(X86_MOV, EDX, EAX);
	} else {
		jitcGetClientRegister(cr2, NATIVE_REG | EDX);
	}
	jitcClobberRegister(NATIVE_REG | EAX);
	if (imm) {
		asmALU32(X86_ADD, EAX, imm);
	}
}

static void getEAX_0_IsumAndEDX(PPC_Register cr1, uint32 imm, PPC_Register cr2)
{
	if (cr1 == PPC_GPR(0)) {
		jitcGetClientRegister(cr2, NATIVE_REG | EDX);
		jitcClobberRegister(NATIVE_REG | EAX);
		asmALU32(X86_MOV, EAX, imm);
	} else {
		getEAXIsumAndEDX(cr1, imm, cr2);
	}
}


void ppc_opc_gen_helper_l(PPC_Register cr1, uint32 imm)
{
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	getEAX_0_Isum(cr1, imm);
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_lu(PPC_Register cr1, uint32 imm)
{
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	getEAXIsum(cr1, imm);
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_lux(PPC_Register cr1, PPC_Register cr2)
{
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	getEAXRsum(cr1, cr2);
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_lx(PPC_Register cr1, PPC_Register cr2)
{
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	getEAX_0_Rsum(cr1, cr2);
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_st(PPC_Register cr1, uint32 imm, PPC_Register cr2)
{
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	getEAX_0_IsumAndEDX(cr1, imm, cr2);
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_stu(PPC_Register cr1, uint32 imm, PPC_Register cr2)
{
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	getEAXIsumAndEDX(cr1, imm, cr2);
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_stux(PPC_Register cr1, PPC_Register cr2, PPC_Register cr3)
{
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	getEAXRsumAndEDX(cr1, cr2, cr3);
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_stx(PPC_Register cr1, PPC_Register cr2, PPC_Register cr3)
{
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	getEAX_0_RsumAndEDX(cr1, cr2, cr3);
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
}
/*

void ppc_opc_gen_helper_l(PPC_Register cr1, uint32 imm)
{
	jitcClobberAll();
	if (cr1 == PPC_GPR(0)) {
		asmALU32(X86_MOV, EAX, imm);
	} else {
		asmALU32(X86_MOV, EAX, (byte*)&gCPU+cr1);
		asmALU32(X86_ADD, EAX, imm);
	}
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_lu(PPC_Register cr1, uint32 imm)
{
	jitcClobberAll();
	byte modrm[6];
	asmALURegMem(X86_MOV, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr1));
	asmALU32(X86_ADD, EAX, imm);
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_lux(PPC_Register cr1, PPC_Register cr2)
{
	jitcClobberAll();
	byte modrm[6];
	asmALURegMem(X86_MOV, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr1));
	asmALURegMem(X86_ADD, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr2));
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_lx(PPC_Register cr1, PPC_Register cr2)
{
	jitcClobberAll();
	byte modrm[6];
	if (cr1 == PPC_GPR(0)) {
		asmALURegMem(X86_MOV, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr2));
	} else {
		asmALURegMem(X86_MOV, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr1));
		asmALURegMem(X86_ADD, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr2));
	}
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_st(PPC_Register cr1, uint32 imm, PPC_Register cr2)
{
	jitcClobberAll();
	byte modrm[6];
	if (cr1 == PPC_GPR(0)) {
		asmALU32(X86_MOV, EAX, imm);
		asmALURegMem(X86_MOV, EDX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr2));
	} else {
		asmALURegMem(X86_MOV, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr1));
		asmALURegMem(X86_MOV, EDX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr2));
		asmALU32(X86_ADD, EAX, imm);
	}
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_stu(PPC_Register cr1, uint32 imm, PPC_Register cr2)
{
	jitcClobberAll();
	byte modrm[6];
	asmALURegMem(X86_MOV, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr1));
	asmALURegMem(X86_MOV, EDX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr2));
	asmALU32(X86_ADD, EAX, imm);
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_stux(PPC_Register cr1, PPC_Register cr2, PPC_Register cr3)
{
	jitcClobberAll();
	byte modrm[6];
	asmALURegMem(X86_MOV, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr1));
	asmALURegMem(X86_MOV, EDX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr3));
	asmALURegMem(X86_ADD, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr2));
	asmALU32(X86_MOV, ESI, gJITC.pc);
}

void ppc_opc_gen_helper_stx(PPC_Register cr1, PPC_Register cr2, PPC_Register cr3)
{
	jitcClobberAll();
	byte modrm[6];
	if (cr1 == PPC_GPR(0)) {
		asmALURegMem(X86_MOV, EDX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr3));
		asmALURegMem(X86_MOV, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr2));
	} else {
		asmALURegMem(X86_MOV, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr1));
		asmALURegMem(X86_MOV, EDX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr3));
		asmALURegMem(X86_ADD, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU+cr2));
	}
	asmALU32(X86_MOV, ESI, gJITC.pc);
}
*/

uint64 FASTCALL ppc_opc_single_to_double(uint32 r)
{
	ppc_single s;
	ppc_double d;
	uint64 ret;
	ppc_fpu_unpack_single(s, r);
	ppc_fpu_single_to_double(s, d);
	ppc_fpu_pack_double(d, ret);
	return ret;
}

uint32 FASTCALL ppc_opc_double_to_single(uint64 r)
{
	uint32 s;
	ppc_double d;
	ppc_fpu_unpack_double(d, r);
	ppc_fpu_pack_single(d, s);
	return s;
}


/*
 *	dcbz		Data Cache Clear to Zero
 *	.464
 */
void ppc_opc_dcbz()
{
	//PPC_L1_CACHE_LINE_SIZE
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	// assert rD=0
	uint32 a = (rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB];
	// BAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	ppc_write_effective_dword(a, 0)
	|| ppc_write_effective_dword(a+8, 0)
	|| ppc_write_effective_dword(a+16, 0)
	|| ppc_write_effective_dword(a+24, 0);
}
JITCFlow ppc_opc_gen_dcbz()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	getEAX_0_Rsum(PPC_GPR(rA), PPC_GPR(rB));
	jitcClobberRegister();
	asmALU32(X86_MOV, ECX, (uint32)0);
	asmALU32(X86_MOV, &gCPU.temp, EAX);
	asmALU32(X86_MOV, EDX, (uint32)0);
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	asmALU32(X86_MOV, ECX, (uint32)0);
	asmALU32(X86_MOV, EAX, &gCPU.temp);
	asmALU32(X86_MOV, EDX, (uint32)0);
	asmALU32(X86_ADD, EAX, 8);
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	asmALU32(X86_MOV, ECX, (uint32)0);
	asmALU32(X86_MOV, EAX, &gCPU.temp);
	asmALU32(X86_MOV, EDX, (uint32)0);
	asmALU32(X86_ADD, EAX, 16);
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	asmALU32(X86_MOV, ECX, (uint32)0);
	asmALU32(X86_MOV, EAX, &gCPU.temp);
	asmALU32(X86_MOV, EDX, (uint32)0);
	asmALU32(X86_ADD, EAX, 24);
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	return flowEndBlock;
}

/*
 *	lbz		Load Byte and Zero
 *	.521
 */
void ppc_opc_lbz()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	uint8 r;
	int ret = ppc_read_effective_byte((rA?gCPU.gpr[rA]:0)+imm, r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lbz()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_l(PPC_GPR(rA), imm);
	asmCALL((NativeAddress)ppc_read_effective_byte_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	return flowContinue;
}
/*
 *	lbzu		Load Byte and Zero with Update
 *	.522
 */
void ppc_opc_lbzu()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	// FIXME: check rA!=0 && rA!=rD
	uint8 r;
	int ret = ppc_read_effective_byte(gCPU.gpr[rA]+imm, r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += imm;
		gCPU.gpr[rD] = r;
	}	
}
JITCFlow ppc_opc_gen_lbzu()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_lu(PPC_GPR(rA), imm);
	asmCALL((NativeAddress)ppc_read_effective_byte_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	if (imm) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALU32(X86_ADD, a, imm);
	}
	return flowContinue;
}
/*
 *	lbzux		Load Byte and Zero with Update Indexed
 *	.523
 */
void ppc_opc_lbzux()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	// FIXME: check rA!=0 && rA!=rD
	uint8 r;
	int ret = ppc_read_effective_byte(gCPU.gpr[rA]+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += gCPU.gpr[rB];
		gCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lbzux()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lux(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_byte_asm);
	if (rD == rB) {
		// don't ask...
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | EAX);
		asmALU32(X86_ADD, a, &gCPU.gpr[rB]);
		jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	} else {
		jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		asmALU32(X86_ADD, a, b);
	}
	return flowContinue;
}
/*
 *	lbzx		Load Byte and Zero Indexed
 *	.524
 */
void ppc_opc_lbzx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	uint8 r;
	int ret = ppc_read_effective_byte((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lbzx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_byte_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	return flowContinue;
}
/*
 *	lfd		Load Floating-Point Double
 *	.530
 */
void ppc_opc_lfd()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, frD, rA, imm);
	uint64 r;
	int ret = ppc_read_effective_dword((rA?gCPU.gpr[rA]:0)+imm, r);
	if (ret == PPC_MMU_OK) {
		gCPU.fpr[frD] = r;
	}	
}
JITCFlow ppc_opc_gen_lfd()
{
	ppc_opc_gen_check_fpu();
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, frD, rA, imm);
	jitcFloatRegisterClobberAll();
	ppc_opc_gen_helper_l(PPC_GPR(rA), imm);
	asmCALL((NativeAddress)ppc_read_effective_dword_asm);
	jitcMapClientRegisterDirty(PPC_FPR_U(frD), NATIVE_REG | ECX);
	jitcMapClientRegisterDirty(PPC_FPR_L(frD), NATIVE_REG | EDX);
	return flowContinue;
}
/*
 *	lfdu		Load Floating-Point Double with Update
 *	.531
 */
void ppc_opc_lfdu()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, frD, rA, imm);
	// FIXME: check rA!=0
	uint64 r;
	int ret = ppc_read_effective_dword(gCPU.gpr[rA]+imm, r);
	if (ret == PPC_MMU_OK) {
		gCPU.fpr[frD] = r;
		gCPU.gpr[rA] += imm;
	}	
}
JITCFlow ppc_opc_gen_lfdu()
{
	ppc_opc_gen_check_fpu();
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, frD, rA, imm);
	jitcFloatRegisterClobberAll();
	ppc_opc_gen_helper_lu(PPC_GPR(rA), imm);
	asmCALL((NativeAddress)ppc_read_effective_dword_asm);
	jitcMapClientRegisterDirty(PPC_FPR_U(frD), NATIVE_REG | ECX);
	jitcMapClientRegisterDirty(PPC_FPR_L(frD), NATIVE_REG | EDX);
	if (imm) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALU32(X86_ADD, a, imm);
	}
	return flowContinue;
}
/*
 *	lfdux		Load Floating-Point Double with Update Indexed
 *	.532
 */
void ppc_opc_lfdux()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, frD, rA, rB);
	// FIXME: check rA!=0
	uint64 r;
	int ret = ppc_read_effective_dword(gCPU.gpr[rA]+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += gCPU.gpr[rB];
		gCPU.fpr[frD] = r;
	}	
}
JITCFlow ppc_opc_gen_lfdux()
{
	ppc_opc_gen_check_fpu();
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, frD, rA, rB);
	ppc_opc_gen_helper_lux(PPC_GPR(rA), PPC_GPR(rB));
	jitcFloatRegisterClobberAll();
	asmCALL((NativeAddress)ppc_read_effective_dword_asm);
	jitcMapClientRegisterDirty(PPC_FPR_U(frD), NATIVE_REG | ECX);
	jitcMapClientRegisterDirty(PPC_FPR_L(frD), NATIVE_REG | EDX);
	NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	lfdx		Load Floating-Point Double Indexed
 *	.533
 */
void ppc_opc_lfdx()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, frD, rA, rB);
	uint64 r;
	int ret = ppc_read_effective_dword((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.fpr[frD] = r;
	}	
}
JITCFlow ppc_opc_gen_lfdx()
{
	ppc_opc_gen_check_fpu();
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, frD, rA, rB);
	jitcFloatRegisterClobberAll();
	ppc_opc_gen_helper_lx(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_dword_asm);
	jitcMapClientRegisterDirty(PPC_FPR_U(frD), NATIVE_REG | ECX);
	jitcMapClientRegisterDirty(PPC_FPR_L(frD), NATIVE_REG | EDX);
	return flowContinue;
}
/*
 *	lfs		Load Floating-Point Single
 *	.534
 */
void ppc_opc_lfs()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, frD, rA, imm);
	uint32 r;
	int ret = ppc_read_effective_word((rA?gCPU.gpr[rA]:0)+imm, r);
	if (ret == PPC_MMU_OK) {
		ppc_single s;
		ppc_double d;
		ppc_fpu_unpack_single(s, r);
		ppc_fpu_single_to_double(s, d);
		ppc_fpu_pack_double(d, gCPU.fpr[frD]);
	}	
}
JITCFlow ppc_opc_gen_lfs()
{
	ppc_opc_gen_check_fpu();
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, frD, rA, imm);
	jitcFloatRegisterClobberAll();
	ppc_opc_gen_helper_l(PPC_GPR(rA), imm);
	asmCALL((NativeAddress)ppc_read_effective_word_asm);
	asmALU32(X86_MOV, EAX, EDX);
	asmCALL((NativeAddress)ppc_opc_single_to_double);
	jitcMapClientRegisterDirty(PPC_FPR_U(frD), NATIVE_REG | EDX);
	jitcMapClientRegisterDirty(PPC_FPR_L(frD), NATIVE_REG | EAX);
	return flowContinue;
}
/*
 *	lfsu		Load Floating-Point Single with Update
 *	.535
 */
void ppc_opc_lfsu()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, frD, rA, imm);
	// FIXME: check rA!=0
	uint32 r;
	int ret = ppc_read_effective_word(gCPU.gpr[rA]+imm, r);
	if (ret == PPC_MMU_OK) {
		ppc_single s;
		ppc_double d;
		ppc_fpu_unpack_single(s, r);
		ppc_fpu_single_to_double(s, d);
		ppc_fpu_pack_double(d, gCPU.fpr[frD]);
		gCPU.gpr[rA] += imm;
	}	
}
JITCFlow ppc_opc_gen_lfsu()
{
	ppc_opc_gen_check_fpu();
	int rA, frD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, frD, rA, imm);
	jitcFloatRegisterClobberAll();
	ppc_opc_gen_helper_lu(PPC_GPR(rA), imm);
	asmCALL((NativeAddress)ppc_read_effective_word_asm);
	asmALU32(X86_MOV, EAX, EDX);
	asmCALL((NativeAddress)ppc_opc_single_to_double);
	jitcMapClientRegisterDirty(PPC_FPR_U(frD), NATIVE_REG | EDX);
	jitcMapClientRegisterDirty(PPC_FPR_L(frD), NATIVE_REG | EAX);
	if (imm) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALU32(X86_ADD, a, imm);
	}
	return flowContinue;
}
/*
 *	lfsux		Load Floating-Point Single with Update Indexed
 *	.536
 */
void ppc_opc_lfsux()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, frD, rA, rB);
	// FIXME: check rA!=0
	uint32 r;
	int ret = ppc_read_effective_word(gCPU.gpr[rA]+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += gCPU.gpr[rB];
		ppc_single s;
		ppc_double d;
		ppc_fpu_unpack_single(s, r);
		ppc_fpu_single_to_double(s, d);
		ppc_fpu_pack_double(d, gCPU.fpr[frD]);
	}	
}
JITCFlow ppc_opc_gen_lfsux()
{
	ppc_opc_gen_check_fpu();
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, frD, rA, rB);
	jitcFloatRegisterClobberAll();
	ppc_opc_gen_helper_lux(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_word_asm);
	asmALU32(X86_MOV, EAX, EDX);
	asmCALL((NativeAddress)ppc_opc_single_to_double);
	jitcMapClientRegisterDirty(PPC_FPR_U(frD), NATIVE_REG | EDX);
	jitcMapClientRegisterDirty(PPC_FPR_L(frD), NATIVE_REG | EAX);
	NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	lfsx		Load Floating-Point Single Indexed
 *	.537
 */
void ppc_opc_lfsx()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, frD, rA, rB);
	uint32 r;
	int ret = ppc_read_effective_word((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		ppc_single s;
		ppc_double d;
		ppc_fpu_unpack_single(s, r);
		ppc_fpu_single_to_double(s, d);
		ppc_fpu_pack_double(d, gCPU.fpr[frD]);
	}	
}
JITCFlow ppc_opc_gen_lfsx()
{
	ppc_opc_gen_check_fpu();
	int rA, frD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, frD, rA, rB);
	jitcFloatRegisterClobberAll();
	ppc_opc_gen_helper_lx(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_word_asm);
	asmALU32(X86_MOV, EAX, EDX);
	asmCALL((NativeAddress)ppc_opc_single_to_double);
	jitcMapClientRegisterDirty(PPC_FPR_U(frD), NATIVE_REG | EDX);
	jitcMapClientRegisterDirty(PPC_FPR_L(frD), NATIVE_REG | EAX);
	return flowContinue;
}
/*
 *	lha		Load Half Word Algebraic
 *	.538
 */
void ppc_opc_lha()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	uint16 r;
	int ret = ppc_read_effective_half((rA?gCPU.gpr[rA]:0)+imm, r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rD] = (r&0x8000)?(r|0xffff0000):r;
	}
}
JITCFlow ppc_opc_gen_lha()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_l(PPC_GPR(rA), imm);
	asmCALL((NativeAddress)ppc_read_effective_half_s_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	return flowContinue;
}
/*
 *	lhau		Load Half Word Algebraic with Update
 *	.539
 */
void ppc_opc_lhau()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	uint16 r;
	// FIXME: rA != 0
	int ret = ppc_read_effective_half(gCPU.gpr[rA]+imm, r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += imm;
		gCPU.gpr[rD] = (r&0x8000)?(r|0xffff0000):r;
	}
}
JITCFlow ppc_opc_gen_lhau()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_lu(PPC_GPR(rA), imm);
	asmCALL((NativeAddress)ppc_read_effective_half_s_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	if (imm) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALU32(X86_ADD, a, imm);
	}
	return flowContinue;
}
/*
 *	lhaux		Load Half Word Algebraic with Update Indexed
 *	.540
 */
void ppc_opc_lhaux()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	uint16 r;
	// FIXME: rA != 0
	int ret = ppc_read_effective_half(gCPU.gpr[rA]+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += gCPU.gpr[rB];
		gCPU.gpr[rD] = (r&0x8000)?(r|0xffff0000):r;
	}
}
JITCFlow ppc_opc_gen_lhaux()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lux(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_half_s_asm);
	if (rD == rB) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | EAX);
		asmALU32(X86_ADD, a, &gCPU.gpr[rB]);
		jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	} else {
		jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		asmALU32(X86_ADD, a, b);
	}
	return flowContinue;
}
/*
 *	lhax		Load Half Word Algebraic Indexed
 *	.541
 */
void ppc_opc_lhax()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	uint16 r;
	// FIXME: rA != 0
	int ret = ppc_read_effective_half((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rD] = (r&0x8000) ? (r|0xffff0000):r;
	}
}
JITCFlow ppc_opc_gen_lhax()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_half_s_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	return flowContinue;
}
/*
 *	lhbrx		Load Half Word Byte-Reverse Indexed
 *	.542
 */
void ppc_opc_lhbrx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	uint16 r;
	int ret = ppc_read_effective_half((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rD] = ppc_bswap_half(r);
	}
}
JITCFlow ppc_opc_gen_lhbrx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_half_z_asm);
	asmALU8(X86_XCHG, DL, DH);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	return flowContinue;
}
/*
 *	lhz		Load Half Word and Zero
 *	.543
 */
void ppc_opc_lhz()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	uint16 r;
	int ret = ppc_read_effective_half((rA?gCPU.gpr[rA]:0)+imm, r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lhz()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_l(PPC_GPR(rA), imm);
	asmCALL((NativeAddress)ppc_read_effective_half_z_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	return flowContinue;
}
/*
 *	lhzu		Load Half Word and Zero with Update
 *	.544
 */
void ppc_opc_lhzu()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	uint16 r;
	// FIXME: rA!=0
	int ret = ppc_read_effective_half(gCPU.gpr[rA]+imm, r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rD] = r;
		gCPU.gpr[rA] += imm;
	}
}
JITCFlow ppc_opc_gen_lhzu()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_lu(PPC_GPR(rA), imm);
	asmCALL((NativeAddress)ppc_read_effective_half_z_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	if (imm) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALU32(X86_ADD, a, imm);
	}
	return flowContinue;
}
/*
 *	lhzux		Load Half Word and Zero with Update Indexed
 *	.545
 */
void ppc_opc_lhzux()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	uint16 r;
	// FIXME: rA != 0
	int ret = ppc_read_effective_half(gCPU.gpr[rA]+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += gCPU.gpr[rB];
		gCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lhzux()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lux(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_half_z_asm);
	if (rD == rB) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | EAX);
		asmALU32(X86_ADD, a, &gCPU.gpr[rB]);
		jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	} else {
		jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		asmALU32(X86_ADD, a, b);
	}
	return flowContinue;
}
/*
 *	lhzx		Load Half Word and Zero Indexed
 *	.546
 */
void ppc_opc_lhzx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	uint16 r;
	int ret = ppc_read_effective_half((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lhzx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_half_z_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	return flowContinue;
}
/*
 *	lmw		Load Multiple Word
 *	.547
 */
void ppc_opc_lmw()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	uint32 ea = (rA ? gCPU.gpr[rA] : 0) + imm;
	while (rD <= 31) {
		if (ppc_read_effective_word(ea, gCPU.gpr[rD])) {
			return;
		}
		rD++;
		ea += 4;
	}
}
JITCFlow ppc_opc_gen_lmw()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	while (rD <= 30) {
		ppc_opc_gen_helper_l(PPC_GPR(rA), imm);
		asmCALL((NativeAddress)ppc_read_effective_dword_asm);
		jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | ECX);
		jitcMapClientRegisterDirty(PPC_GPR(rD+1), NATIVE_REG | EDX);
		rD += 2;
		imm += 8;
         }
	if (rD == 31) {
		ppc_opc_gen_helper_l(PPC_GPR(rA), imm);
		asmCALL((NativeAddress)ppc_read_effective_word_asm);
		jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	}
	return flowContinue;
}
/*
 *	lswi		Load String Word Immediate
 *	.548
 */
void ppc_opc_lswi()
{
	int rA, rD, NB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, NB);
	if (NB==0) NB=32;
	uint32 ea = rA ? gCPU.gpr[rA] : 0;
	uint32 r = 0;
	int i = 4;
	uint8 v;
	while (NB > 0) {
		if (!i) {
			i = 4;
			gCPU.gpr[rD] = r;
			rD++;
			rD%=32;
			r = 0;
		}
		if (ppc_read_effective_byte(ea, v)) {
			return;
		}
		r<<=8;
		r|=v;
		ea++;
		i--;
		NB--;
	}
	while (i) { r<<=8; i--; }
	gCPU.gpr[rD] = r;
}
JITCFlow ppc_opc_gen_lswi()
{
	int rA, rD, NB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, NB);
	if (NB==0) NB=32;
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	if (rA) {
		jitcGetClientRegister(PPC_GPR(rA), NATIVE_REG | EAX);
	} else {
		asmALU32(X86_MOV, EAX, (uint32)0);
	}
	asmALU32(X86_MOV, ECX, NB);
	asmALU32(X86_MOV, EBX, rD);
	asmALU32(X86_MOV, ESI, gJITC.pc);
	jitcClobberAll();
	asmCALL((NativeAddress)ppc_opc_lswi_asm);
	return flowEndBlock;
}
/*
 *	lswx		Load String Word Indexed
 *	.550
 */
void ppc_opc_lswx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	int NB = XER_n(gCPU.xer);
	uint32 ea = gCPU.gpr[rB] + (rA ? gCPU.gpr[rA] : 0);

	uint32 r = 0;
	int i = 4;
	uint8 v;
	while (NB > 0) {
		if (!i) {
			i = 4;
			gCPU.gpr[rD] = r;
			rD++;
			rD%=32;
			r = 0;
		}
		if (ppc_read_effective_byte(ea, v)) {
			return;
		}
		r<<=8;
		r|=v;
		ea++;
		i--;
		NB--;
	}
	while (i) { r<<=8; i--; }
	gCPU.gpr[rD] = r;
}
JITCFlow ppc_opc_gen_lswx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	jitcGetClientRegister(PPC_XER, NATIVE_REG | ECX);
	if (rA) {
		jitcGetClientRegister(PPC_GPR(rA), NATIVE_REG | EAX);
		asmALU32(X86_ADD, EAX, &gCPU.gpr[rB]);
	} else {
		jitcGetClientRegister(PPC_GPR(rB), NATIVE_REG | EAX);
	}
	asmALU32(X86_AND, ECX, 0x7f);
	jitcClobberAll();
	NativeAddress fixup = asmJxxFixup(X86_Z);
	asmALU32(X86_MOV, EBX, rD);
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_opc_lswi_asm);
	asmResolveFixup(fixup, asmHERE());
	return flowEndBlock;
}
/*
 *	lwarx		Load Word and Reserve Indexed
 *	.553
 */
void ppc_opc_lwarx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	uint32 r;
	int ret = ppc_read_effective_word((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rD] = r;
		gCPU.reserve = r;
		gCPU.have_reservation = 1;
	}
}
JITCFlow ppc_opc_gen_lwarx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_word_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	asmALU32(X86_MOV, &gCPU.reserve, EDX);
	asmALU8(X86_MOV, &gCPU.have_reservation, 1);
	return flowContinue;
}
/*
 *	lwbrx		Load Word Byte-Reverse Indexed
 *	.556
 */
void ppc_opc_lwbrx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	uint32 r;
	int ret = ppc_read_effective_word((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rD] = ppc_bswap_word(r);
	}
}
JITCFlow ppc_opc_gen_lwbrx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_word_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	asmBSWAP32(EDX);
	return flowContinue;
}
/*
 *	lwz		Load Word and Zero
 *	.557
 */
void ppc_opc_lwz()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	uint32 r;
	int ret = ppc_read_effective_word((rA?gCPU.gpr[rA]:0)+imm, r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rD] = r;
	}	
}
JITCFlow ppc_opc_gen_lwz()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_l(PPC_GPR(rA), imm);
	asmCALL((NativeAddress)ppc_read_effective_word_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	return flowContinue;
}
/*
 *	lbzu		Load Word and Zero with Update
 *	.558
 */
void ppc_opc_lwzu()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	// FIXME: check rA!=0 && rA!=rD
	uint32 r;
	int ret = ppc_read_effective_word(gCPU.gpr[rA]+imm, r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += imm;
		gCPU.gpr[rD] = r;
	}	
}
JITCFlow ppc_opc_gen_lwzu()
{
	int rA, rD;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	ppc_opc_gen_helper_lu(PPC_GPR(rA), imm);
	asmCALL((NativeAddress)ppc_read_effective_word_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	if (imm) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALU32(X86_ADD, a, imm);
	}
	return flowContinue;
}
/*
 *	lwzux		Load Word and Zero with Update Indexed
 *	.559
 */
void ppc_opc_lwzux()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	// FIXME: check rA!=0 && rA!=rD
	uint32 r;
	int ret = ppc_read_effective_word(gCPU.gpr[rA]+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += gCPU.gpr[rB];
		gCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lwzux()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lux(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_word_asm);
	if (rD == rB) {
		// don't ask...
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | EAX);
		asmALU32(X86_ADD, a, &gCPU.gpr[rB]);
		jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	} else {
		jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		asmALU32(X86_ADD, a, b);
	}
	return flowContinue;
}
/*
 *	lwzx		Load Word and Zero Indexed
 *	.560
 */
void ppc_opc_lwzx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	uint32 r;
	int ret = ppc_read_effective_word((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], r);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rD] = r;
	}
}
JITCFlow ppc_opc_gen_lwzx()
{
	int rA, rD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	ppc_opc_gen_helper_lx(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_word_asm);
	jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
	return flowContinue;
}

static inline NativeReg FASTCALL ppc_opc_gen_helper_lvx_hint(int rA, int rB, uint hint)
{
	NativeReg ret = REG_NO;

	NativeReg reg1 = jitcGetClientRegisterMapping(PPC_GPR(rA));
	NativeReg reg2 = jitcGetClientRegisterMapping(PPC_GPR(rB));

	if (reg1 == hint) {
		jitcClobberCarryAndFlags();
		jitcClobberRegister(NATIVE_REG | reg1);
		ret = reg1;
		jitcTouchRegister(ret);

		if (reg2 != REG_NO) {
			asmALU32(X86_ADD, ret, reg2);
		} else {
			asmALU32(X86_ADD, ret, &gCPU.gpr[rB]);
		}
	} else if (reg2 == hint) {
		jitcClobberCarryAndFlags();
		jitcClobberRegister(NATIVE_REG | reg2);
		ret = reg2;
		jitcTouchRegister(ret);

		if (reg1 != REG_NO) {
			asmALU32(X86_ADD, ret, reg1);
		} else {
			asmALU32(X86_ADD, ret, &gCPU.gpr[rA]);
		}
	} else if ((reg1 != REG_NO) && (reg2 != REG_NO)) {
		/* If both are in register space, and not the hint we're best
		 *   off clobbering the hint, then using leal as a 3-operand
		 *   ADD.
		 * This gives us the performance of an ADD, and removes the
		 *   need for a later MOV into the hint.
		 */
		jitcClobberRegister(NATIVE_REG | hint);
		ret = (NativeReg)hint;
		jitcTouchRegister(ret);

		asmALU32(X86_LEA, ret, reg1, 1, reg2, 0);
	}

	return ret;
}

static inline NativeReg FASTCALL ppc_opc_gen_helper_lvx(int rA, int rB, int hint=0)
{
	NativeReg ret = REG_NO;

	if (!rA) {
		ret = jitcGetClientRegisterMapping(PPC_GPR(rB));

		if (ret == REG_NO) {
			ret = jitcGetClientRegister(PPC_GPR(rB), hint);
		}

		jitcClobberRegister(NATIVE_REG | ret);
		jitcTouchRegister(ret);

		return ret;
	}

	if (hint & NATIVE_REG) {
		ret = ppc_opc_gen_helper_lvx_hint(rA, rB, hint & 0x0f);

		if (ret != REG_NO)
			return ret;
	}

	jitcClobberCarryAndFlags();

	NativeReg reg1 = jitcGetClientRegisterMapping(PPC_GPR(rA));
	NativeReg reg2 = jitcGetClientRegisterMapping(PPC_GPR(rB));

	if (reg2 == REG_NO) {
		ret = jitcGetClientRegister(PPC_GPR(rA));
		jitcClobberRegister(NATIVE_REG | ret);

		asmALU32(X86_ADD, ret, &gCPU.gpr[rB]);
	} else {
		jitcClobberRegister(NATIVE_REG | reg2);
		ret = reg2;

		if (reg1 != REG_NO) {
			asmALU32(X86_ADD, ret, reg1);
		} else {
			asmALU32(X86_ADD, ret, &gCPU.gpr[rA]);
		}
	}

	jitcTouchRegister(ret);
	return ret;
}

/*      lvx	     Load Vector Indexed
 *      v.127
 */
void ppc_opc_lvx()
{
#ifndef __VEC_EXC_OFF__
	if ((gCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, rA, rB);
	Vector_t r;

	int ea = ((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB]);

	int ret = ppc_read_effective_qword(ea, r);
	if (ret == PPC_MMU_OK) {
		gCPU.vr[vrD] = r;
	}
}
JITCFlow ppc_opc_gen_lvx()
{
	ppc_opc_gen_check_vec();
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, rA, rB);
	jitcDropClientVectorRegister(vrD);
	jitcAssertFlushedVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB, NATIVE_REG | EAX);
#if 1
	jitcClobberAll();
	if (regA != EAX) {
		//printf("*** hint miss r%u != r0\n", regA);
		asmALU32(X86_MOV, EAX, regA);
	}
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmALU32(X86_MOV, EDX, (uint32)&(gCPU.vr[vrD]));

	if (0 && gJITC.hostCPUCaps.sse) {
		asmCALL((NativeAddress)ppc_read_effective_qword_sse_asm);
		gJITC.nativeVectorReg = vrD;
	} else {
		asmCALL((NativeAddress)ppc_read_effective_qword_asm);
	}
#else
	asmALU32(X86_AND, regA, ~0x0f);
	asmMOVDMemReg((uint32)&gCPU.vtemp, regA);

	jitcClobberAll();
	if (regA != EAX) {
		//printf("*** hint miss r%u != r0\n", regA);
		asmALU32(X86_MOV, EAX, regA);
	}
	asmALU32(X86_MOV, ESI, gJITC.pc);

	asmCALL((NativeAddress)ppc_read_effective_dword_asm);
	asmMOVDMemReg((uint32)&(gCPU.vr[vrD].w[3]), ECX);
	asmMOVDMemReg((uint32)&(gCPU.vr[vrD].w[2]), EDX);

	asmMOVRegDMem(EAX, (uint32)&gCPU.vtemp);
	asmALU32(X86_OR, EAX, 8);

	asmCALL((NativeAddress)ppc_read_effective_dword_asm);
	asmMOVDMemReg((uint32)&(gCPU.vr[vrD].w[1]), ECX);
	asmMOVDMemReg((uint32)&(gCPU.vr[vrD].w[0]), EDX);
#endif

	return flowContinue;
}


/*      lvxl	    Load Vector Index LRU
 *      v.128
 */
void ppc_opc_lvxl()
{
	ppc_opc_lvx();
	/* This instruction should hint to the cache that the value won't be
	 *   needed again in memory anytime soon.  We don't emulate the cache,
	 *   so this is effectively exactly the same as lvx.
	 */
}
JITCFlow ppc_opc_gen_lvxl()
{
	return ppc_opc_gen_lvx();
}


/*      lvebx	   Load Vector Element Byte Indexed
 *      v.119
 */
void ppc_opc_lvebx()
{
#ifndef __VEC_EXC_OFF__
	if ((gCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, rA, rB);
	uint32 ea;
	uint8 r;
	ea = (rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB];
	int ret = ppc_read_effective_byte(ea, r);
	if (ret == PPC_MMU_OK) {
		VECT_B(gCPU.vr[vrD], ea & 0x0f) = r;
	}
}
JITCFlow ppc_opc_gen_lvebx()
{
	ppc_opc_gen_check_vec();
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, rA, rB);
	jitcDropClientVectorRegister(vrD);
	jitcAssertFlushedVectorRegister(vrD);

	if (vrD == gJITC.nativeVectorReg) {
		gJITC.nativeVectorReg = VECTREG_NO;
	}

	jitcClobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB, NATIVE_REG | EAX);
	asmALU32(X86_MOV, &gCPU.vtemp, regA);

	jitcClobberAll();
	if (regA != EAX) asmALU32(X86_MOV, EAX, regA);
	asmALU32(X86_MOV, ESI, gJITC.pc);

	asmCALL((NativeAddress)ppc_read_effective_byte_asm);
	asmALU32(X86_MOV, EAX, &gCPU.vtemp);
	asmALU32(X86_AND, EAX, 0x0f);
	asmALU32(X86_NOT, EAX);

	asmALU8(X86_MOV, EAX, (uint32)&gCPU.vr[vrD]+16, DL);

	return flowContinue;
}

/*      lvehx	   Load Vector Element Half Word Indexed
 *      v.121
 */
void ppc_opc_lvehx()
{
#ifndef __VEC_EXC_OFF__
	if ((gCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, rA, rB);
	uint32 ea;
	uint16 r;
	ea = ((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB]) & ~1;
	int ret = ppc_read_effective_half(ea, r);
	if (ret == PPC_MMU_OK) {
		VECT_H(gCPU.vr[vrD], (ea & 0x0f) >> 1) = r;
	}
}
JITCFlow ppc_opc_gen_lvehx()
{
	ppc_opc_gen_check_vec();
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, rA, rB);
	jitcDropClientVectorRegister(vrD);
	jitcAssertFlushedVectorRegister(vrD);

	if (vrD == gJITC.nativeVectorReg) {
		gJITC.nativeVectorReg = VECTREG_NO;
	}

	jitcClobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB, NATIVE_REG | EAX);
	asmALU32(X86_MOV, &gCPU.vtemp, regA);
	asmALU32(X86_AND, regA, ~0x01);

	jitcClobberAll();
	if (regA != EAX) asmALU32(X86_MOV, EAX, regA);
	asmALU32(X86_MOV, ESI, gJITC.pc);

	asmCALL((NativeAddress)ppc_read_effective_half_z_asm);
	asmALU32(X86_MOV, EAX, &gCPU.vtemp);
	asmALU32(X86_AND, EAX, 0x0e);
	asmALU32(X86_NOT, EAX);

	asmALU8(X86_MOV, EAX, (uint32)&gCPU.vr[vrD]+15, DL);
	asmALU8(X86_MOV, EAX, (uint32)&gCPU.vr[vrD]+16, DH);

	return flowContinue;
}

/*      lvewx	   Load Vector Element Word Indexed
 *      v.122
 */
void ppc_opc_lvewx()
{
#ifndef __VEC_EXC_OFF__
	if ((gCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, rA, rB);
	uint32 ea;
	uint32 r;
	ea = ((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB]) & ~3;
	int ret = ppc_read_effective_word(ea, r);
	if (ret == PPC_MMU_OK) {
		VECT_W(gCPU.vr[vrD], (ea & 0xf) >> 2) = r;
	}
}
JITCFlow ppc_opc_gen_lvewx()
{
	ppc_opc_gen_check_vec();
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, rA, rB);
	jitcDropClientVectorRegister(vrD);
	jitcAssertFlushedVectorRegister(vrD);

	if (vrD == gJITC.nativeVectorReg) {
		gJITC.nativeVectorReg = VECTREG_NO;
	}

	jitcClobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB, NATIVE_REG | EAX);
	asmALU32(X86_MOV, &gCPU.vtemp, regA);
	asmALU32(X86_AND, regA, ~0x03);

	jitcClobberAll();
	if (regA != EAX) asmALU32(X86_MOV, EAX, regA);
	asmALU32(X86_MOV, ESI, gJITC.pc);

	asmCALL((NativeAddress)ppc_read_effective_word_asm);
	asmALU32(X86_MOV, EAX, &gCPU.vtemp);
	asmALU32(X86_AND, EAX, 0x0c);
	asmALU32(X86_NOT, EAX);

	asmALU32(X86_MOV, EAX, (uint32)&gCPU.vr[vrD]+13, EDX);
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
void ppc_opc_lvsl()
{
#ifndef __VEC_EXC_OFF__
	if ((gCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, rA, rB);
	uint32 ea;
	ea = ((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB]);
#if HOST_ENDIANESS == HOST_ENDIANESS_LE
	memmove(&gCPU.vr[vrD], lvsl_helper+0x10-(ea & 0xf), 16);
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
	memmove(&gCPU.vr[vrD], lvsl_helper+(ea & 0xf), 16);
#else
#error Endianess not supported!
#endif
}
JITCFlow ppc_opc_gen_lvsl()
{
	ppc_opc_gen_check_vec();
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, rA, rB);

	if (vrD == gJITC.nativeVectorReg) {
		gJITC.nativeVectorReg = VECTREG_NO;
	}

	jitcClobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB);
	asmALU32(X86_AND, regA, 0x0f);

	if (gJITC.hostCPUCaps.sse) {
		asmShift32(X86_SHL, regA, 4);

		NativeVectorReg reg1 = jitcMapClientVectorRegisterDirty(vrD);

		asmALUPS(X86_MOVAPS, reg1, regA, (uint32)&lvsl_helper_full);
	} else {
		asmALU32(X86_NOT, regA);
		jitcDropClientVectorRegister(vrD);

		NativeReg reg1 = jitcAllocRegister();
		NativeReg reg2 = jitcAllocRegister();
		NativeReg reg3 = jitcAllocRegister();

		asmALU32(X86_MOV, reg1, regA, (uint32)&lvsl_helper+0x11);
		asmALU32(X86_MOV, reg2, regA, (uint32)&lvsl_helper+0x15);
		asmALU32(X86_MOV, reg3, regA, (uint32)&lvsl_helper+0x19);
		asmALU32(X86_MOV, regA, regA, (uint32)&lvsl_helper+0x1d);

		asmALU32(X86_MOV, &gCPU.vr[vrD].w[0], reg1);
		asmALU32(X86_MOV, &gCPU.vr[vrD].w[1], reg2);
		asmALU32(X86_MOV, &gCPU.vr[vrD].w[2], reg3);
		asmALU32(X86_MOV, &gCPU.vr[vrD].w[3], regA);
	}

	return flowContinue;
}

/*
 *      lvsr	    Load Vector for Shift Right
 *      v.125
 */
void ppc_opc_lvsr()
{
#ifndef __VEC_EXC_OFF__
	if ((gCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, rA, rB);
	uint32 ea;
	ea = ((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB]);
#if HOST_ENDIANESS == HOST_ENDIANESS_LE
	memcpy(&gCPU.vr[vrD], lvsl_helper+(ea & 0xf), 16);
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
	memcpy(&gCPU.vr[vrD], lvsl_helper+0x10-(ea & 0xf), 16);
#else
#error Endianess not supported!
#endif
}
JITCFlow ppc_opc_gen_lvsr()
{
	ppc_opc_gen_check_vec();
	int rA, vrD, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, rA, rB);

	if (vrD == gJITC.nativeVectorReg) {
		gJITC.nativeVectorReg = VECTREG_NO;
	}

	jitcClobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB);
	asmALU32(X86_AND, regA, 0x0f);

	if (gJITC.hostCPUCaps.sse) {
		asmShift32(X86_SHL, regA, 4);

		NativeVectorReg reg1 = jitcMapClientVectorRegisterDirty(vrD);

		asmALUPS(X86_MOVAPS, reg1, regA, (uint32)&lvsr_helper_full);
	} else {
		jitcDropClientVectorRegister(vrD);
		jitcAssertFlushedVectorRegister(vrD);

		NativeReg reg1 = jitcAllocRegister();
		NativeReg reg2 = jitcAllocRegister();
		NativeReg reg3 = jitcAllocRegister();

		asmALU32(X86_MOV, reg1, regA, (uint32)&lvsl_helper);
		asmALU32(X86_MOV, reg2, regA, (uint32)&lvsl_helper+4);
		asmALU32(X86_MOV, reg3, regA, (uint32)&lvsl_helper+8);
		asmALU32(X86_MOV, regA, regA, (uint32)&lvsl_helper+12);

		asmALU32(X86_MOV, &gCPU.vr[vrD].w[0], reg1);
		asmALU32(X86_MOV, &gCPU.vr[vrD].w[1], reg2);
		asmALU32(X86_MOV, &gCPU.vr[vrD].w[2], reg3);
		asmALU32(X86_MOV, &gCPU.vr[vrD].w[3], regA);
	}

	return flowContinue;
}

/*
 *      dst	     Data Stream Touch
 *      v.115
 */
void ppc_opc_dst()
{
	VECTOR_DEBUG;
	/* Since we are not emulating the cache, this is a nop */
}
JITCFlow ppc_opc_gen_dst()
{
	/* Since we are not emulating the cache, this is a nop */
	return flowContinue;
}

/*
 *	stb		Store Byte
 *	.632
 */
void ppc_opc_stb()
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rS, rA, imm);
	ppc_write_effective_byte((rA?gCPU.gpr[rA]:0)+imm, (uint8)gCPU.gpr[rS]) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_stb()
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rS, rA, imm);
	ppc_opc_gen_helper_st(PPC_GPR(rA), imm, PPC_GPR(rS));
	asmCALL((NativeAddress)ppc_write_effective_byte_asm);
	return flowEndBlock;
}
/*
 *	stbu		Store Byte with Update
 *	.633
 */
void ppc_opc_stbu()
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rS, rA, imm);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_byte(gCPU.gpr[rA]+imm, (uint8)gCPU.gpr[rS]);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += imm;
	}
}
JITCFlow ppc_opc_gen_stbu()
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rS, rA, imm);
	// FIXME: check rA!=0
	ppc_opc_gen_helper_stu(PPC_GPR(rA), imm, PPC_GPR(rS));
	asmCALL((NativeAddress)ppc_write_effective_byte_asm);
	if (imm) {
		NativeReg r = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALU32(X86_ADD, r, imm);
	}
	return flowContinue;
}
/*
 *	stbux		Store Byte with Update Indexed
 *	.634
 */
void ppc_opc_stbux()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_byte(gCPU.gpr[rA]+gCPU.gpr[rB], (uint8)gCPU.gpr[rS]);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += gCPU.gpr[rB];
	}
}
JITCFlow ppc_opc_gen_stbux()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stux(PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	asmCALL((NativeAddress)ppc_write_effective_byte_asm);
	NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	stbx		Store Byte Indexed
 *	.635
 */
void ppc_opc_stbx()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	ppc_write_effective_byte((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], (uint8)gCPU.gpr[rS]) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_stbx()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stx(PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	asmCALL((NativeAddress)ppc_write_effective_byte_asm);
	return flowEndBlock;
}
/*
 *	stfd		Store Floating-Point Double
 *	.642
 */
void ppc_opc_stfd()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, frS, rA, imm);
	ppc_write_effective_dword((rA?gCPU.gpr[rA]:0)+imm, gCPU.fpr[frS]) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_stfd()
{
	ppc_opc_gen_check_fpu();
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, frS, rA, imm);
	jitcFloatRegisterClobberAll();
	jitcFlushRegister();
	jitcClobberCarryAndFlags();
	jitcGetClientRegister(PPC_FPR_U(frS), NATIVE_REG | ECX);
	jitcGetClientRegister(PPC_FPR_L(frS), NATIVE_REG | EDX);
	if (rA) {
		jitcGetClientRegister(PPC_GPR(rA), NATIVE_REG | EAX);
		if (imm) {
			asmALU32(X86_ADD, EAX, imm);
		}
	} else {
		asmALU32(X86_MOV, EAX, imm);
	}
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	return flowEndBlock;
}
/*
 *	stfdu		Store Floating-Point Double with Update
 *	.643
 */
void ppc_opc_stfdu()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, frS, rA, imm);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_dword(gCPU.gpr[rA]+imm, gCPU.fpr[frS]);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += imm;
	}
}
JITCFlow ppc_opc_gen_stfdu()
{
	ppc_opc_gen_check_fpu();
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, frS, rA, imm);
	jitcFloatRegisterClobberAll();
	jitcFlushRegister();
	jitcClobberCarryAndFlags();
	jitcGetClientRegister(PPC_FPR_U(frS), NATIVE_REG | ECX);
	jitcGetClientRegister(PPC_FPR_L(frS), NATIVE_REG | EDX);
	// FIXME: check rA!=0
	jitcGetClientRegister(PPC_GPR(rA), NATIVE_REG | EAX);
	if (imm) {
		asmALU32(X86_ADD, EAX, imm);
	}
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	if (imm) {
		NativeReg r = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALU32(X86_ADD, r, imm);
	}
	return flowContinue;
}
/*
 *	stfd		Store Floating-Point Double with Update Indexed
 *	.644
 */
void ppc_opc_stfdux()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, frS, rA, rB);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_dword(gCPU.gpr[rA]+gCPU.gpr[rB], gCPU.fpr[frS]);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += gCPU.gpr[rB];
	}
}
JITCFlow ppc_opc_gen_stfdux()
{
	ppc_opc_gen_check_fpu();
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, frS, rA, rB);
	jitcFloatRegisterClobberAll();
	jitcFlushRegister();
	jitcClobberCarryAndFlags();
	jitcGetClientRegister(PPC_FPR_U(frS), NATIVE_REG | ECX);
	jitcGetClientRegister(PPC_FPR_L(frS), NATIVE_REG | EDX);
	// FIXME: check rA!=0
	jitcGetClientRegister(PPC_GPR(rA), NATIVE_REG | EAX);
	asmALU32(X86_ADD, EAX, &gCPU.gpr[rB]);
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	stfdx		Store Floating-Point Double Indexed
 *	.645
 */
void ppc_opc_stfdx()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, frS, rA, rB);
	ppc_write_effective_dword((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], gCPU.fpr[frS]) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_stfdx()
{
	ppc_opc_gen_check_fpu();
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, frS, rA, rB);
	jitcFloatRegisterClobberAll();
	jitcFlushRegister();
	jitcClobberCarryAndFlags();
	jitcGetClientRegister(PPC_FPR_U(frS), NATIVE_REG | ECX);
	jitcGetClientRegister(PPC_FPR_L(frS), NATIVE_REG | EDX);
	if (rA) {
		jitcGetClientRegister(PPC_GPR(rA), NATIVE_REG | EAX);
		asmALU32(X86_ADD, EAX, &gCPU.gpr[rB]);
	} else {
		jitcGetClientRegister(PPC_GPR(rB), NATIVE_REG | EAX);
	}
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_write_effective_dword_asm);
	return flowEndBlock;
}
/*
 *	stfiwx		Store Floating-Point as Integer Word Indexed
 *	.646
 */
void ppc_opc_stfiwx()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, frS, rA, rB);
	ppc_write_effective_word((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], (uint32)gCPU.fpr[frS]) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_stfiwx()
{
	ppc_opc_gen_check_fpu();
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, frS, rA, rB);
	jitcFloatRegisterClobberAll();
	ppc_opc_gen_helper_stx(PPC_GPR(rA), PPC_GPR(rB), PPC_FPR_L(frS));
	asmCALL((NativeAddress)ppc_write_effective_word_asm);
	return flowEndBlock;
}
/*
 *	stfs		Store Floating-Point Single
 *	.647
 */
void ppc_opc_stfs()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, frS, rA, imm);
	uint32 s;
	ppc_double d;
	ppc_fpu_unpack_double(d, gCPU.fpr[frS]);
	ppc_fpu_pack_single(d, s);
	ppc_write_effective_word((rA?gCPU.gpr[rA]:0)+imm, s) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_stfs()
{
	ppc_opc_gen_check_fpu();
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, frS, rA, imm);
	jitcFloatRegisterClobberAll();
	jitcGetClientRegister(PPC_FPR_U(frS), NATIVE_REG | EDX);
	jitcGetClientRegister(PPC_FPR_L(frS), NATIVE_REG | EAX);
	jitcClobberAll();
	asmCALL((NativeAddress)ppc_opc_double_to_single);
	asmALU32(X86_MOV, EDX, EAX);
	if (rA) {
		jitcGetClientRegister(PPC_GPR(rA), NATIVE_REG | EAX);
		if (imm) {
			asmALU32(X86_ADD, EAX, imm);
		}
	} else {
		asmALU32(X86_MOV, EAX, imm);
	}
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_write_effective_word_asm);
	return flowEndBlock;
}
/*
 *	stfsu		Store Floating-Point Single with Update
 *	.648
 */
void ppc_opc_stfsu()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, frS, rA, imm);
	// FIXME: check rA!=0
	uint32 s;
	ppc_double d;
	ppc_fpu_unpack_double(d, gCPU.fpr[frS]);
	ppc_fpu_pack_single(d, s);
	int ret = ppc_write_effective_word(gCPU.gpr[rA]+imm, s);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += imm;
	}
}
JITCFlow ppc_opc_gen_stfsu()
{
	ppc_opc_gen_check_fpu();
	int rA, frS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, frS, rA, imm);
	jitcFloatRegisterClobberAll();
	jitcGetClientRegister(PPC_FPR_U(frS), NATIVE_REG | EDX);
	jitcGetClientRegister(PPC_FPR_L(frS), NATIVE_REG | EAX);
	jitcClobberAll();
	asmCALL((NativeAddress)ppc_opc_double_to_single);
	asmALU32(X86_MOV, EDX, EAX);
	// FIXME: check rA!=0
	jitcGetClientRegister(PPC_GPR(rA), NATIVE_REG | EAX);
	if (imm) {
		asmALU32(X86_ADD, EAX, imm);
	}
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_write_effective_word_asm);
	if (imm) {
		NativeReg r = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALU32(X86_ADD, r, imm);
	}
	return flowContinue;
}
/*
 *	stfsux		Store Floating-Point Single with Update Indexed
 *	.649
 */
void ppc_opc_stfsux()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, frS, rA, rB);
	// FIXME: check rA!=0
	uint32 s;
	ppc_double d;
	ppc_fpu_unpack_double(d, gCPU.fpr[frS]);
	ppc_fpu_pack_single(d, s);
	int ret = ppc_write_effective_word(gCPU.gpr[rA]+gCPU.gpr[rB], s);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += gCPU.gpr[rB];
	}
}
JITCFlow ppc_opc_gen_stfsux()
{
	ppc_opc_gen_check_fpu();
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, frS, rA, rB);
	jitcFloatRegisterClobberAll();
	jitcGetClientRegister(PPC_FPR_U(frS), NATIVE_REG | EDX);
	jitcGetClientRegister(PPC_FPR_L(frS), NATIVE_REG | EAX);
	jitcClobberAll();
	asmCALL((NativeAddress)ppc_opc_double_to_single);
	asmALU32(X86_MOV, EDX, EAX);
	// FIXME: check rA!=0
	jitcGetClientRegister(PPC_GPR(rA), NATIVE_REG | EAX);
	asmALU32(X86_ADD, EAX, &gCPU.gpr[rB]);
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_write_effective_word_asm);
	NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	stfsx		Store Floating-Point Single Indexed
 *	.650
 */
void ppc_opc_stfsx()
{
	if ((gCPU.msr & MSR_FP) == 0) {
		ppc_exception(PPC_EXC_NO_FPU);
		return;
	}
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, frS, rA, rB);
	uint32 s;
	ppc_double d;
	ppc_fpu_unpack_double(d, gCPU.fpr[frS]);
	ppc_fpu_pack_single(d, s);
	ppc_write_effective_word((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], s) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_stfsx()
{
	ppc_opc_gen_check_fpu();
	int rA, frS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, frS, rA, rB);
	jitcFloatRegisterClobberAll();
	jitcGetClientRegister(PPC_FPR_U(frS), NATIVE_REG | EDX);
	jitcGetClientRegister(PPC_FPR_L(frS), NATIVE_REG | EAX);
	jitcClobberAll();
	asmCALL((NativeAddress)ppc_opc_double_to_single);
	asmALU32(X86_MOV, EDX, EAX);
	if (rA) {
		jitcGetClientRegister(PPC_GPR(rA), NATIVE_REG | EAX);
		asmALU32(X86_ADD, EAX, &gCPU.gpr[rB]);
	} else {
		jitcGetClientRegister(PPC_GPR(rB), NATIVE_REG | EAX);
	}
	jitcClobberAll();
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_write_effective_word_asm);
	return flowEndBlock;
}
/*
 *	sth		Store Half Word
 *	.651
 */
void ppc_opc_sth()
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rS, rA, imm);
	ppc_write_effective_half((rA?gCPU.gpr[rA]:0)+imm, (uint16)gCPU.gpr[rS]) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_sth()
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rS, rA, imm);
	ppc_opc_gen_helper_st(PPC_GPR(rA), imm, PPC_GPR(rS));
	asmCALL((NativeAddress)ppc_write_effective_half_asm);
	return flowEndBlock;
}
/*
 *	sthbrx		Store Half Word Byte-Reverse Indexed
 *	.652
 */
void ppc_opc_sthbrx()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	ppc_write_effective_half((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], ppc_bswap_half((uint16)gCPU.gpr[rS])) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_sthbrx()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stx(PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	asmALU8(X86_XCHG, DL, DH);
	asmCALL((NativeAddress)ppc_write_effective_half_asm);
	return flowEndBlock;
}
/*
 *	sthu		Store Half Word with Update
 *	.653
 */
void ppc_opc_sthu()
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rS, rA, imm);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_half(gCPU.gpr[rA]+imm, (uint16)gCPU.gpr[rS]);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += imm;
	}
}
JITCFlow ppc_opc_gen_sthu()
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rS, rA, imm);
	// FIXME: check rA!=0
	ppc_opc_gen_helper_stu(PPC_GPR(rA), imm, PPC_GPR(rS));
	asmCALL((NativeAddress)ppc_write_effective_half_asm);
	if (imm) {
		NativeReg r = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALU32(X86_ADD, r, imm);
	}
	return flowContinue;
}
/*
 *	sthux		Store Half Word with Update Indexed
 *	.654
 */
void ppc_opc_sthux()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_half(gCPU.gpr[rA]+gCPU.gpr[rB], (uint16)gCPU.gpr[rS]);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += gCPU.gpr[rB];
	}
}
JITCFlow ppc_opc_gen_sthux()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stux(PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	asmCALL((NativeAddress)ppc_write_effective_half_asm);
	NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	sthx		Store Half Word Indexed
 *	.655
 */
void ppc_opc_sthx()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	ppc_write_effective_half((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], (uint16)gCPU.gpr[rS]) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_sthx()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stx(PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	asmCALL((NativeAddress)ppc_write_effective_half_asm);
	return flowEndBlock;
}
/*
 *	stmw		Store Multiple Word
 *	.656
 */
void ppc_opc_stmw()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rS, rA, imm);
	uint32 ea = (rA ? gCPU.gpr[rA] : 0) + imm;
	while (rS <= 31) {
		if (ppc_write_effective_word(ea, gCPU.gpr[rS])) {
			return;
		}
		rS++;
		ea += 4;
	}
}
JITCFlow ppc_opc_gen_stmw()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rS, rA, imm);
        while (rS <= 30) {
                ppc_opc_gen_helper_st(PPC_GPR(rA), imm, PPC_GPR(rS+1));
                jitcGetClientRegister(PPC_GPR(rS), NATIVE_REG | ECX);
                asmCALL((NativeAddress)ppc_write_effective_dword_asm);
                rS += 2;
                imm += 8;
        } 
	if (rS == 31) {
		ppc_opc_gen_helper_st(PPC_GPR(rA), imm, PPC_GPR(rS));
		asmCALL((NativeAddress)ppc_write_effective_word_asm);
	}
	return flowEndBlock;
}
/*
 *	stswi		Store String Word Immediate
 *	.657
 */
void ppc_opc_stswi()
{
	int rA, rS, NB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, NB);
	if (NB==0) NB=32;
	uint32 ea = rA ? gCPU.gpr[rA] : 0;
	uint32 r = 0;
	int i = 0;
	
	while (NB > 0) {
		if (!i) {
			r = gCPU.gpr[rS];
			rS++;
			rS%=32;
			i = 4;
		}
		if (ppc_write_effective_byte(ea, (r>>24))) {
			return;
		}
		r<<=8;
		ea++;
		i--;
		NB--;
	}
}
JITCFlow ppc_opc_gen_stswi()
{
	int rA, rS, NB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, NB);
	if (NB==0) NB=32;
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	if (rA) {
		jitcGetClientRegister(PPC_GPR(rA), NATIVE_REG | EAX);
	} else {
		asmALU32(X86_MOV, EAX, (uint32)0);
	}
	asmALU32(X86_MOV, ECX, NB);
	asmALU32(X86_MOV, EBX, rS);
	asmALU32(X86_MOV, ESI, gJITC.pc);
	jitcClobberAll();
	asmCALL((NativeAddress)ppc_opc_stswi_asm);
	return flowEndBlock;
}
/*
 *	stswx		Store String Word Indexed
 *	.658
 */
void ppc_opc_stswx()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	int NB = XER_n(gCPU.xer);
	uint32 ea = gCPU.gpr[rB] + (rA ? gCPU.gpr[rA] : 0);
	uint32 r = 0;
	int i = 0;
	
	while (NB > 0) {
		if (!i) {
			r = gCPU.gpr[rS];
			rS++;
			rS%=32;
			i = 4;
		}
		if (ppc_write_effective_byte(ea, (r>>24))) {
			return;
		}
		r<<=8;
		ea++;
		i--;
		NB--;
	}
}
JITCFlow ppc_opc_gen_stswx()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	jitcGetClientRegister(PPC_XER, NATIVE_REG | ECX);
	if (rA) {
		jitcGetClientRegister(PPC_GPR(rA), NATIVE_REG | EAX);
		asmALU32(X86_ADD, EAX, &gCPU.gpr[rB]);
	} else {
		jitcGetClientRegister(PPC_GPR(rB), NATIVE_REG | EAX);
	}
	asmALU32(X86_AND, ECX, 0x7f);
	jitcClobberAll();
	NativeAddress fixup = asmJxxFixup(X86_Z);
	asmALU32(X86_MOV, EBX, rS);
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_opc_stswi_asm);
	asmResolveFixup(fixup, asmHERE());
	return flowEndBlock;
}
/*
 *	stw		Store Word
 *	.659
 */
void ppc_opc_stw()
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rS, rA, imm);
	ppc_write_effective_word((rA?gCPU.gpr[rA]:0)+imm, gCPU.gpr[rS]) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_stw()
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rS, rA, imm);
	ppc_opc_gen_helper_st(PPC_GPR(rA), imm, PPC_GPR(rS));
	asmCALL((NativeAddress)ppc_write_effective_word_asm);
	return flowEndBlock;
}
/*
 *	stwbrx		Store Word Byte-Reverse Indexed
 *	.660
 */
void ppc_opc_stwbrx()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	// FIXME: doppelt gemoppelt
	ppc_write_effective_word((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], ppc_bswap_word(gCPU.gpr[rS])) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_stwbrx()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stx(PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	asmBSWAP32(EDX);
	asmCALL((NativeAddress)ppc_write_effective_word_asm);
	return flowEndBlock;
}
/*
 *	stwcx.		Store Word Conditional Indexed
 *	.661
 */
void ppc_opc_stwcx_()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	gCPU.cr &= 0x0fffffff;
	if (gCPU.have_reservation) {
		gCPU.have_reservation = false;
		uint32 v;
		if (ppc_read_effective_word((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], v)) {
			return;
		}
		if (v==gCPU.reserve) {
			if (ppc_write_effective_word((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], gCPU.gpr[rS])) {
				return;
			}
			gCPU.cr |= CR_CR0_EQ;
		}
		if (gCPU.xer & XER_SO) {
			gCPU.cr |= CR_CR0_SO;
		}
	}
}
JITCFlow ppc_opc_gen_stwcx_()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	jitcClobberCarryAndFlags();
	asmALU8(X86_AND, (byte*)&gCPU.cr + 3, 0x0f);
	asmBTx32(X86_BTR, &gCPU.have_reservation, 0);
	NativeAddress no_reservation = asmJxxFixup(X86_NC);
	ppc_opc_gen_helper_lx(PPC_GPR(rA), PPC_GPR(rB));
	asmCALL((NativeAddress)ppc_read_effective_word_asm);
	asmALU32(X86_CMP, EDX, &gCPU.reserve);
	// FIXME: mapFlags?
	NativeAddress fixup = asmJxxFixup(X86_NE);
	ppc_opc_gen_helper_stx(PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	asmCALL((NativeAddress)ppc_write_effective_word_asm);
	asmALU8(X86_OR, (byte*)&gCPU.cr + 3, 0x20);  // CR_CR0_EQ
	asmResolveFixup(fixup, asmHERE());
	asmResolveFixup(no_reservation, asmHERE());
	asmTEST32(&gCPU.xer, XER_SO);
	fixup = asmJxxFixup(X86_Z);
	asmALU8(X86_OR, (byte*)&gCPU.cr + 3, 0x10);  // CR_CR0_SO
	asmResolveFixup(fixup, asmHERE());
	return flowEndBlock;
}
/*
 *	stwu		Store Word with Update
 *	.663
 */
void ppc_opc_stwu()
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rS, rA, imm);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_word(gCPU.gpr[rA]+imm, gCPU.gpr[rS]);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += imm;
	}
}
JITCFlow ppc_opc_gen_stwu()
{
	int rA, rS;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rS, rA, imm);
	// FIXME: check rA!=0
	ppc_opc_gen_helper_stu(PPC_GPR(rA), imm, PPC_GPR(rS));
	asmCALL((NativeAddress)ppc_write_effective_word_asm);
	if (imm) {
		NativeReg r = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALU32(X86_ADD, r, imm);
	}
	return flowContinue;
}
/*
 *	stwux		Store Word with Update Indexed
 *	.664
 */
void ppc_opc_stwux()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	// FIXME: check rA!=0
	int ret = ppc_write_effective_word(gCPU.gpr[rA]+gCPU.gpr[rB], gCPU.gpr[rS]);
	if (ret == PPC_MMU_OK) {
		gCPU.gpr[rA] += gCPU.gpr[rB];
	}
}
JITCFlow ppc_opc_gen_stwux()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stux(PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	asmCALL((NativeAddress)ppc_write_effective_word_asm);
	NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	asmALU32(X86_ADD, a, b);
	return flowContinue;
}
/*
 *	stwx		Store Word Indexed
 *	.665
 */
void ppc_opc_stwx()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	ppc_write_effective_word((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB], gCPU.gpr[rS]) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_stwx()
{
	int rA, rS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	ppc_opc_gen_helper_stx(PPC_GPR(rA), PPC_GPR(rB), PPC_GPR(rS));
	asmCALL((NativeAddress)ppc_write_effective_word_asm);
	return flowEndBlock;
}

/*      stvx	    Store Vector Indexed
 *      v.134
 */
void ppc_opc_stvx()
{
#ifndef __VEC_EXC_OFF__
	if ((gCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrS, rA, rB);

	int ea = ((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB]);

	ppc_write_effective_qword(ea, gCPU.vr[vrS]) != PPC_MMU_FATAL;
}
JITCFlow ppc_opc_gen_stvx()
{
	ppc_opc_gen_check_vec();
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrS, rA, rB);

	jitcFlushClientVectorRegister(vrS);
	jitcAssertFlushedVectorRegister(vrS);

	jitcClobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB, NATIVE_REG | EAX);

#if 1
	jitcClobberAll();
	if (regA != EAX)	asmALU32(X86_MOV, EAX, regA);
	asmALU32(X86_MOV, ESI, gJITC.pc);

	if (0 && vrS == gJITC.nativeVectorReg) {
		asmALU32(X86_MOV, EDX, (uint32)&(gCPU.vr[JITC_VECTOR_TEMP]));
		asmCALL((NativeAddress)ppc_write_effective_qword_sse_asm);
	} else {
		asmALU32(X86_MOV, EDX, (uint32)&(gCPU.vr[vrS]));
		asmCALL((NativeAddress)ppc_write_effective_qword_asm);
	}
#else
	asmALU32(X86_AND, regA, ~0x0f);
	asmMOVDMemReg((uint32)&gCPU.vtemp, regA);

	jitcClobberAll();
	if (regA != EAX)	asmALU32(X86_MOV, EAX, regA);
	asmALU32(X86_MOV, ESI, gJITC.pc);

	asmMOVRegDMem(ECX, (uint32)&(gCPU.vr[vrS])+12);
	asmMOVRegDMem(EDX, (uint32)&(gCPU.vr[vrS])+8);

	asmCALL((NativeAddress)ppc_write_effective_dword_asm);

	asmMOVRegDMem(EAX, (uint32)&gCPU.vtemp);
	asmALU32(X86_OR, EAX, 8);

	asmMOVRegDMem(ECX, (uint32)&(gCPU.vr[vrS])+4);
	asmMOVRegDMem(EDX, (uint32)&(gCPU.vr[vrS])+0);

	asmCALL((NativeAddress)ppc_write_effective_dword_asm);
#endif
	return flowEndBlock;
}

/*      stvxl	   Store Vector Indexed LRU
 *      v.135
 */
void ppc_opc_stvxl()
{
	ppc_opc_stvx();
	/* This instruction should hint to the cache that the value won't be
	 *   needed again in memory anytime soon.  We don't emulate the cache,
	 *   so this is effectively exactly the same as stvx.
	 */
}
JITCFlow ppc_opc_gen_stvxl()
{
	return ppc_opc_gen_stvx();
}

/*      stvebx	  Store Vector Element Byte Indexed
 *      v.131
 */
void ppc_opc_stvebx()
{
#ifndef __VEC_EXC_OFF__
	if ((gCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrS, rA, rB);
	uint32 ea;
	ea = (rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB];
	ppc_write_effective_byte(ea, VECT_B(gCPU.vr[vrS], ea & 0xf));
}
JITCFlow ppc_opc_gen_stvebx()
{
	ppc_opc_gen_check_vec();
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrS, rA, rB);

	jitcFlushClientVectorRegister(vrS);
	jitcAssertFlushedVectorRegister(vrS);

	jitcClobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB, NATIVE_REG | EAX);
	asmALU32(X86_MOV, &gCPU.vtemp, regA);
	asmALU32(X86_AND, regA, 0x0f);
	asmALU32(X86_NOT, regA);

	jitcClobberAll();
	if (regA != EAX) asmALU32(X86_MOV, EAX, regA);
	asmALU32(X86_MOV, ESI, gJITC.pc);

	asmALU8(X86_MOV, DL, EAX, ((uint32)&gCPU.vr[vrS])+16);

	asmALU32(X86_MOV, EAX, &gCPU.vtemp);

	asmCALL((NativeAddress)ppc_write_effective_byte_asm);
	return flowEndBlock;
}


/*      stvehx	  Store Vector Element Half Word Indexed
 *      v.132
 */
void ppc_opc_stvehx()
{
#ifndef __VEC_EXC_OFF__
	if ((gCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrS, rA, rB);
	uint32 ea;
	ea = ((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB]) & ~1;
	ppc_write_effective_half(ea, VECT_H(gCPU.vr[vrS], (ea & 0xf) >> 1));
}
JITCFlow ppc_opc_gen_stvehx()
{
	ppc_opc_gen_check_vec();
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrS, rA, rB);

	jitcFlushClientVectorRegister(vrS);
	jitcAssertFlushedVectorRegister(vrS);

	jitcClobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB, NATIVE_REG | EAX);
	asmALU32(X86_MOV, &gCPU.vtemp, regA);
	asmALU32(X86_AND, regA, 0x0e);
	asmALU32(X86_NOT, regA);

	jitcClobberAll();
	if (regA != EAX) asmALU32(X86_MOV, EAX, regA);
	asmALU32(X86_MOV, ESI, gJITC.pc);

	asmALU8(X86_MOV, DL, EAX, ((uint32)&gCPU.vr[vrS])+15);
	asmALU8(X86_MOV, DH, EAX, ((uint32)&gCPU.vr[vrS])+16);

	asmALU32(X86_MOV, EAX, &gCPU.vtemp);
	asmALU32(X86_AND, EAX, ~0x1);

	asmCALL((NativeAddress)ppc_write_effective_half_asm);
	return flowEndBlock;
}


/*      stvewx	  Store Vector Element Word Indexed
 *      v.133
 */
void ppc_opc_stvewx()
{
#ifndef __VEC_EXC_OFF__
	if ((gCPU.msr & MSR_VEC) == 0) {
		ppc_exception(PPC_EXC_NO_VEC);
		return;
	}
#endif
	VECTOR_DEBUG;
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrS, rA, rB);
	uint32 ea;
	ea = ((rA?gCPU.gpr[rA]:0)+gCPU.gpr[rB]) & ~3;
	ppc_write_effective_word(ea, VECT_W(gCPU.vr[vrS], (ea & 0xf) >> 2));
}
JITCFlow ppc_opc_gen_stvewx()
{
	ppc_opc_gen_check_vec();
	int rA, vrS, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrS, rA, rB);

	jitcFlushClientVectorRegister(vrS);
	jitcAssertFlushedVectorRegister(vrS);

	jitcClobberCarryAndFlags();
	NativeReg regA = ppc_opc_gen_helper_lvx(rA, rB, NATIVE_REG | EAX);
	asmALU32(X86_MOV, &gCPU.vtemp, regA);
	asmALU32(X86_AND, regA, 0x0c);
	asmALU32(X86_NOT, regA);

	jitcClobberAll();
	if (regA != EAX) asmALU32(X86_MOV, EAX, regA);
	asmALU32(X86_MOV, ESI, gJITC.pc);

	asmALU32(X86_MOV, EDX, EAX, ((uint32)&gCPU.vr[vrS])+13);

	asmALU32(X86_MOV, EAX, &gCPU.vtemp);
	asmALU32(X86_AND, EAX, ~0x3);

	asmCALL((NativeAddress)ppc_write_effective_word_asm);
	return flowEndBlock;
}

/*      dstst	   Data Stream Touch for Store
 *      v.117
 */
void ppc_opc_dstst()
{
	VECTOR_DEBUG;
	/* Since we are not emulating the cache, this is a nop */
}
JITCFlow ppc_opc_gen_dstst()
{
	/* Since we are not emulating the cache, this is a nop */
	return flowContinue;
}

/*      dss	     Data Stream Stop
 *      v.114
 */
void ppc_opc_dss()
{
	VECTOR_DEBUG;
	/* Since we are not emulating the cache, this is a nop */
}
JITCFlow ppc_opc_gen_dss()
{
	/* Since we are not emulating the cache, this is a nop */
	return flowContinue;
}
