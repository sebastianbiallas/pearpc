/* 
 *	HT Editor
 *	sysinit.cc - BeOS-specific initialization
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf
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

#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

#include <Application.h>
#include <OS.h>

#include "system/sys.h"

int32 bapp_thread(void *)
{
	be_app->Lock();
	be_app->Run();
}

bool initOSAPI()
{
	thread_id bapp_tid;
	setuid(getuid());
	new BApplication("application/x-vnd.PearPC");
	if (!be_app)
		return false;
	bapp_tid = spawn_thread(bapp_thread, "BApplication(PearPC)", B_NORMAL_PRIORITY, NULL);
	if (bapp_tid < B_OK)
		return false;
	if (resume_thread(bapp_tid) < B_OK)
		return false;
	be_app->Unlock();
	return true;
}

void doneOSAPI()
{
	be_app->Lock();
	be_app->Quit();
	delete be_app;
}
