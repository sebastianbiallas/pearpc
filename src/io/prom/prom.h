/*
 *	PearPC
 *	prom.cc
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

#ifndef __PROM_H__
#define __PROM_H__

#include "system/types.h"
#include "tools/stream.h"

enum PromBootMethod {
	prombmAuto,
	prombmSelect,
	prombmForce,
};

extern PromBootMethod gPromBootMethod;
extern String gPromBootPath;

void prom_init();
void prom_init_config();
void prom_done();
void prom_quiesce();

#endif
