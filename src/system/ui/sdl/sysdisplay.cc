/* 
 *	PearPC
 *	SDLdisplay.cc - screen access functions for SDL
 *
 *	Copyright (C)      2004 John Kelley (pearpc@kelley.ca)
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "system/display.h"
#include "system/sysexcept.h"
#include "tools/snprintf.h"

#include "tools/data.h"
#include "system/types.h"

#include "sysdisplay.h"
#include "io/graphic/gcard.h"
#include "configparser.h"

#define DPRINTF(a...)
//#define DPRINTF(a...) ht_printf(a)

#define printm(s...) ht_printf("[Display/SDL]: "s)

byte *gFrameBuffer = NULL;
uint gFrameBufferScanLineLength = 0;
uint gDamageAreaFirstAddr, gDamageAreaLastAddr;
static SDL_Surface *screen;
static int msec;
static Queue *mEventQueue;
static int mCurMouseX, mCurMouseY;
static bool mMouseButton[3];
static char *mTitle;
static char mCurTitle[200];

//fix this to use SDLKeys instead of scancodes
#ifdef __WIN32__

static uint8 x11_key_to_adb_key[512] = {
	0xff,0x35,0x12,0x13,0x14,0x15,0x17,0x16,0x1a,0x1c,0x19,0x1d,0x1b,0x18,0x33,0x30,
	0x0c,0x0d,0x0e,0x0f,0x11,0x10,0x20,0x22,0x1f,0x23,0x21,0x1e,0x24,0x36,0x00,0x01,
	0x02,0x03,0x05,0x04,0x26,0x28,0x25,0x29,0x27,0xff,0x38,0x2a,0x06,0x07,0x08,0x09,
	0x0b,0x2d,0x2e,0x2b,0x2f,0x2c,0x38,0x43,0x37,0x31,0x39,0x7a,0x78,0x63,0x76,0x60,
	0x61,0x62,0x64,0x65,0x6d,0xff,0xff,0x59,0x5b,0x5c,0x4e,0x56,0x57,0x58,0x45,0x53,
	0x54,0x55,0x52,0x41,0xff,0xff,0xff,0x67,0x6f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x6f,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x4c,0x36,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0x4b,0xff,0xff,0x3a,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0x47,0xff,0x73,0x3e,0x74,0xff,0x3b,0xff,0x3c,0xff,0x77,
	0x3d,0x79,0x72,0x75,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
};

#else

static uint8 x11_key_to_adb_key[256] = {
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

#endif

int SDLSystemDisplay::getKeybLEDs()
{
	int r = 0;
	SDLMod keyMods = SDL_GetModState();
	if (keyMods & KMOD_NUM)
		r |= KEYB_LED_NUM;
	if (keyMods & KMOD_CAPS)
		r |= KEYB_LED_CAPS;
	/*
	if (keyMods & SDLK_SCROLLOCK)
		r |= KEYB_LED_SCROLL;*/
	return r;
}

void SDLSystemDisplay::setKeybLEDs(int leds)
{
	int r = getKeybLEDs() ^ leds;
	SDLMod keyMods = SDL_GetModState();
	
	if (r & KEYB_LED_NUM && leds & KEYB_LED_NUM)
		(int)keyMods |= KMOD_NUM;
	else
		(int)keyMods &= KMOD_NUM;
		
	if (r & KEYB_LED_CAPS && leds & KEYB_LED_CAPS)
		(int)keyMods |= KMOD_CAPS;
	else
		(int)keyMods &= KMOD_CAPS;
	/*
	if (r & KEYB_LED_SCROLL && leds & KEYB_LED_SCROLL)
		keyMods |= SDLK_SCROLLOCK;
	else
		keyMods &= SDLK_SCROLLOCK;
	*/
	SDL_SetModState(keyMods);
}

SDLSystemDisplay::SDLSystemDisplay(const char *name, int xres, int yres, const DisplayCharacteristics &chr)
		:SystemDisplay(chr)
{
	sys_create_mutex(&mutex);
	mEventQueue = new Queue(true);
	mClientChar.width = xres;
	mClientChar.height = yres;

	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0)
		printm("Unable to init: %s\n", SDL_GetError());

	SDL_WM_SetCaption(name,name);

	screen = SDL_SetVideoMode(mClientChar.width, mClientChar.height,
		8*mClientChar.bytesPerPixel, SDL_HWSURFACE);
	gFrameBuffer = (byte*)screen->pixels;
	gFrameBufferScanLineLength = screen->pitch;
	if (SDL_MUSTLOCK(screen))
		SDL_LockSurface(screen);
	SDL_ShowCursor(SDL_DISABLE);
}

