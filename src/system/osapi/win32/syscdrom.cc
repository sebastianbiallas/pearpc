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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#ifdef FASTCALL
#undef FASTCALL
#endif

#include <cstdio>
#include <cstring>
#include "errno.h"

#include "debug/tracers.h"
#include "tools/data.h"
#include "io/ide/cd.h"
#include "aspi-win32.h"
#include "scsipt.h"
#include "scsitypes.h"

#define SCSI_CMD_DIR_IN 1
#define SCSI_CMD_DIR_OUT 2

/// SPTI CD-ROM implementation
class CDROMDeviceSPTI:public CDROMDeviceSCSI
{
private:
	// HANDLE to device
	HANDLE device;

	/// Opens a drive device
	/// @author Alexander Stockinger
	/// @date 07/18/2004
	/// @param letter The drive letter of the drive to open
	/// @return A file handle to the device
	static HANDLE OpenDevice(char letter)
	{

		// Generate the device name
		char fname[16];
		ht_snprintf(fname, sizeof fname, "\\\\.\\%c:", letter);

		// Handle access (different on NT / 2K / XP)
		OSVERSIONINFO ver;
		memset(&ver, 0, sizeof(ver));
		ver.dwOSVersionInfoSize = sizeof(ver);
		GetVersionEx(&ver);

		DWORD flags = GENERIC_READ;
		if (ver.dwPlatformId == VER_PLATFORM_WIN32_NT
		    && ver.dwMajorVersion > 4)
			flags |= GENERIC_WRITE;

		// Create the file handle
		return CreateFile(fname, flags, FILE_SHARE_READ, NULL,
				   OPEN_EXISTING, 0, NULL);
	}
	

protected:
	/// SCSI command pass-through function
	/// @author Alexander Stockinger
	/// @date 07/18/2004
	/// @param command The SCSI command to be sent
	/// @param dir The data direcion flags
	/// @param params byte[11] array containing command dependent parameters
	/// @param buffer Buffer for data exchange
	/// @param buffer_len The size of buffer in bytes
	/// @return If the call could be executed it returns the status from the device, else it returns 0xff
	virtual byte SCSI_ExecCmd(byte command, byte dir, byte params[11], byte *buffer, unsigned int buffer_len)
	{
		byte srb_dir;

		if (dir & SCSI_CMD_DIR_IN)
			srb_dir = SCSI_IOCTL_DATA_IN;
		else if (dir & SCSI_CMD_DIR_OUT)
			srb_dir = SCSI_IOCTL_DATA_OUT;
		else
			srb_dir = SCSI_IOCTL_DATA_UNSPECIFIED;

		// Fill SPTDWB
		SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER swb;

		memset(&swb, 0, sizeof(swb));
		swb.spt.Length = sizeof(SCSI_PASS_THROUGH);
		swb.spt.DataTransferLength = buffer_len;
		swb.spt.DataBuffer = buffer;
		swb.spt.TimeOutValue = 5;
		swb.spt.SenseInfoOffset =
			offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER,
				 ucSenseBuf);
		swb.spt.DataIn = srb_dir;
		if (command < 0x20) {
			swb.spt.CdbLength = 6;
		} else if (command < 0xa0) {
			swb.spt.CdbLength = 10;
		} else {
			swb.spt.CdbLength = 12;
		}
		swb.spt.Cdb[0] = command;
		for (int i=1; i < swb.spt.CdbLength; i++) {
			swb.spt.Cdb[i] = params[i-1];
		}

		// Send cmd
		ULONG ret;
		BOOL status = DeviceIoControl(device,
					      IOCTL_SCSI_PASS_THROUGH_DIRECT,
					      &swb,
					      sizeof(swb),
					      &swb,
					      sizeof(swb),
					      &ret,
					      NULL);

		// Done
		if (!status) {
			IO_IDE_WARN("SPTI: DeviceIoControl() returned error:\n");
			IO_IDE_WARN("command: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x:\n", 
				command, params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9]);
			char buffer[256];

			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0,
				      GetLastError(), 0, buffer, sizeof buffer, 0);
			IO_IDE_WARN("%s\n", buffer);
			return 0xff;
		}
		return swb.spt.ScsiStatus;
	}


