/*
 *	PearPC
 *	sysvm.cc
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

#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include <limits.h>    /* for PAGESIZE */
#ifndef PAGESIZE
#define PAGESIZE 4096
#endif

#include "system/sysvm.h"
#include "tools/snprintf.h"
#include "tools/debug.h"

#ifndef MAP_JIT
#define MAP_JIT 0
#endif

/*
 *	Is every byte of [base, base+size) within maxDisp of near?
 */
static bool sys_within(const void *base, size_t size, const void *near, size_t maxDisp)
{
	uintptr_t lo = (uintptr_t)base;
	uintptr_t hi = lo + size;		// one past the end, the farthest
	uintptr_t n  = (uintptr_t)near;	// address a branch has to span

	uintptr_t dlo = (n > lo) ? n - lo : lo - n;
	uintptr_t dhi = (n > hi) ? n - hi : hi - n;

	return dlo <= maxDisp && dhi <= maxDisp;
}

static void *sys_mmap_rwx(void *hint, size_t size, int flags)
{
#ifdef MAP_FIXED_NOREPLACE
	/*
	 *	Linux 4.17+.  Without it a hint is merely advisory and the kernel
	 *	may hand back an unrelated address, which the caller then rejects.
	 *	Older kernels ignore the unknown flag, which degrades to exactly
	 *	that advisory behaviour -- so no version check is needed.
	 */
	if (hint) flags |= MAP_FIXED_NOREPLACE;
#endif
	void *p = mmap(hint, size, PROT_READ | PROT_WRITE | PROT_EXEC, flags, -1, 0);

	return (p == MAP_FAILED) ? NULL : p;
}

void *sys_alloc_read_write_execute(size_t size, const void *near, size_t maxDisp)
{
	int flags = MAP_ANON | MAP_PRIVATE;
#if defined(__aarch64__) && defined(__APPLE__)
	/*
	 *	Darwin decides where a MAP_JIT region goes; a hint is not honoured.
	 *	The aarch64 backend therefore passes near == NULL and reaches its
	 *	helpers through the stub table in PPC_CPU_State instead.
	 */
	flags |= MAP_JIT;
#endif
	long pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize <= 0) pagesize = PAGESIZE;

	if (near) {
		/*
		 *	Walk outwards from near, below it first: the region right
		 *	above the executable image tends to be the brk heap, so a
		 *	hint there is more likely to be occupied.  Every gap stays
		 *	far enough inside maxDisp (2GB for x86_64) that the check
		 *	below only ever rejects a hint the kernel moved.
		 */
		static const size_t gaps[] = {
			2u<<20,    4u<<20,   8u<<20,   16u<<20,
			32u<<20,  64u<<20, 128u<<20,  256u<<20,
			512u<<20, 768u<<20, 1024u<<20, 1536u<<20
		};
		uintptr_t anchor = (uintptr_t)near & ~(uintptr_t)(pagesize-1);
		size_t roundedSize = (size + pagesize-1) & ~(size_t)(pagesize-1);

		for (size_t i = 0; i < (sizeof gaps / sizeof gaps[0]); i++) {
			uintptr_t below = anchor - gaps[i] - roundedSize;
			uintptr_t above = anchor + gaps[i];
			uintptr_t hints[2] = { below, above };

			/* underflowed past the bottom of the address space */
			if (below > anchor) hints[0] = 0;

			for (size_t h = 0; h < 2; h++) {
				if (!hints[h]) continue;
				void *p = sys_mmap_rwx((void *)hints[h], size, flags);
				if (!p) continue;
				if (sys_within(p, size, near, maxDisp)) return p;
				munmap(p, size);
			}
		}
	}

	return sys_mmap_rwx(NULL, size, flags);
}

void sys_free_read_write_execute(void *p)
{
	// do nothing :(
}

static int sys_prot_to_posix_prot[] = {
	PROT_NONE,
	PROT_READ,
	PROT_WRITE,
	PROT_READ | PROT_WRITE,
};

void sys_mprotect(void *va, size_t size, int protection)
{
	mprotect(va, size, sys_prot_to_posix_prot[protection]);
}

void *sys_mmap_anon(size_t size)
{
	void *ret = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	return (ret == (void *)-1) ? NULL : ret;
}

void *sys_malloc32(size_t size)
{
	void *ret = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED | MAP_32BIT, -1, 0);
	return (ret == (void *)-1) ? NULL : ret;
}

void sys_free32(void *p, size_t size)
{
	munmap(p, size);
}

/*
Just do
shm_id = shmget(IPC_PRIVATE, size, IPC_CREAT | 0700);
shm_addr = shmat(shm_id, 0, 0);
shmctl(shm_id, IPC_RMID, 0);
and you are fine. shmdt(), exit() or a coredump will cleanup the
shared memory segment.


*/

void *sys_mcommit(void *va, size_t size)
{
	// STUB
	return va;
}

void sys_mfree(void *va, size_t size)
{
	munmap(va, size);
}

struct posix_mapping_area {
	char *name;
	int fd;
	void *mem;
	size_t size;
};

bool sys_alloc_mapping_area(sys_mapping_area *area, size_t size)
{
	posix_mapping_area *a = new posix_mapping_area;
	ht_asprintf(&a->name, "/ppc.mem");
	a->fd = shm_open(a->name, O_CREAT | O_RDWR | O_TRUNC, 0666);
	if (a->fd < 0) {
		if (errno == ENOSYS) {
			free(a->name);
			a->name = NULL;
			a->fd = open("ppc.mem", O_CREAT | O_RDWR, 0666);
			if (a->fd == -1) {
				perror("Can't create ppc.mem");
				return false;
			}
		} else {
			perror("shm_open");
			close(a->fd);
			shm_unlink(a->name);
			free(a->name);
			delete a;
			return false;
		}
	}
	if (ftruncate(a->fd, size) < 0) {
		perror("ftruncate");
		close(a->fd);
		if (a->name) {
			shm_unlink(a->name);
			free(a->name);
		}
		delete a;
		return false;
	}
	a->mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, a->fd, 0);
	if (a->mem == (void*)-1) {
		perror("mmap");
		close(a->fd);
		if (a->name) {
			shm_unlink(a->name);
			free(a->name);
		}
		delete a;
		return false;
	}
	a->size = size;
	*area = (sys_mapping_area)a;
	return true;
}

void *sys_mapping_area_ptr(sys_mapping_area area)
{
	return ((posix_mapping_area *)area)->mem;
}

void *sys_map_area(sys_mapping_area area, size_t ofs, size_t size, int protection, void *hint)
{
	posix_mapping_area *a = (posix_mapping_area *)area;
	void *ret = mmap(hint, size, sys_prot_to_posix_prot[protection], MAP_SHARED, a->fd, ofs);
	return (ret == (void *)-1) ? NULL : ret;
}

void sys_unmap_area(void *base, size_t size)
{
	munmap(base, size);
}

void sys_free_mapping_area(sys_mapping_area area)
{
	posix_mapping_area *a = (posix_mapping_area *)area;
	munmap(a->mem, a->size);
	close(a->fd);
	if (a->name) {
		shm_unlink(a->name);
		free(a->name);
	}
	delete a;
}

bool sys_support_vm()
{
	return true;
}
