/* 
 *	HT Editor
 *	clipboard.cc - POSIX-specific (windows-)clipboard functions
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

#include "system/sys.h"

static bool open_clipboard()
{
	return false;
}

static void close_clipboard()
{
}

bool sys_write_data_to_native_clipboard(const void *data, int size)
{
	return false;
}

int sys_get_native_clipboard_data_size()
{
	return false;
}

bool sys_read_data_from_native_clipboard(void *data, int max_size)
{
	return false;
}
