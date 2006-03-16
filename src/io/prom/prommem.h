/*
 *	PearPC
 *	prommem.h
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

#ifndef __IO_PROMMEM_H__
#define __IO_PROMMEM_H__

#include "system/types.h"

#define PROM_MEM_SIZE (2*1024*1024)

bool prom_get_string(String &result, uint32 ea);
bool prom_claim_page(uint32 phys);
bool prom_claim_pages(uint32 phys, uint32 size);
uint32 prom_allocate_mem(uint32 size, uint32 align=0, uint32 virt=0);
bool prom_free_mem(uint32 virt);

uint32 prom_mem_malloc(uint32 s);
void prom_mem_free(uint32 p);
uint32 prom_mem_phys_to_virt(uint32 pa);
uint32 prom_mem_virt_to_phys(uint32 va);
void prom_mem_set(uint32 pa, int c, int size);

bool prom_mem_init();
bool prom_mem_done();

#endif

