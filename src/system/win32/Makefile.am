AUTOMAKE_OPTIONS = foreign

SUFFIXES = .asm .o

noinst_LIBRARIES = libssystem.a
libssystem_a_SOURCES = sysclipboard.cc sysdisplay.cc sysfile.cc systhread.cc \
sysethtun.cc sysinit.cc vaccel.asm types.h tap_constants.h

INCLUDES = -I ../..

.asm.o:
	nasm -O2 @NASM_OPTS@ -o $@ -- $<
