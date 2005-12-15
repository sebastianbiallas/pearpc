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

#include <SDL.h>
#include <SDL_thread.h>

#ifdef __WIN32__
// We need ChangeDisplaySettings
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef FASTCALL
#endif


#include "system/display.h"
#include "system/sysexcept.h"
#include "system/systhread.h"
#include "system/sysvaccel.h"
#include "system/types.h"

#include "tools/data.h"
#include "tools/debug.h"
#include "tools/snprintf.h"

//#include "io/graphic/gcard.h"
#include "configparser.h"

//#define DPRINTF(a...)
#define DPRINTF(a...) ht_printf("[Display/SDL]: "a)

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

	gSDLScreen = NULL;
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
	SDL_WM_SetCaption(curTitle.contentChar(), NULL);
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

	if (SDL_MUSTLOCK(gSDLScreen)) SDL_LockSurface(gSDLScreen);

	sys_convert_display(mClientChar, mSDLChar, gFrameBuffer,
		(byte*)gSDLScreen->pixels, firstDamagedLine, lastDamagedLine);

	if (SDL_MUSTLOCK(gSDLScreen)) SDL_UnlockSurface(gSDLScreen);

	SDL_UpdateRect(gSDLScreen, 0, firstDamagedLine, mClientChar.width, lastDamagedLine-firstDamagedLine+1);

#if 0
	if (mSDLFrameBuffer) { // using software-mode?
		sys_convert_display(mClientChar, mSDLChar, gFrameBuffer,
			mSDLFrameBuffer, firstDamagedLine, lastDamagedLine);
		if (SDL_MUSTLOCK(gSDLScreen))
			SDL_UnlockSurface(gSDLScreen);
	} else {
		// meaning we are in 32 bit. let sdl do a hardware-blit
		// and convert Client to HostFramebuffer Pixelformat
		SDL_Rect srcrect, dstrect;
		srcrect.x = 0;
		srcrect.y = firstDamagedLine;
		srcrect.w = mClientChar.width;
		srcrect.h = lastDamagedLine - firstDamagedLine+1;
		dstrect.x = 0;
		dstrect.y = firstDamagedLine;
		if (SDL_MUSTLOCK(gSDLScreen))
			SDL_UnlockSurface(gSDLScreen);
		SDL_BlitSurface(mSDLClientScreen, &srcrect, gSDLScreen, &dstrect);
	}
	
	// If possible, we should use doublebuffering and SDL_Flip()
	// SDL_Flip(); 
	SDL_UpdateRect(gSDLScreen, 0, firstDamagedLine, mClientChar.width, lastDamagedLine-firstDamagedLine+1);
	if (SDL_MUSTLOCK(gSDLScreen))
		SDL_LockSurface(gSDLScreen);
#endif
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
	// It creates an SDL_Condition and pushes a userevent (no.1) onto
	// the event queue. SDL_CondWait is used to wait for the event-thread
	// to do the actual work (in reacting on the event and calling changeResolutionREAL)
	// and finally signaling back to us, with SDL_Signal, that work is done.
	
	// AND: we have to check if the call came from another thread.
	// otherwise we would block and wait for our own thread to continue.-> endless loop
		
	mSDLChartemp = aCharacteristics;
	if (SDL_ThreadID() != mEventThreadID) { // called from a different thread than sdl eventloop
		SDL_Event ev;
		SDL_mutex *tmpmutex;
	
		//DPRINTF("Forward handler got called\n");
		ev.type = SDL_USEREVENT;
		ev.user.code = 1;
				
	
		tmpmutex = SDL_CreateMutex();
		mWaitcondition = SDL_CreateCond();
	
		SDL_LockMutex(tmpmutex);
		SDL_PushEvent(&ev);		

	 	SDL_CondWait(mWaitcondition, tmpmutex);
		//SDL_CondWait(mWaitcondition, tmpmutex, 5000);

		SDL_UnlockMutex(tmpmutex);
		SDL_DestroyMutex(tmpmutex);
		SDL_DestroyCond(mWaitcondition);
		return mChangeResRet;
	} else {
		// we can call it directly because we are in the same thread
		//ht_printf("direct call\n");
		return changeResolutionREAL(aCharacteristics);
	}

}

