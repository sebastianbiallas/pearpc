/* 
 *	HT Editor
 *	vt100.h - VT100/102 emulator
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

#ifndef __VT100_H__
#define __VT100_H__

#include "system/display.h"

typedef int vcp;

#define VCP(vc_fg, vc_bg) (vcp)((vc_bg) | ((vc_fg)<<8))
#define VCP_BACKGROUND(v) ((v) & 0xff)
#define VCP_FOREGROUND(v) ((v>>8) & 0xff)

enum VT100state { PLAIN, ESC, CSI, CSI_QM, OSC };

class VT100Display: public Object {
protected:
	int x,y,w,h;
	int cursorx, cursory;
	
	VT100state mState;
	vcp mColor;
	vcp mDefaultColor;
	bool mColorLight;	// extra light info (just for setGraphicRendition)
	int mG;
	bool mDECCKM;		// cursor keys send ^[Ox/^[[x
	bool mDECAWM;		// auto-wrap mode
	bool mDECOM;		// origion relative/absolute
	bool mLNM;		// automatic newline (enter = crlf/lf)
	bool mIM;
	int mTop;
	int mBottom;
	char mTermWriteBuf[32768];
	int mTermWriteBufLen;
	int mSaved_cursorx, mSaved_cursory;
	
	SystemDisplay *mDisplay;
	
	/* new */
	void doLF();
	void doCR();
	void doBS();
	void doRI();
	void gotoXY(int ncx, int ncy);
	void gotoAbsXY(int ncx, int ncy);
	void saveCursor();
	void restoreCursor();
	void scrollDown(int mTop, int mBottom, int count);
	void scrollUp(int mTop, int mBottom, int count);
	void setGraphicRendition(int r);
	void setMode(int p, bool newValue);
	void setPrivateMode(int p, bool newValue);
	void termifyColor(int &mColor);
	void getCursor(int &x, int &y);
public:
		VT100Display(int width, int height, SystemDisplay *aDisplay, vcp initColor = VCP(VC_WHITE, VC_BLACK));
	/* extends BufferedRDisplay */
	virtual	void	setBounds(int width, int height);
	/* new */
		void	termWrite(const void *buf, int buflen);
		void	setAutoNewLine(bool b);
};

void vcpToAnsi(char *buf32, vcp color);

#endif /* __VT100_H__ */
