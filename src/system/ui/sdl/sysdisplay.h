/* 
 *	PearPC
 *	sysdisplay.h - SDL display class definition
 *
 *	Copyright (C) 2004 John Kelley (pearpc@kelley.ca)
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
 
#ifndef _SYSDISPLAY_H_
#define _SYSDISPLAY_H_

#include <SDL/SDL.h>
#include <SDL/SDL_keysym.h>
#include "system/systhread.h"

#define MASK(shift, size) (((1 << (size))-1)<<(shift))

class SDLSystemDisplay: public SystemDisplay
{
	sys_thread redrawthread;
	sys_mutex mutex;
	
	uint bitsPerPixelToXBitmapPad(uint bitsPerPixel)
	{
		if (bitsPerPixel <= 8) {
			return 8;
		} else if (bitsPerPixel <= 16) {
			return 16;
		} else {
			return 32;
		}
	}
	void dumpDisplayChar(const DisplayCharacteristics &chr)
	{
		fprintf(stderr, "\tdimensions:          %d x %d pixels\n", chr.width, chr.height);
		fprintf(stderr, "\tpixel size in bytes: %d\n", chr.bytesPerPixel);
		fprintf(stderr, "\tpixel size in bits:  %d\n", chr.bytesPerPixel*8);
		fprintf(stderr, "\tred_mask:            %08x (%d bits)\n", MASK(chr.redShift, chr.redSize), chr.redSize);
		fprintf(stderr, "\tgreen_mask:          %08x (%d bits)\n", MASK(chr.greenShift, chr.greenSize), chr.greenSize);
		fprintf(stderr, "\tblue_mask:           %08x (%d bits)\n", MASK(chr.blueShift, chr.blueSize), chr.blueSize);
		fprintf(stderr, "\tdepth:               %d\n", chr.redSize + chr.greenSize + chr.blueSize);
	}
	void precalcPointers();
	void ToggleFullScreen();
public:
	SDLSystemDisplay(const char *name, int xres, int yres, const DisplayCharacteristics &chr);
	virtual SDLSystemDisplay::~SDLSystemDisplay();
	virtual bool changeResolution(const DisplayCharacteristics &aCharacteristics);
	inline void convertDisplayClientToServer(uint firstLine, uint lastLine);
	virtual void displayShow();
	static void *eventLoop(void *p);
	virtual	void finishMenu();
	virtual bool getEvent(DisplayEvent &ev);
	virtual int getKeybLEDs();
	virtual void getSyncEvent(DisplayEvent &ev);
	void putpixel(uint32 x, uint32 y, uint32 data);
	virtual void queueEvent(DisplayEvent &ev);
	static void *redrawThread(void *p);
	virtual void setKeybLEDs(int leds);
	virtual void startRedrawThread(int msec);
	
	virtual	int toString(char *buf, int buflen) const;
	void updateTitle();	
};

#endif
