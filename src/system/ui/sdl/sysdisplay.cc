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

#include <SDL/SDL.h>

#include "system/display.h"
#include "system/sysexcept.h"
#include "system/systhread.h"
#include "system/sysvaccel.h"
#include "system/types.h"

#include "tools/data.h"
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

	gSDLScreen = NULL;
	mSDLFrameBuffer = NULL;
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
/*
	SDL_Surface *backup, *backup2;
//	SDL_Rect rect;

	if (SDL_MUSTLOCK(gSDLScreen))
		SDL_UnlockSurface(gSDLScreen);
	// we just copy the gSDLScreen to a hw and sw surface and then decie *later which to use
	backup = SDL_CreateRGBSurface(SDL_HWSURFACE, gSDLScreen->w, gSDLScreen->h,
		8*mClientChar.bytesPerPixel, gSDLScreen->format->Rmask,
		gSDLScreen->format->Gmask, gSDLScreen->format->Bmask, 0);
	backup2 = SDL_CreateRGBSurface(SDL_SWSURFACE, gSDLScreen->w, gSDLScreen->h,
		8*mClientChar.bytesPerPixel, gSDLScreen->format->Rmask,
		gSDLScreen->format->Gmask, gSDLScreen->format->Bmask, 0);
	SDL_SetAlpha(backup, 0, 0);
	SDL_SetAlpha(backup2, 0, 0);
	SDL_SetAlpha(gSDLScreen, 0, 0);
	gSDLScreen->format->Amask = 0;
	SDL_BlitSurface(gSDLScreen, NULL, backup, NULL);
	SDL_BlitSurface(gSDLScreen, NULL, backup2, NULL);
	DPRINTF("Toggled FullScreen\n");
       	if (SDL_VideoModeOK(gSDLScreen->w, gSDLScreen->h, 8*mClientChar.bytesPerPixel,
	gSDLScreen->flags ^ SDL_FULLSCREEN)) {
       		gSDLScreen = SDL_SetVideoMode(gSDLScreen->w, gSDLScreen->h, 8*mClientChar.bytesPerPixel,
			(gSDLScreen->flags ^ SDL_FULLSCREEN)|SDL_HWSURFACE);
	}

	gFrameBuffer = (byte*)gSDLScreen->pixels;

	// *later: we decide which to use
	if (gSDLScreen->flags&SDL_HWSURFACE) {
		SDL_BlitSurface(backup, NULL, gSDLScreen, NULL);
	} else {
		SDL_BlitSurface(backup2, NULL, gSDLScreen, NULL);
	}
	if(SDL_MUSTLOCK(gSDLScreen))
		SDL_LockSurface(gSDLScreen);
	SDL_FreeSurface(backup);
	SDL_FreeSurface(backup2);
	damageFrameBufferAll();
	displayShow();*/
}

void SDLSystemDisplay::displayShow()
{
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

	if (mSDLFrameBuffer) {
		sys_convert_display(mClientChar, mSDLChar, gFrameBuffer,
			mSDLFrameBuffer, firstDamagedLine, lastDamagedLine);
/*		memcpy(mSDLFrameBuffer, gFrameBuffer, mClientChar.width *
		mClientChar.height * mClientChar.bytesPerPixel);*/
	}

	if (gSDLScreen && SDL_MUSTLOCK(gSDLScreen)) {
		SDL_UnlockSurface(gSDLScreen);
	}
	// If possible, we should use doublebuffering and SDL_Flip()
	// SDL_Flip(); 
	SDL_UpdateRect(gSDLScreen, 0, firstDamagedLine, 0, lastDamagedLine-firstDamagedLine+1);

	if (gSDLScreen && SDL_MUSTLOCK(gSDLScreen)) {
		SDL_LockSurface(gSDLScreen);
	}
}

void SDLSystemDisplay::convertCharacteristicsToHost(DisplayCharacteristics &aHostChar, const DisplayCharacteristics &aClientChar)
{
	aHostChar = aClientChar;
}

bool SDLSystemDisplay::changeResolution(const DisplayCharacteristics &aCharacteristics)
{
	DPRINTF("Changing resolution to %dx%d\n", aCharacteristics.width, aCharacteristics.height);

        DisplayCharacteristics chr;
	convertCharacteristicsToHost(chr, aCharacteristics);
	uint bitsPerPixel = chr.redSize + chr.greenSize + chr.blueSize;

	if (!SDL_VideoModeOK(chr.width, chr.height,
	bitsPerPixel, SDL_HWSURFACE))
		return false;

	mSDLChar = chr;
	mClientChar = aCharacteristics;

	if (gSDLScreen && SDL_MUSTLOCK(gSDLScreen)) {
		SDL_UnlockSurface(gSDLScreen);
	}

	gSDLScreen = SDL_SetVideoMode(aCharacteristics.width, aCharacteristics.height,
	                          bitsPerPixel, SDL_HWSURFACE);
	if (!gSDLScreen) {
		// FIXME: this is really bad.
		printf("SDL: FATAL: can't switch mode?!\n");
		exit(1);
	}

	if (gSDLScreen->pitch != aCharacteristics.width * aCharacteristics.bytesPerPixel) {
		// FIXME: this is really bad.
		printf("SDL: FATAL: new mode has scanline gap. Trying to revert to old mode.\n");
		exit(1);
	}

	gFrameBuffer = (byte*)realloc(gFrameBuffer, mClientChar.width *
		mClientChar.height * mClientChar.bytesPerPixel);
	memset(gFrameBuffer, 0, mClientChar.width *
		mClientChar.height * mClientChar.bytesPerPixel);
	mSDLFrameBuffer = (byte*)gSDLScreen->pixels;
	damageFrameBufferAll();

	if (SDL_MUSTLOCK(gSDLScreen)) {
		SDL_LockSurface(gSDLScreen);
	}

	ht_printf("SDL rmask %08x, gmask %08x, bmask %08x\n", gSDLScreen->format->Rmask,
		gSDLScreen->format->Gmask, gSDLScreen->format->Bmask);

	return true;
}

void SDLSystemDisplay::getHostCharacteristics(Container &modes)
{
	// FIXME: implement me
}

SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms)
{
	DPRINTF("Making new window %d x %d\n", chr.width, chr.height);
	return new SDLSystemDisplay(title, chr, redraw_ms);
}
