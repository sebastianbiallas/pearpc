/* 
 *	PearPC
 *	sysdisplay.cc - screen access functions for Win32
 *
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

#include "system/display.h"
#include "system/sysexcept.h"
#include "tools/snprintf.h"

// for stopping the CPU
#include "cpu_generic/ppc_cpu.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <process.h>

#undef FASTCALL
#include "system/types.h"

byte *gFramebuffer = NULL;
uint gDamageAreaFirstAddr, gDamageAreaLastAddr;

static HINSTANCE gHInst;
static HWND gHWNDMain;
static byte *winframebuffer = NULL;
static int gWidth;
static int gHeight; 
static int gMenuHeight; 
static HBITMAP gMemoryBitmap = 0;
static unsigned long gWorkerThread = 0;
static DWORD gWorkerThreadID = 0;
static bool gSkipUpdate = false;
static BITMAPINFO gBitmapInfo;
static BITMAPINFO gMenuBitmapInfo;
static int gShiftDown = 0;
static byte *menuData;

struct {
	uint64 r_mask;
	uint64 g_mask;
	uint64 b_mask;
} PACKED gWin32RGBMask;

static bool mMouseEnabled;
static int mLastMouseX, mLastMouseY;
static int mCurMouseX, mCurMouseY;
static int mResetMouseX, mResetMouseY;
static int mHomeMouseX, mHomeMouseY;

static CRITICAL_SECTION gDrawCS;
static CRITICAL_SECTION gEventCS;

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static Queue *gEventQueue;
static DisplayCharacteristics mWinChar;

void displaythread(void *pvoid);
static VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime);

extern "C" void __attribute__((regparm (3))) win32_vaccel_15_to_15(uint32 pixel, byte *input, byte *output);
extern "C" void __attribute__((regparm (3))) win32_vaccel_15_to_16(uint32 pixel, byte *input, byte *output);
extern "C" void __attribute__((regparm (3))) win32_vaccel_15_to_32(uint32 pixel, byte *input, byte *output);
extern "C" void __attribute__((regparm (3))) win32_vaccel_32_to_32(uint32 pixel, byte *input, byte *output);

typedef void __attribute__((regparm (3))) (*win32_vaccel_func_t)(uint32 pixel, byte *input, byte *output);
static win32_vaccel_func_t win32_vaccel_func;

/*
 *
 */
class Win32Display: public SystemDisplay
{
	char mCurTitle[200];
	char *mTitle;
public:

	Win32Display(const char *name, const DisplayCharacteristics &chr)
		:SystemDisplay(chr)
	{
		mWinChar = chr;
		HWND dw = GetDesktopWindow();
		HDC ddc = GetDC(dw);
		mWinChar.bytesPerPixel = (GetDeviceCaps(ddc, BITSPIXEL)+7)/8;
		ReleaseDC(dw, ddc);
		mTitle = strdup(name);
		mMenuHeight = 28;
		gMenuHeight = mMenuHeight;

		gHInst = GetModuleHandle(NULL);

		gFramebuffer = (byte*)realloc(gFramebuffer, mClientChar.width 
			* mClientChar.height * mClientChar.bytesPerPixel);
		winframebuffer = (byte*)realloc(winframebuffer, mWinChar.width 
			* mWinChar.height * mWinChar.bytesPerPixel);
		memset(gFramebuffer, 0, mClientChar.width 
			* mClientChar.height * mClientChar.bytesPerPixel);
		gEventQueue = new Queue(true);
		gWidth = mWinChar.width;
		gHeight = mWinChar.height;
		damageFrameBufferAll();

		selectVaccel();
		mHomeMouseX = mWinChar.width/2;
		mHomeMouseY = mWinChar.height/2;
		InitializeCriticalSection(&gDrawCS);
		InitializeCriticalSection(&gEventCS);

		gWorkerThread = _beginthread(displaythread, 0, this);

		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
	}

	~Win32Display()
	{
		if (gMemoryBitmap) DeleteObject(gMemoryBitmap);

		DeleteCriticalSection(&gDrawCS);
		DeleteCriticalSection(&gEventCS);

		delete gEventQueue;
		free(gFramebuffer);
		free(winframebuffer);
	}

