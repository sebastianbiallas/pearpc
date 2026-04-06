set(target system_arch)

set(all_srcs
    sysfeatures.h
    sysvaccel.cc
    sysendian.h
)

add_library(${target} ${all_srcs})
