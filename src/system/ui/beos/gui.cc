/*
 *	PearPC
 *	gui.cc
 *
 *	Copyright (C) 2003 Sebastian Biallas (sb@biallas.net)
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

#include <Alert.h>
#include <Application.h>
#include <FilePanel.h>
#include "tools/data.h"
#include "system/ui/gui.h"

/*
class MyFilePanel : public BFilePanel {
public:
	MyFilePanel(
};
*/

void sys_gui_init()
{
	// BApplication is created in sysinit (needed for display & clipboard)
	//new BApplication("application/x-vnd.PearPC"); // no, BeOS doesn't look like a copy of Qt, it's Qt which is a copy of the BeAPI :p
}

bool sys_gui_open_file_dialog(String &ret, const String &title, const String &filespec, const String &filespecname, const String &home, bool existing)
{
/*	String f;
	f.assignFormat("%y (%y)", &filespecname, &filespec);
	QFileDialog* fd = new QFileDialog(home.contentChar(), f.contentChar(), NULL, title.contentChar(), FALSE);
	fd->setCaption(title.contentChar());
	if (existing) {
		fd->setMode(QFileDialog::ExistingFile);
	} else {
		fd->setMode(QFileDialog::AnyFile);
	}
	
	QString fileName;
	if (fd->exec() == QDialog::Accepted) {
    		fileName = fd->selectedFile();
		ret = (const char *)fileName;
		delete fd;
		return true;
	} else {
		delete fd;
		return false;
	}
*/
return false;
}

int sys_gui_messagebox(const String &title, const String &text, int buttons)
{
	BAlert *alert;
}
