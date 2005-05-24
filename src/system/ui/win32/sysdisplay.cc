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

#include "system/sysvaccel.h"
#include "system/display.h"
#include "system/sysexcept.h"
#include "tools/snprintf.h"
#include "configparser.h"

#undef FASTCALL

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <process.h>

#undef FASTCALL
#include "system/types.h"
#include "syswin.h"

static byte *winframebuffer = NULL;
static HBITMAP gMemoryBitmap = 0;
static BITMAPINFO gBitmapInfo;

Win32Display::Win32Display(const char *name, const DisplayCharacteristics &chr, int redraw_ms)
	:SystemDisplay(chr, redraw_ms)
{
	mClientChar = chr;
	convertCharacteristicsToHost(mWinChar, mClientChar);
	mTitle = strdup(name);
	mMenuHeight = 28;
	gMenuHeight = mMenuHeight;

	gFrameBuffer = (byte*)realloc(gFrameBuffer, mClientChar.width 
		* mClientChar.height * mClientChar.bytesPerPixel);
	winframebuffer = (byte*)realloc(winframebuffer, mWinChar.width 
		* mWinChar.height * mWinChar.bytesPerPixel);
	memset(gFrameBuffer, 0, mClientChar.width 
		* mClientChar.height * mClientChar.bytesPerPixel);
	damageFrameBufferAll();

	mHomeMouseX = mWinChar.width/2;
	mHomeMouseY = mWinChar.height/2;
	InitializeCriticalSection(&gDrawCS);
	
	mInvisibleCursor = NULL;	
}

Win32Display::~Win32Display()
{
	if (gMemoryBitmap) DeleteObject(gMemoryBitmap);

	DeleteCriticalSection(&gDrawCS);

	free(gFrameBuffer);
	free(winframebuffer);

	if (mInvisibleCursor) DestroyCursor(mInvisibleCursor);
}

void Win32Display::getHostCharacteristics(Container &modes)
{
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
}

void Win32Display::convertCharacteristicsToHost(DisplayCharacteristics &aHostChar, const DisplayCharacteristics &aClientChar)
{
	aHostChar = aClientChar;
	if (!mFullscreen) {
		HWND dw = GetDesktopWindow();
		HDC ddc = GetDC(dw);
		aHostChar.bytesPerPixel = (GetDeviceCaps(ddc, BITSPIXEL)+7)/8;
		aHostChar.scanLineLength = aHostChar.bytesPerPixel * aHostChar.width;
		ReleaseDC(dw, ddc);
	}
	switch (aHostChar.bytesPerPixel) {
	case 2: 
		aHostChar.redShift = 10;
		aHostChar.redSize = 5;
		aHostChar.greenShift = 5;
		aHostChar.greenSize = 5;
		aHostChar.blueShift = 0;
		aHostChar.blueSize = 5;
		break;
	case 4:
		aHostChar.redShift = 16;
		aHostChar.redSize = 8;
		aHostChar.greenShift = 8;
		aHostChar.greenSize = 8;
		aHostChar.blueShift = 0;
		aHostChar.blueSize = 8;
		break;
	}
}

