/*
 *	PearPC
 *	jitc_debug.h
 *
 *	Copyright (C) 2004 Sebastian Biallas (sb@biallas.net)
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

#ifndef __JITC_DEBUG_H__
#define __JITC_DEBUG_H__

//#define JITC_DEBUG

static inline UNUSED uint64 jitcDebugGetTicks()
{
	uint32 s0, s1;
	asm("rdtsc" : "=a" (s0), "=d" (s1));
	return ((uint64)s1)<<32 | s0;
}

#ifdef JITC_DEBUG

void jitcDebugLogNewInstruction();
void jitcDebugLogEmit(const byte *insn, int size);
void jitcDebugLogAdd(const char *fmt, ...);

void jitcDebugInit();
void jitcDebugDone();

#else

static inline UNUSED void jitcDebugLogNewInstruction()
{
}
static inline UNUSED void jitcDebugLogEmit(const byte *insn, int size)
{
}
static inline UNUSED void jitcDebugLogAdd(const char *fmt, ...)
{
}
static inline UNUSED void jitcDebugInit()
{
}
static inline UNUSED void jitcDebugDone()
{
}
#endif

#endif
