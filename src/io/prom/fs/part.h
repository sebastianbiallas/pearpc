/*
 *	PearPC
 *	part.h
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

#ifndef __PART_H__
#define __PART_H__

#include "tools/data.h"
#include "io/ide/idedevice.h"
#include "system/types.h"
#include "fs.h"

enum BootMethod {
	BM_none,
	BM_direct,
	BM_chrp
};

typedef File *(*InstantiateBootFile)(File *aDevice, void *privData);
typedef FileSystem *(*InstantiateFileSystem)(File *aDevice, int partnum);

class PartitionEntry: public Object {
public:
	int mPartNum;
	char *mName;
	char *mType;
	FileOfs mOffset;
	FileOfs mLength;

	BootMethod		mBootMethod;
	InstantiateBootFile	mInstantiateBootFile;
	void *			mInstantiateBootFilePrivData;
	InstantiateFileSystem	mInstantiateFileSystem;
	// mMethod == BM_direct only
	uint64			mBootImageLoadAddr;
	uint64			mBootImageEntrypoint;

			PartitionEntry(int partnum, const char *aName, const char *aType,
				uint64 aOffset, uint64 aLength);
	virtual		~PartitionEntry();
};

class PartitionMap: public Object {
protected:
	Array *	mEntries;
	File *	mDevice;
	uint	mDeviceBlocksize;
public:
			PartitionMap(File *aDevice, uint aDeviceBlocksize);
	virtual		~PartitionMap();

		Enumerator *getPartitions();
	virtual void	getType(String &result);
protected:
		PartitionEntry *addPartition(int partnum, const char *name, const char *type, uint64 offset, uint64 length);
};

class PartitionMapFDisk: public PartitionMap {
public:
			PartitionMapFDisk(File *aDevice, uint aDeviceBlocksize);
	virtual void	getType(String &result);
};

class PartitionMapFDiskSingle: public PartitionMap {
public:
			PartitionMapFDiskSingle(File *aDevice, uint aDeviceBlocksize, const char *type);
	virtual void	getType(String &result);
};

class PartitionMapApple: public PartitionMap {
public:
			PartitionMapApple(File *aDevice, uint aDeviceBlocksize);
	virtual void	getType(String &result);
};

class PartitionMapAppleSingle: public PartitionMap {
public:
			PartitionMapAppleSingle(File *aDevice, uint aDeviceBlocksize, const char *type);
	virtual void	getType(String &result);
};

PartitionMap *partitions_get_map(File *aDevice, uint aDeviceBlocksize);

#endif
