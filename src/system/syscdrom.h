
/*
 *	PearPC
 *	syscdrom.h
 *
 *	Abstraction for ethernet tunnel devices
 *
 *	Copyright (C) 2003,2004 Stefan Weyergraf (stefan@weyergraf.de)
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

#ifndef __SYSCDROM_H__
#define __SYSCDROM_H__

#include "system/types.h"

/* system-dependent (implementation in $MYSYSTEM/ *.cc) */
extern CDROMDevice *createNativeCDROMDevice(const char *device_name,
					    const char *image_name);

#endif /* __SYSETHTUN_H__ */
