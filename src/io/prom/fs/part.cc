/* 
 *	PearPC
 *	part.cc
 *
 *	Copyright (C) 2003 Sebastian Biallas (sb@biallas.net)
 *
 *	Some ideas and code from yaboot/fs.c 
 *	Copyright Ethan Beson and Benjamin Herrenschmidt
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
// for HFS+ info:
// http://developer.apple.com/technotes/tn/tn1150.html
//
// for HFS-MDB info:
// http://developer.apple.com/documentation/Carbon/Reference/File_Manager/file_manager/data_type_62.html

#include <cstring>

#include "tools/endianess.h"
#include "tools/snprintf.h"
#include "tools/except.h"
#include "part.h"
#include "hfs.h"
#include "hfsplus.h"

#define APPLE_PARTITION_STATUS_BOOTABLE	8

#define APPLE_DRIVER_MAGIC	MAGIC16("ER")
#define APPLE_PARTITION_MAGIC	MAGIC16("PM")

struct ApplePartition {
    uint16	signature;
    uint16	res;
    uint32	map_count;	
    uint32	start_block;
    uint32	block_count;
    char	name[32];
    char	type[32];
    uint32	data_start;
    uint32	data_count;
    uint32	status;
    uint32	boot_start;
    uint32	boot_size;
    uint32	boot_load;
    uint32	boot_load2;
    uint32	boot_entry;
    uint32	boot_entry2;
    uint32	boot_cksum;
    char	processor[16];
};

byte ApplePartition_struct[]= {
	   STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,

	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 

	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
    	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,

	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   0,
};

/*
 *
 */
#define ACTIVE_FLAG     0x80

#define EXTENDED        0x05
#define LINUX_PARTITION 0x81
#define LINUX_SWAP      0x82
#define LINUX_NATIVE    0x83
#define LINUX_EXTENDED  0x85

struct FDiskPartition {
	uint8 boot_ind;
	uint8 head;
	uint8 sector;
	uint8 cyl;
	uint8 sys_ind;
	uint8 end_head;
	uint8 end_sector;
	uint8 end_cyl;
	uint32 start;
	uint32 size;
};

byte FDiskPartition_struct[] = {
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, STRUCT_ENDIAN_8, 
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   0,
};

/*
 *
 */
PartitionEntry::PartitionEntry(int partnum, const char *aName, const char *aType, uint64 aOffset,
	uint64 aLength)
{
	mPartNum = partnum;
	mName = strdup(aName);
	mType = strdup(aType);
	mOffset = aOffset;
	mLength = aLength;
	mBootMethod = BM_none;
	mInstantiateBootFile = NULL;
	mInstantiateBootFilePrivData = NULL;
	mInstantiateFileSystem = NULL;
	mBootImageLoadAddr = 0;
	mBootImageEntrypoint = 0;
}

PartitionEntry::~PartitionEntry()
{
	if (mInstantiateBootFilePrivData) free(mInstantiateBootFilePrivData);
	free(mName);
	free(mType);
}

/*
 *
 */
PartitionMap::PartitionMap(File *aDevice, uint aDeviceBlocksize)
{
	mEntries = new Array(true);
	mDevice = aDevice;
	mDeviceBlocksize = aDeviceBlocksize;
	addPartition(-1, "raw disk", "raw", 0, 0);
}

PartitionMap::~PartitionMap()
{
	delete mEntries;
}

Enumerator *PartitionMap::getPartitions()
{
	return mEntries;
}

void PartitionMap::getType(String &result)
{
	result = "raw";
}

PartitionEntry *PartitionMap::addPartition(int partnum, const char *name, const char *type, uint64 offset, uint64 length)
{
	PartitionEntry *e = new PartitionEntry(partnum, name, type, offset, length);
	mEntries->insert(e);
	return e;
}

/*
 *
 */
PartitionMapFDisk::PartitionMapFDisk(File *aDevice, uint aDeviceBlocksize)
	:PartitionMap(aDevice, aDeviceBlocksize)
{
	byte buffer[IDE_MAX_BLOCK_SIZE];
	FDiskPartition *fdisk_part = (FDiskPartition *)&buffer[0x1be];

	uint blocksize = aDeviceBlocksize;
	if (blocksize <= 1) blocksize = 512;
	aDevice->seek(0);
	if (aDevice->read(buffer, blocksize) != blocksize) throw new Exception();

	for (int partition = 1; partition <= 4; partition++, fdisk_part++) {
		createHostStructx(fdisk_part, sizeof *fdisk_part, FDiskPartition_struct, little_endian);
		
		// FIXME: add extended partition support here
		
		char *type;
		switch (fdisk_part->sys_ind) {
		case LINUX_NATIVE:
			type = "Linux";
			break;
		default:
			type = "unknown";
			break;
		}
		
		char name[20];
		ht_snprintf(name, sizeof name, "partition %d", partition);
		
		addPartition(partition, name, type, 
			fdisk_part->start * blocksize, 
			fdisk_part->size * blocksize);
	}
}

void PartitionMapFDisk::getType(String &result)
{
	result = "fdisk";
}

