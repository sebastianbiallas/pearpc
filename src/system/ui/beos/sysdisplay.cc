/* 
 *	PearPC
 *	sysdisplay.cc - screen access functions for BeOS
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf
 *	Copyright (C) 1999-2004 Sebastian Biallas (sb@biallas.net)
 *	Copyright (C) 2004 Francois Revol (revol@free.fr)
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

#include <AppDefs.h>
#include <Application.h>
#include <Bitmap.h>
#include <Cursor.h>
#include <GraphicsDefs.h>
#include <InterfaceDefs.h>
#include <List.h>
#include <Locker.h>
#include <Message.h>
#include <Screen.h>
#include <View.h>
#include <Window.h>
#include <WindowScreen.h>

#include "system/display.h"
#include "system/types.h"
#include "system/systhread.h"
#include "system/sysexcept.h"
#include "system/sysvaccel.h"
#include "tools/data.h"
#include "tools/snprintf.h"

#include "sysbeos.h"

//#define DPRINTF(a...)
#define DPRINTF(a...) ht_printf(a)

//XXX:DEL uint gDamageAreaFirstAddr, gDamageAreaLastAddr;

/*XXX:DEL
struct {
	uint64 r_mask;
	uint64 g_mask;
	uint64 b_mask;
} PACKED gPosixRGBMask;*/

//extern "C" void __attribute__((regparm (3))) posix_vaccel_15_to_15(uint32 pixel, byte *input, byte *output);
//extern "C" void __attribute__((regparm (3))) posix_vaccel_15_to_32(uint32 pixel, byte *input, byte *output);

#if 0 /* moved to sysbeos.cc */
static uint8 beos_key_to_adb_key[] = {
/* ESC F1-F12                                                                   PRTSCR SLOCK */
0x35,    0x7a,0x78,0x63,0x76,0x60,0x61,0x62,0x64,0x65,0x6d,0x67,0x6f,           0xff,  0x6b,0x71,
/*   `   1-0                                                  -   =    BSPACE   INS  HOME P_UP         NLOCK /   *    -    */
    0x32,0x12,0x13,0x14,0x15,0x17,0x16,0x1a,0x1c,0x19,0x1d,0x1b,0x18,  0x33,    0x72,0x73,0x74,        0x47,0x4b,0x43,0x4e,
/* TAB   qwerty...                                                     \        DEL  END  P_DN         7    8    9    +    */
0x30,    0x0c,0x0d,0x0e,0x0f,0x11,0x10,0x20,0x22,0x1f,0x23,0x21,0x1e,  0x2a,    0x75,0x77,0x79,        0x59,0x5b,0x5c,0x45,
/* CLOCK ...                                                           ENTR                            4    5    6         */
0x39,0x00,0x01,0x02,0x03,0x05,0x04,0x26,0x28,0x25,0x29,0x27,           0x24,                           0x56,0x57,0x58,/*107*/
/* SHIFT ...                                                           SHIFT         UP                1    2    3    ENTR */
0x38,      0x06,0x07,0x08,0x09,0x0b,0x2d,0x2e,0x2b,0x2f,0x2c,          0x38,         0x3e,             0x53,0x54,0x55,0x4c,
/* CTRL /WIN/   ALT  SPACE ALTGR  /WIN MENU/   CTRL                           LEFT   DOWN   RIGHT      0         DEL       */
   0x36,/*0,*/ 0x37, 0x31, 0x3a,  /*0,  0,*/   0x36,                          0x3b,  0x3d,  0x3c,      0x52,     0x41


};
#endif

static uint8 blank_cursor_data[] = {
	0x10, /* pixels/side */
	0x01, /* bits/pix */
	0x00, /* hotspot y */
	0x00, /* hotspot x */
	/* color */
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
	/* mask */
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
};

static void findMaskShiftAndSize(uint &shift, uint &size, uint bitmask)
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


static byte *gBeOSFramebuffer = NULL;
/* old value of framebuffer, after replacing with BBitmap::Bits() */
/*
static Display *gXDisplay;
static Window gXWindow;
static GC gWhiteGC, gBlackGC;
static GC gGC;
static XImage *gXImage;
static XImage *gMenuXImage;
static XImage *gMouseXImage;
static Colormap gDefaultColormap;
*/
static BCursor *gBlankCursor;



