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

#include <X11/Xutil.h>

#include "system/display.h"
#include "system/types.h"
#include "system/systhread.h"
#include "system/sysexcept.h"
#include "system/sysvaccel.h"
#include "tools/data.h"
#include "tools/snprintf.h"
//#include "configparser.h"

#include "sysx11.h"

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

class X11SystemDisplay: public SystemDisplay
{
	byte *		mXFrameBuffer;
	GC		mXGC;
	XImage *	mXImage;
	XImage *	mMenuXImage;
	XImage *	mMouseXImage;
	Colormap	mDefaultColormap;

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
	X11SystemDisplay(const char *name, const DisplayCharacteristics &aClientChar, int redraw_ms)
		:SystemDisplay(aClientChar, redraw_ms)
	{
		if (bitsPerPixelToXBitmapPad(mClientChar.bytesPerPixel*8) != mClientChar.bytesPerPixel*8) {
			ht_printf("nope. bytes per pixel is: %d. only 1,2 or 4 are allowed.\n", mClientChar.bytesPerPixel);
			exit(1);
		}

		// mouse
		mouseData = (byte*)malloc(2 * 2 * mClientChar.bytesPerPixel);
		memset(mouseData, 0, 2 * 2 * mClientChar.bytesPerPixel);

		// menu
//		mMenuHeight = 28;
		mMenuHeight = 0;
		menuData = NULL;

		mXImage = NULL;

		mClientChar = aClientChar;
		convertCharacteristicsToHost(mXChar, mClientChar);

		int screen_num = DefaultScreen(gX11Display);
		gX11Window = XCreateSimpleWindow(gX11Display, 
			RootWindow(gX11Display, screen_num), 0, 0,
				mClientChar.width, mClientChar.height+mMenuHeight,
				0, BlackPixel(gX11Display, screen_num),
				BlackPixel(gX11Display, screen_num));

		XStoreName(gX11Display, gX11Window, name);

		XSetWindowAttributes attr;
		attr.save_under = 1;
		attr.backing_store = Always;
		XChangeWindowAttributes(gX11Display, gX11Window, CWSaveUnder|CWBackingStore, &attr);

		mDefaultColormap = DefaultColormap(gX11Display, screen_num);

		XSelectInput(gX11Display, gX11Window, 
			ExposureMask | KeyPressMask | KeyReleaseMask 
			| ButtonPressMask | ButtonReleaseMask | PointerMotionMask
			| EnterWindowMask | LeaveWindowMask | StructureNotifyMask
			| VisibilityChangeMask | FocusChangeMask);

		XMapWindow(gX11Display, gX11Window);

		XEvent event;
		XNextEvent(gX11Display, &event);

		uint XDepth = mXChar.redSize + mXChar.greenSize + mXChar.blueSize;

		mMouseXImage = XCreateImage(gX11Display, DefaultVisual(gX11Display, screen_num),
			XDepth, ZPixmap, 0, (char*)mouseData,
			2, 2,
			mXChar.bytesPerPixel*8, 0);

		mXGC = DefaultGC(gX11Display, screen_num);

		// setup interna
		gFrameBuffer = NULL;
		reinitChar();

		// clear fb once on startup
		memset(gFrameBuffer, 0, mClientChar.width *
			mClientChar.height * mClientChar.bytesPerPixel);

#if 0
		fprintf(stderr, "X Server display characteristics:\n");
		dumpDisplayChar(mXChar);
		fprintf(stderr, "Client display characteristics:\n");
		dumpDisplayChar(mClientChar);
#endif

		// finally set title
		mTitle = strdup(name);
	}

	virtual ~X11SystemDisplay()
	{
		gX11Display = NULL;
		free(mTitle);
		free(mouseData);
		free(gFrameBuffer);
		if (menuData) free(menuData);
	}

	virtual	void finishMenu()
	{
		menuData = (byte*)malloc(mXChar.width *
			mMenuHeight * mXChar.bytesPerPixel);
		mMenuXImage = XCreateImage(gX11Display, DefaultVisual(gX11Display, DefaultScreen(gX11Display)),
			mXChar.redSize+mXChar.greenSize+mXChar.blueSize, ZPixmap, 0, (char*)menuData,
			mXChar.width, mMenuHeight,
			mXChar.bytesPerPixel*8, 0);

		// Is this ugly? Yes!
		fillRGB(0, 0, mClientChar.width, mClientChar.height, MK_RGB(0xff, 0xff, 0xff));
		drawMenu();
//		convertDisplayClientToServer(0, mClientChar.height-1);
		if (mXFrameBuffer) {
			sys_convert_display(mClientChar, mXChar, gFrameBuffer, mXFrameBuffer, 0, mClientChar.height-1);
			memmove(menuData, mXFrameBuffer, mXChar.width * mMenuHeight
				* mXChar.bytesPerPixel);
		} else {
			memmove(menuData, gFrameBuffer, mXChar.width * mMenuHeight
				* mXChar.bytesPerPixel);
		}
	}

