/*
 * libhfs - library for reading and writing Macintosh HFS volumes
 *
 * Copyright (C) 2000 Klaus Halfmann <klaus.halfmann@feri.de>
 * Original work 1996-1998 Robert Leslie <rob@mars.org>
 *
 * This file defines some byte swapping function. I did not find this
 * in any standard or linux way.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "tools/endianess.h"
#include "apple.h"

 /* basic fuction:
    value = swab_inc(ptr);
	ptr is afterwards incremented by sizeof(value)
 */

#define bswap_16(i) createHostInt(&(i), 2, big_endian)
#define bswap_32(i) createHostInt(&(i), 4, big_endian)
#define bswap_64(i) createHostInt64(&(i), 8, big_endian)

#define bswabU16(val) bswap_16(val)

static inline UInt8 bswabU8_inc(char **ptr)
{
	UInt8 v = *(UInt8 *)*ptr;
	*ptr += 1;
	return v;
}

static inline UInt16 bswabU16_inc(char **ptr)
{
	UInt16 v = *(UInt16 *)*ptr;
	*ptr += 2;
	return bswap_16(v);
}

static inline UInt32 bswabU32_inc(char **ptr)
{
	UInt32 v = *(UInt32 *)*ptr;
	*ptr += 4;
	return bswap_32(v);
}

static inline APPLEUInt64 bswabU64_inc(char **ptr)
{
	APPLEUInt64 v = *(APPLEUInt64 *)*ptr;
	*ptr += 8;
	return bswap_64(v);
}

static inline void bstoreU8_inc(char **ptr, UInt8 val)
{
	UInt8 **p = (UInt8 **)ptr;
	**p = val;
	*ptr += 1;
}

static inline void bstoreU16_inc(char **ptr, UInt16 val)
{
	UInt16 **p = (UInt16 **)ptr;
	**p = bswap_16(val);
	*ptr += 2;
}

static inline void bstoreU32_inc(char **ptr, UInt32 val)
{
	UInt32 **p = (UInt32 **)ptr;
	**p = bswap_32(val);
	*ptr += 4;
}

static inline void bstoreU64_inc(char **ptr, APPLEUInt64 val)
{
	APPLEUInt64 **p = (APPLEUInt64 **)ptr;
	**p = bswap_64(val);
	*ptr += 8;
}

