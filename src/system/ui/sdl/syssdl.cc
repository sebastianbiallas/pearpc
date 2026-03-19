/*
 *	PearPC
 *	syssdl.cc
 *
 *	Copyright (C)      2004 Jens v.d. Heydt (mailme@vdh-webservice.de)
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

#include <SDL3/SDL.h>

#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <cstring>

// for stopping the CPU
#include "cpu/cpu.h"

#include "system/sysclk.h"
#include "system/display.h"
#include "system/keyboard.h"
#include "system/mouse.h"
#include "system/systhread.h"
#include "system/systimer.h"

#include "tools/snprintf.h"

#include "syssdl.h"

SDL_Window *	gSDLWindow = NULL;
SDL_Renderer *	gSDLRenderer = NULL;
SDL_Texture *	gSDLTexture = NULL;

SDLSystemDisplay *sd;

static uint8 scancode_to_adb_key[256] = {
	// 0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
	0xff,0xff,0xff,0xff,0x00,0x0b,0x08,0x02,0x0e,0x03,0x05,0x04,0x22,0x26,0x28,0x25,
	0x2e,0x2d,0x1f,0x23,0x0c,0x0f,0x01,0x11,0x20,0x09,0x0d,0x07,0x10,0x06,0x12,0x13,
	0x14,0x15,0x17,0x16,0x1a,0x1c,0x19,0x1d,0x24,0x35,0x33,0x30,0x31,0x1b,0x18,0x21,
	0x1e,0x2a,0x2a,0x27,0x29,0x32,0x2b,0x2f,0x2c,0x39,0x7a,0x78,0x63,0x76,0x60,0x61,
	0x62,0x64,0x65,0x6d,0x67,0x6f,0xff,0xff,0xff,0x72,0x73,0x7b,0x75,0x77,0x74,0x3c,
	0x3b,0x3d,0x3e,0x47,0x4b,0x43,0x4e,0x45,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
	0x5b,0x5c,0x41,0xff,0x0a,0xff,0xff,0x51,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0x36,0x38,0x3a,0x37,0x36,0x38,0x3a,0x37,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
};

static bool handleSDLEvent(const SDL_Event &event)
{
	static bool mouseButton[3] = {false, false, false};
	bool tmpMouseButton[3];

	SystemEvent ev;
	switch (event.type) {
	case SDL_EVENT_USER: {
		if (event.user.code == 1) {  // helper for changeResolution
			sd->mChangeResRet = sd->changeResolutionREAL(sd->mSDLChartemp);
			SDL_SignalCondition(sd->mWaitcondition);
		}
		return true;
	}
	case SDL_EVENT_WINDOW_EXPOSED:
		gDisplay->displayShow();
		return true;
	case SDL_EVENT_KEY_UP: {
		int sc = event.key.scancode;
		if (sc < 0 || sc >= (int)sizeof(scancode_to_adb_key)) break;
		ev.key.keycode = scancode_to_adb_key[sc];
		if ((ev.key.keycode & 0xff) == 0xff) break;
		ev.type = sysevKey;
		ev.key.pressed = false;
		gKeyboard->handleEvent(ev);
		return true;
	}
	case SDL_EVENT_KEY_DOWN: {
		int sc = event.key.scancode;
		if (sc < 0 || sc >= (int)sizeof(scancode_to_adb_key)) break;
		ev.key.keycode = scancode_to_adb_key[sc];
		if ((ev.key.keycode & 0xff) == 0xff) break;
		ev.type = sysevKey;
		ev.key.pressed = true;
		gKeyboard->handleEvent(ev);
		return true;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		ev.type = sysevMouse;
		ev.mouse.type = sme_buttonPressed;
		memcpy(tmpMouseButton, mouseButton, sizeof (tmpMouseButton));
		{
			SDL_MouseButtonFlags buttons = SDL_GetMouseState(NULL, NULL);
			mouseButton[0] = buttons & SDL_BUTTON_LMASK;
			mouseButton[1] = buttons & SDL_BUTTON_MMASK;
			mouseButton[2] = buttons & SDL_BUTTON_RMASK;
		}
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
		return true;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		ev.type = sysevMouse;
		ev.mouse.type = sme_buttonReleased;
		memcpy(tmpMouseButton, mouseButton, sizeof (tmpMouseButton));
		{
			SDL_MouseButtonFlags buttons = SDL_GetMouseState(NULL, NULL);
			mouseButton[0] = buttons & SDL_BUTTON_LMASK;
			mouseButton[1] = buttons & SDL_BUTTON_MMASK;
			mouseButton[2] = buttons & SDL_BUTTON_RMASK;
		}
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
		return true;
	case SDL_EVENT_MOUSE_MOTION:
		ev.type = sysevMouse;
		ev.mouse.type = sme_motionNotify;
		ev.mouse.button1 = mouseButton[0];
		ev.mouse.button2 = mouseButton[1];
		ev.mouse.button3 = mouseButton[2];
		ev.mouse.dbutton = 0;
		ev.mouse.x = (int)event.motion.y;
		ev.mouse.y = (int)event.motion.x;
		ev.mouse.relx = (int)event.motion.xrel;
		ev.mouse.rely = (int)event.motion.yrel;
		gMouse->handleEvent(ev);
		return true;
	case SDL_EVENT_WINDOW_SHOWN:
	case SDL_EVENT_WINDOW_RESTORED:
		gDisplay->setExposed(true);
		return true;
	case SDL_EVENT_WINDOW_HIDDEN:
	case SDL_EVENT_WINDOW_MINIMIZED:
		gDisplay->setExposed(false);
		return true;
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		gDisplay->setMouseGrab(false);
		return true;
	case SDL_EVENT_QUIT:
		gDisplay->setFullscreenMode(false);
		return false;
	}
	return true;
}

// Redraw timer removed — SDL3 on macOS doesn't reliably deliver
// cross-thread events to SDL_WaitEvent. The event loop now uses
// SDL_WaitEventTimeout for periodic redraws instead.

SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms);
SystemKeyboard *allocSystemKeyboard();
SystemMouse *allocSystemMouse();

void initUI(const char *title, const DisplayCharacteristics &aCharacteristics, int redraw_ms, const KeyboardCharacteristics &keyConfig, bool fullscreen)
{
	// SDL must be initialized on the main thread (macOS requirement)
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		ht_printf("SDL: Unable to init: %s\n", SDL_GetError());
		exit(1);
	}

	gDisplay = allocSystemDisplay(title, aCharacteristics, redraw_ms);
	gMouse = allocSystemMouse();
	gKeyboard = allocSystemKeyboard();
	if (!gKeyboard->setKeyConfig(keyConfig)) {
		ht_printf("no keyConfig, or is empty");
		exit(1);
	}

	gDisplay->mFullscreen = fullscreen;

	sd = (SDLSystemDisplay*)gDisplay;
	sd->mEventThreadID = SDL_GetCurrentThreadID();

	// Create window and renderer immediately
	sd->changeResolution(sd->mClientChar);

	sd->initCursor();
	sd->updateTitle();
	SDL_SetWindowMouseGrab(gSDLWindow, false);
	sd->setExposed(true);
}

void runUI()
{
	// This runs on the main thread -- the SDL event loop
	sd->setFullscreenMode(sd->mFullscreen);

	SDL_Event event;
	bool running = true;
	while (running) {
		if (SDL_WaitEventTimeout(&event, gDisplay->mRedraw_ms)) {
			running = handleSDLEvent(event);
			// Drain any additional pending events
			while (running && SDL_PollEvent(&event)) {
				running = handleSDLEvent(event);
			}
		}
		// Periodic redraw regardless of events
		gDisplay->displayShow();
	}

	gDisplay->setMouseGrab(false);

	ppc_cpu_stop();
}

void doneUI()
{
	if (gSDLTexture) SDL_DestroyTexture(gSDLTexture);
	if (gSDLRenderer) SDL_DestroyRenderer(gSDLRenderer);
	if (gSDLWindow) SDL_DestroyWindow(gSDLWindow);
	SDL_Quit();
}
