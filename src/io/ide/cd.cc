/*
 *	PearPC
 *	cd.cc
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
#include <cstring>
#include "errno.h"

#include "debug/tracers.h"
#include "tools/data.h"
#include "cd.h"

// REMOVE ME
#include "cpu_generic/ppc_cpu.h"

#define MM_DEVICE_PROFILE_CDROM 0x0008 // .242

#define MM_DEVICE_FEATURE_PROFILE 0x0000
#define MM_DEVICE_FEATURE_CORE 0x0001
#define MM_DEVICE_FEATURE_MORPHING 0x0002
#define MM_DEVICE_FEATURE_REMOVABLE 0x0003
#define MM_DEVICE_FEATURE_RANDOM_READ 0x0010
#define MM_DEVICE_FEATURE_CD_READ 0x001e
#define MM_DEVICE_FEATURE_INC_WRITE 0x0021 // .192
#define MM_DEVICE_FEATURE_xx 0x002d
#define MM_DEVICE_FEATURE_xx2 0x002e
#define MM_DEVICE_FEATURE_POWER 0x0100
#define MM_DEVICE_FEATURE_CD_AUDIO 0x0103 // .224
#define MM_DEVICE_FEATURE_TIMEOUT 0x0105
#define MM_DEVICE_FEATURE_DVD_CSS 0x0106 // .228
		

CDROMDevice::CDROMDevice(const char *name)
	: IDEDevice(name)
{
	mFeatures = new AVLTree(true);
	mProfiles = new AVLTree(true);
	addProfile(MM_DEVICE_PROFILE_CDROM);
	curProfile = MM_DEVICE_PROFILE_CDROM;
	// Profile CDROM implies these features:
	addFeature(MM_DEVICE_FEATURE_PROFILE);
	addFeature(MM_DEVICE_FEATURE_CORE);
	addFeature(MM_DEVICE_FEATURE_MORPHING);
	addFeature(MM_DEVICE_FEATURE_REMOVABLE);
	addFeature(MM_DEVICE_FEATURE_RANDOM_READ);
	addFeature(MM_DEVICE_FEATURE_CD_READ);
	addFeature(MM_DEVICE_FEATURE_POWER);
	addFeature(MM_DEVICE_FEATURE_TIMEOUT);

	mReady = false;
	mLocked = false;
}

CDROMDevice::~CDROMDevice()
{
	delete mFeatures;
	delete mProfiles;
}
	
bool CDROMDevice::isReady()
{
	return mReady;
}

bool CDROMDevice::setReady(bool aReady)
{
	if (isLocked()) return false;
	mReady = aReady;
	return true;
}

bool CDROMDevice::isLocked()
{
	return mLocked;
}

bool CDROMDevice::setLock(bool aLocked)
{
	if (!isReady()) return false;
	mLocked = aLocked;
	return true;
}

bool CDROMDevice::toggleLock()
{
	return setLock(!isLocked());
}

void CDROMDevice::MSFfromLBA(MSF &msf, LBA lba)
{
	lba += 150;
	msf.m = lba / 4500;
	msf.s = (lba % 4500) / 75;
	msf.f = lba % 75;
}

void CDROMDevice::LBAfromMSF(LBA &lba, MSF msf)
{
	lba = (((msf.m*60) + msf.s) * 75) + msf.f - 150;
}

bool CDROMDevice::validLBA(LBA lba)
{
	return true;
}

uint CDROMDevice::getBlockSize()
{
	return 2048;
}

uint CDROMDevice::getBlockCount()
{
	return getCapacity();
}

int CDROMDevice::getConfig(byte *buf, int aLen, byte RT, int first)
{
	// .284
	byte header[] = {
		0x00, 0x00, 0x00, 0x00,	// length, filled later
		0x00, 0x00,		// res
		curProfile >> 8, curProfile,
	};
	int len = sizeof header - 4;
	switch (RT) {
	case 0x00:
		// return all
		foreach(UInt, f, *mFeatures, {
			int v = f->value;
			if (v >= first) 
				len += getFeature(buf+len, aLen-len, v);
		});
		break;
	case 0x01:
		// return all with current bit
		foreach(UInt, f, *mFeatures, {
			int v = f->value;
			if (v >= first /* FIXME: && (current bit) */) 
				len += getFeature(buf+len, aLen-len, v);
		});
		break;
	case 0x10:
		// return specific
		len += getFeature(buf+len, aLen-len, first);
		break;
	default: 
		IO_IDE_ERR("unknown RT in CDROMDevice::getConfig()\n");
		return -1;
	}
	header[0] = len >> 24; header[1] = len >> 16; header[2] = len >> 8; header[3] = len; 
	put(buf, aLen, header, sizeof header);
	return len;
}

