/* 
 *	PearPC
 *	sysbeos.cc
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf (stefan@weyergraf.de)
 *	Copyright (C) 1999-2004 Sebastian Biallas (sb@biallas.net)
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

#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <cstring>

#include <AppDefs.h>
#include <Application.h>
#include <Bitmap.h>
#include <Cursor.h>
#include <GraphicsDefs.h>
#include <InterfaceDefs.h>
#include <List.h>
#include <Locker.h>
#include <Message.h>
#include <Screen.h>
#include <View.h>
#include <Window.h>
#include <WindowScreen.h>

#include "system/display.h"
#include "system/keyboard.h"
#include "system/mouse.h"
#include "system/types.h"
#include "system/systhread.h"
#include "system/sysexcept.h"
#include "tools/data.h"
#include "tools/snprintf.h"

// for stopping the CPU
#include "cpu/cpu.h"

#include "sysbeos.h"

//#define DPRINTF(a...)
#define DPRINTF(a...) ht_printf(a)

//XXX:DEL uint gDamageAreaFirstAddr, gDamageAreaLastAddr;

/*XXX:DEL
struct {
	uint64 r_mask;
	uint64 g_mask;
	uint64 b_mask;
} PACKED gPosixRGBMask;*/

//extern "C" void __attribute__((regparm (3))) posix_vaccel_15_to_15(uint32 pixel, byte *input, byte *output);
//extern "C" void __attribute__((regparm (3))) posix_vaccel_15_to_32(uint32 pixel, byte *input, byte *output);

static uint8 beos_key_to_adb_key[] = {
/* ESC F1-F12                                                                   PRTSCR SLOCK */
0x35,    0x7a,0x78,0x63,0x76,0x60,0x61,0x62,0x64,0x65,0x6d,0x67,0x6f,           0xff,  0x6b,0x71,
/*   `   1-0                                                  -   =    BSPACE   INS  HOME P_UP         NLOCK /   *    -    */
    0x32,0x12,0x13,0x14,0x15,0x17,0x16,0x1a,0x1c,0x19,0x1d,0x1b,0x18,  0x33,    0x72,0x73,0x74,        0x47,0x4b,0x43,0x4e,
/* TAB   qwerty...                                                     \        DEL  END  P_DN         7    8    9    +    */
0x30,    0x0c,0x0d,0x0e,0x0f,0x11,0x10,0x20,0x22,0x1f,0x23,0x21,0x1e,  0x2a,    0x75,0x77,0x79,        0x59,0x5b,0x5c,0x45,
/* CLOCK ...                                                           ENTR                            4    5    6         */
0x39,0x00,0x01,0x02,0x03,0x05,0x04,0x26,0x28,0x25,0x29,0x27,           0x24,                           0x56,0x57,0x58,/*107*/
/* SHIFT ...                                                           SHIFT         UP                1    2    3    ENTR */
0x38,      0x06,0x07,0x08,0x09,0x0b,0x2d,0x2e,0x2b,0x2f,0x2c,          0x38,         0x3e,             0x53,0x54,0x55,0x4c,
/* CTRL /WIN/   ALT  SPACE ALTGR  /WIN MENU/   CTRL                           LEFT   DOWN   RIGHT      0         DEL       */
   0x36,/*0,*/ 0x37, 0x31, 0x3a,  /*0,  0,*/   0x36,                          0x3b,  0x3d,  0x3c,      0x52,     0x41


};



SDWindow::SDWindow(BRect frame, const char *name)
	: BWindow(frame, name, B_TITLED_WINDOW, B_NOT_RESIZABLE|B_NOT_ZOOMABLE)
{
}

SDWindow::~SDWindow()
{
}

bool SDWindow::QuitRequested()
{
	ppc_cpu_stop();
	/*
	  SDView *view = dynamic_cast<SDView *>(FindView("framebuffer"));
	  
	  if (view) {
	  BMessage *msg = new BMessage(B_QUIT_REQUESTED);
	  view->QueueMessage(msg);
	  }
	*/
	return false;
}

void SDWindow::Show()
{
	BWindow::Show();
	//gDisplay->setExposed(!IsMinimized() && !IsHidden());
	//gDisplay->setExposed(true);
}

void SDWindow::Hide()
{
	BWindow::Hide();
	//gDisplay->setExposed(!IsMinimized() && !IsHidden());
}

void SDWindow::Minimize(bool minimize)
{
	BWindow::Minimize(minimize);
	gDisplay->setExposed(!minimize && !IsHidden());
}

