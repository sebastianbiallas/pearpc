/*
 *	HT Editor
 *	display.h
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf (stefan@weyergraf.de)
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

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "tools/data.h"
#include "types.h"

/* codepages */

#define   CP_DEVICE		0
#define   CP_GRAPHICAL		1
#define   CP_WINDOWS		0x100

/* "graphical" chars (ie. lines, corners and patterns like in ASCII) */

#define	GC_TRANSPARENT		'0'		// transparent

extern byte *	gFramebuffer;
extern uint	gFramebufferScanlineLen;

extern uint gDamageAreaFirstAddr, gDamageAreaLastAddr;

inline void damageFrameBuffer(uint addr)
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
	gDamageAreaLastAddr = 0xfffffff0;
}

inline void healFrameBuffer()
{
	gDamageAreaFirstAddr = 0xfffffff0;
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

enum DisplayEventType {
	evNone = 0,
	evKey = 1,
	evMouse = 2,
};

struct DisplayEvent {
	DisplayEventType type;
	union {
    		struct {
			int x;
		    	int y;
			int relx;
		    	int rely;
			bool button1; // left mouse button
			bool button2; // right mouse button
			bool button3; // middle mouse button
		} mouseEvent;
		struct {
			uint keycode;
			bool pressed;
			char chr;
		} keyEvent;
	};
};

struct DisplayCharacteristics {
	int width, height;
	uint bytesPerPixel;	// may only be 1, 2 or 4
	
	bool indexed;

	uint redShift;
	uint redSize;
	uint greenShift;
	uint greenSize;
	uint blueShift;
	uint blueSize;
};

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

	RGB		palette[256]; // only used in indexed modes

	/* hw cursor */
	int		mHWCursorX, mHWCursorY;
	int		mHWCursorVisible;
	byte *		mHWCursorData;

	/* menu */
	int		mMenuX, mMenuHeight;
	Array *		mMenu;

	/* compose dialog */
	bool		mCatchMouseToggle;

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

	SystemDisplay(const DisplayCharacteristics &aCharacteristics);
	virtual ~SystemDisplay();

	virtual void displayShow() = 0;
	virtual bool changeResolution(const DisplayCharacteristics &aCharacteristics) = 0;
	virtual int  getKeybLEDs() = 0;
	virtual void setKeybLEDs(int leds) = 0;

	/* VT */
	bool	openVT(int width, int height, int dx, int dy, File &font);
	void	closeVT();
	void	print(const char *s);
	void	printf(const char *s, ...);
	virtual void drawChar(int x, int y, vcp color, byte chr);
	virtual void fillVT(int x, int y, int w, int h, vcp color, byte chr);
     	virtual void fillAllVT(vcp color, byte chr);
	void	setAnsiColor(vcp color);

	/* event handling */
	virtual bool getEvent(DisplayEvent &ev)=0;
	virtual void getSyncEvent(DisplayEvent &ev)=0;
	virtual void queueEvent(DisplayEvent &ev)=0;
	virtual	void startRedrawThread(int msec)=0;

	/* ui */
		void insertMenuButton(Stream &str, void (*callback)(void *), void *p);
	virtual	void finishMenu() = 0;
		void drawMenu();
		void clickMenu(int x, int y);
		void composeKeyDialog();
		bool getCatchMouseToggle();
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
};

/* system-dependent (implementation in $MYSYSTEM/ *.cc) */
extern SystemDisplay *gDisplay;
SystemDisplay *allocSystemDisplay(const char *name, const DisplayCharacteristics &chr);

#define KEY_a 0
#define KEY_s 1
#define KEY_d 2
#define KEY_f 3
#define KEY_h 4
#define KEY_g 5
#define KEY_z 6
#define KEY_x 7
#define KEY_c 8
#define KEY_v 9

#define KEY_b 11
#define KEY_q 12
#define KEY_w 13
#define KEY_e 14
#define KEY_r 15
#define KEY_y 16
#define KEY_t 17
#define KEY_1 18
#define KEY_2 19
#define KEY_3 20
#define KEY_4 21
#define KEY_6 22
#define KEY_5 23
#define KEY_EQ 24
#define KEY_9 25
#define KEY_7 26
#define KEY_MINUS 27
#define KEY_8 28
#define KEY_0 29
#define KEY_BRACKET_R 30
#define KEY_o 31
#define KEY_u 32
#define KEY_BRACKET_L 33
#define KEY_i 34
#define KEY_p 35
#define KEY_RETURN 36
#define KEY_l 37
#define KEY_j 38
#define KEY_APOSTROPHE 39
#define KEY_k 40
#define KEY_SEMICOLON 41
#define KEY_BACKSLASH 42
#define KEY_COMMA 43
#define KEY_SLASH 44
#define KEY_n 45
#define KEY_m 46
#define KEY_PERIOD 47
#define KEY_TAB 48
#define KEY_SPACE 49
#define KEY_GRAVE 50
#define KEY_DELETE 51

#define KEY_ESCAPE 53
#define KEY_CONTROL 54
#define KEY_ALT 55
#define KEY_SHIFT 56
#define KEY_CAPS_LOCK 57
#define KEY_ALTGR 58
#define KEY_LEFT 59
#define KEY_RIGHT 60
#define KEY_DOWN 61
#define KEY_UP 62

#define KEY_KP_PERIOD 65
#define KEY_KP_MULTIPLY 67
#define KEY_KP_ADD 69
#define KEY_NUM_LOCK 71
#define KEY_KP_DIVIDE 75
#define KEY_KP_ENTER 76
#define KEY_KP_SUBTRACT 78
#define KEY_KP_0 82
#define KEY_KP_1 83
#define KEY_KP_2 84
#define KEY_KP_3 85
#define KEY_KP_4 86
#define KEY_KP_5 87
#define KEY_KP_6 88
#define KEY_KP_7 89

#define KEY_KP_8 91
#define KEY_KP_9 92

#define KEY_F5 96
#define KEY_F6 97
#define KEY_F7 98
#define KEY_F3 99
#define KEY_F8 100
#define KEY_F9 101

#define KEY_F11 103

#define KEY_F13 105

#define KEY_SCROLL_LOCK 107

#define KEY_F10 109

#define KEY_F12 111

#define KEY_PAUSE 113
#define KEY_INSERT 114
#define KEY_HOME 115
#define KEY_PRIOR 116
#define KEY_REMOVE 117

#define KEY_F4 118
#define KEY_END 119
#define KEY_F2 120
#define KEY_NEXT 121
#define KEY_F1 122
	
//keycode 0x7b = Shift
//keycode 0x7c = AltGr
//keycode 0x7d = Control

#endif /* __DISPLAY_H__ */
