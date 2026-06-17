set(target system)

set(all_srcs
    display.cc
    display.h
    fileofs.h
    sysexcept.cc
    sysexcept.h
    sys.cc
    sys.h
    types.h
    file.cc
    file.h
    vt100.cc
    vt100.h
    font.cc
    font.h
    gif.cc
    gif.h
    systhread.h
    systimer.h
    sysethtun.h
    sysfeatures.h
    keyboard.cc
    keyboard.h
    mouse.cc
    mouse.h
    event.h
    device.cc
    device.h
    sysclk.h
    sysvaccel.h
    syscdrom.h
    sysvm.h
)

add_library(${target} ${all_srcs})

add_subdirectory(osapi)
add_subdirectory(arch)
add_subdirectory(ui)
