AUTOMAKE_OPTIONS = foreign

noinst_LIBRARIES = libui.a

if USE_SYSTEM_POSIX
display_file = ../x11/sysdisplay.cc
endif

if USE_SYSTEM_WIN32
display_file = ../win32/sysdisplay.cc
endif

libui_a_SOURCES = gui.cc $display_file

INCLUDES = -I ../../.. -I/usr/include/gtk-2.0 -I/usr/include/glib-2.0 \
-I/usr/lib/glib-2.0/include -I/usr/include/pango-1.0 -I/usr/lib/gtk-2.0/include \
-I/usr/include/atk-1.0