#define MASK(shift, size) (((1 << (size))-1)<<(shift))
void BeOSSystemDisplay::dumpDisplayChar(const DisplayCharacteristics &chr)
{
	fprintf(stderr, "\tdimensions:          %d x %d pixels\n", chr.width, chr.height);
	fprintf(stderr, "\tpixel size in bytes: %d\n", chr.bytesPerPixel);
	fprintf(stderr, "\tpixel size in bits:  %d\n", chr.bytesPerPixel*8);
	fprintf(stderr, "\tred_mask:            %08x (%d bits)\n", MASK(chr.redShift, chr.redSize), chr.redSize);
	fprintf(stderr, "\tgreen_mask:          %08x (%d bits)\n", MASK(chr.greenShift, chr.greenSize), chr.greenSize);
	fprintf(stderr, "\tblue_mask:           %08x (%d bits)\n", MASK(chr.blueShift, chr.blueSize), chr.blueSize);
	fprintf(stderr, "\tdepth:               %d\n", chr.redSize + chr.greenSize + chr.blueSize);
}
#undef MASK

uint BeOSSystemDisplay::bitsPerPixelToXBitmapPad(uint bitsPerPixel)
{
	if (bitsPerPixel <= 8) {
		return 8;
	} else if (bitsPerPixel <= 16) {
		return 16;
	} else {
		return 32;
	}
}

