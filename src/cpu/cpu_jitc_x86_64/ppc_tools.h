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

#include "system/arch/sysendian.h"

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
