/*
 *	HT Editor
 *	systhread.cc
 *
 *	Copyright (C) 2003 Sebastian Biallas (sb@biallas.net)
 *	Copyright (C) 2004 Francois Revol (revol@free.fr)
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
#include <OS.h>
#include "system/systhread.h"
#include <stdio.h>
#include "system/types.h"
#include "tools/snprintf.h"

//#define DEBUG_THIS

#ifdef DEBUG_THIS
#define PPC_BTH_TRACE(msg...) ht_printf("[BEOS/THREAD] "msg)
#else
#define PPC_BTH_TRACE(msg...)
#endif


struct sys_beos_mutex {
	thread_id thread;
	sem_id sem;
};

struct sys_beos_semaphore {
	sem_id sem;
	sem_id mutex;
};

int sys_create_mutex(sys_mutex *m)
{
	sys_beos_mutex *bm;
	PPC_BTH_TRACE("%s(%p)\n", __FUNCTION__, m);
	bm = (sys_beos_mutex *)malloc(sizeof (sys_beos_mutex));
	bm->thread = 0;
	bm->sem = create_sem(1, "a PearPC mutex");
	PPC_BTH_TRACE("%s: {T:%d, S:%d})\n", __FUNCTION__, bm->thread, bm->sem);
	if (bm->sem < B_OK) {
		free(bm);
		return bm->sem;
	}
	*m = bm;
	return B_OK;
}

int sys_create_semaphore(sys_semaphore *s)
{
	sys_beos_semaphore *bs;
	status_t err;
	PPC_BTH_TRACE("%s(%p)\n", __FUNCTION__, s);
	bs = (sys_beos_semaphore *)malloc(sizeof (sys_beos_semaphore));
	if (!bs)
		return B_NO_MEMORY;
	err = bs->sem = create_sem(0, "a PearPC sem.sem");
	if (err < B_OK) {
		free(bs);
		return err;
	}
	err = bs->mutex = create_sem(1, "a PearPC sem.mutex");
	if (err < B_OK) {
		delete_sem(bs->sem);
		free(bs);
		return err;
	}
	PPC_BTH_TRACE("%s: {S:%d, M:%d})\n", __FUNCTION__, bs->sem, bs->mutex);
	*s = bs;
	return B_OK;
}

int sys_create_thread(sys_thread *t, int flags, sys_thread_function start_routine, void *arg)
{
	thread_id th;
	status_t err;
	PPC_BTH_TRACE("%s(%p, 0x%08x, %p, %p)\n", __FUNCTION__, t, flags, start_routine, arg);
	th = spawn_thread((thread_func)start_routine, "a PearPC thread", B_NORMAL_PRIORITY, arg);
	if (th < B_OK)
		return th;
	err = resume_thread(th);
	if (!err)
		*(thread_id *)t = th;
	PPC_BTH_TRACE("%s: T:%d\n", __FUNCTION__, th);
	return err;
}

void sys_destroy_mutex(sys_mutex m)
{
	sys_beos_mutex *bm = (sys_beos_mutex *)m;
	PPC_BTH_TRACE("%s(%p {T:%d, S:%d})\n", __FUNCTION__, m, bm->thread, bm->sem);
	delete_sem(bm->sem);
	free(m);
}

void sys_destroy_semaphore(sys_semaphore s)
{
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d})\n", __FUNCTION__, s, bs->sem, bs->mutex);
	delete_sem(bs->sem);
	delete_sem(bs->mutex);
	free(s);
}

void sys_destroy_thread(sys_thread t)
{
	status_t err;
	thread_id th = (thread_id)t;
	PPC_BTH_TRACE("%s(T:%d)\n", __FUNCTION__, th);
	//kill_thread(th); // just to make sure
	//wait_for_thread(th, &err);
}

int sys_lock_mutex(sys_mutex m)
{
	status_t err;
	sys_beos_mutex *bm = (sys_beos_mutex *)m;
	PPC_BTH_TRACE("%s(%p {T:%d, S:%d})\n", __FUNCTION__, m, bm->thread, bm->sem);
	if (bm->thread == find_thread(NULL))
		return B_OK;
	err = acquire_sem(bm->sem);
	if (err)
		return err;
	bm->thread = find_thread(NULL);
	return B_OK;
}

int sys_trylock_mutex(sys_mutex m)
{
	status_t err;
	sys_beos_mutex *bm = (sys_beos_mutex *)m;
	PPC_BTH_TRACE("%s(%p {T:%d, S:%d})\n", __FUNCTION__, m, bm->thread, bm->sem);
	if (bm->thread == find_thread(NULL))
		return B_OK;
	err = acquire_sem_etc(bm->sem, 1, B_RELATIVE_TIMEOUT, 0LL);
	if (err)
		return err;
	bm->thread = find_thread(NULL);
	return B_OK;
}

void sys_unlock_mutex(sys_mutex m)
{
	sys_beos_mutex *bm = (sys_beos_mutex *)m;
	PPC_BTH_TRACE("%s(%p {T:%d, S:%d})\n", __FUNCTION__, m, bm->thread, bm->sem);
	bm->thread = 0;
	release_sem(bm->sem);
}

void sys_signal_semaphore(sys_semaphore s)
{
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d})\n", __FUNCTION__, s, bs->sem, bs->mutex);
	release_sem(bs->sem);
}

void sys_signal_all_semaphore(sys_semaphore s)
{
	int32 count;
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d})\n", __FUNCTION__, s, bs->sem, bs->mutex);
	// XXX that's not safe! that should be a kernel atomic op!
	if (get_sem_count(bs->sem, &count) < B_OK)
		return;
	release_sem_etc(bs->sem, count, 0L);
}

void sys_wait_semaphore(sys_semaphore s)
{
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d})\n", __FUNCTION__, s, bs->sem, bs->mutex);
	release_sem(bs->mutex);
	acquire_sem(bs->sem);
	acquire_sem(bs->mutex);
}

void sys_wait_semaphore_bounded(sys_semaphore s, int ms)
{
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d}, %dms)\n", __FUNCTION__, s, bs->sem, bs->mutex, ms);
	release_sem(bs->mutex);
	acquire_sem_etc(bs->sem, 1, B_RELATIVE_TIMEOUT, (bigtime_t)ms*1000);
	acquire_sem(bs->mutex);
}

void sys_lock_semaphore(sys_semaphore s)
{
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d})\n", __FUNCTION__, s, bs->sem, bs->mutex);
	acquire_sem(bs->mutex);
}

void sys_unlock_semaphore(sys_semaphore s)
{
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d})\n", __FUNCTION__, s, bs->sem, bs->mutex);
	release_sem(bs->mutex);
}

void sys_exit_thread(void *ret)
{
	PPC_BTH_TRACE("%s(%p)\n", __FUNCTION__, ret);
	// in BeOS the exit code is supposed to be a success/errno
	// code, but that shouldn't matter, it's just an int32.
	// (as long as it can be casted to a void* (beware 64 bits...))
	exit_thread((status_t)ret);
}

void *sys_join_thread(sys_thread t)
{
	void *ret = NULL;
	thread_id th = (thread_id)t;
	PPC_BTH_TRACE("%s(%d)\n", __FUNCTION__, th);
	//kill_thread(th); // just to make sure
	wait_for_thread(th, (status_t *)&ret);
	return ret;
}
