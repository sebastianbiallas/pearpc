/* 
 *	PearPC
 *	display.cc
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf
 *	Copyright (C) 2003,2004 Sebastian Biallas (sb@biallas.net)
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

#include <new>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

#include "display.h"
#include "tools/snprintf.h"
#include "gif.h"

// For key support
#include "io/cuda/cuda.h"
#include "system/sys.h"
#include "system/keyboard.h"

byte *	gFrameBuffer = NULL;
int 	gDamageAreaFirstAddr, gDamageAreaLastAddr;

#define IS_FGTRANS(c) (VC_GET_BASECOLOR(VCP_FOREGROUND((c)))==VC_TRANSPARENT)
#define IS_BGTRANS(c) (VC_GET_BASECOLOR(VCP_BACKGROUND((c)))==VC_TRANSPARENT)

static vcp mixColors(vcp base, vcp layer)
{
	vc fg, bg;
	if (IS_FGTRANS(layer)) {
		fg = VCP_FOREGROUND(base);
	} else {
		fg = VCP_FOREGROUND(layer);
	}
	if (IS_BGTRANS(layer)) {
		bg = VCP_BACKGROUND(base);
	} else {
		bg = VCP_BACKGROUND(layer);
	}
	if (IS_FGTRANS(layer) && (fg != VC_TRANSPARENT) && (fg == bg)) fg=(fg+1)%8;
	if (IS_BGTRANS(layer) && (bg != VC_TRANSPARENT) && (fg == bg)) bg=(bg+1)%8;
	return VCP(fg, bg);
}

SystemDisplay *gDisplay = NULL;

class MenuEntry: public Object {
	int x, y, w, h;
	void (*mCallback)(void *);
	void *mCallbackParam;
public:
	Gif *mPic;

	MenuEntry(int X, int Y, Gif *pic, void (*callback)(void *), void *callbackparam)
	{
		x = X; y = Y; w = pic->mWidth; h = pic->mHeight;
		mPic = pic;
		mCallback = callback;
		mCallbackParam = callbackparam;
	}
	
	virtual ~MenuEntry()
	{
		delete mPic;
	}
	
	void check(int cx, int cy)
	{
		if (cx >= x && cy >= y && cx <= (x+w) && cy <= (y+h)) {
			mCallback(mCallbackParam);
		}
	}
};


#include "vt100.h"
#include "font.h"

static RGBA _16toRGBA[16] = {
	MK_RGBA(0x22, 0x22, 0x22, 0xff),
	MK_RGBA(0x00, 0x00, 0xaa, 0xff),
	MK_RGBA(0x00, 0xaa, 0x00, 0xff),
	MK_RGBA(0x00, 0xaa, 0xaa, 0xff),
	MK_RGBA(0xaa, 0x00, 0x00, 0xff),
	MK_RGBA(0xaa, 0x00, 0xaa, 0xff),
	MK_RGBA(0xaa, 0xaa, 0x00, 0xff),
	MK_RGBA(0xaa, 0xaa, 0xaa, 0xff),
	MK_RGBA(0x55, 0x55, 0x55, 0xff),
	MK_RGBA(0x55, 0x55, 0xff, 0xff),
	MK_RGBA(0x55, 0xff, 0x55, 0xff),
	MK_RGBA(0x55, 0xff, 0xff, 0xff),
	MK_RGBA(0xff, 0x55, 0x55, 0xff),
	MK_RGBA(0xff, 0x55, 0xff, 0xff),
	MK_RGBA(0xff, 0xff, 0x55, 0xff),
	MK_RGBA(0xff, 0xff, 0xff, 0xff)
};

#define MASK(shift, size) (((1 << (size))-1)<<(shift))
void dumpDisplayChar(const DisplayCharacteristics &chr)
{
	fprintf(stderr, "\tdimensions:          %d x %d pixels\n", chr.width, chr.height);
	fprintf(stderr, "\tpixel size in bytes: %d\n", chr.bytesPerPixel);
	fprintf(stderr, "\tpixel size in bits:  %d\n", chr.bytesPerPixel*8);
	fprintf(stderr, "\tred_mask:            %08x (%d bits)\n", MASK(chr.redShift, chr.redSize), chr.redSize);
	fprintf(stderr, "\tgreen_mask:          %08x (%d bits)\n", MASK(chr.greenShift, chr.greenSize), chr.greenSize);
	fprintf(stderr, "\tblue_mask:           %08x (%d bits)\n", MASK(chr.blueShift, chr.blueSize), chr.blueSize);
	fprintf(stderr, "\tdepth:               %d\n", chr.redSize + chr.greenSize + chr.blueSize);
}

SystemDisplay::SystemDisplay(const DisplayCharacteristics &aClientChr, int redraw_ms)
{
	mClientChar = aClientChr;
	mRedraw_ms = redraw_ms;
	mHWCursorX = 0;
	mHWCursorY = 0;
	mHWCursorVisible = false;
	mHWCursorData = NULL;
	
	mMenu = new Array(true);
	mMenuX = 0;
	mMenuHeight = 20;
	mMouseGrabbed = false;
	
	mFullscreenChanged = false;
	mFullscreen = false;

	mExposed = false;
}

SystemDisplay::~SystemDisplay()
{
	delete mMenu;
}

bool SystemDisplay::openVT(int width, int height, int dx, int dy, File &font)
{
	mVTWidth = width;
	mVTHeight = height;
	mVTDX = dx;
	mVTDY = dy;	
	mFont = new Font();
	if (!((Font*)mFont)->loadFromFile(font)) return false;
	buf = (BufferedChar*)malloc(sizeof (BufferedChar) * width * height);
	vt = new VT100Display(width, height, this);
	((VT100Display*)vt)->setAutoNewLine(true);
	return true;
}

void SystemDisplay::closeVT()
{
	delete vt;
	free(buf);
}

void SystemDisplay::setHWCursor(int x, int y, bool visible, byte *data)
{
	mHWCursorX = x;
	mHWCursorY = y;
	mHWCursorVisible = visible;
	mHWCursorData = data;
	displayShow();
}

void SystemDisplay::print(const char *s)
{
	const void *x = s;
	int xl = strlen(s);
	((VT100Display*)vt)->termWrite(x, xl);
}

void SystemDisplay::printf(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	char buf[1024];
	ht_vsnprintf(buf, sizeof buf, s, ap);
	print(buf);
	va_end(ap);
}

void SystemDisplay::setAnsiColor(vcp color)
{
	char ansiseq[32];
	vcpToAnsi(ansiseq, color);
	print(ansiseq);
}

void SystemDisplay::fillAllVT(vcp color, byte chr)
{
	fillVT(0, 0, mVTWidth, mVTHeight, color, chr);
}

void SystemDisplay::drawChar(int x, int y, vcp color, byte chr)
{
	buf[y*mVTWidth+x].rawchar = chr;	
	buf[y*mVTWidth+x].color = color;

	uint bg = VCP_BACKGROUND(color);
	uint fg = VCP_FOREGROUND(color);
	if (VC_GET_LIGHT(bg)) {
		bg &= 0xf;
		bg += 8;
	}
	if (VC_GET_LIGHT(fg)) {
		fg &= 0xf;
		fg += 8;
	}
	RGBA bg2 = _16toRGBA[bg];
	RGBA fg2 = _16toRGBA[fg];
	((Font*)mFont)->drawFixedChar2(this, x, y, mVTDX, mVTDY, chr, fg2, bg2);
}

void SystemDisplay::fillVT(int x, int y, int w, int h, vcp color, byte chr)
{
	for (int iy = y; iy < y+h; iy++) {
		if (iy >= mVTHeight) break;
		BufferedChar *b = buf+x+ iy * mVTWidth;
		for (int ix = x; ix < x+w; ix++) {
			b->rawchar = chr;
			b->color = mixColors(b->color, color);
			b++;
		}
	}
	for (int iy = 0; iy < mVTHeight; iy++) {
		BufferedChar *b = buf+x+ iy * mVTWidth;
		for (int ix = x; ix < mVTWidth; ix++) {
			uint bg = VCP_BACKGROUND(b->color);
			uint fg = VCP_FOREGROUND(b->color);
			if (VC_GET_LIGHT(bg)) {
				bg &= 0xf;
				bg += 8;
			}
			if (VC_GET_LIGHT(fg)) {
				fg &= 0xf;
				fg += 8;
			}
			RGBA bg2 = _16toRGBA[bg];
			RGBA fg2 = _16toRGBA[fg];
			((Font*)mFont)->drawFixedChar2(this, ix, iy, mVTDX, mVTDY, b->rawchar, fg2, bg2);
			b++;
		}
	}	
}

/*void SystemDisplay::fill(int x, int y, int w, int h, RGB c)
{
	uint color;
	c = MK_RGB(0xff, 0xff, 0xff);
	mixRGB((byte*)&color, c);
	if (x > mClientChar.width) return;
	if (y > mClientChar.height) return;
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (w <= 0) return;
	if (h <= 0) return;
	if (x+w > mClientChar.width) {
		w = mClientChar.width-x;
		if (!w) return;
	}
	if (y+h > mClientChar.height) {
		h = mClientChar.height-y;
		if (!h) return;
	}
	byte *f = framebuffer + ((y*mClientChar.width)+x)*mClientChar.bytesPerPixel;
	switch (mClientChar.bytesPerPixel) {
	case 2:
		while (h--) {
			for (int i=0; i<w; i++) {
				*(f++) = (((color>>8)>>1)&0x7c)|(((color>>16)>>6) & 3);
				*(f++) = (((color>>16)<<2)&0xe0)|(((color>>24)>>3) & 0x1f);
			}
        		f += (mClientChar.width-w)*2;
		}
		break;
	case 4:
		while (h--) {
			for (int i=0; i<w; i++) {
				*(f++) = 0;
				*(f++) = color>>8;
				*(f++) = color>>16;
				*(f++) = color>>24;
			}
        		f += (mClientChar.width-w)*4;
		}
		break;
	}
}*/

