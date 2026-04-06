set(target io_fs)

set(all_srcs
    part.cc
    part.h
    hfs.cc
    hfs.h
    hfsglue.h
    hfsplus.cc
    hfsplus.h
    hfsplusglue.h
    hfsstruct.h
    fs.cc
    fs.h
)

add_library(${target} ${all_srcs})

target_link_libraries(${target}
    PUBLIC
        fs_hfs
        fs_hfsplus
)

add_subdirectory(hfs)
add_subdirectory(hfsplus)
