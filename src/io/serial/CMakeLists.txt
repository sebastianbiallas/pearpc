set(target io_serial)

set(all_srcs
    serial.cc
    serial.h
)

add_library(${target} ${all_srcs})
