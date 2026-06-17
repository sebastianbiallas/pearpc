set(target system_ui)

set(all_srcs
    gui.cc
    sysdisplay.cc
    syswin.cc
    syswin.h
    syskeyboard.cc
    sysmouse.cc
    resources.h
    resources.rc
)

add_library(${target} ${all_srcs})

# TODO: windres -F pe-i386 --include-dir=$(INCLUDE_PATH) -i $< -o $@