int CDROMDevice::modeSense(byte *buf, int aLen, int pc, int page)
{
	return 0;
}

void CDROMDevice::addFeature(int feature)
{
	mFeatures->insert(new UInt(feature));
}

void CDROMDevice::addProfile(int profile)
{
	mProfiles->insert(new UInt(profile));
}

int CDROMDevice::getFeature(byte *buf, int aLen, int feature)
{
	if (aLen <= 0) return 0;
	switch (feature) {
	case MM_DEVICE_FEATURE_PROFILE: {
		int count = mProfiles->count();
		byte list[count*4];
		int idx = 0;
		// FIXME: foreachbwd ??
		foreach(UInt, p, *mProfiles, {
			int v = p->value;
			list[idx++] = v>>8;
			list[idx++] = v;
			list[idx++] = (v == curProfile) ? 0x01 : 0x00;
			list[idx++] = 0x00;
		});
		return createFeature(buf, aLen, MM_DEVICE_FEATURE_PROFILE, 0, true, true, list, count*4);
	}
	case MM_DEVICE_FEATURE_CORE: {
		byte core[] = {0x00, 0x00, 0x00, 0x02}; // 02=ATAPI
		return createFeature(buf, aLen, MM_DEVICE_FEATURE_CORE, 0, true, true, core, sizeof core);
	}
	case MM_DEVICE_FEATURE_MORPHING: {
		byte morph[] = {0x00, 0x00, 0x00, 0x00}; // 1. ASYNC=0 (ATAPI)
		return createFeature(buf, aLen, MM_DEVICE_FEATURE_MORPHING, 1, true, true, morph, sizeof morph);
	}
	case MM_DEVICE_FEATURE_REMOVABLE: {
		byte remove[] = {0x19, 0x00, 0x00, 0x00}; // Tray-Type-Loading, Eject=1, Jmpr=0, LockAllow=1
		return createFeature(buf, aLen, MM_DEVICE_FEATURE_REMOVABLE, 0, true, true, remove, sizeof remove);
	}
	case MM_DEVICE_FEATURE_RANDOM_READ: {
		byte randomread[] = {
		0x00, 0x00, 0x08, 0x00, // Logical Block Size
		0x00, 0x10, // Blocking
		0x00,
		0x00, // PP=0
		};
		// FIXME: persistent, current?
		return createFeature(buf, aLen, MM_DEVICE_FEATURE_RANDOM_READ, 0, true, true, randomread, sizeof randomread);
	}
	case MM_DEVICE_FEATURE_CD_READ: {
		byte cdread[] = {0x00, 0x00, 0x00, 0x00}; // DAP=0, C2=0, CD-Text=0
		// FIXME: persistent, current?
		return createFeature(buf, aLen, MM_DEVICE_FEATURE_CD_READ, 2, true, true, cdread, sizeof cdread);
	}
	case MM_DEVICE_FEATURE_POWER: 
		// FIXME: persistent, current?
		return createFeature(buf, aLen, MM_DEVICE_FEATURE_POWER, 0, true, true, NULL, 0);
	case MM_DEVICE_FEATURE_TIMEOUT: {
		byte timeout[] = {0x00, 0x00, 0x00, 0x00}; // Group 3=0
		return createFeature(buf, aLen, MM_DEVICE_FEATURE_TIMEOUT, 0, true, true, timeout, sizeof timeout);
	}
	default:
		// return size==0 for unimplemented / unsupported features
		return 0;
	}
}