void SystemDisplay::fillRGB(int x, int y, int w, int h, RGB rgb)
{
	while (h--) {
		for (int i=0; i<w; i++) {
			putPixelRGB(x+i, y, rgb);
		}
		y++;
	}
}

void SystemDisplay::setColor(int idx, RGB color)
{
	if (idx >= 0 && idx < 256) palette[idx] = color;
}

RGB  SystemDisplay::getColor(int idx)
{
	if (idx >= 0 && idx < 256) return palette[idx];
	return 0;
}

void SystemDisplay::fillRGBA(int x, int y, int w, int h, RGBA rgba)
{
	while (h--) {
		for (int i=0; i<w; i++) {
			putPixelRGBA(x+i, y, rgba);
		}
		y++;
	}	
}

#include <math.h>
void SystemDisplay::drawCircleFilled(int x, int y, int w, int h, int cx, int cy, int r, RGBA fg, RGBA bg)
{
	for (int iy=y; iy<y+h; iy++) {
		for (int ix=x; ix<x+h; ix++) {
			int rx = ix-cx;
			int ry = iy-cy;
			int d = rx*rx + ry*ry;
			int c = 6;
			if (d < r*r) {
				putPixelRGBA(ix, iy, bg);
			}
			if ((d >= r*r-c*c) && (d <= r*r+c*c)) {
				RGBA fg2 = fg;
				int z = RGBA_A(fg);
//				int q = (3*c*r-sqrt(3*3*c*c*d));
//				if (q<0) z+=q; else z-=q;
//				if (z < 0) z = 0; else if (z>255) z = 255;
				fg2 = RGBA_SETA(fg2, z);
				putPixelRGBA(ix, iy, fg2);
			}
		}
	}
}

