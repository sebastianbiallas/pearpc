/*
 *	PearPC
 *	x86asm.cc
 *
 *	Copyright (C) 2004 Sebastian Biallas (sb@biallas.net)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <cstring>
#include <cstdlib>

#include "tools/debug.h"
#include "tools/snprintf.h"
#include "jitc.h"
#include "jitc_asm.h"
#include "jitc_debug.h"
#include "x86asm.h"

void x86GetCaps(X86CPUCaps &caps)
{
	memset(&caps, 0, sizeof caps);

	caps.loop_align = 8;

	struct {
		uint32 level, c, d, b;
	} id;

	if (!ppc_cpuid_asm(0, &id)) {
		ht_snprintf(caps.vendor, sizeof caps.vendor, "unknown");
		return;
	}

	*((uint32 *)caps.vendor) = id.b;
	*((uint32 *)(caps.vendor+4)) = id.d;
	*((uint32 *)(caps.vendor+8)) = id.c;
	caps.vendor[12] = 0;
	ht_printf("%s\n", caps.vendor);
	if (id.level == 0) return;

	struct {
		uint32 model, features2, features, b;
	} id2;

	ppc_cpuid_asm(1, &id2);
	caps.rdtsc = id2.features & (1<<4);
	caps.cmov = id2.features & (1<<15);
	caps.mmx = id2.features & (1<<23);
	caps._3dnow = id2.features & (1<<31);
	caps._3dnow2 = id2.features & (1<<30);
	caps.sse = id2.features & (1<<25);
	caps.sse2 = id2.features & (1<<26);
	caps.sse3 = id2.features2 & (1<<0);
	
	ppc_cpuid_asm(0x80000000, &id);
	if (id.level >= 0x80000001) {
		// processor supports extended functions
		// now test for 3dnow
		ppc_cpuid_asm(0x80000001, &id2);
		
		caps._3dnow = id2.features & (1<<31);
		caps._3dnow2 = id2.features & (1<<30);
	}
	
	ht_printf("%s%s%s%s%s%s%s\n",
		caps.cmov?" CMOV":"",
		caps.mmx?" MMX":"",
		caps._3dnow?" 3DNOW":"",
		caps._3dnow2?" 3DNOW+":"",
		caps.sse?" SSE":"",
		caps.sse2?" SSE2":"",
		caps.sse3?" SSE3":"");
}

/*
 *	internal functions
 */

static inline void FASTCALL jitcMapRegister(NativeReg nreg, PPC_Register creg)
{
	gJITC.nativeReg[nreg] = creg;
	gJITC.clientReg[creg] = nreg;
}

static inline void FASTCALL jitcUnmapRegister(NativeReg reg)
{
	gJITC.clientReg[gJITC.nativeReg[reg]] = REG_NO;
	gJITC.nativeReg[reg] = PPC_REG_NO;
}

static inline void FASTCALL jitcLoadRegister(NativeReg nreg, PPC_Register creg)
{
	asmALU32(X86_MOV, nreg, (byte*)&gCPU+creg);
	jitcMapRegister(nreg, creg);
	gJITC.nativeRegState[nreg] = rsMapped;
}

static inline void FASTCALL jitcStoreRegister(NativeReg nreg, PPC_Register creg)
{
	asmALU32(X86_MOV, (byte*)&gCPU+creg, nreg);
}

static inline void FASTCALL jitcStoreRegisterUndirty(NativeReg nreg, PPC_Register creg)
{
	jitcStoreRegister(nreg, creg);
	gJITC.nativeRegState[nreg] = rsMapped; // no longer dirty
}

static inline PPC_Register FASTCALL jitcGetRegisterMapping(NativeReg reg)
{
	return gJITC.nativeReg[reg];
}

NativeReg FASTCALL jitcGetClientRegisterMapping(PPC_Register creg)
{
	return gJITC.clientReg[creg];
}

static inline void FASTCALL jitcDiscardRegister(NativeReg r)
{
	// FIXME: move to front of the LRU list
	gJITC.nativeRegState[r] = rsUnused;
}

/*
 *	Puts native register to the end of the LRU list
 */
void FASTCALL jitcTouchRegister(NativeReg r)
{
	NativeRegType *reg = gJITC.nativeRegsList[r];
	if (reg->moreRU) {
		// there's a more recently used register
		if (reg->lessRU) {
			reg->lessRU->moreRU = reg->moreRU;
			reg->moreRU->lessRU = reg->lessRU;
		} else {
			// reg was LRUreg
			gJITC.LRUreg = reg->moreRU;
			reg->moreRU->lessRU = NULL;
		}
		reg->moreRU = NULL;
		reg->lessRU = gJITC.MRUreg;
		gJITC.MRUreg->moreRU = reg;
		gJITC.MRUreg = reg;
	}
}

/*
 *	clobbers and moves to end of LRU list
 */
static inline void FASTCALL jitcClobberAndTouchRegister(NativeReg reg)
{
	switch (gJITC.nativeRegState[reg]) {
	case rsDirty:
		jitcStoreRegisterUndirty(reg, jitcGetRegisterMapping(reg));
		// fall throu
	case rsMapped:
		jitcUnmapRegister(reg);
		gJITC.nativeRegState[reg] = rsUnused;
		break;
	case rsUnused:;
	}
	jitcTouchRegister(reg);
}

/*
 *	clobbers and moves to front of LRU list
 */
static inline void FASTCALL jitcClobberAndDiscardRegister(NativeReg reg)
{
	switch (gJITC.nativeRegState[reg]) {
	case rsDirty:
		jitcStoreRegisterUndirty(reg, jitcGetRegisterMapping(reg));
		// fall throu
	case rsMapped:
		jitcUnmapRegister(reg);
		jitcDiscardRegister(reg);
		break;
	case rsUnused:;
		/*
		 *	Note: it makes no sense to move this register to
		 *	the front of the LRU list here, since only
		 *	other unused register can be before it in the list
		 *
		 *	Note2: it would even be an error to move it here,
		 *	since ESP isn't in the nativeRegsList
		 */
	}
}

void FASTCALL jitcClobberSingleRegister(NativeReg reg)
{
	switch (gJITC.nativeRegState[reg]) {
	case rsDirty:
		jitcStoreRegisterUndirty(reg, jitcGetRegisterMapping(reg));
		// fall throu
	case rsMapped:
		jitcUnmapRegister(reg);
		gJITC.nativeRegState[reg] = rsUnused;
		break;
	case rsUnused:;
	}
}

/*
 *	Dirty register.
 *	Does *not* touch register
 *	Will not produce code.
 */
NativeReg FASTCALL jitcDirtyRegister(NativeReg r)
{
	gJITC.nativeRegState[r] = rsDirty;
	return r;
}

NativeReg FASTCALL jitcAllocFixedRegister(NativeReg reg)
{
	jitcClobberAndTouchRegister(reg);
	return reg;
}

/*
 *	Allocates a native register
 *	May produce a store if no registers are avaiable
 */
NativeReg FASTCALL jitcAllocRegister(int options)
{
	NativeReg reg;
	if (options & NATIVE_REG) {
		// allocate fixed register
		reg = (NativeReg)(options & 0xf);
	} else if (options & NATIVE_REG_8) {
		// allocate eax, ecx, edx or ebx
		NativeRegType *rt = gJITC.LRUreg;
		while (rt->reg > EBX) rt = rt->moreRU;
		reg = rt->reg;
	} else {
		// allocate random register
		reg = gJITC.LRUreg->reg;
	}
	return jitcAllocFixedRegister(reg);
}

/*
 *	Returns native registers that contains value of 
 *	client register or allocates new register which
 *	maps to the client register.
 *	Dirties register.
 *
 *	May produce a store if no registers are avaiable
 *	May produce a MOV/XCHG to satisfy mapping
 *	Will never produce a load
 */
NativeReg FASTCALL jitcMapClientRegisterDirty(PPC_Register creg, int options)
{
	if (options & NATIVE_REG_8) {
		// nyi
		ht_printf("unimpl x86asm:%d\n", __LINE__);
		exit(-1);
	}
	if (options & NATIVE_REG) {
		NativeReg want_reg = (NativeReg)(options & 0xf);
		PPC_Register have_mapping = jitcGetRegisterMapping(want_reg);

		if (have_mapping != PPC_REG_NO) {
			// test if we're lucky
			if (have_mapping == creg) {
				jitcDirtyRegister(want_reg);
				jitcTouchRegister(want_reg);
				return want_reg;
			}

			// we're not lucky, get a new register for the old mapping
			NativeReg temp_reg = jitcAllocRegister();
			// note that AllocRegister also touches temp_reg

			// make new mapping
			jitcMapRegister(want_reg, creg);

			gJITC.nativeRegState[temp_reg] = gJITC.nativeRegState[want_reg];
			// now we can mess with want_reg
			jitcDirtyRegister(want_reg);

			// maybe the old mapping was discarded and we're done
			if (temp_reg == want_reg) return want_reg;

			// ok, restore old mapping
			if (temp_reg == EAX || want_reg == EAX) {
				asmALU32(X86_XCHG, temp_reg, want_reg);
			} else {
				asmALU32(X86_MOV, temp_reg, want_reg);				
			}
			jitcMapRegister(temp_reg, have_mapping);
		} else {
			// want_reg is free
			// unmap creg if needed
			NativeReg reg = jitcGetClientRegisterMapping(creg);
			if (reg != REG_NO) {
				jitcUnmapRegister(reg);
				jitcDiscardRegister(reg);
			}
			jitcMapRegister(want_reg, creg);
			jitcDirtyRegister(want_reg);
		}
		jitcTouchRegister(want_reg);
		return want_reg;
	} else {
		NativeReg reg = jitcGetClientRegisterMapping(creg);
		if (reg == REG_NO) {
			reg = jitcAllocRegister();
			jitcMapRegister(reg, creg);
		} else {
			jitcTouchRegister(reg);
		}
		return jitcDirtyRegister(reg);
	}
}

 
/*
 *	Returns native registers that contains value of 
 *	client register or allocates new register with
 *	this content.
 *
 *	May produce a store if no registers are avaiable
 *	May produce a load if client registers isn't mapped
 *	May produce a MOV/XCHG to satisfy mapping
 */
