/* 
 *	PearPC
 *	mouse.cc
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

#include "tools/snprintf.h"
#include "mouse.h"
#include "display.h"

SystemMouse *gMouse = NULL;

bool SystemMouse::handleEvent(const SystemEvent &ev)
{
	if (ev.type != sysevMouse) return false;
	if (gDisplay->isMouseGrabbed()) {
		return SystemDevice::handleEvent(ev);
	} else {
		if (ev.mouse.type == sme_buttonReleased) {
			gDisplay->setMouseGrab(true);
			return true;
		}
		return false;
	}
}
