/*
 *	PearPC
 *	x86asm_64.cc
 *
 *	Copyright (C) 2004-2010 Sebastian Biallas (sb@biallas.net)
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

#define U32(dest) (*(uint32 *)(void *)(dest))
#define U64(dest) (*(uint64 *)(void *)(dest))

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

	U32(caps.vendor) = id.b;
	U32(caps.vendor+4) = id.d;
	U32(caps.vendor+8) = id.c;
	caps.vendor[12] = 0;
	ht_printf("%s\n", caps.vendor);
	if (id.level == 0) return;

	struct {
		uint32 model, features2, features, b;
	} id2;

	ppc_cpuid_asm(1, &id2);
	caps._3dnow = id2.features & (1<<31);
	caps._3dnow2 = id2.features & (1<<30);
	caps.sse3 = id2.features2 & (1<<0);
	caps.ssse3 = id2.features2 & (1<<9);
	
	ppc_cpuid_asm(0x80000000, &id);
	if (id.level >= 0x80000001) {
		// processor supports extended functions
		// now test for 3dnow
		ppc_cpuid_asm(0x80000001, &id2);
		
		caps._3dnow = id2.features & (1<<31);
		caps._3dnow2 = id2.features & (1<<30);
	}
	
/*	ht_printf("%s%s%s%s%s%s%s\n",
		caps.cmov?" CMOV":"",
		caps.mmx?" MMX":"",
		caps._3dnow?" 3DNOW":"",
		caps._3dnow2?" 3DNOW+":"",
		caps.sse?" SSE":"",
		caps.sse2?" SSE2":"",
		caps.sse3?" SSE3":"");*/
}

/*
 *	internal functions
 */

void JITC::mapRegister(NativeReg nreg, PPC_Register creg)
{
	nativeReg[nreg] = creg;
	clientReg[creg] = nreg;
}

void JITC::unmapRegister(NativeReg reg)
{
	clientReg[nativeReg[reg]] = REG_NO;
	nativeReg[reg] = PPC_REG_NO;
}

void JITC::loadRegister(NativeReg nreg, PPC_Register creg)
{
	if (creg >= PPC_FPR(0) && creg <= PPC_FPR(31)) {
		asmALU64(X86_MOV, nreg, curCPUreg(creg));
	} else {
		asmALU32(X86_MOV, nreg, curCPUreg(creg));
	}
	mapRegister(nreg, creg);
	nativeRegState[nreg] = rsMapped;
}

void JITC::storeRegister(NativeReg nreg, PPC_Register creg)
{
	if (creg >= PPC_FPR(0) && creg <= PPC_FPR(31)) {
		asmALU64(X86_MOV, curCPUreg(creg), nreg);
	} else {
		asmALU32(X86_MOV, curCPUreg(creg), nreg);
	}
}

void JITC::storeRegisterUndirty(NativeReg nreg, PPC_Register creg)
{
	storeRegister(nreg, creg);
	nativeRegState[nreg] = rsMapped; // no longer dirty
}

PPC_Register JITC::getRegisterMapping(NativeReg reg)
{
	return nativeReg[reg];
}

/*
 *	Return native register which maps given client register 
 *	(or REG_NO)
 *	Will not produce code.
 */
NativeReg JITC::getClientRegisterMapping(PPC_Register creg)
{
	return clientReg[creg];
}

void JITC::discardRegister(NativeReg r)
{
	// FIXME: move to front of the LRU list
	nativeRegState[r] = rsUnused;
}

/*
 *	Puts native register to the end of the LRU list
 *	Will not produce code.
 */
void JITC::touchRegister(NativeReg r)
{
	NativeRegType *reg = nativeRegsList[r];
	if (reg->moreRU) {
		// there's a more recently used register
		if (reg->lessRU) {
			reg->lessRU->moreRU = reg->moreRU;
			reg->moreRU->lessRU = reg->lessRU;
		} else {
			// reg was LRUreg
			LRUreg = reg->moreRU;
			reg->moreRU->lessRU = NULL;
		}
		reg->moreRU = NULL;
		reg->lessRU = MRUreg;
		MRUreg->moreRU = reg;
		MRUreg = reg;
	}
}

/*
 *	clobbers and moves to end of LRU list
 */
void JITC::clobberAndTouchRegister(NativeReg reg)
{
	switch (nativeRegState[reg]) {
	case rsDirty:
		storeRegisterUndirty(reg, getRegisterMapping(reg));
		// fall throu
	case rsMapped:
		unmapRegister(reg);
		nativeRegState[reg] = rsUnused;
		break;
	case rsUnused:;
	}
	touchRegister(reg);
}

/*
 *	clobbers and moves to front of LRU list
 */
void JITC::clobberAndDiscardRegister(NativeReg reg)
{
	switch (nativeRegState[reg]) {
	case rsDirty:
		storeRegisterUndirty(reg, getRegisterMapping(reg));
		// fall throu
	case rsMapped:
		unmapRegister(reg);
		discardRegister(reg);
		break;
	case rsUnused:;
		/*
		 *	Note: it makes no sense to move this register to
		 *	the front of the LRU list here, since only
		 *	other unused registers can be before it in the list
		 *
		 *	Note2: it would even be an error to move it here,
		 *	since RSP isn't in the nativeRegsList
		 */
	}
}

void JITC::clobberSingleRegister(NativeReg reg)
{
	switch (nativeRegState[reg]) {
	case rsDirty:
		storeRegisterUndirty(reg, getRegisterMapping(reg));
		// fall throu
	case rsMapped:
		unmapRegister(reg);
		nativeRegState[reg] = rsUnused;
		break;
	case rsUnused:;
	}
}

NativeReg JITC::allocFixedRegister(NativeReg reg)
{
	clobberAndTouchRegister(reg);
	return reg;
}

/*
 *	Dirty register.
 *	Does *not* touch register
 *	Will not produce code.
 */
NativeReg JITC::dirtyRegister(NativeReg r)
{
	nativeRegState[r] = rsDirty;
	return r;
}

/*
 *	Allocates a native register
 *	May produce a store if no registers are avaiable
 */
