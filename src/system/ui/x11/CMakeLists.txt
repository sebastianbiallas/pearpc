set(target system_ui)

include(CMakeFindDependencyMacro)
find_dependency(X11 REQUIRED)

set(all_srcs
    gui.cc
    sysdisplay.cc
    syskeyboard.cc
    sysmouse.cc
    sysx11.cc
    sysx11.h
)

add_library(${target} ${all_srcs})

target_include_directories(${target}
    PUBLIC
        ${X11_X11_INCLUDE_PATH}
)

target_link_libraries(${target}
    PUBLIC
        ${X11_X11_LIB}
)
