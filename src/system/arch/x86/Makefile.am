AUTOMAKE_OPTIONS = foreign

SUFFIXES = .asm .o

noinst_LIBRARIES = libsarch.a
libsarch_a_SOURCES = sysfeatures.h sysvaccel.cc vaccel.asm

INCLUDES = -I ../../..

.asm.o:
	nasm -O99 @NASM_OPTS@ -o $@ -- $<
