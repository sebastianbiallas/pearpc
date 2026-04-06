set(target io)

set(all_srcs
    io.cc
    io.h
)

add_library(${target} ${all_srcs})

add_subdirectory(3c90x)
add_subdirectory(rtl8139)
add_subdirectory(prom)
add_subdirectory(graphic)
add_subdirectory(pic)
add_subdirectory(cuda)
add_subdirectory(pci)
add_subdirectory(ide)
add_subdirectory(macio)
add_subdirectory(nvram)
add_subdirectory(usb)
add_subdirectory(serial)
