/*
 *	PearPC
 *	sysvm.h
 *
 *	Copyright (C) 2004 Sebastian Biallas (sb@biallas.net)
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

#ifndef _SYSVM_H_
#define _SYSVM_H_

#include <stddef.h>
#include "system/types.h"

bool sys_support_vm();

/*
 *	Allocate size bytes of readable, writable and executable memory.
 *
 *	If near is non-NULL, the allocation is placed so that every byte of the
 *	region is within maxDisp bytes of near.  A JIT backend uses this to keep
 *	its translation cache within reach of the PC-relative branches it emits
 *	towards native helper routines in the executable image (x86_64
 *	call/jmp rel32: 2GB, aarch64 BL: 128MB).
 *
 *	The placement is a request, not a guarantee: if no suitable hole can be
 *	found the allocation still succeeds somewhere unconstrained, because a
 *	distant region remains usable for backends that do not need the reach.
 *	Callers that do need it must check the returned address themselves.
 */
void *sys_alloc_read_write_execute(size_t size, const void *near, size_t maxDisp);
void sys_free_read_write_execute(void *p);

void *sys_malloc32(size_t size);
void sys_free32(void *p, size_t size);

#define SYSVM_PROT_NOACCESS	0
#define SYSVM_PROT_READ		1
#define SYSVM_PROT_WRITE	2

void sys_mprotect(void *va, size_t size, int protection);
void *sys_mmap_anon(size_t size);
void *sys_mcommit(void *va, size_t size);
void sys_mfree(void *va, size_t size);

typedef void *sys_mapping_area;

bool sys_alloc_mapping_area(sys_mapping_area *area, size_t size);
void *sys_mapping_area_ptr(sys_mapping_area area);
void *sys_map_area(sys_mapping_area area, size_t ofs, size_t size, int protection, void *hint);
void sys_unmap_area(void *base, size_t size);
void sys_free_mapping_area(sys_mapping_area area);

#endif /* _SYSVM_H_ */
