set(target io_rtl8139)

set(all_srcs
    rtl8139.cc
    rtl8139.h
)

add_library(${target} ${all_srcs})
