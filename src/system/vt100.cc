/* 
 *	HT Editor
 *	vt100.cc - VT100/102 emulator
 *
 *	Copyright (C) 2003 Stefan Weyergraf
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

#include <cctype>
#include <cstdio>
#include <cstring>

#include "tools/debug.h"
#include "vt100.h"

//#define VTERM_LOG_SEQS

#ifdef VTERM_LOG_SEQS
#define VTERMLOG(a...) DPRINTF(a)
#else
#define VTERMLOG(a...)
#endif

static void readCSIParamsMax16(int Pn[], int &n, int &i, const char *buf, int buflen, bool allow_spaces)
{
	n = 0;
	char c = buf[i];
	char f;
	do {
		f = c;
		int p = 0;
		while (isdigit(c) || (allow_spaces && (c ==' '))) {
			if (!(allow_spaces && (c ==' '))) {
				p *= 10;
				p += c-'0';
			}
			i++;
			if (i >= buflen) break;
			c = buf[i];
		}
		if (!n && (f != ';') && !isdigit(f)) break;
		Pn[n++] = isdigit(f) ? p : -1;
		if (i >= buflen) break;
		if (n == 16) break;
		if (c != ';') break;
		i++;
		c = buf[i];
		if (i >= buflen) break;
	} while (1);
}

VT100Display::VT100Display(int width, int height, SystemDisplay *aDisplay, vcp initColor)
{
	mDisplay = aDisplay;
	x = 0; 
	y = 0;
	w = width;
	h = height;
	mState = PLAIN;
	mDefaultColor = initColor;
	mColor = mDefaultColor;
	mColorLight = false;
	mG = 0;
	// this is our default
	mDECCKM	= false;
	mDECAWM	= true;
	mDECOM	= false;
	mLNM	= false;
	mIM	= false;
	mTermWriteBufLen = 0;
	mTop = 0;
	mBottom = h;
	cursorx = cursory = 1;
	gotoAbsXY(0, 0);

	getCursor(mSaved_cursorx, mSaved_cursory);
	mDisplay->fillAllVT(mColor, ' ');
}

void VT100Display::getCursor(int &x, int &y)
{
	x = cursorx;
	y = cursory;
}

void VT100Display::doLF()
{
	VTERMLOG("LF\n");
	if (cursory+1 == mBottom) {
		scrollDown(mTop, mBottom, 1);
	} else if (cursory < h-1) {
		cursory++;
	}
}

void VT100Display::doCR()
{
	VTERMLOG("CR\n");
	cursorx = 0;
}

void VT100Display::doBS()
{
	VTERMLOG("BS\n");
	if (cursorx) cursorx--;
}

void VT100Display::doRI()
{
	VTERMLOG("RI\n");
	if (cursory == mTop) {
		scrollUp(mTop, mBottom, 1);
	} else if (cursory > 0) {
		cursory--;
	}
}

void VT100Display::gotoXY(int ncx, int ncy)
{
	int min_y, max_y;

	if (ncx < 0) {
		cursorx = 0;
	} else if (ncx > w - 1) {
		cursorx = w - 1;
	} else {
		cursorx = ncx;
	}

	if (mDECOM) {
		min_y = mTop;
		max_y = mBottom;
	} else {
		min_y = 0;
		max_y = h;
	}

	if (ncy < min_y) {
		cursory = min_y;
	} else if (ncy >= max_y) {
		cursory = max_y - 1;
	} else {
		cursory = ncy;
	}
}

void VT100Display::gotoAbsXY(int ncx, int ncy)
{
	gotoXY(ncx, mDECOM ? mTop+ncy : ncy);
}

void VT100Display::saveCursor()
{
	getCursor(mSaved_cursorx, mSaved_cursory);
}

void VT100Display::restoreCursor()
{
	gotoXY(mSaved_cursorx, mSaved_cursory);
}

void VT100Display::scrollDown(int mTop, int mBottom, int count)
{
	int n = mBottom-mTop-1;
	while (count--) {
		memmove(mDisplay->buf+w*mTop, mDisplay->buf+w*(mTop+1), sizeof *mDisplay->buf * w*n);
		mDisplay->fillVT(0, mBottom-1, w, 1, mColor, ' ');
	}
}

void VT100Display::scrollUp(int mTop, int mBottom, int count)
{
	int n = mBottom-mTop-1;
	while (count--) {
		memmove(mDisplay->buf+w*(mTop+1), mDisplay->buf+w*mTop, sizeof *mDisplay->buf * w*n);
		mDisplay->fillVT(0, mTop, w, 1, mColor, ' ');
	}
}

void VT100Display::termifyColor(int &mColor)
{
	int fg = VCP_FOREGROUND(mColor);
	int bg = VCP_BACKGROUND(mColor);
	// light black != dark gray (but black) on VT100s
//	if (fg == VC_LIGHT(VC_BLACK)) fg = VC_BLACK;
//	if (bg == VC_LIGHT(VC_BLACK)) bg = VC_BLACK;
	mColor = VCP(fg, bg);
}

void VT100Display::setBounds(int width, int height)
{
	w = width;
	h = height;
	if (cursorx > w-1) cursorx = w-1;
	if (cursory > h-1) cursory = h-1;
	if (mSaved_cursorx > w-1) mSaved_cursorx = w-1;
	if (mSaved_cursory > h-1) mSaved_cursory = h-1;
	mTop = 0;
	mBottom = h;
}

void VT100Display::setGraphicRendition(int r)
{
	vc ctab[8] = { VC_BLACK, VC_RED, VC_GREEN, VC_YELLOW, VC_BLUE, VC_MAGENTA, VC_CYAN, VC_WHITE };
//	VTERMLOG("setgr got %d\non entry: mColorf/g=%d/%d\n", r, VCP_FOREGROUND(mColor), VCP_BACKGROUND(mColor));
	if ((r >= 30) && (r <= 37)) {
		// set foreground ...
		int i = r-30;
		vc bg = VCP_BACKGROUND(mColor);
		vc fg = mColorLight ? VC_LIGHT(ctab[i]) : ctab[i];
		mColor = VCP(fg, bg);
		termifyColor(mColor);
	} else if ((r >= 40) && (r <= 47)) {
		// set background ...
		int i = r-40;
		vc fg = VCP_FOREGROUND(mColor);
		mColor = VCP(fg, ctab[i]);
//		mColor = VCP(fg, VC_LIGHT(VC_WHITE));
		termifyColor(mColor);
	} else if (r == 0) {
		// normal
		mColor = mDefaultColor;
		termifyColor(mColor);
		// FIXME:
		mColorLight = false;
/*	} else if (r == 22) {
		// normal
		mColor = VCP(VC_WHITE, VC_BLACK);*/
	} else if (r == 1) {
		// bold
		vc bg = VCP_BACKGROUND(mColor);
		vc fg = VCP_FOREGROUND(mColor);
		mColor = VCP(VC_LIGHT(fg), bg);
		termifyColor(mColor);
		mColorLight = true;
	} else if (r == 39) {
		// set foreground to default
		vc bg = VCP_BACKGROUND(mColor);
		vc fg = VCP_FOREGROUND(mDefaultColor);
		fg = mColorLight ? VC_LIGHT(fg) : fg;
		mColor = VCP(fg, bg);
		termifyColor(mColor);
	} else if (r == 49) {
		// set background to default
		vc fg = VCP_FOREGROUND(mColor);
		mColor = VCP(fg, VCP_BACKGROUND(mDefaultColor));
		termifyColor(mColor);
	} else {
		VTERMLOG("unsupported gr. rendition %d\n", r);
	}
