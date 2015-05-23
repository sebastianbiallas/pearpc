dnl Process this file with autoconf to produce a configure script.
PACKAGE=pearpc
VERSION=0.6pre

AC_INIT(PACKAGE, VERSION)

dnl Check the system.
AC_CANONICAL_SYSTEM

AC_SUBST(PACKAGE)
AC_SUBST(VERSION)

AC_ARG_ENABLE(release,
	[  --enable-release        make an optimized build [default=yes]],,enable_release="yes")
AC_ARG_ENABLE(fpo,
	[  --enable-fpo            compile with -fomit-frame-pointer [default=yes]],,enable_fpo="yes")
AC_ARG_ENABLE(profiling,
	[  --enable-profiling      make a profile build (implies debug) [default=no]],,enable_profiling="no")
AC_ARG_ENABLE(debug,
	[  --enable-debug          make a debug build [default=yes]],,enable_debug="yes")
AC_ARG_ENABLE(cpu,
	[  --enable-cpu            choose cpu (generic, jitc_x86 or jitc_x86_64) [default=<YOUR-ARCH> or generic]],,enable_cpu="")
AC_ARG_ENABLE(ui,
	[  --enable-ui             choose ui (beos, gtk, qt, sdl, win32, or x11) [system-specific defaults]],,enable_ui="no")

PPC_CFLAGS="-Wundef -Wall -fsigned-char"
PPC_CXXFLAGS="-Wundef -Wall -Woverloaded-virtual -std=c++11 -fsigned-char"
PPC_LDADD=""
PPC_LDFLAGS="$LDFLAGS"
PPC_CCASFLAGS=""

if test "x$enable_release" = "xyes"; then
	PPC_CFLAGS="$CFLAGS $PPC_CFLAGS -O3"
	PPC_CXXFLAGS="$CXXFLAGS $PPC_CXXFLAGS -O3"
else
	PPC_CFLAGS="$PPC_CFLAGS -O0"
	PPC_CXXFLAGS="$PPC_CXXFLAGS -O0"
fi

if test "x$enable_fpo" = "xyes"; then
	PPC_CFLAGS="$PPC_CFLAGS -fomit-frame-pointer"
	PPC_CXXFLAGS="$PPC_CXXFLAGS -fomit-frame-pointer"
fi

if test "x$enable_profiling" = "xyes"; then
dnl	profiling implies debug
	enable_debug = "yes"
	PPC_CFLAGS="$PPC_CFLAGS -pg -g"
	PPC_CXXFLAGS="$PPC_CXXFLAGS -pg -g"
else
	if test "x$enable_debug" = "xyes"; then
		PPC_CFLAGS="$PPC_CFLAGS -g -fno-inline"
		PPC_CXXFLAGS="$PPC_CXXFLAGS -g -fno-inline"
	fi
fi

AC_CONFIG_SRCDIR([src/system/types.h])
AM_INIT_AUTOMAKE(no-define)

AC_CONFIG_HEADER(config.h)

dnl Check OS specifica

case "$target_os" in
linux* | gnu* | k*bsd*-gnu)
	OSAPI_DIR=posix
	if test "x$enable_ui" = "xno"; then
		enable_ui="x11"
	fi	
	AC_MSG_RESULT([*** GNU, building POSIX version. we need pthread and rt.])
	PPC_CFLAGS="$PPC_CFLAGS -pthread"
	PPC_CXXFLAGS="$PPC_CXXFLAGS -pthread"
	PPC_LDADD="$PPC_LDADD -lpthread -lrt"
;;
freebsd*) 
	OSAPI_DIR=posix
	if test "x$enable_ui" = "xno"; then
		enable_ui="x11"
	fi	
	AC_MSG_RESULT([*** FREEBSD, building native FreeBSD version. we need pthread.])
	PPC_LDFLAGS="$PPC_LDFLAGS -pthread"
