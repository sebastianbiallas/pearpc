/*
 *	PearPC
 *	sysendian.h
 *
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

#ifndef __SYSTEM_ARCH_SPECIFIC_SYSENDIAN_H__
#define __SYSTEM_ARCH_SPECIFIC_SYSENDIAN_H__

#include "system/types.h"

#define ppc_dword_from_BE ppc_dword_to_BE
#define ppc_word_from_BE ppc_word_to_BE
#define ppc_half_from_BE ppc_half_to_BE

#define ppc_dword_from_LE ppc_dword_to_LE
#define ppc_word_from_LE ppc_word_to_LE
#define ppc_half_from_LE ppc_half_to_LE

#define ppc_dword_to_LE(n) (n)
#define ppc_word_to_LE(n) (n)
#define ppc_half_to_LE(n) (n)

static inline FUNCTION_CONST uint32 ppc_word_to_BE(uint32 data)
{
	asm (
		"bswap %0": "=r" (data) : "0" (data)
	);
	return data;
}

static inline FUNCTION_CONST uint16 ppc_half_to_BE(uint16 data) 
{
	asm (
		"xchgb %b0,%h0": "=q" (data): "0" (data)
	);
	return data;
}

static inline FUNCTION_CONST uint64 ppc_dword_to_BE(uint64 data)
{
	return (((uint64)ppc_word_to_BE(data)) << 32) | (uint64)ppc_word_to_BE(data >> 32);
}

#endif
