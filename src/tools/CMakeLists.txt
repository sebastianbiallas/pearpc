set(target tools)

set(all_srcs
    atom.cc
    atom.h
    data.cc
    data.h
    debug.cc
    debug.h
    endianess.cc
    endianess.h
    except.cc
    except.h
    snprintf.cc
    snprintf.h
    str.cc
    str.h
    stream.cc
    stream.h
    strtools.cc
    strtools.h
    thread.cc
    thread.h
    cstream.h
    crc32.cc
    crc32.h
    crc32defs.h
    crc32table.h
)

add_library(${target} ${all_srcs})
