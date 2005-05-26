/*
 *	PearPC
 *	syswin.h
 *
 *	Copyright (C) 2004 Stefan Weyergraf
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

#ifndef __SYSWIN_H__
#define __SYSWIN_H__

#include "system/systhread.h"
#include "system/display.h"

extern HWND gHWNDMain;
extern CRITICAL_SECTION gDrawCS;
extern int gMenuHeight; 
extern BITMAPINFO gMenuBitmapInfo;
extern byte *menuData;

class Win32Display: public SystemDisplay
{
	char mCurTitle[200];
	char *mTitle;
	HCURSOR mInvisibleCursor;
	HCURSOR mVisibleCursor;

public:
	DisplayCharacteristics mWinChar;

	Win32Display(const char *name, const DisplayCharacteristics &chr, int redraw_ms);
	virtual ~Win32Display();
	virtual void getHostCharacteristics(Container &modes);
	virtual void convertCharacteristicsToHost(DisplayCharacteristics &aHostChar, const DisplayCharacteristics &aClientChar);
	virtual bool changeResolution(const DisplayCharacteristics &aHostChar);
	virtual	int  toString(char *buf, int buflen) const;
	virtual	void finishMenu();
	virtual void updateTitle();
	virtual void setMouseGrab(bool enable);
	virtual void displayShow();
	void initCursor();
	void showCursor(bool visible);
	void createBitmap();
};

#endif
