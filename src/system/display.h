/*
 *	HT Editor
 *	display.h
 *
 *	Copyright (C) 2003,2004 Stefan Weyergraf
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

#ifndef __SYSTEM_DISPLAY_H__
#define __SYSTEM_DISPLAY_H__

#include "tools/data.h"
#include "tools/stream.h"
#include "types.h"
#include "keyboard.h"

/* codepages */

#define   CP_DEVICE		0
#define   CP_GRAPHICAL		1
#define   CP_WINDOWS		0x100

/* "graphical" chars (ie. lines, corners and patterns like in ASCII) */

#define	GC_TRANSPARENT		'0'		// transparent

extern byte *	gFrameBuffer;
extern int 	gDamageAreaFirstAddr, gDamageAreaLastAddr;

inline void damageFrameBuffer(int addr)
{
	if (addr < gDamageAreaFirstAddr) {
		gDamageAreaFirstAddr = addr;
	}

	if (addr > gDamageAreaLastAddr) {
		gDamageAreaLastAddr = addr;
	}
}

inline void damageFrameBufferAll()
{
	gDamageAreaFirstAddr = 0;
	gDamageAreaLastAddr = 0xffffff0;
}

inline void healFrameBuffer()
{
	gDamageAreaFirstAddr = 0xffffff0;
	gDamageAreaLastAddr = 0;
}

/* virtual colors */

typedef int vc;

#define VC_BLACK			0
#define VC_BLUE				1
#define VC_GREEN			2
#define VC_CYAN				3
#define VC_RED				4
#define VC_MAGENTA			5
#define VC_YELLOW			6
#define VC_WHITE			7
#define VC_TRANSPARENT			8

#define VC_LIGHT(vc) ((vc) | 0x80)

#define VC_GET_LIGHT(vc) ((vc) & 0x80)
#define VC_GET_BASECOLOR(vc) ((vc) & 0x7f)

/* virtual color pairs (fg/bg) */

typedef int vcp;

#define VCP(vc_fg, vc_bg) (vcp)((vc_bg) | ((vc_fg)<<8))
#define VCP_BACKGROUND(v) ((v) & 0xff)
#define VCP_FOREGROUND(v) ((v>>8) & 0xff)

struct BufferedChar {
	uint rawchar;
	vcp color;
};

class DisplayCharacteristics: public Object {
public:
	int width, height;
	int bytesPerPixel;	// may only be 1, 2 or 4
	int scanLineLength;
	int vsyncFrequency;

	int redShift;
	int redSize;
	int greenShift;
	int greenSize;
	int blueShift;
	int blueSize;

	inline DisplayCharacteristics & operator =(const DisplayCharacteristics &chr)
	{
		width = chr.width;
		height = chr.height;
		bytesPerPixel = chr.bytesPerPixel;
		scanLineLength = chr.scanLineLength;
		vsyncFrequency = chr.vsyncFrequency;

		redShift = chr.redShift;
		redSize = chr.redSize;
		greenShift = chr.greenShift;
		greenSize = chr.greenSize;
		blueShift = chr.blueShift;
		blueSize = chr.blueSize;
		return *this;
	}
	
#define COMPARE(a) do {                               \
if (a < ((DisplayCharacteristics *)obj)->a) return -1;  \
if (a > ((DisplayCharacteristics *)obj)->a) return 1;   \
} while (0);

	virtual	int compareTo(const Object *obj) const
	{
		COMPARE(width);
		COMPARE(height);
		COMPARE(bytesPerPixel);
		COMPARE(scanLineLength);
		COMPARE(vsyncFrequency);
		COMPARE(redShift);
		COMPARE(redSize);
		COMPARE(greenShift);
		COMPARE(greenSize);
		COMPARE(blueShift);
		COMPARE(blueSize);		
		return 0;
	}
#undef COMPARE

};

void dumpDisplayChar(const DisplayCharacteristics &chr);

typedef uint32 RGB;
typedef uint32 RGBA;

#define RGBA_R(rgba) (rgba & 0xff)
#define RGBA_G(rgba) ((rgba>>8) & 0xff)
#define RGBA_B(rgba) ((rgba>>16) & 0xff)
#define RGBA_A(rgba) ((rgba>>24) & 0xff)
#define MK_RGBA(r, g, b, a) ((r) | ((g)<<8) | ((b)<<16) | ((a)<<24))
#define RGBA_SETA(rgba, a) (((rgba) & 0xffffff) | (a<<24));

