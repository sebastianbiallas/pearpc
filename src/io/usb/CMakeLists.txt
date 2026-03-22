set(target io_usb)

set(all_srcs
    usb.cc
    usb.h
)

add_library(${target} ${all_srcs})
