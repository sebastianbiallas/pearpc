/*
 *	PearPC
 *	hfs.h
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

#ifndef __HFS_H__
#define __HFS_H__

#include "fs.h"
#include "hfsglue.h"
#include "part.h"

bool tryBootHFS(File *aDevice, uint aDeviceBlocksize, FileOfs start, PartitionEntry *partEnt);

/***/
class HFSFileSystem: public FileSystem {
protected:
	hfs_devicehandle_s dh;
	void	*hfshandle;

public:
			HFSFileSystem(File *device, int partnum /* 1-based */);
	virtual		~HFSFileSystem();
	/* extends FileSystem */
	virtual	File *	open(const String &filename);
	virtual	bool	getBlessedPath(String &blessed);
	/* new */
	virtual	File *	openBootFile();
};

#endif
