/* 
 *	PearPC
 *	hfsplus.cc
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

#include <string.h>

#include "debug/tracers.h"
#include "hfsplus.h"
#include "tools/endianess.h"
#include "tools/except.h"
#include "tools/snprintf.h"

#include "hfs.h"

extern "C" {
#include "hfsplus/libhfsp.h"
#include "hfsplus/volume.h"
#include "hfsplus/btree.h"
#include "hfsplus/blockiter.h"
#include "hfsplus/record.h"
#include "hfsplus/unicode.h"
#include "hfsplus/os.h"
}

typedef uint32	HFSCatalogNodeID;

struct HFSPlusExtentDescriptor {
        uint32                  startBlock PACKED;
	uint32                  blockCount PACKED;
};

struct HFSPlusForkData {
	uint64                  logicalSize PACKED;
        uint32                  clumpSize PACKED;
        uint32                  totalBlocks PACKED;
	HFSPlusExtentDescriptor	extents[8] PACKED;
};

struct HFSPlusVolumeHeader {
	uint16		signature PACKED;
	uint16		version PACKED;
	uint32		attributes PACKED;
	uint32              lastMountedVersion PACKED;
	uint32              journalInfoBlock PACKED;

	uint32              createDate PACKED;
	uint32              modifyDate PACKED;
	uint32              backupDate PACKED;
        uint32              checkedDate PACKED;

	uint32              fileCount PACKED;
        uint32              folderCount PACKED;

	uint32              blockSize PACKED;
        uint32              totalBlocks PACKED;
	uint32              freeBlocks PACKED;

        uint32              nextAllocation PACKED;
	uint32              rsrcClumpSize PACKED;
	uint32              dataClumpSize PACKED;
        HFSCatalogNodeID    nextCatalogID PACKED;

	uint32              writeCount PACKED;
        uint64              encodingsBitmap PACKED;

	uint32              finderInfo[8] PACKED;

        HFSPlusForkData     allocationFile PACKED;
	HFSPlusForkData     extentsFile PACKED;
        HFSPlusForkData     catalogFile PACKED;
	HFSPlusForkData     attributesFile PACKED;
        HFSPlusForkData     startupFile PACKED;
};

byte HFSPlusVolumeHeader_struct[]= {
	   STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
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
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_64 | STRUCT_ENDIAN_HOST,

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	    // 5x forkdata
	    // 1.
	   STRUCT_ENDIAN_64 | STRUCT_ENDIAN_HOST,
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

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	    // 2.
	   STRUCT_ENDIAN_64 | STRUCT_ENDIAN_HOST,
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

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	    // 3.
	   STRUCT_ENDIAN_64 | STRUCT_ENDIAN_HOST,
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

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	    // 4.
	   STRUCT_ENDIAN_64 | STRUCT_ENDIAN_HOST,
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

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	    // 5.
	   STRUCT_ENDIAN_64 | STRUCT_ENDIAN_HOST,
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

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,

	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	   STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,

	   0,
};

/*
 *
 */
static bool doTryBootHFSPlus(const HFSPlusVolumeHeader &vh, File *aDevice, uint aDeviceBlocksize, FileOfs start, PartitionEntry *partEnt);

bool tryBootHFSPlus(File *aDevice, uint aDeviceBlocksize, FileOfs start, PartitionEntry *partEnt)
{
	// HFS+
	HFSPlusVolumeHeader HP_VH;
	aDevice->seek(start+0x400);
	if (aDevice->read((byte*)&HP_VH, sizeof HP_VH) == sizeof HP_VH) {
		createHostStructx(&HP_VH, sizeof HP_VH, HFSPlusVolumeHeader_struct, big_endian);
		if ((HP_VH.signature == HFSPlusSigWord) || (HP_VH.signature == HFSXSigWord)) {
			IO_PROM_FS_TRACE("contains HFS+/HFSX volume (startup file size %08x, total blocks %d)\n", HP_VH.startupFile.logicalSize, HP_VH.startupFile.totalBlocks);
			IO_PROM_FS_TRACE("finderinfo[0]=%08x\n", HP_VH.finderInfo[0]);
			return doTryBootHFSPlus(HP_VH, aDevice, aDeviceBlocksize, start, partEnt);
		}
	}
	return false;
}

static int my_ffs(uint64 f)
{
	for(int i=1; i<64; i++) {
		if (f&1) return i;
		f >>= 1;
	}
	return 0;
}

