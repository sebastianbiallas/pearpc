/*
 *	PearPC
 *	cd.h
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

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "idedevice.h"
#include "tools/snprintf.h"
#include "tools/except.h"

/*
 *
 */
class IDEDeviceFile: public File {
protected:
	IDEDevice &id;
	FileOfs tel;
public:

IDEDeviceFile(IDEDevice &aid) : id(aid)
{
	tel = 0;
}

virtual FileOfs getSize() const
{
	return id.getBlockSize() * id.getBlockCount();
}

virtual void seek(FileOfs offset)
{
	if (!id.promSeek(offset)) throw new IOException(EIO);
	tel = offset;
}

virtual FileOfs	tell() const
{
	return tel;
}

virtual uint read(void *buf, uint size)
{
	uint r = id.promRead((byte*)buf, size);
	tel += r;
	return r;
}

virtual String &getDesc(String &result) const
{
	result.assignFormat("%y", &id);
	return result;
}

};

/*
 *
 */
IDEDevice::IDEDevice(const char *name)
{
	mAcquired = false;
	mSectorFirst = 0;
	mError = NULL;
	sys_create_mutex(&mMutex);
	mName = strdup(name);
}

IDEDevice::~IDEDevice()
{
	if (mError) free(mError);
}

bool IDEDevice::acquire()
{
	if (mAcquired) {
		printf("attempt to reacquire IDEDevice\n");
		exit(-1);
	}
	mAcquired = true;	
	mSectorFirst = 0; // acquire clears deblocking
	sys_lock_mutex(mMutex);
	return true;
}

bool IDEDevice::release()
{
	if (mAcquired) {
		sys_unlock_mutex(mMutex);
		mAcquired = false;
		return true;
	} else {
		return false;
	}
}

void IDEDevice::setMode(int aMode, int aSectorSize)
{
	mMode = aMode;
	mSectorSize = aSectorSize;
}

char *IDEDevice::getError()
{
	return mError;
}

void IDEDevice::setError(const char *error)
{
	if (mError) free(mError);
	mError = strdup(error);
}

int IDEDevice::read(byte *buf, int size)
{
	byte *oldbuf = buf;
	if (mSectorFirst && mSectorFirst < mSectorSize) {
		int copy = MIN(mSectorSize-mSectorFirst, size);
		memcpy(buf, mSector+mSectorFirst, copy);
		size -= copy;
		buf += copy;
		mSectorFirst += copy;
	}
	if (size > 0) {
		while (size >= mSectorSize) {
			readBlock(buf);
			size -= mSectorSize;
			buf += mSectorSize;
		}
		if (size > 0) {
			readBlock(mSector);
			memcpy(buf, mSector, size);
			mSectorFirst = size;
			buf += size;
		}	
	}
	return buf-oldbuf;
}

int IDEDevice::write(byte *buf, int size)
{
	byte *oldbuf = buf;
	if (mSectorFirst && mSectorFirst < mSectorSize) {
		int copy = MIN(mSectorSize-mSectorFirst, size);
		memcpy(mSector+mSectorFirst, buf, copy);
		size -= copy;
		buf += copy;
		mSectorFirst += copy;
		if (mSectorFirst >= mSectorSize) {
			writeBlock(mSector);
		}
	}
	if (size > 0) {
		while (size >= mSectorSize) {
			writeBlock(buf);
			size -= mSectorSize;
			buf += mSectorSize;
		}
		if (size > 0) {
			memcpy(mSector, buf, size);
			mSectorFirst = size;
			buf += size;
		}
	}
	return buf-oldbuf;
}

File *IDEDevice::promGetRawFile()
{
	return new IDEDeviceFile(*this);
}

int IDEDevice::toString(char *buf, int buflen) const
{
	return ht_snprintf(buf, buflen, "%s", mName);
}
