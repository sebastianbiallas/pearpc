/* 
 *	PearPC
 *	sysdisplay.cc - screen access functions for BeOS
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

#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <cstring>

#include <AppDefs.h>
#include <Application.h>
#include <Bitmap.h>
#include <Cursor.h>
#include <GraphicsDefs.h>
#include <List.h>
#include <Message.h>
#include <Screen.h>
#include <View.h>
#include <Window.h>

#include "system/display.h"
#include "system/types.h"
#include "system/systhread.h"
#include "system/sysexcept.h"
#include "tools/data.h"
#include "tools/snprintf.h"
// for stopping the CPU
#include "cpu_generic/ppc_cpu.h"

#define DPRINTF(a...)
//#define DPRINTF(a...) ht_printf(a)

//#define BMP_MENU

struct {
	uint64 r_mask;
	uint64 g_mask;
	uint64 b_mask;
} PACKED gPosixRGBMask;

//extern "C" void __attribute__((regparm (3))) posix_vaccel_15_to_15(uint32 pixel, byte *input, byte *output);
//extern "C" void __attribute__((regparm (3))) posix_vaccel_15_to_32(uint32 pixel, byte *input, byte *output);

#if 0
static uint8 x11_key_to_adb_key[256] = {
	// 0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x35_,0x12_,0x13_,0x14_,0x15_,0x17_,0x16_,
	0x1a_,0x1c_,0x19_,0x1d_,0x1b_,0x18_,0x33_,0x30_,0x0c_,0x0d_,0x0e_,0x0f_,0x11_,0x10_,0x20_,0x22_,
	0x1f_,0x23_,0x21_,0x1e_,0x24_,0x36_,0x00_,0x01_,0x02_,0x03_,0x05_,0x04_,0x26_,0x28_,0x25_,0x29_,
	0x27_,0x32_~,0x38,0x2a_,0x06_,0x07_,0x08_,0x09_,0x0b_,0x2d_,0x2e_,0x2b_,0x2f_,0x2c_,0x38/*0x7b*/,0xff,
	0x37_,0x31_,0xff,0x7a_,0x78_,0x63_,0x76_,0x60_,0x61_,0x62_,0x64_,0x65_,0x6d_,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x67_,
	0x6f_,0x73_,0x3e_,0x74_,0x3b_,0xff???,0x3c_,0x77_,0x3d_,0x79_,0x72_,0x75_,0x4c_,0x36_RCTL,0xff,0xff,
	0xff,0x3a_,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
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


byte *gFramebuffer = NULL;
static byte *beosframebuffer = NULL;
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
static Queue *mEventQueue;
static DisplayCharacteristics mBeChar;
static int mLastMouseX, mLastMouseY;
static int mCurMouseX, mCurMouseY;
static int mResetMouseX, mResetMouseY;
static int mHomeMouseX, mHomeMouseY;
static bool mMouseButton[3];
static bool mMouseEnabled;
static char *mTitle;
static char mCurTitle[200];
static byte *mouseData;
static byte *menuData;

class BeOSSystemDisplay;

class SDWindow : public BWindow {
public:
	SDWindow(BRect frame, const char *name);
	~SDWindow();
virtual bool	QuitRequested();
};

class SDView : public BView {
public:
	SDView(BeOSSystemDisplay *sd, BRect frame, const char *name);
	~SDView();
	
virtual void	MessageReceived(BMessage *msg);
virtual void	Draw(BRect updateRect);
virtual void	MouseDown(BPoint where);
virtual void	MouseUp(BPoint where);
virtual void	MouseMoved(BPoint where, uint32 code, const BMessage *a_message);
virtual void	KeyDown(const char *bytes, int32 numBytes);
virtual void	KeyUp(const char *bytes, int32 numBytes);
virtual void	Pulse();
	
	void	QueueMessage(BMessage *msg);
	BMessage	*UnqueueMessage(bool sync);
private:
	BList	fMsgList;
	BeOSSystemDisplay	*fSystemDisplay;
	BBitmap	*fFramebuffer;
	sem_id	fMsgSem;
};

class BeOSSystemDisplay: public SystemDisplay
{
friend class SDView;
	sys_thread redrawthread;
	sys_mutex mutex;
	BBitmap *fbBitmap;
	BBitmap *fMenuBitmap;
	SDView *view;
	SDWindow *window;
	
