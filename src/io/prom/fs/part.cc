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

#include "debug/tracers.h"
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

/***/
struct FDiskInstantiateBootFilePrivData {
	FileOfs mStart;
	FileOfs mLen;
};

static File *FDiskInstantiateBootFile(File *aDevice, void *priv)
{
	FDiskInstantiateBootFilePrivData *p = (FDiskInstantiateBootFilePrivData*)priv;
	return new CroppedFile(aDevice, true, p->mStart, p->mLen);
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
	if (aDevice->read(buffer, blocksize) != blocksize) throw Exception();

	IO_PROM_FS_TRACE("# boot head sect cyl. type head sect cyl. start size\n"); 
//	IO_PROM_FS_TRACE("1  12   12   12   12   12   12   12   12  1234  1234\n"); 

	for (int partition = 1; partition <= 4; partition++, fdisk_part++) {
		createHostStructx(fdisk_part, sizeof *fdisk_part, FDiskPartition_struct, little_endian);

		IO_PROM_FS_TRACE("%d  %02x   %02x   %02x   %02x   %02x   %02x   %02x   %02x  %04lx  %04lx\n",
			partition,
			fdisk_part->boot_ind,
			fdisk_part->head, fdisk_part->sector, fdisk_part->cyl,
			fdisk_part->sys_ind,
			fdisk_part->end_head, fdisk_part->end_sector, fdisk_part->end_cyl,
			fdisk_part->start, fdisk_part->size);
			
		// FIXME: add extended partition support here
	
		char *type;
		switch (fdisk_part->sys_ind) {
		
		case 0x00:
			type = NULL;
			break;
		case LINUX_PARTITION:
		case LINUX_NATIVE:
			type = "Linux";
			break;
		case LINUX_SWAP:
			type = "swap";
			break;
		default:
			IO_PROM_FS_TRACE("Found unknown partition type: %02x\n",fdisk_part->sys_ind);
			type = "unknown";
			break;
		}
		
		char name[20];
		ht_snprintf(name, sizeof name, "partition %d", partition);

		if (type != NULL) {
			PartitionEntry *partEnt = addPartition(partition, name, type, 
				fdisk_part->start * blocksize, 
				fdisk_part->size * blocksize);

			if (fdisk_part->boot_ind & ACTIVE_FLAG)
			{
				partEnt->mBootMethod = BM_chrp;
				
				FDiskInstantiateBootFilePrivData *priv =
					(FDiskInstantiateBootFilePrivData*)
					malloc(sizeof (FDiskInstantiateBootFilePrivData));
				priv->mStart = fdisk_part->start * blocksize;
				priv->mLen = fdisk_part->size * blocksize;

				partEnt->mInstantiateBootFile = FDiskInstantiateBootFile;
				partEnt->mInstantiateBootFilePrivData = priv;
				partEnt->mInstantiateFileSystem = NULL;
			}
		}
	}
}

void PartitionMapFDisk::getType(String &result)
{
	result = "fdisk";
}

PartitionMapFDiskSingle::PartitionMapFDiskSingle(File *aDevice, uint aDeviceBlocksize, const char *type)
	:PartitionMap(aDevice, aDeviceBlocksize)
{
	uint64 offset = 0;
	uint64 length = aDevice->getSize();

	PartitionEntry *partEnt = addPartition(1, "unknown", type, offset, length);

	FDiskInstantiateBootFilePrivData *priv =
		(FDiskInstantiateBootFilePrivData*)
		malloc(sizeof (FDiskInstantiateBootFilePrivData));
	priv->mStart = offset;
	priv->mLen = length;

	partEnt->mBootMethod = BM_chrp;
	partEnt->mInstantiateBootFile = FDiskInstantiateBootFile;
	partEnt->mInstantiateBootFilePrivData = priv;
	partEnt->mInstantiateFileSystem = NULL;

}

void PartitionMapFDiskSingle::getType(String &result)
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
	if (aDevice->read(buffer, blocksize) != blocksize) throw Exception();

	createHostStructx(apple_part, sizeof *apple_part, ApplePartition_struct, big_endian);

	if (apple_part->signature != APPLE_DRIVER_MAGIC) throw Exception();

	IO_PROM_FS_TRACE("New Apple partition map, (physical) blocksize %d/0x%08x\n", blocksize, blocksize);
	int map_size = 1;
	IO_PROM_FS_TRACE("name             type             status   start    +data    datasize +boot    bootsize bootload bootentry\n"); 
