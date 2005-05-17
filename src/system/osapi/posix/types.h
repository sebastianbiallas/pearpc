/*
 *	PearPC
 *	types.h
 *
 *	Copyright (C) 1999-2003 Sebastian Biallas (sb@biallas.net)
 *	Copyright (C) 1999-2002 Stefan Weyergraf
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

#ifndef __POSIX_TYPES_H__
#define __POSIX_TYPES_H__

#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#include <inttypes.h>
#endif /* HAVE_STDINT_H */
#include <sys/types.h>

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t sint8;
typedef int16_t	sint16;
typedef int32_t sint32;
typedef int64_t sint64;

typedef signed int	sint;

typedef uint8		byte;

/* FreeBSD versions after importing gcc34 has broken offsetof() */
#ifdef __FreeBSD__
#include <sys/param.h>
#if __FreeBSD_version >= 502126
#undef offsetof
#define offsetof(TYPE, MEMBER)					\
	(__offsetof__ (reinterpret_cast <size_t>		\
		(&reinterpret_cast <const volatile char &>     	\
		(static_cast<TYPE *> (0)->MEMBER))))
#endif /* __FreeBSD_version >= 502126 */
#endif /* __FreeBSD__ */

#endif
