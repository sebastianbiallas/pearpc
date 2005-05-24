/* 
 *	PearPC
 *	hfs.cc
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
#include "hfs.h"
#include "hfsstruct.h"
#include "hfsplus.h"
#include "tools/debug.h"
#include "tools/endianess.h"
#include "tools/except.h"
#include "tools/snprintf.h"

extern "C" {
#include "hfs/libhfs.h"
#include "hfs/volume.h"
}

byte HFSNodeDescriptor_struct[] = {
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	0
};

byte HFSBTHdrRec_struct[] = {
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	0
};

byte HFSCatKeyRec_struct[] = {
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	// 31 bytes
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,

	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,

	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,

	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,

	0
};

byte HFSCatFileRec_struct[] = {
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	// FInfo
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	//
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	// FXInfo
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	//
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	// data fork extent record
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	// res fork extent record
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	//
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	0
};

byte HFSMDB_struct[] = {
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_8 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
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
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_32 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	STRUCT_ENDIAN_16 | STRUCT_ENDIAN_HOST,
	0
};

/*
 *
 */
static bool doTryBootHFS(const HFSMDB &mdb, File *aDevice, uint aDeviceBlocksize, FileOfs start, PartitionEntry *partEnt);

bool tryBootHFS(File *aDevice, uint aDeviceBlocksize, FileOfs start, PartitionEntry *partEnt)
{
	HFSMDB mdb;
	aDevice->seek(start+0x400);
	if (aDevice->read((byte*)&mdb, sizeof mdb) == sizeof mdb) {
		createHostStructx(&mdb, sizeof mdb, HFSMDB_struct, big_endian);
		if (mdb.drSigWord == HFSSigWord) {
			if (mdb.drEmbedSigWord == HFSPlusSigWord) {
				// HFS+ (embedded in HFS)
				IO_PROM_FS_TRACE("contains HFS volume, embedding a HFS+ volume\n");
				IO_PROM_FS_TRACE("embed.start=%08x, embed.count=%08x, hfsblksz=%08x\n",
					mdb.drEmbedExtent.startBlock, 
					mdb.drEmbedExtent.blockCount,
					mdb.drAlblkSz);
				return tryBootHFSPlus(aDevice, aDeviceBlocksize, start+mdb.drEmbedExtent.startBlock*mdb.drAlblkSz, partEnt);
			} else {
				IO_PROM_FS_TRACE("contains HFS volume\n");
				return doTryBootHFS(mdb, aDevice, aDeviceBlocksize, start, partEnt);
			}
		}
	}
	return false;
}

struct HFSInstantiateBootFilePrivData {
	int mPartNum;
};

static File *HFSInstantiateBootFile(File *aDevice, void *priv)
{
	HFSInstantiateBootFilePrivData *p = (HFSInstantiateBootFilePrivData*)priv;
	HFSFileSystem *fs = new HFSFileSystem(aDevice, p->mPartNum);
	File *f = fs->openBootFile();
	if (!f) delete fs;
	return f;
}

static FileSystem *HFSInstantiateFileSystem(File *aDevice, int partnum)
{
	return new HFSFileSystem(aDevice, partnum);
}

static bool doTryBootHFS(const HFSMDB &mdb, File *aDevice, uint aDeviceBlocksize, FileOfs start, PartitionEntry *partEnt)
{
	if (partEnt->mPartNum <= 0) return false;
	hfs_devicehandle_s dh;
	dh.mDevice = aDevice;
	dh.mStart = 0;
	hfsvol *vol = hfs_mount(&dh, partEnt->mPartNum-1, HFS_MODE_RDONLY);
	if (vol) {
		hfs_umount(vol);
		HFSInstantiateBootFilePrivData *priv = (HFSInstantiateBootFilePrivData*)
			malloc(sizeof (HFSInstantiateBootFilePrivData));
		priv->mPartNum = partEnt->mPartNum;
		partEnt->mInstantiateBootFilePrivData = priv;
		partEnt->mInstantiateBootFile = HFSInstantiateBootFile;
		partEnt->mInstantiateFileSystem = HFSInstantiateFileSystem;
		partEnt->mBootMethod = BM_chrp;
		return true;
	} else IO_PROM_FS_TRACE("couldn't mount HFS partition.\n");
	return false;
}