int CDROMDevice::createFeature(byte *buf, int len, int feature, int version, bool pp, bool cur, byte *add, int size)
{
	byte header[] = {
		feature>>8, feature,
		version<<2|(pp?0x2:0)|(cur?0x1:0),
		size,
	};
	int l = put(buf, len, header, sizeof header);	
	return l+put(buf+l, len-l, add, size);	
}

int CDROMDevice::put(byte *buf, int aLen, byte *src, int size)
{
	int len = 0;
	while (size > 0) {
		if (aLen > 0) {
			*(buf++) = *(src++);
		} else {
			len += size;
			break;		
		}
		size--;
		aLen--;
		len++;
		
	}
	return len;
}

/*
 *
 */
CDROMDeviceFile::CDROMDeviceFile(const char *name)
	: CDROMDevice(name)
{
	mFile = NULL;
}

CDROMDeviceFile::~CDROMDeviceFile()
{
	if (mFile) sys_fclose(mFile);
}

uint32 CDROMDeviceFile::getCapacity()
{
	return mCapacity;
}

bool CDROMDeviceFile::seek(int blockno)
{
	curLBA = blockno;
	sys_fseek(mFile, blockno*2048);
	return true;
}

int CDROMDeviceFile::readBlock(byte *buf)
{
//	ht_printf("cdrom.readBlock(%x)\n", curLBA);
	if (mMode & IDE_ATAPI_TRANSFER_HDR_SYNC) {
		// .95
		byte sync[]={0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0};
		memcpy(buf, sync, 12);
		buf+=12;
	}
	if (mMode & IDE_ATAPI_TRANSFER_HDR_SECTOR) {
		// .95
		MSF msf;
		MSFfromLBA(msf, curLBA);
		*(buf++) = msf.m;
		*(buf++) = msf.s;
		*(buf++) = msf.f;
		*(buf++) = 0x01; // mode 1 data
	}
	if (mMode & IDE_ATAPI_TRANSFER_DATA) {
		sys_fread(mFile, buf, 2048);
		buf += 2048;
	}
	if (mMode & IDE_ATAPI_TRANSFER_ECC) {
		// .96
		// blablablablabla ;)
		memset(buf, 0, 288);
	}
	curLBA++;
	return 0;
}

int CDROMDeviceFile::writeBlock(byte *buf)
{
	IO_IDE_ERR("attempt to write to CDROM\n");
	return 0;
}

bool CDROMDeviceFile::promSeek(uint64 pos)
{
	return sys_fseek(mFile, pos) == 0;
}

uint CDROMDeviceFile::promRead(byte *buf, uint size)
{
	return sys_fread(mFile, buf, size);
}

bool CDROMDeviceFile::changeDataSource(const char *file)
{
	if (mFile) sys_fclose(mFile);
	mFile = sys_fopen(file, SYS_OPEN_READ);
	if (!mFile) {
		char buf[256];
		ht_snprintf(buf, sizeof buf, "%s: could not open file (%s)", file, strerror(errno));
		setError(buf);
		return false;
	}
	sys_fseek(mFile, 0, SYS_SEEK_END);
	FileOfs fsize = sys_ftell(mFile);
	mCapacity = fsize / 2048 + !!(fsize % 2048);
	return true;
}

void CDROMDeviceFile::readTOC(byte *buf, bool msf, uint8 starttrack, int len, int format)
{
}

void CDROMDeviceFile::eject()
{
}

