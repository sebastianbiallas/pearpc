/*
 *	PearPC
 *	gui.cc
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

extern "C" {
#include <gtk/gtk.h>
}

#ifdef MIN
#undef MIN
#endif

#ifdef MAX
#undef MAX
#endif

#include "tools/data.h"
#include "system/ui/gui.h"

void sys_gui_init()
{
}

static void store_filename (GtkWidget *widget, gpointer user_data) 
{
	GtkWidget *file_selector = (GtkWidget *)user_data;
	const gchar *selected_filename;

	selected_filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION (file_selector));
}



bool sys_gui_open_file_dialog(String &ret, const String &title, const String &filespec, const String &filespecname, const String &home, bool existing)
{
	GtkWidget *file_selector;

	file_selector = gtk_file_selection_new(title.contentChar());
   
	g_signal_connect(GTK_OBJECT (GTK_FILE_SELECTION (file_selector)->ok_button),
		"clicked",
		G_CALLBACK (store_filename),
                (gpointer) file_selector);
      
	g_signal_connect_swapped(GTK_OBJECT (GTK_FILE_SELECTION (file_selector)->ok_button),
		"clicked",
                G_CALLBACK (gtk_widget_destroy), 
                (gpointer) file_selector); 

	g_signal_connect_swapped(GTK_OBJECT (GTK_FILE_SELECTION (file_selector)->cancel_button),
    		"clicked",
                G_CALLBACK (gtk_widget_destroy),
                (gpointer) file_selector); 
   
	gtk_widget_show(file_selector);
}

int sys_gui_messagebox(const String &title, const String &text, int buttons)
{
	
}