void SystemDisplay::drawBox(int x, int y, int w, int h, RGBA fg, RGBA bg)
{
#if 1
	fillRGBA(x, y, w, h, bg);
	for (int xi=0; xi<w; xi++) {
		putPixelRGBA(x+xi, y, fg);
		putPixelRGBA(x+xi, y+h-1, fg);
	}
	for (int yi=1; yi<h-1; yi++) {
		putPixelRGBA(x, y+yi, fg);
		putPixelRGBA(x+w-1, y+yi, fg);
	}
#else
	uint RE = 5;
	fillRGBA(x, y, w, h, bg);
//	drawCircleFilled(x, y, RE, RE, x+RE, y+RE, RE, fg, bg);
	drawCircleFilled(0, 0, 100, 100, 50, 50, 50, fg, bg);
	for (uint xi=RE; xi<w-RE; xi++) {
		putPixelRGBA(x+xi, y, fg);
		putPixelRGBA(x+xi, y+h-1, fg);
	}
	for (uint yi=1+RE; yi<h-1-RE; yi++) {
		putPixelRGBA(x, y+yi, fg);
		putPixelRGBA(x+w-1, y+yi, fg);
	}
#endif
}

void SystemDisplay::insertMenuButton(Stream &str, void (*callback)(void *), void *p)
{
	Gif *pic = new Gif(str);
	mMenu->insert(new MenuEntry(mMenuX, 0, pic, callback, p));
	mMenuX += pic->mWidth;
	mMenuHeight = MAX(mMenuHeight, pic->mHeight);
}

