/*
 *	PearPC
 *	ide.h
 *
 *	Copyright (C) 2003 Sebastian Biallas (sb@biallas.net)
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

#ifndef __IO_IDE_H__
#define __IO_IDE_H__

#include "system/types.h"
#include "system/display.h"
#include "idedevice.h"

/*
 *	IDE is handled by PCI and therefore has no base address
 */

enum IDEProtocol {
	IDE_ATA,
	IDE_ATAPI,
};

struct IDEConfig {
	bool installed;
	IDEProtocol protocol;
	bool lba;
	union {
		struct {
			int cyl;
			int heads;
			int spt;
		} hd;
		struct {
			struct {
				uint8 sense_key;
				uint8 info[4];
				uint8 spec_info[4];
				uint8 key_spec[3];
				uint8 fruc;
				uint8 asc;
				uint8 ascq;
			} sense;
			struct {
			        uint8 command;
			        int   drq_bytes;
			        int   total_remain;
			} atapi;
			int    next_lba;
			int    remain;
			bool   dma;
		} cdrom;
	};
	int bps;
	IDEDevice *device;
};

IDEConfig *ide_get_config(int disk);

void ide_init();
void ide_init_config();

#endif

