/*
 *	PearPC
 *	idedevice.h
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

#ifndef __IDEDEVICE_H__
#define __IDEDEVICE_H__

#include "system/systhread.h"
#include "tools/data.h"
#include "tools/stream.h"

// The maximum size of a CD sector
#define IDE_MAX_BLOCK_SIZE 2352

class IDEDevice: public Object {
protected:
	int	mMode; // this is implementation specific
	bool	mAcquired;
	sys_mutex mMutex;
	char	*mName;
	/* only needed for deblocking read/write */
	uint8	mSector[IDE_MAX_BLOCK_SIZE]; 
	int	mSectorFirst; // first valid byte
	int	mSectorSize;
	char	*mError;

	/* only for prom */
public:
		IDEDevice(const char *name);
	virtual	~IDEDevice();
		bool	acquire();
		bool	release();	
	virtual void	setError(const char *error);
	virtual char *	getError();
	virtual uint	getBlockSize() = 0;
	virtual uint	getBlockCount() = 0;
	virtual bool	seek(uint64 blockno) = 0;
	virtual void	flush() = 0;
		void	setMode(int aMode, int aSectorSize);
	/* these are deblocking read/writes */
	virtual int	read(byte *buf, int size);
	virtual int	write(byte *buf, int size);
	/* these will always fetch a whole sector */
	virtual int	readBlock(byte *buf) = 0;
	virtual int	writeBlock(byte *buf) = 0;
	/* only for prom */
	virtual File *	promGetRawFile();
	virtual bool	promSeek(uint64 pos) = 0;
	virtual uint	promRead(byte *buf, uint size) = 0;
	
	virtual	int	toString(char *buf, int buflen) const;
};

#endif