;;
cygwin*)
	OSAPI_DIR=win32
	if test "x$enable_ui" = "xno"; then
		enable_ui="win32"
	fi	
	AC_MSG_RESULT([*** CYGWIN, building native Win32 version (without cygwin.dll)])
	PPC_CFLAGS="$PPC_CFLAGS -mno-cygwin"
	PPC_CXXFLAGS="$PPC_CXXFLAGS -mno-cygwin"
        PPC_LDADD="$PPC_LDADD -lgdi32 -lcomdlg32 -lwinmm"
	PPC_CCASFLAGS="-DPREFIX=_"
;;
mingw32*)
        OSAPI_DIR=win32
        if test "x$enable_ui" = "xno"; then
		enable_ui="win32"
	fi
	AC_MSG_RESULT([*** MinGW, building native Win32 version])
        PPC_LDADD="$PPC_LDADD -lgdi32 -lcomdlg32 -lwinmm"
	PPC_CCASFLAGS="-DPREFIX=_"
;;
darwin*)
	OSAPI_DIR=posix
	if test "x$enable_ui" = "xno"; then
		enable_ui="x11"
	fi
	AC_MSG_RESULT([*** DARWIN, building native Darwin version (X11)])
	PPC_CCASFLAGS="-DPREFIX=_"
	PPC_CFLAGS="$PPC_CFLAGS -mdynamic-no-pic"
	PPC_CXXFLAGS="$PPC_CXXFLAGS -mdynamic-no-pic"
;;
beos*)
	OSAPI_DIR=beos
	if test "x$enable_ui" = "xno"; then
		enable_ui="beos"
	fi
	AC_MSG_RESULT([*** BEOS, building native BeOS version])
        PPC_LDFLAGS="-lbe -lgame"
	PPC_CFLAGS="$PPC_CFLAGS -Wno-multichar"
	PPC_CXXFLAGS="$PPC_CXXFLAGS -Wno-multichar"
;;
haiku*)
	OSAPI_DIR=beos
	if test "x$enable_ui" = "xno"; then
		enable_ui="beos"
	fi
	AC_MSG_RESULT([*** HAIKU, building native Haiku version])
        PPC_LDFLAGS="-lbe -lgame"
	PPC_CFLAGS="$PPC_CFLAGS -Wno-multichar"
	PPC_CXXFLAGS="$PPC_CXXFLAGS -Wno-multichar"
;;
*) 
	AC_MSG_WARN([configure: warning: $target_os: unknown target OS, assuming POSIX-compatible...])
	OSAPI_DIR=posix
	AC_MSG_WARN([*** assuming POSIX OS, we need pthread])
	PPC_LDADD="$PPC_LDADD -lpthread"
;;	
esac

case "$enable_cpu" in
jitc_x86*)
	CPU_DIR="cpu_jitc_x86"
;;	
generic*)
	CPU_DIR="cpu_generic"
;;	
*)
	CPU_DIR=""
;;	
esac

case "$target" in
i?86*)
	AC_MSG_RESULT([*** x86 target system])
	ARCH_DIR=x86
	if test -z "$CPU_DIR"; then
		CPU_DIR="cpu_jitc_x86"
	fi
;;
x86_64*)
	AC_MSG_RESULT([*** x86_64 target system])
	ARCH_DIR=x86_64
	if test -z "$CPU_DIR"; then
		CPU_DIR="cpu_jitc_x86_64"
	fi
;;
*)
	ARCH_DIR=generic
	if test -z "$CPU_DIR"; then
		CPU_DIR="cpu_generic"
	fi
;;
esac

AC_DEFINE_UNQUOTED(SYSTEM_OSAPI_SPECIFIC_TYPES_HDR, "system/osapi/$OSAPI_DIR/types.h", [Location of system/osapi/$OSAPI_DIR/types.h])
AC_DEFINE_UNQUOTED(SYSTEM_ARCH_SPECIFIC_FEATURES_DIR, "system/arch/$ARCH_DIR/sysfeatures.h", [Location of system/arch/$ARCH_DIR/sysfeatures.h])
AC_DEFINE_UNQUOTED(SYSTEM_ARCH_SPECIFIC_ENDIAN_DIR, "system/arch/$ARCH_DIR/sysendian.h", [Location of system/arch/$ARCH_DIR/sysendian.h])

