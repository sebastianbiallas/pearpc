AUTOMAKE_OPTIONS = foreign

if USE_SYSTEM_BEOS
THE_SYSTEM_DIR=beos
endif

if USE_SYSTEM_POSIX
THE_SYSTEM_DIR=posix
endif

if USE_SYSTEM_WIN32
THE_SYSTEM_DIR=win32
endif

noinst_LIBRARIES = libsystem.a
libsystem_a_SOURCES = display.cc display.h fileofs.h \
sysexcept.cc sysexcept.h sys.cc sys.h types.h file.cc file.h \
vt100.cc vt100.h font.cc font.h gif.cc gif.h systhread.h systimer.h \
sysethtun.h

SUBDIRS = $(THE_SYSTEM_DIR) gui
EXTRA_SUBDIRS = beos posix win32

INCLUDES = -I ..
