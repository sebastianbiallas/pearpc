AUTOMAKE_OPTIONS = foreign

SUFFIXES = .asm .o

noinst_LIBRARIES = libcpu.a

libcpu_a_SOURCES = ppc_alu.cc ppc_alu.h ppc_cpu.cc ppc_cpu.h \
ppc_dec.cc ppc_dec.h ppc_esc.cc ppc_esc.h ppc_exc.cc ppc_exc.h \
ppc_fpu.cc ppc_fpu.h ppc_mmu.cc ppc_mmu.h ppc_opc.cc ppc_opc.h \
ppc_vec.h ppc_vec.cc ppc_tools.h jitc.cc jitc.h jitc_debug.cc jitc_debug.h \
jitc_types.h jitc_mmu.S jitc_tools.S x86asm.cc x86asm.h jitc_asm.h

INCLUDES = -I ../..
