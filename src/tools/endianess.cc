/* 
 *	HT Editor
 *	endianess.cc
 *
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
 
#include <cstring>

#include "debug.h"
#include "endianess.h"
#include "system/types.h"

extern "C" void createForeignInt(void *buf, int i, int size, Endianess to_endianess)
{
	uint8 *b = (uint8*)buf;
	switch (size) {
		case 1: b[0] = i; break;
		case 2:
			switch (to_endianess) {
				case big_endian:
					b[0] = i>>8;
					b[1] = i;
					break;
				case little_endian:
					b[0] = i;
					b[1] = i>>8;
					break;
			}
			break;
		case 4:
			switch (to_endianess) {
				case big_endian:
					b[0] = i>>24;
					b[1] = i>>16;
					b[2] = i>>8;
					b[3] = i;
					break;
				case little_endian:
					b[0] = i;
					b[1] = i>>8;
					b[2] = i>>16;
					b[3] = i>>24;
					break;
			}
			break;
		default: ASSERT(0);
	}
}

extern "C" int createHostInt(const void *buf, int size, Endianess from_endianess)
{
	uint8 *b = (uint8*)buf;
	switch (size) {
		case 1:
			return b[0];
		case 2:
			switch (from_endianess) {
				case big_endian:
					return (b[0]<<8) | b[1];
				case little_endian:
					return (b[1]<<8) | b[0];
			}
			break;
		case 4:
			switch (from_endianess) {
				case big_endian:
					return (b[0]<<24) | (b[1]<<16) | (b[2]<<8) | b[3];
				case little_endian:
					return (b[3]<<24) | (b[2]<<16) | (b[1]<<8) | b[0];
			}
			break;
	}
	ASSERT(0);
	return 0;
}

extern "C" void createForeignInt64(void *buf, uint64 i, int size, Endianess to_endianess)
{
	if (size <= 4) {
		return createForeignInt(buf, i, size, to_endianess);
	}
	uint8 *b = (uint8*)buf;
	uint32 hi = i >> 32;
	uint32 lo = i;
	switch (to_endianess) {
		case big_endian:
			b[0] = hi>>24;
			b[1] = hi>>16;
			b[2] = hi>>8;
			b[3] = hi;
			b[4] = lo>>24;
			b[5] = lo>>16;
			b[6] = lo>>8;
			b[7] = lo;
			break;
		case little_endian:
			b[0] = lo;
			b[1] = lo>>8;
			b[2] = lo>>16;
			b[3] = lo>>24;
			b[4] = hi;
			b[5] = hi>>8;
			b[6] = hi>>16;
			b[7] = hi>>24;
			break;
		default: ASSERT(0);
	}
}

extern "C" uint64 createHostInt64(const void *buf, int size, Endianess from_endianess)
{
	if (size <= 4) {
		return createHostInt(buf, size, from_endianess);
	}
	uint8 *b = (uint8*)buf;
	uint64 q;
	switch (from_endianess) {
		case big_endian:
			q = (((uint64)(b[0]<<24) | (b[1]<<16) | (b[2]<<8) | b[3]) << 32) |
			                       (b[4]<<24) | (b[5]<<16) | (b[6]<<8) | b[7];
			break;
		case little_endian:
			q = (((uint64)(b[7]<<24) | (b[6]<<16) | (b[5]<<8) | b[4]) << 32) |
			                       (b[3]<<24) | (b[2]<<16) | (b[1]<<8) | b[0];
		default: ASSERT(0);
	}
	return q;
}

extern "C" void createHostStructx(void *buf, uint bufsize, const uint8 *table, Endianess from_endianess)
{
	uint8 *buf2 = (uint8*)buf;
	while (*table) {
		if (bufsize) ASSERT(buf2 - (uint8*)buf < bufsize);
		int table2 = *table & ~STRUCT_ENDIAN_HOST;
		if (*table & STRUCT_ENDIAN_HOST) {
			switch (table2) {
				case STRUCT_ENDIAN_8: {
					uint8 a = createHostInt(buf2, STRUCT_ENDIAN_8, from_endianess);
					memcpy(buf2, &a, STRUCT_ENDIAN_8);
					break;
				}
				case STRUCT_ENDIAN_16: {
					uint16 a = createHostInt(buf2, STRUCT_ENDIAN_16, from_endianess);
					memcpy(buf2, &a, STRUCT_ENDIAN_16);
					break;
				}
				case STRUCT_ENDIAN_32: {
					uint32 a = createHostInt(buf2, STRUCT_ENDIAN_32, from_endianess);
					memcpy(buf2, &a, STRUCT_ENDIAN_32);
					break;
				}
				case STRUCT_ENDIAN_64: {
					uint64 q = createHostInt64(buf2, STRUCT_ENDIAN_64, from_endianess);
					memcpy(buf2, &q, STRUCT_ENDIAN_64);
					break;
				}
				default: ASSERT(0);
			}
		}
		buf2 += table2;
		table++;
	}
	if (bufsize) ASSERT(buf2 - (uint8*)buf == bufsize);
}