bool Win32Display::changeResolution(const DisplayCharacteristics &aClientChar)
{
	if (mFullscreen) {
		EnterCriticalSection(&gDrawCS);
		DisplayCharacteristics oldhost = mWinChar;
		DisplayCharacteristics oldclient = mClientChar;
		mClientChar = aClientChar;
		convertCharacteristicsToHost(mWinChar, mClientChar);

		DEVMODE dm;
		dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
//		dm.dmBitsPerPel = (mWinChar.bytesPerPixel == 2) ? 15 : 32; // GRRRR
		dm.dmBitsPerPel = (mWinChar.bytesPerPixel == 2) ? 16 : 32;
		dm.dmPelsWidth = mWinChar.width;
		dm.dmPelsHeight = mWinChar.height;
		ht_printf("*** %d **\n", mWinChar.vsyncFrequency);
		dm.dmDisplayFrequency = mWinChar.vsyncFrequency;
		LONG err = ChangeDisplaySettings(&dm, CDS_TEST);
		if (err != DISP_CHANGE_SUCCESSFUL) {
			if (mWinChar.bytesPerPixel == 2) {
				dm.dmBitsPerPel = 16;
				err = ChangeDisplaySettings(&dm, CDS_TEST);
			}
		}

		if (err != DISP_CHANGE_SUCCESSFUL) {
			mWinChar = oldhost;
			mClientChar = oldclient;
			LeaveCriticalSection(&gDrawCS);

			/*
			 * FIXME: maybe we should switch back to windowed mode
			 *        here if running in fullscreen mode.
			 */

			if (mFullscreenChanged) {
				// do something
			}

			return false;
		}
		ChangeDisplaySettings(&dm, CDS_FULLSCREEN);
		gMenuHeight = 0;
		gFrameBuffer = (byte*)realloc(gFrameBuffer, mClientChar.width 
			* mClientChar.height * mClientChar.bytesPerPixel);
		winframebuffer = (byte*)realloc(winframebuffer, mWinChar.width 
			* mWinChar.height * mWinChar.bytesPerPixel);
		createBitmap();
		damageFrameBufferAll();
		mFullscreenChanged = true;
		LeaveCriticalSection(&gDrawCS);
		SetWindowLong(gHWNDMain, GWL_STYLE, WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_SYSMENU | WS_OVERLAPPED);
		SetWindowPos(gHWNDMain, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		SetWindowPos(gHWNDMain, HWND_TOPMOST, 0, 0, mWinChar.width, mWinChar.height, 0);
		ShowWindow(gHWNDMain, SW_SHOWMAXIMIZED);
		setMouseGrab(true);
		return true;
	} else {
		EnterCriticalSection(&gDrawCS);
		if (mFullscreenChanged) {
			// switch out of fullscreen mode
			gMenuHeight = 28;

			setMouseGrab(false);
			ChangeDisplaySettings(NULL, 0);
		}

		/*
		 * get size of desktop first
		 * (windows doesn't allow windows greater than the desktop)
		 */
		HWND dw = GetDesktopWindow();
		RECT desktoprect;
		GetWindowRect(dw, &desktoprect);
		if (aClientChar.width > (desktoprect.right-desktoprect.left)
		|| aClientChar.height > (desktoprect.bottom-desktoprect.top)) {
			/*
			 * protect user from himself
			 * What shall we do if switching out of full-
			 * screen mode does not work?
			 */
			if (mFullscreenChanged) {
				// FIXME: insert clever code here
			} else {
				return false;
			}
		}
		mFullscreenChanged = false;
		mClientChar = aClientChar;
		convertCharacteristicsToHost(mWinChar, mClientChar);
		gFrameBuffer = (byte*)realloc(gFrameBuffer, mClientChar.width 
			* mClientChar.height * mClientChar.bytesPerPixel);
		winframebuffer = (byte*)realloc(winframebuffer, mWinChar.width 
			* mWinChar.height * mWinChar.bytesPerPixel);

		mHomeMouseX = mWinChar.width/2;
		mHomeMouseY = mWinChar.height/2;
		createBitmap();
		SetWindowLong(gHWNDMain, GWL_STYLE, WS_VISIBLE | WS_SYSMENU | WS_CAPTION | WS_BORDER | WS_MINIMIZEBOX);
		SetWindowPos(gHWNDMain, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		SetWindowPos(gHWNDMain, HWND_TOP, 0, 0,	mWinChar.width + GetSystemMetrics(SM_CXFIXEDFRAME) * 2,
						mWinChar.height + gMenuHeight + GetSystemMetrics(SM_CYFIXEDFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION),	0);
		LeaveCriticalSection(&gDrawCS);

		RECT rect;
		RECT rect2;
		GetWindowRect(gHWNDMain, &rect);
		GetWindowRect(gHWNDMain, &rect2);
		rect.right = rect.left+mWinChar.width;
		rect.bottom = rect.top+mWinChar.height+gMenuHeight;

		AdjustWindowRect(&rect, WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
			| WS_CAPTION | WS_BORDER | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

		MoveWindow(gHWNDMain, rect2.left, rect2.top, rect.right-rect.left, rect.bottom-rect.top, TRUE);
		ShowWindow(gHWNDMain, SW_SHOWNORMAL);
	}
	damageFrameBufferAll();
	return true;
}

int Win32Display::toString(char *buf, int buflen) const
{
	return snprintf(buf, buflen, "Win32");
}

void Win32Display::finishMenu()
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
	sys_convert_display(mClientChar, mWinChar, gFrameBuffer, winframebuffer, 0, mClientChar.height-1);
	memmove(menuData, winframebuffer, mWinChar.width * mMenuHeight
		* mWinChar.bytesPerPixel);
	InvalidateRect(gHWNDMain, NULL, FALSE);
}

void Win32Display::updateTitle()
{
	String key;
	int key_toggle_mouse_grab = gKeyboard->getKeyConfig().key_toggle_mouse_grab;
	SystemKeyboard::convertKeycodeToString(key, key_toggle_mouse_grab);
	ht_snprintf(mCurTitle, sizeof mCurTitle, "%s - [%s %s mouse]", mTitle,key.contentChar(), (isMouseGrabbed() ? "disables" : "enables"));
	SetWindowText(gHWNDMain, mCurTitle);
}

void Win32Display::setMouseGrab(bool enable)
{
	if (mMouseGrabbed == enable) return;
	SystemDisplay::setMouseGrab(enable);
	if (enable) {
		mResetMouseX = mCurMouseX;
		mResetMouseY = mCurMouseY;

		showCursor(false);
		if (mFullscreenChanged) {
			SetCursorPos(mHomeMouseX, mHomeMouseY);
		} else {
			RECT wndRect;
			GetWindowRect(gHWNDMain, &wndRect);
			SetCursorPos(wndRect.left + mHomeMouseX + GetSystemMetrics(SM_CXFIXEDFRAME), 
				wndRect.top + mHomeMouseY + GetSystemMetrics(SM_CYFIXEDFRAME)
				+ GetSystemMetrics(SM_CYCAPTION));
		}
	} else {
		if (mFullscreenChanged) {
			SetCursorPos(mResetMouseX, mResetMouseY);
		} else {
			RECT wndRect;
			GetWindowRect(gHWNDMain, &wndRect);
			SetCursorPos(wndRect.left + mResetMouseX + GetSystemMetrics(SM_CXFIXEDFRAME), 
				wndRect.top + mResetMouseY + GetSystemMetrics(SM_CYFIXEDFRAME)
				+ GetSystemMetrics(SM_CYCAPTION));
		}
		showCursor(true);
	}
}

void Win32Display::displayShow()
{
	if (!isExposed()) return;

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
	if (lastDamagedLine >= (uint)mClientChar.height) {
		lastDamagedLine = mClientChar.height-1;
	}

	sys_convert_display(mClientChar, mWinChar, gFrameBuffer, winframebuffer, firstDamagedLine, lastDamagedLine);

	HDC hdc = GetDC(gHWNDMain);

	SetDIBitsToDevice(
		hdc,
		0,
		gMenuHeight+firstDamagedLine,
		mWinChar.width,
		lastDamagedLine-firstDamagedLine+1,  // number of lines to draw
		0,
		mWinChar.height-lastDamagedLine-1, // line src-position (0,0 = lower left)
		0,
       		mWinChar.height,
		winframebuffer, &gBitmapInfo, DIB_RGB_COLORS);

	ReleaseDC(gHWNDMain, hdc);

	LeaveCriticalSection(&gDrawCS);
}

void Win32Display::createBitmap()
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

void Win32Display::initCursor()
{
	mVisibleCursor = GetCursor();

	int cx = GetSystemMetrics(SM_CXCURSOR); 
	int cy = GetSystemMetrics(SM_CYCURSOR); 

	BYTE andplane[cx*cy];
	BYTE xorplane[cx*cy];

	memset(andplane, 0xff, cx*cy);
	memset(xorplane, 0, cx*cy);
	mInvisibleCursor = CreateCursor(NULL, 0, 0, cx, cy, andplane, xorplane);
}

void Win32Display::showCursor(bool visible)
{
	SetClassLong(gHWNDMain, GCL_HCURSOR, visible ? (LONG)mVisibleCursor : (LONG)mInvisibleCursor);
}

SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms)
{
	if (gDisplay) return gDisplay;
	return new Win32Display(title, chr, redraw_ms);
}