	void dumpDisplayChar(const DisplayCharacteristics &chr);
	uint bitsPerPixelToXBitmapPad(uint bitsPerPixel);
public:
	BeOSSystemDisplay(const char *name, const DisplayCharacteristics &chr);
	virtual ~BeOSSystemDisplay();
	virtual	void finishMenu();
	void updateTitle();
	virtual	int toString(char *buf, int buflen) const;
	void clientMouseEnable(bool enable);
	virtual void getSyncEvent(DisplayEvent &ev);
	virtual bool getEvent(DisplayEvent &ev);
	virtual void displayShow();
	void convertDisplayClientToServer();
	virtual void queueEvent(DisplayEvent &ev);
	static void *eventLoop(void *p);
	virtual void startRedrawThread(int msec);
};

SDWindow::SDWindow(BRect frame, const char *name)
	: BWindow(frame, name, B_TITLED_WINDOW, B_NOT_RESIZABLE|B_NOT_ZOOMABLE)
{
}

SDWindow::~SDWindow()
{
}

bool SDWindow::QuitRequested()
{
	SDView *view = dynamic_cast<SDView *>(FindView("framebuffer"));
	if (view) {
		BMessage *msg = new BMessage(B_QUIT_REQUESTED);
		view->QueueMessage(msg);
	}
	return false;
}

SDView::SDView(BeOSSystemDisplay *sd, BRect frame, const char *name)
	: BView(frame, name, B_FOLLOW_ALL_SIDES, B_WILL_DRAW)
{
	fSystemDisplay = sd;
	fFramebuffer = sd->fbBitmap;
	fMsgList.MakeEmpty();
	SetViewColor(0,255,0);
	fMsgSem = create_sem(1, "PearPC MessageList sem");
}

SDView::~SDView()
{
	delete_sem(fMsgSem);
}

void SDView::MessageReceived(BMessage *msg)
{
	BMessage *event;
	switch (msg->what) {
	case B_UNMAPPED_KEY_DOWN:
	case B_UNMAPPED_KEY_UP:
		event = Window()->DetachCurrentMessage();
		QueueMessage(event);
		return;
	}
	BView::MessageReceived(msg);
}

void SDView::Draw(BRect updateRect)
{
	fSystemDisplay->convertDisplayClientToServer();
#ifdef BMP_MENU
	if (fSystemDisplay->fMenuBitmap)
		DrawBitmap(fSystemDisplay->fMenuBitmap, fSystemDisplay->fMenuBitmap->Bounds());
	DrawBitmap(fFramebuffer, Bounds().OffsetByCopy(0,fSystemDisplay->mMenuHeight));
#else
	DrawBitmap(fFramebuffer, Bounds());
#endif
	if (fSystemDisplay->mHWCursorVisible) {
		//XPutImage(gXDisplay, gXWindow, gGC, gMouseXImage, 0, 0, 
		//	mHWCursorX, mHWCursorY, 2, 2);
	}
}

void SDView::MouseDown(BPoint where)
{
	BMessage *event = Window()->DetachCurrentMessage();
	QueueMessage(event);
}

void SDView::MouseUp(BPoint where)
{
	BMessage *event = Window()->DetachCurrentMessage();
	QueueMessage(event);
}

void SDView::MouseMoved(BPoint where, uint32 code, const BMessage *a_message)
{
	BMessage *event = Window()->DetachCurrentMessage();
	QueueMessage(event);
}

void SDView::KeyDown(const char *bytes, int32 numBytes)
{
	BMessage *event = Window()->DetachCurrentMessage();
	QueueMessage(event);
}

void SDView::KeyUp(const char *bytes, int32 numBytes)
{
	BMessage *event = Window()->DetachCurrentMessage();
	QueueMessage(event);
}

void SDView::Pulse()
{
	Invalidate(Bounds()); /* cause a redraw */
}

void SDView::QueueMessage(BMessage *msg)
{
	fMsgList.AddItem(msg, fMsgList.CountItems());
	release_sem(fMsgSem);
}

BMessage *SDView::UnqueueMessage(bool sync)
{
	BMessage *msg;
	acquire_sem_etc(fMsgSem, 1, sync?0:B_RELATIVE_TIMEOUT, 0LL);
	LockLooper(); /* BList not threadsafe */
	msg = (BMessage *)fMsgList.RemoveItem(0L);
	UnlockLooper();
/*	if (msg)
		msg->PrintToStream();*/
	return msg;
}



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