//	VTERMLOG("on exit: mColorf/g=%d/%d\n", VCP_FOREGROUND(mColor), VCP_BACKGROUND(mColor));
}

void VT100Display::setMode(int p, bool newValue)
{
	if (newValue) {
		// SM - set mode
		VTERMLOG("SM - set mode");
	} else {
		// RM - reset mode
		VTERMLOG("RM - reset mode");
	}
	switch (p) {
		case 4:
			mIM = newValue;
			break;
		case 20:
			mLNM = newValue;
			break;
		default:
			VTERMLOG("unsupported set/reset mode %d (new value %d)\n", p, newValue);
	}
}

void VT100Display::setPrivateMode(int p, bool newValue)
{
	if (newValue) {
		// DECSET - set private mode
		VTERMLOG("DECSET - set private mode");
	} else {
		// DECRST - reset mode
		VTERMLOG("DECRST - reset private mode");
	}
	switch (p) {
		case 1:
			mDECCKM = newValue;
			break;
		case 6:
			mDECOM = newValue;
			break;
		case 7:
			mDECAWM = newValue;
			break;
		case 25:
//			setCursorMode(newValue ? CURSOR_NORMAL : CURSOR_OFF);
			break;
		default:
			VTERMLOG("unsupported private set/reset mode %d (new value %d)\n", p, newValue);
	}
}

