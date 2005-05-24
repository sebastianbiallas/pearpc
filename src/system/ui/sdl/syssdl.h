/*
 *	HT Editor
 *	syssdl.h
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

#ifndef __SYSSDL_H__
#define __SYSSDL_H__

#include <SDL.h>

#include "system/display.h"
#include "system/systhread.h"

extern SDL_Surface *	gSDLScreen;

class SDLSystemDisplay: public SystemDisplay {
protected:
	DisplayCharacteristics	mSDLChar;	
	byte *			mSDLFrameBuffer;
	bool			mChangingScreen;
	SDL_Surface *		mSDLClientScreen;
	sys_mutex		mRedrawMutex;
	SDL_Cursor *		mVisibleCursor;
	SDL_Cursor *		mInvisibleCursor;
	
	uint bitsPerPixelToXBitmapPad(uint bitsPerPixel);
	void dumpDisplayChar(const DisplayCharacteristics &chr);
public:
	char *			mTitle;
	DisplayCharacteristics  mSDLChartemp;
	SDL_cond 		*mWaitcondition;
	bool			mChangeResRet;
	uint32			mEventThreadID;

	SDLSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms);

		void finishMenu();
	virtual	void updateTitle();
	virtual	int  toString(char *buf, int buflen) const;
		void toggleFullScreen();
	virtual	void displayShow();
	virtual	void convertCharacteristicsToHost(DisplayCharacteristics &aHostChar, const DisplayCharacteristics &aClientChar);
	virtual	bool changeResolution(const DisplayCharacteristics &aCharacteristics);	
	virtual	bool changeResolutionREAL(const DisplayCharacteristics &aCharacteristics);	
	virtual	void getHostCharacteristics(Container &modes);
	virtual void setMouseGrab(bool enable);
	virtual void initCursor();
};



#endif