BeOSSystemDisplay::BeOSSystemDisplay(const char *name, const DisplayCharacteristics &aClientChar, int redraw_ms)
	:SystemDisplay(aClientChar, redraw_ms)
{
	::printf("BeOSSystemDisplay::BeOSSystemDisplay()\n");
	sys_create_mutex(&mutex);
	mEventQueue = new Queue(true);
	mMouseEnabled = false;
	fbBitmap = NULL;
	fMenuBitmap = NULL;
	view = NULL;
	window = NULL;
	gBlankCursor = new BCursor(blank_cursor_data);
	gBeOSFramebuffer = NULL;
	gFrameBuffer = NULL;
	mTitle = strdup(name);
	mClientChar = aClientChar;
	convertCharacteristicsToHost(mBeChar, mClientChar);
	
	if (bitsPerPixelToXBitmapPad(mClientChar.bytesPerPixel*8) != mClientChar.bytesPerPixel*8) {
		ht_printf("nope. bytes per pixel is: %d. only 1,2 or 4 are allowed.\n", mClientChar.bytesPerPixel);
		exit(1);
	}
	
	mMenuHeight = 28;
	
/*	framebuffer = (byte*)malloc(mClientChar.width *
		mClientChar.height * mClientChar.bytesPerPixel);
	memset(framebuffer, 0, mClientChar.width *
		mClientChar.height * mClientChar.bytesPerPixel);
*/	
	mHomeMouseX = mClientChar.width / 2;
	mHomeMouseY = mClientChar.height / 2;
	
	mouseData = (byte*)malloc(2 * 2 * mClientChar.bytesPerPixel);
	memset(mouseData, 0, 2 * 2 * mClientChar.bytesPerPixel);
	
	BRect fbbounds(0, 0, mClientChar.width - 1, mClientChar.height - 1);
	BRect frame(fbbounds);
#ifdef BMP_MENU
	frame.bottom += mMenuHeight;
#endif
	BScreen bs;
	BRect sf(bs.Frame());
	frame.OffsetBySelf((sf.Width() - frame.Width()) / 2, (sf.Height() - frame.Height()) / 2);
	
	color_space cs = B_RGB32;
	mBeChar = mClientChar;
	if (mClientChar.bytesPerPixel == 2) {
		cs = B_RGB15;
		mBeChar.redShift = 10;
		mBeChar.redSize = 5;
		mBeChar.greenShift = 5;
		mBeChar.greenSize = 5;
		mBeChar.blueShift = 0;
		mBeChar.blueSize = 5;
/*		cs = B_RGB16;
		mBeChar.redShift = 11;
		mBeChar.redSize = 5;
		mBeChar.greenShift = 5;
		mBeChar.greenSize = 6;
		mBeChar.blueShift = 0;
		mBeChar.blueSize = 5;*/
	} else {
		mBeChar.bytesPerPixel = 4;
		mBeChar.redShift = 16;
		mBeChar.redSize = 8;
		mBeChar.greenShift = 8;
		mBeChar.greenSize = 8;
		mBeChar.blueShift = 0;
		mBeChar.blueSize = 8;
	}
	fbBitmap = new BBitmap(fbbounds, 0, cs);
	if (fbBitmap->InitCheck()) {
		fprintf(stderr, "BBitmap error 0x%08lx\n", fbBitmap->InitCheck());
		exit(1);
	}
	mBeChar.width = fbBitmap->BytesPerRow() / mBeChar.bytesPerPixel;
	
#if 0
	fprintf(stderr, "BBitmap display characteristics:\n");
	dumpDisplayChar(mBeChar);
	fprintf(stderr, "Client display characteristics:\n");
	dumpDisplayChar(mClientChar);
#endif
	
	
	// Maybe client and (X-)server display characeristics match
	if (0 && memcmp(&mClientChar, &mBeChar, sizeof (mClientChar)) == 0) {
		fprintf(stderr, "client and server display characteristics match!!\n");
		gFrameBuffer = (byte *)fbBitmap->Bits();
		gBeOSFramebuffer = NULL;
	} else {
		// Otherwise we need a second framebuffer
		gBeOSFramebuffer = (byte *)fbBitmap->Bits();
		gFrameBuffer = (byte*)malloc(mClientChar.width *
					     mClientChar.height * mClientChar.bytesPerPixel);
		memset(gFrameBuffer, 0, mClientChar.width *
		       mClientChar.height * mClientChar.bytesPerPixel);
		fprintf(stderr, "gFrameBuffer = %p\n", gFrameBuffer);
		//fprintf(stderr, "client and server display characteristics DONT match :-(\n");
	
	}
	
#ifdef BMP_MENU
	fbbounds.bottom += mMenuHeight;
#endif
	
	window = new SDWindow(frame, mTitle);
	view = new SDView(this/*fbBitmap*/, fbbounds, "framebuffer");
	window->AddChild(view);
	view->MakeFocus(true);
	//view->Invalidate(view->Bounds());
	
	window->SetPulseRate(redraw_ms*1000);
	window->Show();
	
	//updateTitle();
	
#if 0 /* XXX: remove */
	switch (mClientChar.bytesPerPixel) {
		case 1:
			ht_printf("nyi: %s::%d", __FILE__, __LINE__);
			exit(-1);
			break;
		case 2:
			gPosixRGBMask.r_mask = 0x7c007c007c007c00ULL;
			gPosixRGBMask.g_mask = 0x03e003e003e003e0ULL;
			gPosixRGBMask.b_mask = 0x001f001f001f001fULL;
			break;
		case 4:
			gPosixRGBMask.r_mask = 0x00ff000000ff0000ULL;
			gPosixRGBMask.g_mask = 0x0000ff000000ff00ULL;
			gPosixRGBMask.b_mask = 0x000000ff000000ffULL;
			break;
	}
#endif
	menuData = NULL;
	setExposed(true);
	::printf("BeOSSystemDisplay::BeOSSystemDisplay() end\n");
}

BeOSSystemDisplay::~BeOSSystemDisplay()
{
	if (!window) return;
	window->Lock();
	window->Quit();
	delete fbBitmap;
	delete gBlankCursor;
	gBlankCursor = NULL;
	
	free(mTitle);
	free(mouseData);
	if (gBeOSFramebuffer)
		free(gFrameBuffer);
	gFrameBuffer = NULL;
	gBeOSFramebuffer = NULL;
#ifdef BMP_MENU
	//if (menuData) free(menuData);
#endif
}

