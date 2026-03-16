/*
 *	PearPC
 *	sysdisplay.cc - screen access functions for SDL
 *
 *	Copyright (C)      2004 Jens v.d. Heydt (mailme@vdh-webservice.de)
 *	Copyright (C)      2004 John Kelley (pearpc@kelley.ca)
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <SDL3/SDL.h>

#include "system/display.h"
#include "system/sysexcept.h"
#include "system/systhread.h"
#include "system/sysvaccel.h"
#include "system/types.h"

#include "tools/data.h"
#include "tools/debug.h"
#include "tools/snprintf.h"

#include "configparser.h"

//#define DPRINTF(...)
#define DPRINTF(...) ht_printf("[Display/SDL]: " __VA_ARGS__)

#include "syssdl.h"


uint SDLSystemDisplay::bitsPerPixelToXBitmapPad(uint bitsPerPixel)
{
	if (bitsPerPixel <= 8) {
		return 8;
	} else if (bitsPerPixel <= 16) {
		return 16;
	} else {
		return 32;
	}
}

#define MASK(shift, size) (((1 << (size))-1)<<(shift))

void SDLSystemDisplay::dumpDisplayChar(const DisplayCharacteristics &chr)
{
	fprintf(stderr, "\tdimensions:          %d x %d pixels\n", chr.width, chr.height);
	fprintf(stderr, "\tpixel size in bytes: %d\n", chr.bytesPerPixel);
	fprintf(stderr, "\tpixel size in bits:  %d\n", chr.bytesPerPixel*8);
	fprintf(stderr, "\tred_mask:            %08x (%d bits)\n", MASK(chr.redShift, chr.redSize), chr.redSize);
	fprintf(stderr, "\tgreen_mask:          %08x (%d bits)\n", MASK(chr.greenShift, chr.greenSize), chr.greenSize);
	fprintf(stderr, "\tblue_mask:           %08x (%d bits)\n", MASK(chr.blueShift, chr.blueSize), chr.blueSize);
	fprintf(stderr, "\tdepth:               %d\n", chr.redSize + chr.greenSize + chr.blueSize);
}

SDLSystemDisplay::SDLSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms)
: SystemDisplay(chr, redraw_ms)
{
	mTitle = strdup(title);

	gFrameBuffer = (byte*)malloc(mClientChar.width *
		mClientChar.height * mClientChar.bytesPerPixel);
	memset(gFrameBuffer, 0, mClientChar.width *
		mClientChar.height * mClientChar.bytesPerPixel);
	damageFrameBufferAll();

	mSDLFrameBuffer = NULL;
	mChangingScreen = false;

	sys_create_mutex(&mRedrawMutex);
}

void SDLSystemDisplay::finishMenu()
{
}

void SDLSystemDisplay::updateTitle()
{
	String key;
	int key_toggle_mouse_grab = gKeyboard->getKeyConfig().key_toggle_mouse_grab;
	SystemKeyboard::convertKeycodeToString(key, key_toggle_mouse_grab);
	String curTitle;
	curTitle.assignFormat("%s - [%s %s mouse]", mTitle, key.contentChar(), (isMouseGrabbed() ? "disables" : "enables"));
	if (gSDLWindow) {
		SDL_SetWindowTitle(gSDLWindow, curTitle.contentChar());
	}
}

int SDLSystemDisplay::toString(char *buf, int buflen) const
{
	return snprintf(buf, buflen, "SDL");
}

void SDLSystemDisplay::displayShow()
{
	if (!isExposed()) return;

	int firstDamagedLine, lastDamagedLine;
	// We've got problems with races here because gcard_write1/2/4
	// might set gDamageAreaFirstAddr, gDamageAreaLastAddr.
	// We can't use mutexes in gcard for speed reasons. So we'll
	// try to minimize the probability of loosing the race.
	if (gDamageAreaFirstAddr > gDamageAreaLastAddr+3) {
	        return;
	}
	int damageAreaFirstAddr = gDamageAreaFirstAddr;
	int damageAreaLastAddr = gDamageAreaLastAddr;
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

	sys_lock_mutex(mRedrawMutex);

	// Convert client framebuffer to host format into mSDLFrameBuffer
	if (mSDLFrameBuffer) {
		sys_convert_display(mClientChar, mSDLChar, gFrameBuffer,
			mSDLFrameBuffer, firstDamagedLine, lastDamagedLine);

		// Update the texture with the converted pixels
		SDL_Rect updateRect;
		updateRect.x = 0;
		updateRect.y = firstDamagedLine;
		updateRect.w = mClientChar.width;
		updateRect.h = lastDamagedLine - firstDamagedLine + 1;
		SDL_UpdateTexture(gSDLTexture, &updateRect,
			mSDLFrameBuffer + firstDamagedLine * mClientChar.width * mSDLChar.bytesPerPixel,
			mClientChar.width * mSDLChar.bytesPerPixel);
	}

	// Render texture to screen
	SDL_RenderTexture(gSDLRenderer, gSDLTexture, NULL, NULL);
	SDL_RenderPresent(gSDLRenderer);

	sys_unlock_mutex(mRedrawMutex);
}

void SDLSystemDisplay::convertCharacteristicsToHost(DisplayCharacteristics &aHostChar, const DisplayCharacteristics &aClientChar)
{
	aHostChar = aClientChar;
}

bool SDLSystemDisplay::changeResolution(const DisplayCharacteristics &aCharacteristics)
{
	// We absolutely have to make sure that SDL_calls are only used
	// in the thread, that did SDL_INIT and created Surfaces etc...
	// This function behaves as a forward-function for changeResolution calls.
	// It creates an SDL_Condition and pushes a userevent onto
	// the event queue. SDL_WaitCondition is used to wait for the event-thread
	// to do the actual work (in reacting on the event and calling changeResolutionREAL)
	// and finally signaling back to us that work is done.

	// AND: we have to check if the call came from another thread.
	// otherwise we would block and wait for our own thread to continue.-> endless loop

	mSDLChartemp = aCharacteristics;
	if (SDL_GetCurrentThreadID() != mEventThreadID) {
		SDL_Event ev;
		SDL_Mutex *tmpmutex;

		SDL_zero(ev);
		ev.type = SDL_EVENT_USER;
		ev.user.code = 1;

		tmpmutex = SDL_CreateMutex();
		mWaitcondition = SDL_CreateCondition();

		SDL_LockMutex(tmpmutex);
		SDL_PushEvent(&ev);

		SDL_WaitCondition(mWaitcondition, tmpmutex);

		SDL_UnlockMutex(tmpmutex);
		SDL_DestroyMutex(tmpmutex);
		SDL_DestroyCondition(mWaitcondition);
		return mChangeResRet;
	} else {
		// we can call it directly because we are in the same thread
		return changeResolutionREAL(aCharacteristics);
	}

}

bool SDLSystemDisplay::changeResolutionREAL(const DisplayCharacteristics &aCharacteristics)
{
	DisplayCharacteristics chr;

	DPRINTF("changeRes got called\n");

	convertCharacteristicsToHost(chr, aCharacteristics);

	DPRINTF("SDL: Changing resolution to %dx%dx%d\n", aCharacteristics.width, aCharacteristics.height, chr.bytesPerPixel * 8);

	mSDLChar = chr;
	mClientChar = aCharacteristics;

	sys_lock_mutex(mRedrawMutex);

	// Destroy old texture if it exists
	if (gSDLTexture) {
		SDL_DestroyTexture(gSDLTexture);
		gSDLTexture = NULL;
	}

	// Create window if it doesn't exist
	if (!gSDLWindow) {
		SDL_WindowFlags windowFlags = 0;
		if (mFullscreen) windowFlags |= SDL_WINDOW_FULLSCREEN;

		if (!SDL_CreateWindowAndRenderer(mTitle,
				aCharacteristics.width, aCharacteristics.height,
				windowFlags, &gSDLWindow, &gSDLRenderer)) {
			ht_printf("SDL: FATAL: can't create window: %s\n", SDL_GetError());
			exit(1);
		}
	} else {
		SDL_SetWindowSize(gSDLWindow, aCharacteristics.width, aCharacteristics.height);
		if (mFullscreen) {
			SDL_SetWindowFullscreen(gSDLWindow, true);
		}
	}

	// Determine pixel format for SDL3 texture
	SDL_PixelFormat pixelFormat;
	switch (chr.bytesPerPixel) {
	case 2:
		pixelFormat = SDL_PIXELFORMAT_XRGB1555;
		mSDLChar.redSize = 5;
		mSDLChar.greenSize = 5;
		mSDLChar.blueSize = 5;
		mSDLChar.redShift = 10;
		mSDLChar.greenShift = 5;
		mSDLChar.blueShift = 0;
		break;
	case 4:
		pixelFormat = SDL_PIXELFORMAT_XRGB8888;
		mSDLChar.redSize = 8;
		mSDLChar.greenSize = 8;
		mSDLChar.blueSize = 8;
		mSDLChar.redShift = 16;
		mSDLChar.greenShift = 8;
		mSDLChar.blueShift = 0;
		break;
	default:
		ASSERT(0);
		break;
	}

	gSDLTexture = SDL_CreateTexture(gSDLRenderer, pixelFormat,
		SDL_TEXTUREACCESS_STREAMING,
		aCharacteristics.width, aCharacteristics.height);
	if (!gSDLTexture) {
		ht_printf("SDL: FATAL: can't create texture: %s\n", SDL_GetError());
		exit(1);
	}

	mFullscreenChanged = mFullscreen;

	gFrameBuffer = (byte*)realloc(gFrameBuffer, mClientChar.width *
		mClientChar.height * mClientChar.bytesPerPixel);

	// Allocate host framebuffer for pixel format conversion
	free(mSDLFrameBuffer);
	mSDLFrameBuffer = (byte*)malloc(mClientChar.width *
		mClientChar.height * mSDLChar.bytesPerPixel);

	damageFrameBufferAll();
	sys_unlock_mutex(mRedrawMutex);
	return true;
}

void SDLSystemDisplay::getHostCharacteristics(Container &modes)
{
	// SDL3: enumerate display modes
	int count = 0;
	SDL_DisplayID displayID = SDL_GetPrimaryDisplay();
	const SDL_DisplayMode * const *sdlModes = SDL_GetFullscreenDisplayModes(displayID, &count);
	if (sdlModes) {
		for (int i = 0; i < count; i++) {
			const SDL_DisplayMode *mode = sdlModes[i];
			DisplayCharacteristics *dc = new DisplayCharacteristics;
			dc->width = mode->w;
			dc->height = mode->h;
			dc->bytesPerPixel = SDL_BYTESPERPIXEL(mode->format);
			dc->scanLineLength = -1;
			dc->vsyncFrequency = (int)mode->refresh_rate;
			dc->redShift = -1;
			dc->redSize = -1;
			dc->greenShift = -1;
			dc->greenSize = -1;
			dc->blueShift = -1;
			dc->blueSize = -1;
			modes.insert(dc);
		}
	}
}

void SDLSystemDisplay::setMouseGrab(bool enable)
{
	if (enable == isMouseGrabbed()) return;
	SystemDisplay::setMouseGrab(enable);
	if (gSDLWindow) {
		if (enable) {
			SDL_SetCursor(mInvisibleCursor);
			SDL_SetWindowMouseGrab(gSDLWindow, true);
		} else {
			SDL_SetCursor(mVisibleCursor);
			SDL_SetWindowMouseGrab(gSDLWindow, false);
		}
	}
}

void SDLSystemDisplay::initCursor()
{
	mVisibleCursor = SDL_GetDefaultCursor();
	// Create an invisible cursor
	SDL_Surface *surface = SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_RGBA8888);
	if (surface) {
		memset(surface->pixels, 0, surface->h * surface->pitch);
		mInvisibleCursor = SDL_CreateColorCursor(surface, 0, 0);
		SDL_DestroySurface(surface);
	} else {
		mInvisibleCursor = mVisibleCursor;
	}
}

SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms)
{
	DPRINTF("Making new window %d x %d\n", chr.width, chr.height);
	return new SDLSystemDisplay(title, chr, redraw_ms);
}
