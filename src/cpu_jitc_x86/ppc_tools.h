/*
 *	PearPC
 *	ppc_tools.h
 *
 *	Copyright (C) 2003 Sebastian Biallas (sb@biallas.net)
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

#ifndef __PPC_TOOLS_H__
#define __PPC_TOOLS_H__

#include "system/types.h"

#define HOST_IS_LE
#define HOST_IS_X86


#ifdef HOST_IS_LE

#define ppc_dword_from_BE ppc_dword_to_BE
#define ppc_word_from_BE ppc_word_to_BE
#define ppc_half_from_BE ppc_half_to_BE

#ifdef HOST_IS_X86

static inline __attribute__((const)) uint32 ppc_word_to_BE(uint32 data)
{
	asm (
		"bswapl %0": "=r" (data) : "0" (data)
	);
	return data;
}

static inline __attribute__((const)) uint16 ppc_half_to_BE(uint16 data) 
{
	asm (
		"xchgb %b0,%h0": "=q" (data): "0" (data)
	);
	return data;
}

static inline __attribute__((const)) uint64 ppc_dword_to_BE(uint64 data)
{
	return (((uint64)ppc_word_to_BE(data)) << 32) | (uint64)ppc_word_to_BE(data >> 32);
}


#else

static inline __attribute__((const))uint32 ppc_word_to_BE(uint32 data)
{
	return (data>>24)|((data>>8)&0xff00)|((data<<8)&0xff0000)|(data<<24);
}

static inline __attribute__((const))uint64 ppc_dword_to_BE(uint64 data)
{
	return (((uint64)ppc_word_to_BE(data)) << 32) | (uint64)ppc_word_to_BE(data >> 32);
}

static inline __attribute__((const))uint16 ppc_half_to_BE(uint16 data)
{
	return (data<<8)|(data>>8);
}

#endif
#else

bla

#endif

#ifndef HOST_IS_X86
static inline __attribute__((const)) bool ppc_carry_3(uint32 a, uint32 b, uint32 c)
{
	if ((a+b) < a) {
		return true;
	}
	if ((a+b+c) < c) {
		return true;
	}
	return false;
}
#else
static inline __attribute__((const)) bool ppc_carry_3(uint32 a, uint32 b, uint32 c)
{
	register uint8 t;
	asm (
		"addl %2, %1\n\t"
		"setc %0\n\t"
		"addl %3, %1\n\t"
		"adcb $0, %0"
		: "=&Q" (t), "+&r" (a)
		:  "g" (b), "g" (c)
	);
	return t;
}
#endif
static inline __attribute__((const)) uint32 ppc_word_rotl(uint32 data, int n)
{
	n &= 0x1f;
	return (data << n) | (data >> (32-n));
}

#endif
