/* 
 *	PearPC
 *	keyboard.cc
 *
 *	Copyright (C) 2004 Stefan Weyergraf (stefan@weyergraf.de)
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

#include "keyboard.h"

SystemKeyboard *gKeyboard = NULL;

static char *key_names[] = {
"A","S","D","F","H","G","Z","X","C","V",
NULL,"B","Q","W","E","R","Y","T","1","2",
"3","4","6","5","=","9","7","-","8","0","]",
"O","U","[","I","P","Return","L","J","'","K",
";","\\",",","/","N","M",".","Tab","Space",
"`", "Backspace",NULL,"Escape","Ctrl","Alt","Shift","Caps-Lock","Right-Alt","Left",
"Right","Down","Up",NULL,NULL,"Keypad .",NULL,"Keypad *",NULL,"Keypad +",
NULL,"Numlock",NULL,NULL,NULL,"Keypad /","Keypad Enter",NULL,"Keypad -",NULL,
NULL,NULL,"Keypad 0","Keypad 1","Keypad 2","Keypad 3","Keypad 4","Keypad 5","Keypad 6","Keypad 7",
NULL,"Keypad 8","Keypad 9",NULL,NULL,NULL,"F5","F6","F7","F3",
"F8","F9",NULL,"F11",NULL,"F13",NULL,"Scrolllock",NULL,"F10",
NULL,"F12",NULL,"Pause","Insert","Home","Pageup","Delete","F4","End",
"F2","Pagedown","F1",
};

bool SystemKeyboard::handleEvent(const SystemEvent &ev)
{
	if ((ev.type == sysevKey) && (ev.key.keycode == KEY_F12)) {
		// do sth.
		return true;
	} else {
		return SystemDevice::handleEvent(ev);
	}
}