struct HFSPlusInstantiateBootFilePrivData {
	int mPartNum;
};

static File *HFSPlusInstantiateBootFile(File *aDevice, void *priv)
{
	HFSPlusInstantiateBootFilePrivData *p = (HFSPlusInstantiateBootFilePrivData*)priv;
	HFSPlusFileSystem *fs = new HFSPlusFileSystem(aDevice, p->mPartNum);
	File *f = fs->openBootFile();
	if (!f) delete fs; // otherwise fs is owned by f
	return f;
}

static FileSystem *HFSPlusInstantiateFileSystem(File *aDevice, int partnum)
{
	return new HFSPlusFileSystem(aDevice, partnum);
}

static bool doTryBootHFSPlus(const HFSPlusVolumeHeader &vh, File *aDevice, uint aDeviceBlocksize, FileOfs start, PartitionEntry *partEnt)
{
	if (partEnt->mPartNum <= 0) return false;
	hfsplus_devicehandle_s dh;
	dh.mDevice = aDevice;
	dh.mStart = 0;
	volume vol;
	ht_printf("start: %qd\n", start);
	if (volume_open(&vol, &dh, partEnt->mPartNum-1, HFSP_MODE_RDONLY) == 0) {
		volume_close(&vol);
		HFSPlusInstantiateBootFilePrivData *priv = (HFSPlusInstantiateBootFilePrivData*)
			malloc(sizeof (HFSPlusInstantiateBootFilePrivData));
		priv->mPartNum = partEnt->mPartNum;
		partEnt->mInstantiateBootFilePrivData = priv;
		partEnt->mInstantiateBootFile = HFSPlusInstantiateBootFile;
		partEnt->mInstantiateFileSystem = HFSPlusInstantiateFileSystem;
		partEnt->mBootMethod = BM_chrp;
		return true;
	} else IO_PROM_FS_TRACE("couldn't mount HFS+ partition.\n");
	return false;
}

/***/
class HFSPlusFile: public File {
protected:
	byte *block;
	int blocksize;
	int blocksize_bits;
	int blockfill;
	int blockpos;
	FileOfs blockofs;
	blockiter it;
	volume *		vol;
	hfsp_cat_file		mFile;
	FileOfs			mCurOfs;
	FileSystem *		mFS;
	bool			mOwnFS;
public:

HFSPlusFile(volume *aVol, hfsp_cat_file aFile, FileSystem *fs, bool own_fs)
{
	vol = aVol;
	mFile = aFile;
	mOwnFS = own_fs;
	mFS = fs;

	blockiter_init(&it, vol, &mFile.data_fork, HFSP_EXTENT_DATA, mFile.id);
	blockpos = 0;
	blockfill = 0;
	blockofs = 0;

	block = (byte*)malloc(vol->vol.blocksize);
	blocksize = vol->vol.blocksize;
	blocksize_bits = my_ffs(vol->vol.blocksize)-1;
	if (vol->vol.blocksize != (1 << blocksize_bits))
		throw new MsgfException("invalid blocksize: %d (not a power of 2)",
			vol->vol.blocksize);
}

virtual ~HFSPlusFile()
{
	if (mOwnFS) delete mFS;
}

virtual FileOfs	getSize() const
{
	return mFile.data_fork.total_size;
}

virtual uint read(void *buf, uint n)
{
	uint on = n;
	byte *b = (byte*)buf;
	while (n) {
		if (blockfill == 0) {
			hfsp_os_seek(&vol->fd, blockiter_curr(&it), blocksize_bits);
			if (hfsp_os_read(&vol->fd, block, 1, blocksize_bits) != 1) break;
			blockfill = blocksize;
			blockpos = 0;
		} else if (blockpos == blockfill) {
			if (blockiter_next(&it) != 0) break;
			blockofs++;
			blockfill = 0;
			continue;
		}
		uint c = blockfill-blockpos;
		uint t = n;
		if (t > c) t = c;
		memcpy(b, block+blockpos, t);
		blockpos += t;
		b += t;
		n -= t;
	}
	return on - n;
}

virtual void seek(FileOfs offset)
{
	uint boffset = offset / blocksize;
	blockiter_init(&it, vol, &mFile.data_fork, HFSP_EXTENT_DATA, mFile.id);
	if (boffset && (blockiter_skip(&it, boffset) != 0))
		throw new IOException(EIO);
	blockofs = boffset;
	hfsp_os_seek(&vol->fd, blockiter_curr(&it), blocksize_bits);
	if (hfsp_os_read(&vol->fd, block, 1, blocksize_bits) != 1) throw new IOException(ENOSYS);
	blockfill = blocksize;
	blockpos = offset % blocksize;
}

virtual FileOfs tell() const
{
	return blockofs * blocksize + blockpos;
}

};

