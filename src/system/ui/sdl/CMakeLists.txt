set(target system_ui)

include(CMakeFindDependencyMacro)
find_dependency(SDL REQUIRED)

set(all_srcs
    gui.cc
    sysdisplay.cc
    syskeyboard.cc
    sysmouse.cc
    syssdl.cc
    syssdl.h
)

add_library(${target} ${all_srcs})

target_include_directories(${target}
    PUBLIC
        ${SDL_INCLUDE_DIR}
)

target_link_libraries(${target}
    PUBLIC
        ${SDL_LIBRARY}
)