void BeOSSystemDisplay::finishMenu()
{
#ifdef BMP_MENU
/*	menuData = (byte*)malloc(mBeChar.width *
		mMenuHeight * mBeChar.bytesPerPixel);
	gMenuXImage = XCreateImage(gXDisplay, DefaultVisual(gXDisplay, DefaultScreen(gXDisplay)),
		mBeChar.redSize+mBeChar.greenSize+mBeChar.blueSize, ZPixmap, 0, (char*)menuData,
		mBeChar.width, mMenuHeight,
		mBeChar.bytesPerPixel*8, 0);
*/	
	// Is this ugly? Yes!
	fillRGB(0, 0, mClientChar.width, mClientChar.height, MK_RGB(0xff, 0xff, 0xff));
	drawMenu();
	convertDisplayClientToServer(0, mClientChar.height-1);
	fMenuBitmap = new BBitmap(BRect(0,0,mBeChar.width-1,mMenuHeight-1), 0, fbBitmap->ColorSpace());
	menuData = (byte *)fMenuBitmap->Bits();
	if (gBeOSFramebuffer) {
		memmove(menuData, gBeOSFramebuffer, mBeChar.width * mMenuHeight
			* mBeChar.bytesPerPixel);
	} else {
		memmove(menuData, gFrameBuffer, mBeChar.width * mMenuHeight
			* mBeChar.bytesPerPixel);
	}
	view->LockLooper();
	view->Invalidate(BRect(0, 0, mBeChar.width-1, mMenuHeight-1));
	view->UnlockLooper();

/*
	if (fMenuBitmap) {
		BWindow *w = new BWindow(fMenuBitmap->Bounds(), "debug", B_TITLED_WINDOW, 0L);
		BView *v = new BView(fMenuBitmap->Bounds(), "debugview", B_FOLLOW_NONE, B_WILL_DRAW);
		w->AddChild(v);
		v->SetViewBitmap(fMenuBitmap);
		w->Show();
	}*/
	
#endif
}

void BeOSSystemDisplay::convertCharacteristicsToHost(DisplayCharacteristics &aHostChar, const DisplayCharacteristics &aClientChar)
{
	BScreen bs;
	aHostChar = aClientChar;
	switch (bs.ColorSpace()) {
	case B_RGB32:
	case B_RGBA32:
	case B_RGB24:
		aHostChar.bytesPerPixel = 4;
		break;
	case B_RGB16:
	case B_RGB15:
	case B_RGBA15:
		aHostChar.bytesPerPixel = 2;
		break;
	case B_CMAP8:
	case B_GRAY8:
		aHostChar.bytesPerPixel = 1;
		break;
	default:
		exit(1);
	}
	aHostChar.scanLineLength = aHostChar.bytesPerPixel * bs.Frame().IntegerWidth();
}

bool BeOSSystemDisplay::changeResolution(const DisplayCharacteristics &aCharacteristics)
{
	// reject resolutions bigger than desktop. 
	// Not really necessary as BeOS isn't as broken as Windoze(tm).
	BScreen bs;
	BRect desktoprect(bs.Frame());
	if (aCharacteristics.width > (desktoprect.Width() + 1)
	|| aCharacteristics.height > (desktoprect.Height() + 1)) {
		// protect user from himself
		return false;
	}
	if (!view)
		return false;

	//return false;
	if (!view->LockLooper())
		return false;
	mClientChar = aCharacteristics;

	mHomeMouseX = mClientChar.width / 2;
	mHomeMouseY = mClientChar.height / 2;

	mBeChar.height = mClientChar.height;
	mBeChar.width = mClientChar.width;
	gFrameBuffer = (byte*)realloc(gFrameBuffer, mClientChar.width 
		* mClientChar.height * mClientChar.bytesPerPixel);
	memset(gFrameBuffer, 0, mClientChar.width 
		* mClientChar.height * mClientChar.bytesPerPixel);
	delete fbBitmap;
	fbBitmap = NULL;
	BRect fbbounds(0, 0, mClientChar.width - 1, mClientChar.height - 1);
	color_space cs = B_RGB32;
	mBeChar = mClientChar;
	if (mClientChar.bytesPerPixel == 2) {
		cs = B_RGB15;
		mBeChar.redShift = 10;
		mBeChar.redSize = 5;
		mBeChar.greenShift = 5;
		mBeChar.greenSize = 5;
		mBeChar.blueShift = 0;
		mBeChar.blueSize = 5;
	} else {
		mBeChar.bytesPerPixel = 4;
		mBeChar.redShift = 16;
		mBeChar.redSize = 8;
		mBeChar.greenShift = 8;
		mBeChar.greenSize = 8;
		mBeChar.blueShift = 0;
		mBeChar.blueSize = 8;
	}
	fbBitmap = new BBitmap(fbbounds, 0, cs);
	if (fbBitmap->InitCheck()) {
		fprintf(stderr, "BBitmap error 0x%08lx\n", fbBitmap->InitCheck());
		exit(1);
	}
	gBeOSFramebuffer = (byte *)fbBitmap->Bits();
	mBeChar.width = fbBitmap->BytesPerRow() / mBeChar.bytesPerPixel;
	//dumpDisplayChar(mBeChar);
	damageFrameBufferAll();
#ifdef BMP_MENU
	// WTF does this -1 come from ??
	window->ResizeTo(mBeChar.width-1, mBeChar.height+mMenuHeight-1);
#else
	window->ResizeTo(mBeChar.width-1, mBeChar.height-1);
#endif
	BRect sf(bs.Frame());
	window->MoveTo((sf.Width() - window->Bounds().Width()) / 2, (sf.Height() - window->Bounds().Height()) / 2);
	
	view->UnlockLooper();
	
	// XXX: test that!
	return true;
}