SDLSystemDisplay::~SDLSystemDisplay()
{
}

void SDLSystemDisplay::finishMenu()
{
}

void SDLSystemDisplay::updateTitle() 
{
//	ht_snprintf(mCurTitle, sizeof mCurTitle, "%s - [F12 %s mouse]", mTitle, mMouseEnabled ? "disables" : "enables");
//	SDL_WM_SetCaption(mTitle, NULL);
}

int SDLSystemDisplay::toString(char *buf, int buflen) const
{
	return snprintf(buf, buflen, "SDL");
}

void SDLSystemDisplay::ToggleFullScreen()
{
	SDL_Surface *backup, *backup2;
	SDL_Rect rect;

	if (SDL_MUSTLOCK(screen))
		SDL_UnlockSurface(screen);
	// we just copy the screen to a hw and sw surface and then decie *later which to use
	backup = SDL_CreateRGBSurface(SDL_HWSURFACE, screen->w, screen->h,
		8*mClientChar.bytesPerPixel, screen->format->Rmask,
		screen->format->Gmask, screen->format->Bmask, 0);
	backup2 = SDL_CreateRGBSurface(SDL_SWSURFACE, screen->w, screen->h,
		8*mClientChar.bytesPerPixel, screen->format->Rmask,
		screen->format->Gmask, screen->format->Bmask, 0);
	SDL_SetAlpha(backup, 0, 0);
	SDL_SetAlpha(backup2, 0, 0);
	SDL_SetAlpha(screen, 0, 0);
	screen->format->Amask = 0;
	SDL_BlitSurface(screen, NULL, backup, NULL);
	SDL_BlitSurface(screen, NULL, backup2, NULL);
	printm("Toggled FullScreen\n");
       	if (SDL_VideoModeOK(screen->w, screen->h, 32, screen->flags ^ SDL_FULLSCREEN)) {
       		screen = SDL_SetVideoMode(screen->w, screen->h, 8*mClientChar.bytesPerPixel,
			(screen->flags ^ SDL_FULLSCREEN)|SDL_HWSURFACE);
	}

	gFrameBuffer = (byte*)screen->pixels;
	gFrameBufferScanLineLength = screen->pitch;

	// *later: we decide which to use
	if (screen->flags&SDL_HWSURFACE) {
		SDL_BlitSurface(backup, NULL, screen, NULL);
	} else {
		SDL_BlitSurface(backup2, NULL, screen, NULL);
	}
	if(SDL_MUSTLOCK(screen))
		SDL_LockSurface(screen);
	SDL_FreeSurface(backup);
	SDL_FreeSurface(backup2);
	damageFrameBufferAll();
	displayShow();
	return;
}

void SDLSystemDisplay::getSyncEvent(DisplayEvent &ev)
{
	SDL_Event event;
	char buffer[4];
	while (1) {
		SDL_WaitEvent( &event);
		switch (event.type) {
		case SDL_QUIT:
			exit(0);
			return;
		case SDL_KEYUP: 
			ev.keyEvent.keycode = x11_key_to_adb_key[event.key.keysym.scancode];
			if ((ev.keyEvent.keycode & 0xff) == 0xff)
				break;
			ev.type = evKey;
			ev.keyEvent.pressed = false;
			ev.keyEvent.chr = buffer[0];
			return;
		case SDL_KEYDOWN:
			ev.keyEvent.keycode = x11_key_to_adb_key[event.key.keysym.scancode];
			if ((ev.keyEvent.keycode & 0xff) == 0xff) 
				break;
			ev.type = evKey;
			ev.keyEvent.pressed = true;
			ev.keyEvent.keycode = x11_key_to_adb_key[event.key.keysym.scancode];
			ev.keyEvent.chr = buffer[0];
			return;
		}
	}
}

