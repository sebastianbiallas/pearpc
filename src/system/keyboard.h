/*
 *	PearPC
 *	keyboard.h
 *
 *	Copyright (C) 2004 Stefan Weyergraf (stefan@weyergraf.de)
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

#ifndef __SYSTEM_KEYBOARD_H__
#define __SYSTEM_KEYBOARD_H__

#include "system/types.h"

#include "system/device.h"
#include "system/event.h"

#include "tools/data.h"

#define KEY_a		0
#define KEY_s		1
#define KEY_d		2
#define KEY_f		3
#define KEY_h		4
#define KEY_g		5
#define KEY_z		6
#define KEY_x		7
#define KEY_c		8
#define KEY_v		9

#define KEY_b		11
#define KEY_q		12
#define KEY_w		13
#define KEY_e		14
#define KEY_r		15
#define KEY_y		16
#define KEY_t		17
#define KEY_1		18
#define KEY_2		19
#define KEY_3		20
#define KEY_4		21
#define KEY_6		22
#define KEY_5		23
#define KEY_EQ		24
#define KEY_9		25
#define KEY_7		26
#define KEY_MINUS	27
#define KEY_8		28
#define KEY_0		29
#define KEY_BRACKET_R	30
#define KEY_o		31
#define KEY_u		32
#define KEY_BRACKET_L	33
#define KEY_i		34
#define KEY_p		35
#define KEY_RETURN	36
#define KEY_l		37
#define KEY_j		38
#define KEY_APOSTROPHE	39
#define KEY_k		40
#define KEY_SEMICOLON	41
#define KEY_BACKSLASH	42
#define KEY_COMMA	43
#define KEY_SLASH	44
#define KEY_n		45
#define KEY_m		46
#define KEY_PERIOD	47
#define KEY_TAB		48
#define KEY_SPACE	49
#define KEY_GRAVE	50
#define KEY_DELETE	51

#define KEY_ESCAPE	53
#define KEY_CONTROL	54
#define KEY_ALT		55
#define KEY_SHIFT	56
#define KEY_CAPS_LOCK	57
#define KEY_ALTGR	58
#define KEY_LEFT	59
#define KEY_RIGHT	60
#define KEY_DOWN	61
#define KEY_UP		62

#define KEY_KP_PERIOD	65
#define KEY_KP_MULTIPLY	67
#define KEY_KP_ADD	69
#define KEY_NUM_LOCK	71
#define KEY_KP_DIVIDE	75
#define KEY_KP_ENTER	76
#define KEY_KP_SUBTRACT	78
#define KEY_KP_0	82
#define KEY_KP_1	83
#define KEY_KP_2	84
#define KEY_KP_3	85
#define KEY_KP_4	86
#define KEY_KP_5	87
#define KEY_KP_6	88
#define KEY_KP_7	89

#define KEY_KP_8	91
#define KEY_KP_9	92

#define KEY_F5		96
#define KEY_F6		97
#define KEY_F7		98
#define KEY_F3		99
#define KEY_F8		100
#define KEY_F9		101

#define KEY_F11		103

#define KEY_F13		105

#define KEY_SCROLL_LOCK	107

#define KEY_F10		109

#define KEY_F12		111

#define KEY_PAUSE	113
#define KEY_INSERT	114
#define KEY_HOME	115
#define KEY_PRIOR	116
#define KEY_REMOVE	117

#define KEY_F4		118
#define KEY_END		119
#define KEY_F2		120
#define KEY_NEXT	121
#define KEY_F1		122

//keycode 0x7b = Shift
//keycode 0x7c = AltGr
//keycode 0x7d = Control


#define KEYB_LED_NUM	1
#define KEYB_LED_CAPS	2
#define KEYB_LED_SCROLL 3

#define KEYCODE_CTRL    0x100
#define KEYCODE_LALT	0x200
#define KEYCODE_RALT	0x400
#define KEYCODE_SHIFT	0x800

/* system-dependent (implementation in ui / $MYUI / *.cc) */
class SystemKeyboard: public SystemDevice {
	int mShift;
	int mLAlt;
	int mRAlt;
	int mCtrl;
public:
			SystemKeyboard();
	virtual int	getKeybLEDs() = 0;
	virtual void	setKeybLEDs(int leds) = 0;
	virtual bool	handleEvent(const SystemEvent &ev);
	static	bool	convertKeycodeToString(String &result, int keycode);
	static	bool	convertStringToKeycode(int &keycode, const String &s);
	static	bool	adbKeyToAscii(char &chr, int adbcode);
};

/* system-independent (implementation in keyboard.cc) */
extern SystemKeyboard *gKeyboard;

#endif /* __SYSTEM_KEYBOARD_H__ */
