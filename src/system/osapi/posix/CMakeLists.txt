set(target osapi)

find_package(Threads REQUIRED)

set(all_srcs
    sysclipboard.cc
    sysfile.cc
    sysinit.cc
    sysethtun.cc
    systhread.cc
    systimer.cc
    syscdrom.cc
    types.h
    sysvm.cc
)

add_library(${target} ${all_srcs})

target_link_libraries(${target}
    PUBLIC
        Threads::Threads
)
