/*
 *	PearPC
 *	fs.h
 *
 *	Copyright (C) 2004 Stefan Weyergraf
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

#ifndef __FS_H__
#define __FS_H__

#include "tools/data.h"
#include "tools/stream.h"

class FileSystem: public Object {
protected:
	File *mDevice;
public:
			FileSystem(File *device);
	/* new */
	virtual	File *	open(const String &filename) = 0;
	virtual	bool	getBlessedPath(String &blessed) = 0;
};

#endif
