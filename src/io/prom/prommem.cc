/*
 *	PearPC
 *	prommem.cc
 *
 *	Copyright (C) 2003 Sebastian Biallas (sb@biallas.net)
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
#include "cpu/mem.h"
#include "prom.h"
#include "promosi.h"
#include "prommem.h"

/*char *prom_ea_string(uint32 ea)
{
	byte *r;
	if (ppc_direct_effective_memory_handle(ea, r)) {
		IO_PROM_ERR("nciht gut\n");
		return NULL;
	}
	return (char*)r;
}*/

bool prom_get_string(String &result, uint32 ea)
{
	uint32 pa;
	result = "";
	if (!ppc_prom_effective_to_physical(pa, ea)) {
		IO_PROM_ERR("can't translate address in %s\n", __FUNCTION__);
		return false;
	}
	while (1) {
		byte mem[128];
		if (!ppc_dma_read(mem, pa, sizeof mem)) {
			IO_PROM_ERR("read memory in %s\n", __FUNCTION__);
			return false;
		}
		byte *end;
		if ((end = (byte *)memchr(mem, 0, sizeof mem))) {
			if (end != mem) {
				String s(mem, end-mem);
				result += s;
			}
			return true;
		} else {
			String s(mem, sizeof mem);
			result += s;
		}
		pa += sizeof mem;
	}
}

static byte *gPhysMemoryUsed;
static int gPhysMemoryLastpage;

static int gPromMemStart, gPromMemEnd;

struct malloc_entry {
	uint32 prev PACKED;
	uint32 size PACKED;
};

#define MALLOC_BLOCK_FREE (1<<31)
#define MALLOC_BLOCK_GUARD (1)

static uint32 gPromMemFreeBlock;
static uint32 gPromMemLastBlock;

bool prom_claim_page(uint32 phys)
{
	uint32 page = phys / 4096;
	if (gPhysMemoryUsed[page / 8] & (1<<(page&0x7))) {
		IO_PROM_WARN("%08x in use!\n", phys);
		return false;
	}
	gPhysMemoryUsed[page / 8] |= (1<<(page&0x7));
	return true;
}

bool prom_claim_pages(uint32 phys, uint32 size)
{
	sint32 s=size;
	while (s > 0) {
		if (!prom_claim_page(phys)) return false;
		phys += 4096;
		s -= 4096;
	}
	return true;
}

extern uint32 gMemorySize; // GRRR

uint32 prom_get_free_page()
{
	int s = gMemorySize/4096/8+1;
	for (int i=gPhysMemoryLastpage; i<s; i++) {
		if (gPhysMemoryUsed[i]!=0xff) {
			int x=1;
			for (int j=0; j<8; j++) {
				if (!(gPhysMemoryUsed[i] & x)) {
					uint32 pa = (i*8+j)*4096;
					prom_claim_page(pa);
					gPhysMemoryLastpage = i;
					return pa;
				}
				x<<=1;
			}
		}
	}
	return 0;
}

uint32 prom_allocate_virt(uint32 size, uint32 align)
{
	return 0;
}

uint32 prom_allocate_mem(uint32 size, uint32 align, uint32 virt)
{
	uint32 ret;
	if (virt == 0) {
		ret = prom_mem_malloc(size+align-1);
		if (ret % align) {
			ret += align - (ret % align);
		}
	} else {
		int pages = (size / 4096) + ((size % 4096)?1:0);
		ret = virt;
		for (int i=0; i<pages; i++) {
			// test if phys==virtual mapping possible
			if ((virt >= gMemorySize) ||
			    (gPhysMemoryUsed[virt/4096/8] & (1 << ((virt/4096) & 7)))) {
				uint32 pa = prom_get_free_page();
				if (!pa) return (uint32) -1;
				ppc_prom_page_create(virt, pa);
			} else {
				prom_claim_page(virt);
				ppc_prom_page_create(virt, virt);
			}
			virt+=4096;
		}
	}
	return ret;
}

bool prom_free_mem(uint32 virt)
{
	return false;
}

void prom_mem_set(uint32 pa, int c, int size)
{
	if (!ppc_dma_set(pa, c, size)) {
		IO_PROM_ERR("in mem_set\n");
	}
}


static uint32 prom_mem_entry_get_size(uint32 pa)
{
	uint32 r;
	ppc_dma_read(&r, pa, 4);
	return r;
}

static uint32 prom_mem_entry_get_prev(uint32 pa)
{
	uint32 r;
	ppc_dma_read(&r, pa+4, 4);
	return r;
}

static void prom_mem_entry_set_size(uint32 pa, uint32 v)
{
	ppc_dma_write(pa, &v, 4);
}

static void prom_mem_entry_set_prev(uint32 pa, uint32 v)
{
	ppc_dma_write(pa+4, &v, 4);
}

