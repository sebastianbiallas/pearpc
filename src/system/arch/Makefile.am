AUTOMAKE_OPTIONS = foreign

if USE_ARCH_GENERIC
THE_ARCH_DIR=generic
endif

if USE_ARCH_X86
THE_ARCH_DIR=x86
endif

if USE_ARCH_X86_64
THE_ARCH_DIR=x86_64
endif

EXTRA_DIST = sysfeatures.h sysendian.h

SUBDIRS = $(THE_ARCH_DIR)
EXTRA_SUBDIRS = generic x86 x86_64

AM_CPPFLAGS = -I ../..