bool SDLSystemDisplay::getEvent(DisplayEvent &ev)
{
	if (!mEventQueue->isEmpty()) {
		Pointer *p = (Pointer *)mEventQueue->deQueue();
		ev = *(DisplayEvent *)p->value;
		free(p->value);
		delete p;
		return true;
	}
	SDL_Event event;
	char buffer[4];
	if (SDL_PollEvent(&event)) {
		switch (event.type) {
		// not sure if this is important...
		case SDL_VIDEOEXPOSE:
			if(SDL_MUSTLOCK(screen))
				SDL_UnlockSurface(screen);
			if(SDL_MUSTLOCK(screen))
				SDL_LockSurface(screen);
			damageFrameBufferAll();
			displayShow();
			return true;
		case SDL_QUIT: //should we trap this and send power key?
			SDL_WM_GrabInput(SDL_GRAB_OFF);
			exit(0);
			return true;
		case SDL_KEYUP: 
			ev.keyEvent.keycode = x11_key_to_adb_key[event.key.keysym.scancode];
			if ((ev.keyEvent.keycode & 0xff) == 0xff) break;
			ev.type = evKey;
			ev.keyEvent.pressed = false;
			ev.keyEvent.chr = buffer[0];
			return true;
		case SDL_KEYDOWN:
			ev.keyEvent.keycode = x11_key_to_adb_key[event.key.keysym.scancode];
			if(event.key.keysym.mod & KMOD_SHIFT){
				if(event.key.keysym.sym == SDLK_F10) {
					if(msec > 11) 
						msec-=10;
					printm("redraw delay: %d msec\n",msec/10);
					break;
				}
				if(event.key.keysym.sym == SDLK_F11) {
					if(msec < 491) 
						msec+=10;
					printm("redraw delay: %d msec\n",msec/10);
					break;
				}
				if(event.key.keysym.sym == SDLK_F9) {
					if(SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_OFF) {
						SDL_WM_GrabInput(SDL_GRAB_ON);
						ht_printf("Grab on!\n");
						break;;
					}
					SDL_WM_GrabInput(SDL_GRAB_OFF);
					printm("Grab off!\n");
					break;
				}
				if(event.key.keysym.sym == SDLK_F12) 
					ToggleFullScreen();
			}
//			if(event.key.keysym.sym == SDLK_F1) { changeCDFunc(); }                                
			if ((ev.keyEvent.keycode & 0xff) == 0xff) break;
			ev.type = evKey;
			ev.keyEvent.pressed = true;
			ev.keyEvent.keycode = x11_key_to_adb_key[event.key.keysym.scancode];
			ev.keyEvent.chr = buffer[0];
			return true;

		case SDL_MOUSEBUTTONDOWN:
			mMouseButton[0] = false;
			mMouseButton[1] = false;
			mMouseButton[2] = false;
			ev.type = evMouse;
			if((SDL_GetMouseState(NULL, NULL)&SDL_BUTTON(3)))
				mMouseButton[2] = true;
			if((SDL_GetMouseState(NULL, NULL)&SDL_BUTTON(2)))
				mMouseButton[1] = true;
			if((SDL_GetMouseState(NULL, NULL)&SDL_BUTTON(1)))
				mMouseButton[0] = true;
			ev.mouseEvent.button1 = mMouseButton[0];
			ev.mouseEvent.button2 = mMouseButton[2];
			ev.mouseEvent.button3 = mMouseButton[1];
			ev.mouseEvent.x = mCurMouseX;
			ev.mouseEvent.y = mCurMouseY;
			ev.mouseEvent.relx = 0;
			ev.mouseEvent.rely = 0;
			return true;
		case SDL_MOUSEBUTTONUP:
			mMouseButton[0] = false;
			mMouseButton[1] = false;
			mMouseButton[2] = false;
			ev.type = evMouse;
			if((SDL_GetMouseState(NULL, NULL)&SDL_BUTTON(3))) //shouldn't below be false?
				mMouseButton[2] = true;
			if((SDL_GetMouseState(NULL, NULL)&SDL_BUTTON(2)))
				mMouseButton[1] = true;
			if((SDL_GetMouseState(NULL, NULL)&SDL_BUTTON(1)))
				mMouseButton[0] = true;
			ev.mouseEvent.button1 = mMouseButton[0];
			ev.mouseEvent.button2 = mMouseButton[2];
			ev.mouseEvent.button3 = mMouseButton[1];
			ev.mouseEvent.x = mCurMouseX;
			ev.mouseEvent.y = mCurMouseY;
			ev.mouseEvent.relx = 0;
			ev.mouseEvent.rely = 0;
			sys_unlock_mutex(mutex);
			return true;
		case SDL_MOUSEMOTION:
			ev.type = evMouse;
			ev.mouseEvent.button1 = mMouseButton[0];
			ev.mouseEvent.button2 = mMouseButton[1];
			ev.mouseEvent.button3 = mMouseButton[2];
			ev.mouseEvent.relx = event.motion.xrel;
			ev.mouseEvent.rely = event.motion.yrel;
			return true;
		}
	}
	return false;
}

