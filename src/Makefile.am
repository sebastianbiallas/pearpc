AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS	= ppc

if USE_CPU_GENERIC
THE_CPU_DIR=cpu_generic
endif

if USE_CPU_JITC_X86
THE_CPU_DIR=cpu_jitc_x86
endif

SUBDIRS		= $(THE_CPU_DIR) debug io system tools
EXTRA_SUBDIRS   = cpu_generic cpu_jitc_x86

INCLUDES	=

ppc_LDADD	= @CPU_DIR@/libcpu.a io/graphic/libgraphic.a \
io/ide/libide.a system/ui/@UI_DIR@/libui.a system/arch/@ARCH_DIR@/libsarch.a \
debug/libdebug.a io/libio.a io/prom/libprom.a \
io/prom/fs/libfs.a io/prom/fs/hfs/libhfs.a io/prom/fs/hfsplus/libhfsplus.a \
io/pic/libpic.a io/pci/libpci.a io/macio/libmacio.a io/nvram/libnvram.a \
io/cuda/libcuda.a io/3c90x/lib3c90x.a io/rtl8139/librtl8139.a io/usb/libusb.a \
tools/libtools.a system/libsystem.a system/osapi/@OSAPI_DIR@/libsosapi.a @PPC_LDADD@

ppc_LDFLAGS	= @PPC_LDFLAGS@

ppc_SOURCES	= main.cc info.h ppc_img.c ppc_img.h \
ppc_font.c ppc_font.h ppc_button_changecd.c ppc_button_changecd.h \
configparser.cc configparser.h

dist2: distdir
	$(AMTAR) chof - $(distdir) | BZIP2=$(BZIP2_ENV) bzip2 -c >$(distdir).tar.bz2
	$(am__remove_distdir)

dist-all2: dist2
