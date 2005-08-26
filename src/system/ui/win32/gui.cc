/*
 *	PearPC
 *	gui.cc
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

#include <cstdlib>

#include "system/ui/gui.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>

void sys_gui_init()
{
}

bool sys_gui_open_file_dialog(String &ret, const String &title, const String &filespec, const String &filespecname, const String &home, bool existing)
{
	char *filename = (char*)malloc(1024);
	filename[0] = 0;
	OPENFILENAME ofn;
	ofn.lStructSize = sizeof ofn;
//	ofn.hwndOwner = hwndDlg;
	ofn.hwndOwner = NULL;
	ofn.hInstance = 0;
	String filter(filespecname);
	filter.appendChar(0);
	filter += filespec;
	filter.appendChar(0);
	filter.appendChar(0);
	ofn.lpstrFilter = filter.contentChar();
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = 1024;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = title.contentChar();
	ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | (existing ?  OFN_FILEMUSTEXIST : 0);
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;
	if (GetOpenFileName(&ofn)) {
		ret.assign(filename);
		free(filename);
		return true;
	}
	free(filename);
	return false;
}

int sys_gui_messagebox(const String &title, const String &text, int buttons)
{
	
}