bool SDLSystemDisplay::changeResolutionREAL(const DisplayCharacteristics &aCharacteristics)
{
    	Uint32 videoFlags = 0; 		/* Flags to pass to SDL_SetVideoMode */
        DisplayCharacteristics chr;
                  
        DPRINTF("changeRes got called\n");
                            
	convertCharacteristicsToHost(chr, aCharacteristics);
	
	/*
	 * From the SDL documentation:
	 * "Note: The bpp parameter is the number of bits per pixel,
	 * so a bpp of 24 uses the packed representation of 3 bytes/pixel.
	 * For the more common 4 bytes/pixel mode, use a bpp of 32.
	 * Somewhat oddly, both 15 and 16 will request a 2 bytes/pixel
	 * mode, but different pixel formats."
	 *
	 * Because of their odd convention, we have to mess with 
	 * bytesPerPixel here.
	 */
	uint bitsPerPixel;
	switch (chr.bytesPerPixel) {
	case 2:
		bitsPerPixel = 15;
		break;
	case 4:
		bitsPerPixel = 32;
		break;
	default:
		ASSERT(0);
		break;
	}
		
	DPRINTF("SDL: Changing resolution to %dx%dx%d\n", aCharacteristics.width, aCharacteristics.height,bitsPerPixel);

	if (mFullscreen) videoFlags |= SDL_FULLSCREEN;
	if (!SDL_VideoModeOK(chr.width, chr.height, bitsPerPixel, videoFlags)) {
		/*
		 * We can't this mode in fullscreen
		 * so we try if we can use it in windowed mode.
		 */
		if (!mFullscreen) return false;
		videoFlags &= ~SDL_FULLSCREEN;
		if (!SDL_VideoModeOK(chr.width, chr.height, bitsPerPixel, videoFlags)) {
			return false;
		}
		mFullscreen = false;
		/*
		 * We can use the mode in windowed mode.
		 */
	}
	
	if (SDL_VideoModeOK(chr.width, chr.height, bitsPerPixel, videoFlags | SDL_SWSURFACE)) {
		videoFlags |= SDL_SWSURFACE;
		DPRINTF("can use SWSURFACE\n");
		if (SDL_VideoModeOK(chr.width, chr.height, bitsPerPixel, videoFlags | SDL_HWACCEL)) {
			videoFlags |= SDL_HWACCEL;
			DPRINTF("can use HWACCEL\n");
		}
	}

	mSDLChar = chr;
	mClientChar = aCharacteristics;
	
	sys_lock_mutex(mRedrawMutex);
#if 0
	if (gSDLScreen && SDL_MUSTLOCK(gSDLScreen)) {
		SDL_UnlockSurface(gSDLScreen);
	}
#endif

	gSDLScreen = SDL_SetVideoMode(aCharacteristics.width, aCharacteristics.height,
                          bitsPerPixel, videoFlags);

	if (!gSDLScreen) {
		// FIXME: this is really bad.
		ht_printf("SDL: FATAL: can't switch mode?!\n");
		exit(1);
	}

#ifdef __WIN32__
	if (videoFlags & SDL_FULLSCREEN) {
		DEVMODE refresh;
		refresh.dmDisplayFrequency = chr.vsyncFrequency;
		ChangeDisplaySettings(&refresh, DM_DISPLAYFREQUENCY);
	}
#else
	// FIXME: implement refreshrate change for other host OS
#endif

	mFullscreenChanged = videoFlags & SDL_FULLSCREEN;
	if (gSDLScreen->pitch != aCharacteristics.width * aCharacteristics.bytesPerPixel) {
		// FIXME: this is really bad.
		ht_printf("SDL: FATAL: new mode has scanline gap. Trying to revert to old mode.\n");
		exit(1);
	}	

	gFrameBuffer = (byte*)realloc(gFrameBuffer, mClientChar.width *
		mClientChar.height * mClientChar.bytesPerPixel);
#if 0
	if (mSDLClientScreen) {
		// if this is a modechange, free the old surface first.
		if (SDL_MUSTLOCK(gSDLScreen))
			SDL_UnlockSurface(gSDLScreen);
		SDL_FreeSurface(mSDLClientScreen);
	}

	// Init Clientsurface
	mSDLClientScreen = SDL_CreateRGBSurface(SDL_HWSURFACE, mClientChar.width, mClientChar.height,
		bitsPerPixel, 0x0000ff00, 0x00ff0000, 0xff000000, 0);
	// Mask isn't important since we only use it as a buffer and never let SDL draw with it...
	// Though it is used in 32 bit, and then the mask is ok

	if (!mSDLClientScreen) {
		ht_printf("SDL: FATAL: can't create surface\n");
		exit(1);
	}

	gFrameBuffer = (byte*)mSDLClientScreen->pixels;

	// are we running in 32 bit? use sdl, else use pearpc's software convert
	if (bitsPerPixel != 32) {
		mSDLFrameBuffer = (byte*)gSDLScreen->pixels;
	} else { 
		mSDLFrameBuffer = NULL;
	}

	// clean up
	if (SDL_MUSTLOCK(gSDLScreen)) 
		SDL_LockSurface(gSDLScreen);
	if (SDL_MUSTLOCK(mSDLClientScreen))
		SDL_LockSurface(mSDLClientScreen);
#endif

	//ht_printf("SDL rmask %08x, gmask %08x, bmask %08x\n", gSDLScreen->format->Rmask,
	//	gSDLScreen->format->Gmask, gSDLScreen->format->Bmask);
	// 
	mSDLChar.redSize = 8 - gSDLScreen->format->Rloss;
	mSDLChar.greenSize = 8 - gSDLScreen->format->Gloss;
	mSDLChar.blueSize = 8 - gSDLScreen->format->Bloss;
	mSDLChar.redShift = gSDLScreen->format->Rshift;
	mSDLChar.greenShift = gSDLScreen->format->Gshift;
	mSDLChar.blueShift = gSDLScreen->format->Bshift;

        damageFrameBufferAll();
	sys_unlock_mutex(mRedrawMutex);
	return true;
}

