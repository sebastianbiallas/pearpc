/*
 *	PearPC
 *	ata.h
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

#ifndef __ATA_H__
#define __ATA_H__

#include "system/file.h"
#include "idedevice.h"

// Flags for IDEDevice::mMode
#define ATA_DEVICE_MODE_PLAIN 0 // just a 512 byte sector
#define ATA_DEVICE_MODE_ECC   1 // add 4 byte ECC

class ATADevice: public IDEDevice {
public:
	int mCyl;
	int mHeads;
	int mSpt;
	uint blocks;

			ATADevice(const char *name);
	virtual 	~ATADevice();
		void	init(int aHeads, int aCyl, int mSpt);
	virtual uint	getBlockSize();
	virtual uint	getBlockCount();
};

class ATADeviceFile: public ATADevice {
	SYS_FILE *mFile;
public:
		ATADeviceFile(const char *name, const char *filename);
	virtual ~ATADeviceFile();

	virtual bool	seek(uint64 blockno);
	virtual void	flush();
	virtual int	readBlock(byte *buf);
	virtual int	writeBlock(byte *buf);

	virtual bool	promSeek(uint64 pos);
	virtual uint	promRead(byte *buf, uint size);
};

#endif
