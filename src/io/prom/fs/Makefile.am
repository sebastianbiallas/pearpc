AUTOMAKE_OPTIONS = foreign

noinst_LIBRARIES = libfs.a

SUBDIRS = hfs hfsplus

libfs_a_SOURCES = part.cc part.h hfs.cc hfs.h hfsglue.h hfsplus.cc hfsplus.h \
hfsplusglue.h hfsstruct.h fs.cc fs.h

INCLUDES = -I ../../..