/*
 *	PearPC
 *	jitc_types.h
 *
 *	Copyright (C) 2004 Sebastian Biallas (sb@biallas.net)
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

#ifndef __JITC_TYPES_H__
#define __JITC_TYPES_H__

#include "system/types.h"

/* 
 *	This refers to (code) addresses in the host computer
 *	They have to be in the translation cache.
 */
typedef byte *NativeAddress;

/*
 *	This is the return type of opcode generation functions
 */
enum JITCFlow {
	flowContinue,
	flowEndBlock,
	flowEndBlockUnreachable,
};

typedef void (*ppc_opc_function)();

#endif
