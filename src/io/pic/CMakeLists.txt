set(target io_pic)

set(all_srcs
    pic.cc
    pic.h
)

add_library(${target} ${all_srcs})