void VT100Display::setAutoNewLine(bool b)
{
	mLNM = b;
}

void VT100Display::termWrite(const void *aBuf, int buflen)
{
	ASSERT(mTermWriteBufLen+buflen < (int)sizeof mTermWriteBuf);
	memcpy(mTermWriteBuf+mTermWriteBufLen, aBuf, buflen);
	buflen += mTermWriteBufLen;
	const char *buf = mTermWriteBuf;
	mTermWriteBufLen = 0;
	int last_seq_start;	
	for (int i=0; i < buflen; i++) {
		last_seq_start = i;
		unsigned char c = buf[i];
		switch (c) {
			case 7:
				VTERMLOG("BELL ignored\n");
				continue;
			case 8:
				doBS();
				continue;
			case 9:
				while (cursorx < w - 1) {
					cursorx++;
					if ((cursorx < 0) || (cursorx % 8==0)) break;
				}
				continue;
			case 10: 
			case 11: 
			case 12:
				doLF();
				if (!mLNM) continue;
			case 13:
				doCR();
				continue;
			case 14:
				// select G1
				VTERMLOG("select G1\n");
				mG = 1;
				continue;
			case 15:
				// select G0
				VTERMLOG("select G0\n");
				mG = 0;
				continue;
			case 24: case 26:
				VTERMLOG("24/26 ignored\n");
				mState = PLAIN;
				continue;
			case 27:
				mState = ESC;
				continue;
		}
		switch (mState) {
	    		case PLAIN:
				if ((c >= 32) /* && (c <= 128)*/) {
//				if (c <32) c = 'X';
//					if (mG == 0) {
						mDisplay->drawChar(cursorx, cursory, mColor, (byte)c);
//					} else {
//						mDisplay->drawChar(cursorx, cursory, mColor, gc2mygc((byte)c), CP_GRAPHICAL);
//					}
					if (cursorx < w-1) {
						cursorx++;
					} else {
						if (mDECAWM) {
							doCR();
							doLF();
						}
					}
					VTERMLOG("'%c' cxy=%d,%d colorf/b=%d/%d\n", c, cursorx, cursory, VCP_FOREGROUND(mColor), VCP_BACKGROUND(mColor));
				} else {
//					char buf[128];
//					sprintf(buf, "unsupported PLAIN char %d/0x%0x\n", (byte)c, (byte)c);
//					VTERMLOG(buf);
					mDisplay->drawChar(cursorx, cursory, mColor, c-0x80);
//					printChar(cursorx, cursory, mColor, ' ');
					if (cursorx < w-1) {
						cursorx++;
					} else {
						if (mDECAWM) {
							doCR();
							doLF();
						}
					}
				}
				break;
			case ESC:
				switch (c) {
					case '[':
						mState = CSI;
						break;
					case ']':
						mState = OSC;
						break;
					case '7':
						VTERMLOG("ESC-7: save cursor\n");
						saveCursor();
						mState = PLAIN;
						break;
					case '8':
						VTERMLOG("ESC-8: restore cursor\n");
						restoreCursor();
						mState = PLAIN;
						break;
					case 'D': {
						VTERMLOG("ESC-D: line feed\n");
						doLF();
						mState = PLAIN;
						break;
					}
					case 'E': {
						VTERMLOG("ESC-E: CR,LF\n");
						doCR();
						doLF();
						mState = PLAIN;
						break;
					}
					case 'M': {
						VTERMLOG("ESC-M: cursor up\n");
						doRI();
						mState = PLAIN;
						break;
					}
					case '(':
						i++;
						if (i >= buflen) break;
						c = buf[i];
						VTERMLOG("G0 set charset (%c)\n", c);
						switch (c) {
							case '0':
								mColor = VCP(VC_WHITE, VC_BLUE);
								break;
							default:
								mColor = VCP(VC_WHITE, VC_BLACK);
						}
						i++;
						mState = PLAIN;
						break;
					case ')':
						i++;
						if (i >= buflen) break;
						c = buf[i];
						VTERMLOG("G1 set charset (%c)\n", c);
						switch (c) {
							case '0':
								mColor = VCP(VC_WHITE, VC_BLUE);
								break;
							default:
								mColor = VCP(VC_WHITE, VC_BLACK);
						}
						i++;
						mState = PLAIN;
						break;
					case '*':
						i++;
						if (i >= buflen) break;
						c = buf[i];
						VTERMLOG("G2 set charset (%c)\n", c);
						switch (c) {
							case '0':
								mColor = VCP(VC_WHITE, VC_BLUE);
								break;
							default:
								mColor = VCP(VC_WHITE, VC_BLACK);
						}
						i++;
						mState = PLAIN;
						break;
					case '+':
						i++;
						if (i >= buflen) break;
						c = buf[i];
						VTERMLOG("G3 set charset (%c)\n", c);
						switch (c) {
							case '0':
								mColor = VCP(VC_WHITE, VC_BLUE);
								break;
							default:
								mColor = VCP(VC_WHITE, VC_BLACK);
						}
						i++;
						mState = PLAIN;
						break;
					case '=':
						VTERMLOG("set decckm\n");
						mDECCKM = true;
						mState = PLAIN;
						break;
					case '>':
						VTERMLOG("reset decckm\n");
						mDECCKM = false;
						mState = PLAIN;
						break;
					default: {
						VTERMLOG("unsupported ESC-sequence '%c'\n", c);
						mState = PLAIN;
					}
				}
				break;
#define CSI_ARG(idx, defaultval) (((idx<n) && (Pn[idx] != -1)) ? Pn[idx] : defaultval)
			case CSI_QM:
			case CSI: {
				int Pn[16];
				int n;
//				VTERMLOG("CSI-fmt: %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n", buf[i+0], buf[i+1], buf[i+2], buf[i+3], buf[i+4], buf[i+5], buf[i+6], buf[i+7], buf[i+8], buf[i+9], buf[i+10], buf[i+11], buf[i+12], buf[i+13], buf[i+14], buf[i+15]);
				readCSIParamsMax16(Pn, n, i, buf, buflen, false);
				if (i >= buflen) break;
				c = buf[i];
				switch (c) {
					case 'A': {
						// CUU - cursor up (stop at top)
						VTERMLOG("CUU - cursor up\n");
						int p = CSI_ARG(0, 1);
						gotoXY(cursorx, cursory-p);
						mState = PLAIN;
						break;
					}
					case 'B': {
						// CUD - cursor down (stop at bottom)
						VTERMLOG("CUD - cursor down\n");
						int p = CSI_ARG(0, 1);
						gotoXY(cursorx, cursory+p);
						mState = PLAIN;
						break;
					}
					case 'C': {
						// CUF - cursor forward/right (stop at far right)
						VTERMLOG("CUF - cursor forward\n");
						int p = CSI_ARG(0, 1);
						gotoXY(cursorx+p, cursory);
						mState = PLAIN;
						break;
					}
					case 'D': {
						// CUB - cursor backward/left (stop at far left)
						VTERMLOG("CUB - cursor backward\n");
						int p = CSI_ARG(0, 1);
						gotoXY(cursorx-p, cursory);
						mState = PLAIN;
						break;
					}
					case 'E': {
						// ? - cursor ?
						VTERMLOG("? - cursor ? (1)\n");
						int p = CSI_ARG(0, 1);
						gotoXY(0, cursory+p);
						mState = PLAIN;
						break;
					}
					case 'F': {
						// ? - cursor ?
						VTERMLOG("? - cursor ? (2)\n");
						int p = CSI_ARG(0, 1);
						gotoXY(0, cursory-p);
						mState = PLAIN;
						break;
					}
					case '`':
					case 'G': {
						// CHA - cursor x position
						VTERMLOG("CHA - set cursor x\n");
						int p = CSI_ARG(0, 1);
						gotoXY(p-1, cursory);
						mState = PLAIN;
						break;
					}
					case 'd': {
						// VPA - cursor y position
						VTERMLOG("VPA - set cursor y\n");
						int p = CSI_ARG(0, 1);
						gotoAbsXY(cursorx, p-1);
						mState = PLAIN;
						break;
					}
					case 'f':
					case 'H': {
						// CUP - cursor position
						VTERMLOG("CUP - set cursor\n");
						int py = CSI_ARG(0, 1);
						int px = CSI_ARG(1, 1);
						gotoAbsXY(px-1, py-1);
						mState = PLAIN;
						break;
					}
					case 'J': {
						// ED - erase in screen (cursor does not move)
						int p = CSI_ARG(0, 0);
						switch (p) {
							case 0:
								// erase from cursor(inclusive) to end of screen
								VTERMLOG("ED 0 - erase from cursor(inclusive) to end of screen\n");
								mDisplay->fillVT(cursorx, cursory, w-cursorx, 1, mColor, ' ');
								mDisplay->fillVT(0, cursory+1, w, h-cursory-1, mColor, ' ');
								break;
							case 1:
								// erase from beginning of screen to cursor(inclusive)
								VTERMLOG("ED 1 - erase from beginning of screen to cursor(inclusive)\n");
								mDisplay->fillVT(0, 0, w, cursory, mColor, ' ');
								mDisplay->fillVT(0, cursory, cursorx+1, 1, mColor, ' ');
								break;
							case 2:
								// erase whole screen
								VTERMLOG("ED 2 - erase whole screen\n");
								mDisplay->fillVT(0, 0, w, h, mColor, ' ');
								break;
							default:
								VTERMLOG("ED ? - unsupported !\n");
						}
						mState = PLAIN;
						break;
					}
					case 'K': {
						// EL - erase in line (cursor does not move)
						int p = CSI_ARG(0, 0);
						switch (p) {
							case 0:
								// erase from cursor(inclusive) to EOL
								VTERMLOG("EL 0 - erase in line: from cursor to EOL\n");
								mDisplay->fillVT(cursorx, cursory, w-cursorx, 1, mColor, ' ');
								break;
							case 1:
								// erase from BOL to cursor(inclusive)
								VTERMLOG("EL 1 - erase in line: from BOL to cursor\n");
								mDisplay->fillVT(0, cursory, cursorx+1, 1, mColor, ' ');
								break;
							case 2:
								// erase line containing cursor
								VTERMLOG("EL 2 - erase line containing cursor\n");
								mDisplay->fillVT(0, cursory, w, 1, mColor, ' ');
								break;
							default:
								VTERMLOG("EL ? - unsupported !\n");
						}
						mState = PLAIN;
						break;
					}
					case 'L': {
						// IL - insert n lines (from cursor)
						VTERMLOG("IL - insert n lines(s)\n");
						int p = CSI_ARG(0, 1);
						if (p > h-cursory) VTERMLOG("DL: sc1 !!!");
						if (p > h-cursory) p = h-cursory;
						if (p < 0) p = 0;
						scrollUp(cursory, mBottom, p);
						mState = PLAIN;
						break;
					}
					case 'M': {
						// DL - delete n lines (from cursor)
						VTERMLOG("DL - delete n lines(s)\n");
						int p = CSI_ARG(0, 1);
						if (p > h-cursory) VTERMLOG("DL: sc1 !!!");
						if (p > h-cursory) p = h-cursory;
						if (p < 0) p = 0;
						scrollDown(cursory, mBottom, p);
						mState = PLAIN;
						break;
					}
					case 'P': {
						// DCH - delete n chars (from cursor)
						VTERMLOG("DCH - delete n character(s)\n");
						int p = CSI_ARG(0, 1);
						if (p > w-cursorx) VTERMLOG("DCH: sc1 !!!");
						if (p > w-cursorx) p = w-cursorx;
						if (p < 0) p = 0;
						BufferedChar *b = mDisplay->buf+w*cursory;
						memmove(b+cursorx, b+cursorx+p, sizeof (BufferedChar) * (w-cursorx-p));
						mDisplay->fillVT(w-p, cursory, p, 1, mColor, ' ');
						mState = PLAIN;
						break;
					}
					case 'X': {
						// ECH - erase n chars (from cursor)
						VTERMLOG("ECH - erase n character(s)\n");
						int p = CSI_ARG(0, 1);
						if (p > w-cursorx) VTERMLOG("ECH: sc1 !!!\n");
						if (p > w-cursorx) p = w-cursorx;
						if (p < 0) p = 0;
						mDisplay->fillVT(cursorx, cursory, p, 1, mColor, ' ');
						mState = PLAIN;
						break;
					}
					case '@': {
						// ICH - insert (blank) character(s)
						VTERMLOG("ICH - insert (blank) character(s)\n");
						int p = CSI_ARG(0, 1);
						if (p > w-cursorx) VTERMLOG("ICH: sc1 !!!");
						if (p > w-cursorx) p = w-cursorx;
						if (p < 0) p = 0;
						BufferedChar *b = mDisplay->buf+w*cursory;
						memmove(b+cursorx+p, b+cursorx, sizeof (BufferedChar) * (w-cursorx-p));
						mDisplay->fillVT(cursorx, cursory, p, 1, mColor, ' ');
						mState = PLAIN;
						break;
					}
					case 'r': {
						// set scroll region
						VTERMLOG("DECSTBM - set scroll region\n");
						int r1 = CSI_ARG(0, 1);
						int r2 = CSI_ARG(1, h);
						if ((r1 > 0) && (r1 < r2) && (r2 <= h)) {
							mTop = r1-1;
							mBottom = r2;
							gotoAbsXY(0, 0);
						}
						mState = PLAIN;
						break;
					}
					case 's': {
						// save cursor
						VTERMLOG("? - save cursor\n");
						saveCursor();
						mState = PLAIN;
						break;
					}
					case 'u': {
						// restore cursor
						VTERMLOG("? - restore cursor\n");
						restoreCursor();
						mState = PLAIN;
						break;
					}
					case 'm': {
						// SGR - select graphic rendition
						VTERMLOG("SGR - set graphic rendition\n");
						for (int k=0; k<n; k++) {
							setGraphicRendition(CSI_ARG(k, 0));
						}
						if (n == 0) setGraphicRendition(0);
						mState = PLAIN;
						break;
					}
					case 'h':
					case 'l': {
						bool newValue = (c=='h');
						int p = CSI_ARG(0, -1);
						if (mState == CSI) {
							setMode(p, newValue);
						} else {
							setPrivateMode(p, newValue);
						}
						mState = PLAIN;
						break;
					}
					case '?': {
						mState = CSI_QM;
						break;
					}
					default:
						VTERMLOG("unsupported CSI-sequence '%c' (", c);
						for (int k=0; k<n; k++) {
							VTERMLOG("%d", Pn[k]);
							if (k+1<n) VTERMLOG(", ");
						}
						VTERMLOG(")\n");
						mState = PLAIN;
				}
				VTERMLOG("(");
				for (int k=0; k<n; k++) {
					VTERMLOG("%d", Pn[k]);
					if (k+1<n) VTERMLOG(", ");
				}
				VTERMLOG("), cxy=%d,%d\n", cursorx, cursory);
				break;
			}
#undef CSI_ARG
			case OSC: {
				int p = 0;
				while ((i< buflen) && isdigit(c)) {
					p *= 10;
					p += c-'0';
					c = buf[++i];
				}
				// skip ';'
				c = buf[++i];
				while ((i<buflen) && (c>=32)) {
					c = buf[++i];
				}
				if (i >= buflen) break;
				switch (p) {
					case 0: {
						VTERMLOG("OSC: set text params\n");
						VTERMLOG("%c%c%c%c%c%c\n", buf[start], buf[start+1], buf[start+2], buf[start+3], buf[start+4], buf[start+5]);
						mState = PLAIN;
						break;
					}
					default:
						VTERMLOG("unsupported OSC-sequence no. %d\n", p);
						mState = PLAIN;
				}
				break;
			}
		}
		if (i >= buflen) {
			memmove(mTermWriteBuf, mTermWriteBuf+last_seq_start, buflen-last_seq_start);
			mTermWriteBufLen = buflen-last_seq_start;
			break;
		}
	}
}

