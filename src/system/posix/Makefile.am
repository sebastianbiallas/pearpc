AUTOMAKE_OPTIONS = foreign

#SUFFIXES = .asm .o

noinst_LIBRARIES = libssystem.a
libssystem_a_SOURCES = sysclipboard.cc sysdisplay.cc sysfile.cc \
sysinit.cc sysethtun.cc systhread.cc types.h
#vaccel.asm

INCLUDES = -I ../.. @X_CFLAGS@

.asm.o:
	nasm -O2 @NASM_OPTS@ -o $@ -- $<
