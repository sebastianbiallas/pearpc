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

#include "system/sys.h"

#define WIN32_LEAN_AND_MEAN

#include <tchar.h>
#include <windows.h>

#include "debug/tracers.h"
#include <time.h>
#include <mmsystem.h>
#include "system/systimer.h"

static void (*decTimerCB)(sys_timer);  

struct sys_timer_struct
{
	MMRESULT timerID;
	uint64   timerRes;
	sys_timer_callback cb_func;
};

bool sys_create_timer(sys_timer *t, sys_timer_callback cb_func)
{
	#define TARGET_RESOLUTION 1         // 1-millisecond target resolution

	TIMECAPS tc;
	uint64    wTimerRes;

	if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) 
	{	
	    // Error; application can't continue.
	}

	wTimerRes = min(max(tc.wPeriodMin, TARGET_RESOLUTION), tc.wPeriodMax);
	sys_timer_struct * timer = new(sys_timer_struct);
	timeBeginPeriod(wTimerRes);
	timer->timerID = 0;
	timer->timerRes = wTimerRes;
	timer->cb_func = cb_func;
	(*t) = reinterpret_cast<sys_timer*>(timer);
	return true;
}

void sys_delete_timer(sys_timer t)
{
	sys_timer_struct * timer = reinterpret_cast<sys_timer_struct *>(t);
	
	if(timer->timerID)
		timeKillEvent(timer->timerID);
	timeEndPeriod(timer->timerRes);
	delete(timer);
	return;
}

static inline long long int toMSecs(time_t secs, long int nanosecs)
{
	return secs * 1000 + (nanosecs + (nanosecs % (1000 * 1000))) / (1000*1000);
}

void CALLBACK TimeProc(UINT uID, UINT UMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	sys_timer t = reinterpret_cast<sys_timer>(dwUser);
	sys_timer_struct * timer = reinterpret_cast<sys_timer_struct *>(dwUser);
	timer->cb_func(t);
}

void sys_set_timer(sys_timer t, time_t secs, long int nanosecs, bool periodic)
{
	sys_timer_struct * timer = reinterpret_cast<sys_timer_struct *>(t);
	long long int msecs = toMSecs(secs, nanosecs);
	if (msecs == 0)
		timer->cb_func(t);
	if(periodic)
		timer->timerID = timeSetEvent(msecs, 0, TimeProc, reinterpret_cast<DWORD>(timer), TIME_PERIODIC);
	else
		timer->timerID = timeSetEvent(msecs, 0, TimeProc, reinterpret_cast<DWORD>(timer), TIME_ONESHOT);
	
	return;
}

uint64 sys_get_timer_resolution(sys_timer t)
{
	sys_timer_struct * timer = reinterpret_cast<sys_timer_struct *>(t);
	return(timer->timerRes); 
}

uint64 sys_get_cpu_ticks()
{
	uint64 test;
	QueryPerformanceCounter((_LARGE_INTEGER *)&test);
	return test;
}

uint64 sys_get_cpu_ticks_per_second()
{
	uint64 test;
	QueryPerformanceFrequency((_LARGE_INTEGER *)&test);
	return test;
}