NativeReg FASTCALL jitcGetClientRegister(PPC_Register creg, int options)
{
	if (options & NATIVE_REG_8) {
		NativeReg client_reg_maps_to = jitcGetClientRegisterMapping(creg);
		if (client_reg_maps_to == REG_NO) {
			NativeReg reg = jitcAllocRegister(NATIVE_REG_8);
			jitcLoadRegister(reg, creg);
			return reg;
		} else {
			if (client_reg_maps_to <= EBX) {
				jitcTouchRegister(client_reg_maps_to);
				return client_reg_maps_to;
			}
			NativeReg want_reg = jitcAllocRegister(NATIVE_REG_8);
			asmALU32(X86_MOV, want_reg, client_reg_maps_to);
			jitcUnmapRegister(client_reg_maps_to);
			jitcMapRegister(want_reg, creg);
			gJITC.nativeRegState[want_reg] = gJITC.nativeRegState[client_reg_maps_to];
			gJITC.nativeRegState[client_reg_maps_to] = rsUnused;
			return want_reg;
		}
	}
	if (options & NATIVE_REG) {
		NativeReg want_reg = (NativeReg)(options & 0xf);
		PPC_Register native_reg_maps_to = jitcGetRegisterMapping(want_reg);
		NativeReg client_reg_maps_to = jitcGetClientRegisterMapping(creg);
		if (native_reg_maps_to != PPC_REG_NO) {
			// test if we're lucky
			if (native_reg_maps_to == creg) {
				jitcTouchRegister(want_reg);
			} else {
				// we need to satisfy mapping
				if (client_reg_maps_to != REG_NO) {
					asmALU32(X86_XCHG, want_reg, client_reg_maps_to);
					RegisterState rs = gJITC.nativeRegState[want_reg];
					gJITC.nativeRegState[want_reg] = gJITC.nativeRegState[client_reg_maps_to];
					gJITC.nativeRegState[client_reg_maps_to] = rs;
					jitcMapRegister(want_reg, creg);
					jitcMapRegister(client_reg_maps_to, native_reg_maps_to);
					jitcTouchRegister(want_reg);
				} else {
					// client register isn't mapped
					jitcAllocFixedRegister(want_reg);
					jitcLoadRegister(want_reg, creg);
				}
			}
			return want_reg;
		} else {
			// want_reg is free 
			jitcTouchRegister(want_reg);
			if (client_reg_maps_to != REG_NO) {
				asmALU32(X86_MOV, want_reg, client_reg_maps_to);
				gJITC.nativeRegState[want_reg] = gJITC.nativeRegState[client_reg_maps_to];
				jitcUnmapRegister(client_reg_maps_to);
				jitcDiscardRegister(client_reg_maps_to);
				jitcMapRegister(want_reg, creg);
			} else {
				jitcLoadRegister(want_reg, creg);
			}
			return want_reg;
		}
	} else {
		NativeReg client_reg_maps_to = jitcGetClientRegisterMapping(creg);
		if (client_reg_maps_to != REG_NO) {
			jitcTouchRegister(client_reg_maps_to);
			return client_reg_maps_to;
		} else {
			NativeReg reg = jitcAllocRegister();
			jitcLoadRegister(reg, creg);
			return reg;
		}
	}
}

/*
 *	Same as jitcGetClientRegister() but also dirties result
 */
NativeReg FASTCALL jitcGetClientRegisterDirty(PPC_Register creg, int options)
{
	return jitcDirtyRegister(jitcGetClientRegister(creg, options));
}

static inline void FASTCALL jitcFlushSingleRegister(NativeReg reg)
{
	if (gJITC.nativeRegState[reg] == rsDirty) {
		jitcStoreRegisterUndirty(reg, jitcGetRegisterMapping(reg));
	}
}

static inline void FASTCALL jitcFlushSingleRegisterDirty(NativeReg reg)
{
	if (gJITC.nativeRegState[reg] == rsDirty) {
		jitcStoreRegister(reg, jitcGetRegisterMapping(reg));
	}
}

/*
 *	Flushes native register(s).
 *	Resets dirty flags.
 *	Will produce a store if register is dirty.
 */
void FASTCALL jitcFlushRegister(int options)
{
	if (options == NATIVE_REGS_ALL) {
		for (NativeReg i = EAX; i <= EDI; i = (NativeReg)(i+1)) jitcFlushSingleRegister(i);
	} else if (options & NATIVE_REG) {
		NativeReg reg = (NativeReg)(options & 0xf);
		jitcFlushSingleRegister(reg);
	}
}

/*
 *	Flushes native register(s).
 *	Doesnt reset dirty flags.
 *	Will produce a store if register is dirty.
 */
void FASTCALL jitcFlushRegisterDirty(int options)
{
	if (options == NATIVE_REGS_ALL) {
		for (NativeReg i = EAX; i <= EDI; i = (NativeReg)(i+1)) jitcFlushSingleRegisterDirty(i);
	} else if (options & NATIVE_REG) {
		NativeReg reg = (NativeReg)(options & 0xf);
		jitcFlushSingleRegisterDirty(reg);
	}
}
/*
 *	Clobbers native register(s).
 *	Register is unused afterwards.
 *	Will produce a store if register was dirty.
 */          
void FASTCALL jitcClobberRegister(int options)
{
	if (options == NATIVE_REGS_ALL) {
		/*
		 *	We dont use clobberAndDiscard here
		 *	since it make no sense to move one register
		 *	if we clobber all
		 */
		for (NativeReg i = EAX; i <= EDI; i=(NativeReg)(i+1)) jitcClobberSingleRegister(i);
	} else if (options & NATIVE_REG) {
		NativeReg reg = (NativeReg)(options & 0xf);
		jitcClobberAndDiscardRegister(reg);
	}
}

/*
 *
 */
void FASTCALL jitcFlushAll()
{
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	jitcFlushVectorRegister();
}

/*
 *
 */
void FASTCALL jitcClobberAll()
{
	jitcClobberCarryAndFlags();
	jitcClobberRegister();
	jitcFloatRegisterClobberAll();
	jitcTrashVectorRegister();
}

/*
 *	Invalidates all mappings
 *
 *	Will never produce code
 */          
void FASTCALL jitcInvalidateAll()
{
#if 0
	for (int i=EAX; i<=EDI; i++) {
		if(gJITC.nativeRegState[i] != rsDirty) {
			printf("!!! Unflushed register invalidated!\n");
		}
	}
#endif

	memset(gJITC.nativeReg, PPC_REG_NO, sizeof gJITC.nativeReg);
	memset(gJITC.nativeRegState, rsUnused, sizeof gJITC.nativeRegState);
	memset(gJITC.clientReg, REG_NO, sizeof gJITC.clientReg);
	gJITC.nativeCarryState = gJITC.nativeFlagsState = rsUnused;

	for (unsigned int i=XMM0; i<=XMM7; i++) {
		if(gJITC.nativeVectorRegState[i] == rsDirty) {
			printf("!!! Unflushed vector register invalidated! (XMM%u)\n", i);
		}
	}

	memset(gJITC.n2cVectorReg, PPC_VECTREG_NO, sizeof gJITC.n2cVectorReg);
	memset(gJITC.c2nVectorReg, VECTREG_NO, sizeof gJITC.c2nVectorReg);
	memset(gJITC.nativeVectorRegState, rsUnused, sizeof gJITC.nativeVectorRegState);

	gJITC.nativeVectorReg = VECTREG_NO;
}

/*
 *	Gets the client carry flags into the native carry flag
 *
 *	
 */
void FASTCALL jitcGetClientCarry()
{
	if (gJITC.nativeCarryState == rsUnused) {
		jitcClobberFlags();

#if 0
		// bt [gCPU.xer], XER_CA
		asmBTx32(X86_BT, &gCPU.xer, 29);
#else
		// bt [gCPU.xer_ca], 0
		asmBTx32(X86_BT, &gCPU.xer_ca, 0);
#endif
		gJITC.nativeCarryState = rsMapped;
	}
}

void FASTCALL jitcMapFlagsDirty(PPC_CRx cr)
{
	gJITC.nativeFlags = cr;
	gJITC.nativeFlagsState = rsDirty;
}

PPC_CRx FASTCALL jitcGetFlagsMapping()
{
	return gJITC.nativeFlags;
}

bool FASTCALL jitcFlagsMapped()
{
	return gJITC.nativeFlagsState != rsUnused;
}

bool FASTCALL jitcCarryMapped()
{
	return gJITC.nativeCarryState != rsUnused;
}

void FASTCALL jitcMapCarryDirty()
{
	gJITC.nativeCarryState = rsDirty;
}

static inline void FASTCALL jitcFlushCarry()
{
	asmSET8(X86_C, &gCPU.xer_ca);
}

#if 0

static inline void FASTCALL jitcFlushFlags()
{
	asmCALL((NativeAddress)ppc_flush_flags_asm);
}

#else

uint8 jitcFlagsMapping[257];
uint8 jitcFlagsMapping2[256];
uint8 jitcFlagsMappingCMP_U[257];
uint8 jitcFlagsMappingCMP_L[257];

static inline void FASTCALL jitcFlushFlags()
{
	NativeReg r = jitcAllocRegister(NATIVE_REG_8);
	asmSET8(X86_S, (NativeReg8)r);
	asmSET8(X86_Z, (NativeReg8)(r+4));
	asmMOVxx32_16(X86_MOVZX, r, r);
	asmALU8(X86_AND, (byte*)&gCPU.cr+3, 0x0f);
	asmALU8(X86_MOV, (NativeReg8)r, r, (uint32)&jitcFlagsMapping);
	asmALU8(X86_OR, (byte*)&gCPU.cr+3, (NativeReg8)r);
}

#endif

static inline void jitcFlushFlagsAfterCMP(X86FlagTest t1, X86FlagTest t2, byte mask, int disp, uint32 map)
{
	NativeReg r = jitcAllocRegister(NATIVE_REG_8);
	asmSET8(t1, (NativeReg8)r);
	asmSET8(t2, (NativeReg8)(r+4));
	asmMOVxx32_16(X86_MOVZX, r, r);
	asmALU8(X86_AND, (byte*)&gCPU.cr+disp, mask);
	asmALU8(X86_MOV, (NativeReg8)r, r, map);
	asmALU8(X86_OR, (byte*)&gCPU.cr+disp, (NativeReg8)r);
}

void FASTCALL jitcFlushFlagsAfterCMPL_U(int disp)
{
	jitcFlushFlagsAfterCMP(X86_A, X86_B, 0x0f, disp, (uint32)&jitcFlagsMappingCMP_U);
}

void FASTCALL jitcFlushFlagsAfterCMPL_L(int disp)
{
	jitcFlushFlagsAfterCMP(X86_A, X86_B, 0xf0, disp, (uint32)&jitcFlagsMappingCMP_L);
}