public:
	/// Constructor
	/// @author Alexander Stockinger
	/// @date 07/17/2004
	/// @param name The name of the CDROM device
	CDROMDeviceSPTI (const char *name):CDROMDeviceSCSI(name),
		device(INVALID_HANDLE_VALUE)
	{
	}

	/// Destructor
	/// @author Alexander Stockinger
	/// @date 07/18/2004
	virtual ~CDROMDeviceSPTI()
	{
		setLock(false);
		if (device != INVALID_HANDLE_VALUE)
			CloseHandle(device);
	}

	/// Sets the drive letter of the real CD drive
	/// @author Alexander Stockinger
	/// @date 07/18/2004
	/// @param name The drive letter, e.g. "e:" or "e:\"
	/// @return A file handle to the device
	void setDrive(const char *name)
	{
		// Check parameter
		size_t len = strlen(name);

		if (len != 2 && len != 3)
			IO_IDE_ERR("SPTI: Invalid drive name '%s'\n", name);

		if (name[1] != ':')
			IO_IDE_ERR("SPTI: Invalid drive name '%s'\n", name);
		if (len == 3 && name[2] != '\\')
			IO_IDE_ERR("SPTI: Invalid drive name '%s'\n", name);

		char letter = name[0];

		if (letter >= 'a' && letter <= 'z')
			letter -= ('a' - 'A');
		if (letter < 'A' || letter > 'Z')
			IO_IDE_ERR("SPTI: Invalid drive '%c:'\n", letter);

		// Open the device
		device = OpenDevice(letter);
		if (device == INVALID_HANDLE_VALUE)
			IO_IDE_ERR("SPTI: Cannot open drive '%c:'\n", letter);
	}

};


/// ASPI CD-ROM implementation
class CDROMDeviceASPI:public CDROMDeviceSCSI
{
private:
	/// Module handle to ASPI DLL
	HMODULE hASPI;

	/// SCSI host adapter ID
	unsigned int a;

	/// SCSI target ID
	unsigned int t;

	/// SCSI lun ID
	unsigned int l;

protected:
	/// SCSI command pass-trough function
	/// @author Alexander Stockinger
	/// @date 07/17/2004
	/// @param command The SCSI command to be sent
	/// @param dir The data direcion flags
	/// @param params byte[8] array containing command dependent parameters
	/// @param buffer Buffer for data exchange
	/// @param buffer_len The size of buffer in bytes
	/// @return If the call could be executed it returns the status from the device, else it returns 0xff
	virtual byte SCSI_ExecCmd(byte command, byte dir, byte params[8],
				  byte *buffer, unsigned int buffer_len)
	{
		byte srb_dir;
		if (dir & SCSI_CMD_DIR_IN)
			 srb_dir = SRB_DIR_IN;
		else if (dir & SCSI_CMD_DIR_OUT)
			 srb_dir = SRB_DIR_OUT;
		else
			 srb_dir = 0;


		// Create event
		HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
		ResetEvent(event);

		// Prepare SRB
		SRB_ExecSCSICmd cmd;
		memset(&cmd, 0, sizeof(cmd));
		cmd.SRB_Cmd = SC_EXEC_SCSI_CMD;
		cmd.SRB_HaId = a;
		cmd.SRB_Target = t;
		cmd.SRB_Lun = l;
		cmd.SRB_Flags = srb_dir | SRB_EVENT_NOTIFY;
		cmd.SRB_SenseLen = SENSE_LEN;
		cmd.SRB_PostProc = event;
		cmd.SRB_BufPointer = buffer;
		cmd.SRB_BufLen = buffer_len;
		cmd.SRB_CDBLen = 10;
		cmd.CDBByte[0] = command;
		cmd.CDBByte[1] = params[0];
		cmd.CDBByte[2] = params[1];
		cmd.CDBByte[3] = params[2];
		cmd.CDBByte[4] = params[3];
		cmd.CDBByte[5] = params[4];
		cmd.CDBByte[6] = params[5];
		cmd.CDBByte[7] = params[6];
		cmd.CDBByte[8] = params[7];

		// Send cmd and wait for event
		DWORD status = SendASPI32Command((LPSRB) & cmd);
		if (status == SS_PENDING)
			WaitForSingleObject(event, 100000);

		// Clean up
		CloseHandle(event);

		// Check error conditions
		if (status != SS_COMP && status != SS_PENDING)
			 return 0xff;

		// Return error code
		return cmd.SRB_TargStat;
	}
public:
	/// Constructor
	/// @author Alexander Stockinger
	/// @date 07/13/2004
	/// @param name The name of the CDROM device
	CDROMDeviceASPI(const char *name):CDROMDeviceSCSI(name)
	{
		// See if ASPI is available
		hASPI = (HMODULE) LoadLibrary("wnaspi32.dll");
		if (hASPI == INVALID_HANDLE_VALUE || hASPI == 0)
			IO_IDE_ERR("No ASPI support (Could not load wnaspi32.dll).\n");

		// Load ASPI function addresses
		SendASPI32Command =
			(DWORD(*)(LPSRB)) GetProcAddress(hASPI,
							 "SendASPI32Command");
		GetASPI32DLLVersion =
			(DWORD(*)(void)) GetProcAddress(hASPI,
							"GetASPI32DLLVersion");
		GetASPI32SupportInfo =
			(DWORD(*)(void)) GetProcAddress(hASPI,
							"GetASPI32SupportInfo");

		if (!SendASPI32Command || !GetASPI32DLLVersion
		    || !GetASPI32SupportInfo)
			IO_IDE_ERR("Error loading wnaspi32.dll\n");

		// Make sure the init function is being called
		GetASPI32SupportInfo();
	}