/***/
HFSPlusFileSystem::HFSPlusFileSystem(File *device, int partnum)
: FileSystem(device)
{
	dh.mDevice = mDevice;
	dh.mStart = 0;
	hfsplushandle = malloc(sizeof (volume));
	volume *vol = (volume*)hfsplushandle;
	if (volume_open(vol, &dh, partnum-1, HFSP_MODE_RDONLY) != 0)
		throw new MsgException("can't open HFS+ volume");
}

HFSPlusFileSystem::~HFSPlusFileSystem()
{
	volume *vol = (volume*)hfsplushandle;
	volume_close(vol);
	free(hfsplushandle);
}

File *HFSPlusFileSystem::open(const String &filename)
{
/*	hfsfile *f = hfs_open((hfsvol*)hfshandle, filename.contentChar());
	if (!f) return NULL;
	return new HFSPlusFile(f, this, false);*/
	return NULL;
}

// WARNING: on success this will bind this filesystem to the file (mOwnFS)
File *HFSPlusFileSystem::openBootFile()
{
	volume *vol = (volume*)hfsplushandle;
	uint id = createHostInt(vol->vol.finder_info, 4, big_endian);
	record startupFolderRec;
	if (record_init_cnid(&startupFolderRec, &vol->catalog, id) != 0) return NULL;
	if (startupFolderRec.record.type != HFSP_FOLDER_THREAD) return NULL;

	record rec;
	if (record_init_parent(&rec, &startupFolderRec) == 0) do {
		switch (rec.record.type) {
		case HFSP_FOLDER: {
//			char buf[256];
//			unicode_uni2asc(buf, &rec.key.name, sizeof buf);
//			IO_PROM_FS_TRACE("folder: %s\n", buf);
			break;
		}
		case HFSP_FILE: {
			char buf[256];
			unicode_uni2asc(buf, &rec.key.name, sizeof buf);
			OSType t = rec.record.u.file.user_info.fdType;
			char t2[5];
			t2[0] = t >> 24;
			t2[1] = t >> 16;
			t2[2] = t >> 8;
			t2[3] = t;
			t2[4] = 0;
/*			OSType c = rec.record.u.file.user_info.fdCreator;
			char c2[5];
			c2[0] = c >> 24;
			c2[1] = c >> 16;
			c2[2] = c >> 8;
			c2[3] = c;
			c2[4] = 0;*/
			if (strcmp(t2, "tbxi") == 0) {
				return new HFSPlusFile(vol, rec.record.u.file, this, true);
			}
//			IO_PROM_FS_TRACE("file: %4s/%4s %s\n", t2, c2, buf);
			break;
		}
		case HFSP_FOLDER_THREAD: {
//			char buf[256];
//			unicode_uni2asc(buf, &rec.record.u.thread.nodeName, sizeof buf);
//			IO_PROM_FS_TRACE("folder thread: %s\n", buf);
			break;
		}
		case HFSP_FILE_THREAD: {
//			IO_PROM_FS_TRACE("file thread\n");
			break;
		}
		}
	} while (record_next(&rec) == 0);

	return NULL;
}

bool HFSPlusFileSystem::getBlessedPath(String &blessed)
{
	volume *vol = (volume*)hfsplushandle;
	uint startupFolderID = createHostInt(vol->vol.finder_info, 4, big_endian);

	uint id = startupFolderID;
	uint maxit = 200;
	blessed.assign("/");
	while ((id != HFSP_ROOT_CNID) && maxit--) {
		record rec;
		if (record_init_cnid(&rec, &vol->catalog, id) != 0) return false;
		if (rec.record.type != HFSP_FOLDER_THREAD) return false;

		char buf[256];
		unicode_uni2asc(buf, &rec.record.u.thread.nodeName, sizeof buf);
		String p(buf);
		blessed.prepend(p);
		blessed.prepend("/");
		id = rec.record.u.thread.parentID;
	}
	return (id == HFSP_ROOT_CNID);
}