#define RGB_R(rgb) (rgb & 0xff)
#define RGB_G(rgb) ((rgb>>8) & 0xff)
#define RGB_B(rgb) ((rgb>>16) & 0xff)
#define MK_RGB(r, g, b) ((r) | ((g)<<8) | ((b)<<16))

#define KEYB_LED_NUM 1
#define KEYB_LED_CAPS 2
#define KEYB_LED_SCROLL 3

class SystemDisplay: public Object
{
protected:
	Object		*vt;
	Object		*mFont;
	int		mVTWidth;
	int		mVTHeight;
	int		mVTDX;
	int		mVTDY;
	bool		mExposed;
	RGB		palette[256]; // only used in indexed modes

	/* hw cursor */
	int		mHWCursorX, mHWCursorY;
	int		mHWCursorVisible;
	byte *		mHWCursorData;

public: // until we know better
	/* menu */
	int		mMenuX, mMenuHeight;
	Array *		mMenu;

	bool		mMouseGrabbed;
	int		mRedraw_ms;
	int mCurMouseX, mCurMouseY;
	int mResetMouseX, mResetMouseY;
	int mHomeMouseX, mHomeMouseY;
	bool mFullscreen;
	bool mFullscreenChanged;
	
	static inline void convertBaseColor(uint &b, uint fromBits, uint toBits)
	{
		if (toBits > fromBits) {
			b <<= toBits - fromBits;
		} else {
			b >>= fromBits - toBits;
		}
	}

	void	mixRGB();
public:
	DisplayCharacteristics	mClientChar;
	BufferedChar	*buf;

			SystemDisplay(const DisplayCharacteristics &aClientChr, int redraw_ms);
	virtual 	~SystemDisplay();

	virtual void	displayShow() = 0;

	/*
	 *	Note: this function might do different things when in / not in fullscreen
	 *	mode.
	 */
	virtual void	convertCharacteristicsToHost(DisplayCharacteristics &aHostChar, const DisplayCharacteristics &aClientChar) = 0;

	virtual bool	changeResolution(const DisplayCharacteristics &aChar) = 0;
		bool	setFullscreenMode(bool fullscreen);
	virtual void	getHostCharacteristics(Container &modes) = 0;

	/* VT */
	bool	openVT(int width, int height, int dx, int dy, File &font);
	void	closeVT();
	void	print(const char *s);
	void	printf(const char *s, ...);
	virtual void drawChar(int x, int y, vcp color, byte chr);
	virtual void fillVT(int x, int y, int w, int h, vcp color, byte chr);
     	virtual void fillAllVT(vcp color, byte chr);
	void	setAnsiColor(vcp color);

	/* ui */
		void insertMenuButton(Stream &str, void (*callback)(void *), void *p);
	virtual	void updateTitle() = 0;
	virtual	void finishMenu() = 0;
		void drawMenu();
		void clickMenu(int x, int y);
		void composeKeyDialog();
		void drawCircleFilled(int x, int y, int w, int h, int cx, int cy, int radius, RGBA fg, RGBA bg);
		void drawBox(int x, int y, int w, int h, RGBA fg, RGBA bg);
		void setHWCursor(int x, int y, bool visible, byte *data);
		void setColor(int idx, RGB color);
		RGB  getColor(int idx);
		void fillRGB(int x, int y, int w, int h, RGB rgb);
		void fillRGBA(int x, int y, int w, int h, RGBA rgba);
		void mixRGB(byte *pixel, RGB rgb);
		void mixRGBA(byte *pixel, RGBA rgba);
		void outText(int x, int y, RGBA fg, RGBA bg, const char *text);
		void putPixelRGB(int x, int y, RGB rgb);
		void putPixelRGBA(int x, int y, RGBA rgba);

		inline void setExposed(bool exposed)
		{
			mExposed = exposed;
		}

		inline bool isExposed()
		{
			return mExposed;
		}

		inline bool isMouseGrabbed()
		{
			return mMouseGrabbed;
		}
		
		virtual void setMouseGrab(bool mouseGrab);
};

extern SystemDisplay *gDisplay;

// should be declared elsewhere
void initUI(const char *title, const DisplayCharacteristics &aCharacteristics, int redraw_ms, const KeyboardCharacteristics &keyCharacteristics, bool fullscreen);
void doneUI();

#endif /* __SYSTEM_DISPLAY_H__ */
