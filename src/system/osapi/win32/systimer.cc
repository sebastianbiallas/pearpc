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
// Some stuff from msdn.microsoft.com documentation
//

#include <sys/types.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN

#include <tchar.h>
#include <windows.h>
#include <time.h>
#include <mmsystem.h>

#undef FASTCALL

#include "system/sys.h"

#include "debug/tracers.h"
#include "system/systimer.h"

struct sys_timer_struct
{
	MMRESULT timerID;
	UINT     timerRes;
	sys_timer_callback cb_func;
};

bool sys_create_timer(sys_timer *t, sys_timer_callback cb_func)
{
	#define TARGET_RESOLUTION 1         // 1-millisecond target resolution

	TIMECAPS tc;
	UINT    timerRes;

	if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
		// Error; application can't continue.
		return false;
	}

	timerRes = min(max(tc.wPeriodMin, TARGET_RESOLUTION), tc.wPeriodMax);
	if (timeBeginPeriod(timerRes) != TIMERR_NOERROR) {
		// Error; application can't continue.
		return false;
	}
	sys_timer_struct * timer = new sys_timer_struct;
	timer->timerID = 0;
	timer->timerRes = timerRes;
	timer->cb_func = cb_func;
	(*t) = reinterpret_cast<sys_timer*>(timer);
	return true;
}

void sys_delete_timer(sys_timer t)
{
	sys_timer_struct * timer = reinterpret_cast<sys_timer_struct *>(t);
	
	if (timer->timerID)
		timeKillEvent(timer->timerID);
	timeEndPeriod(timer->timerRes);
	delete timer;
}

static inline long long int toMSecs(time_t secs, long int nanosecs)
{
	return secs * 1000 + (nanosecs + 500*1000) / (1000*1000);
}

void CALLBACK TimeProc(UINT uID, UINT UMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	sys_timer t = reinterpret_cast<sys_timer>(dwUser);
	sys_timer_struct * timer = reinterpret_cast<sys_timer_struct *>(dwUser);
	timer->timerID = 0;
	timer->cb_func(t);
}

void sys_set_timer(sys_timer t, time_t secs, long int nanosecs, bool periodic)
{
	sys_timer_struct * timer = reinterpret_cast<sys_timer_struct *>(t);
	UINT msecs = toMSecs(secs, nanosecs);
	if (msecs == 0) {
		timer->cb_func(t);
	} else {
		if (timer->timerID) timeKillEvent(timer->timerID);
		timer->timerID = timeSetEvent(msecs, timer->timerRes, TimeProc, reinterpret_cast<DWORD>(timer), (periodic) ? TIME_PERIODIC : TIME_ONESHOT);
		if (!timer->timerID) {
			ht_printf("baeh! %d %d \n", msecs, timer->timerRes);
			exit(-1);
		}
	}
}

uint64 sys_get_timer_resolution(sys_timer t)
{
	sys_timer_struct * timer = reinterpret_cast<sys_timer_struct *>(t);
	return timer->timerRes;
}

uint64 sys_get_hiresclk_ticks()
{
	uint64 counter;
	static uint64 lastCounter = 0;
	static uint64 counterBase = 0;
	QueryPerformanceCounter((_LARGE_INTEGER *)&counter);
	if (counter < lastCounter) {
		// overflow
		// FIXME: we loose some ticks here every 47 days
		counterBase += lastCounter;
	}
	lastCounter = counter;
	return counter + counterBase;
}

uint64 sys_get_hiresclk_ticks_per_second()
{
	uint64 frq;
	QueryPerformanceFrequency((_LARGE_INTEGER *)&frq);
	return frq;
}
