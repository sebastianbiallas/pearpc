set(target fs_hfs)

set(all_srcs
    apple.h
    block.c
    block.h
    btree.c
    btree.h
    data.c
    data.h
    file.c
    file.h
    hfs.c
    hfs.h
    libhfs.h
    low.c
    low.h
    medium.c
    medium.h
    node.c
    node.h
    os.cc
    os.h
    record.c
    record.h
    version.c
    version.h
    volume.c
    volume.h
)

add_library(${target} ${all_srcs})
