/*
 *	PearPC
 *	hfsplus.h
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

#ifndef __HFSPLUS_H__
#define __HFSPLUS_H__

#include "hfsplusglue.h"
#include "part.h"

#define HFSPlusSigWord		MAGIC16("H+")
#define HFSXSigWord		MAGIC16("HX")

bool tryBootHFSPlus(File *aDevice, uint aDeviceBlocksize, FileOfs start, PartitionEntry *partEnt);

/***/
class HFSPlusFileSystem: public FileSystem {
protected:
	hfsplus_devicehandle_s dh;
	void	*hfsplushandle;
public:
			HFSPlusFileSystem(File *device, int partnum /* 1-based */);
	virtual		~HFSPlusFileSystem();
	/* extends FileSystem */
	virtual	File *	open(const String &filename);
	virtual	bool	getBlessedPath(String &blessed);
	/* new */
	virtual	File *	openBootFile();
};

#endif