SDView::SDView(BeOSSystemDisplay *sd, BRect frame, const char *name)
	: BView(frame, name, B_FOLLOW_ALL_SIDES, B_PULSE_NEEDED|B_WILL_DRAW)
{
	fSystemDisplay = sd;
	sd->fbBitmap;
	fMsgList.MakeEmpty();
	//SetViewColor(0,255,0);
	SetViewColor(B_TRANSPARENT_32_BIT);
	fMsgSem = create_sem(1, "PearPC MessageList sem");
	// here we don't use the View's looper to lock the msg list,
	// so the window thread and main thread aren't interleaved 
	// just because they are playing with the msg list.
	// much better on my dual :))
	fMsgListLock = new BLocker("PearPC MessageList lock", true);
}

SDView::~SDView()
{
	delete_sem(fMsgSem);
	delete fMsgListLock;
}

void SDView::MessageReceived(BMessage *msg)
{
	BMessage *event;
	switch (msg->what) {
	case B_UNMAPPED_KEY_DOWN:
	case B_UNMAPPED_KEY_UP:
		event = Window()->DetachCurrentMessage();
		QueueMessage(event);
		return;
	}
	BView::MessageReceived(msg);
}

void SDView::Draw(BRect updateRect)
{
	//fSystemDisplay->convertDisplayClientToServer();
#ifdef BMP_MENU
	BRect r(updateRect);
	r.bottom = MIN(fSystemDisplay->mMenuHeight-1, r.bottom);
	if (fSystemDisplay->fMenuBitmap && (r.top <= fSystemDisplay->mMenuHeight))
		DrawBitmap(fSystemDisplay->fMenuBitmap, r, r);
	BRect src(updateRect);
	src.OffsetBySelf(0,-fSystemDisplay->mMenuHeight);
	//src.top = MAX(0, src.top);
	DrawBitmap(fSystemDisplay->fbBitmap, src, updateRect);
#else
	DrawBitmap(fSystemDisplay->fbBitmap, updateRect, updateRect);
#endif
	if (fSystemDisplay->mHWCursorVisible) {
		//XPutImage(gXDisplay, gXWindow, gGC, gMouseXImage, 0, 0, 
		//	mHWCursorX, mHWCursorY, 2, 2);
	}
}

void SDView::MouseDown(BPoint where)
{
	BMessage *event = Window()->CurrentMessage();
	SystemEvent ev;
	int32 buttons;
	ev.type = sysevMouse;
	//if (!mMouseEnabled) break;
	if (event->FindInt32("buttons", (int32 *)&buttons) < B_OK)
		buttons = 0;
	ev.mouse.type = sme_buttonPressed;
	ev.mouse.x = gDisplay->mCurMouseX;
	ev.mouse.y = gDisplay->mCurMouseY;
	ev.mouse.relx = 0;
	ev.mouse.rely = 0;
	ev.mouse.button1 = (buttons & B_PRIMARY_MOUSE_BUTTON) != 0;
	ev.mouse.button2 = (buttons & B_SECONDARY_MOUSE_BUTTON) != 0;
	ev.mouse.button3 = (buttons & B_TERTIARY_MOUSE_BUTTON) != 0;
	
	gMouse->handleEvent(ev);
}

void SDView::MouseUp(BPoint where)
{
	BMessage *event = Window()->CurrentMessage();
	SystemEvent ev;
	int32 buttons;
	ev.type = sysevMouse;
	//if (!mMouseEnabled) break;
	if (event->FindInt32("buttons", (int32 *)&buttons) < B_OK)
		buttons = 0;
	ev.mouse.type = sme_buttonReleased;
	ev.mouse.x = gDisplay->mCurMouseX;
	ev.mouse.y = gDisplay->mCurMouseY;
	ev.mouse.relx = 0;
	ev.mouse.rely = 0;
	ev.mouse.button1 = (buttons & B_PRIMARY_MOUSE_BUTTON) != 0;
	ev.mouse.button2 = (buttons & B_SECONDARY_MOUSE_BUTTON) != 0;
	ev.mouse.button3 = (buttons & B_TERTIARY_MOUSE_BUTTON) != 0;
	
	gMouse->handleEvent(ev);
}

