/* 
 *	PearPC
 *	sysx11.cc
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf (stefan@weyergraf.de)
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

#include <SDL/SDL.h>

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

#include "syssdl.h"

sys_mutex	gSDLMutex;
SDL_Surface *	gSDLScreen;

static uint8 sdl_key_to_adb_key[256] = {
	// 0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x35,0x12,0x13,0x14,0x15,0x17,0x16,
	0x1a,0x1c,0x19,0x1d,0x1b,0x18,0x33,0x30,0x0c,0x0d,0x0e,0x0f,0x11,0x10,0x20,0x22,
	0x1f,0x23,0x21,0x1e,0x24,0x36,0x00,0x01,0x02,0x03,0x05,0x04,0x26,0x28,0x25,0x29,
	0x27,0x32,0x38,0x2a,0x06,0x07,0x08,0x09,0x0b,0x2d,0x2e,0x2b,0x2f,0x2c,0x38,0x43,
	0x3a,0x31,0xff,0x7a,0x78,0x63,0x76,0x60,0x61,0x62,0x64,0x65,0x6d,0x47,0xff,0x59,
	0x5b,0x5c,0x4e,0x56,0x57,0x58,0x45,0x53,0x54,0x55,0x52,0x41,0xff,0xff,0xff,0x67,
	0x6f,0x73,0x3e,0x74,0x3b,0xff,0x3c,0x77,0x3d,0x79,0x72,0x75,0x4c,0x36,0xff,0xff,
	0x4b,0x37,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
};

static bool handleSDLEvent(const SDL_Event &event)
{
	SystemEvent ev;
	switch (event.type) {
	case SDL_VIDEOEXPOSE:
		if(SDL_MUSTLOCK(gSDLScreen))
			SDL_LockSurface(gSDLScreen);
		damageFrameBufferAll();
		gDisplay->displayShow();
		if(SDL_MUSTLOCK(gSDLScreen))
			SDL_UnlockSurface(gSDLScreen);
		return true;
	case SDL_QUIT: // should we trap this and send power key?
		SDL_WM_GrabInput(SDL_GRAB_OFF);
		exit(0);
		return true;
	case SDL_KEYUP:
		ev.key.keycode = sdl_key_to_adb_key[event.key.keysym.scancode];
		if ((ev.key.keycode & 0xff) == 0xff) break;
		ev.type = sysevKey;
		ev.key.pressed = false;
		ev.key.chr = event.key.keysym.unicode;
		return gKeyboard->handleEvent(ev);
	case SDL_KEYDOWN:
		ev.key.keycode = sdl_key_to_adb_key[event.key.keysym.scancode];
//		if (event.key.keysym.mod & KMOD_SHIFT){
/*			if (event.key.keysym.sym == SDLK_F9) {
				if(SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_OFF) {
					SDL_WM_GrabInput(SDL_GRAB_ON);
					DPRINTF("Grab on!\n");
					break;;
				}
				SDL_WM_GrabInput(SDL_GRAB_OFF);
				DPRINTF("Grab off!\n");
				break;
			}*/
//			if (event.key.keysym.sym == SDLK_F12) 
//				ToggleFullScreen();
//		}
//		if(event.key.keysym.sym == SDLK_F1) { changeCDFunc(); }                                
		if ((ev.key.keycode & 0xff) == 0xff) break;
		ev.type = sysevKey;
		ev.key.pressed = true;
		ev.key.keycode = sdl_key_to_adb_key[event.key.keysym.scancode];
		ev.key.chr = event.key.keysym.unicode;
		return gKeyboard->handleEvent(ev);
	default:
		handleSDLEvent(event);
	}
/*	static bool mouseButton[3] = {false, false, false};

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

		sys_lock_mutex(gSDLMutex);
		XLookupString((XKeyEvent*)&event, buffer, sizeof buffer, &keysym, &compose);
		sys_unlock_mutex(gSDLMutex);
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

		sys_lock_mutex(gSDLMutex);
		XLookupString((XKeyEvent*)&event, buffer, sizeof buffer, &keysym, &compose);
		sys_unlock_mutex(gSDLMutex);
		ev.key.chr = buffer[0];

		gKeyboard->handleEvent(ev);
		break;
	}
	case ButtonPress: {
		SystemEvent ev;
		ev.type = sysevMouse;
		switch (((XButtonEvent *)&event)->button) {
		case Button1:
			mouseButton[0] = true;
			break;
		case Button2:
			mouseButton[1] = true;
			break;
		case Button3:
			mouseButton[2] = true;
			break;
		}
		ev.mouse.type = sme_buttonPressed;
		ev.mouse.button1 = mouseButton[0];
		ev.mouse.button2 = mouseButton[2];
		ev.mouse.button3 = mouseButton[1];
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
		switch (((XButtonEvent *)&event)->button) {
		case Button1:
			mouseButton[0] = false;
			break;
		case Button2:
			mouseButton[1] = false;
			break;
		case Button3:
			mouseButton[2] = false;
			break;
		}
		ev.mouse.type = sme_buttonReleased;
		ev.mouse.button1 = mouseButton[0];
		ev.mouse.button2 = mouseButton[2];
		ev.mouse.button3 = mouseButton[1];
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
		ev.mouse.relx = gDisplay->mCurMouseX - gDisplay->mHomeMouseX;
		ev.mouse.rely = gDisplay->mCurMouseY - gDisplay->mHomeMouseY;
		if (gDisplay->isMouseGrabbed()) {
			sys_lock_mutex(gSDLMutex);
			XWarpPointer(gSDLScreen, gSDLWindow, gSDLWindow, 0, 0, 0, 0, gDisplay->mHomeMouseX, gDisplay->mHomeMouseY);
			sys_unlock_mutex(gSDLMutex);
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
		gDisplay->setExposed(true);
		break;
	case UnmapNotify:
		gDisplay->setExposed(false);
		break;
	case VisibilityNotify: {
		bool visible = (event.xvisibility.state != VisibilityFullyObscured);
		gDisplay->setExposed(visible);
		break;
	}
	}*/
}

static Uint32 SDL_redrawCallback(Uint32 interval, void *param)
{
	SDL_Event event;

	event.type = SDL_VIDEOEXPOSE;
	SDL_PushEvent(&event);
	return 0;
}

static void *SDLeventLoop(void *p)
{
	int redraw_interval_msec = gDisplay->mRedraw_ms;

//	SDL_AddTimer(redraw_interval_msec, SDL_redrawCallback, NULL);

	while (1) {
		SDL_Event event;
		SDL_WaitEvent(&event);
		handleSDLEvent(event);
	}
	return NULL;
}

void initUI()
{
	// connect to X server
/*	char *display = getenv("DISPLAY");
	if (display == NULL) {
		display = ":0.0";
	}
	gSDLScreen = XOpenDisplay(display);
	if (!gSDLScreen) {
		printf("can't open SDL display (%s)!\n", display);
		exit(1);
	}*/
}

void startUI()
{
	sys_thread SDLeventLoopThread;

	if (sys_create_thread(&SDLeventLoopThread, 0, SDLeventLoop, NULL)) {
		printf("can't create x11 event thread!\n");
		exit(1);
	}
}

void doneUI()
{
//	XCloseDisplay(gSDLScreen);
}