	bool changeResolution(const DisplayCharacteristics &aCharacteristics)
	{
		/*
		 * get size of desktop first
		 * (windows doesn't allow windows greater than the desktop)
		 */
		HWND dw = GetDesktopWindow();
		RECT desktoprect;
		GetWindowRect(dw, &desktoprect);
		if (aCharacteristics.width > (desktoprect.right-desktoprect.left)
		|| aCharacteristics.height > (desktoprect.bottom-desktoprect.top)) {
			// protect user from himself
			return false;
		}		

		EnterCriticalSection(&gDrawCS);
		mClientChar = aCharacteristics;
		mWinChar.height = mClientChar.height;
		mWinChar.width = mClientChar.width;

		gFramebuffer = (byte*)realloc(gFramebuffer, mClientChar.width 
			* mClientChar.height * mClientChar.bytesPerPixel);
		winframebuffer = (byte*)realloc(winframebuffer, mWinChar.width 
			* mWinChar.height * mWinChar.bytesPerPixel);
		memset(gFramebuffer, 0, mClientChar.width 
			* mClientChar.height * mClientChar.bytesPerPixel);

		gHeight = mWinChar.height;
		gWidth = mWinChar.width;
		mHomeMouseX = mWinChar.width/2;
		mHomeMouseY = mWinChar.height/2;
		createBitmap();
		selectVaccel();
		LeaveCriticalSection(&gDrawCS);

		RECT rect;
		RECT rect2;
		GetWindowRect(gHWNDMain, &rect);
		GetWindowRect(gHWNDMain, &rect2);
		rect.right = rect.left+mWinChar.width;
		rect.bottom = rect.top+mWinChar.height+gMenuHeight;
		AdjustWindowRect(&rect, WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
			| WS_CAPTION | WS_BORDER | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
		MoveWindow(gHWNDMain, rect2.left, rect2.top, rect.right-rect.left, rect.bottom-rect.top, FALSE);

		damageFrameBufferAll();
		return true;
	}

	virtual	int toString(char *buf, int buflen) const
	{
		return snprintf(buf, buflen, "Win32");
	}

	void selectVaccel()
	{
		win32_vaccel_func = NULL;
		switch (mWinChar.bytesPerPixel) {
		case 1:
			ht_printf("nyi: %s::%d", __FILE__, __LINE__);
			exit(-1);
			break;
		case 2:
			mWinChar.redShift = 10;
			mWinChar.redSize = 5;
			mWinChar.greenShift = 5;
			mWinChar.greenSize = 5;
			mWinChar.blueShift = 0;
			mWinChar.blueSize = 5;
			switch (mClientChar.bytesPerPixel) {
			case 2: win32_vaccel_func = win32_vaccel_15_to_15; break;
			break;
			}
			break;
		case 4:
			mWinChar.redShift = 16;
			mWinChar.redSize = 8;
			mWinChar.greenShift = 8;
			mWinChar.greenSize = 8;
			mWinChar.blueShift = 0;
			mWinChar.blueSize = 8;
			switch (mClientChar.bytesPerPixel) {
			case 2: win32_vaccel_func = win32_vaccel_15_to_32; break;
			case 4: win32_vaccel_func = win32_vaccel_32_to_32; break;
			break;
			}
		}
		switch (mClientChar.bytesPerPixel) {
		case 1:
			/* ht_printf("nyi: %s::%d", __FILE__, __LINE__);
			exit(-1); */
			break;
		case 2:
			gWin32RGBMask.r_mask = 0x7c007c007c007c00ULL;
			gWin32RGBMask.g_mask = 0x03e003e003e003e0ULL;
			gWin32RGBMask.b_mask = 0x001f001f001f001fULL;
			break;
		case 4:
			gWin32RGBMask.r_mask = 0x00ff000000ff0000ULL;
			gWin32RGBMask.g_mask = 0x0000ff000000ff00ULL;
			gWin32RGBMask.b_mask = 0x000000ff000000ffULL;
			break;
		}
	}

	virtual	void finishMenu()
	{
		menuData = (byte*)malloc(mWinChar.width *
			mMenuHeight * mWinChar.bytesPerPixel);
		gMenuBitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		gMenuBitmapInfo.bmiHeader.biWidth = mWinChar.width;
		gMenuBitmapInfo.bmiHeader.biHeight = -mMenuHeight;
		gMenuBitmapInfo.bmiHeader.biPlanes = 1;
		gMenuBitmapInfo.bmiHeader.biBitCount = 8*mWinChar.bytesPerPixel;
		gMenuBitmapInfo.bmiHeader.biCompression = BI_RGB;
		gMenuBitmapInfo.bmiHeader.biSizeImage = 
			mWinChar.width * mMenuHeight
			* mWinChar.bytesPerPixel;
		gMenuBitmapInfo.bmiHeader.biXPelsPerMeter = 4500;
		gMenuBitmapInfo.bmiHeader.biYPelsPerMeter = 4500;
		gMenuBitmapInfo.bmiHeader.biClrUsed = 0;
		gMenuBitmapInfo.bmiHeader.biClrImportant = 0;

		// Is this ugly? Yes!
		fillRGB(0, 0, mClientChar.width, mClientChar.height, MK_RGB(0xff, 0xff, 0xff));
		drawMenu();
		displayConvert(0, mClientChar.height-1);
		memmove(menuData, winframebuffer, mWinChar.width * mMenuHeight
			* mWinChar.bytesPerPixel);
		InvalidateRect(gHWNDMain, NULL, FALSE);
	}

	void updateTitle()
	{
		ht_snprintf(mCurTitle, sizeof mCurTitle, "%s - [F12 %s mouse]", mTitle, mMouseEnabled ? "disables" : "enables");
		SetWindowText(gHWNDMain, mCurTitle);
	}

	void clientMouseEnable(bool enable)
	{
		mMouseEnabled = enable;
		updateTitle();
		if (enable) {
			mResetMouseX = mCurMouseX;
			mResetMouseY = mCurMouseY;

			ShowCursor(FALSE);
			RECT wndRect;
			GetWindowRect(gHWNDMain, &wndRect);
			SetCursorPos(wndRect.left + mHomeMouseX + GetSystemMetrics(SM_CXFIXEDFRAME), 
				wndRect.top + mHomeMouseY + GetSystemMetrics(SM_CYFIXEDFRAME)
				+ GetSystemMetrics(SM_CYCAPTION));
		} else {
			RECT wndRect;
			GetWindowRect(gHWNDMain, &wndRect);
			SetCursorPos(wndRect.left + mResetMouseX + GetSystemMetrics(SM_CXFIXEDFRAME), 
				wndRect.top + mResetMouseY + GetSystemMetrics(SM_CYFIXEDFRAME)
				+ GetSystemMetrics(SM_CYCAPTION));
			ShowCursor(TRUE);
		}
	}

	virtual bool getEvent(DisplayEvent &ev)
	{
		EnterCriticalSection(&gEventCS);
		if (!gEventQueue->isEmpty()) {
			Pointer *p = (Pointer*)gEventQueue->deQueue();
			LeaveCriticalSection(&gEventCS);
			ev = *(DisplayEvent *)p->value;
			free(p->value);
			delete p;
			return true;
		}
		LeaveCriticalSection(&gEventCS);
		return false;
	}

	virtual void getSyncEvent(DisplayEvent &ev)
	{
		// FIXME:
		while (!getEvent(ev)) Sleep(0);
	}

	virtual void queueEvent(DisplayEvent &ev)
	{
		DisplayEvent *e = (DisplayEvent *)malloc(sizeof (DisplayEvent));
		*e = ev;
		gEventQueue->enQueue(new Pointer(e));
	}

	void displayConvert(uint firstLine, uint lastLine)
	{
		byte *buf = gFramebuffer;
		byte *xbuf = winframebuffer;
		if (win32_vaccel_func) {
			// FIXME: take advantage of first/lastLine
			win32_vaccel_func(mClientChar.height*mClientChar.width, buf, xbuf);
		} else {
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
					convertBaseColor(r, mClientChar.redSize, mWinChar.redSize);
					convertBaseColor(g, mClientChar.greenSize, mWinChar.greenSize);
					convertBaseColor(b, mClientChar.blueSize, mWinChar.blueSize);
					p = (r << mWinChar.redShift) | (g << mWinChar.greenShift)
						| (b << mWinChar.blueShift);
					switch (mWinChar.bytesPerPixel) {
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
					xbuf += mWinChar.bytesPerPixel;
					buf += mClientChar.bytesPerPixel;			
				}
			}
		}	
	}

