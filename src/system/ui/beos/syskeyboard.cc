/* 
 *	PearPC
 *	syskeyboard.cc - keyboard access functions for BeOS
 *
 *	Copyright (C) 1999-2004 Stefan Weyergraf (stefan@weyergraf.de)
 *	Copyright (C) 1999-2004 Sebastian Biallas (sb@biallas.net)
 *	Copyright (C) 2004 Francois Revol (revol@free.fr)
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

#include <InterfaceDefs.h>

#include "system/display.h"
#include "system/keyboard.h"

#define DPRINTF(a...)
//#define DPRINTF(a...) ht_printf(a)

class BeOSSystemKeyboard: public SystemKeyboard {
public:
	virtual int getKeybLEDs()
	{
		int r = 0;
		uint32 mods = modifiers();
		if (mods & B_CAPS_LOCK) r |= KEYB_LED_NUM;
		if (mods & B_CAPS_LOCK) r |= KEYB_LED_CAPS;
		if (mods & B_SCROLL_LOCK) r |= KEYB_LED_SCROLL;
		return r;
	}

	virtual void setKeybLEDs(int leds)
	{
		uint32 mods = 0;
		if (leds & KEYB_LED_NUM) mods |= B_CAPS_LOCK;
		if (leds & KEYB_LED_CAPS) mods |= B_CAPS_LOCK;
		if (leds & KEYB_LED_SCROLL) mods |= B_SCROLL_LOCK;
		// XXX: warning: set_keyboard_locks() doesn't work correctly
		set_keyboard_locks(mods);
	}

	virtual bool handleEvent(const SystemEvent &ev)
	{
		return SystemKeyboard::handleEvent(ev);
	}
};

SystemKeyboard *allocSystemKeyboard()
{
	if (gKeyboard) return NULL;
	return new BeOSSystemKeyboard();
}
