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

#ifndef __CD_H__
#define __CD_H__

#include "idedevice.h"

struct MSF {
	uint8 m,s,f;
};

typedef uint32 LBA;

// Flags for IDEDevice::mMode
#define IDE_ATAPI_TRANSFER_HDR_SYNC 0x80
#define IDE_ATAPI_TRANSFER_HDR_SECTOR_SUB 0x40
#define IDE_ATAPI_TRANSFER_HDR_SECTOR 0x20
#define IDE_ATAPI_TRANSFER_DATA 0x10
#define IDE_ATAPI_TRANSFER_ECC 0x08

class CDROMDevice: public IDEDevice {
	bool mLocked;
	bool mReady;
	Container *mFeatures, *mProfiles;
	int curProfile;
public:
		CDROMDevice(const char *name);
	virtual	~CDROMDevice();
	
	virtual bool	isReady();
		bool	isLocked();
	virtual	bool	setLock(bool aLocked);
		bool	toggleLock();
	static	void	MSFfromLBA(MSF &msf, LBA lba);
	static	void	LBAfromMSF(LBA &lba, MSF msf);
	virtual bool	validLBA(LBA lba);
	virtual int	getConfig(byte *buf, int len, byte RT, int first);
	virtual uint32	getCapacity() = 0;
	virtual int	modeSense(byte *buf, int len, int pc, int page);
	virtual uint	getBlockSize();
	virtual uint	getBlockCount();
	virtual	bool	setReady(bool aReady);
	virtual void	readTOC(byte *buf, bool msf, uint8 starttrack, int len, int format) = 0;
	virtual void	eject() = 0;
protected:
		void	addFeature(int feature);
		void	addProfile(int profile);
	virtual int	getFeature(byte *buf, int len, int feature);
		int	createFeature(byte *buf, int len, int feature, int version, bool pp, bool cur, byte *add, int size);
		int	put(byte *buf, int len, byte *src, int size);
};

class CDROMDeviceFile: public CDROMDevice {
	SYS_FILE	*mFile;
	LBA		curLBA;
	uint32		mCapacity;
public:
		CDROMDeviceFile(const char *name);
	virtual	~CDROMDeviceFile();	

	virtual uint32	getCapacity();
		bool	changeDataSource(const char *file);
	virtual bool	seek(int blockno);
	virtual int	readBlock(byte *buf);
	virtual int	writeBlock(byte *buf);
	virtual void	readTOC(byte *buf, bool msf, uint8 starttrack, int len, int format);
	virtual void	eject();

	virtual bool	promSeek(uint64 pos);
	virtual uint	promRead(byte *buf, uint size);
};

#endif
