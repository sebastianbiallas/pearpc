/*
 *	HT Editor
 *	event.h
 *
 *	Copyright (C) 2004 Stefan Weyergraf (stefan@weyergraf.de)
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

#ifndef __SYSTEM_EVENT_H__
#define __SYSTEM_EVENT_H__

#include "keyboard.h"

enum SystemEventType {
	sysevNone	= 0,
	sysevKey,
	sysevMouse
};

struct SystemEvent {
	SystemEventType type;
	union {
    		struct {
			int x;
		    	int y;
			int relx;
		    	int rely;
			bool button1; // left mouse button
			bool button2; // right mouse button
			bool button3; // middle mouse button
		} mouseEvent;
		SystemKeyEvent key;
	};
};

#endif /* __SYSTEM_EVENT_H__ */
