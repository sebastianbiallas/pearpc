AUTOMAKE_OPTIONS = foreign

if USE_ARCH_GENERIC
THE_ARCH_DIR=generic
endif

if USE_ARCH_X86
THE_ARCH_DIR=x86
endif

noinst_LIBRARIES = libsystemarch.a
libsystemarch_a_SOURCES = sysfeatures.h sysendian.h

SUBDIRS = $(THE_ARCH_DIR)
EXTRA_SUBDIRS = generic x86

INCLUDES = -I ../..
