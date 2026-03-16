/*
 *	PearPC
 *	keyboard.cc - keyboardaccess functions for POSIX
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

#include <cstdlib>

#include <SDL3/SDL.h>

#include "system/display.h"
#include "system/keyboard.h"

class SDLSystemKeyboard: public SystemKeyboard {
public:
	virtual int getKeybLEDs()
	{
		int r = 0;
		SDL_Keymod keyMods = SDL_GetModState();
		if (keyMods & SDL_KMOD_NUM)
			r |= KEYB_LED_NUM;
		if (keyMods & SDL_KMOD_CAPS)
			r |= KEYB_LED_CAPS;
		return r;
	}

	void setKeybLEDs(int leds)
	{
	}

	virtual bool handleEvent(const SystemEvent &ev)
	{
		return SystemKeyboard::handleEvent(ev);
	}
};

SystemKeyboard *allocSystemKeyboard()
{
	if (gKeyboard) return NULL;
	return new SDLSystemKeyboard();
}
