set(CMAKE_CXX_STANDARD 11)

set(PEARPC_UI "sdl" CACHE STRING "Select the user interface")
set_property(CACHE PEARPC_UI PROPERTY STRINGS sdl x11 win32)

set(PEARPC_CPU "generic" CACHE STRING "Select the CPU emulation mode")
set_property(CACHE PEARPC_CPU PROPERTY STRINGS generic x86 x86_64)

if (WIN32)
    set(OSAPI_DIR win32)
else()
    set(OSAPI_DIR posix)
endif()

add_compile_options(
    -DSYSTEM_ARCH_SPECIFIC_ENDIAN_DIR="system/arch/${PEARPC_CPU}/sysendian.h"
    -DSYSTEM_ARCH_SPECIFIC_FEATURES_DIR="system/arch/${PEARPC_CPU}/sysfeatures.h"
    -DSYSTEM_OSAPI_SPECIFIC_TYPES_HDR="system/osapi/${OSAPI_DIR}/types.h"
)

include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckFunctionExists)

CHECK_INCLUDE_FILE(stdint.h have_stdint_h)
CHECK_INCLUDE_FILE(mach/clock.h have_mach_clock_h)
CHECK_INCLUDE_FILE(unistd.h have_unistd_h)
CHECK_INCLUDE_FILE(linux/netlink.h have_linux_tun)

CHECK_SYMBOL_EXISTS(setenv stdlib.h have_setenv)
CHECK_SYMBOL_EXISTS(memset string.h have_memset)
CHECK_SYMBOL_EXISTS(gettimeofday sys/time.h have_gettimeofday)
CHECK_SYMBOL_EXISTS(SIGRTMIN signal.h have_sigrtmin)
CHECK_FUNCTION_EXISTS(clock_settime have_clock_settime)
CHECK_FUNCTION_EXISTS(setitimer have_setitimer)

if (have_sigrtmin AND have_clock_settime)
    set(systimer_signal SIGRTMIN)
    add_compile_options(-DUSE_POSIX_REALTIME_CLOCK=1)
elseif (have_setitimer)
    set(systimer_signal SIGALRM)
    add_compile_options(-DUSE_POSIX_SETITIMER=1)
else()
    message(FATAL_ERROR "Unable to find timer mechanism, should have either POSIX realtime API or BSD interval timer API")
endif()

add_compile_options(
    $<$<BOOL:${have_setenv}>:-DHAVE_SETENV=1>
    $<$<BOOL:${have_memset}>:-DHAVE_MEMSET=1>
    $<$<BOOL:${have_gettimeofday}>:-DHAVE_GETTIMEOFDAY=1>
    $<$<BOOL:${have_stdint_h}>:-DHAVE_STDINT_H=1>
    $<$<BOOL:${have_mach_clock_h}>:-DHAVE_MACH_CLOCK_H=1>
    $<$<BOOL:${have_unistd_h}>:-DHAVE_UNISTD_H=1>
    $<$<BOOL:${have_linux_tun}>:-DHAVE_LINUX_TUN=1>
    -DSYSTIMER_SIGNAL=${systimer_signal}
)

include(TestBigEndian)
test_big_endian(_big_endian)

add_compile_options(
    -DHOST_ENDIANESS_BE=1234
    -DHOST_ENDIANESS_LE=4321
)

if (_big_endian)
    add_compile_options(-DHOST_ENDIANESS=HOST_ENDIANESS_BE)
else()
    add_compile_options(-DHOST_ENDIANESS=HOST_ENDIANESS_LE)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_compile_options(-D_FILE_OFFSET_BITS=64)
endif()

add_subdirectory(tools)
add_subdirectory(debug)
add_subdirectory(system)
add_subdirectory(cpu)
add_subdirectory(io)

set(all_srcs
    main.cc
    info.h
    ppc_img.c
    ppc_img.h
    ppc_font.c
    ppc_font.h
    ppc_button_changecd.c
    ppc_button_changecd.h
    configparser.cc
    configparser.h
)

set(target ppc)

add_executable(${target} ${all_srcs})

target_link_libraries(${target}
    system_arch
    cpu
    debugger
    io_pci
    io_3c90x
    io_rtl8139
    io_prom
    io_fs
    io_graphic
    io_pic
    io_cuda
    io_ide
    io_macio
    io_nvram
    io_usb
    io_serial
    io
    cpu
    tools
    system_ui
    system
    system_arch
    osapi
    $<$<BOOL:${have_sigrtmin}>:rt>
)