	virtual void displayShow()
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

		// we enter the critical section early, so that
		// changeResolution can't conflict here
		EnterCriticalSection(&gDrawCS);

		damageAreaLastAddr += 3;	// this is a hack. For speed reasons we
						// inaccurately set gDamageAreaLastAddr
						// to the first (not last) byte accessed
						// accesses are up to 4 bytes "long".

		firstDamagedLine = damageAreaFirstAddr / (mClientChar.width * mClientChar.bytesPerPixel);
		lastDamagedLine = damageAreaLastAddr / (mClientChar.width * mClientChar.bytesPerPixel);
		// Overflow may happen, because of the hack used above
		// and others, that set lastAddr = 0xfffffff0
		if (lastDamagedLine >= mClientChar.height) {
			lastDamagedLine = mClientChar.height-1;
		}

		displayConvert(firstDamagedLine, lastDamagedLine);

		HDC hdc = GetDC(gHWNDMain);

		SetDIBitsToDevice(
			hdc,
			0,
			gMenuHeight+firstDamagedLine,
			gWidth,
			lastDamagedLine-firstDamagedLine+1,  // number of lines to draw
			0,
			gHeight-lastDamagedLine-1, // line src-position (0,0 = lower left)
			0,
        		gHeight,
			winframebuffer, &gBitmapInfo, DIB_RGB_COLORS);

