set(target fs_hfsplus)

set(all_srcs
    apple.h
    blockiter.c
    blockiter.h
    btree.c
    btree.h
    hfs.h
    hfsp.h
    hfstime.c
    hfstime.h
    libhfsp.c
    libhfsp.h
    os.cc
    os.h
    partitions.c
    partitions.h
    record.c
    record.h
    swab.h
    unicode.c
    unicode.h
    volume.c
    volume.h
)

add_library(${target} ${all_srcs})
