AUTOMAKE_OPTIONS = foreign

EXTRA_DIST = ifppc_up.setuid.cc ifppc_down.setuid.cc

if USE_SYSTEM_POSIX
noinst_PROGRAMS = ifppc_down.setuid ifppc_up.setuid
ifppc_up_setuid_SOURCES = ifppc_up.setuid.cc
ifppc_down_setuid_SOURCES = ifppc_down.setuid.cc
endif