void BeOSSystemDisplay::getHostCharacteristics(Container &modes)
{
	// FIXME: implement me
}

int BeOSSystemDisplay::getKeybLEDs()
{
	int r = 0;
	uint32 mods = modifiers();
	if (mods & B_CAPS_LOCK) r |= KEYB_LED_NUM;
	if (mods & B_CAPS_LOCK) r |= KEYB_LED_CAPS;
	if (mods & B_SCROLL_LOCK) r |= KEYB_LED_SCROLL;
	return r;
}

void BeOSSystemDisplay::setKeybLEDs(int leds)
{
	uint32 mods = 0;
	if (leds & KEYB_LED_NUM) mods |= B_CAPS_LOCK;
	if (leds & KEYB_LED_CAPS) mods |= B_CAPS_LOCK;
	if (leds & KEYB_LED_SCROLL) mods |= B_SCROLL_LOCK;
	// XXX: warning: set_keyboard_locks() doesn't work correctly
	set_keyboard_locks(mods);
}

void BeOSSystemDisplay::updateTitle() 
{
	String key;
	int key_toggle_mouse_grab = gKeyboard->getKeyConfig().key_toggle_mouse_grab;
	SystemKeyboard::convertKeycodeToString(key, key_toggle_mouse_grab);
	ht_snprintf(mCurTitle, sizeof mCurTitle, "%s - [%s %s mouse]", mTitle,key.contentChar(), (isMouseGrabbed() ? "disables" : "enables"));
	window->Lock();
	window->SetTitle(mCurTitle);
	window->Unlock();
}

int BeOSSystemDisplay::toString(char *buf, int buflen) const
{
	return snprintf(buf, buflen, "POSIX BeOS");
}

void BeOSSystemDisplay::setMouseGrab(bool enable)
{
	SystemDisplay::setMouseGrab(enable);
	updateTitle();
	if (!view->LockLooper())
		return;
	if (enable) {
		mResetMouseX = mCurMouseX;
		mResetMouseY = mCurMouseY;
		view->SetViewCursor(gBlankCursor);
		
		BPoint p(mHomeMouseX, mHomeMouseY);
		view->ConvertToScreen(&p);
		set_mouse_position((int32)p.x, (int32)p.y);
	} else {
		view->SetViewCursor(B_CURSOR_SYSTEM_DEFAULT);
		
		BPoint p(mResetMouseX, mResetMouseY);
		view->ConvertToScreen(&p);
		set_mouse_position((int32)p.x, (int32)p.y);
	}
	view->UnlockLooper();
}

