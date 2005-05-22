/*
 *	PearPC
 *	ata.cc
 *
 *	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
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
#include <errno.h>

#include "debug/tracers.h"
#include "ata.h"

#include "tools/snprintf.h"

ATADevice::ATADevice(const char *name)
	: IDEDevice(name)
{
}

void ATADevice::init(int aHeads, int aCyl, int aSpt)
{
	mHeads = aHeads;
	mCyl = aCyl;
	mSpt = aSpt;
}

ATADevice::~ATADevice()
{
}

uint ATADevice::getBlockSize()
{
	return 512;
}

uint ATADevice::getBlockCount()
{
	return blocks;
}

/*
 *
 */
ATADeviceFile::ATADeviceFile(const char *name, const char *filename)
	: ATADevice(name)
{
	mFile = sys_fopen(filename, SYS_OPEN_READ | SYS_OPEN_WRITE);
	if (mFile) {
		sys_fseek(mFile, 0, SYS_SEEK_END);
		uint64 size = sys_ftell(mFile);
		uint64 cyl = size / 516096ULL;
		blocks = size / 512;
		if ((size % 516096) || cyl > 65535) {
			// we only support disk images with 16 heads and 63 spt
			sys_fclose(mFile);
			mFile = NULL;
			setError("invalid format (filesize isn't a multiple of 516096)");
		} else {
			init(16, cyl, 63);
		}
	} else {
		char buf[256];
		ht_snprintf(buf, sizeof buf, "%s: could not open file (%s)", filename, strerror(errno));
		setError(buf);
	}
}

ATADeviceFile::~ATADeviceFile()
{
}

bool ATADeviceFile::seek(uint32 blockno)
{
	sys_fseek(mFile, 512 * (uint64)blockno);
	return true;
}

void ATADeviceFile::flush()
{
	sys_flush(mFile);
}

int ATADeviceFile::readBlock(byte *buf)
{
	sys_fread(mFile, buf, 512);
	if (mMode & ATA_DEVICE_MODE_ECC) {
		// add ECC bytes..
		IO_IDE_ERR("ATADeviceFile: ECC not implemented\n");
	}
	return 0;
}

int ATADeviceFile::writeBlock(byte *buf)
{
	sys_fwrite(mFile, buf, 512);
	return 0;
}

bool ATADeviceFile::promSeek(FileOfs pos)
{
	return sys_fseek(mFile, pos) == 0;
}

uint ATADeviceFile::promRead(byte *buf, uint size)
{
	return sys_fread(mFile, buf, size);
}