NativeReg JITC::allocRegister(int options)
{
	NativeReg reg;
	if (options & NATIVE_REG) {
		// allocate fixed register
		reg = (NativeReg)(options & 0xf);
	} else {
		// allocate random register
		reg = LRUreg->reg;
	}
	return allocFixedRegister(reg);
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
NativeReg JITC::mapClientRegisterDirty(PPC_Register creg, int options)
{
	if (options & NATIVE_REG) {
		NativeReg want_reg = (NativeReg)(options & 0xf);
		PPC_Register have_mapping = getRegisterMapping(want_reg);

		if (have_mapping != PPC_REG_NO) {
			// test if we're lucky
			if (have_mapping == creg) {
				dirtyRegister(want_reg);
				touchRegister(want_reg);
				return want_reg;
			}

			// we're not lucky, get a new register for the old mapping
			NativeReg temp_reg = allocRegister();
			// note that AllocRegister also touches temp_reg

			// make new mapping
			mapRegister(want_reg, creg);

			nativeRegState[temp_reg] = nativeRegState[want_reg];
			// now we can mess with want_reg
			dirtyRegister(want_reg);

			// maybe the old mapping was discarded and we're done
			if (temp_reg == want_reg) return want_reg;

			// ok, restore old mapping
			if (creg >= PPC_FPR(0) && creg <= PPC_FPR(31)) {
				asmALU64(X86_MOV, temp_reg, want_reg);
			} else {
				asmALU32(X86_MOV, temp_reg, want_reg);
			}
			mapRegister(temp_reg, have_mapping);
		} else {
			// want_reg is free
			// unmap creg if needed
			NativeReg reg = getClientRegisterMapping(creg);
			if (reg != REG_NO) {
				unmapRegister(reg);
				discardRegister(reg);
			}
			mapRegister(want_reg, creg);
			dirtyRegister(want_reg);
		}
		touchRegister(want_reg);
		return want_reg;
	} else {
		NativeReg reg = getClientRegisterMapping(creg);
		if (reg == REG_NO) {
			reg = allocRegister();
			mapRegister(reg, creg);
		} else {
			touchRegister(reg);
		}
		return dirtyRegister(reg);
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
NativeReg JITC::getClientRegister(PPC_Register creg, int options)
{
	if (options & NATIVE_REG) {
		NativeReg want_reg = (NativeReg)(options & 0xf);
		PPC_Register native_reg_maps_to = getRegisterMapping(want_reg);
		NativeReg client_reg_maps_to = getClientRegisterMapping(creg);
		if (native_reg_maps_to != PPC_REG_NO) {
			// test if we're lucky
			if (native_reg_maps_to == creg) {
				touchRegister(want_reg);
			} else {
				// we need to satisfy mapping
				if (client_reg_maps_to != REG_NO) {
					// FIXME: not good
					asmALU64(X86_XCHG, want_reg, client_reg_maps_to);
					RegisterState rs = nativeRegState[want_reg];
					nativeRegState[want_reg] = nativeRegState[client_reg_maps_to];
					nativeRegState[client_reg_maps_to] = rs;
					mapRegister(want_reg, creg);
					mapRegister(client_reg_maps_to, native_reg_maps_to);
					touchRegister(want_reg);
				} else {
					// client register isn't mapped
					allocFixedRegister(want_reg);
					loadRegister(want_reg, creg);
				}
			}
			return want_reg;
		} else {
			// want_reg is free 
			touchRegister(want_reg);
			if (client_reg_maps_to != REG_NO) {
				if (creg >= PPC_FPR(0) && creg <= PPC_FPR(31)) {
					asmALU64(X86_MOV, want_reg, client_reg_maps_to);
				} else {
					asmALU32(X86_MOV, want_reg, client_reg_maps_to);
				}
				nativeRegState[want_reg] = nativeRegState[client_reg_maps_to];
				unmapRegister(client_reg_maps_to);
				discardRegister(client_reg_maps_to);
				mapRegister(want_reg, creg);
			} else {
				loadRegister(want_reg, creg);
			}
			return want_reg;
		}
	} else {
		NativeReg client_reg_maps_to = getClientRegisterMapping(creg);
		if (client_reg_maps_to != REG_NO) {
			touchRegister(client_reg_maps_to);
			return client_reg_maps_to;
		} else {
			NativeReg reg = allocRegister();
			loadRegister(reg, creg);
			return reg;
		}
	}
}

/*
 *	Same as jitcGetClientRegister() but also dirties result
 */
NativeReg JITC::getClientRegisterDirty(PPC_Register creg, int options)
{
	return dirtyRegister(getClientRegister(creg, options));
}

void JITC::flushSingleRegister(NativeReg reg)
{
	if (nativeRegState[reg] == rsDirty) {
		storeRegisterUndirty(reg, getRegisterMapping(reg));
	}
}

void JITC::flushSingleRegisterDirty(NativeReg reg)
{
	if (nativeRegState[reg] == rsDirty) {
		storeRegister(reg, getRegisterMapping(reg));
	}
}

/*
 *	Flushes native register(s).
 *	Resets dirty flags.
 *	Will produce a store if register is dirty.
 */
void JITC::flushRegister(int options)
{
	if (options == NATIVE_REGS_ALL) {
		for (NativeReg i = RAX; i <= R15; i = (NativeReg)(i+1)) flushSingleRegister(i);
	} else if (options & NATIVE_REG) {
		NativeReg reg = (NativeReg)(options & 0xf);
		flushSingleRegister(reg);
	}
}

/*
 *	Flushes native register(s).
 *	Doesnt reset dirty flags.
 *	Will produce a store if register is dirty.
 */
void JITC::flushRegisterDirty(int options)
{
	if (options == NATIVE_REGS_ALL) {
		for (NativeReg i = RAX; i <= R15; i = (NativeReg)(i+1)) flushSingleRegisterDirty(i);
	} else if (options & NATIVE_REG) {
		NativeReg reg = (NativeReg)(options & 0xf);
		flushSingleRegisterDirty(reg);
	}
}
/*
 *	Clobbers native register(s).
 *	Register is unused afterwards.
 *	Will produce a store if register was dirty.
 */          
void JITC::clobberRegister(int options)
{
	if (options == NATIVE_REGS_ALL) {
		/*
		 *	We dont use clobberAndDiscard here
		 *	since it make no sense to move one register
		 *	if we clobber all
		 */
		for (NativeReg i = RAX; i <= R15; i=(NativeReg)(i+1)) clobberSingleRegister(i);
	} else if (options & NATIVE_REG) {
		NativeReg reg = (NativeReg)(options & 0xf);
		clobberAndDiscardRegister(reg);
	}
}

/*
 *
 */
void JITC::flushAll()
{
	clobberCarryAndFlags();
	flushRegister();
//	flushVectorRegister(); FIX64
}

/*
 *
 */
void JITC::clobberAll()
{
	clobberCarryAndFlags();
	clobberRegister();
	floatRegisterClobberAll();
//	trashVectorRegister(); FIX64
}

/*
 *	Invalidates all mappings
 *
 *	Will never produce code
 */          
void JITC::invalidateAll()
{
#if 0
	for (int i=RAX; i<=R15; i++) {
		if(jitc.nativeRegState[i] != rsDirty) {
			printf("!!! Unflushed register invalidated!\n");
		}
	}
#endif

	for (uint i=RAX; i<=R15; i++) {
		if (nativeReg[i] != PPC_REG_NO) {
			clientReg[nativeReg[i]] = REG_NO;
			nativeReg[i] = PPC_REG_NO;
		}
		nativeRegState[i] = rsUnused;
	}
	nativeCarryState = nativeFlagsState = rsUnused;

#if 0
	for (unsigned int i=XMM0; i<=XMM15; i++) {
		if(nativeVectorRegState[i] == rsDirty) {
			printf("!!! Unflushed vector register invalidated! (XMM%u)\n", i);
		}
	}

	memset(n2cVectorReg, PPC_VECTREG_NO, sizeof n2cVectorReg);
	memset(c2nVectorReg, VECTREG_NO, sizeof c2nVectorReg);
	memset(nativeVectorRegState, rsUnused, sizeof nativeVectorRegState);

	nativeVectorReg = VECTREG_NO;
#endif
}

/*
 *	Gets the client carry flags into the native carry flag
 *	
 */
void JITC::getClientCarry()
{
	if (nativeCarryState == rsUnused) {
		clobberFlags();

		asmBTx32(X86_BT, curCPU(xer_ca), 0);
		nativeCarryState = rsMapped;
	}
}

void JITC::mapFlagsDirty(PPC_CRx cr)
{
	nativeFlags = cr;
	nativeFlagsState = rsDirty;
}

PPC_CRx JITC::getFlagsMapping()
{
	return nativeFlags;
}

bool JITC::flagsMapped()
{
	return nativeFlagsState != rsUnused;
}

bool JITC::carryMapped()
{
	return nativeCarryState != rsUnused;
}

void JITC::mapCarryDirty()
{
	nativeCarryState = rsDirty;
}

void JITC::flushCarry()
{
	asmSET8(X86_C, curCPU(xer_ca));
}

#if 0

void JITC::flushFlags()
{
	asmCALL((NativeAddress)ppc_flush_flags_asm);
}

#else

uint8 jitcFlagsMapping[257];
uint8 jitcFlagsMapping2[256];
uint8 jitcFlagsMappingCMP_U[257];
uint8 jitcFlagsMappingCMP_L[257];

void JITC::flushFlags()
{
#if 0
	NativeReg r = allocRegister(NATIVE_REG_8);
	asmSET8(X86_S, r);
	asmSET8(X86_Z, 8)(r+4));
	asmMOVxxRegReg16(X86_MOVZX, r, r);
	asmALU8(X86_AND, curCPU(cr)+3, 0x0f);
	asmALU8(X86_MOV, (NativeReg8)r, modrm, x86_mem(modrm, r, (uint32)&jitcFlagsMapping));
	asmALU8(X86_OR, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU.cr+3), (NativeReg8)r);
#else 
	NativeReg r;
	PPC_Register pr = getRegisterMapping(RAX);
	if (pr != PPC_REG_NO) {
		r = allocRegister();
		asmALU64(X86_MOV, r, RAX);
	}
	asmSimple(X86_LAHF);
//	asmMOVxxRegReg8(X86_MOVZX, EAX, AH);
	byte instr[3] = {0x0f, 0xb6, 0xc4};
	emit(instr, sizeof(instr));
	asmALU8(X86_AND, curCPU(cr)+3, 0x0f);
	asmALU8(X86_MOV, RAX, RAX, uint64(jitcFlagsMapping2));
	asmALU8(X86_OR, curCPU(cr)+3, RAX);
	if (pr != PPC_REG_NO) {
		asmALU64(X86_MOV, RAX, r);
	}
#endif
}

#endif

void JITC::flushFlagsAfterCMP(X86FlagTest t1, X86FlagTest t2, byte mask, int disp, void *map)
{
	NativeReg r = allocRegister(NATIVE_REG | RAX);
//	byte instr2[5] = {0xb8, 0x00, 0x00, 0x00, 0x00};
//	emit(instr2, sizeof instr2);
	asmSET8(t1, r);
	byte instr[3] = {0x0f, 0x90+t2, 0xc0+4};
	emit(instr, sizeof(instr));
	asmMOVxx32_16(X86_MOVZX, r, r);
//	asmALU32(X86_CMP, RAX, 0x101);
//	NativeAddress fixup = asmJxxFixup(X86_NE);
//	instr[0] = 0xcc;
//	emit(instr, 1);
//	asmResolveFixup(fixup, asmHERE());
	asmALU8(X86_AND, curCPU(cr)+disp, mask);
	asmALU8(X86_MOV, r, r, uint64(map));
	asmALU8(X86_OR, curCPU(cr)+disp, r);
	//FIX64
}

void JITC::flushFlagsAfterCMPL_U(int disp)
{
	flushFlagsAfterCMP(X86_A, X86_B, 0x0f, disp, jitcFlagsMappingCMP_U);
}

void JITC::flushFlagsAfterCMPL_L(int disp)
{
	flushFlagsAfterCMP(X86_A, X86_B, 0xf0, disp, jitcFlagsMappingCMP_L);
}

void JITC::flushFlagsAfterCMP_U(int disp)
{
	flushFlagsAfterCMP(X86_G, X86_L, 0x0f, disp, jitcFlagsMappingCMP_U);
}

void JITC::flushFlagsAfterCMP_L(int disp)
{
	flushFlagsAfterCMP(X86_G, X86_L, 0xf0, disp, jitcFlagsMappingCMP_L);
}

void JITC::clobberFlags()
{
	if (nativeFlagsState == rsDirty) {
		if (nativeCarryState == rsDirty) {
			flushCarry();
		}
		flushFlags();
		nativeCarryState = rsUnused;
	}
	nativeFlagsState = rsUnused;
}

void JITC::clobberCarry()
{
	if (nativeCarryState == rsDirty) {
		flushCarry();
	}
	nativeCarryState = rsUnused;
}

void JITC::clobberCarryAndFlags()
{
	if (nativeCarryState == rsDirty) {
		if (nativeFlagsState == rsDirty) {
			flushCarry();
			flushFlags();
			nativeCarryState = nativeFlagsState = rsUnused;
		} else {
			clobberCarry();
		}
	} else {
		clobberFlags();
	}
}

/*
 *	ONLY FOR DEBUG! DON'T CALL (unless you know what you are doing)
 */
void JITC::flushCarryAndFlagsDirty()
{
	if (nativeCarryState == rsDirty) {
		flushCarry();
		if (nativeFlagsState == rsDirty) {
			flushFlags();
		}
	} else {
		if (nativeFlagsState == rsDirty) {
			flushFlags();
		}
	}
}

/**
 *		Assembler stuff
 *
 */

void JITC::asmNOP(int n)
{
	if (n <= 0) return;
	byte instr[15];
	n--;
	for (int i=0; i < n; i++) {
		instr[i] = 0x66;
	}
	instr[n] = 0x90;
	emit(instr, n+1);
}

static uint mkmodrm(byte *instr, NativeReg r, uint32 disp)
{
	r = NativeReg(r & 7);
	if (r == RSP) {
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
	} else if (r == RBP) {
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

static uint mksib(byte *instr, NativeReg base, int scale, NativeReg index, uint32 disp, uint &rex)
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
	if (index == RSP) {
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
		index = RSP;
	}	
	if (base == REG_NO) {
		base = RBP;
		mod = 0;
	} else {
		if ((base & 7) == RBP && mod == 0) {
			mod = 1;
		}
	}
	rex |= ((base & 8) >> 3) | ((index & 8) >> 2);
	base = NativeReg(base & 7);
	index = NativeReg(index & 7);
	instr[0] |= (mod << 6) | 4;
	instr[1] = (ss << 6) | (index << 3) | base;
	switch (mod) {
	case 1:
		instr[2] = disp;
		return 3;
	case 2:
		*((uint32 *)&instr[2]) = disp;
		return 6;
	}
	return 2;
}

void JITC::asmTEST32(NativeReg base, uint32 disp, uint32 imm)
{
	byte instr[15];
	uint32 mask = 0xff;
	uint len;
	for (uint i=0; i<4; i++) {
		// optimize test dword ptr [x], 0xff -> test byte ptr
		if ((imm & ~mask) == 0) {
			if (base > 7) {
				instr[0] = 0x41;
				instr[1] = 0xf6;
				instr[2] = 0;
				len = mkmodrm(instr+2, base, disp+i)+2;
			} else {
				instr[0] = 0xf6;
				instr[1] = 0;
				len = mkmodrm(instr+1, base, disp+i)+1;
			}
			instr[len] = imm >> (i*8);
			emit(instr, len+1);
			return;
		}
		mask <<= 8;
	}
	if (base > 7) {
		instr[0] = 0x41;
		instr[1] = 0xf7;
		instr[2] = 0;
		len = mkmodrm(instr+2, base, disp)+2;
	} else {
		instr[0] = 0xf7;
		instr[1] = 0;
		len = mkmodrm(instr+1, base, disp)+1;
	}
	U32(instr+len) = imm;
	emit(instr, len+4);
}

void JITC::asmOR32(NativeReg base, uint32 disp, uint32 imm)
{
	byte instr[15];
	uint32 mask = 0xff;
	uint len;
	for (uint i=0; i<4; i++) {
		// optimize or dword ptr [x], 0xff -> or byte ptr
		if ((imm & ~mask) == 0) {
			if (base > 7) {
				instr[0] = 0x41;
				instr[1] = 0x80;
				instr[2] = 0x08;
				len = mkmodrm(instr+2, base, disp+i)+2;
			} else {
				instr[0] = 0x80;
				instr[1] = 0x08;
				len = mkmodrm(instr+1, base, disp+i)+1;
			}
			instr[len] = imm >> (i*8);
			emit(instr, len+1);
			return;
		}
		mask <<= 8;
	}
	if (base > 7) {
		instr[0] = 0x41;
		instr[1] = 0x81;
		instr[2] = 0x08;
		len = mkmodrm(instr+2, base, disp)+2;
	} else {
		instr[0] = 0x81;
		instr[1] = 0x08;
		len = mkmodrm(instr+1, base, disp)+1;
	}
	U32(instr + len) = imm;
	emit(instr, len+4);
}

void JITC::asmAND32(NativeReg base, uint32 disp, uint32 imm)
{
	byte instr[15];
	uint32 mask = 0xff;
	uint len;
	for (uint i=0; i<4; i++) {
		// optimize and dword ptr [x], 0xffffff00 -> and byte ptr
		if ((imm & ~mask) == ~mask) {
			if (base > 7) {
				instr[0] = 0x41;
				instr[1] = 0x80;
				instr[2] = 0x20;
				len = mkmodrm(instr+2, base, disp+i)+2;
			} else {
				instr[0] = 0x80;
				instr[1] = 0x20;
				len = mkmodrm(instr+1, base, disp+i)+1;
			}
			instr[len] = imm >> (i*8);
			emit(instr, len+1);
			return;
		}
		mask <<= 8;
	}
	if (base > 7) {
		instr[0] = 0x41;
		instr[1] = 0x81;
		instr[2] = 0x20;
		len = mkmodrm(instr+2, base, disp)+2;
	} else {
		instr[0] = 0x81;
		instr[1] = 0x20;
		len = mkmodrm(instr+1, base, disp)+1;
	}
	U32(instr + len) = imm;
	emit(instr, len+4);
}

void JITC::asmSimpleMODRM64(uint8 opc, NativeReg reg1, NativeReg reg2)
{
	byte instr[3] = {0x48 + ((reg1>>3)<<2) + (reg2>>3), 
	                 opc, 0xc0+((reg1&7)<<3)+(reg2&7)};
	emit(instr, sizeof(instr));
}

void JITC::asmSimpleMODRM32(uint8 opc, NativeReg reg1, NativeReg reg2)
{
	if ((reg1 | reg2) > 7) {
		byte instr[3] = {0x40 + ((reg1>>3)<<2) + (reg2>>3), 
		                 opc, 0xc0+((reg1&7)<<3)+(reg2&7)};
		emit(instr, sizeof(instr));
	} else {
		byte instr[2] = {opc, 0xc0+(reg1<<3)+reg2};
		emit(instr, sizeof(instr));
	}
}

void JITC::asmSimpleMODRM8(uint8 opc, NativeReg reg1, NativeReg reg2)
{
	if ((reg1 & reg2) > 4) {
		byte instr[3] = {0x40 + ((reg1>>3)<<2) + (reg2>>3), 
		                 opc, 0xc0+((reg1&7)<<3)+(reg2&7)};
		emit(instr, sizeof(instr));
	} else {
		byte instr[2] = {opc, 0xc0+(reg1<<3)+reg2};
		emit(instr, sizeof(instr));
	}
}

void JITC::asmALU64(X86ALUopc opc, NativeReg reg1, NativeReg reg2)
{
	switch (opc) {
	case X86_MOV: 
		asmSimpleMODRM64(0x8b, reg1, reg2);
	        break;
	case X86_TEST:
		asmSimpleMODRM64(0x85, reg1, reg2);
	        break;
	case X86_XCHG:
		asmSimpleMODRM64(0x87, reg1, reg2);
	        break;
	default:
		asmSimpleMODRM64(0x03+(opc<<3), reg1, reg2);
	}	
}

void JITC::asmALU32(X86ALUopc opc, NativeReg reg1, NativeReg reg2)
{
	switch (opc) {
	case X86_MOV: 
		asmSimpleMODRM32(0x8b, reg1, reg2);
	        break;
	case X86_TEST:
		asmSimpleMODRM32(0x85, reg1, reg2);
	        break;
	case X86_XCHG:
/*		if (reg1 == EAX) {
			emit1(0x90+reg2);
		} else if (reg2 == EAX) {
			emit1(0x90+reg1);
		} else {*/
			asmSimpleMODRM32(0x87, reg1, reg2);
//		}
	        break;
	default:
		asmSimpleMODRM32(0x03+(opc<<3), reg1, reg2);
	}	
}

void JITC::asmALU8(X86ALUopc opc, NativeReg reg1, NativeReg reg2)
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

void JITC::asmMOVABS(NativeReg reg, uint64 value)
{
	byte instr[10] = {0x48+(reg>>3), 0xb8+(reg&7)};
	U64(instr + 2) = value;
	emit(instr, sizeof instr);
}

void JITC::asmSimpleALU32(X86ALUopc opc, NativeReg reg, uint32 imm)
{
	if (imm <= 0x7f || imm >= 0xffffff80) {
		if (reg > 7) {
			byte instr[4] = {0x41, 0x83, 0xc0+(opc<<3)+(reg&7), imm};
			emit(instr, sizeof(instr));
		} else {
			byte instr[3] = {0x83, 0xc0+(opc<<3)+reg, imm};
			emit(instr, sizeof(instr));
		}
	} else {
		if (reg == RAX) {
			byte instr[5];
			instr[0] = 0x05+(opc<<3);
			U32(instr + 1) = imm;
			emit(instr, sizeof(instr));
		} else {
			if (reg > 7) {
				byte instr[7];
				instr[0] = 0x41;
				instr[1] = 0x81;
				instr[2] = 0xc0+(opc<<3)+(reg&7);
				U32(instr + 3) = imm;
				emit(instr, sizeof(instr));
			} else {
				byte instr[6];
				instr[0] = 0x81;
				instr[1] = 0xc0+(opc<<3)+reg;
				U32(instr + 2) = imm;
				emit(instr, sizeof(instr));
			}
		}
	}
}

void JITC::asmTEST32(NativeReg reg1, uint32 imm)
{
	if (reg1 == RAX) {
		byte instr[5];
		instr[0] = 0xa9;
		U32(instr + 1) = imm;
		emit(instr, sizeof(instr));
	} else {
		if (reg1 > 7) { 
			byte instr[7];
			instr[0] = 0x41;
			instr[1] = 0xf7;
			instr[2] = 0xc0+(reg1&7);
			U32(instr + 3) = imm;
			emit(instr, sizeof(instr));
		} else {
			byte instr[6];
			instr[0] = 0xf7;
			instr[1] = 0xc0+reg1;
			U32(instr + 2) = imm;
			emit(instr, sizeof(instr));
		}
	}
}

void JITC::asmALU32(X86ALUopc opc, NativeReg reg, uint32 imm)
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
//	case X86_CMP:
//		if (imm == 0) {
//			asmALU(X86_OR, reg1, reg1);
//		} else {
//			asmSimpleALU(opc, reg1, imm);
//		}
//		break;
	default:
		asmSimpleALU32(opc, reg, imm);
	}
}

