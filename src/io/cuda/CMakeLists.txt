set(target io_cuda)

set(all_srcs
    cuda.cc
    cuda.h
)

add_library(${target} ${all_srcs})
