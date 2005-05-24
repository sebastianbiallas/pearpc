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

#include <SDL.h>
#include <SDL_thread.h>

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

SDL_Surface *	gSDLScreen;
static bool	gSDLVideoExposePending = false;
SDL_TimerID SDL_RedrawTimerID;

SDLSystemDisplay *sd;

#if 0

// Argl, this doesn't work, stupid SDL

static uint8 sdl_key_to_adb_key[512];

static struct {
	SDLKey sdlkey;
	uint8  adbkey;
} sdladbkeys[] = {
	{SDLK_BACKSPACE, KEY_DELETE},
	{SDLK_TAB, KEY_TAB},
	{SDLK_RETURN, KEY_RETURN},
	{SDLK_PAUSE, KEY_PAUSE},
	{SDLK_ESCAPE, KEY_ESCAPE},
	{SDLK_SPACE, KEY_SPACE},
	{SDLK_COMMA, KEY_COMMA},
	{SDLK_MINUS, KEY_MINUS},
	{SDLK_PERIOD, KEY_PERIOD},
	{SDLK_0, KEY_0},
	{SDLK_1, KEY_1},
	{SDLK_2, KEY_2},
	{SDLK_3, KEY_3},
	{SDLK_4, KEY_4},
	{SDLK_5, KEY_5},
	{SDLK_6, KEY_6},
	{SDLK_7, KEY_7},
	{SDLK_8, KEY_8},
	{SDLK_9, KEY_9},
	{SDLK_SEMICOLON, KEY_SEMICOLON},
	{SDLK_LEFTBRACKET, KEY_BRACKET_R},
	{SDLK_BACKSLASH, KEY_BACKSLASH},
	{SDLK_RIGHTBRACKET, KEY_BRACKET_L},
	{SDLK_BACKQUOTE, KEY_GRAVE},
	{SDLK_a, KEY_a},
	{SDLK_b, KEY_b},
	{SDLK_c, KEY_c},
	{SDLK_d, KEY_d},
	{SDLK_e, KEY_e},
	{SDLK_f, KEY_f},
	{SDLK_g, KEY_g},
	{SDLK_h, KEY_h},
	{SDLK_i, KEY_i},
	{SDLK_j, KEY_j},
	{SDLK_k, KEY_k},
	{SDLK_l, KEY_l},
	{SDLK_m, KEY_m},
	{SDLK_n, KEY_n},
	{SDLK_o, KEY_o},
	{SDLK_p, KEY_p},
	{SDLK_q, KEY_q},
	{SDLK_r, KEY_r},
	{SDLK_s, KEY_s},
	{SDLK_t, KEY_t},
	{SDLK_u, KEY_u},
	{SDLK_v, KEY_v},
	{SDLK_w, KEY_w},
	{SDLK_x, KEY_x},
	{SDLK_y, KEY_y},
	{SDLK_z, KEY_z},
	{SDLK_DELETE, KEY_REMOVE},
	{SDLK_KP0, KEY_KP_0},
	{SDLK_KP1, KEY_KP_1},
	{SDLK_KP2, KEY_KP_2},
	{SDLK_KP3, KEY_KP_3},
	{SDLK_KP4, KEY_KP_4},
	{SDLK_KP5, KEY_KP_5},
	{SDLK_KP6, KEY_KP_6},
	{SDLK_KP7, KEY_KP_7},
	{SDLK_KP8, KEY_KP_8},
	{SDLK_KP9, KEY_KP_9},
	{SDLK_KP_PERIOD, KEY_KP_PERIOD},
	{SDLK_KP_DIVIDE, KEY_KP_DIVIDE},
	{SDLK_KP_MULTIPLY, KEY_KP_MULTIPLY},
	{SDLK_KP_MINUS, KEY_KP_SUBTRACT},
	{SDLK_KP_PLUS, KEY_KP_ADD},
	{SDLK_KP_ENTER, KEY_KP_ENTER},
	{SDLK_UP, KEY_UP},
	{SDLK_DOWN, KEY_DOWN},
	{SDLK_RIGHT, KEY_RIGHT},
	{SDLK_LEFT, KEY_LEFT},
	{SDLK_INSERT, KEY_INSERT},
	{SDLK_HOME, KEY_HOME},
	{SDLK_END, KEY_END},
	{SDLK_PAGEUP, KEY_PRIOR},
	{SDLK_PAGEDOWN, KEY_NEXT},
	{SDLK_F1, KEY_F1},
	{SDLK_F2, KEY_F2},
	{SDLK_F3, KEY_F3},
	{SDLK_F4, KEY_F4},
	{SDLK_F5, KEY_F5},
	{SDLK_F6, KEY_F6},
	{SDLK_F7, KEY_F7},
	{SDLK_F8, KEY_F8},
	{SDLK_F9, KEY_F9},
	{SDLK_F10, KEY_F10},
	{SDLK_F11, KEY_F11},
	{SDLK_F12, KEY_F12},
	{SDLK_F13, KEY_F13},
	{SDLK_NUMLOCK, KEY_NUM_LOCK},
	{SDLK_CAPSLOCK, KEY_CAPS_LOCK},
	{SDLK_SCROLLOCK, KEY_SCROLL_LOCK},
	{SDLK_RSHIFT, KEY_SHIFT},
	{SDLK_LSHIFT, KEY_SHIFT},
	{SDLK_RCTRL, KEY_CONTROL},
	{SDLK_LCTRL, KEY_CONTROL},
	{SDLK_RALT, KEY_ALTGR},
	{SDLK_LALT, KEY_ALT},
};

