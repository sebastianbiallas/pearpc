/* 
 *	PearPC
 *	keyboard.cc - keyboard access functions for Windows
 *
 *	Copyright (C) 1999-2004 Stefan Weyergraf
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#undef FASTCALL

#include <cstdlib>

#include "system/systhread.h"
#include "syswin.h"

#include "system/display.h"
#include "system/keyboard.h"

class WinSystemKeyboard: public SystemKeyboard {
public:
	virtual int  getKeybLEDs()
	{
		int r = 0;
		if (GetAsyncKeyState(VK_NUMLOCK) & 1) r |= KEYB_LED_NUM;
		if (GetAsyncKeyState(VK_CAPITAL) & 1) r |= KEYB_LED_CAPS;
		if (GetAsyncKeyState(VK_SCROLL) & 1) r |= KEYB_LED_SCROLL;
		return r;
	}

	virtual void setKeybLEDs(int leds)
	{
		int r = getKeybLEDs() ^ leds;
		if (r & KEYB_LED_NUM) {
			keybd_event(VK_NUMLOCK, MapVirtualKey(VK_NUMLOCK, 0), KEYEVENTF_EXTENDEDKEY, 0);
			keybd_event(VK_NUMLOCK, MapVirtualKey(VK_NUMLOCK, 0), KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
		}
		if (r & KEYB_LED_CAPS) {
			keybd_event(VK_CAPITAL, MapVirtualKey(VK_CAPITAL, 0), KEYEVENTF_EXTENDEDKEY, 0);
			keybd_event(VK_CAPITAL, MapVirtualKey(VK_CAPITAL, 0), KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
		}
		if (r & KEYB_LED_SCROLL) {
			keybd_event(VK_SCROLL, MapVirtualKey(VK_SCROLL, 0), KEYEVENTF_EXTENDEDKEY, 0);
			keybd_event(VK_SCROLL, MapVirtualKey(VK_SCROLL, 0), KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
		}
	}

	virtual bool handleEvent(const SystemEvent &ev)
	{
		return SystemKeyboard::handleEvent(ev);
	}
};

SystemKeyboard *allocSystemKeyboard()
{
	if (gKeyboard) return NULL;
	return new WinSystemKeyboard();
}