PPC_CFLAGS="$PPC_CFLAGS -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -pipe";
PPC_CXXFLAGS="$PPC_CXXFLAGS -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -pipe";

AM_CONDITIONAL(USE_OSAPI_BEOS, test x$OSAPI_DIR = xbeos)
AM_CONDITIONAL(USE_OSAPI_POSIX, test x$OSAPI_DIR = xposix)
AM_CONDITIONAL(USE_OSAPI_WIN32, test x$OSAPI_DIR = xwin32)

AM_CONDITIONAL(USE_CPU_GENERIC, test x$CPU_DIR = xcpu_generic)
AM_CONDITIONAL(USE_CPU_JITC_X86, test x$CPU_DIR = xcpu_jitc_x86)
AM_CONDITIONAL(USE_CPU_JITC_X86_64, test x$CPU_DIR = xcpu_jitc_x86_64)

dnl Checks for programs.
AC_PROG_RANLIB
AC_PATH_PROGS(MAKE, gmake make, :)
AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AM_PROG_AS

dnl To ensure backward compatibility, Automake's `AM_PROG_LEX' invokes
dnl (indirectly) this macro twice, which will cause an annoying but
dnl benign "`AC_PROG_LEX' invoked multiple times" warning.  Future
dnl versions of Automake will fix this issue; meanwhile, just ignore
dnl this message.
dnl --- GNU Autoconf (version 2.58, 18 March 2004)
AM_PROG_LEX

AC_PROG_YACC
AC_PATH_PROG(AR, ar)
AC_PATH_PROG(AR, gar)

AC_DEFINE(HOST_ENDIANESS_BE, 1234, [big-endian byte-order helper constant])
AC_DEFINE(HOST_ENDIANESS_LE, 4321, [little-endian byte-order helper constant])

dnl Figure out what version of the posix system timers to use
if test "x$OSAPI_DIR" = "xposix"; then
	AC_CHECK_LIB(rt, clock_settime,
	[
		AC_DEFINE(USE_POSIX_REALTIME_CLOCK, 1, [Prefer POSIX realtime clock API])
		PPC_LDADD="$PPC_LDADD -lrt"
		AC_EGREP_CPP(yes,
		[
			#include <signal.h>
			#ifdef SIGRTMIN
				   yes
			#endif
		], syssignal=SIGRTMIN, syssignal=SIGALRM
		)
	], [
		AC_CHECK_LIB(c, setitimer,
		[
			AC_DEFINE(USE_POSIX_SETITIMER, 1, [Use interval timer clock API])
			syssignal=SIGALRM
			PPC_LDADD="$PPC_LDADD"
		], [
			AC_MSG_ERROR([*** Unable to find timer mechanism, should have either POSIX realtime API or BSD interval timer API ***])
		]
		)
	]
	)
	AC_DEFINE_UNQUOTED(SYSTIMER_SIGNAL, $syssignal, [Which signal to use for clock timer])
fi	

AC_C_BIGENDIAN(
	AC_DEFINE(HOST_ENDIANESS, HOST_ENDIANESS_BE, [Which host endianess/byte-order?]),
 	AC_DEFINE(HOST_ENDIANESS, HOST_ENDIANESS_LE, [Which host endianess/byte-order?]),
	[
	AC_MSG_WARN([couldn't determine host endianess/byte-order. assuming your machine is little-endian.])
	AC_DEFINE(HOST_ENDIANESS, HOST_ENDIANESS_LE, [Which host endianess/byte-order?])
	]
)

dnl This non-sense is needed to work around the fact, that autoconf-2.13 provides
dnl a different AC_C_BIGENDIAN macro (without parameters) and doesn't moan when
dnl giving them anyway. GRRRRR. autoconf-2.5 works OK.
case "$ac_cv_c_bigendian" in
yes*)
	AC_DEFINE(HOST_ENDIANESS, HOST_ENDIANESS_BE, [Which host endianess/byte-order?])
;;
no*)
  	AC_DEFINE(HOST_ENDIANESS, HOST_ENDIANESS_LE, [Which host endianess/byte-order?])
