AUTOMAKE_OPTIONS = foreign

SUFFIXES = .rc .o

noinst_LIBRARIES = libui.a

libui_a_SOURCES = gui.cc sysdisplay.cc syswin.cc syswin.h syskeyboard.cc \
sysmouse.cc resources.h resources.rc

EXTRA_DIST = ppc.ico

INCLUDES = -I ../../..

.rc.o:
	windres -F pe-i386 $(INCLUDES) -i $< -o $@
