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
#define MM_DEVICE_PROFILE_DVDROM 0x0010 // d.375

#define MM_DEVICE_FEATURE_PROFILE 0x0000
#define MM_DEVICE_FEATURE_CORE 0x0001
#define MM_DEVICE_FEATURE_MORPHING 0x0002
#define MM_DEVICE_FEATURE_REMOVABLE 0x0003
#define MM_DEVICE_FEATURE_RANDOM_READ 0x0010
#define MM_DEVICE_FEATURE_CD_READ 0x001e
#define MM_DEVICE_FEATURE_DVD_READ 0x001f
#define MM_DEVICE_FEATURE_INC_WRITE 0x0021 // .192
#define MM_DEVICE_FEATURE_xx 0x002d
#define MM_DEVICE_FEATURE_xx2 0x002e
#define MM_DEVICE_FEATURE_POWER 0x0100
#define MM_DEVICE_FEATURE_CD_AUDIO 0x0103 // .224
#define MM_DEVICE_FEATURE_TIMEOUT 0x0105
#define MM_DEVICE_FEATURE_DVD_CSS 0x0106 // .228
#define MM_DEVICE_FEATURE_RT_STREAM 0x0107
		

CDROMDevice::CDROMDevice(const char *name)
	: IDEDevice(name)
{
	mFeatures = new AVLTree(true);
	mProfiles = new AVLTree(true);
	addProfile(MM_DEVICE_PROFILE_DVDROM);
	curProfile = MM_DEVICE_PROFILE_DVDROM;
	// Profile DVDROM implies these features:
	addFeature(MM_DEVICE_FEATURE_PROFILE);
	addFeature(MM_DEVICE_FEATURE_CORE);
	addFeature(MM_DEVICE_FEATURE_MORPHING);
	addFeature(MM_DEVICE_FEATURE_REMOVABLE);
	addFeature(MM_DEVICE_FEATURE_RANDOM_READ);
	addFeature(MM_DEVICE_FEATURE_CD_READ);
	addFeature(MM_DEVICE_FEATURE_DVD_READ);
	addFeature(MM_DEVICE_FEATURE_POWER);
	addFeature(MM_DEVICE_FEATURE_TIMEOUT);
	addFeature(MM_DEVICE_FEATURE_RT_STREAM);

	mReady = false;
	mLocked = false;
	is_dvd = false;
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
	case 0x02:
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
		byte core[] = {0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00}; // 02=ATAPI, DBEvent=0
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
	case MM_DEVICE_FEATURE_DVD_READ: {
		byte dvdread[] = {0x00, 0x00, 0x00, 0x00}; //MULTI110=0, Dual-R=0

		// FIXME: persistent, current?
		return createFeature(buf, aLen, MM_DEVICE_FEATURE_DVD_READ, 2, true, true, dvdread, sizeof dvdread);
	}
	case MM_DEVICE_FEATURE_POWER: 
		// FIXME: persistent, current?
		return createFeature(buf, aLen, MM_DEVICE_FEATURE_POWER, 0, true, true, NULL, 0);
	case MM_DEVICE_FEATURE_TIMEOUT: {
		byte timeout[] = {0x00, 0x00, 0x00, 0x00}; // Group 3=0
		return createFeature(buf, aLen, MM_DEVICE_FEATURE_TIMEOUT, 0, true, true, timeout, sizeof timeout);
	}
	case MM_DEVICE_FEATURE_RT_STREAM: {
		byte rt_stream[] = { 0x00, 0x00, 0x00, 0x00 }; // RBCD=0 SCS=0 MP2A=0 WSPD=0 SW=0

		return createFeature(buf, aLen, MM_DEVICE_FEATURE_RT_STREAM, 0, true, true, rt_stream, sizeof rt_stream);
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

int CDROMDevice::writeBlock(byte *buf)
{
	IO_IDE_ERR("attempt to write to CDROM\n");
	return 0;
}

int CDROMDevice::readDVDStructure(byte *buf, int len, uint8 subcommand, uint32 address, uint8 layer, uint8 format, uint8 AGID, uint8 control)
{
	uint32 capacity = getCapacity();

	if (!is_dvd) {
		return 0;
	}

	if (subcommand == 0x0000) {
		switch (format) {
		case 0x00:	//Physical format information
			buf[0] = 0;
			buf[1] = 21;

			buf[2] = 0x00; // reserved
			buf[3] = 0x00; // reserved

			buf[4] = 0x01; //DVD-ROM + Version 1.0x
			buf[5] = 0x0f; //120mm + No max rate specified
			buf[6] = 0x00; //1 layer + PTP + embossed data
			buf[7] = 0x00; //0.267um/bit + 0.74um/track

			buf[8] = 0x00; //pad
			buf[9] = 0x03; //start of physical data
			buf[10] = 0x00; //start of physical data
			buf[11] = 0x00; //start of physical data

			buf[12] = 0x00; //pad
			buf[13] = capacity >> 16; //end of physical data
			buf[14] = capacity >> 8;  //end of physical data
			buf[15] = capacity;       //end of physical data

			buf[16] = 0x00; //pad
			buf[17] = 0x00; //end sector number in layer 0
			buf[18] = 0x00; //end sector number in layer 0
			buf[19] = 0x00; //end sector number in layer 0

			buf[20] = 0x00; //BCA + reserved

			// 21-n : defined as reserved for DVD-ROMs

			return 21;

		case 0x01:	// Copyright Information
			buf[0] = 0;
			buf[1] = 8;

			buf[2] = 0;
			buf[3] = 0;

			buf[4] = 0x00; // no copyright information
			buf[5] = 0x00; // all regions allowed
			buf[6] = 0x00; // reserved
			buf[7] = 0x00; // reserved

			return 8;

		default:
			ht_printf("readDVDStructure with Unsupported Format (%i)\n", format);
			return 0;
		}
	} else {
		ht_printf("readDVDStructure on Unsupported Media (%i)\n", subcommand);
		return 0;
	}
}

void CDROMDevice::activateDVD(bool onoff)
{
	is_dvd = onoff;
}

bool CDROMDevice::isDVD(void)
{
	return is_dvd;
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

bool CDROMDeviceFile::seek(uint64 blockno)
{
	curLBA = blockno;
	sys_fseek(mFile, (uint64)blockno * 2048);
	return true;
}

void CDROMDeviceFile::flush()
{
	sys_flush(mFile);
}

int CDROMDeviceFile::readBlock(byte *buf)
{
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

	if (!is_dvd && (mCapacity > 1151850)) {
		/* In case the image just can't be a CD-ROM */
		this->activateDVD(true);
	}

	return true;
}

int CDROMDeviceFile::readTOC(byte *buf, bool msf, uint8 starttrack, int len, int format)
{
	switch (format) {
	case 0: {
		// .415
		// start_track == track
		byte sector[12] = {
			   0,
			  10,

			   1, // first track
			   1, // last track
			0x00, // res
			0x16, // .408 (Data track, copy allowed :) )
			   1, // track number
			0x00, // res
			0x00, // LBA
			0x00,
			0x00,
			0x00, // LBA 
		};
		if (msf) {
			MSF m;
			MSFfromLBA(m, 0);
			sector[8] = 0x00;
			sector[9] = m.m;
			sector[10] = m.s;
			sector[11] = m.f; 
		}
		return put(buf, len, sector, sizeof sector);
	}
	case 1: {
		// .418 Multisession information
		// start_track == 0
		byte sector[12] = {
			   0,
			  10,

			   1, // first session
			   1, // last session
			0x00, // res
			0x16, // .408 (Data track, copy allowed :) )
			   1, // first track number in last complete session
			0x00, // res
			0x00, // LBA
			0x00,
			0x00,
			0x00, // LBA 
		};
		if (msf) {
			MSF m;
			MSFfromLBA(m, 0);
			sector[8] = 0x00;
			sector[9] = m.m;
			sector[10] = m.s;
			sector[11] = m.f; 
		}
		return put(buf, len, sector, sizeof sector);
	}
	case 2: {
		// .420 Raw TOC
		// start_track == session number

		MSF msf_cap, msf_zero;
		// FIXME: only when (msf)?
		MSFfromLBA(msf_cap, getCapacity());
		MSFfromLBA(msf_zero, 0);

		byte sector[48] = {
			   0,
			sizeof sector - 2,
			   1, // first session
			   1, // last session

			      // points a0-af tracks b0-bf

			   1, // session number
			0x16, // .408 (Data track, copy allowed :) )
			   0, // track number
			0xa0, // point (lead-in)
			0x00, // min
			0x00, // sec
			0x00, // frame		
			0x00, // zero
			0x01, // first track
			0x00, // disk type
			0x00, //

			   1, // session number
			0x16, // .408 (Data track, copy allowed :) )
			   0, // track number
			0xa1, // point
			0x00, // min
			0x00, // sec
			0x00, // frame
			0x00, // zero
			0x01, // last track
			0x00, // 
			0x00, // 

			   1, // session number
			0x16, // .408 (Data track, copy allowed :) )
			   0, // track number
			0xa2, // point (lead-out)
			0x00, // min
			0x00, // sec
			0x00, // frame
			0x00, // zero
			msf_cap.m,   // start
			msf_cap.s,   //  of
			msf_cap.f,   //  leadout

			   1, // session number
			0x16, // .408 (Data track, copy allowed :) )
			   0, // track number
			   1, // point (real track)
			0x00, // min
			0x00, // sec
			0x00, // frame
			0x00, // zero
			msf_zero.m,  // start
			msf_zero.s,  //  of
			msf_zero.f,  //  track
		};
		return put(buf, len, sector, sizeof sector);
	}
	case 3:
		// PMA
	case 4:
		// ATIP
	case 5: 
		// CDTEXT
	default: {
		IO_IDE_WARN("read toc: format %d not supported.\n", format);
		byte sector[2] = {0, 0};
		return put(buf, len, sector, sizeof sector);
	}
	}
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
     :CDROMDevice(name)
{
	buffer_size = SCSI_BUFFER_SECTORS;
	buffer_base = (LBA) - SCSI_BUFFER_SECTORS;
	data_buffer = NULL;
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
	byte params[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	byte res = SCSI_ExecCmd(SCSI_UNITREADY, SCSI_CMD_DIR_OUT, params);
	mReady = res == SCSI_STATUS_GOOD;
	return mReady;
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param lock true for locking the tray, false for unlocking
/// @return true on successful execution, else false
bool CDROMDeviceSCSI::setLock(bool lock)
{
	bool ret = CDROMDevice::setLock(lock);
	if (ret) {
		byte params[8] = {0, 0, 0, 0, 0, 0, 0, 0};
		params[3] = lock ? SCSI_TRAYLOCK_LOCKED : SCSI_TRAYLOCK_UNLOCKED;
		ret = SCSI_ExecCmd(SCSI_TRAYLOCK, SCSI_CMD_DIR_OUT, params) == SCSI_STATUS_GOOD;
	}
	return ret;
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @return The number of sectors on the inserted media
uint32 CDROMDeviceSCSI::getCapacity()
{
	if (!isReady()) {
		IO_IDE_ERR("CDROMDeviceSCSI::getCapacity() failed: not ready.\n");
	} else {
		byte buf[8];

		byte params[8] = {0, 0, 0, 0, 0, 0, 0, 0};
		byte res = SCSI_ExecCmd(SCSI_READCDCAP, SCSI_CMD_DIR_IN, params, buf, 8);
		if (res != SCSI_STATUS_GOOD) return 0;
	
		return (buf[0]<<24) + (buf[1]<<16) + (buf[2]<<8) + buf[3];
	}
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param blockno The sector to seek to
/// @return true on successful execution, else false
bool CDROMDeviceSCSI::seek(uint64 blockno)
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
bool CDROMDeviceSCSI::readBufferedData(byte *buf, uint sector)
{
	if (buffer_size) {
		// If we have the requested data buffered, return it
		int buffer_delta = (int) sector - (int) buffer_base;
		if (buffer_delta >= 0 && buffer_delta < (int) buffer_size) {
			uint offset = CD_FRAMESIZE * buffer_delta;
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
		byte sync[] = {0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0};
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


/** 
 * @param buffer result buffer
 * @param msf if true use MSF encoding otherwise LBA
 * @param starttrack start track
 * @param len maximum length of buffer
 * @param format see CDROMDeviceFile::readTOC
*/ 
int CDROMDeviceSCSI::readTOC(byte *buf, bool msf, uint8 starttrack, int len, int format)
{
	byte params[9] = {
		msf ? 2 : 0,
		0,
		0,
		0,
		0,
		starttrack,
		len >> 8,
		len >> 0,
		format << 6,
	};
	if (SCSI_ExecCmd(SCSI_READ_TOC, SCSI_CMD_DIR_IN,
	  params, buf, len) == SCSI_STATUS_GOOD) {
		ht_printf("readtoc: %d\n", (buf[0] << 8) | (buf[1] << 0));
		return (buf[0] << 8) | (buf[1] << 0);
	} else {
		ht_printf("readtoc failed\n");
		return 0;
	}
}

int CDROMDeviceSCSI::getConfig(byte *buf, int len, byte RT, int first)
{
	static byte rt[] = {
		0x00, 0x01, 0x02, 0x03, 0x43, 0x6f, 0x6e, 0x74,
		0x61, 0x69, 0x6e, 0x73, 0x00, 0x50, 0x00, 0x45, 
		0x00, 0x41, 0x00, 0x52, 0x00, 0x50, 0x00, 0x43,
		0x20, 0x43, 0x6f, 0x64, 0x65, 0x2e, 0x20, 0x50,
		0x6f, 0x72, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x20,
		0x43, 0x6f, 0x70, 0x79, 0x72, 0x69, 0x67, 0x68,
		0x74, 0x20, 0x28, 0x43, 0x29, 0x20, 0x53, 0x65,
		0x62, 0x61, 0x73, 0x74, 0x69, 0x61, 0x6e, 0x20,
		0x42, 0x69, 0x61, 0x6c, 0x6c, 0x61, 0x73, 0x20,
		0x32, 0x30, 0x30, 0x34, 0x00, 0x46, 0x00, 0x00
	};

	byte params[9] = {
		rt[RT],
		first >> 8,
		first >> 0,
		0,
		0,
		0,
		len >> 8,
		len >> 0,
		0,
	};
	if (SCSI_ExecCmd(rt[(4<<4)+13], SCSI_CMD_DIR_IN,
	  params, buf, len) == SCSI_STATUS_GOOD) {
		uint32 reslen = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3] << 0) + 4;
		return reslen;
	} else {
		ht_printf("getconfig failed\n");
		return 0;
	}
}

int CDROMDeviceSCSI::readDVDStructure(byte *buf, int len, uint8 subcommand, uint32 address, uint8 layer, uint8 format, uint8 AGID, uint8 control)
{
	byte params[11] = {
		subcommand,
		address >> 24,
		address >> 16,
		address >>  8,
		address >>  0,
		layer,
		format,
		len >> 8,
		len >> 0,
		AGID << 6,
		control,
	};
	if (SCSI_ExecCmd(0xad, SCSI_CMD_DIR_IN,
	  params, buf, len) == SCSI_STATUS_GOOD) {
		uint32 reslen = (buf[0] << 8) | (buf[1] << 0) + 2;
		return reslen;
	} else {
		ht_printf("readDVDStructure failed\n");
		return 0;
	}
}

/// @author Alexander Stockinger
/// @date 07/17/2004
void CDROMDeviceSCSI::eject()
{
	if (!isLocked()) {
		byte params[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
		params[3] = true ? SCSI_EJECTTRAY_UNLOAD : SCSI_EJECTTRAY_LOAD;
		SCSI_ExecCmd(SCSI_EJECTTRAY, SCSI_CMD_DIR_OUT, params);
		buffer_base = (LBA) - buffer_size;
	}
}

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
/// @return The number of bytes actually read
uint CDROMDeviceSCSI::readData(byte *buf, uint64 pos, uint count)
{
	uint res = 0;
	while (count) {
		uint sector = pos / CD_FRAMESIZE;
		uint offset = pos % CD_FRAMESIZE;
		uint read = CD_FRAMESIZE - offset;
		read = MIN(read, count);

		byte buffer[CD_FRAMESIZE];
		bool ret = readBufferedData(buffer, sector);
		if (!ret) return res;

		memcpy(buf, buffer + offset, read);

		count -= read;
		res += read;
		buf += read;
		pos += read;
	}
	return res;
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param buf The buffer to read into (expected to be at least size bytes)
/// @param size The number of bytes to read
/// @return The number of bytes read
uint CDROMDeviceSCSI::promRead(byte *buf, uint size)
{
	if (!isReady()) {
		IO_IDE_WARN("CDROMDeviceSCSI::promRead(): not ready.\n");
		return 0;
	}
	return readData(buf, prompos, size);
}

/// @author Alexander Stockinger
/// @date 07/17/2004
/// @param start The first sector to read
/// @param buffer The data buffer to read into
/// @param buffer_bytes The size of the data buffer in bytes
/// @param sectors The number of consecutive sectors to read
/// @return true on successful execution, else false
bool CDROMDeviceSCSI::SCSI_ReadSectors(uint32 start, byte *buffer,
				       uint buffer_bytes,
				       uint sectors)
{
	byte params[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

	params[1] = (byte) (start >> 24);
	params[2] = (byte) (start >> 16);
	params[3] = (byte) (start >> 8);
	params[4] = (byte) (start >> 0);

	params[6] = sectors >> 8;	// Number of sectors to read
	params[7] = sectors;		// Number of sectors to read

	byte ret = SCSI_ExecCmd(SCSI_READ10, SCSI_CMD_DIR_IN, params, buffer, buffer_bytes);
	return ret == SCSI_STATUS_GOOD;
}
