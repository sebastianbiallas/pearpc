/// @file systimer.h
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

#ifndef _SYSTIMER_H_
#define _SYSTIMER_H_

#include <time.h>
#include "types.h"

typedef void *sys_timer;

/**
 * Function that will be called when the timer expires
 * @param t Expired timer.
 */
typedef void (*sys_timer_callback)(sys_timer t);

/**
 * @brief Creates a timer that can be used for high resolution callback.
 *
 * The timer has limited resolution unless the process has been made to
 * be a realtime process, in which case accuracy down to 1 nanosecond is
 * theoretically possible.
 *
 * @param t Pointer to a sys_timer to receive the timer. If the call fails,
 *          This value will be assigned NULL.
 * @param cb_func Pointer to the callback function to call when the timer
 *                expires.
 * @return true if the function succeeds and the timer is created, false
 * otherwise.
 */
bool sys_create_timer(sys_timer *t, sys_timer_callback cb_func);

/**
 * Free resources associated with the given timer
 *
 * @param t Timer to delete.
 */
void sys_delete_timer(sys_timer t);

/**
 * @brief Resets the timer to expire in the given time.
 *
 * Time is relative to the current time, so in @a secs seconds and
 * @a nanosecs, the timer will expire and call the callback given
 * when the timer was created as soon as the process is scheduled by
 * the host operating system.
 * Setting the timer when it is already arm has the effect of resetting
 * the timer.
 *
 * @param t Timer to set.
 * @param secs Number of seconds before the timer should expire.
 * @param nanosecs Number of nanoseconds before the timer should expire.
 * @param periodic If true, the timer is made periodic.
 */
void sys_set_timer(sys_timer t, time_t secs, long int nanosecs, bool periodic);

/**
 * Retrieve the accuracy of the timer in nanoseconds.
 *
 * @return resolution in nanoseconds, 0 on error.
 */
uint64 sys_get_timer_resolution(sys_timer t);

#endif /* _SYSTIMER_H_ */