void JITC::asmMOV32_NoFlags(NativeReg reg1, uint32 imm)
{
	if (reg1 > 7) {
		byte instr[6];
		instr[0] = 0x41;
		instr[1] = 0xb8+(reg1&7);
		U32(instr + 2) = imm;
		emit(instr, sizeof(instr));
	} else {
		byte instr[5];
		instr[0] = 0xb8+reg1;
		U32(instr + 1) = imm;
		emit(instr, sizeof(instr));
	}
}

void JITC::asmALU32(X86ALUopc1 opc, NativeReg reg)
{
	if (reg > 7) {
		byte instr[3] = {0x41, 0xf7, opc+(reg&7)};
		emit(instr, 3);		
	} else {
		byte instr[2] = {0xf7, opc+reg};
		emit(instr, 2);
	}
}

void JITC::asmALU64(X86ALUopc opc, NativeReg reg, NativeReg base, uint32 disp)
{	
	byte instr[15];
	uint len=0;
	instr[0] = 0x48+((reg>>3)<<2)+(base>>3);
	switch (opc) {
	case X86_MOV:
		instr[1] = 0x8b;
		break;
	case X86_LEA:
		instr[1] = 0x8d;
		break;
	case X86_XCHG:
		instr[1] = 0x87;
		break;
	case X86_TEST:
		instr[1] = 0x85;
		break;
	default:
		instr[1] = 0x03+(opc<<3);
	}
	len += 2;
	instr[2] = (reg&7)<<3;
	len += mkmodrm(instr+len, base, disp);
	emit(instr, len);
}

