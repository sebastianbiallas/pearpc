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
	bm = (sys_beos_mutex *)malloc(sizeof (sys_beos_mutex));
	bm->thread = 0;
	bm->sem = create_sem(1, "a PearPC mutex");
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
	bs = (sys_beos_semaphore *)malloc(sizeof (sys_beos_semaphore));
	if (!bs)
		return B_NO_MEMORY;
	bs->sem = create_sem(0, "a PearPC sem.sem");
	if (bs->sem < B_OK)
		return bs->sem;
	bs->mutex = create_sem(1, "a PearPC sem.mutex");
	if (bs->mutex < B_OK)
		return bs->mutex;
	*s = bs;
	return B_OK;
}

int sys_create_thread(sys_thread *t, int flags, sys_thread_function start_routine, void *arg)
{
	thread_id th;
	status_t err;
	th = spawn_thread((thread_func)start_routine, "a PearPC thread", B_NORMAL_PRIORITY, arg);
	if (th < B_OK)
		return th;
	err = resume_thread(th);
	if (!err)
		*(thread_id *)t = th;
	return err;
}

void sys_destroy_mutex(sys_mutex m)
{
	delete_sem(((sys_beos_mutex *)m)->sem);
	free(m);
}

void sys_destroy_semaphore(sys_semaphore s)
{
	delete_sem(((sys_beos_semaphore *)s)->sem);
	delete_sem(((sys_beos_semaphore *)s)->mutex);
	free(s);
}

void sys_destroy_thread(sys_thread t)
{
	status_t err;
	thread_id th = (thread_id)t;
	//kill_thread(th); // just to make sure
	//wait_for_thread(th, &err);
}

int sys_lock_mutex(sys_mutex m)
{
	status_t err;
	sys_beos_mutex *bm = (sys_beos_mutex *)m;
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
	bm->thread = 0;
	release_sem(bm->sem);
}

void sys_signal_semaphore(sys_semaphore s)
{
	release_sem(((sys_beos_semaphore *)s)->sem);
}

void sys_signal_all_semaphore(sys_semaphore s)
{
	int32 count;
	// XXX that's not safe! that should be a kernel atomic op!
	if (get_sem_count(((sys_beos_semaphore *)s)->sem, &count) < B_OK)
		return;
	release_sem_etc(((sys_beos_semaphore *)s)->sem, count, 0L);
}

void sys_wait_semaphore(sys_semaphore s)
{
	release_sem(((sys_beos_semaphore *)s)->mutex);
	acquire_sem(((sys_beos_semaphore *)s)->sem);
	acquire_sem(((sys_beos_semaphore *)s)->mutex);
}

void sys_wait_semaphore_bounded(sys_semaphore s, int ms)
{
	release_sem(((sys_beos_semaphore *)s)->mutex);
	acquire_sem_etc(((sys_beos_semaphore *)s)->sem, 1, B_RELATIVE_TIMEOUT, (bigtime_t)ms*1000);
	acquire_sem(((sys_beos_semaphore *)s)->mutex);
}

void sys_lock_semaphore(sys_semaphore s)
{
	acquire_sem(((sys_beos_semaphore *)s)->mutex);
}

void sys_unlock_semaphore(sys_semaphore s)
{
	release_sem(((sys_beos_semaphore *)s)->mutex);
}

void sys_exit_thread(void *ret)
{
	// in BeOS the exit code is supposed to be a success/errno
	// code, but that shouldn't matter, it's just an int32.
	// (as long as it can be casted to a void* (beware 64 bits...))
	exit_thread((status_t)ret);
}

void *sys_join_thread(sys_thread t)
{
	void *ret = NULL;
	thread_id th = (thread_id)t;
	//kill_thread(th); // just to make sure
	wait_for_thread(th, (status_t *)&ret);
	return ret;
}
