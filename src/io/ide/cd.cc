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
#include "scsicmds.h"

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

// ----------------------------- File based CDROM device ------------------------------------

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

void CDROMDeviceFile::flush()
{
	sys_flush(mFile);
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


//---------------------- SCSI based CDROM drive -----------------------

#define CD_FRAMESIZE 2048

// For me values up to 32 worked, 0 disables caching
#define SCSI_BUFFER_SECTORS 32


/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param name The name of the CDROM device
CDROMDeviceSCSI::CDROMDeviceSCSI(const char *name)
     :CDROMDevice (name)
{
	buffer_size = SCSI_BUFFER_SECTORS;
	buffer_base = (LBA) - SCSI_BUFFER_SECTORS;
	data_buffer = (byte *) 0;
	// Alloc read ahead buffer
	if (buffer_size)
		data_buffer = new byte[buffer_size * CD_FRAMESIZE];

}

/// @author Alexander Stockinger
/// @date 07/17/2004
CDROMDeviceSCSI::~CDROMDeviceSCSI()
{
	delete[]data_buffer;
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @return true if drive is ready, else false
bool CDROMDeviceSCSI::isReady()
{
	setReady(SCSI_GetReady());
	return CDROMDevice::isReady();
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param lock true for locking the tray, false for unlocking
/// @return true on successful execution, else false
bool CDROMDeviceSCSI::setLock(bool lock)
{
	bool ret = CDROMDevice::setLock(lock);
	if (!ret)
	    return false;
	return SCSI_Lock(isLocked());
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @return The number of sectors on the inserted media
uint32 CDROMDeviceSCSI::getCapacity()
{
	if (!isReady())
		IO_IDE_ERR("CDROMDeviceSCSI::getCapacity() failed: not ready.\n");
	return SCSI_GetSectorCount();
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param blockno The sector to seek to
/// @return true on successful execution, else false
bool CDROMDeviceSCSI::seek(int blockno)
{
	curLBA = blockno;
	return true;
}

/// @author Alexander Stockinger
/// @date 07/17/2004
void CDROMDeviceSCSI::flush()
{
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param sector The sector to read
/// @param buf The buffer to read into (size is expected to be CD_FRAMESIZE bytes)
/// @return true on successful execution, else false
bool CDROMDeviceSCSI::readBufferedData(byte *buf, unsigned int sector)
{
	if (buffer_size) {
		// If we have the requested data buffered, return it
		int buffer_delta = (int) sector - (int) buffer_base;
		if (buffer_delta >= 0 && buffer_delta < (int) buffer_size) {
			unsigned int offset = CD_FRAMESIZE * buffer_delta;
			memcpy(buf, data_buffer + offset, CD_FRAMESIZE);
			return true;
		}

		// If not, buffer some more and return the requested one
		buffer_base = sector;
		bool ret = SCSI_ReadSectors(sector, data_buffer, CD_FRAMESIZE * buffer_size, buffer_size);
		memcpy(buf, data_buffer, CD_FRAMESIZE);
		return ret;
	} else
		return SCSI_ReadSectors(sector, buf, CD_FRAMESIZE, 1);
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param buf The buffer to read into (size is expected to be CD_FRAMESIZE bytes)
/// @return Always 0 at the moment
int CDROMDeviceSCSI::readBlock(byte *buf)
{
	if (mMode & IDE_ATAPI_TRANSFER_HDR_SYNC) {
		// .95
		byte sync[] = { 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0 };
		memcpy(buf, sync, 12);
		buf += 12;
	}
	if (mMode & IDE_ATAPI_TRANSFER_HDR_SECTOR) {
		// .95
		MSF msf;
		MSFfromLBA(msf, curLBA);
		*(buf++) = msf.m;
		*(buf++) = msf.s;
		*(buf++) = msf.f;
		*(buf++) = 0x01;	// mode 1 data
	}
	if (mMode & IDE_ATAPI_TRANSFER_DATA) {
		//bool ret = ASPI_ReadSector(a, t, l, curLBA, buf, CD_FRAMESIZE);
		bool ret = readBufferedData(buf, curLBA);
		if (!ret)
			IO_IDE_WARN("Error reading from SCSI CD drive (sector %d)\n", curLBA);
		buf += CD_FRAMESIZE;
	}
	if (mMode & IDE_ATAPI_TRANSFER_ECC) {
		// .96
		// blablablablabla ;)
		memset(buf, 0, 288);
	}
	curLBA++;
	return 0;
}


/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param buf The buffer to read into (size is expected to be CD_FRAMESIZE bytes)
/// @return Always 0 at the moment
int CDROMDeviceSCSI::writeBlock(byte *buf)
{
	IO_IDE_ERR("Cannot write to CDROM\n");
	return 0;
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param buffer UNKNOWN
/// @param msf UNKNOWN
/// @param starttrack UNKNOWN
/// @param len UNKNOWN
/// @param format UNKNOWN
void CDROMDeviceSCSI::readTOC(byte *buffer, bool msf, uint8 starttrack, int len, int format)
{
}

/// @author Alexander Stockinger
/// @date 07/17/2004
void CDROMDeviceSCSI::eject()
{
	if (isLocked())
		return;
	SCSI_Eject(true);
	buffer_base = (LBA) - buffer_size;
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param pos The new seek position (byte address)
/// @return true on successful execution, else false
bool CDROMDeviceSCSI::promSeek(uint64 pos)
{
	prompos = pos;
	return true;
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param buf The buffer to read into (expected to be at least count bytes)
/// @param pos The first byte to read
/// @param count The number of bytes to read
/// @return The number of bytes actually read, if a error occured -1
int CDROMDeviceSCSI::readData(byte *buf, uint64 pos, unsigned int count)
{
	unsigned int sector = pos / CD_FRAMESIZE;
	unsigned int offset = pos % CD_FRAMESIZE;
	unsigned int read = CD_FRAMESIZE - offset;
	read = MIN(read, count);

	byte buffer[CD_FRAMESIZE];
	bool ret = readBufferedData(buffer, sector);
	if (!ret)
		return -1;

	memcpy(buf, buffer + offset, read);
	return read;
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param buf The buffer to read into (expected to be at least size bytes)
/// @param size The number of bytes to read
/// @return The number of bytes read
uint CDROMDeviceSCSI::promRead(byte *buf, uint size)
{
	if (!isReady()) {
		IO_IDE_WARN("CDROMDeviceASPI::promRead(): not ready.\n");
		return 0;
	}

	unsigned int read = 0;
	while (read != size) {
		int cr = readData(buf + read, prompos, size - read);
		if (cr == -1)
			return read;
		read += cr;
		prompos += cr;
	}
	return size;
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @return true if drive is ready, else false
bool CDROMDeviceSCSI::SCSI_GetReady()
{
	byte params[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	byte res = SCSI_ExecCmd(SCSI_UNITREADY, SCSI_CMD_DIR_OUT, params);
	return res == STATUS_GOOD;
}


/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param locked true for locking the tray, false for unlocking
/// @return true on successful execution, else false
bool CDROMDeviceSCSI::SCSI_Lock(bool locked)
{
	byte params[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	params[3] = locked ? SCSI_TRAYLOCK_LOCKED : SCSI_TRAYLOCK_UNLOCKED;
	byte res = SCSI_ExecCmd(SCSI_TRAYLOCK, SCSI_CMD_DIR_OUT, params);
	return res == STATUS_GOOD;
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @return The number of sectors on the physical media
unsigned int CDROMDeviceSCSI::SCSI_GetSectorCount()
{
	byte buf[8];

	byte params[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	byte res =
		SCSI_ExecCmd(SCSI_READCDCAP, SCSI_CMD_DIR_IN, params, buf, 8);
	if (res != STATUS_GOOD)
		return 0;

	int ret =
		((buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) +
		 buf[3]) * ((buf[4] << 24) + (buf[5] << 16) + (buf[6] << 8) +
			    buf[7]);
	// TODO: Seen better code for that somehwere...
	return ret / CD_FRAMESIZE;
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param start The first sector to read
/// @param buffer The data buffer to read into
/// @param buffer_bytes The size of the data buffer in bytes
/// @param sectors The number of consecutive sectors to read
/// @return true on successful execution, else false
bool CDROMDeviceSCSI::SCSI_ReadSectors(unsigned long start, byte *buffer,
				       unsigned int buffer_bytes,
				       unsigned int sectors)
{
	byte params[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	params[1] = (byte) (start >> 24);
	params[2] = (byte) (start >> 16);
	params[3] = (byte) (start >> 8);
	params[4] = (byte) (start >> 0);

	params[7] = sectors;	// Number of sectors to read

	byte ret = SCSI_ExecCmd(SCSI_READ10, SCSI_CMD_DIR_IN, params, buffer, buffer_bytes);
	return ret == STATUS_GOOD;
}

/// @author Alexander Stockinger
/// @date 07/13/2004
/// @param eject true: eject, false: insert
/// @return true on successful execution, else false
bool CDROMDeviceSCSI::SCSI_Eject(bool eject)
{
	byte params[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	params[3] = eject ? SCSI_EJECTTRAY_UNLOAD : SCSI_EJECTTRAY_LOAD;
	return SCSI_ExecCmd(SCSI_EJECTTRAY, SCSI_CMD_DIR_OUT, params);
}