BeOSSystemDisplay::BeOSSystemDisplay(const char *name, const DisplayCharacteristics &chr)
	:SystemDisplay(chr)
{
	sys_create_mutex(&mutex);
	mEventQueue = new Queue(true);
	mMouseEnabled = false;
	fbBitmap = NULL;
	fMenuBitmap = NULL;
	view = NULL;
	window = NULL;
	gBlankCursor = new BCursor(blank_cursor_data);
	beosframebuffer = NULL;
	
	if (bitsPerPixelToXBitmapPad(mClientChar.bytesPerPixel*8) != mClientChar.bytesPerPixel*8) {
		printf("nope. bytes per pixel is: %d. only 1,2 or 4 are allowed.\n", mClientChar.bytesPerPixel);
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
	mBeChar.indexed = false;
	
#if 0
	fprintf(stderr, "BBitmap display characteristics:\n");
	dumpDisplayChar(mBeChar);
	fprintf(stderr, "Client display characteristics:\n");
	dumpDisplayChar(mClientChar);
#endif
	
	
	uint bDepth = mBeChar.redSize + mBeChar.greenSize + mBeChar.blueSize;
	// Maybe client and (X-)server display characeristics match
	if (0 && memcmp(&mClientChar, &mBeChar, sizeof (mClientChar)) == 0) {
		fprintf(stderr, "client and server display characteristics match!!\n");
		gFramebuffer = (byte *)fbBitmap->Bits();
	
#if 0
		gXImage = XCreateImage(gXDisplay, DefaultVisual(gXDisplay, screen_num),
			XDepth, ZPixmap, 0, (char*)gFramebuffer,
			mBeChar.width, mBeChar.height,
			mBeChar.bytesPerPixel*8, 0);
#endif
	} else {
		// Otherwise we need a second framebuffer
		beosframebuffer = (byte *)fbBitmap->Bits();
		gFramebuffer = (byte*)malloc(mClientChar.width *
			mClientChar.height * mClientChar.bytesPerPixel);
		memset(gFramebuffer, 0, mClientChar.width *
			mClientChar.height * mClientChar.bytesPerPixel);
			
		fprintf(stderr, "client and server display characteristics DONT match :-(\n");
	
	}
	
	//fbbounds.bottom += mMenuHeight;
	window = new SDWindow(frame, name);
	view = new SDView(this/*fbBitmap*/, fbbounds, "framebuffer");
	window->AddChild(view);
	view->MakeFocus(true);
	
	window->Show();
	
	mTitle = strdup(name);
	updateTitle();
	

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
	menuData = NULL;
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
	if (beosframebuffer)
		free(gFramebuffer);
	gFramebuffer = NULL;
	beosframebuffer = NULL;
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
	convertDisplayClientToServer();
	fMenuBitmap = new BBitmap(BRect(0,0,mBeChar.width-1,mMenuHeight-1), 0, fbBitmap->ColorSpace());
	menuData = (byte *)fMenuBitmap->Bits();
	if (beosframebuffer) {
		memmove(menuData, beosframebuffer, mBeChar.width * mMenuHeight
			* mBeChar.bytesPerPixel);
	} else {
		memmove(menuData, gFramebuffer, mBeChar.width * mMenuHeight
			* mBeChar.bytesPerPixel);
	}
	if (fMenuBitmap) {
		BWindow *w = new BWindow(fMenuBitmap->Bounds(), "debug", B_TITLED_WINDOW, 0L);
		BView *v = new BView(fMenuBitmap->Bounds(), "debugview", B_FOLLOW_NONE, B_WILL_DRAW);
		w->AddChild(v);
		v->SetViewBitmap(fMenuBitmap);
		w->Show();
	}
	
#endif
}

void BeOSSystemDisplay::updateTitle() 
{
	ht_snprintf(mCurTitle, sizeof mCurTitle, "%s - [F12 %s mouse]", mTitle, mMouseEnabled ? "disables" : "enables");
	window->Lock();
	window->SetTitle(mCurTitle);
	window->Unlock();
}

int BeOSSystemDisplay::toString(char *buf, int buflen) const
{
	return snprintf(buf, buflen, "POSIX BeOS");
}

void BeOSSystemDisplay::clientMouseEnable(bool enable)
{
	mMouseEnabled = enable;
	updateTitle();
	if (enable) {
		mResetMouseX = mCurMouseX;
		mResetMouseY = mCurMouseY;
		
#if 0
		static Cursor cursor;
		static unsigned cursor_created = 0;
		
		static char shape_bits[16*16] = {0, };
		static char mask_bits[16*16] = {0, };
		
		if (!cursor_created) {
			Pixmap shape, mask;
			XColor white, black;
			shape = XCreatePixmapFromBitmapData(gXDisplay,
					RootWindow(gXDisplay, DefaultScreen(gXDisplay)),
					shape_bits, 16, 16, 1, 0, 1);
			mask =  XCreatePixmapFromBitmapData(gXDisplay,
					RootWindow(gXDisplay, DefaultScreen(gXDisplay)),
					mask_bits, 16, 16, 1, 0, 1);
			XParseColor(gXDisplay, gDefaultColormap, "black", &black);
			XParseColor(gXDisplay, gDefaultColormap, "white", &white);
			cursor = XCreatePixmapCursor(gXDisplay, shape, mask,
					&white, &black, 1, 1);
			cursor_created = 1;
		}
		
		XDefineCursor(gXDisplay, gXWindow, cursor);
		XWarpPointer(gXDisplay, gXWindow, gXWindow, 0, 0, 0, 0, mHomeMouseX, mHomeMouseY);
#endif
		be_app->Lock();
		be_app->HideCursor();
		be_app->Unlock();
		/*view->LockLooper();
		if (gBlankCursor)
			view->SetViewCursor(gBlankCursor);
		view->UnlockLooper();*/
	} else {
		//set_mouse_position(mResetMouseX, mResetMouseY);
		be_app->Lock();
		be_app->ShowCursor();
		be_app->Unlock();
		/*view->LockLooper();
		view->SetViewCursor(B_CURSOR_SYSTEM_DEFAULT);
		view->UnlockLooper();*/
	}
//		mLastMouseX = mCurMouseX = mLastMouseY = mCurMouseY = -1;
}
	
void BeOSSystemDisplay::getSyncEvent(DisplayEvent &ev)
{
	int32 key;
	int32 raw_char;

	if (!window) return;
	BMessage *event;
	char buffer[4];
	while (1) {
		//view->LockLooper();
		event = view->UnqueueMessage(true);
		//view->UnlockLooper();
		if (!event)
			break;
		switch (event->what) {
/*		//taken care of by the window thread
		case _UPDATE_:
			displayShow();
			break;
*/
		case B_QUIT_REQUESTED:
			//event->PrintToStream();
			ppc_stop();
			break;
		case B_KEY_UP:
		case B_UNMAPPED_KEY_UP:
			if (event->FindInt32("key", &key) < B_OK)
				key = 0;
			if (event->FindInt32("raw_char", &raw_char) < B_OK)
				raw_char = 0;
			if (!key || (key > 255)) break;
			ev.keyEvent.keycode = beos_key_to_adb_key[key-1];
			DPRINTF("keys[%d-1]=%d, 0x%x\n", key, beos_key_to_adb_key[key-1], beos_key_to_adb_key[key-1]);
			if (ev.keyEvent.keycode == KEY_F12) break;
			if ((ev.keyEvent.keycode & 0xff) == 0xff) break;
			ev.type = evKey;
			ev.keyEvent.pressed = false;
			/*XLookupString((XKeyEvent*)&event, buffer, sizeof buffer, &keysym, &compose);
			ev.keyEvent.chr = buffer[0];*/
			ev.keyEvent.chr = (char)raw_char;
			delete event;
			return;
		case B_KEY_DOWN:
		case B_UNMAPPED_KEY_DOWN:
			if (event->FindInt32("key", &key) < B_OK)
				key = 0;
			if (event->FindInt32("raw_char", &raw_char) < B_OK)
				raw_char = 0;
			if (!key || (key > 255)) break;
			ev.keyEvent.keycode = beos_key_to_adb_key[key-1];
			DPRINTF("keys[%d-1]=%d, 0x%x\n", key, beos_key_to_adb_key[key-1], beos_key_to_adb_key[key-1]);
			if (ev.keyEvent.keycode == KEY_F12) {
				clientMouseEnable(!mMouseEnabled);
				break;
			}
			if ((ev.keyEvent.keycode & 0xff) == 0xff) break;
			ev.type = evKey;
			ev.keyEvent.pressed = true;
			/*XLookupString((XKeyEvent*)&event, buffer, sizeof buffer, &keysym, &compose);
			ev.keyEvent.chr = buffer[0];*/
			ev.keyEvent.chr = (char)raw_char;
			delete event;
			return;
		}
		delete event;
	}
}

bool BeOSSystemDisplay::getEvent(DisplayEvent &ev)
{
	if (!window) return false;
	if (!mEventQueue->isEmpty()) {
		Pointer *p = (Pointer *)mEventQueue->deQueue();
		ev = *(DisplayEvent *)p->value;
		free(p->value);
		delete p;
		return true;
	}
	BMessage *event;
	char buffer[4];
	BPoint where;
	uint32 buttons;
	uint32 modifiers;
	int32 transit;
	int32 key;
	int32 raw_char;
	while (1) {
		//view->LockLooper();
		event = view->UnqueueMessage(false);
		//view->UnlockLooper();
		if (!event)
			break;
		switch (event->what) {
/*		//taken care of by the window thread
		case _UPDATE_:
			displayShow();
			break;
*/
		case B_QUIT_REQUESTED:
			//event->PrintToStream();
			ppc_stop();
			break;
		case B_KEY_UP:
		case B_UNMAPPED_KEY_UP:
			if (event->FindInt32("key", &key) < B_OK)
				key = 0;
			if (event->FindInt32("raw_char", &raw_char) < B_OK)
				raw_char = 0;
			if (!key || (key > 255)) break;
			ev.keyEvent.keycode = beos_key_to_adb_key[key-1];
			DPRINTF("keys[%d-1]=%d, 0x%x\n", key, beos_key_to_adb_key[key-1], beos_key_to_adb_key[key-1]);
			if (ev.keyEvent.keycode == KEY_F12) break;
			if ((ev.keyEvent.keycode & 0xff) == 0xff) break;
			ev.type = evKey;
			ev.keyEvent.pressed = false;
			/*XLookupString((XKeyEvent*)&event, buffer, sizeof buffer, &keysym, &compose);
			ev.keyEvent.chr = buffer[0];*/
			ev.keyEvent.chr = (char)raw_char;
			delete event;
			return true;
		case B_KEY_DOWN:
		case B_UNMAPPED_KEY_DOWN:
			if (event->FindInt32("key", &key) < B_OK)
				key = 0;
			if (event->FindInt32("raw_char", &raw_char) < B_OK)
				raw_char = 0;
			if (!key || (key > 255)) break;
			ev.keyEvent.keycode = beos_key_to_adb_key[key-1];
			DPRINTF("keys[%d-1]=%d, 0x%x\n", key, beos_key_to_adb_key[key-1], beos_key_to_adb_key[key-1]);
			if (ev.keyEvent.keycode == KEY_F12 && mCurMouseX != -1) {
				clientMouseEnable(!mMouseEnabled);
				break;
			}
			if ((ev.keyEvent.keycode & 0xff) == 0xff) break;
			ev.type = evKey;
			ev.keyEvent.pressed = true;
			/*XLookupString((XKeyEvent*)&event, buffer, sizeof buffer, &keysym, &compose);
			ev.keyEvent.chr = buffer[0];*/
			ev.keyEvent.chr = (char)raw_char;
			delete event;
			return true;
		case B_MOUSE_DOWN:
			if (!mMouseEnabled) break;
			if (event->FindPoint("where", &where) >= B_OK) {
				mCurMouseX = ev.mouseEvent.x = (int)where.x;
				mCurMouseY = ev.mouseEvent.y = (int)where.y;
			}
			if (event->FindInt32("buttons", (int32 *)&buttons) < B_OK)
				buttons = 0;
			ev.type = evMouse;
			ev.mouseEvent.button1 = (buttons & B_PRIMARY_MOUSE_BUTTON) != 0;
			ev.mouseEvent.button2 = (buttons & B_SECONDARY_MOUSE_BUTTON) != 0;
			ev.mouseEvent.button3 = (buttons & B_TERTIARY_MOUSE_BUTTON) != 0;
			ev.mouseEvent.x = mCurMouseX;
			ev.mouseEvent.y = mCurMouseY;
			ev.mouseEvent.relx = 0;
			ev.mouseEvent.rely = 0;
			delete event;
			return true;
		case B_MOUSE_UP: 
			if (event->FindPoint("where", &where) >= B_OK) {
				mCurMouseX = ev.mouseEvent.x = (int)where.x;
				mCurMouseY = ev.mouseEvent.y = (int)where.y;
			}
			if (event->FindInt32("buttons", (int32 *)&buttons) < B_OK)
				buttons = 0;
			if (!mMouseEnabled) {
				if (mCurMouseY < mMenuHeight) {
					
					clickMenu(mCurMouseX, mCurMouseY);
					
				}
			} else {
				ev.type = evMouse;
				ev.mouseEvent.button1 = (buttons & B_PRIMARY_MOUSE_BUTTON) != 0;
				ev.mouseEvent.button2 = (buttons & B_SECONDARY_MOUSE_BUTTON) != 0;
				ev.mouseEvent.button3 = (buttons & B_TERTIARY_MOUSE_BUTTON) != 0;
				ev.mouseEvent.x = mCurMouseX;
				ev.mouseEvent.y = mCurMouseY;
				ev.mouseEvent.relx = 0;
				ev.mouseEvent.rely = 0;
				delete event;
				return true;
			}
			break;
		case B_MOUSE_MOVED:
			if (event->FindPoint("where", &where) < B_OK)
				where = BPoint(0,0);
			if (event->FindInt32("buttons", (int32 *)&buttons) < B_OK)
				buttons = 0;
			if (event->FindInt32("be:transit", &transit) < B_OK)
				transit = B_INSIDE_VIEW;
			mCurMouseX = ev.mouseEvent.x = (int)where.x;
			mCurMouseY = ev.mouseEvent.y = (int)where.y;
			if (mCurMouseX == mHomeMouseX && mCurMouseY == mHomeMouseY) break;
			if (!mMouseEnabled) break;
			mLastMouseX = mCurMouseX;
			mLastMouseY = mCurMouseY;
			if (mLastMouseX == -1) break;
			ev.type = evMouse;
			ev.mouseEvent.button1 = (buttons & B_PRIMARY_MOUSE_BUTTON) != 0;
			ev.mouseEvent.button2 = (buttons & B_SECONDARY_MOUSE_BUTTON) != 0;
			ev.mouseEvent.button3 = (buttons & B_TERTIARY_MOUSE_BUTTON) != 0;
			ev.mouseEvent.relx = mCurMouseX - mHomeMouseX;
			ev.mouseEvent.rely = mCurMouseY - mHomeMouseY;
			//XWarpPointer(gXDisplay, gXWindow, gXWindow, 0, 0, 0, 0, mHomeMouseX, mHomeMouseY);
			if (transit == B_EXITED_VIEW) // == LeaveNotify
				mLastMouseX = mCurMouseX = mLastMouseY = mCurMouseY = -1;
			
			delete event;
			return true;
#if 0
		case EnterNotify:
			mLastMouseX = mCurMouseX = ((XEnterWindowEvent *)&event)->x;
			mLastMouseY = mCurMouseY = ((XEnterWindowEvent *)&event)->y;
				break;
		case LeaveNotify:
			mLastMouseX = mCurMouseX = mLastMouseY = mCurMouseY = -1;
			break;
#endif
		}
		delete event;
	}
	return false;
}

void BeOSSystemDisplay::displayShow()
{
	view->LockLooper();
	//view->Invalidate(view->Bounds());
	view->Draw(view->Bounds());
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

void BeOSSystemDisplay::convertDisplayClientToServer()
{
	if (!beosframebuffer) return;	// great! nothing to do.
	byte *buf = gFramebuffer;
	byte *xbuf = beosframebuffer;
/*	if ((mClientChar.bytesPerPixel == 2) && (mBeChar.bytesPerPixel == 2)) {
		posix_vaccel_15_to_15(mClientChar.height*mClientChar.width, buf, xbuf);
	} else if ((mClientChar.bytesPerPixel == 2) && (mBeChar.bytesPerPixel == 4)) {
		posix_vaccel_15_to_32(mClientChar.height*mClientChar.width, buf, xbuf);
	} else */
	for (int y=0; y < mClientChar.height; y++) {
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
					xbuf[0] = p;
					break;
				case 2:
					xbuf[1] = p>>8; xbuf[0] = p;
					break;
				case 4:
					*(uint32*)xbuf = p;
					break;
			}
			xbuf += mBeChar.bytesPerPixel;
			buf += mClientChar.bytesPerPixel;
		}
	}
}

void BeOSSystemDisplay::queueEvent(DisplayEvent &ev)
{
	DisplayEvent *e = (DisplayEvent *)malloc(sizeof (DisplayEvent));
	*e = ev;
	mEventQueue->enQueue(new Pointer(e));
}

void *BeOSSystemDisplay::eventLoop(void *p)
{
}

void BeOSSystemDisplay::startRedrawThread(int msec)
{
	window->Lock();
	window->SetPulseRate(msec*1000);
	window->Unlock();
}

SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr)
{
	if (gDisplay) return gDisplay;
	return new BeOSSystemDisplay(title, chr);
}

