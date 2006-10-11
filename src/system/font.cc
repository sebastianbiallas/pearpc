/* 
 *	PearPC
 *	font.cc
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

#include <cstdlib>
#include <cstring>
#include "system/font.h"

struct FFH {
	byte magic[4];
	byte height;
	byte dist;
	byte res0[11];
} PACKED;

struct FChar {
	byte width;
	uint16 data[0];
};

Font::Font()
{
	mData = NULL;
}

Font::~Font()
{
	free(mData);
}

bool Font::loadFromFile(File &file)
{
	free(mData);
	mData = NULL;
	FFH hdr;
	file.readx(&hdr, sizeof hdr);
	unsigned int c = 256 * (2 * hdr.height + 1);
	mData = (byte*)malloc(c);
	memset(mData, 0, c);
	mCharWidth = 1;
	mCharHeight = hdr.height;
	mBytes = 2;
	file.readx(mData, c);
	for (int i=0; i<256; i++) {
		byte width = mData[(mCharHeight*2+1)*i];
		if (width > mCharWidth) mCharWidth = width;
	}
	mPadX = 0;
	mPadY = 0;
	mRealWidth = mCharWidth;
	mRealHeight = mCharHeight;
	return true;
}

void Font::setPadding(int px, int py)
{
	mRealWidth = mRealWidth-mPadX+px;
	mRealHeight = mRealHeight-mPadY+py;
}

void Font::drawChar(int x, int y, byte c, uint fgcolor, uint bgcolor, byte *tobuf, int bufwidth, int bufheight, int bufcolorbytes)
{
	byte width = mData[(mCharHeight*2+1)*c];
	int jshift = (mCharWidth - width)/2;
	int bufsize = bufwidth * bufheight * bufcolorbytes;
	uint o = (x + y * bufwidth)*bufcolorbytes;
	for (int i=0; i < mCharHeight; i++) {
		byte *chr = mData + (mCharHeight*2+1)*c + i*2 + 1;
		uint16 cdata = (chr[1] << 8) | chr[0];
		int a = o + i * bufwidth * bufcolorbytes;
		for (int j=0; j< mCharWidth; j++) {
			if (a < bufsize) {
				uint c;
				if ((cdata << jshift) & (1 << j)) {
					c = fgcolor;
				} else {
					c = bgcolor;
				}
				switch (bufcolorbytes) {
				case 1:
					tobuf[a] = c;
					break;
				case 2:
					tobuf[a+0]=(((c>>8)>>1)&0x7c)|(((c>>16)>>6) & 3);
					tobuf[a+1]=(((c>>16)<<2)&0xe0)|(((c>>24)>>3) & 0x1f);
					break;
				case 4:
					tobuf[a+0]=0;
					tobuf[a+1]=c>>8;
					tobuf[a+2]=c>>16;
					tobuf[a+3]=c>>24;
					break;
				}
			}
			a += bufcolorbytes;
		}
	}
}

void Font::drawFixedChar(int x, int y, int dx, int dy, byte c, uint fgcolor, uint bgcolor, byte *tobuf, int bufwidth, int bufheight, int bufcolorbytes)
{
	drawChar(dx+x*mRealWidth, dy+y*mRealHeight, c, fgcolor, bgcolor, tobuf, bufwidth, bufheight, bufcolorbytes);
}

void Font::drawChar2(SystemDisplay *toDisplay, int x, int y, byte c, RGBA fgcolor, RGBA bgcolor)
{
	byte width = mData[(mCharHeight*2+1)*c];
	int jshift = (mCharWidth - width)/2;
	for (int i=0; i < mCharHeight; i++) {
		byte *chr = mData + (mCharHeight*2+1)*c + i*2 + 1;
		uint16 cdata = (chr[1] << 8) | chr[0];
		for (int j=0; j< mCharWidth; j++) {
			RGBA c;
			if ((cdata << jshift) & (1 << j)) {
				c = fgcolor;
			} else {
				c = bgcolor;
			}
			toDisplay->putPixelRGBA(x+j, y+i, c);
		}
	}
}

void Font::drawFixedChar2(SystemDisplay *toDisplay, int x, int y, int dx, int dy, byte c, RGBA fgcolor, RGBA bgcolor)
{
	drawChar2(toDisplay, dx+x*mRealWidth, dy+y*mRealHeight, c, fgcolor, bgcolor);
}
