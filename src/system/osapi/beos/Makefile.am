AUTOMAKE_OPTIONS = foreign

noinst_LIBRARIES = libsosapi.a

libsosapi_a_SOURCES = sysclipboard.cc sysfile.cc \
syscdrom.cc sysethtun.cc sysinit.cc systhread.cc systimer.cc types.h

INCLUDES = -I ../../..

sysethtun.cc:
	ln -s ../posix/$@ $@
