/* 
 *	PearPC
 *	keyboard.cc - keyboard access functions for X11
 *
 *	Copyright (C) 1999-2004 Stefan Weyergraf (stefan@weyergraf.de)
 *	Copyright (C) 1999-2004 Sebastian Biallas (sb@biallas.net)
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

#include <cstdlib>

#include "system/systhread.h"
#include "sysx11.h"

extern Display *	gX11Display;
extern Window		gX11Window;

#include "system/display.h"
#include "system/keyboard.h"

#define DPRINTF(a...)
//#define DPRINTF(a...) ht_printf(a)

class X11SystemKeyboard: public SystemKeyboard {
public:
	virtual int getKeybLEDs()
	{
		return 0;
	}

	virtual void setKeybLEDs(int leds)
	{
	}

	virtual bool handleEvent(const SystemEvent &ev)
	{
		if (ev.type != sysevKey) return false;
		if (ev.key.keycode == KEY_F12) {
			gDisplay->setMouseGrab(!gDisplay->isMouseGrabbed());
			return true;
		}
		return SystemDevice::handleEvent(ev);
	}
};

SystemKeyboard *allocSystemKeyboard()
{
	if (gKeyboard) return NULL;
	return new X11SystemKeyboard();
}