;;
esac
dnl /non-sense

if test -z "$AR"; then
	AC_MSG_ERROR([*** 'ar' and 'gar' missing, please install one of them or fix your \$PATH ***])
fi

dnl Checks for libraries.

UI_DIR="$enable_ui"
case "$enable_ui" in
x11)
	AC_PATH_X
	AC_PATH_XTRA

	AC_CHECK_LIB(X11, XOpenDisplay,
		[
			AC_SUBST(X_CFLAGS)
			PPC_LDADD="$PPC_LDADD -lX11"
			PPC_LDFLAGS="$PPC_LDFLAGS $X_LIBS"
		],
		AC_MSG_ERROR([Could not find XOpenDisplay in -lX11.]),
		$X_LIBS
	)
;;
qt)
	dnl Check for QTDIR environment variable.
	AC_MSG_CHECKING([whether QTDIR environment variable is set])
	if test -z "$QTDIR"; then
	  AC_MSG_RESULT([no])
	  AC_MSG_ERROR([QTDIR must be properly set.])
	else
	  AC_MSG_RESULT([$QTDIR])
	fi

	dnl Checks for Qt library.
	AC_CACHE_CHECK([for Qt library],
	  ac_qtlib, [
	  for X in qt-mt qt; do
	    if test -z "$ac_qtlib"; then
	      if test -f $QTDIR/lib/lib$X.so -o -f $QTDIR/lib/lib$X.a; then
	        ac_qtlib=$X
	      fi
	    fi
	  done
	])
	if test -z "$ac_qtlib"; then
	  AC_MSG_ERROR([Qt library not found. Maybe QTDIR isn't properly set.])
	fi

	dnl Check for Qt multi-thread support.
	if test "x$ac_qtlib" = "xqt-mt"; then
	  ac_thread="thread"
	fi
	AC_SUBST(ac_thread)

	CFLAGS="$CFLAGS -I$QTDIR/include"
	LIBS="-L$QTDIR/lib -L/usr/X11R6/lib"

	AC_CACHE_CHECK(
		[for Qt library version >= 3.0.0], ac_qtlib_version,
		[	AC_TRY_LINK(
			[
				#include "qglobal.h"
			],
			[
				#if QT_VERSION < 0x030000
				#error Qt library 3.0.0 or greater required.
				#endif
			],
			[
				ac_qtlib_version="yes"
				QT_CFLAGS="$CFLAGS"
				QT_LIBS="-l$ac_qtlib"
				QT_LDFLAGS="$LIBS"
			],
			[
				QT_CFLAGS=""
				QT_LIBS=""
				QT_LDFLAGS=""
				AC_MSG_ERROR([*** QT version 3.0.0 is required for QT gui.  Upgrade or try a different --enable-ui option.])
			])
			CFLAGS="$ac_save_CFLAGS"
			LIBS="$ac_save_LIBS"
		]
	)

	AC_SUBST(QT_CFLAGS)

	dnl A common error message:
dnl	ac_qtdir_errmsg="not found in current PATH. Maybe QT development environment isn't available (qt3-devel)."

	PPC_LDADD="$PPC_LDADD $QT_LIBS"
	PPC_LDFLAGS="$PPC_LDFLAGS $QT_LDFLAGS"
;;
gtk)
;;
beos)
;;
sdl)
	SDL_VERSION=1.2.0

	AM_PATH_SDL($SDL_VERSION,
		    :,
		    AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
	)
	
	SDL_CFLAGS=`echo "$SDL_CFLAGS" | sed 's/-Dmain=SDL_main//'`
	SDL_LIBS=`echo "$SDL_LIBS" | sed 's/-mwindows//' | sed 's/-lmingw32//'`
	LDFLAGS="$LDFLAGS $SDL_LIBS"
	PPC_CFLAGS="$PPC_CFLAGS $SDL_CFLAGS"
	PPC_CXXFLAGS="$PPC_CXXFLAGS $SDL_CFLAGS"
	PPC_LDADD="$PPC_LDADD $SDL_LIBS"
	PPC_LDFLAGS="$PPC_LDFLAGS $SDL_LIBS"
