set(target system_arch)

enable_language(ASM)

set(all_srcs
    sysfeatures.h
    sysvaccel.cc
    vaccel.S
    sysendian.h
)

add_library(${target} ${all_srcs})
