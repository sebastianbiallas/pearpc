/// @file systimer.cc
/// @author Francois Revol
///

//
// Copyright (c) 2004 Francois Revol
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <OS.h>
#include "system/systimer.h"

static void signal_handler(int signo, void *sa_userdata/*, regs*/);
static const int kTimerSignal = SIGUSR2;//SIGRTMIN;

struct sys_timer_struct
{
	sys_timer_callback callback;
	thread_id thread;
	thread_id target;
	bigtime_t period;
	bool periodic;

	sys_timer_struct(sys_timer_callback cb)
			: callback(cb), thread(-1), target(-1), period(0LL), periodic(false)
	{
	}
};

static void signal_handler(int signo, void *sa_userdata/*, siginfo_t *extra, void *junk*/)
{
	sys_timer_struct *timer = reinterpret_cast<sys_timer_struct *>(sa_userdata);
	timer->callback(reinterpret_cast<sys_timer>(timer));
}

static int32 timer_thread(void *_arg)
{
	sys_timer_struct *timer = (sys_timer_struct *)_arg;
	do {
		snooze(timer->period);
		kill(timer->target, kTimerSignal);
	} while (timer->periodic);
	return B_OK;
}

bool sys_create_timer(sys_timer *t, sys_timer_callback cb_func)
{
	struct sigaction act;

	sigemptyset(&act.sa_mask);
	act.sa_handler = (__signal_func_ptr)signal_handler; // POSIX SUX
	//act.sa_flags = SA_SIGINFO;

	*t = 0;
	if (sigemptyset(&act.sa_mask) == -1) {
		perror("Error calling sigemptyset");
		return false;
	}

	sys_timer_struct *newTimer = new sys_timer_struct( cb_func );
	act.sa_userdata = newTimer;

	if (sigaction(kTimerSignal, &act, 0) == -1) {
		perror("Error calling sigaction");
		delete newTimer;
		return false;
	}

	*t = reinterpret_cast<sys_timer>(newTimer);

	return true;
}

void sys_delete_timer(sys_timer t)
{
	sys_timer_struct *timer = reinterpret_cast<sys_timer_struct *>(t);

	if (timer->thread > B_OK) {
		status_t err;
		kill_thread(timer->thread);
		wait_for_thread(timer->thread, &err);
		timer->thread = -1;
	}
	delete timer;
}

static inline long long int toUSecs(time_t secs, long int nanosecs)
{
	return secs * 1000 * 1000 + nanosecs / 1000;
}

void sys_set_timer(sys_timer t, time_t secs, long int nanosecs, bool periodic)
{
	sys_timer_struct *timer = reinterpret_cast<sys_timer_struct *>(t);
	//fprintf(stderr, "%s(%p, %lds %ldns, %s)\n", __FUNCTION__, timer, secs, nanosecs, periodic?"t":"f");
	timer->periodic = periodic;
	timer->period = toUSecs(secs, nanosecs);
	timer->target = find_thread(NULL);
	if (timer->thread > B_OK) {
		status_t err;
		kill_thread(timer->thread);
		wait_for_thread(timer->thread, &err);
	}
	timer->thread = spawn_thread(timer_thread, "timer_thread", B_DISPLAY_PRIORITY, (void *)timer);
	if (timer->thread >= B_OK) {
		resume_thread(timer->thread);
	} // else handle error ???
}

uint64 sys_get_timer_resolution(sys_timer t)
{
	return 3*1000; // [ns] something like that.
}

uint64 sys_get_hiresclk_ticks()
{
	// so simple :p
	// on x86 it does use rdtsc, and the cpu counter on ppc.
	return system_time();
}

uint64 sys_get_hiresclk_ticks_per_second()
{
	return 1000000ULL;
}
