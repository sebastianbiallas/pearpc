/*
 *	PearPC
 *	syscdrom.cc
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
#include <Drivers.h>
#include <unistd.h>
#include <fcntl.h>
#include "errno.h"

#include "debug/tracers.h"
#include "io/ide/cd.h"

/// BeOS ATAPI-based CD-ROM implementation
class CDROMDeviceBeOS:public CDROMDeviceFile
{
public:
	 CDROMDeviceBeOS(const char *name);
	virtual ~CDROMDeviceBeOS();

	bool changeDataSource(const char *file);
	virtual void eject();
};

CDROMDeviceBeOS::CDROMDeviceBeOS(const char *name)
	: CDROMDeviceFile(name)
{
	changeDataSource(name); // base class doesn't open on create
}

CDROMDeviceBeOS::~CDROMDeviceBeOS()
{
}

bool CDROMDeviceBeOS::changeDataSource(const char *file)
{
	return CDROMDeviceFile::changeDataSource(file);
}

void CDROMDeviceBeOS::eject()
{
	//if (!mFile) return; // no file open
	// annoying: mFile is private, must open() again...
	int fd = open(mName, O_RDWR);
	if (fd < 0)
		return;
	ioctl(fd, B_EJECT_DEVICE, NULL, 0);
	// we don't care if it worked (might be a regular file)
	close(fd);
}

/// Creates a native CDROM device
/// @param device_name The PearPC internal device name for the drive
/// @param image_name The image / device name to identify the real hardware
/// @return On success a pointer to a CDROMDevice is returned, else NULL
/// @author Alexander Stockinger
/// @date 07/19/2004
CDROMDevice* createNativeCDROMDevice(const char* device_name, const char* image_name)
{
	// IO_IDE_WARN("No native CDROMs supported on BeOS\n");
	// check for a real cdrom drive...
	device_geometry geom;
	int err;
	int fd = open(device_name, O_RDONLY);
	err = ioctl(fd, B_GET_GEOMETRY, &geom, sizeof(geom));
	close(fd);
	if (err < 0) {
		IO_IDE_WARN("Can't open CDROM '%s'\n", device_name);
		return NULL;
	}
	if (geom.device_type != B_CD) {
		IO_IDE_WARN("File '%s' not a CDROM device\n", device_name);
		return NULL;
	}
	// create it
	CDROMDevice *dev = new CDROMDeviceBeOS(device_name);
	return dev;
}