void SDLSystemDisplay::getFrameBufferInfo(DisplayFrameBufferInfo &fbi)
{
	fbi.frameBuffer = screen->pixels;
	fbi.scanLineLength = screen->pitch;
}

void SDLSystemDisplay::displayShow()
{
	uint firstDamagedLine, lastDamagedLine;
	// We've got problems with races here because gcard_write1/2/4
	// might set gDamageAreaFirstAddr, gDamageAreaLastAddr.
	// We can't use mutexes in gcard for speed reasons. So we'll
	// try to minimize the probability of loosing the race.
	if (gDamageAreaFirstAddr > gDamageAreaLastAddr+3) {
	        return;
	}
	uint damageAreaFirstAddr = gDamageAreaFirstAddr;
	uint damageAreaLastAddr = gDamageAreaLastAddr;
	healFrameBuffer();
	// end of race
	damageAreaLastAddr += 3;	// this is a hack. For speed reasons we
					// inaccurately set gDamageAreaLastAddr
					// to the first (not last) byte accessed
					// accesses are up to 4 bytes "long".
	firstDamagedLine = damageAreaFirstAddr / (mClientChar.width * mClientChar.bytesPerPixel);
	lastDamagedLine = damageAreaLastAddr / (mClientChar.width * mClientChar.bytesPerPixel);
	// Overflow may happen, because of the hack used above
	// and others, that set lastAddr = 0xfffffff0 (damageFrameBufferAll())
	if (lastDamagedLine >= mClientChar.height) {
		lastDamagedLine = mClientChar.height-1;
	}

	// If possible, we should use doublebuffering and SDL_Flip()
	// SDL_Flip(); 
	SDL_UpdateRect(screen, 0, firstDamagedLine, 0, lastDamagedLine-firstDamagedLine+1);
}

void SDLSystemDisplay::queueEvent(DisplayEvent &ev)
{
	DisplayEvent *e = (DisplayEvent *)malloc(sizeof (DisplayEvent));
	*e = ev;
	mEventQueue->enQueue(new Pointer(e));
}

void *SDLSystemDisplay::redrawThread(void *p)
{
	msec = *((int *)p);

	while (1) {
		if (screen->flags & SDL_HWSURFACE) {
			SDL_Delay(1000);
		} else {
			gDisplay->displayShow();
			SDL_Delay(msec);
		}
	}
	return NULL;
}

void *SDLSystemDisplay::eventLoop(void *p)
{
	return NULL;
}

void SDLSystemDisplay::startRedrawThread(int msec)
{
	// decide whether we need a redraw thread or not
	char sdl_driver[16];
	SDL_VideoDriverName(sdl_driver, 15);
	printm("Using driver: %s\n", sdl_driver);
	if (strncmp(sdl_driver, "dga", 3) != 0) {
		sys_create_thread(&redrawthread, 0, redrawThread, &msec);
	}
}

bool SDLSystemDisplay::changeResolution(const DisplayCharacteristics &aCharacteristics)
{
	printm("Changing resolution to %dx%d\n", aCharacteristics.width, aCharacteristics.height);
	if (aCharacteristics.bytesPerPixel != 4) return false;
	if (!SDL_VideoModeOK(aCharacteristics.width, aCharacteristics.height, 8*aCharacteristics.bytesPerPixel, screen->flags))
		return false;
	if (SDL_MUSTLOCK(screen)) {
		SDL_UnlockSurface(screen);
	}
	screen = SDL_SetVideoMode(aCharacteristics.width, aCharacteristics.height,
	                          8*aCharacteristics.bytesPerPixel, screen->flags|SDL_HWSURFACE);
	if (!screen || (screen->pitch != aCharacteristics.width * aCharacteristics.bytesPerPixel)) {
		printf("SDL: WARN: new mode has scanline gap. Trying to revert to old mode.\n");
		screen = SDL_SetVideoMode(mClientChar.width, mClientChar.height,
			8*mClientChar.bytesPerPixel, screen->flags|SDL_HWSURFACE);
		if (!screen) {
			printf("SDL: FATAL: couldn't restore previous screen mode\n");
			exit(1);
		}
	}

	gFrameBuffer = (byte*)screen->pixels;
	gFrameBufferScanLineLength = screen->pitch;

	if (SDL_MUSTLOCK(screen)) {
		SDL_LockSurface(screen);
	}
        mClientChar = aCharacteristics;
	damageFrameBufferAll();

	return true;
}

SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr)
{
	printm("Making new window %d x %d\n", chr.width, chr.height);
	return new SDLSystemDisplay(title, chr.width, chr.height, chr);
}
