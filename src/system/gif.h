/* 
 *	PearPC
 *	gif.h
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
#ifndef __GIF_H__
#define __GIF_H__

#include "tools/data.h"
#include "system/display.h"

class Gif: public Object {
	byte *pic;
	byte mPal[768];
public:
	int mWidth, mHeight;
		Gif();		
		Gif(Stream &str);
		~Gif();
	bool	loadFromByteStream(Stream &str);
	void	draw(SystemDisplay *display, int x, int y);
};

#endif
