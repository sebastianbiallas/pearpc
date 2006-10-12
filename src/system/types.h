/*
 *	PearPC
 *	types.h
 *
 *	Copyright (C) 1999-2003 Sebastian Biallas (sb@biallas.net)
 *	Copyright (C) 1999-2004 Stefan Weyergraf
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

#ifndef __TYPES_H__
#define __TYPES_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef MIN
#undef MIN
#endif
#ifdef MAX
#undef MAX
#endif
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/*
 *	compiler magic
 */

#ifdef __GNUC__

	// FIXME: configure
#	ifndef __ppc__
#		define FASTCALL __attribute__((regparm (3)))
#	else
#		define FASTCALL
#	endif

#	define FUNCTION_CONST	__attribute__((const))
#	define PACKED		__attribute__((packed))
#	define UNUSED		__attribute__((unused))
#	define DEPRECATED	__attribute__((deprecated))
#	define NORETURN		__attribute__((noreturn))
#	define ALIGN_STRUCT(n)	__attribute__((aligned(n)))
#else
#	error "you're not using the GNU C compiler :-( please add the macros and conditionals for your compiler"
#endif /* !__GNUC__ */

/*
 *	integers
 */

#include SYSTEM_OSAPI_SPECIFIC_TYPES_HDR

/*
 *	NULL
 */

#ifndef NULL
#	define NULL 0
#endif

//FIXME: configure somehow (?)
#ifndef UINT128
#	define UINT128
typedef struct uint128 {
	uint64 l;
	uint64 h;
} uint128;
typedef struct sint128 {
	sint64 l;
	sint64 h;
} sint128;
#endif

union float_uint32 {
	uint32 u;
	float f;
};

union double_uint64 {
	uint64 u;
	double d;
};

#endif
