/*
 *	PearPC
 *	jitc.h
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

#ifndef __JITC_H__
#define __JITC_H__

#include "jitc_types.h"
#include "x86asm.h"

/*
 *	The size of a fragment
 *	This is the size of portion of translated client code
 */
#define FRAGMENT_SIZE 256

/*
 *	Used to describe a fragment of translated client code
 *	If fragment is empty/invalid it isn't assigned to a
 *	client page but in the freeFragmentsList
 */
struct TranslationCacheFragment {
	/*
	 *	The start address of this fragement in the 
	 *	translation cache.
	 */
	NativeAddress base;
	/*
	 *	If fragment is assigned to a client page
	 *	this points to the previous used segment of this page.
	 *	Used when freeing pages.
	 *
	 *	Else this points to the next entry in the list of free fragments.
	 */
	TranslationCacheFragment *prev; 
};

/*
 *	Used to describe a (not neccessarily translated) client page
 */
struct ClientPage {
	/*
	 *	This is used to translate client page addresses 
	 *	into host addresses. Address isn't (yet) an entrypoint 
	 *	or not yet translated if entrypoints[page_offset] == 0
	 *
	 *	Note that a client page has 4096 bytes, but there are only
	 *	1024 possible entries per page.
	 */
	NativeAddress entrypoints[1024];
	uint32 baseaddress;

	/*
	 *	The fragment which has space left
	 *	== NULL if page isn't translated yet
	 */
	TranslationCacheFragment *tcf_current; 
	
	int bytesLeft;		// Remaining bytes in current fragment
	NativeAddress tcp;	// Translation cache pointer

	ClientPage *moreRU;	// points to a page which was used more recently
	ClientPage *lessRU;	// points to a page which was used less recently
};

struct NativeRegType {
	NativeReg reg;
	NativeRegType *moreRU;	// points to a register which was used more recently
	NativeRegType *lessRU;	// points to a register which was used less recently
};

enum RegisterState {
	rsUnused = 0,
	rsMapped = 1,
	rsDirty = 2,
};

#define TLB_ENTRIES 32

struct JITC {	
	/*
	 *	This is the array of all (physical) pages of the client.
	 *	The entries might be NULL indicating the this base address
	 *	isn't translated yet.
	 */
	ClientPage **clientPages;

	/*
	 *	These are the TLB-Entries
	 */
	uint32 tlb_code_eff[TLB_ENTRIES];
	uint32 tlb_data_read_eff[TLB_ENTRIES];
	uint32 tlb_data_write_eff[TLB_ENTRIES];
	uint32 tlb_code_phys[TLB_ENTRIES];
	uint32 tlb_data_read_phys[TLB_ENTRIES];
	uint32 tlb_data_write_phys[TLB_ENTRIES];
	uint64 tlb_code_hits;
	uint64 tlb_data_read_hits;
	uint64 tlb_data_write_hits;
	uint64 tlb_code_misses;
	uint64 tlb_data_read_misses;
	uint64 tlb_data_write_misses;

	/*
	 *	Capabilities of the host cpu
	 */
	X86CPUCaps hostCPUCaps;

	/*
	 *	If nativeReg[i] is set, it indicates to which client
	 *	register this native register corrensponds.
	 */
	PPC_Register nativeReg[8];
	
	RegisterState nativeRegState[8];

	/*
	 *	number of stack entries (0 <= TOP <= 8)
	 */
	int nativeFloatTOP;
	/*
	 *	Indexed by type JitcFloatReg
	 */
	int nativeFloatRegStack[9];
	RegisterState nativeFloatRegState[9];

	/*
	 *
	 */
	PPC_CRx nativeFlags;
	RegisterState nativeFlagsState;
	RegisterState nativeCarryState;
	
	/*
	 *	If clientRegister is set, in indicates to which native
	 *	register this client register corrensponds.
	 *	Indexed by type PPC_Register
	 */
	NativeReg clientReg[sizeof gCPU];
	
	/*
	 *	If clientFloatReg[i] is set fpr[i] is mapped to the native
	 *	float register clientFloatReg[i]
	 */
	JitcFloatReg clientFloatReg[32];

	/*
	 *	An element of S_8 and its inverse (indexed by JitcFloatReg)
	 *	to keep track of FXCH
	 */
	JitcFloatReg floatRegPerm[9];
	JitcFloatReg floatRegPermInverse[9];

	/*
	 *	Do this only once per basic block
	 */
	bool checkedPriviledge;
	bool checkedFloat;
	bool checkedVector; 
	 
	/*
	 *	Only used for the LRU list
	 */
	NativeRegType *nativeRegsList[8];
		 
	/*
	 *	Points to the least/most recently used register
	 */
	NativeRegType *LRUreg;
	NativeRegType *MRUreg;

	/*
	 *	This is the least recently used page
	 *	(ie. the page that is freed if no more pages are available)
	 *	Must can be NULL if freeFragmentsList is != NULL
	 */
	ClientPage *LRUpage;
	ClientPage *MRUpage;

	/*
	 *	These are the unused fragments as a linked list.
	 *	Can be NULL.
	 */
	TranslationCacheFragment *freeFragmentsList;

	/*
	 *	These are the unused client pages as a linked list.
	 *	Can be NULL.
	 */
	ClientPage *freeClientPages;
	
	/*
	 *
	 */
	byte *translationCache;
	
	/*
	 *	Only valid while compiling
	 */
	ClientPage *currentPage;
	uint32 pc;
	uint32 current_opc;

	/*
	 *	Statistics
	 */
	uint64	destroy_write;
	uint64	destroy_oopages;
	uint64	destroy_ootc;
};
extern JITC gJITC;


void FASTCALL jitcEmit1(byte b);
void FASTCALL jitcEmit(byte *instr, int size);
bool FASTCALL jitcEmitAssure(int size);

extern "C" void FASTCALL jitcDestroyAndFreeClientPage(ClientPage *cp);
extern "C" NativeAddress FASTCALL jitcNewPC(uint32 entry);

bool jitc_init(int maxClientPages, uint32 tcSize);
void jitc_done();

static UNUSED void ppc_opc_gen_interpret(ppc_opc_function func) 
{
	jitcClobberAll();
	byte modrm[6];
	asmALUMemImm(X86_MOV, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU.current_opc), gJITC.current_opc);
	asmCALL((NativeAddress)func);
}

#endif