void SDView::MouseMoved(BPoint where, uint32 code, const BMessage *a_message)
{
	BMessage *event = Window()->CurrentMessage();
	SystemEvent ev;
	int32 buttons;
	if (code == B_OUTSIDE_VIEW)
		return;
	if (event->FindInt32("buttons", (int32 *)&buttons) < B_OK)
		buttons = 0;
	gDisplay->mCurMouseX = ev.mouse.x = (int)where.x;
	gDisplay->mCurMouseY = ev.mouse.y = (int)where.y;
	if (gDisplay->mCurMouseX == gDisplay->mHomeMouseX && gDisplay->mCurMouseY == gDisplay->mHomeMouseY) return;
	if (gDisplay->mCurMouseX == -1) return;
	ev.type = sysevMouse;
	ev.mouse.type = sme_motionNotify;
	ev.mouse.button1 = (buttons & B_PRIMARY_MOUSE_BUTTON) != 0;
	ev.mouse.button2 = (buttons & B_SECONDARY_MOUSE_BUTTON) != 0;
	ev.mouse.button3 = (buttons & B_TERTIARY_MOUSE_BUTTON) != 0;
	ev.mouse.dbutton = 0;
	ev.mouse.relx = gDisplay->mCurMouseX - gDisplay->mHomeMouseX;
	ev.mouse.rely = gDisplay->mCurMouseY - gDisplay->mHomeMouseY;
	if (gDisplay->isMouseGrabbed()) {
		BPoint p(gDisplay->mHomeMouseX, gDisplay->mHomeMouseY);
		//printf("nukemouse %f %f\n", p.x, p.y);
		ConvertToScreen(&p);
		//printf("nukemouses %f %f\n", p.x, p.y);
		set_mouse_position((int32)p.x, (int32)p.y);
	}
	if (code == B_EXITED_VIEW) {
		if (gDisplay->isMouseGrabbed()) gDisplay->setMouseGrab(false);
	}
	gMouse->handleEvent(ev);
}

void SDView::KeyDown(const char *bytes, int32 numBytes)
{
	BMessage *event = Window()->CurrentMessage();
	char buffer[4];
	uint32 modifiers;
	int32 key;
	int32 raw_char;
	SystemEvent ev;
	if (event->FindInt32("key", &key) < B_OK)
		key = 0;
	if (event->FindInt32("raw_char", &raw_char) < B_OK)
		raw_char = 0;
	if (!key || (key > 255)) return;
	ev.key.keycode = beos_key_to_adb_key[key-1];
	DPRINTF("keys[%d-1]=%d, 0x%x\n", key, beos_key_to_adb_key[key-1], beos_key_to_adb_key[key-1]);
	if (ev.key.keycode == KEY_F12) return;
	if ((ev.key.keycode & 0xff) == 0xff) return;
	ev.type = sysevKey;
	ev.key.pressed = true;
	ev.key.chr = (char)raw_char;
	gKeyboard->handleEvent(ev);
}

void SDView::KeyUp(const char *bytes, int32 numBytes)
{
	BMessage *event = Window()->CurrentMessage();
	char buffer[4];
	uint32 modifiers;
	int32 key;
	int32 raw_char;
	SystemEvent ev;
	if (event->FindInt32("key", &key) < B_OK)
		key = 0;
	if (event->FindInt32("raw_char", &raw_char) < B_OK)
		raw_char = 0;
	if (!key || (key > 255)) return;
	ev.key.keycode = beos_key_to_adb_key[key-1];
	DPRINTF("keys[%d-1]=%d, 0x%x\n", key, beos_key_to_adb_key[key-1], beos_key_to_adb_key[key-1]);
	if (ev.key.keycode == KEY_F12) return;
	if ((ev.key.keycode & 0xff) == 0xff) return;
	ev.type = sysevKey;
	ev.key.pressed = false;
	ev.key.chr = (char)raw_char;
	gKeyboard->handleEvent(ev);
}

void SDView::Pulse()
{
	BWindow *w = Window();
	if (w && !w->IsHidden() && !w->IsMinimized())
		fSystemDisplay->displayShow();
	//Invalidate(Bounds()); /* cause a redraw */
}

void SDView::QueueMessage(BMessage *msg)
{
	fMsgListLock->Lock(); /* BList not threadsafe */
	fMsgList.AddItem(msg, fMsgList.CountItems());
	fMsgListLock->Unlock();
	release_sem(fMsgSem);
}

BMessage *SDView::UnqueueMessage(bool sync)
{
	BMessage *msg;
	acquire_sem_etc(fMsgSem, 1, sync?0:B_RELATIVE_TIMEOUT, 0LL);
	//LockLooper();
	fMsgListLock->Lock(); /* BList not threadsafe */
	msg = (BMessage *)fMsgList.RemoveItem(0L);
	//UnlockLooper();
	fMsgListLock->Unlock();
/*	if (msg)
		msg->PrintToStream();*/
	return msg;
}

extern SystemDisplay *allocSystemDisplay(const char *title, const DisplayCharacteristics &chr, int redraw_ms);
extern SystemMouse *allocSystemMouse();
extern SystemKeyboard *allocSystemKeyboard();


void initUI(const char *title, const DisplayCharacteristics &aCharacteristics, int redraw_ms, KeyboardCharacteristics const &keyConfig, bool fullscreen)
{
	gDisplay = allocSystemDisplay(title, aCharacteristics, redraw_ms);
	gMouse = allocSystemMouse();
	gKeyboard = allocSystemKeyboard();
	if(!gKeyboard->setKeyConfig(keyConfig)) {
		ht_printf("no keyConfig, or is empty");
		exit(1);
	}
	gDisplay->updateTitle();
}

void doneUI()
{
	delete gDisplay;
	delete gKeyboard;
	delete gMouse;
}
