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
#include "system/systimer.h"

static void signal_handler(int signo, siginfo_t *extra, void *junk);
static const int kTimerSignal = SIGRTMIN;
static const int kClockRT = CLOCK_PROCESS_CPUTIME_ID;
static const int kClock = CLOCK_REALTIME;

struct sys_timer_struct
{
	struct sigevent event_info;
	timer_t timer_id;
	sys_timer_callback callback;
	int clock;
	uint64 timer_res;

	sys_timer_struct(sys_timer_callback cb)
			: timer_id(0), callback(cb), clock(kClock), timer_res(0)
	{
		memset(&event_info, 0, sizeof(event_info));

		event_info.sigev_notify = SIGEV_SIGNAL;
		event_info.sigev_signo = kTimerSignal;
		event_info.sigev_value.sival_ptr = this;
	}
};

static void signal_handler(int signo, siginfo_t *extra, void *junk)
{
	sys_timer_struct *timer = reinterpret_cast<sys_timer_struct *>(extra->si_value.sival_ptr);
	timer->callback(reinterpret_cast<sys_timer>(timer));
}


bool sys_create_timer(sys_timer *t, sys_timer_callback cb_func)
{
	struct sigaction act;

	sigemptyset(&act.sa_mask);
	act.sa_sigaction = signal_handler;
	act.sa_flags = SA_SIGINFO;

	*t = 0;
	if (sigemptyset(&act.sa_mask) == -1) {
		perror("Error calling sigemptyset");
		return false;
	}

	if (sigaction(kTimerSignal, &act, 0) == -1) {
		perror("Error calling sigaction");
		return false;
	}

	sys_timer_struct *newTimer = new sys_timer_struct( cb_func );

	if (timer_create(kClockRT, &(newTimer->event_info),
					 &(newTimer->timer_id)) < 0) {
		if (timer_create(kClock, &(newTimer->event_info),
						 &(newTimer->timer_id)) < 0) {
			perror("Timer create error");
			delete newTimer;
			return false;
		} else {
			newTimer->clock = kClock;
		}
	} else {
		newTimer->clock = kClockRT;
	}

	struct timespec clockRes;
	if (0 == clock_getres(newTimer->clock, &clockRes)) {
		newTimer->timer_res = uint64(clockRes.tv_sec) * 1000 * 1000 * 1000;
		newTimer->timer_res += uint64(clockRes.tv_nsec);
	}

	*t = reinterpret_cast<sys_timer>(newTimer);

	return true;
}

void sys_delete_timer(sys_timer t)
{
	sys_timer_struct *timer = reinterpret_cast<sys_timer_struct *>(t);

	timer_delete(timer->timer_id);
	delete timer;
}

static inline long long int toNSecs(time_t secs, long int nanosecs)
{
	return secs * 1000 * 1000 * 1000 + nanosecs;
}

void sys_set_timer(sys_timer t, time_t secs, long int nanosecs, bool periodic)
{
	sys_timer_struct *timer = reinterpret_cast<sys_timer_struct *>(t);
	struct itimerspec itime;

	itime.it_value.tv_sec = secs;
	itime.it_value.tv_nsec = nanosecs;// + (nanosecs % timer->timer_res);
//	if (itime.it_value.tv_nsec > 1000000000ULL) {
//		itime.it_value.tv_sec += 1;
//		itime.it_value.tv_nsec -= 1000000000ULL;
//	}

	if (periodic) {
		itime.it_interval.tv_sec = secs;
		itime.it_interval.tv_nsec = nanosecs;
	} else {
		itime.it_interval.tv_sec = 0;
		itime.it_interval.tv_nsec = 0;
	}

//	printf("timer %ld\n", itime.it_value.tv_nsec);
	timer_settime(timer->timer_id, 0, &itime, NULL);
}

uint64 sys_get_timer_resolution(sys_timer t)
{
	sys_timer_struct *timer = reinterpret_cast<sys_timer_struct *>(t);
	return timer->timer_res;
}

uint64 sys_get_cpu_ticks()
{
#ifdef __i386__
# ifdef __linux__
	uint64 retval;
	__asm__ __volatile__("rdtsc" : "=A" (retval));
	return retval;
# else
	return clock();
# endif
#else
	return clock();
#endif
}

uint64 sys_get_cpu_ticks_per_second()
{
#ifdef __i386__
# ifdef __linux__
	// FIXME: Doesn't handle variable speed CPUs
	static bool init = true;
	static uint64 ticksPerSec = 0;

	if (init) {
		FILE *cpuFile = fopen("/proc/cpuinfo", "r");
		if (cpuFile) {
			char lineBuf[1024];

			lineBuf[1023] = '\0';
			while (0 != fgets(lineBuf, 1023, cpuFile)) {
				if (0 == strncmp("cpu MHz", lineBuf, 7)) {
					char *sep = strchr(lineBuf, ':');
					if (sep) {
						double tmpval = 0.0;
						if (1 == sscanf(sep, ": %lf", &tmpval)) {
							tmpval *= 1000000.0;
							ticksPerSec = static_cast<uint64>(tmpval);
							break;
						}
					}
				}
			}
			fclose(cpuFile);
		}

		if (0 == ticksPerSec) {
			printf("Unable to query cpu speed from /proc/cpuinfo");
			ticksPerSec = CLOCKS_PER_SEC;
		}

		init = false;
	}

	return ticksPerSec;
# else
	return CLOCKS_PER_SEC;
# endif
#else
	return CLOCKS_PER_SEC;
#endif
}