void JITC::asmALU64(X86ALUopc opc, NativeReg reg, NativeReg base, int scale, NativeReg index, uint32 disp)
{
	byte instr[15];
	uint len = 0, rex = 0;
	rex = 0x48 + ((reg & 8) >> 1) + ((base & 8) >> 3) + ((index & 8) >> 2);
	len++;
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
	instr[len] = (reg&7) << 3;
	len += mksib(instr+len, base, scale, index, disp, rex);
	if (rex) instr[0] = rex;
	emit(instr, len);
}

void JITC::asmALU32(X86ALUopc opc, NativeReg reg, NativeReg base, uint32 disp)
{	
	byte instr[15];
	uint len=0;
	if ((reg | base) > 7) {
		instr[0] = 0x40+((reg>>3)<<2)+(base>>3);
		len++;
	}
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
	emit(instr, len);
}

void JITC::asmALU8(X86ALUopc opc, NativeReg reg, NativeReg base, uint32 disp)
{
	byte instr[15];
	uint len=0;
	if (reg > 3 || base > 7) {
		instr[0] = 0x40+((reg>>3)<<2)+(base>>3);
		len++;
	}
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
	emit(instr, len);
}

void JITC::asmALU64(X86ALUopc opc, NativeReg base, uint32 disp, NativeReg reg)
{	
	byte instr[15];
	uint len=0;
	instr[0] = 0x48+((reg>>3)<<2)+(base>>3);
	switch (opc) {
	case X86_MOV:
		instr[1] = 0x89;
		break;
	case X86_XCHG:
		instr[1] = 0x87;
		break;
	case X86_TEST:
		instr[1] = 0x85;
		break;
	default:
		instr[1] = 0x01+(opc<<3);
	}
	len += 2;
	instr[2] = (reg&7)<<3;
	len += mkmodrm(instr+len, base, disp);
	emit(instr, len);
}

