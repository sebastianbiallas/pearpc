/*
 *	PearPC
 *	sysethtun.cc
 *
 *	win32-specific ethernet-tunnel access
 *
 *	Copyright (C) 2003 Stefan Weyergraf (stefan@weyergraf.de)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// networking on Win32 not yet implemented

#include <errno.h>

#include "system/sysethtun.h"

static int pdnull_open(enet_iface_t *is, char *intf_name, int *sigio_capable, const byte *mac)
{
	return ENOSYS;
}

packet_driver_t g_sys_ethtun_pd = {
	name:			"null",
	open: 			pdnull_open,
};
