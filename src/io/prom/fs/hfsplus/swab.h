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

 /* basic fuction:
    value = swab_inc(ptr);
	ptr is afterwards incremented by sizeof(value)
 */

/*#if BYTE_ORDER == LITTLE_ENDIAN*/

#define bswap_16(i) createHostInt(&i, 2, big_endian)
#define bswap_32(i) createHostInt(&i, 4, big_endian)
#define bswap_64(i) createHostInt64(&i, 8, big_endian)

#define bswabU16(val) bswap_16(val)

#define abswabU16_inc(ptr) bswap_16(*(*((UInt16**) (void *)(ptr)))++)
#define abswabU32_inc(ptr) bswap_32(*(*((UInt32**) (void *)(ptr)))++)
#define abswabU64_inc(ptr) bswap_64(*(*((APPLEUInt64**) (void *)(ptr)))++)

#define abstoreU16_inc(ptr, val) (*(*((UInt16**) (void *)(ptr)))++) = bswap_16(val)
#define abstoreU32_inc(ptr, val) (*(*((UInt32**) (void *)(ptr)))++) = bswap_32(val)
#define abstoreU64_inc(ptr, val) (*(*((APPLEUInt64**) (void *)(ptr)))++) = bswap_64(val)

#include "apple.h"
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
	APPLEUInt64 v = *(APPLEUInt64 *)ptr;
	*ptr += 8;
	return bswap_64(v);
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
	**p = bswap_32(val);
	*ptr += 8;
}

/*#else // BYTE_ORDER == BIG_ENDIAN

#define bswabU16(val) val

#define bswabU16_inc(ptr) (*((UInt16*) (ptr))++)
#define bswabU32_inc(ptr) (*((UInt32*) (ptr))++)
#define bswabU64_inc(ptr) (*((APPLEUInt64*) (ptr))++)

#define bstoreU16_inc(ptr, val) (*((UInt16*) (ptr))++) = val
#define bstoreU32_inc(ptr, val) (*((UInt32*) (ptr))++) = val
#define bstoreU64_inc(ptr, val) (*((APPLEUInt64*) (ptr))++) = val

#endif*/

/* for the sake of completeness and readability */
#define bswabU8_inc(ptr)       (*(*((UInt8**) (void *)(ptr)))++)
#define bstoreU8_inc(ptr,val)  (*(*((UInt8**) (void *)(ptr)))++) = val
