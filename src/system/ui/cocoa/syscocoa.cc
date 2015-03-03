/* 
 *	PearPC
 *	syscocoa.cc
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf
 *	Copyright (C) 1999-2015 Sebastian Biallas (sb@biallas.net)
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

#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <cstring>

#include "system/sysclk.h"
#include "system/display.h"
#include "system/keyboard.h"
#include "system/mouse.h"

#include "tools/snprintf.h"

#include "syscocoa.h"

extern SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms, bool fullscreen);
extern SystemMouse *allocSystemMouse();
extern SystemKeyboard *allocSystemKeyboard();

void initUI(const char *title, const DisplayCharacteristics &aCharacteristics, int redraw_ms, const KeyboardCharacteristics &keyConfig, bool fullscreen)
{

	gDisplay = allocSystemDisplay(title, aCharacteristics, redraw_ms, fullscreen);
	gMouse = allocSystemMouse();
	gKeyboard = allocSystemKeyboard();
	if(!gKeyboard->setKeyConfig(keyConfig)) {
		ht_printf("no keyConfig, or is empty");
		exit(1);
	}
	gDisplay->updateTitle();

}

void doneUI()
{
}