	/*
	 *	must be called with gX11Mutex locked
	 */
	void reinitChar()
	{
		gFrameBuffer = (byte*)realloc(gFrameBuffer, mClientChar.width *
			mClientChar.height * mClientChar.bytesPerPixel);
		damageFrameBufferAll();

		mHomeMouseX = mClientChar.width / 2;
		mHomeMouseY = mClientChar.height / 2;

		uint XDepth = mXChar.redSize + mXChar.greenSize + mXChar.blueSize;
		int screen_num = DefaultScreen(gX11Display);

		if (mXImage) XDestroyImage(mXImage);	// no need to free gXFrameBuffer. XDeleteImage does this.

		// Maybe client and (X-)server display characteristics match
		if (0 && memcmp(&mClientChar, &mXChar, sizeof (mClientChar)) == 0) {
//			fprintf(stderr, "client and server display characteristics match!!\n");
			mXFrameBuffer = NULL;

			mXImage = XCreateImage(gX11Display, DefaultVisual(gX11Display, screen_num),
				XDepth, ZPixmap, 0, (char*)gFrameBuffer,
				mXChar.width, mXChar.height,
				mXChar.bytesPerPixel*8, 0);
		} else {
			// Otherwise we need a second framebuffer
//			fprintf(stderr, "client and server display characteristics DONT match :-(\n");
			mXFrameBuffer = (byte*)malloc(mXChar.width
				* mXChar.height * mXChar.bytesPerPixel);

			mXImage = XCreateImage(gX11Display, DefaultVisual(gX11Display, screen_num),
				XDepth, ZPixmap, 0, (char*)mXFrameBuffer,
				mXChar.width, mXChar.height,
				mXChar.bytesPerPixel*8, 0);
		}
	}

	virtual void convertCharacteristicsToHost(DisplayCharacteristics &aHostChar, const DisplayCharacteristics &aClientChar)
	{
		sys_lock_mutex(gX11Mutex);
		int screen_num = DefaultScreen(gX11Display);

		XVisualInfo info_tmpl;
		int ninfo;
		info_tmpl.screen = screen_num;
		info_tmpl.visualid = XVisualIDFromVisual(DefaultVisual(gX11Display, screen_num));
		XVisualInfo *info = XGetVisualInfo(gX11Display,
			VisualScreenMask | VisualIDMask, &info_tmpl, &ninfo);
		/*
		fprintf(stderr, "got %d XVisualInfo's:\n", ninfo);
		for (int i=0; i<ninfo; i++) {
			fprintf(stderr, "XVisualInfo %d:\n", i);
			fprintf(stderr, "\tscreen = %d:\n", info[i].screen);
			fprintf(stderr, "\tdepth = %d:\n", info[i].depth);
			fprintf(stderr, "\tred   = %x:\n", info[i].red_mask);
			fprintf(stderr, "\tgreen = %x:\n", info[i].green_mask);
			fprintf(stderr, "\tblue  = %x:\n", info[i].blue_mask);
		}
		*/
		// generate X characteristics from visual info
		aHostChar = aClientChar;
		if (ninfo) {
			aHostChar.bytesPerPixel = bitsPerPixelToXBitmapPad(info->depth) >> 3;
			aHostChar.scanLineLength = aHostChar.bytesPerPixel * aHostChar.width;
			findMaskShiftAndSize(aHostChar.redShift, aHostChar.redSize, info->red_mask);
			findMaskShiftAndSize(aHostChar.greenShift, aHostChar.greenSize, info->green_mask);
			findMaskShiftAndSize(aHostChar.blueShift, aHostChar.blueSize, info->blue_mask);
		} else {
			sys_unlock_mutex(gX11Mutex);
			printf("ARGH! Couldn't get XVisualInfo...\n");
			exit(1);
		}
		XFree(info);
		sys_unlock_mutex(gX11Mutex);
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
		/*
		 *	I don't understand how XResizeWindow is supposed to work
		 *	If you understand, feel free to fix this:
		 */
		DisplayCharacteristics oldClientChar = mClientChar;
		DisplayCharacteristics oldHostChar = mXChar;
		DisplayCharacteristics newXChar;
		
		convertCharacteristicsToHost(newXChar, aClientChar);
		sys_lock_mutex(gX11Mutex);
		mXChar = newXChar;
		mClientChar = aClientChar;
		if (bitsPerPixelToXBitmapPad(mXChar.bytesPerPixel*8) != mXChar.bytesPerPixel*8) {
			fprintf(stderr, "bitsPerPixelToXBitmapPad(%d) failed\n", mXChar.bytesPerPixel*8);
			mClientChar = oldClientChar;
			mXChar = oldHostChar;
			sys_unlock_mutex(gX11Mutex);
			return false;
		}

		XResizeWindow(gX11Display, gX11Window, mXChar.width, mXChar.height+mMenuHeight);

		XSync(gX11Display, False);

		XWindowAttributes attr;
		if (!XGetWindowAttributes(gX11Display, gX11Window, &attr)) {
			fprintf(stderr, "Couldn't get X window size\n");
			mClientChar = oldClientChar;
			mXChar = oldHostChar;
			XResizeWindow(gX11Display, gX11Window, mXChar.width, mXChar.height+mMenuHeight);
			sys_unlock_mutex(gX11Mutex);
			return false;
		}

		if ((int)attr.width < mXChar.width || (int)attr.height < (mXChar.height+mMenuHeight)) {
	    		fprintf(stderr, "Couldn't change X window size to %dx%d\n", mXChar.width, mXChar.height);
	    		fprintf(stderr, "Reported new size: %dx(%d+%d)\n", attr.width, attr.height-mMenuHeight, mMenuHeight);
			mClientChar = oldClientChar;
			mXChar = oldHostChar;
			XResizeWindow(gX11Display, gX11Window, mXChar.width, mXChar.height+mMenuHeight);
			sys_unlock_mutex(gX11Mutex);
			return false;
		}

		reinitChar();
		sys_unlock_mutex(gX11Mutex);
		fprintf(stderr, "Change resolution OK\n");
		return true;
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
		String key;
		int key_toggle_mouse_grab = gKeyboard->getKeyConfig().key_toggle_mouse_grab;
		SystemKeyboard::convertKeycodeToString(key, key_toggle_mouse_grab);
		ht_snprintf(mCurTitle, sizeof mCurTitle, "%s - [%s %s mouse]", mTitle,key.contentChar(), (isMouseGrabbed() ? "disables" : "enables"));
		XTextProperty name_prop;
		char *mCurTitlep = mCurTitle;
		XStringListToTextProperty(&mCurTitlep, 1, &name_prop);
		XSetWMName(gX11Display, gX11Window, &name_prop);
	}
	