static int vcToAnsi(char *buf, vc color, bool fg)
{
	vc ctab[8] = { VC_BLACK, VC_RED, VC_GREEN, VC_YELLOW, VC_BLUE, VC_MAGENTA, VC_CYAN, VC_WHITE };
	int ansi = -1;
	for (int i=0; i<8; i++) if (ctab[i] == VC_GET_BASECOLOR(color)) {
		ansi = i;
		break;
	}
	if (ansi != -1) {
		bool ansibold = VC_GET_LIGHT(color);
		if (ansibold) {
			return sprintf(buf, "%d;1", (fg?0:10)+30+ansi);
		} else {
			return sprintf(buf, "%d", (fg?0:10)+30+ansi);
		}
	}
	return 0;
}

void vcpToAnsi(char *buf32, vcp color)
{
	int i = 0;
	i += sprintf(buf32+i, "\e[");
	int fga = vcToAnsi(buf32+i, VCP_FOREGROUND(color), true);
	i += fga;
	if (fga) i += sprintf(buf32+i, ";");
	int bga = vcToAnsi(buf32+i, VCP_BACKGROUND(color), false);
	i += bga;
	if (!bga && !fga) {
		*buf32 = 0;
		return;
	}
	if (!bga) i--;
	i += sprintf(buf32+i, "m");
}

