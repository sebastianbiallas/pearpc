/* 
 *	PearPC
 *	font.h
 *
 *	Copyright (C) 1999-2003 Sebastian Biallas (sb@biallas.net)
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
#ifndef __FONT_H__
#define __FONT_H__

#include "tools/data.h"
#include "display.h"

class Font: public Object {
	int mCharWidth;
	int mCharHeight;
	int mRealWidth;
	int mRealHeight;
	int mPadX;
	int mPadY;
	int mBytes;
	byte *mData;
public:
		Font();
	bool	loadFromFile(File &file);
	void	setPadding(int px, int py);
	        ~Font();

	void	drawChar(int x, int y, byte c, uint fgcolor, uint bgcolor, byte *tobuf, int bufwidth, int bufheight, int bufcolorbytes);
	void	drawFixedChar(int x, int y, int dx, int dy, byte c, uint fgcolor, uint bgcolor, byte *tobuf, int bufwidth, int bufheight, int bufcolorbytes);

	void	drawChar2(SystemDisplay *toDisplay, int x, int y, byte c, RGBA fgcolor, RGBA bgcolor);
	void	drawFixedChar2(SystemDisplay *toDisplay, int x, int y, int dx, int dy, byte c, RGBA fgcolor, RGBA bgcolor);
};

#endif
