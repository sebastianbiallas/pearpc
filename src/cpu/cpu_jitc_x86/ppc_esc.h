/*
 *	PearPC
 *	ppc_dec.h
 *
 *	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
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

#ifndef __PPC_ESC_H__
#define __PPC_ESC_H__

#include "system/types.h"

#define PPC_OPC_ESCAPE_VM 0x0069BABE

#define PPC_ESCAPE_IF_VERSION		0

// values for r3
#define PPC_INTERN_VERSION		0

#define PPC_INTERN_MEMSET		1
#define PPC_INTERN_MEMCPY		2
#define PPC_INTERN_BZERO		3
#define PPC_INTERN_BZERO_PHYS		4
#define PPC_INTERN_BCOPY		5
#define PPC_INTERN_BCOPY_PHYS		6
#define PPC_INTERN_BCOPY_PHYSVIR	7
#define PPC_INTERN_COPY_PAGE		8


void FASTCALL ppc_escape_vm(uint32 func, uint32 *esp, uint32 client_pc);

#endif
