/*
 *	PearPC
 *	sysvaccel.h
 *
 *	Abstraction for video conversion function acceleration
 *
 *	Copyright (C) 2004 Stefan Weyergraf
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

#ifndef __SYSVACCEL_H__
#define __SYSVACCEL_H__

#include "system/display.h"

void sys_convert_display(
	const DisplayCharacteristics &aSrcChar,
	const DisplayCharacteristics &aDestChar,
	const void *aSrcBuf,
	void *aDestBuf,
	int firstLine,
	int lastLine);

#endif