/***/
class HFSFile: public File {
protected:
	hfsfile *	mFile;
	FileOfs		mCurOfs;
	FileSystem *	mFS;
	bool		mOwnFS;
public:

HFSFile(hfsfile *file, FileSystem *fs, bool own_fs)
{
	mFile = file;
	mCurOfs = 0;
	mFS = fs;
	mOwnFS = own_fs;
}

virtual ~HFSFile()
{
	hfs_close(mFile);
	if (mOwnFS) delete mFS;
}

virtual FileOfs	getSize() const
{
	return mFile->cat.u.fil.filLgLen;
}

virtual uint read(void *buf, uint size)
{
	uint r = hfs_read(mFile, buf, size);
	mCurOfs += r;
	return r;
}

virtual void seek(FileOfs offset)
{
	hfs_seek(mFile, offset, HFS_SEEK_SET);
	mCurOfs = offset;
}

virtual FileOfs tell() const
{
	return mCurOfs;
}

};

/***/
HFSFileSystem::HFSFileSystem(File *device, int partnum)
: FileSystem(device)
{
	dh.mDevice = mDevice;
	dh.mStart = 0;
	hfshandle = hfs_mount(&dh, partnum-1, HFS_MODE_RDONLY);
	if (!hfshandle) throw MsgException("couldn't mount HFS file system");
}

HFSFileSystem::~HFSFileSystem()
{
	hfs_umount((hfsvol*)hfshandle);
}

File *HFSFileSystem::open(const String &filename)
{
	hfsfile *f = hfs_open((hfsvol*)hfshandle, filename.contentChar());
	if (!f) return NULL;
	return new HFSFile(f, this, false);
}

// WARNING: on success this will bind this filesystem to the file (mOwnFS)
File *HFSFileSystem::openBootFile()
{
	hfsvol *vol = (hfsvol*)hfshandle;
	uint startupFolderID = vol->mdb.drFndrInfo[0];
	hfsdir *dir = hfs_opendir_by_id(vol, startupFolderID);
	if (!dir) {
		IO_PROM_FS_TRACE("couldn't get Startup Folder of HFS partition.\n");
		return NULL;
	}
	hfsdirent dirent;
	while (hfs_readdir(dir, &dirent) == 0) {
//		IO_PROM_FS_TRACE("%-4s %08x %s\n", dirent.u.file.type, dirent.cnid, dirent.name);
		if (strcmp(dirent.u.file.type, "tbxi") == 0) {
			hfsfile *file = hfs_open_by_dirent(vol, &dirent);
			if (file) {
/*				IO_PROM_FS_TRACE("got Startup FILE!\n");
				char buf[32];
				hfs_read(file, buf, 32);
				buf[31] = 0;
				IO_PROM_FS_TRACE("%s\n", buf);*/
				return new HFSFile(file, this, true);
			}
		}
	}
	IO_PROM_FS_TRACE("couldn't find a Startup File in the Startup Folder of the HFS partition.\n");
	return NULL;
}

bool HFSFileSystem::getBlessedPath(String &blessed)
{
	hfsvol *vol = (hfsvol*)hfshandle;
	uint startupFolderID = vol->mdb.drFndrInfo[0];
	uint id = startupFolderID;
	uint maxit = 200;
	blessed.assign("/");
	while ((id != HFS_CNID_ROOTDIR) && maxit--) {
		CatDataRec dthd;
	        if (v_getdthread(vol, id, &dthd, NULL) <= 0) return false;
		String p(dthd.u.dthd.thdCName);
		blessed.prepend(p);
		blessed.prepend("/");
		id = dthd.u.dthd.thdParID;
	}
	return (id == HFS_CNID_ROOTDIR);
}
