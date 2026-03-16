/*
 *  PearPC
 *  ppc_vec.cc - AArch64 JIT AltiVec instruction generation (stub)
 *
 *  Copyright (C) 2026 Sebastian Biallas (sb@biallas.net)
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

/*
 *  Stub: AltiVec opcode generation functions are not yet implemented
 *  for AArch64. All gen functions are routed through ppc_opc_gen_invalid
 *  in ppc_dec.cc, which returns flowEndBlockUnreachable.
 */

#include "jitc.h"
#include "jitc_asm.h"
#include "ppc_cpu.h"
