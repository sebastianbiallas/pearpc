/* 
 *	PearPC
 *	gui.h
 *
 *	Copyright (C) 1999-2003 Sebastian Biallas (sb@biallas.net)
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
#ifndef __GUI_H__
#define __GUI_H__

#include "tools/data.h"
#include "tools/str.h"

void sys_gui_init();
bool sys_gui_open_file_dialog(String &ret, const String &title, const String &filespec, const String &filespecname, const String &home, bool existing);

#define MB_INFO		0
#define MB_QUESTION	1
#define MB_WARN		2
#define MB_ERR		3

#define MB_YES		0x100
#define MB_NO		0x200
#define MB_OK		0x300
#define MB_CANCEL	0x400

int sys_gui_messagebox(const String &title, const String &text, int buttons);

#endif