static void createSDLToADBKeytable()
{
	memset(sdl_key_to_adb_key, 0xff, sizeof sdl_key_to_adb_key);
	for (uint i=0; i < (sizeof sdladbkeys / sizeof sdladbkeys[0]); i++) {
		if (sdladbkeys[i].sdlkey > sizeof sdl_key_to_adb_key) {
			ht_printf("%d > 256 for key %d\n", sdladbkeys[i].sdlkey, sdladbkeys[i].adbkey);
		}
		sdl_key_to_adb_key[sdladbkeys[i].sdlkey] = sdladbkeys[i].adbkey;
	}
}

#else

#ifdef __WIN32__
static byte scancode_to_adb_key[] = {
//00   01   02   03   04   05   06   07   08   09   0a   0b   0c   0d   0e   0f
 0xff,0x35,0x12,0x13,0x14,0x15,0x17,0x16,0x1a,0x1c,0x19,0x1d,0x1b,0x18,0x33,0x30,
 0x0c,0x0d,0x0e,0x0f,0x11,0x10,0x20,0x22,0x1f,0x23,0x21,0x1e,0x24,0x36,0x00,0x01,
 0x02,0x03,0x05,0x04,0x26,0x28,0x25,0x29,0x27,0xff,0x38,0x2a,0x06,0x07,0x08,0x09, 
 0x0b,0x2d,0x2e,0x2b,0x2f,0x2c,0x38,0x43,0x37,0x31,0x39,0x7a,0x78,0x63,0x76,0x60,
 0x61,0x62,0x64,0x65,0x6d,0xff,0xff,0x59,0x5b,0x5c,0x4e,0x56,0x57,0x58,0x45,0x53,
 0x54,0x55,0x52,0x41,0xff,0xff,0x0a,0x67,0x6f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x36,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0x4b,0xff,0xff,0x3a,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0x71,0xff,0x73,0x3e,0x74,0xff,0x3b,0xff,0x3c,0xff,0x77,
 0x3d,0x79,0x72,0x75,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
};

#else