	/// Destructor
	/// @author Alexander Stockinger
	/// @date 07/13/2004
	virtual ~CDROMDeviceASPI()
	{
		setLock(false);
		if (hASPI != INVALID_HANDLE_VALUE)
			FreeLibrary(hASPI);
	}

	/// Checks if the ASPI device is a CD drive
	/// @author Alexander Stockinger
	/// @date 07/17/2004
	/// @return true if the specified device is CD drive, else false
	bool ASPI_IsDeviceCDROM()
	{
		// Get adapter cound
		const DWORD info = GetASPI32SupportInfo();
		const byte adapter_count LOBYTE(LOWORD(info));

		// Make sure the host adapter exists
		if (a >= adapter_count)
			return false;

		// Get device type
		SRB_GDEVBlock desc;

		memset(&desc, 0, sizeof(desc));
		desc.SRB_Cmd = SC_GET_DEV_TYPE;
		desc.SRB_HaId = a;
		desc.SRB_Target = t;
		desc.SRB_Lun = l;
		SendASPI32Command((LPSRB) & desc);

		// False on errors
		if (desc.SRB_Status != SS_COMP)
			return false;

		// Done
		return desc.SRB_DeviceType == DTYPE_CDROM;
	}

	/// Sets the SCSI target for the ASPI device
	/// @author Alexander Stockinger
	/// @date 07/13/2004
	/// @param name The SCSI device id in the form "a,t,l"
	void setSCSITarget(const char *name)
	{
		// Extract SCSI device info
		char *spath = new char[strlen(name) + 1];

		strcpy(spath, name);
		char *scsi[3] = { spath, 0, 0 };
		int i = 0;

		for (char *c = spath; *c; c++) {
			if (i == 3)
				break;
			if (*c == ',') {
				i++;
				*c = 0;
				c++;
				scsi[i] = c;
			}
		}

		if (scsi[1] == 0 || scsi[2] == 0)
			IO_IDE_ERR("Invalid SCSI path: %s", name);

		a = atoi(scsi[0]);
		t = atoi(scsi[1]);
		l = atoi(scsi[2]);
		delete[]spath;

		// Check device type
		if (!ASPI_IsDeviceCDROM())
			IO_IDE_WARN("SCSI device %d/%d/%d is not a CDROM drive.\n",
				 a, t, l);

		// Set initial ready state
		isReady();
	}
};

/// Creates a native CDROM device
/// @param device_name The PearPC internal device name for the drive
/// @param image_name The image / device name to identify the real hardware
/// @return On success a pointer to a CDROMDevice is returned, else NULL
/// @author Alexander Stockinger
/// @date 07/19/2004
CDROMDevice *createNativeCDROMDevice(const char *device_name,
				     const char *image_name)
{
	if (strlen(image_name) < 2)
		return NULL;

	CDROMDevice *ret = NULL;

	if (image_name[1] == ':') {
		CDROMDeviceSPTI *spti = new CDROMDeviceSPTI(device_name);

		spti->setDrive(image_name);
		ret = spti;
	} else {
		CDROMDeviceASPI *aspi = new CDROMDeviceASPI(device_name);

		aspi->setSCSITarget(image_name);
		ret = aspi;
	}

	return ret;
}
