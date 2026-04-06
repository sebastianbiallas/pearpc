set(target io_nvram)

set(all_srcs
    nvram.cc
    nvram.h
)

add_library(${target} ${all_srcs})