void JITC::asmALU32(X86ALUopc opc, NativeReg base, uint32 disp, NativeReg reg)
{	
	byte instr[15];
	uint len=0;
	if ((reg | base) > 7) {
		instr[0] = 0x40+((reg>>3)<<2)+(base>>3);
		len++;
	}
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
	instr[len] = (reg&7)<<3;
	len += mkmodrm(instr+len, base, disp);
	emit(instr, len);
}

void JITC::asmALU8(X86ALUopc opc, NativeReg base, uint32 disp, NativeReg reg)
{
	byte instr[15];
	uint len=0;
	if (reg > 3 || base > 7) {
		instr[0] = 0x40+((reg>>3)<<2)+(base>>3);
		len++;
	}
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
	instr[len] = (reg&7)<<3;
	len += mkmodrm(instr+len, base, disp);
	emit(instr, len);
}

void JITC::asmALU32(X86ALUopc opc, NativeReg reg, NativeReg base, int scale, NativeReg index, uint32 disp)
{
	byte instr[15];
	uint len = 0, rex = 0;
	if ((reg | base | index) & 8) {
		rex = 0x40 + ((reg & 8) >> 1) + ((base & 8) >> 3) + ((index & 8) >> 2);
		len++;
	}
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
	instr[len] = (reg&7) << 3;
	len += mksib(instr+len, base, scale, index, disp, rex);
	if (rex) instr[0] = rex;
	emit(instr, len);
	
}

void JITC::asmALU32(X86ALUopc opc, NativeReg base, int scale, NativeReg index, uint64 disp, NativeReg reg)
{
	byte instr[15];
	uint len = 0, rex = 0;
	if ((reg | base | index) & 8) {
		rex = 0x40 + ((reg & 8) >> 1);
		len++;
	}
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
	len += mksib(instr+len, base, scale, index, disp, rex);
	if (rex) instr[0] = rex;
	emit(instr, len);
}

void JITC::asmSimpleALU32(X86ALUopc opc, NativeReg base, uint32 disp, uint32 imm)
{
	byte instr[15];
	uint len = 0;
	if (base & 8) {
		instr[0] = 0x41;
		len++;
	}
	if (imm <= 0x7f || imm >= 0xffffff80) {
		instr[len++] = 0x83;
		instr[len] = (opc<<3);
		len += mkmodrm(instr+len, base, disp);
		instr[len] = imm;
		emit(instr, len+1);
	} else {
		instr[len++] = 0x81;
		instr[len] = (opc<<3);
		len += mkmodrm(instr+len, base, disp);
		U32(instr + len) = imm;
		emit(instr, len+4);
	}
}

