/*
 *	HT Editor
 *	event.h
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

#ifndef __SYSTEM_EVENT_H__
#define __SYSTEM_EVENT_H__

#include "system/types.h"
#include "tools/data.h"

enum SystemEventType {
	sysevNone	= 0,
	sysevKey,
	sysevMouse
};

enum SystemMouseEventType {
	sme_buttonPressed,
	sme_buttonReleased,
	sme_motionNotify
};

struct SystemEvent {
	SystemEventType type;
	union {
    		struct {
			SystemMouseEventType type;
			int x;
		    	int y;
			int relx;
		    	int rely;
			bool button1;	// left mouse button
			bool button2;	// right mouse button
			bool button3;	// middle mouse button
			int  dbutton;	// mouse button that changed since last sysevMouse (0 if none)
		} mouse;
		struct {
			uint keycode;
		    	bool pressed;
			char chr;
		} key;
	};
};

class SystemEventObject: public Object {
public:
	SystemEvent	mEv;

	SystemEventObject(const SystemEvent &ev) : mEv(ev) {}
};

typedef bool (*SystemEventHandler)(const SystemEvent &ev);

#endif /* __SYSTEM_EVENT_H__ */