void FASTCALL jitcFlushFlagsAfterCMP_U(int disp)
{
	jitcFlushFlagsAfterCMP(X86_G, X86_L, 0x0f, disp, (uint32)&jitcFlagsMappingCMP_U);
}

void FASTCALL jitcFlushFlagsAfterCMP_L(int disp)
{
	jitcFlushFlagsAfterCMP(X86_G, X86_L, 0xf0, disp, (uint32)&jitcFlagsMappingCMP_L);
}

void FASTCALL jitcClobberFlags()
{
	if (gJITC.nativeFlagsState == rsDirty) {
		if (gJITC.nativeCarryState == rsDirty) {
			jitcFlushCarry();
		}
		jitcFlushFlags();
		gJITC.nativeCarryState = rsUnused;
	}
	gJITC.nativeFlagsState = rsUnused;
}

void FASTCALL jitcClobberCarry()
{
	if (gJITC.nativeCarryState == rsDirty) {
		jitcFlushCarry();
	}
	gJITC.nativeCarryState = rsUnused;
}

void FASTCALL jitcClobberCarryAndFlags()
{
	if (gJITC.nativeCarryState == rsDirty) {
		if (gJITC.nativeFlagsState == rsDirty) {
			jitcFlushCarry();
			jitcFlushFlags();
			gJITC.nativeCarryState = gJITC.nativeFlagsState = rsUnused;
		} else {
			jitcClobberCarry();
		}
	} else {
		jitcClobberFlags();
	}
}

/*
 *	ONLY FOR DEBUG! DON'T CALL (unless you know what you are doing)
 */
void FASTCALL jitcFlushCarryAndFlagsDirty()
{
	if (gJITC.nativeCarryState == rsDirty) {
		jitcFlushCarry();
		if (gJITC.nativeFlagsState == rsDirty) {
			jitcFlushFlags();
		}
	} else {
		if (gJITC.nativeFlagsState == rsDirty) {
			jitcFlushFlags();
		}
	}
}

/*
 *	jitcFloatRegisterToNative converts the stack-independent 
 *	register r to a stack-dependent register ST(i)
 */
NativeFloatReg FASTCALL jitcFloatRegisterToNative(JitcFloatReg r)
{
	return X86_FLOAT_ST(gJITC.nativeFloatTOP-gJITC.floatRegPerm[r]);
}

/*
 *	jitcFloatRegisterFromNative converts the stack-dependent 
 *	register ST(r) to a stack-independent JitcFloatReg
 */
JitcFloatReg FASTCALL jitcFloatRegisterFromNative(NativeFloatReg r)
{
	ASSERT(gJITC.nativeFloatTOP > r);
	return gJITC.floatRegPermInverse[gJITC.nativeFloatTOP-r];
}

/*
 *	Returns true iff r is on top of the floating point register
 *	stack.
 */
bool FASTCALL jitcFloatRegisterIsTOP(JitcFloatReg r)
{
	ASSERT(r != JITC_FLOAT_REG_NONE);
	return gJITC.floatRegPerm[r] == gJITC.nativeFloatTOP;
}

/*
 *	Exchanges r to the front of the stack.
 */
JitcFloatReg FASTCALL jitcFloatRegisterXCHGToFront(JitcFloatReg r)
{
	ASSERT(r != JITC_FLOAT_REG_NONE);
	if (jitcFloatRegisterIsTOP(r)) return r;
	
	asmFXCH(jitcFloatRegisterToNative(r));
	JitcFloatReg s = jitcFloatRegisterFromNative(Float_ST0);
	ASSERT(s != r);
	// set floatRegPerm := floatRegPerm * (s r)
	int tmp = gJITC.floatRegPerm[r];
	gJITC.floatRegPerm[r] = gJITC.floatRegPerm[s];
	gJITC.floatRegPerm[s] = tmp;
	
	// set floatRegPermInverse := (s r) * floatRegPermInverse
	r = gJITC.floatRegPerm[r];
	s = gJITC.floatRegPerm[s];
	tmp = gJITC.floatRegPermInverse[r];
	gJITC.floatRegPermInverse[r] = gJITC.floatRegPermInverse[s];
	gJITC.floatRegPermInverse[s] = tmp;

	return r;
}

/*
 *	Dirties r
 */
JitcFloatReg FASTCALL jitcFloatRegisterDirty(JitcFloatReg r)
{
	gJITC.nativeFloatRegState[r] = rsDirty;
	return r;
}

void FASTCALL jitcFloatRegisterInvalidate(JitcFloatReg r)
{
	jitcFloatRegisterXCHGToFront(r);
	asmFFREEP(Float_ST0);	
	int creg = gJITC.nativeFloatRegStack[r];
	gJITC.clientFloatReg[creg] = JITC_FLOAT_REG_NONE;
	gJITC.nativeFloatTOP--;
}

void FASTCALL jitcPopFloatStack(JitcFloatReg hint1, JitcFloatReg hint2)
{
	ASSERT(gJITC.nativeFloatTOP > 0);
	
	JitcFloatReg r;
	for (int i=0; i<4; i++) {
		r = jitcFloatRegisterFromNative(X86_FLOAT_ST(gJITC.nativeFloatTOP-i-1));
		if (r != hint1 && r != hint2) break;
	}
	
	// we can now free r
	int creg = gJITC.nativeFloatRegStack[r];
	jitcFloatRegisterXCHGToFront(r);
	if (gJITC.nativeFloatRegState[r] == rsDirty) {
		asmFSTP_Double(&gCPU.fpr[creg]);
	} else {
		asmFFREEP(Float_ST0);
	}
	gJITC.nativeFloatRegState[r] = rsUnused;
	gJITC.clientFloatReg[creg] = JITC_FLOAT_REG_NONE;
	gJITC.nativeFloatTOP--;
}

static JitcFloatReg FASTCALL jitcPushFloatStack(int creg)
{
	ASSERT(gJITC.nativeFloatTOP < 8);
	gJITC.nativeFloatTOP++;
	int r = gJITC.floatRegPermInverse[gJITC.nativeFloatTOP];
	asmFLD_Double(&gCPU.fpr[creg]);
	return r;
}

/*
 *	Creates a copy of r on the stack. If the stack is full, it will
 *	clobber an entry. It will not clobber r nor hint.
 */
JitcFloatReg FASTCALL jitcFloatRegisterDup(JitcFloatReg freg, JitcFloatReg hint)
{
//	ht_printf("dup %d\n", freg);
	if (gJITC.nativeFloatTOP == 8) {
		// stack is full
		jitcPopFloatStack(freg, hint);
	}
	asmFLD(jitcFloatRegisterToNative(freg));
	gJITC.nativeFloatTOP++;
	int r = gJITC.floatRegPermInverse[gJITC.nativeFloatTOP];
	gJITC.nativeFloatRegState[r] = rsUnused; // not really mapped
	return r;
}

void FASTCALL jitcFloatRegisterClobberAll()
{
	if (!gJITC.nativeFloatTOP) return;
	
	do  {
		JitcFloatReg r = jitcFloatRegisterFromNative(Float_ST0);
		int creg = gJITC.nativeFloatRegStack[r];
		switch (gJITC.nativeFloatRegState[r]) {
		case rsDirty: {	
			asmFSTP_Double(&gCPU.fpr[creg]);
			gJITC.clientFloatReg[creg] = JITC_FLOAT_REG_NONE;
			break;
		}
		case rsMapped:
			asmFFREEP(Float_ST0);
			gJITC.clientFloatReg[creg] = JITC_FLOAT_REG_NONE;
			break;
		case rsUnused: {ASSERT(0);}
		}
	} while (--gJITC.nativeFloatTOP);
}

void FASTCALL jitcFloatRegisterStoreAndPopTOP(JitcFloatReg r)
{
	asmFSTP(jitcFloatRegisterToNative(r));
	gJITC.nativeFloatTOP--;
}

void FASTCALL jitcClobberClientRegisterForFloat(int creg)
{
	NativeReg r = jitcGetClientRegisterMapping(PPC_FPR_U(creg));
	if (r != REG_NO) jitcClobberRegister(r | NATIVE_REG);
	r = jitcGetClientRegisterMapping(PPC_FPR_L(creg));
	if (r != REG_NO) jitcClobberRegister(r | NATIVE_REG);
}

void FASTCALL jitcInvalidateClientRegisterForFloat(int creg)
{
	// FIXME: no need to clobber, invalidate would be enough
	jitcClobberClientRegisterForFloat(creg);
}

JitcFloatReg FASTCALL jitcGetClientFloatRegisterMapping(int creg)
{
	return gJITC.clientFloatReg[creg];
}

JitcFloatReg FASTCALL jitcGetClientFloatRegisterUnmapped(int creg, int hint1, int hint2)
{
	JitcFloatReg r = jitcGetClientFloatRegisterMapping(creg);
	if (r == JITC_FLOAT_REG_NONE) {
		if (gJITC.nativeFloatTOP == 8) {
			jitcPopFloatStack(hint1, hint2);
		}
		r = jitcPushFloatStack(creg);
		gJITC.nativeFloatRegState[r] = rsUnused;
	}
	return r;
}

JitcFloatReg FASTCALL jitcGetClientFloatRegister(int creg, int hint1, int hint2)
{
	JitcFloatReg r = jitcGetClientFloatRegisterMapping(creg);
	if (r == JITC_FLOAT_REG_NONE) {
		if (gJITC.nativeFloatTOP == 8) {
			jitcPopFloatStack(hint1, hint2);
		}
		r = jitcPushFloatStack(creg);
		gJITC.clientFloatReg[creg] = r;
		gJITC.nativeFloatRegStack[r] = creg;
		gJITC.nativeFloatRegState[r] = rsMapped;
	}
	return r;
}

JitcFloatReg FASTCALL jitcMapClientFloatRegisterDirty(int creg, JitcFloatReg freg)
{
	if (freg == JITC_FLOAT_REG_NONE) {
		freg = jitcFloatRegisterFromNative(Float_ST0);
	}
	gJITC.clientFloatReg[creg] = freg;
	gJITC.nativeFloatRegStack[freg] = creg;
	gJITC.nativeFloatRegState[freg] = rsDirty;
	return freg;
}

/*
 *
 */
NativeAddress FASTCALL asmHERE()
{
	return gJITC.currentPage->tcp;
}

void FASTCALL asmNOP(int n)
{
	if (n <= 0) return;
	byte instr[15];
	for (int i=0; i < (n-1); i++) {
		instr[i] = 0x66;
	}
	instr[n-1] = 0x90;
	jitcEmit(instr, n);	
}

