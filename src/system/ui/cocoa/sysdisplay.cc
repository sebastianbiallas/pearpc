/* 
 *	PearPC
 *	sysdisplay.cc - screen access functions for X11
 *
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

#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <cstring>

#include "system/display.h"
#include "system/types.h"
#include "system/systhread.h"
#include "system/sysexcept.h"
#include "system/sysvaccel.h"
#include "system/sysvm.h"
#include "tools/data.h"
#include "tools/snprintf.h"

#include "syscocoa.h"
#include "display.h"

#define DPRINTF(a...)
//#define DPRINTF(a...) ht_printf(a)

static void findMaskShiftAndSize(int &shift, int &size, uint bitmask)
{
	if (!bitmask) {
		shift = 0;
		size = 0;
		return;
	}
	shift = 0;
	while (!(bitmask & 1)) {
		shift++;
		bitmask >>= 1;
	}
	size = 0;
	while (bitmask & 1) {
		size++;
		bitmask >>= 1;
	}
}

class CocoaSystemDisplay: public SystemDisplay
{
	byte *mXFrameBuffer;
	DisplayCharacteristics mXChar;
	char *mTitle;
	char mCurTitle[200];
	byte *mouseData;
	byte *menuData;

	int bitsPerPixelToXBitmapPad(int bitsPerPixel)
	{
		if (bitsPerPixel <= 8) {
			return 8;
		} else if (bitsPerPixel <= 16) {
			return 16;
		} else {
			return 32;
		}
	}
public:
	CocoaSystemDisplay(const char *name, const DisplayCharacteristics &aClientChar, int redraw_ms)
		: SystemDisplay(aClientChar, redraw_ms)
	{
		if (bitsPerPixelToXBitmapPad(mClientChar.bytesPerPixel*8) != mClientChar.bytesPerPixel*8) {
			ht_printf("nope. bytes per pixel is: %d. only 1,2 or 4 are allowed.\n", mClientChar.bytesPerPixel);
			exit(1);
		}

		// mouse
		mouseData = (byte*)malloc(2 * 2 * mClientChar.bytesPerPixel);
		memset(mouseData, 0, 2 * 2 * mClientChar.bytesPerPixel);

		// menu
		mMenuHeight = 0;
		menuData = NULL;

		mClientChar = aClientChar;
		convertCharacteristicsToHost(mXChar, mClientChar);

		// setup interna
		gFrameBuffer = NULL;
		reinitChar();

		// clear fb once on startup
		memset(gFrameBuffer, 0, mClientChar.width *
			mClientChar.height * mClientChar.bytesPerPixel);

		// finally set title
		mTitle = strdup(name);
		
		openDisplay();
	}

	virtual ~CocoaSystemDisplay()
	{
		free(mTitle);
		free(mouseData);
		free(menuData);
	}

	virtual	void finishMenu()
	{
	}

	/*
	 *	must be called with gX11Mutex locked
	 */
	void reinitChar()
	{
	        gFrameBuffer = (byte*) malloc(mClientChar.width *
			mClientChar.height * mClientChar.bytesPerPixel);
	}

	virtual void convertCharacteristicsToHost(DisplayCharacteristics &aHostChar, const DisplayCharacteristics &aClientChar)
	{
	}

	virtual bool changeResolution(const DisplayCharacteristics &aClientChar)
	{
		if (aClientChar.width == mClientChar.width 
		 && aClientChar.height == mClientChar.height
		 && aClientChar.bytesPerPixel == mClientChar.bytesPerPixel) {
			return true;
		} else {
			return false;
		}
	}

	virtual bool doChangeResolution(const DisplayCharacteristics &aClientChar)
	{
	        return false;
	}

	virtual void getHostCharacteristics(Container &modes)
	{
		// FIXME: implement me
	}

	virtual int getKeybLEDs()
	{
		return 0;
	}

	virtual void setKeybLEDs(int leds)
	{
	}

	virtual void updateTitle() 
	{
	}
	
	virtual	int toString(char *buf, int buflen) const
	{
		return snprintf(buf, buflen, "Cocoa");
	}

	virtual void setMouseGrab(bool enable)
	{
	}

	virtual void displayShow()
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

	}
};

SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms, bool fullscreen)
{
	if (gDisplay) return NULL;
	return new CocoaSystemDisplay(title, chr, redraw_ms);
}
