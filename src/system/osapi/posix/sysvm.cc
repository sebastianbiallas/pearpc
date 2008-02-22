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

void *sys_alloc_read_write_execute(size_t size)
{
	void *p = mmap(0, size, PROT_READ | PROT_WRITE | PROT_EXEC, 
		MAP_32BIT | MAP_ANON | MAP_PRIVATE, -1, 0);

	return (p == (void *)-1) ? NULL : p;
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
#ifdef __linux__
	posix_mapping_area *a = new posix_mapping_area;
	ht_asprintf(&a->name, "/ppc.mem");
	a->fd = shm_open(a->name, O_CREAT | O_RDWR | O_TRUNC, 0666);
	if (a->fd < 0) {
		if (errno == ENOSYS) {
			free(a->name);
			a->name = NULL;
			a->fd = open("ppc.mem", O_CREAT | O_RDWR);
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
#else
#error unimplemented
#endif
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
