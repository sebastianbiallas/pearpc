/* 
 *	PearPC
 *	device.cc
 *
 *	Copyright (C) 2004 Stefan Weyergraf
 *	Copyright (C) 2003,2004 Sebastian Biallas (sb@biallas.net)
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

#include <stdio.h>

#include <tools/snprintf.h>

#include "device.h"

SystemDevice::SystemDevice()
{
	mAttachedEventHandler = NULL;
}

bool SystemDevice::handleEvent(const SystemEvent &ev)
{
	return mAttachedEventHandler ? mAttachedEventHandler(ev) : false;
}

void SystemDevice::attachEventHandler(SystemEventHandler cevh)
{
	if (mAttachedEventHandler) {
		ht_printf("INTERNAL ERROR: only 1 attached event handler allowed.\n");
		exit(1);
	}
	mAttachedEventHandler = cevh;
}