static uint8 scancode_to_adb_key[256] = {
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

#endif

#endif

static bool handleSDLEvent(const SDL_Event &event)
{
	static bool mouseButton[3] = {false, false, false};
	bool tmpMouseButton[3];

	SystemEvent ev;
	switch (event.type) {
	case SDL_USEREVENT:
		if (event.user.code == 1) {  // helper for changeResolution
			//ht_printf("got forward event\n");
			sd->mChangeResRet = sd->changeResolutionREAL(sd->mSDLChartemp);
			SDL_CondSignal(sd->mWaitcondition); // Signal, that condition is over.
		}
		return true;
	case SDL_VIDEOEXPOSE:
		gDisplay->displayShow();
		gSDLVideoExposePending = false;
		return true;
	case SDL_KEYUP:
		ev.key.keycode = scancode_to_adb_key[event.key.keysym.scancode];
//		ht_printf("%x %x up  ", event.key.keysym.scancode, ev.key.keycode);
		if ((ev.key.keycode & 0xff) == 0xff) break;
		ev.type = sysevKey;
		ev.key.pressed = false;
		gKeyboard->handleEvent(ev);
		return true;
	case SDL_KEYDOWN:
		ev.key.keycode = scancode_to_adb_key[event.key.keysym.scancode];
//		ht_printf("%x %x %x dn  \n", event.key.keysym.sym, event.key.keysym.scancode, ev.key.keycode);
		if ((ev.key.keycode & 0xff) == 0xff) break;
		ev.type = sysevKey;
		ev.key.pressed = true;
		gKeyboard->handleEvent(ev);
		return true;
	case SDL_MOUSEBUTTONDOWN:
		ev.type = sysevMouse;
		ev.mouse.type = sme_buttonPressed;
		memcpy(tmpMouseButton, mouseButton, sizeof (tmpMouseButton));
		mouseButton[0] = SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(1);
		mouseButton[1] = SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(2);
		mouseButton[2] = SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(3);
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
	case SDL_MOUSEBUTTONUP:
		ev.type = sysevMouse;
		ev.mouse.type = sme_buttonReleased;
		memcpy(tmpMouseButton, mouseButton, sizeof (tmpMouseButton));
		mouseButton[0] = SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(1);
		mouseButton[1] = SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(2);
		mouseButton[2] = SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(3);
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
	case SDL_MOUSEMOTION:
		ev.type = sysevMouse;
		ev.mouse.type = sme_motionNotify;
		ev.mouse.button1 = mouseButton[0];
		ev.mouse.button2 = mouseButton[1];
		ev.mouse.button3 = mouseButton[2];
		ev.mouse.dbutton = 0;
		ev.mouse.x = event.motion.y;
		ev.mouse.y = event.motion.x;
		ev.mouse.relx = event.motion.xrel;
		ev.mouse.rely = event.motion.yrel;
		gMouse->handleEvent(ev);
		return true;
	case SDL_ACTIVEEVENT:
		if (event.active.state & SDL_APPACTIVE) {
			gDisplay->setExposed(event.active.gain);
		}
		if (event.active.state & SDL_APPINPUTFOCUS) {
			if (!event.active.gain) {
				gDisplay->setMouseGrab(false);			
			}
		}
		return true;
	case SDL_QUIT:		
		gDisplay->setFullscreenMode(false);
		return false;
	}
	return true;
}

static Uint32 SDL_redrawCallback(Uint32 interval, void *param)
{
	SDL_Event event;

	if (!gSDLVideoExposePending) {
		event.type = SDL_VIDEOEXPOSE;
		// according to the docs, "You may always call SDL_PushEvent" in an SDL
		// timer callback function
		SDL_PushEvent(&event);
		gSDLVideoExposePending = true;
	}
	return interval;
}

sys_timer gSDLRedrawTimer;
static bool eventThreadAlive;

static void *SDLeventLoop(void *p)
{
	eventThreadAlive = true;
#ifdef __WIN32__
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE) < 0) {
#else
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTTHREAD | SDL_INIT_NOPARACHUTE) < 0) {
#endif
		ht_printf("SDL: Unable to init: %s\n", SDL_GetError());
		exit(1);
	}

	atexit(SDL_Quit); // give SDl a chance to clean up before exit!
	sd = (SDLSystemDisplay*)gDisplay;

	sd->initCursor();

	sd->updateTitle();
	sd->mEventThreadID = SDL_ThreadID();

        SDL_WM_GrabInput(SDL_GRAB_OFF);

	sd->changeResolution(sd->mClientChar);
	sd->setExposed(true);

	gSDLVideoExposePending = false;
	SDL_RedrawTimerID = SDL_AddTimer(gDisplay->mRedraw_ms, SDL_redrawCallback, NULL);

	sd->setFullscreenMode(sd->mFullscreen);

	SDL_Event event;
	do {
		SDL_WaitEvent(&event);
	} while (handleSDLEvent(event));

	gDisplay->setMouseGrab(false);

	if (SDL_RedrawTimerID)
		SDL_RemoveTimer(SDL_RedrawTimerID);

	ppc_cpu_stop();

	eventThreadAlive = false;
	
	return NULL;
}

SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms);
SystemKeyboard *allocSystemKeyboard();
SystemMouse *allocSystemMouse();

void initUI(const char *title, const DisplayCharacteristics &aCharacteristics, int redraw_ms, const KeyboardCharacteristics &keyConfig, bool fullscreen)
{
#if 0
	createSDLToADBKeytable();
#endif

	sys_thread SDLeventLoopThread;

	gDisplay = allocSystemDisplay(title, aCharacteristics, redraw_ms);
	gMouse = allocSystemMouse();
	gKeyboard = allocSystemKeyboard();
	if (!gKeyboard->setKeyConfig(keyConfig)) {
		ht_printf("no keyConfig, or is empty");
		exit(1);
	}

	gDisplay->mFullscreen = fullscreen;

	if (sys_create_thread(&SDLeventLoopThread, 0, SDLeventLoop, NULL)) {
		ht_printf("SDL: can't create event thread!\n");
		exit(1);
	}
}

void doneUI()
{
	if (eventThreadAlive) {
		SDL_Event event;
		event.type = SDL_QUIT;
		SDL_PushEvent(&event);
		while (eventThreadAlive) SDL_Delay(10); // FIXME: UGLY!
	}
	SDL_Quit();
}