	virtual	int toString(char *buf, int buflen) const
	{
		return snprintf(buf, buflen, "POSIX X11");
	}

	virtual void setMouseGrab(bool enable)
	{
		SystemDisplay::setMouseGrab(enable);
		updateTitle();
		if (enable) {
			mResetMouseX = mCurMouseX;
			mResetMouseY = mCurMouseY;

			static Cursor cursor;
			static bool cursor_created = false;

			static char shape_bits[16*16] = {0, };
			static char mask_bits[16*16] = {0, };

			if (!cursor_created) {
				Pixmap shape, mask;
				XColor white, black;
				shape = XCreatePixmapFromBitmapData(gX11Display,
						RootWindow(gX11Display, DefaultScreen(gX11Display)),
						shape_bits, 16, 16, 1, 0, 1);
				mask =  XCreatePixmapFromBitmapData(gX11Display,
						RootWindow(gX11Display, DefaultScreen(gX11Display)),
						mask_bits, 16, 16, 1, 0, 1);
				XParseColor(gX11Display, mDefaultColormap, "black", &black);
				XParseColor(gX11Display, mDefaultColormap, "white", &white);
				cursor = XCreatePixmapCursor(gX11Display, shape, mask,
						&white, &black, 1, 1);
				cursor_created = true;
			}

			XDefineCursor(gX11Display, gX11Window, cursor);
			XWarpPointer(gX11Display, gX11Window, gX11Window, 0, 0, 0, 0, mHomeMouseX, mHomeMouseY);
		} else {
			XWarpPointer(gX11Display, gX11Window, gX11Window, 0, 0, 0, 0, mResetMouseX, mResetMouseY);
			XUndefineCursor(gX11Display, gX11Window);
		}
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

		if (mXFrameBuffer) {
			sys_convert_display(mClientChar, mXChar, gFrameBuffer, mXFrameBuffer, firstDamagedLine, lastDamagedLine);
		}

		sys_lock_mutex(gX11Mutex);
		// draw menu
/*		XPutImage(gX11Display, gX11Window, mXGC, mMenuXImage, 0, 0, 0, 0,
			mClientChar.width,
			mMenuHeight);*/

		XPutImage(gX11Display, gX11Window, mXGC, mXImage,
			0,
			firstDamagedLine,
			0,
			mMenuHeight+firstDamagedLine,
			mClientChar.width,
			lastDamagedLine-firstDamagedLine+1);

/*		if (mHWCursorVisible) {
			XPutImage(gX11Display, gX11Window, mXGC, mMouseXImage, 0, 0, 
				mHWCursorX, mHWCursorY, 2, 2);
		}*/
		sys_unlock_mutex(gX11Mutex);
	}
};

SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms, bool fullscreen)
{
	if (gDisplay) return NULL;
	return new X11SystemDisplay(title, chr, redraw_ms);
}
