/*
 *	HT Editor
 *	systhread.cc
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
#include <cerrno>
#include <sys/time.h>
#include <pthread.h>

#include "system/systhread.h"

#ifdef HAVE_MACH_CLOCK_H
#include <mach/clock.h>
static mach_port_t clock_port;
#endif

struct sys_pthread_semaphore {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

int sys_create_mutex(sys_mutex *m)
{
	*m = (pthread_mutex_t *)malloc(sizeof (pthread_mutex_t));
	return pthread_mutex_init((pthread_mutex_t *)*m, NULL);
}

int sys_create_semaphore(sys_semaphore *s)
{
	*s = (sys_pthread_semaphore *)malloc(sizeof (sys_pthread_semaphore));
	pthread_mutex_init(&((sys_pthread_semaphore*)*s)->mutex, NULL);
	return pthread_cond_init(&((sys_pthread_semaphore*)*s)->cond, NULL);
}

int sys_create_thread(sys_thread *t, int flags, sys_thread_function start_routine, void *arg)
{
	*t = (pthread_t *)malloc(sizeof (pthread_t));
	return pthread_create((pthread_t *)*t, 0, start_routine, arg);
}

void sys_destroy_mutex(sys_mutex m)
{
	pthread_mutex_destroy((pthread_mutex_t *)m);
	free(m);
}

void sys_destroy_semaphore(sys_semaphore s)
{
	pthread_mutex_destroy(&((sys_pthread_semaphore*)s)->mutex);
	pthread_cond_destroy(&((sys_pthread_semaphore*)s)->cond);
	free(s);
}

void sys_destroy_thread(sys_thread t)
{
	// NOOP for pthreads
}

int sys_lock_mutex(sys_mutex m)
{
	return pthread_mutex_lock((pthread_mutex_t*)m);
}

int sys_trylock_mutex(sys_mutex m)
{
	return pthread_mutex_trylock((pthread_mutex_t*)m);
}

void sys_unlock_mutex(sys_mutex m)
{
	pthread_mutex_unlock((pthread_mutex_t*)m);
}

void sys_signal_semaphore(sys_semaphore s)
{
	pthread_cond_signal(&((sys_pthread_semaphore*)s)->cond);
}

void sys_signal_all_semaphore(sys_semaphore s)
{
	pthread_cond_broadcast(&((sys_pthread_semaphore*)s)->cond);
}

void sys_wait_semaphore(sys_semaphore s)
{
	pthread_cond_wait(&((sys_pthread_semaphore*)s)->cond, &((sys_pthread_semaphore*)s)->mutex);
}

void sys_wait_semaphore_bounded(sys_semaphore s, int ms)
{
	struct timespec ts;
	uint64 nsec;
#ifdef HAVE_MACH_CLOCK_H
	mach_timespec_t ts2;
	clock_get_time(clock_port, &ts2);
	nsec = (ts2.tv_nsec + ((uint64)ms)*1000*1000);
	ts.tv_sec = ts2.tv_sec+(uint)(nsec/1000000000);
	ts.tv_nsec = (nsec % 1000000000ULL);
#else
	clock_gettime(CLOCK_REALTIME, &ts);
	nsec = (ts.tv_nsec + ((uint64)ms)*1000*1000);
	ts.tv_sec = ts.tv_sec+(uint)(nsec/1000000000);
	ts.tv_nsec = (nsec % 1000000000ULL);
#endif
	pthread_cond_timedwait(&((sys_pthread_semaphore*)s)->cond, &((sys_pthread_semaphore*)s)->mutex, &ts);
}

void sys_lock_semaphore(sys_semaphore s)
{
	pthread_mutex_lock(&((sys_pthread_semaphore*)s)->mutex);
}

void sys_unlock_semaphore(sys_semaphore s)
{
	pthread_mutex_unlock(&((sys_pthread_semaphore*)s)->mutex);
}

void sys_exit_thread(void *ret)
{
	pthread_exit(ret);
}

void *sys_join_thread(sys_thread t)
{
	void *ret;
	pthread_join(*(pthread_t *)t, &ret);
	return ret;
}
