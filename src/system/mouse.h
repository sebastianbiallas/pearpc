/*
 *	PearPC
 *	mouse.h
 *
 *	Copyright (C) 2004 Stefan Weyergraf
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

#ifndef __SYSTEM_MOUSE_H__
#define __SYSTEM_MOUSE_H__

#include "system/types.h"

#include "system/device.h"
#include "system/event.h"

#include "tools/data.h"

/* system-dependent (implementation in ui / $MYUI / *.cc) */
class SystemMouse: public SystemDevice {
public:
	virtual bool handleEvent(const SystemEvent &ev);
};

/* system-independent (implementation in keyboard.cc) */
extern SystemMouse *gMouse;

#endif /* __SYSTEM_KEYBOARD_H__ */
