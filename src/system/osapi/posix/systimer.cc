/// @file systimer.cc
/// @author Kimball Thurston
///

//
// Copyright (c) 2004 Kimball Thurston
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
#include <sys/time.h>

#include "system/systimer.h"
#include "tools/snprintf.h"

static const int kTimerSignal = SYSTIMER_SIGNAL;
#ifdef USE_POSIX_REALTIME_CLOCK
static void signal_handler(int signo, siginfo_t *extra, void *junk);
static const int kClockRT = CLOCK_PROCESS_CPUTIME_ID;
static const int kClock = CLOCK_REALTIME;
#elif USE_POSIX_SETITIMER
static void signal_handler(int signo);
static const int kClock = ITIMER_REAL;
struct sys_timer_struct;
static sys_timer_struct *gSingleTimer = NULL;
static const int kSignalFlags = 0;
#else
#error no timer support
#endif

struct sys_timer_struct
{
#ifdef USE_POSIX_REALTIME_CLOCK
	struct sigevent event_info;
	timer_t timer_id;
#endif
	sys_timer_callback callback;
	int clock;
	uint64 timer_res;

	sys_timer_struct(sys_timer_callback cb)
			: callback(cb), clock(kClock), timer_res(0)
	{
#ifdef USE_POSIX_REALTIME_CLOCK
		memset(&event_info, 0, sizeof event_info);
		timer_id = 0;

		event_info.sigev_notify = SIGEV_SIGNAL;
		event_info.sigev_signo = kTimerSignal;
		event_info.sigev_value.sival_ptr = this;
#endif
	}
};

#ifdef USE_POSIX_REALTIME_CLOCK
static void signal_handler(int signo, siginfo_t *extra, void *junk)
{
	sys_timer_struct *timer = reinterpret_cast<sys_timer_struct *>(extra->si_value.sival_ptr);
	timer->callback(reinterpret_cast<sys_timer>(timer));
}
#else
# ifdef USE_POSIX_SETITIMER
static void signal_handler(int signo)
{
	if (gSingleTimer != NULL) {
		gSingleTimer->callback(reinterpret_cast<sys_timer>(gSingleTimer));
	}
}
# endif
#endif

bool sys_create_timer(sys_timer *t, sys_timer_callback cb_func)
{
	*t = 0;

	sys_timer_struct *newTimer = new sys_timer_struct(cb_func);

#ifdef USE_POSIX_REALTIME_CLOCK
	int clocks[] = {kClockRT, kClock};

	struct timespec clockRes;
	bool foundTimer = false;

	for (uint i=0; i < (sizeof clocks / sizeof clocks[0]); i++) {
		if (clock_getres(clocks[i], &clockRes) == 0
		 && timer_create(clocks[i], &newTimer->event_info,
					 &newTimer->timer_id) == 0) {

			newTimer->clock = clocks[i];
			foundTimer = true;
			break;
		}
	}
	
	if (!foundTimer) {
		perror("Timer create error");
		delete newTimer;
		return false;
	}

	newTimer->timer_res = uint64(clockRes.tv_sec) * 1000 * 1000 * 1000;
	newTimer->timer_res += uint64(clockRes.tv_nsec);
#else
# ifdef USE_POSIX_SETITIMER
	if (gSingleTimer != NULL) {
		ht_printf("There can only be one active sys timer at a time using\n"
				  "interval timers.\n");
		delete newTimer;
		return false;
	}
	newTimer->timer_res = 10 * 1000 * 1000;
# endif
#endif
	struct sigaction act;

	if (sigemptyset(&act.sa_mask) == -1) {
		perror("Error calling sigemptyset");
		return false;
	}

#ifdef USE_POSIX_REALTIME_CLOCK
	act.sa_sigaction = signal_handler;
	act.sa_flags = SA_SIGINFO;
#else
# ifdef USE_POSIX_SETITIMER
	act.sa_handler = signal_handler;
	act.sa_flags = 0;
# endif
#endif
	if (sigaction(kTimerSignal, &act, 0) == -1) {
		perror("Error calling sigaction");
		return false;
	}

	*t = reinterpret_cast<sys_timer>(newTimer);
#ifdef USE_POSIX_SETITIMER
	gSingleTimer = newTimer;
#endif

	return true;
}

void sys_delete_timer(sys_timer t)
{
	sys_timer_struct *timer = reinterpret_cast<sys_timer_struct *>(t);

#ifdef USE_POSIX_REALTIME_CLOCK
	timer_delete(timer->timer_id);
#else
# ifdef USE_POSIX_SETITIMER
	struct itimerval itime;

	itime.it_value.tv_sec = 0;
	itime.it_value.tv_usec = 0;
	itime.it_interval.tv_sec = 0;
	itime.it_interval.tv_usec = 0;

	setitimer(timer->clock, &itime, NULL);
	gSingleTimer = NULL;
# endif
#endif

	delete timer;
}

void sys_set_timer(sys_timer t, time_t secs, long int nanosecs, bool periodic)
{
	sys_timer_struct *timer = reinterpret_cast<sys_timer_struct *>(t);
#ifdef USE_POSIX_REALTIME_CLOCK
	struct itimerspec itime;

	itime.it_value.tv_sec = secs;
	// FIXME: Do we need to have rounding based on timer resolution here?
	itime.it_value.tv_nsec = nanosecs;// + (nanosecs % timer->timer_res);

	if (periodic) {
		itime.it_interval.tv_sec = secs;
		itime.it_interval.tv_nsec = nanosecs;
	} else {
		itime.it_interval.tv_sec = 0;
		itime.it_interval.tv_nsec = 0;
	}

	if (timer_settime(timer->timer_id, 0, &itime, NULL) < 0) {
		perror(__FUNCTION__);
	}
	
#else
# ifdef USE_POSIX_SETITIMER
	struct itimerval itime;

	itime.it_value.tv_sec = secs;
	itime.it_value.tv_usec = (nanosecs + 500) / 1000;

	if (periodic) {
		itime.it_interval = itime.it_value;
	} else {
		itime.it_interval.tv_sec = 0;
		itime.it_interval.tv_usec = 0;
	}

	setitimer(timer->clock, &itime, NULL);
# endif
#endif
}

uint64 sys_get_timer_resolution(sys_timer t)
{
	sys_timer_struct *timer = reinterpret_cast<sys_timer_struct *>(t);
	return timer->timer_res;
}

uint64 sys_get_hiresclk_ticks()
{
#if HAVE_GETTIMEOFDAY
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);
	//__asm__ __volatile__("rdtsc" : "=A" (retval));

	return (uint64(tv.tv_sec) * 1000000) + tv.tv_usec;
#else
	return clock();
#endif
}

uint64 sys_get_hiresclk_ticks_per_second()
{
#if HAVE_GETTIMEOFDAY
	return 1000000;
#else
	return clock();
#endif
}