		ReleaseDC(gHWNDMain, hdc);
		/*RECT updated_area;
		updated_area.left = 0;
		updated_area.right = gWidth;
		updated_area.top = gMenuHeight;
		updated_area.bottom = gMenuHeight+gHeight;*/
		// Dont Invalidate or WM_PAINT would be called....
		//InvalidateRect(gHWNDMain, &updated_area, FALSE);

		LeaveCriticalSection(&gDrawCS);
	}

	virtual	void startRedrawThread(int msec)
	{
		SetTimer(gHWNDMain, 0, msec, TimerProc);
	}

	void createBitmap()
	{
		if (gMemoryBitmap) DeleteObject(gMemoryBitmap);
		HDC hdc = GetDC(gHWNDMain);
		gMemoryBitmap = CreateCompatibleBitmap(hdc, mWinChar.width, mWinChar.height);
		ReleaseDC(gHWNDMain, hdc);

		gBitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		gBitmapInfo.bmiHeader.biWidth = mWinChar.width;
		// Height is negative for top-down bitmap
		gBitmapInfo.bmiHeader.biHeight = -mWinChar.height;
		gBitmapInfo.bmiHeader.biPlanes = 1;
		gBitmapInfo.bmiHeader.biBitCount = 8*mWinChar.bytesPerPixel;
		gBitmapInfo.bmiHeader.biCompression = BI_RGB;
		gBitmapInfo.bmiHeader.biSizeImage = 
			mWinChar.width * mWinChar.height 
			* mWinChar.bytesPerPixel;
		gBitmapInfo.bmiHeader.biXPelsPerMeter = 4500;
		gBitmapInfo.bmiHeader.biYPelsPerMeter = 4500;
		gBitmapInfo.bmiHeader.biClrUsed = 0;
		gBitmapInfo.bmiHeader.biClrImportant = 0;
	}
};

static bool needUpdateDisplay()
{
	if (!gSkipUpdate) {
 		RECT rect;
 		HDC hdc = GetDC(gHWNDMain);
 		int gcb = GetClipBox(hdc, &rect);
 		ReleaseDC(gHWNDMain, hdc);
 		if (gcb != NULLREGION) {
 			return true;
		}
	}
	return false;
}

