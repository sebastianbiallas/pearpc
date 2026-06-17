set(target io_prom)

set(all_srcs
    prom.cc
    prom.h
    prommem.cc
    prommem.h
    fcode.cc
    fcode.h 
    forth.cc
    forth.h
    forthtable.h
    forthtable.cc
    promboot.cc
    promboot.h
    promosi.cc
    promosi.h
    promdt.cc
    promdt.h
)

add_library(${target} ${all_srcs})

add_subdirectory(fs)