;;
win32)
;;
*)
	AC_MSG_ERROR([Invalid parameter to --enable-ui: '$enable_ui'. Run './configure --help' for a list of valid parameters.])
;;
esac

AM_CONDITIONAL(USE_ARCH_X86, test x$ARCH_DIR = xx86)
AM_CONDITIONAL(USE_ARCH_X86_64, test x$ARCH_DIR = xx86_64)
AM_CONDITIONAL(USE_ARCH_GENERIC, test x$ARCH_DIR = xgeneric)

AM_CONDITIONAL(USE_UI_QT, test x$UI_DIR = xqt)
AM_CONDITIONAL(USE_UI_GTK, test x$UI_DIR = xgtk)
AM_CONDITIONAL(USE_UI_BEOS, test x$UI_DIR = xbeos)
AM_CONDITIONAL(USE_UI_SDL, test x$UI_DIR = xsdl)
AM_CONDITIONAL(USE_UI_WIN32, test x$UI_DIR = xwin32)
AM_CONDITIONAL(USE_UI_X11, test x$UI_DIR = xx11)

dnl Checks for some functions.
AC_CHECK_FUNCS([gettimeofday memset setenv])

AC_CHECK_FUNCS(log2, , AC_MSG_CHECKING(for log2 in math.h)
	AC_TRY_LINK([#include <math.h>], 
	[ log2(1.0); ], 
	AC_DEFINE(HAVE_LOG2) AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)))

AC_CHECK_FUNCS(exp2, , AC_MSG_CHECKING(for exp2 in math.h)
	AC_TRY_LINK([#include <math.h>], 
	[ exp2(0.0); ], 
	AC_DEFINE(HAVE_EXP2) AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)))

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(pthread.h, AC_DEFINE(PTHREAD_HDR, <pthread.h>, [Have pthread.h?]))
AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([asm/types.h])
AC_CHECK_HEADERS([stdint.h])

case "$target_os" in
linux*)
        AC_CHECK_HEADERS([asm/types.h])
        
        dnl Have Linux TUN?
        AC_CHECK_HEADERS(linux/netlink.h, AC_DEFINE(HAVE_LINUX_TUN, 1, [Have TUN/TAP headers?]),,
             [[
             #if HAVE_ASM_TYPES_H
             # include <asm/types.h>
             #endif
             #if HAVE_SYS_SOCKET_H
             # include <sys/socket.h>
             #endif
             ]])
;;
beos*)
	echo "BEOS CFLAGS=$CFLAGS"
	echo "BEOS CPPFLAGS=$CPPFLAGS"
	echo "BEOS CXFLAGS=$CXXFLAGS"
        dnl Have BeOS TUN ? (same API as Linux, except the device path)
        dnl might be in linux/ for compatibility
        AC_CHECK_HEADERS(linux/if_tun.h, AC_DEFINE(HAVE_BEOS_TUN, 1, [Have TUN/TAP headers?]))
        dnl but it really makes more sense in net/ (WTF does Linux put that in linux/ ?)
        dnl AC_CHECK_HEADERS(net/if_tun.h, AC_DEFINE(HAVE_BEOS_TUN, 1, [Have TUN/TAP headers?]))
	dnl needed to enumerate interfaces under BONE
	AC_CHECK_LIB(socket, socket, [PPC_LDFLAGS="$PPC_LDFLAGS -lsocket"])
