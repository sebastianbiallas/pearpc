/// @file sysclk.h
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

#ifndef _SYSCLK_H_
#define _SYSCLK_H_

#include "types.h"
/**
 * Retrieves the host global high-resolution ticks. This is expected to be a
 * counter that is very fast to query, and has very good resolution.
 *
 * On x86 architecture, this is probably best implemented as retrieving the time
 * stamp counter (rdtsc).*/
uint64 sys_get_hiresclk_ticks();

/*
 * Find the number of high resolution ticks per second.
 */
uint64 sys_get_hiresclk_ticks_per_second();

#endif /* _SYSCLK_H_ */
