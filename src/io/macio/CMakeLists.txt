set(target io_macio)

set(all_srcs
    macio.cc
    macio.h
)

add_library(${target} ${all_srcs})