static uint mkmodrm(byte *instr, NativeReg r, uint32 disp)
{
	if (r == REG_NO) {
		instr[0] |= 0x05;
		*(uint32 *)(instr+1) = disp;
		return 5;		
	} else if (r == ESP) {
		if (disp == 0) {
			instr[0] |= 0x04;
			instr[1] = 0x24;
			return 2;
		} else if (disp < 0x80 || disp >= 0xffffff80) {
			instr[0] |= 0x44;
			instr[1] = 0x24;
			instr[2] = disp;
			return 3;
		} else {
			instr[0] |= 0x84;
			instr[1] = 0x24;
			*(uint32 *)(instr+2) = disp;
			return 6;
		}
	} else if (r == EBP) {
		if (disp < 0x80 || disp >= 0xffffff80) {
			instr[0] |= 0x45;
			instr[1] = disp;
			return 2;
		} else {
			instr[0] |= 0x85;
			*(uint32 *)(instr+1) = disp;
			return 5;
		}
	} else {
		if (disp == 0) {
			instr[0] |= r;
			return 1;
		} else if (disp < 0x80 || disp >= 0xffffff80) {
			instr[0] |= 0x40+r;
			instr[1] = disp;
			return 2;
		} else {
			instr[0] |= 0x80+r;
			*(uint32 *)(instr+1) = disp;
			return 5;
		}
	}
}

static uint mksib(byte *instr, NativeReg base, int scale, NativeReg index, uint32 disp)
{
	uint mod, ss;
	if (disp == 0) {
		mod = 0;
	} else if (disp < 0x80 || disp >= 0xffffff80) {
		mod = 1;
	} else {
		mod = 2;
	}
	if (base == REG_NO && index != REG_NO) {
		switch (scale) {
		case 2: case 3: case 5: case 9:
			scale--;
			base = index;
			break;
		}
	}
	if (index == ESP) {
		/* if (scale > 1) not allowed! */
		if (scale == 1) {
			/* if (base == REG_SP) not allowed!; */
			NativeReg temp = index;
			index = base;
			base = temp;
		}
	}
	if (index != REG_NO) {
		static byte scales[] = {0, 0, 1, 0, 2, 0, 0, 0, 3};
		ss = scales[scale];
	} else {
		ss = 0;
		index = ESP;
	}
	uint dmod = mod;
	if (base == REG_NO) {
		base = EBP;
		dmod = 2;
	} else {
		if (base == EBP && mod == 0) {
			dmod = mod = 1;
		}
	}
	instr[0] |= (mod << 6) | 4;
	instr[1] = (ss << 6) | (index << 3) | base;
	switch (dmod) {
	case 1:
		instr[2] = disp;
		return 3;
	case 2:
		*((uint32 *)&instr[2]) = disp;
		return 6;
	}
	return 2;
}

void FASTCALL asmTEST32(NativeReg base, uint32 disp, uint32 imm)
{
	byte instr[15];
	uint32 mask = 0xff;
	uint len;
	for (uint i=0; i<4; i++) {
		// optimize test dword ptr [x], 0xff -> test byte ptr
		if ((imm & ~mask) == 0) {
			instr[0] = 0xf6;
			instr[1] = 0;
			len = mkmodrm(instr+1, base, disp+i)+1;
			instr[len] = imm >> (i*8);
			jitcEmit(instr, len+1);
			return;
		}
		mask <<= 8;
	}
	instr[0] = 0xf7;
	instr[1] = 0;
	len = mkmodrm(instr+1, base, disp)+1;
	*((uint32 *)&instr[len]) = imm;
	jitcEmit(instr, len+4);
}

void FASTCALL asmOR32(NativeReg base, uint32 disp, uint32 imm)
{
	byte instr[15];
	uint32 mask = 0xff;
	uint len;
	for (uint i=0; i<4; i++) {
		// optimize or dword ptr [x], 0xff -> or byte ptr
		if ((imm & ~mask) == 0) {
			instr[0] = 0x80;
			instr[1] = 0x08;
			len = mkmodrm(instr+1, base, disp+i)+1;
			instr[len] = imm >> (i*8);
			jitcEmit(instr, len+1);
			return;
		}
		mask <<= 8;
	}
	instr[0] = 0x81;
	instr[1] = 0x08;
	len = mkmodrm(instr+1, base, disp)+1;
	*((uint32 *)&instr[len]) = imm;
	jitcEmit(instr, len+4);
}

void FASTCALL asmAND32(NativeReg base, uint32 disp, uint32 imm)
{
	byte instr[15];
	uint32 mask = 0xff;
	uint len;
	for (uint i=0; i<4; i++) {
		// optimize and dword ptr [x], 0xffffff00 -> and byte ptr
		if ((imm & ~mask) == ~mask) {
			instr[0] = 0x80;
			instr[1] = 0x20;
			len = mkmodrm(instr+1, base, disp+i)+1;
			instr[len] = imm >> (i*8);
			jitcEmit(instr, len+1);
			return;
		}
		mask <<= 8;
	}
	instr[0] = 0x81;
	instr[1] = 0x20;
	len = mkmodrm(instr+1, base, disp)+1;
	*((uint32 *)&instr[len]) = imm;
	jitcEmit(instr, len+4);
}

static void asmSimpleMODRM32(uint8 opc, NativeReg reg1, NativeReg reg2)
{
	byte instr[2] = {opc, 0xc0+(reg1<<3)+reg2};
	jitcEmit(instr, sizeof(instr));
}

static void asmSimpleMODRM8(uint8 opc, NativeReg8 reg1, NativeReg8 reg2)
{
	byte instr[2] = {opc, 0xc0+(reg1<<3)+reg2};
	jitcEmit(instr, sizeof(instr));
}

void FASTCALL asmALU32(X86ALUopc opc, NativeReg reg1, NativeReg reg2)
{
	switch (opc) {
	case X86_MOV: 
		asmSimpleMODRM32(0x8b, reg1, reg2);
	        break;
	case X86_TEST:
		asmSimpleMODRM32(0x85, reg1, reg2);
	        break;
	case X86_XCHG:
		if (reg1 == EAX) {
			jitcEmit1(0x90+reg2);
		} else if (reg2 == EAX) {
			jitcEmit1(0x90+reg1);
		} else {
			asmSimpleMODRM32(0x87, reg1, reg2);
		}
	        break;
	default:
		asmSimpleMODRM32(0x03+(opc<<3), reg1, reg2);
	}	
}

void FASTCALL asmALU8(X86ALUopc opc, NativeReg8 reg1, NativeReg8 reg2)
{
	switch (opc) {
	case X86_MOV: 
		asmSimpleMODRM8(0x8a, reg1, reg2);
	        break;
	case X86_TEST:
		asmSimpleMODRM8(0x84, reg1, reg2);
	        break;
	case X86_XCHG:
		asmSimpleMODRM8(0x86, reg1, reg2);
	        break;
	default:
		asmSimpleMODRM8(0x02+(opc<<3), reg1, reg2);
	}	
}

void FASTCALL asmSimpleALU32(X86ALUopc opc, NativeReg reg, uint32 imm)
{
	if (imm <= 0x7f || imm >= 0xffffff80) {
		byte instr[3] = {0x83, 0xc0+(opc<<3)+reg, imm};
		jitcEmit(instr, sizeof(instr));
	} else {
		if (reg == EAX) {
			byte instr[5];
			instr[0] = 0x05+(opc<<3);
			*((uint32 *)&instr[1]) = imm;
			jitcEmit(instr, sizeof(instr));
		} else {
			byte instr[6];
			instr[0] = 0x81;
			instr[1] = 0xc0+(opc<<3)+reg;
			*((uint32 *)&instr[2]) = imm;
			jitcEmit(instr, sizeof(instr));
		}
	}
}

void FASTCALL asmTEST32(NativeReg reg1, uint32 imm)
{
	if (reg1 == EAX) {
		byte instr[5];
		instr[0] = 0xa9;
		*((uint32 *)&instr[1]) = imm;
		jitcEmit(instr, sizeof(instr));
	} else {
		byte instr[6];
		instr[0] = 0xf7;
		instr[1] = 0xc0+reg1;
		*((uint32 *)&instr[2]) = imm;
		jitcEmit(instr, sizeof(instr));
	}
}

void FASTCALL asmALU32(X86ALUopc opc, NativeReg reg, uint32 imm)
{
	switch (opc) {
	case X86_MOV:
		if (imm == 0) {
			asmALU32(X86_XOR, reg, reg);
		} else {
			asmMOV32_NoFlags(reg, imm);
		}
		break;
	case X86_TEST:
		asmTEST32(reg, imm);
		break;
	case X86_XCHG:
		// internal error
		break;
	default:
		asmSimpleALU32(opc, reg, imm);
	}
}

void FASTCALL asmALU8(X86ALUopc opc, NativeReg8 reg, uint8 imm)
{
	switch (opc) {
	case X86_MOV: {
		byte instr[2];
		instr[0] = 0xb0+reg;
		instr[1] = imm;
		jitcEmit(instr, sizeof instr);
		break;
	}	
	case X86_TEST:
		if (reg == AL) {
			byte instr[2];
			instr[0] = 0xa8;
			instr[1] = imm;
			jitcEmit(instr, sizeof instr);
		} else {
			byte instr[3];
			instr[0] = 0xf6;
			instr[1] = 0xc0+reg;
			instr[2] = imm;
			jitcEmit(instr, sizeof instr);
		}
	default:
		if (reg == AL) {
			byte instr[2];
			instr[0] = 0x04+(opc<<3);
			instr[1] = imm;
			jitcEmit(instr, sizeof instr);
		} else {
			byte instr[3];
			instr[0] = 0x80;
			instr[1] = 0xc0+(opc<<3)+reg;
			instr[2] = imm;
			jitcEmit(instr, sizeof instr);
		}
	}
}

