/*
 *	PearPC
 *	sysfeatures.h
 *
 *	Stupid GCC: we can't name this file features.h...
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

#ifndef __SYSTEM_SYSFEATURES_H__
#define __SYSTEM_SYSFEATURES_H__

/*	We've got the following features:
 *	PPC_VIDEO_CONVERT_ACCEL_FEATURE		see sysvaccel.h
 */

// provider priorities
#define	PPC_FEATURE_NOT_PROVIDED		0
//#define	PPC_FEATURE_PROVIDED_BY_OSAPI		1
#define	PPC_FEATURE_PROVIDED_BY_ARCH		2

#include "arch/sysfeatures.h"
//#include "osapi/sysfeatures.h"

#endif