void BeOSSystemDisplay::displayShow()
{
	uint firstDamagedLine, lastDamagedLine;
	// We've got problems with races here because gcard_write1/2/4
	// might set gDamageAreaFirstAddr, gDamageAreaLastAddr.
	// We can't use mutexes in gcard for speed reasons. So we'll
	// try to minimize the probability of loosing the race.
	if (!isExposed()) return;
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
	//convertDisplayClientToServer(firstDamagedLine, lastDamagedLine);
	// wtf, doesn't seem to work...
	sys_convert_display(mClientChar, mBeChar, gFrameBuffer, gBeOSFramebuffer, firstDamagedLine, lastDamagedLine);
	view->LockLooper();
	
	//view->Invalidate(view->Bounds());
	
	// supposedly faster than Invalidate() ...
	BRect bounds(view->Bounds());
	bounds.top = firstDamagedLine;
	bounds.bottom = lastDamagedLine;
#ifdef BMP_MENU
	bounds.OffsetBySelf(0, mMenuHeight);
#endif
	view->Draw(bounds);
	view->Flush();
	
	view->UnlockLooper();
/*
	convertDisplayClientToServer();
	sys_lock_mutex(mutex);
	XPutImage(gXDisplay, gXWindow, gGC, gMenuXImage, 0, 0, 0, 0,
		gDisplay->mClientChar.width,
		mMenuHeight);
	XPutImage(gXDisplay, gXWindow, gGC, gXImage, 0, 0, 0, mMenuHeight,
		gDisplay->mClientChar.width,
		gDisplay->mClientChar.height);
	if (mHWCursorVisible) {
		XPutImage(gXDisplay, gXWindow, gGC, gMouseXImage, 0, 0, 
			mHWCursorX, mHWCursorY, 2, 2);
	}
	sys_unlock_mutex(mutex);
*/
}

void BeOSSystemDisplay::convertDisplayClientToServer(uint firstLine, uint lastLine)
{
	if (!gBeOSFramebuffer) return;	// great! nothing to do.
	byte *buf = gFrameBuffer + mClientChar.bytesPerPixel * mClientChar.width * firstLine;
	byte *bbuf = gBeOSFramebuffer + mBeChar.bytesPerPixel * mBeChar.width * firstLine;
/*	if ((mClientChar.bytesPerPixel == 2) && (mBeChar.bytesPerPixel == 2)) {
		posix_vaccel_15_to_15(mClientChar.height*mClientChar.width, buf, xbuf);
	} else if ((mClientChar.bytesPerPixel == 2) && (mBeChar.bytesPerPixel == 4)) {
		posix_vaccel_15_to_32(mClientChar.height*mClientChar.width, buf, xbuf);
	} else */
	for (int y=firstLine; y <= lastLine; y++) {
		for (int x=0; x < mClientChar.width; x++) {
			uint r, g, b;
			uint p;
			switch (mClientChar.bytesPerPixel) {
				case 1:
					p = palette[buf[0]];
					break;
				case 2:
					p = (buf[0] << 8) | buf[1];
					break;
				case 4:
					p = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
					break;
				default:
					ht_printf("internal error in %s:%d\n", __FILE__, __LINE__);
					exit(1);
					break;
			}
			r = (p >> mClientChar.redShift) & ((1<<mClientChar.redSize)-1);
			g = (p >> mClientChar.greenShift) & ((1<<mClientChar.greenSize)-1);
			b = (p >> mClientChar.blueShift) & ((1<<mClientChar.blueSize)-1);
			convertBaseColor(r, mClientChar.redSize, mBeChar.redSize);
			convertBaseColor(g, mClientChar.greenSize, mBeChar.greenSize);
			convertBaseColor(b, mClientChar.blueSize, mBeChar.blueSize);
			p = (r << mBeChar.redShift) | (g << mBeChar.greenShift)
				| (b << mBeChar.blueShift);
			switch (mBeChar.bytesPerPixel) {
				case 1:
					bbuf[0] = p;
					break;
				case 2:
					bbuf[1] = p>>8; bbuf[0] = p;
					break;
				case 4:
					*(uint32*)bbuf = p;
					break;
			}
			bbuf += mBeChar.bytesPerPixel;
			buf += mClientChar.bytesPerPixel;
		}
	}
}

#if 0
void BeOSSystemDisplay::queueEvent(DisplayEvent &ev)
{
	DisplayEvent *e = (DisplayEvent *)malloc(sizeof (DisplayEvent));
	*e = ev;
	mEventQueue->enQueue(new Pointer(e));
}
#endif

void BeOSSystemDisplay::startRedrawThread(int msec)
{
::printf("startRedrawTh\n");
	window->Lock();
	window->SetPulseRate(msec*1000);
	window->Unlock();
}

SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms)
{
	if (gDisplay) return NULL;
	return new BeOSSystemDisplay(title, chr, redraw_ms);
}
