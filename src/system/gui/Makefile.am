AUTOMAKE_OPTIONS = foreign

if USE_GUI_BEOS
THE_GUI_DIR=beos
endif

if USE_GUI_QT
THE_GUI_DIR=qt
endif

if USE_GUI_GTK
THE_GUI_DIR=gtk
endif

if USE_GUI_WIN32
THE_GUI_DIR=win32
endif

if USE_GUI_NOGUI
THE_GUI_DIR=nogui
endif

EXTRA_DIST = gui.h

SUBDIRS = $(THE_GUI_DIR)
EXTRA_SUBDIRS = beos qt gtk win32 nogui

INCLUDES = -I ../..
