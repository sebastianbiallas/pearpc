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
#include <device/scsi.h>
#include <drivers/Drivers.h>
#include <drivers/CAM.h>
#include <unistd.h>
#include <fcntl.h>
#include "errno.h"

#include "debug/tracers.h"
#include "io/ide/cd.h"

#define IO_NCD_TRACE(msg...) IO_IDE_TRACE("[CDROM/BEOS] "msg)
#define IO_NCD_WARN(msg...) IO_IDE_WARN("[CDROM/BEOS] "msg)

#define SCSI_CMD_DIR_IN 1
#define SCSI_CMD_DIR_OUT 2

#define CD_FRAMESIZE 2048

/// BeOS ATAPI-based CD-ROM implementation
class CDROMDeviceBeOS:public CDROMDeviceSCSI
{
private:
	int fd;
public:
	 CDROMDeviceBeOS(const char *name);
	virtual ~CDROMDeviceBeOS();
	virtual byte SCSI_ExecCmd(byte command, byte dir, byte params[11], byte *buffer, unsigned int buffer_len);

	virtual void eject();
};

CDROMDeviceBeOS::CDROMDeviceBeOS(const char *name)
	: CDROMDeviceSCSI(name)
{
	IO_NCD_TRACE("%s()\n", __FUNCTION__);
	fd = ::open(name, O_RDONLY);
}

CDROMDeviceBeOS::~CDROMDeviceBeOS()
{
	::close(fd);
}

/// SCSI command pass-through function
/// @author Alexander Stockinger
/// @date 07/18/2004
/// @param command The SCSI command to be sent
/// @param dir The data direcion flags
/// @param params byte[11] array containing command dependent parameters
/// @param buffer Buffer for data exchange
/// @param buffer_len The size of buffer in bytes
/// @return If the call could be executed it returns the status from the device, else it returns 0xff
byte CDROMDeviceBeOS::SCSI_ExecCmd(byte command, byte dir, byte params[11], byte *buffer, unsigned int buffer_len)
{
	raw_device_command cmd;
	int ret;
	int i;
	
	memset(&cmd, 0, sizeof(cmd));
	cmd.flags = B_RAW_DEVICE_SHORT_READ_VALID;
	if (dir & SCSI_CMD_DIR_IN)
		cmd.flags |= B_RAW_DEVICE_DATA_IN;

	cmd.command[0] = command;
	if (command < 0x20)
		cmd.command_length = 6;
	else if (command < 0xa0)
		cmd.command_length = 10;
	else
		cmd.command_length = 12;
	//cmd.command_length = 10; //XXX: force
	for (i = 0; i + 1 < cmd.command_length; i++)
		cmd.command[i+1] = params[i];
	cmd.data = buffer;
	cmd.data_length = buffer_len;
	cmd.timeout = 5000000;
	ret = ::ioctl(fd, B_RAW_DEVICE_COMMAND, &cmd, sizeof(cmd));
	//IO_NCD_TRACE("command done. scsi status: 0x%08lx cam status: 0x%08lx\n", cmd.scsi_status, cmd.cam_status);
	if (ret < 0 /*|| cmd.scsi_status || cmd.cam_status*/) {
		IO_NCD_WARN("ioctl() returned error 0x%08lx (%s)\n", errno, strerror(errno));
		IO_NCD_WARN("command: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x:\n",
		command, params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9]);
		return 0xff;
	}
	return cmd.scsi_status;
}


void CDROMDeviceBeOS::eject()
{
	IO_NCD_TRACE("%s()\n", __FUNCTION__);
	::ioctl(fd, B_EJECT_DEVICE, NULL, 0);
}

/// Creates a native CDROM device
/// @param device_name The PearPC internal device name for the drive
/// @param image_name The image / device name to identify the real hardware
/// @return On success a pointer to a CDROMDevice is returned, else NULL
/// @author Alexander Stockinger
/// @date 07/19/2004
CDROMDevice* createNativeCDROMDevice(const char* device_name, const char* image_name)
{
	// IO_NCD_WARN("No native CDROMs supported on BeOS\n");
	// check for a real cdrom drive...
	device_geometry geom;
	int err;
	int fd = open(image_name, O_RDONLY);
	err = ioctl(fd, B_GET_GEOMETRY, &geom, sizeof(geom));
	close(fd);
	if (err < 0) {
		IO_NCD_WARN("Can't open CDROM '%s'\n", image_name);
		return NULL;
	}
	if (geom.device_type != B_CD) {
		IO_NCD_WARN("File '%s' not a CDROM device\n", image_name);
		return NULL;
	}
	// create it
	CDROMDevice *dev = new CDROMDeviceBeOS(image_name);
	return dev;
}
