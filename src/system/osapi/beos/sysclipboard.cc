/* 
 *	HT Editor
 *	clipboard.cc - BeOS-specific clipboard functions
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf
 *	Copyright (C) 2004 Francois Revol (revol@free.fr)
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

#include <AppDefs.h>
#include <Clipboard.h>

#include "system/sys.h"

bool sys_write_data_to_native_clipboard(const void *data, int size)
{
	BMessage *clip;
	be_clipboard->Lock();
	be_clipboard->Clear();
	clip = be_clipboard->Data();
	clip->AddData("text/plain", B_MIME_TYPE, data, size);
	be_clipboard->Commit();
	be_clipboard->Unlock();
	return true;
}

int sys_get_native_clipboard_data_size()
{
	return false;
}

bool sys_read_data_from_native_clipboard(void *data, int max_size)
{
	BMessage *clip;
	const void *cdata;
	ssize_t csize;
	be_clipboard->Lock();
	clip = be_clipboard->Data();
	if (clip->FindData("text/plain", B_MIME_TYPE, &cdata, &csize) < B_OK) {
		be_clipboard->Unlock();
		return false;
	}
	be_clipboard->Unlock();
	memcpy(data, cdata, MIN(max_size, csize));
	return true;
}
