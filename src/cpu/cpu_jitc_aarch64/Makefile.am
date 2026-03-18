AUTOMAKE_OPTIONS = foreign

noinst_LIBRARIES = libcpu.a

libcpu_a_SOURCES = ppc_alu.cc ppc_cpu.cc ppc_cpu.h \
ppc_dec.cc ppc_exc.cc \
ppc_fpu.cc ppc_mmu.cc ppc_opc.cc \
ppc_vec.cc jitc.cc jitc.h jitc_debug.cc jitc_debug.h \
jitc_types.h jitc_mmu.S jitc_tools.S aarch64asm.cc aarch64asm.h jitc_asm.h \
jitc_common.h

AM_CPPFLAGS = -I ../..
