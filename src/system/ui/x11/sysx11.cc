/* 
 *	PearPC
 *	sysx11.cc
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf
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

#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <sys/time.h>

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

#include "sysx11.h"

sys_mutex	gX11Mutex;
Display *	gX11Display = NULL;
Window		gX11Window;

static uint8 x11_key_to_adb_key[256] = {
	// 0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x35,0x12,0x13,0x14,0x15,0x17,0x16,
	0x1a,0x1c,0x19,0x1d,0x1b,0x18,0x33,0x30,0x0c,0x0d,0x0e,0x0f,0x11,0x10,0x20,0x22,
	0x1f,0x23,0x21,0x1e,0x24,0x36,0x00,0x01,0x02,0x03,0x05,0x04,0x26,0x28,0x25,0x29,
	0x27,0x32,0x38,0x2a,0x06,0x07,0x08,0x09,0x0b,0x2d,0x2e,0x2b,0x2f,0x2c,0x38,0x43,
	0x37,0x31,0xff,0x7a,0x78,0x63,0x76,0x60,0x61,0x62,0x64,0x65,0x6d,0x47,0xff,0x59,
	0x5b,0x5c,0x4e,0x56,0x57,0x58,0x45,0x53,0x54,0x55,0x52,0x41,0xff,0xff,0x0a,0x67,
	0x6f,0x73,0x3e,0x74,0x3b,0xff,0x3c,0x77,0x3d,0x79,0x72,0x75,0x4c,0x36,0xff,0xff,
	0x4b,0x3a,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
};

static void handleX11Event(const XEvent &event)
{
	static bool visible = true;
	static bool mapped = true;
	static bool mouseButton[3] = {false, false, false};
	bool tmpMouseButton[3];

	switch (event.type) {
	case GraphicsExpose:
	case Expose:
		damageFrameBufferAll();
		gDisplay->displayShow();
		break;
	case KeyRelease: {
		char buffer[4];
		SystemEvent ev;
		XComposeStatus compose;
		KeySym keysym;

		ev.key.keycode = x11_key_to_adb_key[event.xkey.keycode];
		if ((ev.key.keycode & 0xff) == 0xff) break;
		ev.type = sysevKey;
		ev.key.pressed = false;

		sys_lock_mutex(gX11Mutex);
		XLookupString((XKeyEvent*)&event, buffer, sizeof buffer, &keysym, &compose);
		sys_unlock_mutex(gX11Mutex);
		ev.key.chr = buffer[0];

		gKeyboard->handleEvent(ev);
		break;
	}
	case KeyPress: {
		char buffer[4];
		XComposeStatus compose;
		KeySym keysym;

		SystemEvent ev;
		ev.key.keycode = x11_key_to_adb_key[event.xkey.keycode];
		if ((ev.key.keycode & 0xff) == 0xff) break;
		ev.type = sysevKey;
		ev.key.pressed = true;
		ev.key.keycode = x11_key_to_adb_key[event.xkey.keycode];

		sys_lock_mutex(gX11Mutex);
		XLookupString((XKeyEvent*)&event, buffer, sizeof buffer, &keysym, &compose);
		sys_unlock_mutex(gX11Mutex);
		ev.key.chr = buffer[0];

		gKeyboard->handleEvent(ev);
		break;
	}
	case ButtonPress: {
		SystemEvent ev;
		ev.type = sysevMouse;
		memcpy(tmpMouseButton, mouseButton, sizeof (tmpMouseButton));
		switch (((XButtonEvent *)&event)->button) {
		case Button1:
			mouseButton[0] = true;
			break;
		case Button2:
			mouseButton[2] = true;
			break;
		case Button3:
			mouseButton[1] = true;
			break;
		}
		ev.mouse.type = sme_buttonPressed;
		ev.mouse.button1 = mouseButton[0];
		ev.mouse.button2 = mouseButton[1];
		ev.mouse.button3 = mouseButton[2];
		if (mouseButton[0] != tmpMouseButton[0]) {
			ev.mouse.dbutton = 1;
		} else if (mouseButton[1] != tmpMouseButton[1]) {
			ev.mouse.dbutton = 2;
		} else if (mouseButton[2] != tmpMouseButton[2]) {
			ev.mouse.dbutton = 3;
		} else {
			ev.mouse.dbutton = 0;
		}
		ev.mouse.x = gDisplay->mCurMouseX;
		ev.mouse.y = gDisplay->mCurMouseY;
		ev.mouse.relx = 0;
		ev.mouse.rely = 0;

		gMouse->handleEvent(ev);
		break;
	}
	case ButtonRelease: {
		SystemEvent ev;	
		ev.type = sysevMouse;
		memcpy(tmpMouseButton, mouseButton, sizeof (tmpMouseButton));
		switch (((XButtonEvent *)&event)->button) {
		case Button1:
			mouseButton[0] = false;
			break;
		case Button2:
			mouseButton[2] = false;
			break;
		case Button3:
			mouseButton[1] = false;
			break;
		}
		ev.mouse.type = sme_buttonReleased;
		ev.mouse.button1 = mouseButton[0];
		ev.mouse.button2 = mouseButton[1];
		ev.mouse.button3 = mouseButton[2];
		if (mouseButton[0] != tmpMouseButton[0]) {
			ev.mouse.dbutton = 1;
		} else if (mouseButton[1] != tmpMouseButton[1]) {
			ev.mouse.dbutton = 2;
		} else if (mouseButton[2] != tmpMouseButton[2]) {
			ev.mouse.dbutton = 3;
		} else {
			ev.mouse.dbutton = 0;
		}
		ev.mouse.x = gDisplay->mCurMouseX;
		ev.mouse.y = gDisplay->mCurMouseY;
		ev.mouse.relx = 0;
		ev.mouse.rely = 0;

		gMouse->handleEvent(ev);
		break;
	}
	case MotionNotify: {
		SystemEvent ev;	
		gDisplay->mCurMouseX = ev.mouse.x = ((XPointerMovedEvent *)&event)->x;
		gDisplay->mCurMouseY = ev.mouse.y = ((XPointerMovedEvent *)&event)->y;
		if (gDisplay->mCurMouseX == gDisplay->mHomeMouseX && gDisplay->mCurMouseY == gDisplay->mHomeMouseY) break;
		if (gDisplay->mCurMouseX == -1) break;
		ev.type = sysevMouse;
		ev.mouse.type = sme_motionNotify;
		ev.mouse.button1 = mouseButton[0];
		ev.mouse.button2 = mouseButton[1];
		ev.mouse.button3 = mouseButton[2];
		ev.mouse.dbutton = 0;
		ev.mouse.relx = gDisplay->mCurMouseX - gDisplay->mHomeMouseX;
		ev.mouse.rely = gDisplay->mCurMouseY - gDisplay->mHomeMouseY;
		if (gDisplay->isMouseGrabbed()) {
			sys_lock_mutex(gX11Mutex);
			XWarpPointer(gX11Display, gX11Window, gX11Window, 0, 0, 0, 0, gDisplay->mHomeMouseX, gDisplay->mHomeMouseY);
			sys_unlock_mutex(gX11Mutex);
		}

		gMouse->handleEvent(ev);
		break;
	}
	case EnterNotify:
		gDisplay->mCurMouseX = ((XEnterWindowEvent *)&event)->x;
		gDisplay->mCurMouseY = ((XEnterWindowEvent *)&event)->y;
		break;
	case LeaveNotify:
		gDisplay->mCurMouseX = gDisplay->mCurMouseY = -1;
		break;
	case FocusOut:
		if (gDisplay->isMouseGrabbed()) gDisplay->setMouseGrab(false);
		break;
	case MapNotify:
		mapped = true;
		gDisplay->setExposed(visible);
		break;
	case UnmapNotify:
		mapped = false;
		gDisplay->setExposed(false);
		break;
	case VisibilityNotify:
		visible = (event.xvisibility.state != VisibilityFullyObscured);
		gDisplay->setExposed(mapped && visible);
		break;
	}
}

static inline bool checkHandleX11Event()
{
	XEvent event;
	uint xevmask = KeyPressMask | KeyReleaseMask | ExposureMask
		| ButtonPressMask | ButtonReleaseMask | PointerMotionMask
		| EnterWindowMask | LeaveWindowMask | StructureNotifyMask
		| VisibilityChangeMask;

	sys_lock_mutex(gX11Mutex);
	if (!XCheckWindowEvent(gX11Display, gX11Window,
	xevmask, &event)) {
		sys_unlock_mutex(gX11Mutex);
		return false;
	}
	sys_unlock_mutex(gX11Mutex);

	handleX11Event(event);
	return true;
}

static void *X11eventLoop(void *p)
{
	int fd = ConnectionNumber(gX11Display);

	int redraw_interval_msec = gDisplay->mRedraw_ms;
	uint64 redraw_interval_clk = redraw_interval_msec*sys_get_hiresclk_ticks_per_second()/1000;
	uint64 clk_per_sec = sys_get_hiresclk_ticks_per_second();
	uint64 next_redraw_clk = sys_get_hiresclk_ticks();

	while (1) {
		while (1) {
			uint64 clk = sys_get_hiresclk_ticks();

			if (clk >= next_redraw_clk) {
				uint64 d = clk - next_redraw_clk;
				// We may have missed some scheduled display
				// redraws. We'll just ignore this and
				// keep drawing every 'redraw_interval_msec' msecs
				d %= redraw_interval_clk;
				next_redraw_clk = clk + redraw_interval_clk - d;
				gDisplay->displayShow();
			}
			struct timeval tm;
			fd_set zerofds;
			fd_set xfds;

			FD_ZERO(&zerofds);
			FD_ZERO(&xfds);
			FD_SET(fd, &xfds);

			uint64 x = (next_redraw_clk - clk) * 1000000 / clk_per_sec;
			tm.tv_sec = 0;
			tm.tv_usec = x;

			if (checkHandleX11Event()) break;
			if (select(fd+1, &xfds, &zerofds, &zerofds, &tm)) break;
		}

		// kind of limit the number of X events we handle to give the above
		// code a chance
		for (int i=0; i<500; i++) {
			if (!checkHandleX11Event()) break;
		}
	}
	return NULL;
}

extern SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms, bool fullscreen);
extern SystemMouse *allocSystemMouse();
extern SystemKeyboard *allocSystemKeyboard();

void initUI(const char *title, const DisplayCharacteristics &aCharacteristics, int redraw_ms, const KeyboardCharacteristics &keyConfig, bool fullscreen)
{
	// connect to X server
	char *display = getenv("DISPLAY");
	if (display == NULL) {
		display = ":0.0";
	}
	gX11Display = XOpenDisplay(display);
	if (!gX11Display) {
		ht_printf("can't open X11 display (%s)!\n", display);
		exit(1);
	}

	sys_create_mutex(&gX11Mutex);

	gDisplay = allocSystemDisplay(title, aCharacteristics, redraw_ms, fullscreen);
	gMouse = allocSystemMouse();
	gKeyboard = allocSystemKeyboard();
	if(!gKeyboard->setKeyConfig(keyConfig)) {
		ht_printf("no keyConfig, or is empty");
		exit(1);
	}
	gDisplay->updateTitle();

	sys_thread X11eventLoopThread;

	if (sys_create_thread(&X11eventLoopThread, 0, X11eventLoop, NULL)) {
		ht_printf("can't create x11 event thread!\n");
		exit(1);
	}
}

void doneUI()
{
	XCloseDisplay(gX11Display);
}