/***/
struct RawInstantiateBootFilePrivData {
	FileOfs mStart;
	FileOfs mLen;
};

static File *RawInstantiateBootFile(File *aDevice, void *priv)
{
	RawInstantiateBootFilePrivData *p = (RawInstantiateBootFilePrivData*)priv;
	return new CroppedFile(aDevice, true, p->mStart, p->mLen);
}

/***/
PartitionMapApple::PartitionMapApple(File *aDevice, uint aDeviceBlocksize)
	:PartitionMap(aDevice, aDeviceBlocksize)
{
	byte buffer[IDE_MAX_BLOCK_SIZE];
	ApplePartition *apple_part = (ApplePartition *)&buffer;
	uint blocksize = mDeviceBlocksize;
	/*if (blocksize <= 1)*/ blocksize = 512;
	aDevice->seek(0);
	if (aDevice->read(buffer, blocksize) != blocksize) throw new Exception();

	createHostStructx(apple_part, sizeof *apple_part, ApplePartition_struct, big_endian);

	if (apple_part->signature != APPLE_DRIVER_MAGIC) throw new Exception();

	ht_fprintf(stderr, "New Apple partition map, (physical) blocksize %d/0x%08x\n", blocksize, blocksize);
	int map_size = 1;
	ht_fprintf(stderr, "name             status   start    +data    datasize +boot    bootsize bootload bootentry\n"); 
//	ht_fprintf(stderr, "0123456789123456 12345678 12345678 12345678 12345678 12345678 12345678 12345678 12345678\n");
	for (int block = 1; block < map_size + 1; block++) {
		aDevice->seek(block*blocksize);
		if (aDevice->read(buffer, blocksize) != blocksize) continue;

		createHostStructx(apple_part, sizeof *apple_part, ApplePartition_struct, big_endian);

		if (apple_part->signature != APPLE_PARTITION_MAGIC) throw new Exception();

		if (block == 1) map_size = apple_part->map_count;

		PartitionEntry *partEnt = addPartition(block, apple_part->name, apple_part->type, 
			(apple_part->start_block+apple_part->data_start) * blocksize, 
			apple_part->data_count * blocksize);
		ht_fprintf(stderr, "%-16s %08x %08x %08x %08x %08x %08x %08x %08x\n",
			apple_part->name, apple_part->status,
			apple_part->start_block*blocksize,
			apple_part->data_start*blocksize,
			apple_part->data_count*blocksize,
			apple_part->boot_start*blocksize,
			apple_part->boot_size,
			apple_part->boot_load,
			apple_part->boot_entry);

		// Try HFS
		if (tryBootHFS(aDevice, blocksize, apple_part->start_block*blocksize, partEnt))
			continue;
		// Try HFS+
		if (tryBootHFSPlus(aDevice, blocksize, apple_part->start_block*blocksize, partEnt))
			continue;
		// Try boot via partition info.
		if (apple_part->boot_size && apple_part->boot_load && apple_part->boot_entry) {
			partEnt->mBootMethod = BM_direct;

			RawInstantiateBootFilePrivData *priv =
				(RawInstantiateBootFilePrivData*)
				malloc(sizeof (RawInstantiateBootFilePrivData));
			priv->mStart = apple_part->start_block*blocksize
				+ apple_part->boot_start*blocksize;
			priv->mLen = apple_part->boot_size;

			partEnt->mInstantiateBootFile = RawInstantiateBootFile;
			partEnt->mInstantiateBootFilePrivData = priv;
			partEnt->mInstantiateFileSystem = NULL;

			partEnt->mBootImageLoadAddr = apple_part->boot_load;
			partEnt->mBootImageEntrypoint = apple_part->boot_entry;
			continue;
		}
	}
}

void PartitionMapApple::getType(String &result)
{
	result = "apple";
}

/*
 *
 */
PartitionMap *partitions_get_map(File *aDevice, uint aDeviceBlocksize)
{
	byte buffer[IDE_MAX_BLOCK_SIZE];
	uint blocksize = aDeviceBlocksize;
	if (blocksize <= 1) blocksize = 512;
	aDevice->seek(0);
	if (aDevice->read(buffer, blocksize) != blocksize) {
		return new PartitionMap(aDevice, aDeviceBlocksize);
	}
	if (blocksize >= 2 && buffer[0] == 0x45 && buffer[1] == 0x52) {
		try {
			return new PartitionMapApple(aDevice, aDeviceBlocksize);
		} catch (Exception *e) {
			String s;
			ht_fprintf(stderr, "exception probing Apple partitions: %y\n", &e->reason(s));
			delete e;
		}
	}
	if (blocksize >= 512 && buffer[510] == 0x55 && buffer[511] == 0xaa) {
		try {
			return new PartitionMapFDisk(aDevice, aDeviceBlocksize);
		} catch (Exception *e) {
			String s;
			ht_fprintf(stderr, "exception probing fdisk partitions: %y\n", &e->reason(s));
			delete e;
		}
	}
	return new PartitionMap(aDevice, aDeviceBlocksize);
}
