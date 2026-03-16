/*
 *  PearPC
 *  ppc_vec.h
 *
 *  Copyright (C) 2004 Daniel Foesch (dfoesch@cs.nmsu.edu)
 *  Copyright (C) 2026 AArch64 port
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __PPC_VEC_H__
#define __PPC_VEC_H__

#define PPC_OPC_VRc (1 << 10)

#if HOST_ENDIANESS == HOST_ENDIANESS_LE

#define VECT_B(reg, index) ((reg).b[15 - (index)])
#define VECT_SB(reg, index) ((reg).sb[15 - (index)])
#define VECT_H(reg, index) ((reg).h[7 - (index)])
#define VECT_SH(reg, index) ((reg).sh[7 - (index)])
#define VECT_W(reg, index) ((reg).w[3 - (index)])
#define VECT_SW(reg, index) ((reg).sw[3 - (index)])
#define VECT_D(reg, index) ((reg).d[1 - (index)])
#define VECT_SD(reg, index) ((reg).sd[1 - (index)])
#define VECT_FLOAT(reg, index) ((reg).f[3 - (index)])

#elif HOST_ENDIANESS == HOST_ENDIANESS_BE

#define VECT_B(reg, index) ((reg).b[(index)])
#define VECT_SB(reg, index) ((reg).sb[(index)])
#define VECT_H(reg, index) ((reg).h[(index)])
#define VECT_SH(reg, index) ((reg).sh[(index)])
#define VECT_W(reg, index) ((reg).w[(index)])
#define VECT_SW(reg, index) ((reg).sw[(index)])
#define VECT_D(reg, index) ((reg).d[(index)])
#define VECT_SD(reg, index) ((reg).sd[(index)])
#define VECT_FLOAT(reg, index) ((reg).f[(index)])

#else
#error "No endianess defined"
#endif

/*
 *  AltiVec interpreter function stubs.
 *  TODO: declare all AltiVec opcode functions here when needed.
 */

#include "jitc_types.h"

void ppc_opc_mfvscr(PPC_CPU_State &aCPU);
void ppc_opc_mtvscr(PPC_CPU_State &aCPU);

JITCFlow ppc_opc_gen_mfvscr(JITC &aJITC);
JITCFlow ppc_opc_gen_mtvscr(JITC &aJITC);

#endif
