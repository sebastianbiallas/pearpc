/* 
 *	HT Editor
 *	sysvaccel.cc - generic implementation
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

#include "system/sysvaccel.h"

#include "tools/snprintf.h"

static inline void convertBaseColor(uint &b, uint fromBits, uint toBits)
{
	if (toBits > fromBits) {
		b <<= toBits - fromBits;
	} else {
		b >>= fromBits - toBits;
	}
}

void sys_convert_display(
	const DisplayCharacteristics &aSrcChar,
	const DisplayCharacteristics &aDestChar,
	const void *aSrcBuf,
	void *aDestBuf,
	int firstLine,
	int lastLine)
{
	byte *src = (byte*)aSrcBuf + aSrcChar.bytesPerPixel * aSrcChar.width * firstLine;
	byte *dest = (byte*)aDestBuf + aDestChar.bytesPerPixel * aDestChar.width * firstLine;
	for (int y=firstLine; y <= lastLine; y++) {
		for (int x=0; x < aSrcChar.width; x++) {
			uint r, g, b;
			uint p;
			switch (aSrcChar.bytesPerPixel) {
			case 2:
				p = (src[0] << 8) | src[1];
				break;
			case 4:
				p = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
				break;
			default:
				ht_printf("internal error in %s:%d\n", __FILE__, __LINE__);
				exit(1);
			break;
			}
			r = (p >> aSrcChar.redShift) & ((1<<aSrcChar.redSize)-1);
			g = (p >> aSrcChar.greenShift) & ((1<<aSrcChar.greenSize)-1);
			b = (p >> aSrcChar.blueShift) & ((1<<aSrcChar.blueSize)-1);
			convertBaseColor(r, aSrcChar.redSize, aDestChar.redSize);
			convertBaseColor(g, aSrcChar.greenSize, aDestChar.greenSize);
			convertBaseColor(b, aSrcChar.blueSize, aDestChar.blueSize);
			p = (r << aDestChar.redShift) | (g << aDestChar.greenShift)
				| (b << aDestChar.blueShift);
			switch (aDestChar.bytesPerPixel) {
			case 2:
				*(uint16*)dest = p;
				break;
			case 3:
				dest[0] = p; dest[1] = p>>8; dest[2] = p>>16;
				break;
			case 4:
				*(uint32*)dest = p;
				break;
			default:
				ht_printf("internal error in %s:%d\n", __FILE__, __LINE__);
				exit(1);
			}
			dest += aDestChar.bytesPerPixel;
			src += aSrcChar.bytesPerPixel;
		}
		dest += aDestChar.scanLineLength - aDestChar.width*aDestChar.bytesPerPixel;
		src += aSrcChar.scanLineLength - aSrcChar.width*aSrcChar.bytesPerPixel;
	}
}