static VOID CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
	if (needUpdateDisplay()) gDisplay->displayShow();
}

/*
 *	This is the thread doing the display
 *	and event handling stuff
 */
void displaythread(void *pvoid) 
{
	gWorkerThreadID = GetCurrentThreadId();
	Win32Display *display = (Win32Display *)pvoid;

	WNDCLASS wc;

	memset(&wc,0,sizeof wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = (WNDPROC)MainWndProc;
	wc.hInstance = gHInst;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = "ClassClass";
	wc.lpszMenuName = 0;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClass(&wc);

	RECT rect;
	rect.top = 0; rect.left = 0;
	rect.bottom = mWinChar.height+gMenuHeight;
	rect.right = mWinChar.width;
	AdjustWindowRect(&rect, WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
		| WS_CAPTION | WS_BORDER | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

	gHWNDMain = CreateWindow("ClassClass", "PearPC",
		WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
		| WS_CAPTION | WS_BORDER | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 
		rect.right-rect.left, rect.bottom-rect.top,
		NULL, NULL, gHInst, NULL);

	display->updateTitle();

	display->createBitmap();

	display->displayShow();
	ShowWindow(gHWNDMain, SW_SHOW);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	ppc_stop();

	_endthread();
}


void MainWndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	switch(id) {
/*	case IDM_EXIT:
		PostMessage(hwnd, WM_CLOSE, 0, 0);
		break;*/
	}
}

static byte ascii_to_scancode[] = {
};

static byte scancode_to_ascii[] = {
//00   01   02   03   04   05   06   07   08   09   0a   0b   0c   0d   0e   0f
0x00,'\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',0x08,'\t',
 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']','\n',0x00, 'a', 's',
 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',0x00,0x00,0x00,'\\', 'z', 'x', 'c', 'v', 
 'b', 'n', 'm', ',', '.', '/',0x00,0x00,0x00, ' ',0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

//00   01   02   03   04   05   06   07   08   09   0a   0b   0c   0d   0e   0f
0x00,'\e', '1', '2', '3', '4', '5', '6', '7', '*', '(', ')', '_', '+',0x08,'\t',
 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']','\n',0x00, 'A', 'S',
 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',0x00,0x00,0x00,'\\', 'Z', 'X', 'C', 'V', 
 'B', 'N', 'M', '<', '>', '?',0x00,0x00,0x00, ' ',0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

byte scancode_to_mackey[] = {
//00   01   02   03   04   05   06   07   08   09   0a   0b   0c   0d   0e   0f
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
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
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

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_PAINT: 
	{
		EnterCriticalSection(&gDrawCS);
		PAINTSTRUCT ps;
		
		HDC hdc = BeginPaint(hwnd, &ps);
			
			
		SetDIBitsToDevice(hdc, 0, 0, gWidth, gMenuHeight, 0, 0,
			0, gMenuHeight, menuData, &gMenuBitmapInfo, DIB_RGB_COLORS);
		SetDIBitsToDevice(hdc, 0, gMenuHeight, gWidth, gHeight, 0, 0,
			0, gHeight, winframebuffer, &gBitmapInfo, DIB_RGB_COLORS);
		EndPaint(hwnd, &ps); 		
			
 		LeaveCriticalSection(&gDrawCS);
		break;
	}
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		// This tests if the key is really pressed
		// or if it is only a repeated event
		if (!(lParam & (1<<30))) {
			if ((wParam == VK_F12) && gDisplay->getCatchMouseToggle()) {
				((Win32Display*)gDisplay)->clientMouseEnable(!mMouseEnabled);
			} else {
				int scancode = HIWORD(lParam) & 0x01FF;
				DisplayEvent *ev = (DisplayEvent *)malloc(sizeof (DisplayEvent));
				ev->type = evKey;
				ev->keyEvent.pressed = true;

				int chr = 0;
				if (scancode < 128) {
					chr = scancode_to_ascii[scancode + gShiftDown*128];
				}
				if (scancode == 42 || scancode == 54) gShiftDown = 1;
				EnterCriticalSection(&gEventCS);
				if (scancode == 0x138) {
					// altgr == ctrl+alt --> release ctrl, press alt
					ev->keyEvent.chr = 0;
					ev->keyEvent.keycode = scancode_to_mackey[0x1d] | 0x80000000;
					ev->keyEvent.pressed = false;
					gEventQueue->enQueue(new Pointer(ev)); 
					ev = (DisplayEvent *)malloc(sizeof (DisplayEvent));
					ev->type = evKey;
					ev->keyEvent.pressed = true;
					ev->keyEvent.chr = 0;
					ev->keyEvent.keycode = scancode_to_mackey[0x138];
					gEventQueue->enQueue(new Pointer(ev));
				} else {
					ev->keyEvent.keycode = scancode_to_mackey[scancode];
					if ((ev->keyEvent.keycode & 0xff) != 0xff) {
						ev->keyEvent.chr = chr;
						gEventQueue->enQueue(new Pointer(ev));
					} else {
						free(ev);
					}
				}
				LeaveCriticalSection(&gEventCS);
			}
		}
		break;
	case WM_KEYUP:
	case WM_SYSKEYUP: {
		if (wParam != VK_F12) {
			int scancode = HIWORD(lParam) & 0x01FF;
			DisplayEvent *ev = (DisplayEvent *)malloc(sizeof (DisplayEvent));
			ev->type = evKey;
			ev->keyEvent.pressed = false;

			if (scancode == 42 || scancode == 54) gShiftDown = 0;

			ev->keyEvent.keycode = scancode_to_mackey[scancode] | 0x80000000;

			if ((ev->keyEvent.keycode & 0xff) != 0xff) {
				EnterCriticalSection(&gEventCS);
				gEventQueue->enQueue(new Pointer(ev));
				LeaveCriticalSection(&gEventCS);
			} else {
				free(ev);
			}
		}
		break;
	}
	case WM_CHAR:
  	case WM_DEADCHAR:
	case WM_SYSCHAR:
	case WM_SYSDEADCHAR:
		break;

	case WM_LBUTTONUP:
		if (!mMouseEnabled) {
			if (HIWORD(lParam) < gMenuHeight) {
				gDisplay->clickMenu(LOWORD(lParam), HIWORD(lParam));
			}
		}
		// fall throu
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
	case WM_RBUTTONUP:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_MOUSEMOVE: {
		mCurMouseX = LOWORD(lParam);
		mCurMouseY = HIWORD(lParam);
		if (!mMouseEnabled) break;
		if (msg == WM_MOUSEMOVE) {
			if (mCurMouseX == mHomeMouseX && mCurMouseY == mHomeMouseY) break;
		}
		mLastMouseX = mCurMouseX;
		mLastMouseY = mCurMouseY;
		DisplayEvent *ev = (DisplayEvent *)malloc(sizeof (DisplayEvent));
		ev->type = evMouse;
		ev->mouseEvent.button1 = wParam & MK_LBUTTON;
		ev->mouseEvent.button2 = wParam & MK_MBUTTON;
		ev->mouseEvent.button3 = wParam & MK_RBUTTON;
		ev->mouseEvent.relx = mCurMouseX - mHomeMouseX;
		ev->mouseEvent.rely = mCurMouseY - mHomeMouseY;

		EnterCriticalSection(&gEventCS);
		gEventQueue->enQueue(new Pointer(ev));
		LeaveCriticalSection(&gEventCS);

		RECT wndRect;
		GetWindowRect(hwnd, &wndRect);
		SetCursorPos(wndRect.left + mHomeMouseX + GetSystemMetrics(SM_CXFIXEDFRAME), 
				wndRect.top + mHomeMouseY + GetSystemMetrics(SM_CYFIXEDFRAME)
				+ GetSystemMetrics(SM_CYCAPTION));
		break;
	}
	case WM_SIZE:
		gSkipUpdate = (wParam == SIZE_MINIMIZED);
		break;
	case WM_COMMAND:
		MainWndProc_OnCommand(hwnd, (int)(LOWORD(wParam)), (HWND)lParam, (UINT)HIWORD(wParam));
		break;     
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}

SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr)
{
	if (gDisplay) return gDisplay;
	return new Win32Display(title, chr);
}
