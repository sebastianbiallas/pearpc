AUTOMAKE_OPTIONS = foreign

noinst_LIBRARIES = libsystem.a
libsystem_a_SOURCES = display.cc display.h fileofs.h \
sysexcept.cc sysexcept.h sys.cc sys.h types.h file.cc file.h \
vt100.cc vt100.h font.cc font.h gif.cc gif.h systhread.h systimer.h \
sysethtun.h sysfeatures.h keyboard.cc keyboard.h event.h device.cc device.h

SUBDIRS = arch osapi ui

INCLUDES = -I ..
