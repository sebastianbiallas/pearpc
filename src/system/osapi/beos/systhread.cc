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

//#define DEBUG_VERBOSE
//#define DEBUG_ASSERTS

#ifdef DEBUG_VERBOSE
#define PPC_BTH_TRACE(msg...) ht_printf("[BEOS/THREAD] "msg)
#else
#define PPC_BTH_TRACE(msg...)
#endif

#ifdef DEBUG_ASSERTS
#define ASSERT_SEM(s) assert_sem(s)
#define ASSERT_THREAD(t) assert_thread(t)
#else
#define ASSERT_SEM(s)
#define ASSERT_THREAD(t)
#endif

struct sys_beos_mutex {
	thread_id thread;
	sem_id sem;
};

struct sys_beos_semaphore {
	sem_id sem;
	sem_id mutex;
};

void assert_sem(sem_id s)
{
	int32 cookie = 0;
	sem_info si;
	while (get_next_sem_info(0, &cookie, &si) == B_OK) {
		if (si.sem == s)
			return; /* it belongs to the team, ok */
	}
	debugger("trying to use a sem we don't own!!!");
}

void assert_thread(thread_id t)
{
	int32 cookie = 0;
	thread_info ti;
	while (get_next_thread_info(0, &cookie, &ti) == B_OK) {
		if (ti.thread == t)
			return; /* it belongs to the team, ok */
	}
	debugger("trying to use a thread we don't own!!!");
}

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
	ASSERT_SEM(bm->sem);
	delete_sem(bm->sem);
	free(m);
}

void sys_destroy_semaphore(sys_semaphore s)
{
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d})\n", __FUNCTION__, s, bs->sem, bs->mutex);
	ASSERT_SEM(bs->sem);
	delete_sem(bs->sem);
	ASSERT_SEM(bs->mutex);
	delete_sem(bs->mutex);
	free(s);
}

void sys_destroy_thread(sys_thread t)
{
	status_t err;
	thread_id th = (thread_id)t;
	PPC_BTH_TRACE("%s(T:%d)\n", __FUNCTION__, th);
	ASSERT_THREAD(th);
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
	ASSERT_SEM(bm->sem);
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
	ASSERT_SEM(bm->sem);
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
	ASSERT_SEM(bm->sem);
	release_sem(bm->sem);
}

void sys_signal_semaphore(sys_semaphore s)
{
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d})\n", __FUNCTION__, s, bs->sem, bs->mutex);
	ASSERT_SEM(bs->sem);
	release_sem(bs->sem);
}

void sys_signal_all_semaphore(sys_semaphore s)
{
	int32 count;
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d})\n", __FUNCTION__, s, bs->sem, bs->mutex);
	ASSERT_SEM(bs->sem);
	// XXX that's not safe! that should be a kernel atomic op!
	if (get_sem_count(bs->sem, &count) < B_OK)
		return;
	release_sem_etc(bs->sem, count, 0L);
}

void sys_wait_semaphore(sys_semaphore s)
{
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d})\n", __FUNCTION__, s, bs->sem, bs->mutex);
	ASSERT_SEM(bs->mutex);
	ASSERT_SEM(bs->sem);
	release_sem(bs->mutex);
	acquire_sem(bs->sem);
	acquire_sem(bs->mutex);
}

void sys_wait_semaphore_bounded(sys_semaphore s, int ms)
{
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d}, %dms)\n", __FUNCTION__, s, bs->sem, bs->mutex, ms);
	ASSERT_SEM(bs->mutex);
	ASSERT_SEM(bs->sem);
	release_sem(bs->mutex);
	acquire_sem_etc(bs->sem, 1, B_RELATIVE_TIMEOUT, (bigtime_t)ms*1000);
	acquire_sem(bs->mutex);
}

void sys_lock_semaphore(sys_semaphore s)
{
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d})\n", __FUNCTION__, s, bs->sem, bs->mutex);
	ASSERT_SEM(bs->mutex);
	acquire_sem(bs->mutex);
}

void sys_unlock_semaphore(sys_semaphore s)
{
	sys_beos_semaphore *bs = (sys_beos_semaphore *)s;
	PPC_BTH_TRACE("%s(%p {S:%d, M:%d})\n", __FUNCTION__, s, bs->sem, bs->mutex);
	ASSERT_SEM(bs->mutex);
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
	ASSERT_THREAD(th);
	//kill_thread(th); // just to make sure
	wait_for_thread(th, (status_t *)&ret);
	return ret;
}