void SDLSystemDisplay::getHostCharacteristics(Container &modes)
{
#ifdef __WIN32__
	DEVMODE dm;
	DWORD num = 0;
	while (EnumDisplaySettings(NULL, num++, &dm)) {
		switch (dm.dmBitsPerPel) {
		case 15:
		case 16:
			dm.dmBitsPerPel = 2;
			break;
		case 32:
			dm.dmBitsPerPel = 4;
			break;
		default:
			continue;
		}		
		DisplayCharacteristics *dc = new DisplayCharacteristics;
		dc->width = dm.dmPelsWidth;
		dc->height = dm.dmPelsHeight;
		dc->bytesPerPixel = dm.dmBitsPerPel;
		dc->scanLineLength = -1;
		dc->vsyncFrequency = dm.dmDisplayFrequency;
		dc->redShift = -1;
		dc->redSize = -1;
		dc->greenShift = -1;
		dc->greenSize = -1;
		dc->blueShift = -1;
		dc->blueSize = -1;
		modes.insert(dc);
	}
#else
#if 0
	//ARGL, won't work

	SDL_Rect **modes;
	modes = SDL_ListModes(NULL, SDL_FULLSCREEN);

	/* Check is there are any modes available */
	if (modes == (SDL_Rect **)0){
		DPRINTF("No modes available!\n");
		return;
	}

	/* Check if our resolution is restricted */
	if (modes == (SDL_Rect **)-1) {
		return;
	} else {
	}
#endif
#endif
}

void SDLSystemDisplay::setMouseGrab(bool enable)
{
	if (enable == isMouseGrabbed()) return;
	SystemDisplay::setMouseGrab(enable);
	if (enable) {
//		SDL_ShowCursor(SDL_DISABLE);
		SDL_SetCursor(mInvisibleCursor);
		SDL_WM_GrabInput(SDL_GRAB_ON);
	} else {
		SDL_SetCursor(mVisibleCursor);
		SDL_WM_GrabInput(SDL_GRAB_OFF);
//		SDL_ShowCursor(SDL_ENABLE);
	}
}

void SDLSystemDisplay::initCursor()
{
	mVisibleCursor = SDL_GetCursor();
	// FIXME: need a portable way of getting cursor sizes
	byte mask[64];
	memset(mask, 0, sizeof mask);
	mInvisibleCursor = SDL_CreateCursor(mask, mask, 16, 16, 0, 0);
}

SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms)
{
	DPRINTF("Making new window %d x %d\n", chr.width, chr.height);
	return new SDLSystemDisplay(title, chr, redraw_ms);
}
