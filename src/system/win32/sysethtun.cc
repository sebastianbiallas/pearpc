/*
 *	PearPC
 *	sysethtun.cc
 *
 *	win32-specific ethernet-tunnel access
 *
 *	Copyright (C) 2004 John Kelley (pearpc@kelley.ca)
 *	Copyright (C) 2004 Stefan Weyergraf (stefan@weyergraf.de)
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

// FIXME: What about multiple instances of Win32EthTunDevice?

#include <errno.h>
#include <stdio.h>
#include <winsock.h>
#include <windows.h>
#include <Winioctl.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "system/sysethtun.h"
#include "tools/except.h"
#include "tap_constants.h"
#include "tools/snprintf.h"

//minimum version that we will work with
#define TAP_WIN32_MIN_MAJOR 7
#define TAP_WIN32_MIN_MINOR 1

#define printm(s...) printf("[TAP-WIN32]: "s)
#define BUFFER_SIZE	65536
#define READ_SIZE	16384
#define ERRORMSG_SIZE	1024

//simple function to translate an error code into a string
static void GetErrorString(char *out, DWORD error) 
{
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    		  	NULL,
    			error,
    			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
    			(LPTSTR) out,
    			ERRORMSG_SIZE,
    			NULL );
}
//checks to see if a given NIC GUID is a TAP-WIN32 device
static bool is_tap_win32_dev(const char *guid)
{
	HKEY netcard_key;
	LONG status;
	DWORD len;
	int i = 0;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, ADAPTER_KEY, 0, KEY_READ, &netcard_key);

	if (status != ERROR_SUCCESS) {
		printm("Error opening registry key: %s\n", ADAPTER_KEY);
		return false;
	}

	while (true) {
		char enum_name[256];
		char unit_string[256];
		HKEY unit_key;
		char component_id_string[] = "ComponentId";
		char component_id[256];
		char net_cfg_instance_id_string[] = "NetCfgInstanceId";
		char net_cfg_instance_id[256];
		DWORD data_type;

		len = sizeof (enum_name);
		status = RegEnumKeyEx(netcard_key, i, enum_name, &len, NULL, NULL, NULL, NULL);
		if (status == ERROR_NO_MORE_ITEMS)
			break;
		else if (status != ERROR_SUCCESS) {
			printm("Error enumerating registry subkeys of key: %s\n",
				ADAPTER_KEY);
			return false;
		}
	
		snprintf (unit_string, sizeof(unit_string), "%s\\%s", ADAPTER_KEY, enum_name);

		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, unit_string, 0, KEY_READ, &unit_key);

		if (status != ERROR_SUCCESS) {
			printm("Error opening registry key: %s\n", unit_string); 
			return false;
		} else {
			len = sizeof (component_id);
			status = RegQueryValueEx(unit_key, component_id_string, NULL, &data_type, (BYTE *)component_id,	&len);
			if (!(status != ERROR_SUCCESS || data_type != REG_SZ)) {
				len = sizeof (net_cfg_instance_id);
				status = RegQueryValueEx(unit_key, net_cfg_instance_id_string, NULL, &data_type, (BYTE *)net_cfg_instance_id,	&len);

				if (status == ERROR_SUCCESS && data_type == REG_SZ)
				{
					if ((!strcmp (component_id, "tap") || !strcmp (component_id, "tapdev"))
						&& !strcmp (net_cfg_instance_id, guid))	{
						RegCloseKey (unit_key);
						RegCloseKey (netcard_key);
						return true;
					}
				}
			}
			RegCloseKey (unit_key);
		}
		++i;
	}
	RegCloseKey (netcard_key);
	return false;
}


static int get_device_guid(char *name, int name_size, char *actual_name, int actual_name_size)
{
	LONG status;
	HKEY control_net_key;
	DWORD len;
	int i = 0;
	int stop = 0;

	status = RegOpenKeyEx( HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY,	0, KEY_READ, &control_net_key);

	if (status != ERROR_SUCCESS) {
		printm("Error opening registry key: %s", NETWORK_CONNECTIONS_KEY);
		return 1;
	}

	while (!stop)
	{
		char enum_name[256];
		char connection_string[256];
		HKEY connection_key;
		char name_data[256];
		DWORD name_type;
		const char name_string[] = "Name";

		len = sizeof (enum_name);
		status = RegEnumKeyEx(control_net_key, i, enum_name, &len, NULL, NULL, NULL, NULL);

		if (status == ERROR_NO_MORE_ITEMS)
			break;
		else if (status != ERROR_SUCCESS) {
			printm("Error enumerating registry subkeys of key: %s", NETWORK_CONNECTIONS_KEY);
			return 1;
		}

		snprintf(connection_string, sizeof(connection_string),"%s\\%s\\Connection", NETWORK_CONNECTIONS_KEY, enum_name);
		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, connection_string, 0, KEY_READ, &connection_key);
		
		if (status == ERROR_SUCCESS) {
			len = sizeof (name_data);
			status = RegQueryValueEx(connection_key, name_string, NULL, &name_type, (BYTE *)name_data, &len);

			if (status != ERROR_SUCCESS || name_type != REG_SZ) {
				printm("Error opening registry key: %s\\%s\\%s", NETWORK_CONNECTIONS_KEY, connection_string, name_string);
			        return 1;
			} else {
				if (is_tap_win32_dev (enum_name)) {
					printm("Found TAP device named '%s'\n", name_data);
					snprintf(name, name_size, "%s", enum_name);
					if (actual_name)
						snprintf(actual_name, actual_name_size, "%s", name_data);
					stop = 1;
				}
			}
			RegCloseKey (connection_key);
		}
		++i;
	}
	RegCloseKey (control_net_key);

	if (stop == 0)
		return 1;

	return 0; 
}

class Win32EthTunDevice: public EthTunDevice {
protected:
	HANDLE		mFile;
	unsigned char	mBuf[BUFFER_SIZE];
	DWORD		mBuflen;
	OVERLAPPED	mOverlapped;

//sets the media status of the TAP device (cable connected or not)
bool tap_set_status(ULONG status)
{
	unsigned long len = 0;
	bool ret;
	ret = DeviceIoControl(mFile, TAP_IOCTL_SET_MEDIA_STATUS,
				&status, sizeof (status),
				&status, sizeof (status), &len, NULL);
	if (!ret){
		char errmsg[ERRORMSG_SIZE];
		GetErrorString(errmsg, GetLastError());
		printm("Failed: %s\n", errmsg);
	}
	return ret;
}

public:

Win32EthTunDevice()
{
}

virtual ~Win32EthTunDevice()
{
	if (mFile != INVALID_HANDLE_VALUE)
		shutdownDevice();
}

int initDevice()
{
	char device_path[256];
	char device_guid[0x100];
	int rc;
	HANDLE handle = NULL;

	printm("Enumerating TAP devices...\n");
	rc = get_device_guid(device_guid, sizeof device_guid, NULL, 0);
	if (rc != 0) {
		throw new MsgException("Could not locate any installed TAP-WIN32 devices.");
	}

	 //Open Windows TAP-Win32 adapter
	 ht_snprintf(device_path, sizeof device_path, "%s%s%s",
		  USERMODEDEVICEDIR,
		  device_guid,
		  TAPSUFFIX);
		  
	handle = CreateFile(
		device_path,
		GENERIC_READ | GENERIC_WRITE,
		0,
		0,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED,
		0);

	if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
		throw new MsgException("Opening TAP connection failed");
	}

	mFile = handle;
	mOverlapped.Offset = 0;
	mOverlapped.OffsetHigh = 0;
	mOverlapped.hEvent = CreateEvent(NULL, TRUE, false, NULL);
	
	//check TAP driver version against our minimum supported version
	{
    		ULONG info[3];
    		ULONG len;
    		info[0] = info[1] = info[2] = 0;
    		if (DeviceIoControl (handle, TAP_IOCTL_GET_VERSION, 
    			&info, sizeof (info), 
    			&info, sizeof (info), &len, NULL)) {
			printm("Driver Version %d.%d\n",
	     			(int) info[0],
	     			(int) info[1]);
      		} else {
      			if (DeviceIoControl (handle, OLD_TAP_IOCTL_GET_VERSION, 
    			&info, sizeof (info), 
    			&info, sizeof (info), &len, NULL)) {
			printm("Driver Version %d.%d %s.\n",
	     			(int) info[0],
	     			(int) info[1],
		     		(info[2] ? "(DEBUG) " : ""));

      			} else {
				char errmsg[ERRORMSG_SIZE];
				GetErrorString(errmsg, GetLastError());
				throw new MsgfException("Could not get driver version info: %s\n", errmsg);
			}
		}
    		if ( !(info[0] > TAP_WIN32_MIN_MAJOR || (info[0] == TAP_WIN32_MIN_MAJOR && info[1] >= TAP_WIN32_MIN_MINOR)) ) {
      			throw new MsgfException("ERROR:  This version of PearPC requires a TAP-Win32 driver that is at least version %d.%d\n"
      						"Please install an updated version from http://prdownloads.sourceforge.net/openvpn/openvpn-2.0_beta2-install.exe\n",
			   			TAP_WIN32_MIN_MAJOR,
	   					TAP_WIN32_MIN_MINOR);
		}
  	}
  	
  	//connect our virtual cat5 cable to the TAP device
	if (!tap_set_status(true)) {
		if (CloseHandle(handle) != 1) {
			printm("Error closing handle.\n");
		}
		throw new MsgfException("Setting Media Status to connected failed (handle is %d)\n", handle);
	}
	return 0;
}

int shutdownDevice()
{
	printm("Setting Media Status to disconnected.\n");
	if (!tap_set_status(false)) {
		printm("Error disconnecting media.\n");
	}
	printm("Closing TAP-WIN32 handle.\n");
	CloseHandle(mFile);
	mFile = INVALID_HANDLE_VALUE;
	return 0;
}

virtual	uint recvPacket(void *buf, uint size)
{
	if (mBuflen > size) {
		// no partial packets. drop it.
		mBuflen = 0;
		return 0;
	}
	memcpy(buf, mBuf, mBuflen);
	uint ret = mBuflen;
	mBuflen = 0;
	return ret;
}

virtual	int waitRecvPacket()
{
	DWORD status;
	mOverlapped.Offset = 0;
	mOverlapped.OffsetHigh = 0;
	ResetEvent(mOverlapped.hEvent);
	status = ReadFile(mFile, mBuf, READ_SIZE, &mBuflen, &mOverlapped);
	if (!status) {
		DWORD e = GetLastError();
		if (e == ERROR_IO_PENDING) {
			WaitForSingleObject(mOverlapped.hEvent, INFINITE);
			if (!GetOverlappedResult(mFile, &mOverlapped, &mBuflen, FALSE)) {
				printm("You should never see this error\n");
			}
		} else {
			char errmsg[ERRORMSG_SIZE];
			GetErrorString(errmsg, e);
			printm("Bad read error: %s\n", errmsg);

			return EIO;
		}
	}
	return 0;
}

virtual	uint sendPacket(void *buf, uint size)
{
	DWORD written;
	BOOL ret;
	OVERLAPPED wrov = {0};
	ret = WriteFile(mFile, buf, size, &written, &wrov);
	if (!ret) {
		char errmsg[ERRORMSG_SIZE];
		GetErrorString(errmsg, GetLastError());
		printm("Sending of %d bytes failed (%d bytes sent): %s\n", size, written, errmsg);
	}
	return written;
}

virtual	uint getWriteFramePrefix()
{
	return 0;
}

}; // end of Win32EthTunDevice

EthTunDevice *createEthernetTunnel()
{
	return new Win32EthTunDevice();
}
