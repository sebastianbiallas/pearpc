set(target debugger)

find_package(BISON)
find_package(FLEX)

bison_target(debugparse debugparse.y ${CMAKE_CURRENT_BINARY_DIR}/debugparse.c)
flex_target(lex lex.l ${CMAKE_CURRENT_BINARY_DIR}/lex.c)
add_flex_bison_dependency(lex debugparse)

set(all_srcs
    asm.cc
    asm.h
    ppcdis.cc
    ppcdis.h
    ppcopc.cc
    ppcopc.h
    tracers.h
    debugparse.y
    debugtype.h
    lex.l
    lex.h
    parsehelper.c
    parsehelper.h
    debugger.cc
    debugger.h
    stdfuncs.cc
    stdfuncs.h
    x86opc.cc
    x86opc.h
    x86dis.cc
    x86dis.h
    ${BISON_debugparse_OUTPUTS}
    ${FLEX_lex_OUTPUTS}
)

add_library(${target} ${all_srcs})

target_include_directories(${target}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
