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

#ifndef __WIN32_TYPES_H__
#define __WIN32_TYPES_H__

// >> FIXME: only works on x86
typedef unsigned char	uint8;
typedef unsigned short	uint16;
typedef unsigned int	uint32;
typedef unsigned long long uint64;

typedef signed char	sint8;
typedef signed short	sint16;
typedef signed int	sint32;
typedef signed long long sint64;
// <<

typedef unsigned int	uint;
typedef signed int	sint;

typedef uint8		byte;

#endif
