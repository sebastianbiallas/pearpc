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
#include "cpu_generic/ppc_mmu.h"
#include "prom.h"
#include "promosi.h"
#include "prommem.h"

char *prom_ea_string(uint32 ea)
{
	byte *r;
	if (ppc_direct_effective_memory_handle(ea, r)) {
		IO_PROM_ERR("nciht gut\n");
		return NULL;
	}
	return (char*)r;
}

byte *gPhysMemoryUsed;
int gPhysMemoryLastpage;

int gPromMemStart, gPromMemEnd;
#define PROM_MEM_SIZE (2*1024*1024)

struct malloc_entry {
	uint32 prev;
	uint32 size;
};
#define MALLOC_BLOCK_FREE (1<<31)
#define MALLOC_BLOCK_GUARD (1)

malloc_entry *gPromMemFreeBlock;
malloc_entry *gPromMemLastBlock;
uint32 gPromMemFreeBlockp;

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
				ppc_mmu_page_create(virt, pa);
			} else {
				prom_claim_page(virt);
				ppc_mmu_page_create(virt, virt);
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

void *prom_mem_eaptr(uint32 ea)
{
	byte *p;
	int r;
	uint32 pa;
	if (!((r = ppc_effective_to_physical(ea, PPC_MMU_READ | PPC_MMU_NO_EXC, pa)))) {
		r = ppc_direct_physical_memory_handle(pa, p);
	}
	if (r) return NULL; else return p;
}

void *prom_mem_ptr(uint32 pa)
{
	byte *p;
	if (ppc_direct_physical_memory_handle(pa, p)) {
		return NULL;
	} else {
		return p;
	}
}

void prom_mem_set(uint32 pa, int c, int size)
{
	if (pa >= gMemorySize || (pa+size) >= gMemorySize) {
		IO_PROM_ERR("in mem_set\n");
	}	
	memset(gMemory+pa, c, size);
}

uint32 prom_mem_malloc(uint32 size)
{
//	ht_printf("malloc: %d", size);
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
		uint32 s = gPromMemFreeBlock->size & ~MALLOC_BLOCK_FREE;
//		ht_printf("s: %08x\n", s);
		if (s > size) {
			// found block
			uint32 blockp = gPromMemFreeBlockp + s - size;
//			ht_printf("blockp: %08x\n", blockp);
			malloc_entry *block = (malloc_entry*)prom_mem_ptr(blockp);
			malloc_entry *next = (malloc_entry*)prom_mem_ptr(gPromMemFreeBlockp + s);
			gPromMemFreeBlock->size = (s - size) | MALLOC_BLOCK_FREE;
			next->prev = blockp;
			block->prev = gPromMemFreeBlockp;
			block->size = size;
//			ht_printf("malloced at %x\n", blockp+sizeof(malloc_entry));
			return blockp+sizeof(malloc_entry);
		}
		if (s == size) {
			// exact match
			gPromMemFreeBlock->size &= ~MALLOC_BLOCK_FREE;
			r = gPromMemFreeBlockp+sizeof(malloc_entry);
			ok = true;
		}
		do { 
			gPromMemFreeBlockp = gPromMemFreeBlock->prev;
			gPromMemFreeBlock = (malloc_entry*)prom_mem_ptr(gPromMemFreeBlockp);
			if (gPromMemFreeBlock->size & MALLOC_BLOCK_GUARD) {
				if (i) {
					IO_PROM_ERR("out of memory!\n");
				}
				i++;
				gPromMemFreeBlockp = gPromMemLastBlock->prev;
				gPromMemFreeBlock = (malloc_entry*)prom_mem_ptr(gPromMemFreeBlockp);
			}
		} while (!(gPromMemFreeBlock->size & MALLOC_BLOCK_FREE));
	}
	return r;
}

void prom_mem_free(uint32 p)
{
	p -= sizeof(malloc_entry);
	malloc_entry *block = (malloc_entry*)prom_mem_ptr(p);
	if (block->size & MALLOC_BLOCK_FREE) {	
		IO_PROM_ERR("attempt to free unused block!\n");
	}
	if (block->size & MALLOC_BLOCK_GUARD) {
		IO_PROM_ERR("attempt to free guard block!\n");
	}
	malloc_entry *prev = (malloc_entry*)prom_mem_ptr(block->prev);
	malloc_entry *next = (malloc_entry*)prom_mem_ptr(p+block->size);
	if ((next->size & MALLOC_BLOCK_FREE) && !(next->size & MALLOC_BLOCK_GUARD)) {
		// merge with upper block
		malloc_entry *nnext = (malloc_entry*)(p+block->size+(next->size & ~MALLOC_BLOCK_FREE));
		nnext->prev = p;
		block->size += next->size & ~MALLOC_BLOCK_FREE;
	}
	if ((prev->size & MALLOC_BLOCK_FREE) && !(prev->size & MALLOC_BLOCK_GUARD)) {
		// merge with lower block
		prev->size += block->size;
		next->prev = block->prev;
		gPromMemFreeBlock = prev;
	} else {
		block->size |= MALLOC_BLOCK_FREE;
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
	 * end the the physical and virtual memory
	 */

	gPromMemStart = gMemorySize - PROM_MEM_SIZE;
	gPromMemEnd = gMemorySize;
	uint32 v = 0-PROM_MEM_SIZE;
	// Allocate the physical pages
	for (int i=gPromMemStart; i<gPromMemEnd; i+=4096) {
		prom_claim_page(i);
		ppc_mmu_page_create(v, i);
		v+=4096;
	}

//	ht_printf("gPromMemStart: %x\ngPromMemEnd: %x\n", gPromMemStart, gPromMemEnd);
	
	malloc_entry *start = (malloc_entry*)prom_mem_ptr(gPromMemStart);
	malloc_entry *end = (malloc_entry*)prom_mem_ptr(gPromMemEnd-sizeof(malloc_entry));
	malloc_entry *mem = (malloc_entry*)prom_mem_ptr(gPromMemStart+sizeof(malloc_entry));

	start->size = sizeof(malloc_entry) | MALLOC_BLOCK_GUARD;
	mem->size = (PROM_MEM_SIZE-2*sizeof(malloc_entry)) | MALLOC_BLOCK_FREE;
//	ht_printf("%x\n", PROM_MEM_SIZE-2*sizeof(malloc_entry));
	end->size = sizeof(malloc_entry) | MALLOC_BLOCK_GUARD;

	start->prev = gPromMemEnd-sizeof(malloc_entry);
	end->prev = gPromMemStart+sizeof(malloc_entry);
	mem->prev = gPromMemStart;

	gPromMemLastBlock = end;
	gPromMemFreeBlock = mem;
	gPromMemFreeBlockp = gPromMemStart+sizeof(malloc_entry);

	/*
	 *	malloc works now
	 */

	gPromOSIEntry = prom_mem_malloc(4);
	ppc_write_physical_word(gPromOSIEntry, PROM_MAGIC_OPCODE);
	gPromOSIEntry = prom_mem_phys_to_virt(gPromOSIEntry);
	return true;
}

bool prom_mem_done()
{
	delete[] gPhysMemoryUsed;
	gPhysMemoryUsed = NULL;
	return true;
}
