/*
 *	PearPC
 *	sysbeos.h
 *
 *	Copyright (C) 2004 Stefan Weyergraf (stefan@weyergraf.de)
 *	Copyright (C) 2004 Francois Revol (revol@free.fr)
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

#ifndef __SYSBEOS_H__
#define __SYSBEOS_H__

//#define BMP_MENU


class BeOSSystemDisplay;
class SDView;

class SDWindow : public BWindow {
public:
	SDWindow(BRect frame, const char *name);
	~SDWindow();
virtual bool	QuitRequested();
virtual void	Show();
virtual void	Hide();
virtual void	Minimize(bool minimize);
};

class SDView : public BView {
public:
	SDView(BeOSSystemDisplay *sd, BRect frame, const char *name);
	~SDView();
	
virtual void	MessageReceived(BMessage *msg);
virtual void	Draw(BRect updateRect);
virtual void	MouseDown(BPoint where);
virtual void	MouseUp(BPoint where);
virtual void	MouseMoved(BPoint where, uint32 code, const BMessage *a_message);
virtual void	KeyDown(const char *bytes, int32 numBytes);
virtual void	KeyUp(const char *bytes, int32 numBytes);
virtual void	Pulse();
	
	void	QueueMessage(BMessage *msg);
	BMessage	*UnqueueMessage(bool sync);
private:
	BList	fMsgList;
	BLocker	*fMsgListLock;
	BeOSSystemDisplay	*fSystemDisplay;
	BBitmap	*fFramebuffer;
	sem_id	fMsgSem;
};


class BeOSSystemDisplay: public SystemDisplay
{
friend class SDView;
	Queue *mEventQueue;
	DisplayCharacteristics mBeChar;
	int mLastMouseX, mLastMouseY;
	//int mCurMouseX, mCurMouseY;
	//int mResetMouseX, mResetMouseY;
	//int mHomeMouseX, mHomeMouseY;
	bool mMouseButton[3];
	bool mMouseEnabled;
	char *mTitle;
	char mCurTitle[200];
	byte *mouseData;
	byte *menuData;
	/*  */
	sys_thread redrawthread;
	sys_mutex mutex;
	BBitmap *fbBitmap;
	BBitmap *fMenuBitmap;
	SDView *view;
	SDWindow *window;
	
	void dumpDisplayChar(const DisplayCharacteristics &chr);
	uint bitsPerPixelToXBitmapPad(uint bitsPerPixel);
public:
	BeOSSystemDisplay(const char *name, const DisplayCharacteristics &chr, int redraw_ms);
	virtual ~BeOSSystemDisplay();
	virtual	void finishMenu();
	virtual void convertCharacteristicsToHost(DisplayCharacteristics &aHostChar, const DisplayCharacteristics &aClientChar);
	virtual bool changeResolution(const DisplayCharacteristics &aCharacteristics);
	virtual void getHostCharacteristics(Container &modes);
	int getKeybLEDs();
	void setKeybLEDs(int leds);
	void updateTitle();
	virtual	int toString(char *buf, int buflen) const;
	virtual void setMouseGrab(bool enable);
	virtual void displayShow();
	void convertDisplayClientToServer(uint firstLine, uint lastLine);
	//virtual void queueEvent(DisplayEvent &ev);
	virtual void startRedrawThread(int msec);
};

/* implementation */

#endif