//	IO_PROM_FS_TRACE("0123456789123456 0123456789123456 12345678 12345678 12345678 12345678 12345678 12345678 12345678 12345678\n");
	for (int block = 1; block < map_size + 1; block++) {
		aDevice->seek(block*blocksize);
		if (aDevice->read(buffer, blocksize) != blocksize) continue;

		createHostStructx(apple_part, sizeof *apple_part, ApplePartition_struct, big_endian);

		if (apple_part->signature != APPLE_PARTITION_MAGIC) throw Exception();

		if (block == 1) map_size = apple_part->map_count;

		PartitionEntry *partEnt = addPartition(block, apple_part->name, apple_part->type, 
			(apple_part->start_block+apple_part->data_start) * blocksize, 
			apple_part->data_count * blocksize);

		IO_PROM_FS_TRACE("%-16s %-16s %08x %08x %08x %08x %08x %08x %08x %08x\n",
			apple_part->name, 
			apple_part->type,
			apple_part->status,
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

PartitionMapAppleSingle::PartitionMapAppleSingle(File *aDevice, uint aDeviceBlocksize, const char *type)
	:PartitionMap(aDevice, aDeviceBlocksize)
{
	uint64 offset = 0;
	uint64 length = aDevice->getSize();

	PartitionEntry *partEnt = addPartition(1, "unknown", type, offset, length);

	if (tryBootHFS(aDevice, aDeviceBlocksize, offset, partEnt))
		;
	else if (tryBootHFSPlus(aDevice, aDeviceBlocksize, offset, partEnt))
		;
}

void PartitionMapAppleSingle::getType(String &result)
{
	result = "apple";
}

/*
 *
 */
PartitionMap *partitions_get_map(File *aDevice, uint aDeviceBlocksize)
{
	byte buffer[IDE_MAX_BLOCK_SIZE];
	byte signature[2];
	uint blocksize = aDeviceBlocksize;
	if (blocksize <= 1) blocksize = 512;
	aDevice->seek(0);
	if (aDevice->read(buffer, blocksize) != blocksize) {
		IO_PROM_FS_TRACE("device read failed while probing partitions\n");
		return new PartitionMap(aDevice, aDeviceBlocksize);
	}
	// look for partition maps
	if (blocksize >= 2 && buffer[0] == 0x45 && buffer[1] == 0x52) {
		IO_PROM_FS_TRACE("this looks like a Apple partition map to me...\n");
	
		try {
			return new PartitionMapApple(aDevice, aDeviceBlocksize);
		} catch (const Exception &e) {
			String s;
			e.reason(s);
			IO_PROM_FS_TRACE("exception probing Apple partitions: %y\n", &s);
		}
	}
	if (blocksize >= 512 && buffer[510] == 0x55 && buffer[511] == 0xaa) {
		IO_PROM_FS_TRACE("this looks like a FDisk partition map to me...\n");
	
		try {
			return new PartitionMapFDisk(aDevice, aDeviceBlocksize);
		} catch (const Exception &e) {
			String s;
			e.reason(s);
			IO_PROM_FS_TRACE("exception probing fdisk partitions: %y\n", &s);
		}
	}
	// look for raw partitions
	aDevice->seek(0x400);
	if (aDevice->read(signature, 2) == 2) {
		if (signature[0] == 0x42 && signature[1] == 0x44) {
			IO_PROM_FS_TRACE("this looks like a single HFS partition to me...\n");
	
			try {
				return new PartitionMapAppleSingle(aDevice, aDeviceBlocksize, "Apple_HFS");
			} catch (const Exception &e) {
				String s;
				e.reason(s);
				IO_PROM_FS_TRACE("exception probing HFS partition: %y\n", &s);
			}
		}
		if (signature[0] == 0x48 && (signature[1] == 0x2b || signature[2] == 0x58)) {
			IO_PROM_FS_TRACE("this looks like a single HFS+ partition to me...\n");

			try {
				return new PartitionMapAppleSingle(aDevice, aDeviceBlocksize, "Apple_HFS");
			} catch (const Exception &e) {
				String s;
				e.reason(s);
				IO_PROM_FS_TRACE("exception probing HFS+ partition: %y\n", &s);
			}
		}
	}
	aDevice->seek(0x438);
	if (aDevice->read(signature, 2) == 2) {
		if (signature[0] == 0x53 && signature[1] == 0xef) {
			IO_PROM_FS_TRACE("this looks like a single ext2 partition to me...\n");
	
			try {
				return new PartitionMapFDiskSingle(aDevice, aDeviceBlocksize, "ext2");
			} catch (const Exception &e) {
				String s;
				e.reason(s);
				IO_PROM_FS_TRACE("exception probing ext2 partition: %y\n", &s);
			}
		}
	}
	
	IO_PROM_FS_TRACE("probe found no partitions in %d bytes block\n",blocksize);
	return new PartitionMap(aDevice, aDeviceBlocksize);
}
