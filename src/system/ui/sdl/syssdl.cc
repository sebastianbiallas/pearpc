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
#include "system/systhread.h"
#include "system/systimer.h"

#include "tools/snprintf.h"

#include "syssdl.h"

SDL_Surface *	gSDLScreen;
static bool	gSDLVideoExposePending = false;

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

static bool handleSDLEvent(const SDL_Event &event)
{
//	static bool visible = true;
//	static bool mapped = true;
	static bool mouseButton[3] = {false, false, false};
	bool tmpMouseButton[3];

	SystemEvent ev;
	switch (event.type) {
	case SDL_VIDEOEXPOSE:
		gDisplay->displayShow();
		gSDLVideoExposePending = false;
		return true;
	case SDL_QUIT: // should we trap this and send power key?
		SDL_WM_GrabInput(SDL_GRAB_OFF);
		exit(0);
		return true;
	case SDL_KEYUP:
		ev.key.keycode = sdl_key_to_adb_key[event.key.keysym.sym];
		if ((ev.key.keycode & 0xff) == 0xff) break;
		ev.type = sysevKey;
		ev.key.pressed = false;
		return gKeyboard->handleEvent(ev);
	case SDL_KEYDOWN:
		ev.key.keycode = sdl_key_to_adb_key[event.key.keysym.sym];
		if ((ev.key.keycode & 0xff) == 0xff) break;
		ev.type = sysevKey;
		ev.key.pressed = true;
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

static Uint32 SDL_redrawCallback(Uint32 interval, void *param)
{
	SDL_Event event;

//	ht_printf("redrawtimer\n");
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

static void *SDLeventLoop(void *p)
{
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
		printf("SDL: Unable to init: %s\n", SDL_GetError());
		exit(1);
	}
	SDLSystemDisplay *sd = (SDLSystemDisplay*)gDisplay;

	SDL_WM_SetCaption(sd->mTitle, sd->mTitle);

        SDL_WM_GrabInput(SDL_GRAB_OFF);

	sd->changeResolution(sd->mClientChar);
	damageFrameBufferAll();

	gSDLVideoExposePending = false;
	SDL_AddTimer(gDisplay->mRedraw_ms, SDL_redrawCallback, NULL);

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
	createSDLToADBKeytable();

	sys_thread SDLeventLoopThread;

	gDisplay = allocSystemDisplay(title, aCharacteristics, redraw_ms);
	gMouse = allocSystemMouse();
	gKeyboard = allocSystemKeyboard();

	if (sys_create_thread(&SDLeventLoopThread, 0, SDLeventLoop, NULL)) {
		ht_printf("SDL: can't create event thread!\n");
		exit(1);
	}
}

void doneUI()
{
}