uint32 prom_mem_malloc(uint32 size)
{
	if (!size) {
		IO_PROM_ERR("zero byte allocation!\n");
	}
	size = (size+7) & ~7;
	size += sizeof(malloc_entry);

//	ht_printf(" --> %d\n", size);
	
	int i=0;
	bool ok=false;
	uint32 r = 0;
	while (!ok) {
		uint32 s = prom_mem_entry_get_size(gPromMemFreeBlock) & ~MALLOC_BLOCK_FREE;
//		ht_printf("s: %08x\n", s);
		if (s > size) {
			// found block
			uint32 blockp = gPromMemFreeBlock + s - size;
//			ht_printf("blockp: %08x\n", blockp);
			
			uint32 block = blockp;
			uint32 next = gPromMemFreeBlock+s;
			
			prom_mem_entry_set_size(gPromMemFreeBlock, (s - size) | MALLOC_BLOCK_FREE);
			prom_mem_entry_set_prev(next, blockp);
			prom_mem_entry_set_prev(blockp, gPromMemFreeBlock);
			prom_mem_entry_set_size(blockp, size);						
			
//			ht_printf("malloced at %x\n", blockp+sizeof(malloc_entry));
			return blockp+sizeof(malloc_entry);
		}
		if (s == size) {
			// exact match
			prom_mem_entry_set_size(gPromMemFreeBlock, s);
			r = gPromMemFreeBlock + sizeof(malloc_entry);
			ok = true;
		}
		do {
			gPromMemFreeBlock = prom_mem_entry_get_prev(gPromMemFreeBlock);
			if (prom_mem_entry_get_size(gPromMemFreeBlock) & MALLOC_BLOCK_GUARD) {
				if (i) {
					IO_PROM_ERR("out of memory!\n");
				}
				i++;
				gPromMemFreeBlock = prom_mem_entry_get_prev(gPromMemLastBlock);
			}
		} while (!(prom_mem_entry_get_size(gPromMemFreeBlock) & MALLOC_BLOCK_FREE));
	}
	return r;
}

void prom_mem_free(uint32 p)
{
	uint32 block = p - sizeof(malloc_entry);
	if (prom_mem_entry_get_size(block) & MALLOC_BLOCK_FREE) {	
		IO_PROM_ERR("attempt to free unused block!\n");
	}
	if (prom_mem_entry_get_size(block) & MALLOC_BLOCK_GUARD) {
		IO_PROM_ERR("attempt to free guard block!\n");
	}
	uint32 prev = prom_mem_entry_get_prev(block);
	uint32 next = block + prom_mem_entry_get_size(block);
	if ((prom_mem_entry_get_size(next) & MALLOC_BLOCK_FREE) && !(prom_mem_entry_get_size(next) & MALLOC_BLOCK_GUARD)) {
		// merge with upper block
		uint32 nnext = block + prom_mem_entry_get_size(block) + (prom_mem_entry_get_size(next) & ~MALLOC_BLOCK_FREE);
		prom_mem_entry_set_prev(nnext, block);
		prom_mem_entry_set_size(block, prom_mem_entry_get_size(next) & ~MALLOC_BLOCK_FREE);
	}
	if ((prom_mem_entry_get_size(prev) & MALLOC_BLOCK_FREE) && !(prom_mem_entry_get_size(prev) & MALLOC_BLOCK_GUARD)) {
		// merge with lower block
		prom_mem_entry_set_size(prev, prom_mem_entry_get_size(prev) + prom_mem_entry_get_size(block));
		prom_mem_entry_set_prev(next, prom_mem_entry_get_prev(block));
		gPromMemFreeBlock = prev;
	} else {
		prom_mem_entry_set_size(block, prom_mem_entry_get_size(block) | MALLOC_BLOCK_FREE);
		gPromMemFreeBlock = block;
	}
}

uint32 prom_mem_virt_to_phys(uint32 v)
{
	return v+PROM_MEM_SIZE+gPromMemStart;
}

uint32 prom_mem_phys_to_virt(uint32 p)
{
	return p-gPromMemStart+(0-PROM_MEM_SIZE);
}

bool prom_mem_init()
{
	gPhysMemoryUsed = new byte[gMemorySize / 4096 / 8+1];
	memset(gPhysMemoryUsed, 0, gMemorySize / 4096 / 8+1);
	gPhysMemoryLastpage = 1;

	/*
	 * The Prom-Memory (where the device-tree etc. resides) will be at the
	 * end of the physical and virtual memory
	 */

	gPromMemStart = gMemorySize - PROM_MEM_SIZE;
	gPromMemEnd = gMemorySize;
	uint32 v = 0-PROM_MEM_SIZE;
	// Allocate the physical pages
	for (int i=gPromMemStart; i<gPromMemEnd; i+=4096) {
		prom_claim_page(i);
		ppc_prom_page_create(v, i);
		v+=4096;
	}

//	ht_printf("gPromMemStart: %x\ngPromMemEnd: %x\n", gPromMemStart, gPromMemEnd);
	
	uint32 start = gPromMemStart;
	uint32 end = gPromMemEnd - sizeof(malloc_entry);
	uint32 mem = gPromMemStart + sizeof(malloc_entry);

	prom_mem_entry_set_size(start, sizeof(malloc_entry) | MALLOC_BLOCK_GUARD);
	prom_mem_entry_set_size(mem, (PROM_MEM_SIZE-2*sizeof(malloc_entry)) | MALLOC_BLOCK_FREE);
	prom_mem_entry_set_size(end, sizeof(malloc_entry) | MALLOC_BLOCK_GUARD);

	prom_mem_entry_set_prev(start, gPromMemEnd-sizeof(malloc_entry));
	prom_mem_entry_set_prev(end, gPromMemStart+sizeof(malloc_entry));
	prom_mem_entry_set_prev(mem, gPromMemStart);

	gPromMemLastBlock = end;
	gPromMemFreeBlock = mem;

	/*
	 *	malloc works now
	 */

#define DW(w) (w)>>24, (w)>>16, (w)>>8, (w)>>0, 
	uint8 magic_opcode[] = {
		DW(PROM_MAGIC_OPCODE)
		DW(0x4e800020)	// blr
	};
	gPromOSIEntry = prom_mem_malloc(sizeof magic_opcode);
	ppc_dma_write(gPromOSIEntry, &magic_opcode, sizeof magic_opcode);
	gPromOSIEntry = prom_mem_phys_to_virt(gPromOSIEntry);
	return true;
}

bool prom_mem_done()
{
	delete[] gPhysMemoryUsed;
	gPhysMemoryUsed = NULL;
	return true;
}
