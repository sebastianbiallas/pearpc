/* 
 *	PearPC
 *	clipboard.cc - Win32-specific (windows-)clipboard functions
 *
 *	Copyright (C) 1999-2003 Sebastian Biallas (sb@biallas.net)
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
bool sys_write_data_to_native_clipboard(const void *data, int size)
{
	// FIXME:
	if (!OpenClipboard(0)) return false;
	HGLOBAL hdata;
	hdata = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, size);
	if (hdata) {
		void *ptr = GlobalLock(hdata);
		memmove(ptr, data, size);
		GlobalUnlock(hdata);
		if (SetClipboardData(CF_OEMTEXT, hdata)) {
			CloseClipboard();
			return true;
		}
	}
	CloseClipboard();
	return false;
}

int sys_get_native_clipboard_data_size()
{
	return 0;
}

bool sys_read_data_from_native_clipboard(void *data, int max_size)
{
	if (!OpenClipboard(0)) return false;
	HANDLE hdata = GetClipboardData(CF_OEMTEXT);
	if (!hdata) {        	
		CloseClipboard();
		return false;
	}
	int size = GlobalSize(hdata);
	void *ptr = GlobalLock(hdata);
	memmove(data, ptr, MIN(size, max_size));
	GlobalUnlock(hdata);
	CloseClipboard();
	return true;
}

