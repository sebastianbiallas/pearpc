/*
 *	PearPC
 *	promboot.cc
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

#ifndef __PROMBOOT_H__
#define __PROMBOOT_H__

#include "system/types.h"
#include "tools/stream.h"

bool prom_user_boot_partition(File *&file, uint32 &size, bool &direct, uint32 &loadAddr, uint32 &entryAddr);

bool mapped_load_elf(File &f);
bool mapped_load_xcoff(File &f, uint disp_ofs);
bool mapped_load_chrp(File &f);
bool mapped_load_direct(File &f, uint vaddr, uint pc);

bool prom_load_boot_file();

#endif
