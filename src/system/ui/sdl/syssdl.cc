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
#include "system/systimer.h"

#include "tools/snprintf.h"

#include "syssdl.h"

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
//	static bool visible = true;
//	static bool mapped = true;
	static bool mouseButton[3] = {false, false, false};
	bool tmpMouseButton[3];

	SystemEvent ev;
	switch (event.type) {
	case SDL_VIDEOEXPOSE:
		damageFrameBufferAll();
		gDisplay->displayShow();
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
	case SDL_MOUSEBUTTONDOWN:
		ev.type = sysevMouse;
		ev.mouse.type = sme_buttonPressed;
		memcpy(tmpMouseButton, mouseButton, sizeof (tmpMouseButton));
		if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(3))
			tmpMouseButton[2] = true;
		if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(2))
			tmpMouseButton[1] = true;
		if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(1))
			tmpMouseButton[0] = true;
		ev.mouse.button1 = tmpMouseButton[0];
		ev.mouse.button2 = tmpMouseButton[1];
		ev.mouse.button3 = tmpMouseButton[2];
		if (mouseButton[0] != tmpMouseButton[0]) {
			ev.mouse.dbutton = 1;
		} else if (mouseButton[1] != tmpMouseButton[1]) {
			ev.mouse.dbutton = 2;
		} else if (mouseButton[2] != tmpMouseButton[2]) {
			ev.mouse.dbutton = 3;
		} else {
			ev.mouse.dbutton = 0;
		}
		memcpy(mouseButton, tmpMouseButton, sizeof (tmpMouseButton));
		ev.mouse.x = gDisplay->mCurMouseX;
		ev.mouse.y = gDisplay->mCurMouseY;
		ev.mouse.relx = 0;
		ev.mouse.rely = 0;
		return gMouse->handleEvent(ev);
	case SDL_MOUSEBUTTONUP:
		ev.type = sysevMouse;
		ev.mouse.type = sme_buttonReleased;
		memcpy(tmpMouseButton, mouseButton, sizeof (tmpMouseButton));
		if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(3))
			tmpMouseButton[2] = false;
		if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(2))
			tmpMouseButton[1] = false;
		if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(1))
			tmpMouseButton[0] = false;
		ev.mouse.button1 = tmpMouseButton[0];
		ev.mouse.button2 = tmpMouseButton[1];
		ev.mouse.button3 = tmpMouseButton[2];
		if (mouseButton[0] != tmpMouseButton[0]) {
			ev.mouse.dbutton = 1;
		} else if (mouseButton[1] != tmpMouseButton[1]) {
			ev.mouse.dbutton = 2;
		} else if (mouseButton[2] != tmpMouseButton[2]) {
			ev.mouse.dbutton = 3;
		} else {
			ev.mouse.dbutton = 0;
		}
		memcpy(mouseButton, tmpMouseButton, sizeof (tmpMouseButton));
		ev.mouse.x = gDisplay->mCurMouseX;
		ev.mouse.y = gDisplay->mCurMouseY;
		ev.mouse.relx = 0;
		ev.mouse.rely = 0;
		return gMouse->handleEvent(ev);
	case SDL_MOUSEMOTION:
		gDisplay->mCurMouseX = ev.mouse.x = gDisplay->mHomeMouseX + event.motion.xrel;
		gDisplay->mCurMouseY = ev.mouse.y = gDisplay->mHomeMouseY + event.motion.yrel;
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
		return gMouse->handleEvent(ev);
	}
/*	static bool mouseButton[3] = {false, false, false};

	switch (event.type) {

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
	return false;
}

static void SDL_redrawCallback(sys_timer t)
{
	SDL_Event event;

	event.type = SDL_VIDEOEXPOSE;
	SDL_PushEvent(&event);
}

sys_timer gSDLRedrawTimer;

static void *SDLeventLoop(void *p)
{
	sys_create_timer(&gSDLRedrawTimer, SDL_redrawCallback);
	sys_set_timer(gSDLRedrawTimer, 0, gDisplay->mRedraw_ms*1000000, true);

	while (1) {
		SDL_Event event;
		SDL_WaitEvent(&event);
		handleSDLEvent(event);
	}
	return NULL;
}

SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms);
SystemKeyboard *allocSystemKeyboard();
SystemMouse *allocSystemMouse();

void initUI(const char *title, const DisplayCharacteristics &aCharacteristics, int redraw_ms)
{
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
		printf("SDL: Unable to init: %s\n", SDL_GetError());
		exit(1);
	}

	gDisplay = allocSystemDisplay(title, aCharacteristics, redraw_ms);
	gMouse = allocSystemMouse();
	gKeyboard = allocSystemKeyboard();

	sys_thread SDLeventLoopThread;

	if (sys_create_thread(&SDLeventLoopThread, 0, SDLeventLoop, NULL)) {
		printf("SDL: can't create event thread!\n");
		exit(1);
	}
}

void doneUI()
{
}
