AUTOMAKE_OPTIONS = foreign

if USE_UI_BEOS
THE_UI_DIR=beos
endif

if USE_UI_QT
THE_UI_DIR=qt
endif

if USE_UI_GTK
THE_UI_DIR=gtk
endif

if USE_UI_WIN32
THE_UI_DIR=win32
endif

if USE_UI_SDL
THE_UI_DIR=sdl
endif

if USE_UI_X11
THE_UI_DIR=x11
endif

EXTRA_DIST = gui.h

SUBDIRS = $(THE_UI_DIR)
EXTRA_SUBDIRS = beos qt gtk win32 sdl x11

INCLUDES = -I ../..