;;
darwin*)
        AC_CHECK_HEADERS(mach/clock.h, AC_DEFINE(HAVE_MACH_CLOCK_H, 1, [Have mach/clock.h]))
;;
esac

dnl AC_CONFIG_HEADER(config.h)

CFLAGS="$PPC_CFLAGS"
CXXFLAGS="$PPC_CXXFLAGS"
CCASFLAGS="$PPC_CCASFLAGS"

dnl Makefile vars.
AC_SUBST(ARCH_DIR)
AC_SUBST(CPU_DIR)
AC_SUBST(UI_DIR)
AC_SUBST(OSAPI_DIR)
AC_SUBST(CFLAGS)
AC_SUBST(CXXFLAGS)
AC_SUBST(PPC_LDADD)
AC_SUBST(PPC_LDFLAGS)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

AC_CHECK_SIZEOF(char, 1)
AC_CHECK_SIZEOF(short, 2)
AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(long int, 4)
AC_CHECK_SIZEOF(long long, 8)

#AC_SUBST(PTHREAD_FLAGS)

AC_OUTPUT([
Makefile
doc/Makefile
scripts/Makefile
src/Makefile
src/cpu/Makefile
src/cpu/cpu_generic/Makefile
src/cpu/cpu_jitc_x86/Makefile
src/cpu/cpu_jitc_x86_64/Makefile
src/debug/Makefile
src/io/Makefile
src/io/3c90x/Makefile
src/io/ide/Makefile
src/io/graphic/Makefile
src/io/rtl8139/Makefile
src/io/prom/Makefile
src/io/prom/fs/Makefile
src/io/prom/fs/hfs/Makefile
src/io/prom/fs/hfsplus/Makefile
src/io/cuda/Makefile
src/io/pci/Makefile
src/io/pic/Makefile
src/io/macio/Makefile
src/io/nvram/Makefile
src/io/usb/Makefile
src/io/serial/Makefile
src/system/Makefile
src/system/arch/Makefile
src/system/arch/x86/Makefile
src/system/arch/x86_64/Makefile
src/system/arch/generic/Makefile
src/system/osapi/Makefile
src/system/osapi/beos/Makefile
src/system/osapi/posix/Makefile
src/system/osapi/win32/Makefile
src/system/ui/Makefile
src/system/ui/beos/Makefile
src/system/ui/gtk/Makefile
src/system/ui/qt/Makefile
src/system/ui/sdl/Makefile
src/system/ui/win32/Makefile
src/system/ui/x11/Makefile
src/tools/Makefile
])

AC_MSG_RESULT([])
AC_MSG_RESULT([$0 successful.])
AC_MSG_RESULT([])
AC_MSG_RESULT([================================================================================])
AC_MSG_RESULT([                             Configuration summary])
AC_MSG_RESULT([================================================================================])
AC_MSG_RESULT([])
AC_MSG_RESULT([cpu emulation method:                $CPU_DIR])
AC_MSG_RESULT([compiled for architecture:           $ARCH_DIR])
AC_MSG_RESULT([compiled for OS-API:                 $OSAPI_DIR])
AC_MSG_RESULT([compiled for UI system:              $UI_DIR])
AC_MSG_RESULT([enable debug:                        $enable_debug])
AC_MSG_RESULT([enable profiling:                    $enable_profiling])
AC_MSG_RESULT([make release build:                  $enable_release])
AC_MSG_RESULT([omit frame pointer:                  $enable_fpo])
AC_MSG_RESULT([final C compiler flags:              $PPC_CFLAGS])
AC_MSG_RESULT([final C++ compiler flags:            $PPC_CXXFLAGS])
AC_MSG_RESULT([final linker flags:                  $PPC_LDFLAGS])
AC_MSG_RESULT([final linker add:                    $PPC_LDADD])
AC_MSG_RESULT([])
AC_MSG_RESULT([================================================================================])