void JITC::asmALU32(X86ALUopc opc, NativeReg base, uint32 disp, uint32 imm)
{
	byte instr[15];
	uint len = 0;
	switch (opc) {
	case X86_MOV: {
		if (base & 8) {
			instr[0] = 0x41;
			len++;
		}
		instr[len++] = 0xc7;
		instr[len] = 0;
		len += mkmodrm(instr+len, base, disp);
		U32(instr + len) = imm;
		emit(instr, len+4);
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

void JITC::asmALU8(X86ALUopc opc, NativeReg base, uint32 disp, uint8 imm)
{
	byte instr[15];
	uint len = 0;
	if (base > 7) {
		instr[len] = 0x41;
		len++;
	}
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
		emit(instr, len+1);
		return;
	}
	len++;
	instr[len] = 0;
	len += mkmodrm(instr+len, base, disp);
	instr[len] = imm;
	emit(instr, len+1);
}

void JITC::asmMOVxx32_16(X86MOVxx opc, NativeReg reg1, NativeReg reg2)
{
	if (reg1 > 7 || reg2 > 4) {
		byte instr[4] = {0x40+((reg1>>3)<<2)+(reg2>>3), 
		                 0x0f, opc+1, 0xc0+((reg1&7)<<3)+(reg2&7)};
		emit(instr, sizeof(instr));		
	} else {
		byte instr[3] = {0x0f, opc+1, 0xc0+(reg1<<3)+reg2};
		emit(instr, sizeof(instr));
	}	
}

void JITC::asmMOVxx32_8(X86MOVxx opc, NativeReg reg1, NativeReg reg2)
{
	if (reg1 > 7 || reg2 > 3) {
		byte instr[4] = {0x40+((reg1>>3)<<2)+(reg2>>3), 
		                 0x0f, opc, 0xc0+((reg1&7)<<3)+(reg2&7)};
		emit(instr, sizeof(instr));		
	} else {
		byte instr[3] = {0x0f, opc, 0xc0+(reg1<<3)+reg2};
		emit(instr, sizeof(instr));
	}
}

void JITC::asmMOVxx32_8(X86MOVxx opc, NativeReg reg, NativeReg base, uint32 disp)
{
	byte instr[15];
	uint len=0;
	if (reg > 7 || base > 7) {
		instr[0] = 0x40+((reg>>3)<<2)+(base>>3);
		len++;
	}
	instr[len++] = 0x0f;
	instr[len++] = opc;
	instr[len] = (reg & 7) << 3;
	len += mkmodrm(instr+len, base, disp);
	emit(instr, len);
}

void JITC::asmSET8(X86FlagTest flags, NativeReg reg)
{
	if (reg > 3) {
		byte instr[4] = {0x40+(reg>>3), 0x0f, 0x90+flags, 0xc0+(reg&7)};
		emit(instr, sizeof(instr));
	} else {
		byte instr[3] = {0x0f, 0x90+flags, 0xc0+reg};
		emit(instr, sizeof(instr));
	}
}

void JITC::asmSET8(X86FlagTest flags, NativeReg base, uint32 disp)
{
	byte instr[15];
	uint len = 0;
	if (base > 7) {
		instr[len++] = 0x41;
	}
	instr[len++] = 0x0f;
	instr[len++] = 0x90+flags;
	instr[len] = 0;
	len += mkmodrm(instr+len, base, disp);
	emit(instr, len);
}

void JITC::asmCMOV32(X86FlagTest flags, NativeReg reg1, NativeReg reg2)
{
	if ((reg1 | reg2) > 7) {
		byte instr[4] = {0x40+((reg1>>3)<<2)+(reg2>>3), 
		                 0x0f, 0x40+flags, 0xc0+((reg1&7)<<3)+(reg2&7)};
		emit(instr, sizeof(instr));
	} else {
		byte instr[3] = {0x0f, 0x40+flags, 0xc0+(reg1<<3)+reg2};
		emit(instr, sizeof(instr));
	}
}

void JITC::asmCMOV32(X86FlagTest flags, NativeReg reg, NativeReg base, uint32 disp)
{
	byte instr[15];
	uint len = 0;
	if (reg > 3 || base > 7) {
		instr[len++] = 0x40+((reg>>3)<<2)+(base>>3);
	}
	instr[len++] = 0x0f;
	instr[len++] = 0x40+flags;
	instr[len] = 0;
	len += mkmodrm(instr+len, base, disp);
	emit(instr, len);
}

void JITC::asmShift64(X86ShiftOpc opc, NativeReg reg, uint32 imm)
{
	if (imm == 1) {
		byte instr[3] = {0x48+(reg >> 3), 0xd1, 0xc0+opc+(reg&7)};
		emit(instr, sizeof(instr));
	} else {
		byte instr[4] = {0x48+(reg >> 3), 0xc1, 0xc0+opc+(reg&7), imm};
		emit(instr, sizeof(instr));
	}
}

void JITC::asmShift32(X86ShiftOpc opc, NativeReg reg, uint32 imm)
{
	if (imm == 1) {
		if (reg > 7) {
			byte instr[3] = {0x41, 0xd1, 0xc0+opc+(reg&7)};
			emit(instr, sizeof(instr));
		} else { 
			byte instr[2] = {0xd1, 0xc0+opc+reg};
			emit(instr, sizeof(instr));
		}
	} else {
		if (reg > 7) {
			byte instr[4] = {0x41, 0xc1, 0xc0+opc+(reg&7), imm};
			emit(instr, sizeof(instr));
		} else { 
			byte instr[3] = {0xc1, 0xc0+opc+reg, imm};
			emit(instr, sizeof(instr));
		}
	}
}

void JITC::asmShift16(X86ShiftOpc opc, NativeReg reg, uint imm)
{
	if (imm == 1) {
		if (reg > 7) {
			byte instr[4] = {0x66, 0x41, 0xd1, 0xc0+opc+(reg&7)};
			emit(instr, sizeof(instr));
		} else { 
			byte instr[3] = {0x66, 0xd1, 0xc0+opc+reg};
			emit(instr, sizeof(instr));
		}
	} else {
		if (reg > 7) {
			byte instr[5] = {0x66, 0x41, 0xc1, 0xc0+opc+(reg&7), imm};
			emit(instr, sizeof(instr));
		} else { 
			byte instr[4] = {0x66, 0xc1, 0xc0+opc+reg, imm};
			emit(instr, sizeof(instr));
		}
	}
}

void JITC::asmShift32CL(X86ShiftOpc opc, NativeReg reg)
{
	if (reg > 7) {
		byte instr[3] = {0x41, 0xd3, 0xc0+opc+(reg&7)};
		emit(instr, sizeof(instr));
	} else {
		byte instr[2] = {0xd3, 0xc0+opc+reg};
		emit(instr, sizeof(instr));
	}
}

void JITC::asmIMUL32(NativeReg reg1, NativeReg reg2, uint32 imm)
{
	if (imm <= 0x7f || imm >= 0xffffff80) {	
		if ((reg1 | reg2) > 7) {
			byte instr[4] = {0x40+(reg2>>3)+((reg1>>3)<<2), 
			                 0x6b, 0xc0+((reg1&7)<<3)+(reg2&7), imm};
			emit(instr, sizeof(instr));
		} else {
			byte instr[3] = {0x6b, 0xc0+(reg1<<3)+reg2, imm};
			emit(instr, sizeof(instr));
		}
	} else {
		if ((reg1 | reg2) > 7) {
			byte instr[7] = {0x40+(reg2>>3)+((reg1>>3)<<2), 
			                 0x69, 0xc0+((reg1&7)<<3)+(reg2&7)};
			U32(instr + 3) = imm;
			emit(instr, sizeof(instr));
		} else {
			byte instr[6] = {0x69, 0xc0+(reg1<<3)+reg2};
			U32(instr + 2) = imm;
			emit(instr, sizeof(instr));
		}
	}
}


void JITC::asmIMUL32(NativeReg reg1, NativeReg reg2)
{
	if ((reg1 | reg2) > 7) {
		byte instr[4] = {0x40+(reg2>>3)+((reg1>>3)<<2),
		                 0x0f, 0xaf, 0xc0+((reg1&0x7)<<3)+(reg2&0x7)};
		emit(instr, sizeof(instr));
	} else {
		byte instr[3] = {0x0f, 0xaf, 0xc0+(reg1<<3)+reg2};
		emit(instr, sizeof(instr));
	}
}

void JITC::asmINC32(NativeReg reg1)
{
	if (reg1 > 7) {
		byte instr[3] = {0x41, 0xff, 0xc0+(reg1&0x7)};
		emit(instr, sizeof(instr));
	} else {
		byte instr[2] = {0xff, 0xc0+reg1};
		emit(instr, sizeof(instr));
	}
}

void JITC::asmDEC32(NativeReg reg1)
{
	if (reg1 > 7) {
		byte instr[3] = {0x41, 0xff, 0xc8+(reg1&0x7)};
		emit(instr, sizeof(instr));
	} else {
		byte instr[2] = {0xff, 0xc8+reg1};
		emit(instr, sizeof(instr));
	}
}

void JITC::asmBTx32(X86BitTest opc, NativeReg reg1, int value)
{
	if (reg1 > 7) {
		byte instr[5] = {0x41, 0x0f, 0xba, 0xc0+(opc<<3)+(reg1&7), value};
		emit(instr, sizeof instr);
	} else {
		byte instr[4] = {0x0f, 0xba, 0xc0+(opc<<3)+reg1, value};
		emit(instr, sizeof instr);
	}
}

void JITC::asmBTx32(X86BitTest opc, NativeReg reg1, uint32 disp, int value)
{
	byte instr[15];
	if (reg1 > 7) {
		instr[0] = 0x41;
		instr[1] = 0x0f;
		instr[2] = 0xba;
		instr[3] = opc << 3;
		uint len = mkmodrm(instr+3, reg1, disp)+3;
		instr[len] = value;
		emit(instr, len+1);
	} else {
		instr[0] = 0x0f;
		instr[1] = 0xba;
		instr[2] = opc << 3;
		uint len = mkmodrm(instr+2, reg1, disp)+2;
		instr[len] = value;
		emit(instr, len+1);
	}
}


void JITC::asmBSx32(X86BitSearch opc, NativeReg reg1, NativeReg reg2)
{
	if ((reg1 | reg2) > 7) {
		byte instr[4] = {0x40+((reg1>>3)<<2)+(reg2>>3), 
		                 0x0f, opc, 0xc0+((reg1&7)<<3)+(reg2&7)};
		emit(instr, sizeof(instr));
	} else {
		byte instr[3] = {0x0f, opc, 0xc0+(reg1<<3)+reg2};
		emit(instr, sizeof(instr));
	}
}

void JITC::asmBSWAP64(NativeReg reg)
{
	byte instr[3];
	instr[0] = 0x48+(reg>>3);
	instr[1] = 0x0f;
	instr[2] = 0xc8+(reg & 0x7);
	emit(instr, 3);	
}

void JITC::asmBSWAP32(NativeReg reg)
{
	byte instr[3];
	if (reg > 7) {
		instr[0] = 0x41;
		instr[1] = 0x0f;
		instr[2] = 0xc8+(reg & 0x7);
		emit(instr, 3);	
	} else {
		instr[0] = 0x0f;
		instr[1] = 0xc8+reg;
		emit(instr, 2);	
	}
}

void JITC::asmJMP(NativeAddress to)
{
	/*
	 *	We use emitAssure here, since
	 *	we have to know the exact address of the jump
	 *	instruction (since it is relative)
	 */
restart:
	byte instr[5];
	uint32 rel = uint32(to - (currentPage->tcp+2));
	if (rel <= 0x7f || rel >= 0xffffff80) {
		if (!emitAssure(2)) goto restart;
		instr[0] = 0xeb;
		instr[1] = rel;
		emit(instr, 2);
	} else {
		if (!emitAssure(5)) goto restart;
		instr[0] = 0xe9;
		U32(instr + 1) = uint32(to - (currentPage->tcp+5));
		emit(instr, 5);
	}
}

void JITC::asmJxx(X86FlagTest flags, NativeAddress to)
{
restart:
	byte instr[6];
	uint32 rel = uint32(to - (currentPage->tcp+2));
	if (rel <= 0x7f || rel >= 0xffffff80) {
		if (!emitAssure(2)) goto restart;
		instr[0] = 0x70+flags;
		instr[1] = rel;
		emit(instr, 2);
	} else {
		if (!emitAssure(6)) goto restart;
		instr[0] = 0x0f;
		instr[1] = 0x80+flags;
		U32(instr + 2) = uint32(to - (currentPage->tcp+6));
		emit(instr, 6);
	}
}

NativeAddress JITC::asmJMPFixup()
{
	byte instr[5];
	instr[0] = 0xe9;
#ifdef JITC_DEBUG
	memset(instr+1, 0, 4);
#endif
	emit(instr, 5);
	return currentPage->tcp - 4;
}

NativeAddress JITC::asmJxxFixup(X86FlagTest flags)
{
	byte instr[6];
	instr[0] = 0x0f;
	instr[1] = 0x80+flags;
#ifdef JITC_DEBUG
	memset(instr+2, 0, 4);
#endif
	emit(instr, 6);
	return currentPage->tcp - 4;
}

void JITC::asmCALL(NativeAddress to)
{
	emitAssure(5);
	byte instr[5];
	instr[0] = 0xe8;
	U32(instr + 1) = uint32(to - (currentPage->tcp+5));
	emit(instr, 5);
}

void JITC::asmSimple(X86SimpleOpc simple)
{
	if (simple > 0xff) {
		emit((byte*)&simple, 2);
	} else {
		emit1(simple);
	}
}


#if 0
/*
 *	Maps one client vector register to one native vector register
 *	Will never emit any code.
 */
static inline void jitcMapVectorRegister(NativeVectorReg nreg, JitcVectorReg creg)
{
	//printf("*** map: XMM%u (vr%u)\n", nreg, creg);
	jitc.n2cVectorReg[nreg] = creg;
	jitc.c2nVectorReg[creg] = nreg;

	jitc.nativeVectorRegState[nreg] = rsMapped;
}

/*
 *	Unmaps the native vector register from any client vector register
 *	Will never emit any code.
 */
static inline void jitcUnmapVectorRegister(NativeVectorReg nreg)
{
	JitcVectorReg creg = jitc.n2cVectorReg[nreg];

	if (nreg != VECTREG_NO && creg != PPC_VECTREG_NO) {
		//printf("*** unmap: XMM%u (vr%u)\n", nreg, jitc.n2cVectorReg[nreg]);

		jitc.n2cVectorReg[nreg] = PPC_VECTREG_NO;
		jitc.c2nVectorReg[creg] = VECTREG_NO;

		jitc.nativeVectorRegState[nreg] = rsUnused;
	}
}

/*
 *	Marks the native vector register as dirty.
 *	Does *not* touch native vector register.
 *	Will not produce code.
 */
void jitcDirtyVectorRegister(NativeVectorReg nreg)
{
	JitcVectorReg creg = jitc.n2cVectorReg[nreg];

	//printf("*** dirty(%u) with creg = %u\n", nreg, creg);

	if (creg == JITC_VECTOR_NEG1 || creg == PPC_VECTREG_NO) {
		//printf("*** dirty: %u = %u or %u\n", creg, JITC_VECTOR_NEG1, PPC_REG_NO);
		return;
	}

	if (jitc.nativeVectorRegState[nreg] == rsUnused) {
		printf("!!! Attemped dirty of an anonymous vector register!\n");
		return;
	}

	if (creg == jitc.nativeVectorReg) {
		jitc.nativeVectorReg = VECTREG_NO;
	}

	jitc.nativeVectorRegState[nreg] = rsDirty;
}

/*
 *	Marks the native vector register as non-dirty.
 *	Does *not* flush native vector register.
 *	Will not produce code.
 */
static inline void jitcUndirtyVectorRegister(NativeVectorReg nreg)
{
	if (jitc.nativeVectorRegState[nreg] > rsMapped) {
		//printf("*** undirty: XMM%u (vr%u)\n", nreg, jitc.n2cVectorReg[nreg]);

		jitc.nativeVectorRegState[nreg] = rsMapped;
	}
}

/*
 *	Loads a native vector register with its mapped value.
 *	Does not alter the native vector register's markings.
 *	Will always emit an load.
 */
static inline void jitcLoadVectorRegister(NativeVectorReg nreg)
{
	JitcVectorReg creg = jitc.n2cVectorReg[nreg];

	if (creg == JITC_VECTOR_NEG1 && jitc.hostCPUCaps.sse2) {
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
static inline void jitcStoreVectorRegister(NativeVectorReg nreg)
{
	JitcVectorReg creg = jitc.n2cVectorReg[nreg];

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
NativeVectorReg jitcGetClientVectorRegisterMapping(JitcVectorReg creg)
{
	return jitc.c2nVectorReg[creg];
}

/*
 *	Makes the vector register the least recently used vector register.
 *	Will never emit any code.
 */
static inline void jitcDiscardVectorRegister(NativeVectorReg nreg)
{
	NativeVectorReg lreg, mreg;

	mreg = jitc.MRUvregs[nreg];
	lreg = jitc.LRUvregs[nreg];

	// remove from the list
	jitc.MRUvregs[lreg] = mreg;
	jitc.LRUvregs[mreg] = lreg;

	mreg = jitc.MRUvregs[XMM_SENTINEL];

	// insert into the list in the LRU spot
	jitc.LRUvregs[nreg] = XMM_SENTINEL;
	jitc.MRUvregs[nreg] = mreg;

	jitc.LRUvregs[mreg] = nreg;
	jitc.MRUvregs[XMM_SENTINEL] = nreg;
}

/*
 *	Makes the vector register the most recently used vector register.
 *	Will never emit any code.
 */
void jitcTouchVectorRegister(NativeVectorReg nreg)
{
	NativeVectorReg lreg, mreg;

	mreg = jitc.MRUvregs[nreg];
	lreg = jitc.LRUvregs[nreg];

	// remove from the list
	jitc.MRUvregs[lreg] = mreg;
	jitc.LRUvregs[mreg] = lreg;

	lreg = jitc.LRUvregs[XMM_SENTINEL];

	// insert into the list in the LRU spot
	jitc.MRUvregs[nreg] = XMM_SENTINEL;
	jitc.LRUvregs[nreg] = lreg;

	jitc.MRUvregs[lreg] = nreg;
	jitc.LRUvregs[XMM_SENTINEL] = nreg;
}

/*
 *	Unmaps a native vector register, and marks it least recently used.
 *	Will not emit any code.
 */
void jitcDropSingleVectorRegister(NativeVectorReg nreg)
{
	jitcDiscardVectorRegister(nreg);
	jitcUnmapVectorRegister(nreg);
}

int jitcAssertFlushedVectorRegister(JitcVectorReg creg)
{
	NativeVectorReg nreg = jitc.c2nVectorReg[creg];

	if (nreg != VECTREG_NO && jitc.nativeVectorRegState[nreg] == rsDirty) {
		printf("!!! Unflushed vector XMM%u (vr%u)!\n", nreg, creg);
		return 1;
	}
	return 0;
}
int jitcAssertFlushedVectorRegisters()
{
	int ret = 0;

	for (JitcVectorReg i=0; i<32; i++)
		ret |= jitcAssertFlushedVectorRegister(i);

	return ret;
}

void jitcShowVectorRegisterStatus(JitcVectorReg creg)
{
	NativeVectorReg nreg = jitc.c2nVectorReg[creg];

	if (nreg != VECTREG_NO) {
		int status = jitc.nativeVectorRegState[nreg];
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
static inline void jitcFlushSingleVectorRegister(NativeVectorReg nreg)
{
	if (jitc.nativeVectorRegState[nreg] == rsDirty) {
		//printf("*** flush: XMM%u (vr%u)\n", nreg, jitc.n2cVectorReg[nreg]);
		jitcStoreVectorRegister(nreg);
	}
}

/*
 *	Flushes the register, frees it, and makes it least recently used.
 *	Will produce a store, if the native vector register was dirty.
 */
static inline void jitcTrashSingleVectorRegister(NativeVectorReg nreg)
{
	if (jitc.nativeVectorRegState[nreg] > rsUnused) {
		//printf("*** trash: XMM%u (vr%u)\n", nreg, jitc.n2cVectorReg[nreg]);
	}

	jitcFlushSingleVectorRegister(nreg);
	jitcDropSingleVectorRegister(nreg);
}

/*
 *	Flushes the register, frees it, and makes it most recently used.
 *	Will produce a store, if the native vector register was dirty.
 */
static inline void jitcClobberSingleVectorRegister(NativeVectorReg nreg)
{
	if (jitc.nativeVectorRegState[nreg] > rsUnused) {
		//printf("*** clobber: XMM%u (vr%u)\n", nreg, jitc.n2cVectorReg[nreg]);
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
NativeVectorReg jitcAllocVectorRegister(int hint)
{
	NativeVectorReg nreg = jitc.MRUvregs[XMM_SENTINEL];

	if (hint >= XMM_SENTINEL) {
		nreg = jitc.LRUvregs[nreg];

		jitcTrashSingleVectorRegister(nreg);
	} else if (hint) {
		for (int i=1; i<hint; i++) {
			nreg = jitc.MRUvregs[nreg];
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
NativeVectorReg jitcMapClientVectorRegisterDirty(JitcVectorReg creg, int hint)
{
	NativeVectorReg nreg = jitc.c2nVectorReg[creg];

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
NativeVectorReg jitcGetClientVectorRegister(JitcVectorReg creg, int hint)
{
	NativeVectorReg nreg = jitc.c2nVectorReg[creg];

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
NativeVectorReg jitcGetClientVectorRegisterDirty(JitcVectorReg creg, int hint)
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
void jitcFlushVectorRegister(int options)
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
void jitcFlushVectorRegisterDirty(int options)
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
void jitcClobberVectorRegister(int options)
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
void jitcTrashVectorRegister(int options)
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
void jitcDropVectorRegister(int options)
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

void jitcFlushClientVectorRegister(JitcVectorReg creg)
{
	NativeVectorReg nreg = jitc.c2nVectorReg[creg];

	if (nreg != VECTREG_NO) {
		jitcFlushSingleVectorRegister(nreg);
		jitcUndirtyVectorRegister(nreg);
	}
}

void jitcTrashClientVectorRegister(JitcVectorReg creg)
{
	NativeVectorReg nreg = jitc.c2nVectorReg[creg];

	if (nreg != VECTREG_NO) {
		jitcTrashSingleVectorRegister(nreg);
	}
}

void jitcClobberClientVectorRegister(JitcVectorReg creg)
{
	NativeVectorReg nreg = jitc.c2nVectorReg[creg];

	if (nreg != VECTREG_NO) {
		jitcClobberSingleVectorRegister(nreg);
	}
}

void jitcDropClientVectorRegister(JitcVectorReg creg)
{
	NativeVectorReg nreg = jitc.c2nVectorReg[creg];

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
NativeVectorReg jitcRenameVectorRegisterDirty(NativeVectorReg reg, JitcVectorReg creg, int hint)
{
	NativeVectorReg nreg = jitc.c2nVectorReg[creg];

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
		JitcVectorReg reg2 = jitc.n2cVectorReg[reg];

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

void asmMOVAPS(NativeVectorReg reg, const void *disp)
{
	byte instr[8] = { 0x0f, 0x28 };

	instr[2] = 0x05 | (reg << 3);
	U32(instr + 3) = uint32(disp);

	emit(instr, 7);
}

void asmMOVAPS(const void *disp, NativeVectorReg reg)
{
	byte instr[8] = { 0x0f, 0x29 };

	instr[2] = 0x05 | (reg << 3);
	U32(instr + 3) = uint32(disp);

	emit(instr, 7);
}

void asmMOVUPS(NativeVectorReg reg, const void *disp)
{
	byte instr[8] = { 0x0f, 0x10 };

	instr[2] = 0x05 | (reg << 3);
	U32(instr + 3) = uint32(disp);

	emit(instr, 7);
}

void asmMOVUPS(const void *disp, NativeVectorReg reg)
{
	byte instr[8] = { 0x0f, 0x11 };

	instr[2] = 0x05 | (reg << 3);
	U32(instr + 3) = uint32(disp);

	emit(instr, 7);
}

void asmMOVSS(NativeVectorReg reg, const void *disp)
{
	byte instr[10] = { 0xf3, 0x0f, 0x10 };

	instr[3] = 0x05 | (reg << 3);
	U32(instr + 4) = uint32(disp);

	emit(instr, 8);
}

void asmMOVSS(const void *disp, NativeVectorReg reg)
{
	byte instr[10] = { 0xf3, 0x0f, 0x11 };

	instr[3] = 0x05 | (reg << 3);
	U32(instr + 4) = uint32(disp);
 
	emit(instr, 8);
}

void asmALUPS(X86ALUPSopc opc, NativeVectorReg reg1, NativeVectorReg reg2)
{
	byte instr[4] = { 0x0f };

	instr[1] = opc;
	instr[2] = 0xc0 + (reg1 << 3) + reg2;

	emit(instr, 3);
}

void asmALUPS(X86ALUPSopc opc, NativeVectorReg reg1, modrm_p modrm)
{
	byte instr[16] = { 0x0f };
	int len = modrm++[0];

	instr[1] = opc;
	memcpy(&instr[2], modrm, len);
	instr[2] |= (reg1 << 3);

	emit(instr, len+2);
}

void asmPALU(X86PALUopc opc, NativeVectorReg reg1, NativeVectorReg reg2)
{
	byte instr[5] = { 0x66, 0x0f };

	instr[2] = opc;
	instr[3] = 0xc0 + (reg1 << 3) + reg2;

	emit(instr, 4);
}

void asmPALU(X86PALUopc opc, NativeVectorReg reg1, modrm_p modrm)
{
	byte instr[5] = { 0x66, 0x0f };
	int len = modrm++[0];

	instr[2] = opc;
	memcpy(&instr[3], modrm, len);
	instr[3] |= (reg1 << 3);

	emit(instr, len+3);
}

void asmSHUFPS(NativeVectorReg reg1, NativeVectorReg reg2, int order)
{
	byte instr[5] = { 0x0f, 0xc6, 0xc0+(reg1<<3)+reg2, order };

	emit(instr, 4);
}

void asmSHUFPS(NativeVectorReg reg1, modrm_p modrm, int order)
{
	byte instr[16] = { 0x0f, 0xc6 };
	int len = modrm++[0];

	memcpy(&instr[2], modrm, len);
	instr[2] |= (reg1 << 3);
	instr[len+2] = order;

	emit(instr, len+3);
}

void asmPSHUFD(NativeVectorReg reg1, NativeVectorReg reg2, int order)
{
	byte instr[6] = { 0x66, 0x0f, 0x70, 0xc0+(reg1<<3)+reg2, order };

	emit(instr, 5);
}

void asmPSHUFD(NativeVectorReg reg1, modrm_p modrm, int order)
{
	byte instr[5] = { 0x66, 0x0f, 0x70 };
	int len = modrm++[0];

	memcpy(&instr[3], modrm, len);
	instr[3] |= (reg1 << 3);
	instr[len+3] = order;

	emit(instr, len+4);
}
#endif
