set(target io_ide)

set(all_srcs
    ide.cc
    ide.h
    idedevice.cc
    idedevice.h
    ata.cc
    ata.h
    cd.cc
    cd.h
    scsicmds.h
)

add_library(${target} ${all_srcs})
