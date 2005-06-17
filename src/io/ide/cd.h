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

struct MSF
{
	uint8 m, s, f;
};

typedef uint32 LBA;

// Flags for IDEDevice::mMode
#define IDE_ATAPI_TRANSFER_HDR_SYNC 0x80
#define IDE_ATAPI_TRANSFER_HDR_SECTOR_SUB 0x40
#define IDE_ATAPI_TRANSFER_HDR_SECTOR 0x20
#define IDE_ATAPI_TRANSFER_DATA 0x10
#define IDE_ATAPI_TRANSFER_ECC 0x08

struct Sense {
	uint8 sense_key;
	uint8 info[4];
	uint8 spec_info[4];
	uint8 key_spec[3];
	uint8 fruc;
	uint8 asc;
	uint8 ascq;
};

enum CDROMReceiveResult {
	CDROMReceiveOK,
	CDROMReceiveNop,
	CDROMReceiveError
};

class CDROMDevice: public IDEDevice {
protected:
	bool		mLocked;
	bool		mReady;
	Container	*mFeatures, *mProfiles;
	int		curProfile;
	bool		is_dvd;
public:
			CDROMDevice(const char *name);
	virtual		~CDROMDevice ();

	virtual	bool	isReady();
		bool	isLocked();
	virtual bool	setLock(bool aLocked);
		bool	toggleLock();
	static	void	MSFfromLBA(MSF & msf, LBA lba);
	static	void	LBAfromMSF(LBA & lba, MSF msf);
	virtual bool	validLBA(LBA lba);
	virtual int	getConfig(byte *buf, int len, byte RT, int first);
	virtual uint32	getCapacity() = 0;
	virtual int	modeSense(byte *buf, int len, int pc, int page);
	virtual uint	getBlockSize();
	virtual uint	getBlockCount();
	virtual bool	setReady(bool aReady);
	virtual int	readTOC(byte *buf, bool msf, uint8 starttrack, int len,
			     int format) = 0;
	virtual void	eject() = 0;
	virtual	int	writeBlock(byte *buf);

	virtual	int	readDVDStructure(byte *buf, int len, uint8 subcommand, uint32 address, uint8 layer, uint8 format, uint8 AGID, uint8 control);
	virtual void	activateDVD(bool onoff);
	virtual bool	isDVD(void);

protected:
		void	addFeature(int feature);
		void	addProfile(int profile);
	virtual	int	getFeature(byte *buf, int len, int feature);
		int	createFeature(byte *buf, int len, int feature, int version,
			  bool pp, bool cur, byte *add, int size);
		int	put(byte *buf, int len, byte *src, int size);
};

class CDROMDeviceFile: public CDROMDevice {
	SYS_FILE	*mFile;
	LBA		curLBA;
	uint32		mCapacity;
public:
			CDROMDeviceFile(const char *name);
	virtual		~CDROMDeviceFile();

	virtual	uint32	getCapacity();
		bool	changeDataSource(const char *file);
	virtual	bool	seek(uint64 blockno);
	virtual	void	flush();
	virtual	int	readBlock(byte *buf);
	virtual	int	readTOC(byte *buf, bool msf, uint8 starttrack, int len,
				int format);
	virtual void	eject();

	virtual	bool	promSeek(uint64 pos);
	virtual	uint	promRead(byte *buf, uint size);
};

/// Generic interface for SCSI based implementations of a CD drive
class CDROMDeviceSCSI: public CDROMDevice {
private:
	/// Current seek position
	LBA	curLBA;

	/// Seek position for PROM (byte)
	uint64	prompos;

	/// Read ahead buffer
	byte	*data_buffer;

	/// First buffered sector
	LBA	buffer_base;

	/// Size of read ahead buffer in sectors
	uint	buffer_size;

	//////////////////////////////////
	// Internal utility functions
	//////////////////////////////////
private:
	/// Set's the simulated drive's ready state based on the physical one's
	void	checkReady();

	/// Support function for byte-wise reading from CD (for PROM)
	uint	readData(byte *buf, uint64 pos, uint count);

	/// Actual sector reading function with read-ahead buffer
	bool	readBufferedData(byte *buf, uint sector);


	//////////////////////////////////
	// Abstract interface for impls.
	//////////////////////////////////
protected:
	/// Handle SRB composition / sending / waiting
	virtual	byte	SCSI_ExecCmd(byte command, byte dir, byte params[8],
				  byte *buffer = 0, uint buffer_len = 0) = 0;

	//////////////////////////////////
	// SCSI helper functions
	//////////////////////////////////
private:
	/// Reads a number of sectors from the physical media
	bool	SCSI_ReadSectors(uint32 start, byte *buffer,
				uint buffer_bytes,
				uint sectors);

	//////////////////////////////////
	// CDROMDevice interface
	//////////////////////////////////
public:
	/// Returns the ready state
	virtual	bool	isReady();

	/// Sets the tray lock state
	virtual	bool	setLock(bool lock);

	/// Returns the size of the inserted media in sectors
	virtual	uint32	getCapacity();

	/// Sets the current seek position to the specified block
	virtual	bool	seek(uint64 blockno);

	/// Flushes the write buffer (empty function here)
	virtual	void	flush();

	/// Reads a block from the media
	virtual	int	readBlock(byte *buf);

	/// Reads the CDs TOC
	virtual	int	readTOC(byte *buffer, bool msf, uint8 starttrack,
				int len, int format);

	virtual int	getConfig(byte *buf, int len, byte RT, int first);
	virtual	int	readDVDStructure(byte *buf, int len, uint8 subcommand, uint32 address, uint8 layer, uint8 format, uint8 AGID, uint8 control);

	/// Ejects the tray
	virtual	void	eject();

	/// Seek function for PROM
	virtual	bool	promSeek(uint64 pos);

	/// Read function for PROM
	virtual	uint	promRead(byte *buf, uint size);


	//////////////////////////////////
	// Member functions
	//////////////////////////////////
protected:
	/// Constructor
	CDROMDeviceSCSI(const char *name);

	/// Destructor
	virtual ~CDROMDeviceSCSI();
};

#endif
