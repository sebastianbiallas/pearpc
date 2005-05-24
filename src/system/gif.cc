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

#include <cstring>

#include "system/types.h"
#include "system/gif.h"
#include "tools/except.h"
#include "tools/snprintf.h"

struct GIF_IDB {
	byte ISH;	// = 0x2c
	uint16 x;
	uint16 y;
	uint16 width;
	uint16 height;
	byte flags;
	byte initial_code_size;
} PACKED;

Gif::Gif()
{
	pic = NULL;
}

Gif::~Gif()
{
	delete[] pic;
}

Gif::Gif(Stream &str)
{
	pic = NULL;
	if (!loadFromByteStream(str)) {
		String res; str.getDesc(res);
		throw MsgfException("error loading '%y' (not a gif?)", &res);
	}
}

static inline bool getlogbyte(Stream &stream, int width, int &bitleft, int &byteleft, uint16 &lbyte, uint16 &curcode, int &bidx, byte *buf)
{
	curcode = lbyte >> (8-bitleft);
	curcode &= (1<<width)-1;
	int want = width - bitleft;
	int have = bitleft;
	bitleft -= width;
	if (bitleft<0) bitleft = 0;
	while (want > 0) {
		if (!byteleft) {
			stream.readx(buf, 1);
			byteleft = buf[0];
			if (!byteleft) return false;
			stream.readx(buf, byteleft);
			bidx = 0;
		}
		byteleft--;
		lbyte = buf[bidx++];
		bitleft+=8;
		int take = want;
		if (take > 8) take = 8;
		int a = lbyte & ((1<<take)-1);
		a <<= have;
		curcode |= a;
		bitleft -= take;
		have += take;
		want -= take;
	}
	return true;
}

bool Gif::loadFromByteStream(Stream &stream)
{
	byte buf[768];
	int idx;
	uint16 alphStack[4096];
	uint16 alphPrefix[4096];
	uint16 alphTail[4096];
	memset(alphStack, 0xff, sizeof alphStack);
	memset(alphPrefix, 0xff, sizeof alphPrefix);
	memset(alphTail, 0xff, sizeof alphTail);
	
	int free, width, max, stackp, bitleft, byteleft, bidx;
	uint16 curcode, oldcode, readb, lbyte, special;
	oldcode = 0;
	special = 0;
	
	stream.readx(buf, 6); // magic
	if (buf[0] != 'G' || buf[1] != 'I' || buf[2] != 'F' || buf[3] != '8' || buf[5] != 'a') return false;
	int version;
	switch (buf[4]) {
	case '7': version = 7; break;
	case '9': version = 9; break;
	default: return false;
	}

	stream.readx(buf, 7); // header

	mWidth = buf[0] + (buf[1]<<8);
	mHeight = buf[2] + (buf[3]<<8);
	if (buf[4] & 0x80) {
		// global color table
		int palsize = 3*(1<<((buf[4] & 7)+1));
		stream.readx(mPal, palsize);
	}
	
	stream.readx(buf, 1);
	while (buf[0] == 0x21) {
		// skip extension blocks
		stream.readx(buf, 2);
		stream.readx(buf, buf[1]+1);
		stream.readx(buf, 1);
	}
	stream.readx(buf+1, 10);
	GIF_IDB *i = (GIF_IDB*)&buf;
/*	printf("IDB: ISH=%02x, x=%04x, y=%04x, width=%04x, height=%04x, flags=%02x\n",
		i->ISH, i->x, i->y, i->width, i->height, i->flags
	);*/

	// image descriptor?
	if (i->ISH != 0x2c) return false;

	if (i->flags & 128) {		
		// local color table
		int palsize = 3*(1<<((buf[8] & 7)+1));
		stream.readx(mPal, palsize);
	}

	int initial_code_size = i->initial_code_size;
	int _CLR = 1<<initial_code_size;
	int _EOF = _CLR+1;
	lbyte = 0;
	free = _EOF+1;
	width = initial_code_size+1;
	max = (1<<width)-1;
//	ht_printf("i: %d c: %d e: %d max: %d, free: %d\n", initial_code_size, _CLR, _EOF, max, free);
	stackp = 0;
	bitleft = 0;
	byteleft = 0;
	idx = 0;
	
	pic = new byte[mWidth*mHeight];
	
	while (true) {
		if (!getlogbyte(stream, width, bitleft, byteleft, lbyte, curcode, bidx, buf)) return false;
		g:
		if (curcode == _EOF) break;
		if (curcode == _CLR) {
			width = initial_code_size+1;
			max = (1<<width)-1;
			free = _EOF;
			memset(alphStack, 0xff, sizeof alphStack);
			memset(alphPrefix, 0xff, sizeof alphPrefix);
			memset(alphTail, 0xff, sizeof alphTail);
			if (!getlogbyte(stream, width, bitleft, byteleft, lbyte, curcode, bidx, buf)) return false;
			
			special = curcode;
			oldcode = curcode;
			goto g;
		}
		readb = curcode;
		if (curcode >= free) {
			// new code 
			curcode = oldcode;
			alphStack[stackp++] = special;
		}
		// code in alphabet
		while (curcode > _CLR) {
			// decode
			alphStack[stackp++] = alphTail[curcode];
			curcode = alphPrefix[curcode];
			if (curcode == 0xffff) return false;
		}
		alphStack[stackp] = curcode;
		special = curcode;
		do {
			pic[idx++] = alphStack[stackp--];
		} while (stackp >= 0);
		stackp = 0;
		alphPrefix[free] = oldcode;
		alphTail[free] = curcode;
		oldcode = readb;
		free++;
		if (free > max) {
			if (width < 12) {
				width++;
				max = (1<<width)-1;
			}
		}
	}
	stream.readx(buf, 2);
	if (buf[0] != 0x00 || buf[1] != 0x3b) return false;
//	ht_printf("mWidth: %d mHeight: %d\n", mWidth, mHeight);
	return true;
}

void Gif::draw(SystemDisplay *display, int x, int y)
{
	int p=0;
	switch (display->mClientChar.bytesPerPixel) {
	case 1:
		return;
	case 2: {
		byte *f = gFrameBuffer+y*display->mClientChar.width*2+x*2;
		for (int i=0; i<mHeight; i++) {
			for (int j=0; j<mWidth; j++) {
				int c = pic[p]*3;
				f[0]=((mPal[c]>>1)&0x7c)|(mPal[c+1]>>6);
				f[1]=((mPal[c+1]<<2)&0xe0)|(mPal[c+2]>>3);
				f+=2;
				p++;
			}
			f += display->mClientChar.scanLineLength - mWidth*2;
		}
		break;
	}
	case 4: {
		byte *f = gFrameBuffer+y*display->mClientChar.width*4+x*4;
		for (int i=0; i<mHeight; i++) {
			for (int j=0; j<mWidth; j++) {
				int c = pic[p]*3;
				f[0]=0;
				f[1]=mPal[c];
				f[2]=mPal[c+1];
				f[3]=mPal[c+2];
				f+=4;
				p++;
			}
			f += display->mClientChar.scanLineLength - mWidth*4;
		}
		break;
	}
	default:
		ht_printf("unknown bytes per pixel in gif.cc\n");
		exit(1);
	}
	damageFrameBuffer(y * display->mClientChar.width*2 + x*2);
	damageFrameBuffer((y+mHeight) * display->mClientChar.width*2 + (x+mWidth) * 2);
}