void SystemDisplay::drawMenu()
{
	int x = 0;
	for (int i=0; i < (int)mMenu->count(); i++) {
		MenuEntry *e = (MenuEntry*)(*mMenu)[i];
		e->mPic->draw(gDisplay, x, (mMenuHeight - e->mPic->mHeight) / 2);
		x += e->mPic->mWidth;
	}
}

void SystemDisplay::clickMenu(int x, int y)
{
	for (int i=0; i < (int)mMenu->count(); i++) {
		MenuEntry *e = (MenuEntry*)(*mMenu)[i];
		e->check(x, y);
	}
}

void SystemDisplay::composeKeyDialog()
{
	return;
	// Doesn't work since should get executed in the CPU thread
#if 0
	byte *oldframebuffer = (byte*)malloc(mClientChar.scanLineLength * mClientChar.height);
	memcpy(oldframebuffer, gFrameBuffer, mClientChar.scanLineLength * mClientChar.height);

	const int w = 400;
	const int h = 200;
	int x = (mClientChar.width-w)/2;
	int y = (mClientChar.height-h)/2;

	uint keys[4];
	int k=0;
	const RGBA fg = MK_RGBA(0,0,0,0xff);
	const RGBA bg = MK_RGBA(0xaa,0xee,0xee,0xb0);
	const RGBA tr = MK_RGBA(0,0,0,0);
	
	bool regrabMouse = false;
	if (isMouseGrabbed()) {
		regrabMouse = true;
		setMouseGrab(false);
	}

	while (1) {
redo:
		memcpy(gFrameBuffer, oldframebuffer, mClientChar.scanLineLength * mClientChar.height);
		drawBox(x, y, w, h, fg, bg);
		outText(x+10, y+10, fg, tr, "Press keys to compose key sequence...");

		String name;
		for (int i=0; i<k; i++) {
			String key_name;
			SystemKeyboard::convertKeycodeToString(key_name, keys[i]);
			name += key_name;
			if (i+1 < k) name += " + ";
		}

		outText(x+10, y+50, fg, tr, name);

		uint32 keycode;
		do {
			while (!cuda_prom_get_key(keycode)) sys_suspend();
		} while (keycode & 0x80);

		if (keycode == KEY_F11) break;

		for (int i=0; i<k; i++) {
			if (keys[i] == keycode) {
				for (int j=i+1; j<k; j++) {
					keys[j-1] = keys[j];
				}
				k--;
				goto redo;
			}
		}

		if (k<4) {
			keys[k] = keycode;
			k++;
		}
	}

	SystemEvent ev;
	ev.type = sysevKey;
	ev.key.pressed = true;
	for (int i=0; i<k; i++) {
		ev.key.keycode = keys[i];
		gKeyboard->handleEvent(ev);
	}
	ev.key.pressed = false;
	for (int i=k-1; i>=0; i--) {
		ev.key.keycode = keys[i];
		gKeyboard->handleEvent(ev);
	}

	memcpy(gFrameBuffer, oldframebuffer, mClientChar.scanLineLength * mClientChar.height);
	free(oldframebuffer);
	if (regrabMouse) setMouseGrab(true);
	damageFrameBufferAll();
#endif
}