void FASTCALL asmMOV32_NoFlags(NativeReg reg1, uint32 imm)
{
	byte instr[5];
	instr[0] = 0xb8+reg1;
	*((uint32 *)&instr[1]) = imm;
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmALU16(X86ALUopc opc, NativeReg16 reg, const void *mem)
{
	byte o;
	switch (opc) {
		case X86_MOV: o = 0x8b; break;
		case X86_TEST: o = 0x85; break;
		default: o = (opc<<3)+3;
	} 
	byte instr[3+4] = {0x66, o, 0x05+(reg<<3)};
	*((uint32 *)&instr[3]) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmALU16(X86ALUopc opc, NativeReg16 reg, uint16 imm)
{
	byte o;
	byte o2 = 0;
	switch (opc) {
		case X86_MOV: o = 0xc7; break;
		case X86_TEST: o = 0xf7; break;
		default: o = 0x81; o2 = opc<<3;
	} 
	byte instr[3+2] = {0x66, o, 0xc0+reg+o2};
	*((uint16 *)&instr[3]) = imm;
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmALU16(X86ALUopc opc, const void *mem, NativeReg16 reg)
{
	byte o;
	switch (opc) {
		case X86_MOV: o = 0x89; break;
		case X86_TEST: o = 0x85; break;
		default: o = (opc<<3)+1;
	} 
	byte instr[3+4] = {0x66, o, 0x05+(reg<<3)};
	*((uint32 *)&instr[3]) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmALU32(X86ALUopc1 opc, NativeReg reg)
{
	byte instr[2] = {0xf7, opc+reg};
	jitcEmit(instr, 2);
}

void FASTCALL asmALU32(X86ALUopc opc, NativeReg reg, NativeReg base, uint32 disp)
{	
	byte instr[15];
	uint len=0;
	switch (opc) {
	case X86_MOV:
		instr[len] = 0x8b;
		break;
	case X86_LEA:
		instr[len] = 0x8d;
		break;
	case X86_XCHG:
		instr[len] = 0x87;
		break;
	case X86_TEST:
		instr[len] = 0x85;
		break;
	default:
		instr[len] = 0x03+(opc<<3);
	}
	len++;
	instr[len] = (reg&7)<<3;
	len += mkmodrm(instr+len, base, disp);
	jitcEmit(instr, len);
}

void FASTCALL asmALU8(X86ALUopc opc, NativeReg8 reg, NativeReg base, uint32 disp)
{
	byte instr[15];
	uint len=0;
	switch (opc) {
	case X86_MOV:
		instr[len] = 0x8a;
		break;
	case X86_XCHG:
		instr[len] = 0x86;
		break;
	case X86_TEST:
		instr[len] = 0x84;
		break;
	default:
		instr[len] = 0x02+(opc<<3);
	}
	len++;
	instr[len] = (reg&7)<<3;
	len += mkmodrm(instr+len, base, disp);
	jitcEmit(instr, len);
}

void FASTCALL asmALU32(X86ALUopc opc, NativeReg base, uint32 disp, NativeReg reg)
{	
	byte instr[15];
	uint len=0;
	switch (opc) {
	case X86_MOV:
		instr[len] = 0x89;
		break;
	case X86_XCHG:
		instr[len] = 0x87;
		break;
	case X86_TEST:
		instr[len] = 0x85;
		break;
	default:
		instr[len] = 0x01+(opc<<3);
	}
	len++;
	instr[len] = reg<<3;
	len += mkmodrm(instr+len, base, disp);
	jitcEmit(instr, len);
}

void FASTCALL asmALU8(X86ALUopc opc, NativeReg base, uint32 disp, NativeReg8 reg)
{
	byte instr[15];
	uint len=0;
	switch (opc) {
	case X86_MOV:
		instr[len] = 0x88;
		break;
	case X86_XCHG:
		instr[len] = 0x86;
		break;
	case X86_TEST:
		instr[len] = 0x84;
		break;
	default:
		instr[len] = 0x00+(opc<<3);
	}
	len++;
	instr[len] = reg<<3;
	len += mkmodrm(instr+len, base, disp);
	jitcEmit(instr, len);
}

void FASTCALL asmALU32(X86ALUopc opc, NativeReg reg, NativeReg base, int scale, NativeReg index, uint32 disp)
{
	byte instr[15];
	uint len = 0;
	switch (opc) {
	case X86_MOV:
		instr[len] = 0x8b;
		break;
	case X86_LEA:
		instr[len] = 0x8d;
		break;
	case X86_XCHG:
		instr[len] = 0x87;
		break;
	case X86_TEST:
		instr[len] = 0x85;
		break;
	default:
		instr[len] = 0x03+(opc<<3);
	}
	len++;
	instr[len] = reg << 3;
	len += mksib(instr+len, base, scale, index, disp);
	jitcEmit(instr, len);
	
}

void FASTCALL asmALU32(X86ALUopc opc, NativeReg base, int scale, NativeReg index, uint32 disp, NativeReg reg)
{
	byte instr[15];
	uint len = 0;
	switch (opc) {
	case X86_MOV:
		instr[len] = 0x89;
		break;
	case X86_XCHG:
		instr[len] = 0x87;
		break;
	case X86_TEST:
		instr[len] = 0x85;
		break;
	default:
		instr[len] = 0x01+(opc<<3);
	}
	len++;
	instr[len] = (reg & 7) << 3;
	len += mksib(instr+len, base, scale, index, disp);
	jitcEmit(instr, len);
}

void FASTCALL asmSimpleALU32(X86ALUopc opc, NativeReg base, uint32 disp, uint32 imm)
{
	byte instr[15];
	uint len = 0;
	if (imm <= 0x7f || imm >= 0xffffff80) {
		instr[len++] = 0x83;
		instr[len] = (opc<<3);
		len += mkmodrm(instr+len, base, disp);
		instr[len] = imm;
		jitcEmit(instr, len+1);
	} else {
		instr[len++] = 0x81;
		instr[len] = (opc<<3);
		len += mkmodrm(instr+len, base, disp);
		*((uint32 *)&instr[len]) = imm;
		jitcEmit(instr, len+4);
	}
}

void FASTCALL asmALU32(X86ALUopc opc, NativeReg base, uint32 disp, uint32 imm)
{
	byte instr[15];
	uint len = 0;
	switch (opc) {
	case X86_MOV: {
		instr[len++] = 0xc7;
		instr[len] = 0;
		len += mkmodrm(instr+len, base, disp);
		*((uint32 *)&instr[len]) = imm;		
		jitcEmit(instr, len+4);
		break;
	}
	case X86_XCHG:
		// internal error
		break;
	case X86_TEST:
		asmTEST32(base, disp, imm);
		break;
	default:
		asmSimpleALU32(opc, base, disp, imm);
	}
}

void FASTCALL asmALU8(X86ALUopc opc, NativeReg base, uint32 disp, uint8 imm)
{
	byte instr[15];
	uint len = 0;
	switch (opc) {
	case X86_MOV:
		instr[len] = 0xc6;
		break;
	case X86_XCHG:
		// internal error
		break;
	case X86_TEST:
		instr[len] = 0xf6;
		break;
	default:
		instr[len++] = 0x80;
		instr[len] = (opc<<3);
		len += mkmodrm(instr+len, base, disp);
		instr[len] = imm;
		jitcEmit(instr, len+1);
		return;
	}
	len++;
	instr[len] = 0;
	len += mkmodrm(instr+len, base, disp);
	instr[len] = imm;
	jitcEmit(instr, len+1);
}

void FASTCALL asmMOVxx32_16(X86MOVxx opc, NativeReg reg1, NativeReg reg2)
{
	byte instr[3] = {0x0f, opc+1, 0xc0+(reg1<<3)+reg2};
	jitcEmit(instr, sizeof(instr));
}

void FASTCALL asmMOVxx32_16(X86MOVxx opc, NativeReg reg1, const void *mem)
{
	byte instr[3+4] = {0x0f, opc+1, 0x05+(reg1<<3)};
	*((uint32 *)&instr[3]) = uint32(mem);
	jitcEmit(instr, sizeof(instr));
}

void FASTCALL asmMOVxx32_8(X86MOVxx opc, NativeReg reg1, NativeReg8 reg2)
{
	byte instr[3] = {0x0f, opc, 0xc0+(reg1<<3)+reg2};
	jitcEmit(instr, sizeof(instr));
}

void FASTCALL asmMOVxx32_8(X86MOVxx opc, NativeReg reg1, const void *mem)
{
	byte instr[3+4] = {0x0f, opc, 0x05+(reg1<<3)};
	*((uint32 *)&instr[3]) = uint32(mem);
	jitcEmit(instr, sizeof(instr));
}

void FASTCALL asmSET8(X86FlagTest flags, NativeReg8 reg)
{
	byte instr[3] = {0x0f, 0x90+flags, 0xc0+reg};
	jitcEmit(instr, sizeof(instr));
}

void FASTCALL asmSET8(X86FlagTest flags, NativeReg base, uint32 disp)
{
	byte instr[15];
	uint len = 0;
	instr[len++] = 0x0f;
	instr[len++] = 0x90+flags;
	instr[len] = 0;
	len += mkmodrm(instr+len, base, disp);
	jitcEmit(instr, len);
}

void FASTCALL asmCMOV32(X86FlagTest flags, NativeReg reg1, NativeReg reg2)
{
	byte instr[3] = {0x0f, 0x40+flags, 0xc0+(reg1<<3)+reg2};
	jitcEmit(instr, sizeof(instr));
}

void FASTCALL asmCMOV32(X86FlagTest flags, NativeReg reg, NativeReg base, uint32 disp)
{
	byte instr[15];
	uint len = 0;
	instr[len++] = 0x0f;
	instr[len++] = 0x40+flags;
	instr[len] = 0;
	len += mkmodrm(instr+len, base, disp);
	jitcEmit(instr, len);
}

void FASTCALL asmShift32(X86ShiftOpc opc, NativeReg reg, uint imm)
{
	if (imm == 1) {
		byte instr[2] = {0xd1, 0xc0+opc+reg};
		jitcEmit(instr, sizeof(instr));
	} else {
		byte instr[3] = {0xc1, 0xc0+opc+reg, imm};
		jitcEmit(instr, sizeof(instr));
	}
}

void FASTCALL asmShift16(X86ShiftOpc opc, NativeReg reg, uint imm)
{
	if (imm == 1) {
		byte instr[3] = {0x66, 0xd1, 0xc0+opc+reg};
		jitcEmit(instr, sizeof(instr));
	} else {
		byte instr[4] = {0x66, 0xc1, 0xc0+opc+reg, imm};
		jitcEmit(instr, sizeof(instr));
	}
}

void FASTCALL asmShift32CL(X86ShiftOpc opc, NativeReg reg)
{
	byte instr[2] = {0xd3, 0xc0+opc+reg};
	jitcEmit(instr, sizeof(instr));
}

void FASTCALL asmShift16CL(X86ShiftOpc opc, NativeReg16 reg)
{
	byte instr[3] = {0x66, 0xd3, 0xc0+opc+reg};
	jitcEmit(instr, sizeof(instr));
}

void FASTCALL asmShift8CL(X86ShiftOpc opc, NativeReg8 reg)
{
	byte instr[2] = {0xd2, 0xc0+opc+reg};
	jitcEmit(instr, sizeof(instr));
}

void FASTCALL asmIMUL32(NativeReg reg1, NativeReg reg2, uint32 imm)
{
	if (imm <= 0x7f || imm >= 0xffffff80) {	
		byte instr[3] = {0x6b, 0xc0+(reg1<<3)+reg2, imm};
		jitcEmit(instr, sizeof(instr));
	} else {
		byte instr[6] = {0x69, 0xc0+(reg1<<3)+reg2};
		*((uint32*)(&instr[2])) = imm;
		jitcEmit(instr, sizeof(instr));
	}
}


void FASTCALL asmIMUL32(NativeReg reg1, NativeReg reg2)
{
	byte instr[3] = {0x0f, 0xaf, 0xc0+(reg1<<3)+reg2};
	jitcEmit(instr, sizeof(instr));
}

void FASTCALL asmINC32(NativeReg reg1)
{
	jitcEmit1(0x40+reg1);
}

void FASTCALL asmDEC32(NativeReg reg1)
{
	jitcEmit1(0x48+reg1);
}

void FASTCALL asmBTx32(X86BitTest opc, NativeReg reg1, int value)
{
	byte instr[4] = {0x0f, 0xba, 0xc0+(opc<<3)+reg1, value};
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmBTx32(X86BitTest opc, NativeReg reg1, uint32 disp, int value)
{
	byte instr[15];
	instr[0] = 0x0f;
	instr[1] = 0xba;
	instr[2] = opc << 3;
	uint len = mkmodrm(instr+2, reg1, disp)+2;
	instr[len] = value;
	jitcEmit(instr, len+1);
}

void FASTCALL asmBTx32(X86BitTest opc, const void *mem, int value)
{
	byte instr[3+4+1] = {0x0f, 0xba, 0x05+(opc<<3)};
	*((uint32*)(&instr[3])) = uint32(mem);
	instr[7] = value;
	jitcEmit(instr, sizeof instr);	
}

void FASTCALL asmBSx32(X86BitSearch opc, NativeReg reg1, NativeReg reg2)
{
	byte instr[3] = {0x0f, opc, 0xc0+(reg1<<3)+reg2};
	jitcEmit(instr, sizeof(instr));
}

void FASTCALL asmBSWAP32(NativeReg reg)
{
	byte instr[3];
	instr[0] = 0x0f;
	instr[1] = 0xc8+reg;
	jitcEmit(instr, 2);	
}

void FASTCALL asmJMP(NativeAddress to)
{
	/*
	 *	We use emitAssure here, since
	 *	we have to know the exact address of the jump
	 *	instruction (since it is relative)
	 */
restart:
	byte instr[5];
	uint32 rel = uint32(to - (gJITC.currentPage->tcp+2));
	if (rel <= 0x7f || rel >= 0xffffff80) {	
		if (!jitcEmitAssure(2)) goto restart;
		instr[0] = 0xeb;
		instr[1] = rel;
		jitcEmit(instr, 2);
	} else {
		if (!jitcEmitAssure(5)) goto restart;
		instr[0] = 0xe9;
		*((uint32 *)&instr[1]) = uint32(to - (gJITC.currentPage->tcp+5));
		jitcEmit(instr, 5);
	}
}

void FASTCALL asmJxx(X86FlagTest flags, NativeAddress to)
{
restart:
	byte instr[6];
	uint32 rel = uint32(to - (gJITC.currentPage->tcp+2));
	if (rel <= 0x7f || rel >= 0xffffff80) {
		if (!jitcEmitAssure(2)) goto restart;
		instr[0] = 0x70+flags;
		instr[1] = rel;
		jitcEmit(instr, 2);
	} else {
		if (!jitcEmitAssure(6)) goto restart;
		instr[0] = 0x0f;
		instr[1] = 0x80+flags;
		*((uint32 *)&instr[2]) = uint32(to - (gJITC.currentPage->tcp+6));
		jitcEmit(instr, 6);
	}
}

NativeAddress FASTCALL asmJMPFixup()
{
	byte instr[5];
	instr[0] = 0xe9;
#ifdef JITC_DEBUG
	memset(instr+1, 0, 4);
#endif
	jitcEmit(instr, 5);
	return gJITC.currentPage->tcp - 4;
}

NativeAddress FASTCALL asmJxxFixup(X86FlagTest flags)
{
	byte instr[6];
	instr[0] = 0x0f;
	instr[1] = 0x80+flags;
#ifdef JITC_DEBUG
	memset(instr+2, 0, 4);
#endif
	jitcEmit(instr, 6);
	return gJITC.currentPage->tcp - 4;
}

void FASTCALL asmResolveFixup(NativeAddress at, NativeAddress to)
{
	if (to == 0) {
		to = gJITC.currentPage->tcp;
	}
	*((uint32 *)at) = uint32(to) - (uint32(at) + 4);
}

void FASTCALL asmCALL(NativeAddress to)
{
	jitcEmitAssure(5);
	byte instr[5];
	instr[0] = 0xe8;
	*((uint32 *)&instr[1]) = uint32(to - (gJITC.currentPage->tcp+5));
	jitcEmit(instr, 5);
}

void FASTCALL asmSimple(X86SimpleOpc simple)
{
	if (simple > 0xff) {
		jitcEmit((byte*)&simple, 2);
	} else {
		jitcEmit1(simple);
	}
}

/*
 *	Maps one client vector register to one native vector register
 *	Will never emit any code.
 */
static inline void FASTCALL jitcMapVectorRegister(NativeVectorReg nreg, JitcVectorReg creg)
{
	//printf("*** map: XMM%u (vr%u)\n", nreg, creg);
	gJITC.n2cVectorReg[nreg] = creg;
	gJITC.c2nVectorReg[creg] = nreg;

	gJITC.nativeVectorRegState[nreg] = rsMapped;
}

/*
 *	Unmaps the native vector register from any client vector register
 *	Will never emit any code.
 */
static inline void FASTCALL jitcUnmapVectorRegister(NativeVectorReg nreg)
{
	JitcVectorReg creg = gJITC.n2cVectorReg[nreg];

	if (nreg != VECTREG_NO && creg != PPC_VECTREG_NO) {
		//printf("*** unmap: XMM%u (vr%u)\n", nreg, gJITC.n2cVectorReg[nreg]);

		gJITC.n2cVectorReg[nreg] = PPC_VECTREG_NO;
		gJITC.c2nVectorReg[creg] = VECTREG_NO;

		gJITC.nativeVectorRegState[nreg] = rsUnused;
	}
}

/*
 *	Marks the native vector register as dirty.
 *	Does *not* touch native vector register.
 *	Will not produce code.
 */
void FASTCALL jitcDirtyVectorRegister(NativeVectorReg nreg)
{
	JitcVectorReg creg = gJITC.n2cVectorReg[nreg];

	//printf("*** dirty(%u) with creg = %u\n", nreg, creg);

	if (creg == JITC_VECTOR_NEG1 || creg == PPC_VECTREG_NO) {
		//printf("*** dirty: %u = %u or %u\n", creg, JITC_VECTOR_NEG1, PPC_REG_NO);
		return;
	}

	if (gJITC.nativeVectorRegState[nreg] == rsUnused) {
		printf("!!! Attemped dirty of an anonymous vector register!\n");
		return;
	}

	if (creg == gJITC.nativeVectorReg) {
		gJITC.nativeVectorReg = VECTREG_NO;
	}

	gJITC.nativeVectorRegState[nreg] = rsDirty;
}

/*
 *	Marks the native vector register as non-dirty.
 *	Does *not* flush native vector register.
 *	Will not produce code.
 */
static inline void FASTCALL jitcUndirtyVectorRegister(NativeVectorReg nreg)
{
	if (gJITC.nativeVectorRegState[nreg] > rsMapped) {
		//printf("*** undirty: XMM%u (vr%u)\n", nreg, gJITC.n2cVectorReg[nreg]);

		gJITC.nativeVectorRegState[nreg] = rsMapped;
	}
}

/*
 *	Loads a native vector register with its mapped value.
 *	Does not alter the native vector register's markings.
 *	Will always emit an load.
 */
static inline void FASTCALL jitcLoadVectorRegister(NativeVectorReg nreg)
{
	JitcVectorReg creg = gJITC.n2cVectorReg[nreg];

	if (creg == JITC_VECTOR_NEG1 && gJITC.hostCPUCaps.sse2) {
		//printf("*** load neg1: XMM%u\n", nreg);

		/* On a P4, we can load -1 far faster with logic */
		asmPALU(PALUD(X86_PCMPEQ), nreg, nreg);
		return;
	}

	//printf("*** load: XMM%u (vr%u)\n", nreg, creg);
	asmMOVAPS(nreg, &gCPU.vr[creg]);
}

/*
 *	Stores a native vector register to its mapped client vector register.
 *	Does not alter the native vector register's markings.
 *	Will always emit a store.
 */
static inline void FASTCALL jitcStoreVectorRegister(NativeVectorReg nreg)
{
	JitcVectorReg creg = gJITC.n2cVectorReg[nreg];

	if (creg == JITC_VECTOR_NEG1 || creg == PPC_VECTREG_NO)
		return;

	//printf("*** store: XMM%u (vr%u)\n", nreg, creg);

	asmMOVAPS(&gCPU.vr[creg], nreg);
}

/*
 *	Returns the native vector register that is mapped to the client
 *		vector register.
 *	Will never emit any code.
 */
NativeVectorReg FASTCALL jitcGetClientVectorRegisterMapping(JitcVectorReg creg)
{
	return gJITC.c2nVectorReg[creg];
}

/*
 *	Makes the vector register the least recently used vector register.
 *	Will never emit any code.
 */
static inline void FASTCALL jitcDiscardVectorRegister(NativeVectorReg nreg)
{
	NativeVectorReg lreg, mreg;

	mreg = gJITC.MRUvregs[nreg];
	lreg = gJITC.LRUvregs[nreg];

	// remove from the list
	gJITC.MRUvregs[lreg] = mreg;
	gJITC.LRUvregs[mreg] = lreg;

	mreg = gJITC.MRUvregs[XMM_SENTINEL];

	// insert into the list in the LRU spot
	gJITC.LRUvregs[nreg] = XMM_SENTINEL;
	gJITC.MRUvregs[nreg] = mreg;

	gJITC.LRUvregs[mreg] = nreg;
	gJITC.MRUvregs[XMM_SENTINEL] = nreg;
}

/*
 *	Makes the vector register the most recently used vector register.
 *	Will never emit any code.
 */
void FASTCALL jitcTouchVectorRegister(NativeVectorReg nreg)
{
	NativeVectorReg lreg, mreg;

	mreg = gJITC.MRUvregs[nreg];
	lreg = gJITC.LRUvregs[nreg];

	// remove from the list
	gJITC.MRUvregs[lreg] = mreg;
	gJITC.LRUvregs[mreg] = lreg;

	lreg = gJITC.LRUvregs[XMM_SENTINEL];

	// insert into the list in the LRU spot
	gJITC.MRUvregs[nreg] = XMM_SENTINEL;
	gJITC.LRUvregs[nreg] = lreg;

	gJITC.MRUvregs[lreg] = nreg;
	gJITC.LRUvregs[XMM_SENTINEL] = nreg;
}

/*
 *	Unmaps a native vector register, and marks it least recently used.
 *	Will not emit any code.
 */
void FASTCALL jitcDropSingleVectorRegister(NativeVectorReg nreg)
{
	jitcDiscardVectorRegister(nreg);
	jitcUnmapVectorRegister(nreg);
}

int FASTCALL jitcAssertFlushedVectorRegister(JitcVectorReg creg)
{
	NativeVectorReg nreg = gJITC.c2nVectorReg[creg];

	if (nreg != VECTREG_NO && gJITC.nativeVectorRegState[nreg] == rsDirty) {
		printf("!!! Unflushed vector XMM%u (vr%u)!\n", nreg, creg);
		return 1;
	}
	return 0;
}
int FASTCALL jitcAssertFlushedVectorRegisters()
{
	int ret = 0;

	for (JitcVectorReg i=0; i<32; i++)
		ret |= jitcAssertFlushedVectorRegister(i);

	return ret;
}

void FASTCALL jitcShowVectorRegisterStatus(JitcVectorReg creg)
{
	NativeVectorReg nreg = gJITC.c2nVectorReg[creg];

	if (nreg != VECTREG_NO) {
		int status = gJITC.nativeVectorRegState[nreg];
		char *text;

		if (status == rsUnused)
			text = "unused";
		else if (status == rsMapped)
			text = "mapped";
		else if (status == rsDirty)
			text = "dirty";
		else
			text = "unknown";

		//printf("*** vr%u => XMM%u (%s)\n", creg, nreg, text);
	} else {
		//printf("*** vr%u => memory\n", creg);
	}
}

/*
 *	If the native vector register is marked dirty, then it writes that
 *		value out to the client vector register store.
 *	Will produce a store, if the native vector register is dirty.
 */
static inline void FASTCALL jitcFlushSingleVectorRegister(NativeVectorReg nreg)
{
	if (gJITC.nativeVectorRegState[nreg] == rsDirty) {
		//printf("*** flush: XMM%u (vr%u)\n", nreg, gJITC.n2cVectorReg[nreg]);
		jitcStoreVectorRegister(nreg);
	}
}

/*
 *	Flushes the register, frees it, and makes it least recently used.
 *	Will produce a store, if the native vector register was dirty.
 */
static inline void FASTCALL jitcTrashSingleVectorRegister(NativeVectorReg nreg)
{
	if (gJITC.nativeVectorRegState[nreg] > rsUnused) {
		//printf("*** trash: XMM%u (vr%u)\n", nreg, gJITC.n2cVectorReg[nreg]);
	}

	jitcFlushSingleVectorRegister(nreg);
	jitcDropSingleVectorRegister(nreg);
}

/*
 *	Flushes the register, frees it, and makes it most recently used.
 *	Will produce a store, if the native vector register was dirty.
 */
static inline void FASTCALL jitcClobberSingleVectorRegister(NativeVectorReg nreg)
{
	if (gJITC.nativeVectorRegState[nreg] > rsUnused) {
		//printf("*** clobber: XMM%u (vr%u)\n", nreg, gJITC.n2cVectorReg[nreg]);
	}

	jitcFlushSingleVectorRegister(nreg);
	jitcTouchVectorRegister(nreg);
	jitcUnmapVectorRegister(nreg);
}

/*
 *	Allocates a native vector register.
 *	If hint is non-zero, then it indicates that the value is unlikely
 *		to be re-used soon, so to keep it at the end of the LRU.
 *	To use hints, pass hint == the number of temporary registers
 *	May produce a store, if no native vector registers are available.
 */
NativeVectorReg FASTCALL jitcAllocVectorRegister(int hint)
{
	NativeVectorReg nreg = gJITC.MRUvregs[XMM_SENTINEL];

	if (hint >= XMM_SENTINEL) {
		nreg = gJITC.LRUvregs[nreg];

		jitcTrashSingleVectorRegister(nreg);
	} else if (hint) {
		for (int i=1; i<hint; i++) {
			nreg = gJITC.MRUvregs[nreg];
		}

		jitcTrashSingleVectorRegister(nreg);
	} else {
		jitcClobberSingleVectorRegister(nreg);
	}

	return nreg;
}

/*
 *	Returns native vector register that contains value of client
 *		register or allocates new vector register which maps to
 *		the client register.
 *	Marks the register dirty.
 *
 *	May produce a store, if no registers are available.
 *	Will never produce a load.
 */
NativeVectorReg FASTCALL jitcMapClientVectorRegisterDirty(JitcVectorReg creg, int hint)
{
	NativeVectorReg nreg = gJITC.c2nVectorReg[creg];

	if (nreg == VECTREG_NO) {
		nreg = jitcAllocVectorRegister(hint);

		jitcMapVectorRegister(nreg, creg);
	} else if (hint) {
		jitcDiscardVectorRegister(nreg);
	} else {
		jitcTouchVectorRegister(nreg);
	}

	jitcDirtyVectorRegister(nreg);

	return nreg;
}

/*
 *	Returns native vector register that contains the value of the
 *		client vector register, or allocates new register, and
 *		loads this value into it.
 *
 *	May produce a store, if no register are available.
 *	May produce a load, if client vector register isn't mapped.
 */
NativeVectorReg FASTCALL jitcGetClientVectorRegister(JitcVectorReg creg, int hint)
{
	NativeVectorReg nreg = gJITC.c2nVectorReg[creg];

	if (nreg == VECTREG_NO) {
		nreg = jitcAllocVectorRegister(hint);
		jitcMapVectorRegister(nreg, creg);

		jitcLoadVectorRegister(nreg);
	} else if (hint) {
		jitcDiscardVectorRegister(nreg);
	} else {
		jitcTouchVectorRegister(nreg);
	}

	return nreg;
}

/*
 *	Returns native vector register that contains the value of the
 *		client vector register, or allocates new register, and
 *		loads this value into it.
 *	Will mark the native vector register as dirty.
 *
 *	May produce a store, if no register are available.
 *	May produce a load, if client vector register isn't mapped.
 */
NativeVectorReg FASTCALL jitcGetClientVectorRegisterDirty(JitcVectorReg creg, int hint)
{
	NativeVectorReg nreg = jitcGetClientVectorRegister(creg, hint);

	jitcDirtyVectorRegister(nreg);

	return nreg;
}

/*
 *	Flushes native vector register(s).
 *	Resets dirty flags.
 *	Will produce stores, if vector registers are dirty.
 */
void FASTCALL jitcFlushVectorRegister(int options)
{
	if (options == JITC_VECTOR_REGS_ALL) {
		for (unsigned int i = XMM0; i <= XMM7; i++) {
			jitcFlushSingleVectorRegister((NativeVectorReg)i);
			jitcUndirtyVectorRegister((NativeVectorReg)i);
		}
	} else if (options & NATIVE_REG) {
		NativeVectorReg nreg = (NativeVectorReg)(options & 0xf);

		jitcFlushSingleVectorRegister(nreg);
		jitcUndirtyVectorRegister(nreg);
	}
}

/*
 *	Flushes native vector register(s).
 *	Doesn't reset dirty flags.
 *	Will produce stores, if vector registers are dirty.
 */
void FASTCALL jitcFlushVectorRegisterDirty(int options)
{
	if (options == JITC_VECTOR_REGS_ALL) {
		for (unsigned int i = XMM0; i <= XMM7; i++) {
			jitcFlushSingleVectorRegister((NativeVectorReg)i);
		}
	} else if (options & NATIVE_REG) {
		NativeVectorReg nreg = (NativeVectorReg)(options & 0xf);

		jitcFlushSingleVectorRegister(nreg);
	}
}

/*
 *	Clobbers native vector register(s).
 *	Will produce stores, if vector registers are dirty.
 */
void FASTCALL jitcClobberVectorRegister(int options)
{
	if (options == JITC_VECTOR_REGS_ALL) {
		for (unsigned int i = XMM0; i <= XMM7; i++) {
			jitcClobberSingleVectorRegister((NativeVectorReg)i);
		}
	} else if (options & NATIVE_REG) {
		NativeVectorReg nreg = (NativeVectorReg)(options & 0xf);

		jitcClobberSingleVectorRegister(nreg);
	}
}

/*
 *	Trashes native vector register(s).
 *	Will produce stores, if vector registers are dirty.
 */
void FASTCALL jitcTrashVectorRegister(int options)
{
	if (options == JITC_VECTOR_REGS_ALL) {
		for (unsigned int i = XMM0; i <= XMM7; i++) {
			jitcTrashSingleVectorRegister((NativeVectorReg)i);
		}
	} else if (options & NATIVE_REG) {
		NativeVectorReg nreg = (NativeVectorReg)(options & 0xf);

		jitcTrashSingleVectorRegister(nreg);
	}
}

/*
 *	Drops native vector register(s).
 *	Will not produce any code.
 */
void FASTCALL jitcDropVectorRegister(int options)
{
	if (options == JITC_VECTOR_REGS_ALL) {
		for (unsigned int i = XMM0; i <= XMM7; i++) {
			jitcDropSingleVectorRegister((NativeVectorReg)i);
		}
	} else if (options & NATIVE_REG) {
		NativeVectorReg nreg = (NativeVectorReg)(options & 0xf);

		jitcDropSingleVectorRegister(nreg);
	}
}

void FASTCALL jitcFlushClientVectorRegister(JitcVectorReg creg)
{
	NativeVectorReg nreg = gJITC.c2nVectorReg[creg];

	if (nreg != VECTREG_NO) {
		jitcFlushSingleVectorRegister(nreg);
		jitcUndirtyVectorRegister(nreg);
	}
}

void FASTCALL jitcTrashClientVectorRegister(JitcVectorReg creg)
{
	NativeVectorReg nreg = gJITC.c2nVectorReg[creg];

	if (nreg != VECTREG_NO) {
		jitcTrashSingleVectorRegister(nreg);
	}
}

void FASTCALL jitcClobberClientVectorRegister(JitcVectorReg creg)
{
	NativeVectorReg nreg = gJITC.c2nVectorReg[creg];

	if (nreg != VECTREG_NO) {
		jitcClobberSingleVectorRegister(nreg);
	}
}

void FASTCALL jitcDropClientVectorRegister(JitcVectorReg creg)
{
	NativeVectorReg nreg = gJITC.c2nVectorReg[creg];

	if (nreg != VECTREG_NO) {
		jitcDropSingleVectorRegister(nreg);
	}
}

/*
 *	Renames a native vector register to a different client register.
 *	Will not emit a load.
 *	May emit a reg->reg move, if the vector register was in memory.
 *	May emit a store, if the vector register was dirty
 */
NativeVectorReg FASTCALL jitcRenameVectorRegisterDirty(NativeVectorReg reg, JitcVectorReg creg, int hint)
{
	NativeVectorReg nreg = gJITC.c2nVectorReg[creg];

	if (nreg == reg) {
		/*	That's weird... it's already mapped...	*/
	} else if (nreg != VECTREG_NO) {
		/*	It's already in a register, so rather than losing
		 *	reg pool depth, just move the value.
		 */
		asmALUPS(X86_MOVAPS, nreg, reg);
	} else {
		/*	Otherwise, only the source register is in the reg
		 *	pool, so flush it, then remap it.
	 	*/
		JitcVectorReg reg2 = gJITC.n2cVectorReg[reg];

		if (reg2 != VECTREG_NO) {
			jitcFlushSingleVectorRegister(reg);
			jitcUnmapVectorRegister(reg);
		}

		nreg = reg;
		jitcMapVectorRegister(nreg, creg);
	}

	if (hint) jitcDiscardVectorRegister(nreg);
	else      jitcTouchVectorRegister(nreg);

	jitcDirtyVectorRegister(nreg);

	return nreg;
}

void FASTCALL asmMOVAPS(NativeVectorReg reg, const void *disp)
{
	byte instr[7] = {0x0f, 0x28, 0x05 | (reg << 3)};

	*((uint32 *)&instr[3]) = uint32(disp);

	jitcEmit(instr, 7);
}

void FASTCALL asmMOVAPS(const void *disp, NativeVectorReg reg)
{
	byte instr[7] = {0x0f, 0x29, 0x05 | (reg << 3)};

	*((uint32 *)&instr[3]) = uint32(disp);

	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmMOVUPS(NativeVectorReg reg, const void *disp)
{
	byte instr[7] = {0x0f, 0x10, 0x05 | (reg << 3)};

	*((uint32 *)&instr[3]) = uint32(disp);

	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmMOVUPS(const void *disp, NativeVectorReg reg)
{
	byte instr[7] = {0x0f, 0x11, 0x05 | (reg << 3)};

	*((uint32 *)&instr[3]) = uint32(disp);

	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmMOVSS(NativeVectorReg reg, const void *disp)
{
	byte instr[8] = {0xf3, 0x0f, 0x10, 0x05 | (reg << 3)};

	*((uint32 *)&instr[4]) = (uint32)disp;

	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmMOVSS(const void *mem, NativeVectorReg reg)
{
	byte instr[8] = {0xf3, 0x0f, 0x11, 0x05 | (reg << 3)};

	*((uint32 *)&instr[4]) = uint32(mem);
 
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmALUPS(X86ALUPSopc opc, NativeVectorReg reg1, NativeVectorReg reg2)
{
	byte instr[3] = {0x0f, opc,  0xc0 + (reg1 << 3) + reg2};

	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmALUPS(X86ALUPSopc opc, NativeVectorReg reg1, NativeReg base, uint32 disp)
{
	byte instr[15];
	instr[0] = 0x0f;
	instr[1] = opc;
	instr[2] = reg1<<3;

	jitcEmit(instr, mkmodrm(instr+2, base, disp)+2);
}

void FASTCALL asmPALU(X86PALUopc opc, NativeVectorReg reg1, NativeVectorReg reg2)
{
	byte instr[4] = {0x66, 0x0f, opc, 0xc0 + (reg1 << 3) + reg2};

	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmPALU(X86PALUopc opc, NativeVectorReg reg1, const void *mem)
{
	byte instr[8] = {0x66, 0x0f, opc, 0x05+(reg1 << 3)};

	*((uint32*)(&instr[4])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmSHUFPS(NativeVectorReg reg1, NativeVectorReg reg2, int order)
{
	byte instr[4] = {0x0f, 0xc6, 0xc0+(reg1<<3)+reg2, order};

	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmSHUFPS(NativeVectorReg reg1, const void *mem, int order)
{
	byte instr[3+4+1] = {0x0f, 0xc6, 0xc0+(reg1<<3)};

	*((uint32*)(&instr[3])) = uint32(mem);
	instr[7] = order;

	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmPSHUFD(NativeVectorReg reg1, NativeVectorReg reg2, int order)
{
	byte instr[5] = {0x66, 0x0f, 0x70, 0xc0+(reg1<<3)+reg2, order};

	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmPSHUFD(NativeVectorReg reg1, const void *mem, int order)
{
	byte instr[4+4+1] = {0x66, 0x0f, 0x70, 0x05+(reg1 << 3)};

	*((uint32*)(&instr[4])) = uint32(mem);
	instr[8] = order;

	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFComp(X86FloatCompOp op, NativeFloatReg sti)
{
	byte instr[2];

	memcpy(instr, &op, 2);
	instr[1] += sti;

	jitcEmit(instr, 2);
}

void FASTCALL asmFIComp(X86FloatICompOp op, const void *mem)
{
	byte instr[6];

	instr[0] = op;
	instr[1] = 0x05 | 2<<3;
	*((uint32*)(&instr[2])) = uint32(mem);

	jitcEmit(instr, sizeof instr);

}

void FASTCALL asmFICompP(X86FloatICompOp op, const void *mem)
{
	byte instr[6];

	instr[0] = op;
	instr[1] = 0x05 | 3<<3;
	*((uint32*)(&instr[2])) = uint32(mem);

	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFArith(X86FloatArithOp op, const void *mem)
{
	int mod = 0;
	switch (op) {
	case X86_FADD:
		mod = 0;
		break;
	case X86_FMUL:
		mod = 1;
		break;
	case X86_FDIV:
		mod = 6;
		break;
	case X86_FDIVR:
		mod = 7;
		break;
	case X86_FSUB:
		mod = 4;
		break;
	case X86_FSUBR:
		mod = 5;
		break;
	}
	byte instr[6];
	instr[0] = 0xdc;
	instr[1] = 0x05 | (mod<<3);
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);	
}

void FASTCALL asmFArith_ST0(X86FloatArithOp op, NativeFloatReg sti)
{
	byte instr[2] = {0xd8, op+sti};
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFArith_STi(X86FloatArithOp op, NativeFloatReg sti)
{
	byte instr[2] = {0xdc, op+sti};
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFArithP_STi(X86FloatArithOp op, NativeFloatReg sti)
{
	byte instr[2] = {0xde, op+sti};
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFXCH(NativeFloatReg sti)
{
	byte instr[2] = {0xd9, 0xc8+sti};
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFFREE(NativeFloatReg sti)
{
	byte instr[2] = {0xdd, 0xc0+sti};
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFFREEP(NativeFloatReg sti)
{
	/* 
	 * AMD says:
	 * "Note that the FREEP instructions, although insufficiently 
	 * documented in the past, is supported by all 32-bit x86 processors."
	 */
	byte instr[2] = {0xdf, 0xc0+sti};
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFSimple(X86FloatOp op)
{
	jitcEmit((byte*)&op, 2);
}

void FASTCALL asmFLD_Single(const void *mem)
{
	byte instr[6];
	instr[0] = 0xd9;
	instr[1] = 0x05;
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFLD_Double(const void *mem)
{
	byte instr[6];
	instr[0] = 0xdd;
	instr[1] = 0x05;
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFLD(NativeFloatReg sti)
{
	byte instr[2] = {0xd9, 0xc0+sti};
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFILD_W(const void *mem)
{
	byte instr[6];
	instr[0] = 0xdf;
	instr[1] = 0x05;
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFILD_D(const void *mem)
{
	byte instr[6];
	instr[0] = 0xdb;
	instr[1] = 0x05;
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFILD_Q(const void *mem)
{
	byte instr[6];
	instr[0] = 0xdf;
	instr[1] = 0x05 | (5<<3);
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFST_Single(const void *mem)
{
	byte instr[6];
	instr[0] = 0xd9;
	instr[1] = 0x05 | (2<<3);
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFSTP_Single(const void *mem)
{
	byte instr[6];
	instr[0] = 0xd9;
	instr[1] = 0x05 | (3<<3);
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFST_Double(const void *mem)
{
	byte instr[6];
	instr[0] = 0xdd;
	instr[1] = 0x05 | (2<<3);
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFSTP_Double(const void *mem)
{
	byte instr[6];
	instr[0] = 0xdd;
	instr[1] = 0x05 | (3<<3);
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFST(NativeFloatReg sti)
{
	byte instr[2] = {0xdd, 0xd0+sti};
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFSTP(NativeFloatReg sti)
{
	byte instr[2] = {0xdd, 0xd8+sti};
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFISTP_W(const void *mem)
{
	byte instr[6];
	instr[0] = 0xdf;
	instr[1] = 0x05 | (3<<3);
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFISTP_D(const void *mem)
{
	byte instr[6];
	instr[0] = 0xdb;
	instr[1] = 0x05 | (3<<3);
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFISTP_Q(const void *mem)
{
	byte instr[6];
	instr[0] = 0xdf;
	instr[1] = 0x05 | (7<<3);
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFISTTP(const void *mem)
{
	byte instr[6];
	instr[0] = 0xdb;
	instr[1] = 0x05 | (1<<3);
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFLDCW(const void *mem)
{
	byte instr[6];
	instr[0] = 0xd9;
	instr[1] = 0x05 | (5<<3);
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFSTCW(const void *mem)
{
	byte instr[6];
	instr[0] = 0xd9;
	instr[1] = 0x05 | (7<<3);
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFSTSW(const void *mem)
{
	byte instr[6];
	instr[0] = 0xdd;
	instr[1] = 0x05 | (7<<3);
	*((uint32*)(&instr[2])) = uint32(mem);
	jitcEmit(instr, sizeof instr);
}

void FASTCALL asmFSTSW_EAX()
{
	byte instr[15] = {0xdf, 0xe0};
	jitcEmit(instr, 2);
}

