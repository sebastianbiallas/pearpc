/*
 *	PearPC
 *	device.h
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

#ifndef __SYSTEM_DEVICE_H__
#define __SYSTEM_DEVICE_H__

#include "tools/data.h"
#include "system/event.h"

class SystemDevice: public Object {
protected:
	SystemEventHandler	mAttachedEventHandler;
public:
	SystemDevice();

	/**
	 *	@returns	true if the event has been handled, false otherwise
	 */
	virtual bool	handleEvent(const SystemEvent &ev);

	virtual void	attachEventHandler(SystemEventHandler cevh);
};

#endif /* __SYSTEM_DEVICE_H__ */