void SystemDisplay::outText(int x, int y, RGBA fg, RGBA bg, const char *text)
{
	while (*text) {
		((Font*)mFont)->drawChar2(this, x, y, *text, fg, bg);
		text++;
		x+=8;
	}
}

void SystemDisplay::mixRGB(byte *pixel, RGB rgb)
{
	uint r = RGB_R(rgb);
	uint g = RGB_G(rgb);
	uint b = RGB_B(rgb);
	convertBaseColor(r, 8, mClientChar.redSize);
	convertBaseColor(g, 8, mClientChar.greenSize);
	convertBaseColor(b, 8, mClientChar.blueSize);
	uint p = (r << mClientChar.redShift) | (g << mClientChar.greenShift)
		| (b << mClientChar.blueShift);
	switch (mClientChar.bytesPerPixel) {
	case 1:
		pixel[0] = rgb;
		break;
	case 2:
		pixel[0] = p>>8;
		pixel[1] = p;
		break;
	case 4:
		pixel[0] = p>>24;
		pixel[1] = p>>16;
		pixel[2] = p>>8;
		pixel[3] = p;
		break;
	}
}

void SystemDisplay::mixRGBA(byte *pixel, RGBA rgba)
{
	uint r = RGBA_R(rgba);
	uint g = RGBA_G(rgba);
	uint b = RGBA_B(rgba);
	uint a = RGBA_A(rgba);
	convertBaseColor(r, 8, mClientChar.redSize);
	convertBaseColor(g, 8, mClientChar.greenSize);
	convertBaseColor(b, 8, mClientChar.blueSize);
	uint p;
	switch (mClientChar.bytesPerPixel) {
		case 1:
			p = pixel[0];
			break;
		case 2:
			p = (pixel[0] << 8) | pixel[1];
			break;
		case 4:
			p = (pixel[0] << 24) | (pixel[1] << 16) | (pixel[2] << 8) | pixel[3];
			break;
		default: 
			ht_printf("internal error in %s:%d\n", __FILE__, __LINE__);
			exit(1);
			break;
	}
	uint sr = (p>>mClientChar.redShift)   & ((1<<mClientChar.redSize)-1);
	uint sg = (p>>mClientChar.greenShift) & ((1<<mClientChar.greenSize)-1);
	uint sb = (p>>mClientChar.blueShift)  & ((1<<mClientChar.blueSize)-1);
	sr = (r*a + sr*(255-a))/255;
	sg = (g*a + sg*(255-a))/255;
	sb = (b*a + sb*(255-a))/255;
	p = (sr << mClientChar.redShift) | (sg << mClientChar.greenShift)
		| (sb << mClientChar.blueShift);
	switch (mClientChar.bytesPerPixel) {
	case 1:
		pixel[0] = p;
		break;
	case 2:
		pixel[0] = p>>8;
		pixel[1] = p;
		break;
	case 4:
		pixel[0] = p>>24;
		pixel[1] = p>>16;
		pixel[2] = p>>8;
		pixel[3] = p;
		break;
	}
}

void SystemDisplay::putPixelRGB(int x, int y, RGB rgb)
{
	uint addr = x*mClientChar.bytesPerPixel + y*mClientChar.scanLineLength;
	mixRGB(&gFrameBuffer[addr], rgb);
	damageFrameBuffer(addr);
}

void SystemDisplay::putPixelRGBA(int x, int y, RGBA rgba)
{
	uint addr = x*mClientChar.bytesPerPixel + y*mClientChar.scanLineLength;
	mixRGBA(&gFrameBuffer[addr], rgba);
	damageFrameBuffer(addr);
}

void SystemDisplay::setMouseGrab(bool mouseGrab)
{
	mMouseGrabbed = mouseGrab;
	if (!mFullscreenChanged) updateTitle();
}

bool SystemDisplay::setFullscreenMode(bool fullscreen)
{
	mFullscreen = fullscreen;
	changeResolution(mClientChar);
	if (mFullscreenChanged) {
		setMouseGrab(true);
	} else {
		setMouseGrab(false);
	}
	return mFullscreenChanged;
}
