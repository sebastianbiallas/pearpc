AUTOMAKE_OPTIONS = foreign

if USE_CPU_GENERIC
THE_ARCH_DIR=cpu_generic
endif

if USE_CPU_JITC_X86
THE_ARCH_DIR=cpu_jitc_x86
endif

EXTRA_DIST = cpu.h debug.h mem.h

SUBDIRS = $(THE_ARCH_DIR)
EXTRA_SUBDIRS = cpu_jitc_generic cpu_jitc_x86

INCLUDES = -I ..
