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

#include "config.h"		// we need config.h
#include "system/types.h"

#if HOST_ENDIANESS == HOST_ENDIANESS_LE

/*
 *		Little-endian machine
 */
// FIXME: check that machine really is x86
#	define HOST_IS_X86

#	ifndef HOST_IS_X86

#		error "Your machine if little-endian but not an x86. you cant use jitc_x86 on this machine."

#	endif

#elif HOST_ENDIANESS == HOST_ENDIANESS_BE

/*
 *		Big-endian machine
 */
#	error "Your machine is big-endian. The CPU that you have configured (JITC_X86) only works on little-endian Intel/AMD x86 architectures."

#else

/*
 *		Weird-endian machine
 *	HOST_ENDIANESS is neither little- nor big-endian?
 */
#	error "What kind of a weird machine do you have? It's neither little- nor big-endian??? This is unsupported."

#endif

#define ppc_dword_from_BE ppc_dword_to_BE
#define ppc_word_from_BE ppc_word_to_BE
#define ppc_half_from_BE ppc_half_to_BE

static inline __attribute__((const)) uint32 ppc_word_to_BE(uint32 data)
{
	asm (
		"bswap %0": "=r" (data) : "0" (data)
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

static inline __attribute__((const)) bool ppc_carry_3(uint32 a, uint32 b, uint32 c)
{
	register uint8 t;
	asm (
		"addl %2, %1\n\t"
		"setc %0\n\t"
		"addl %3, %1\n\t"
		"adcb $0, %0"
		: "=&q" (t), "+&r" (a)
		:  "g" (b), "g" (c)
	);
	return t;
}
/*
equivalent C-construction:
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
*/

static inline __attribute__((const)) uint32 ppc_word_rotl(uint32 data, int n)
{
	n &= 0x1f;
	return (data << n) | (data >> (32-n));
}

#endif
