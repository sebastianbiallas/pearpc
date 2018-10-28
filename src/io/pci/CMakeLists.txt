set(target io_pci)

set(all_srcs
    pci.cc
    pci.h
)

add_library(${target} ${all_srcs})
