set(target osapi)

set(all_srcs
    sysclipboard.cc
    sysfile.cc
    systhread.cc
    sysethtun.cc
    systimer.cc
    sysinit.cc
    types.h
    tap_constants.h
    syscdrom.cc
    scsipt.h
    aspi-win32.h
    scsitypes.h
)

add_library(${target} ${all_srcs})
