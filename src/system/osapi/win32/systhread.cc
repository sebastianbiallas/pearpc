/*
 *	PearPC
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
#include "system/systhread.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <process.h>

struct sys_win32_semaphore {
	CRITICAL_SECTION cs;
	HANDLE sem;
};

int sys_create_mutex(sys_mutex *m)
{
	*m = malloc(sizeof (CRITICAL_SECTION));
	InitializeCriticalSection((CRITICAL_SECTION *)*m);
	return 0;
}

int sys_create_semaphore(sys_semaphore *s)
{
	*s = malloc(sizeof (sys_win32_semaphore));
	InitializeCriticalSection(&((sys_win32_semaphore *)*s)->cs);
	((sys_win32_semaphore *)*s)->sem = CreateSemaphore(NULL, 0, 1000000, NULL);
	return 0;
}

int sys_create_thread(sys_thread *t, int flags, sys_thread_function start_routine, void *arg)
{
	unsigned long *p = (unsigned long *)malloc(sizeof (unsigned long));
	*t = p;
	*p = _beginthread((void (*)(void *))start_routine, 0, arg);
	return 0;
}

void sys_destroy_mutex(sys_mutex m)
{
	DeleteCriticalSection((CRITICAL_SECTION *)m);
	free(m);
}

void sys_destroy_semaphore(sys_semaphore s)
{
	DeleteCriticalSection(&((sys_win32_semaphore *)s)->cs);
	CloseHandle(((sys_win32_semaphore *)s)->sem);
	free(s);
}

void sys_destroy_thread(sys_thread t)
{
	// NOOP
}

int sys_lock_mutex(sys_mutex m)
{
	EnterCriticalSection((CRITICAL_SECTION *)m);
	return 0;
}

int sys_trylock_mutex(sys_mutex m)
{
	// this doesnt seems to be possible on windows
	EnterCriticalSection((CRITICAL_SECTION *)m);
	return 0;
}

void sys_unlock_mutex(sys_mutex m)
{
	LeaveCriticalSection((CRITICAL_SECTION *)m);
}

void sys_signal_semaphore(sys_semaphore s)
{
	ReleaseSemaphore(((sys_win32_semaphore *)s)->sem, 1, NULL);
}

void sys_signal_all_semaphore(sys_semaphore s)
{
	ReleaseSemaphore(((sys_win32_semaphore *)s)->sem, 1, NULL);
}

void sys_wait_semaphore(sys_semaphore s)
{
	LeaveCriticalSection(&((sys_win32_semaphore *)s)->cs);
	WaitForSingleObject(((sys_win32_semaphore *)s)->sem, INFINITE);
	EnterCriticalSection(&((sys_win32_semaphore *)s)->cs);
}

void sys_wait_semaphore_bounded(sys_semaphore s, int ms)
{
	LeaveCriticalSection(&((sys_win32_semaphore *)s)->cs);	
	WaitForSingleObject(((sys_win32_semaphore *)s)->sem, ms);
	EnterCriticalSection(&((sys_win32_semaphore *)s)->cs);
}

void sys_lock_semaphore(sys_semaphore s)
{
	EnterCriticalSection(&((sys_win32_semaphore *)s)->cs);
}

void sys_unlock_semaphore(sys_semaphore s)
{
	LeaveCriticalSection(&((sys_win32_semaphore *)s)->cs);
}

void sys_exit_thread(void *ret)
{
}

void *sys_join_thread(sys_thread t)
{
}
