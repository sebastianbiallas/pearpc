/*
 *	PearPC
 *	jitc.cc
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

#include <cstdlib>
#include <cstring>

#include "system/sys.h"
#include "tools/snprintf.h"

#include "jitc.h"
#include "jitc_debug.h"
#include "jitc_asm.h"

#include "ppc_dec.h"
#include "ppc_mmu.h"
#include "ppc_tools.h"

JITC gJITC;

static TranslationCacheFragment *jitcAllocFragment();

/*
 *	Intern
 *	Called whenever a new fragment is needed
 *	returns true if a new fragment was really necessary
 */
static bool FASTCALL jitcEmitNextFragment()
{
	// save old
	TranslationCacheFragment *tcf_old = gJITC.currentPage->tcf_current;
	NativeAddress tcp_old = gJITC.currentPage->tcp;
	// alloc new
	gJITC.currentPage->tcf_current = jitcAllocFragment();
	gJITC.currentPage->tcf_current->prev = tcf_old;
	if (((uint)(gJITC.currentPage->tcf_current->base - gJITC.currentPage->tcp)) < 20) {
		// next Fragment directly follows
		gJITC.currentPage->bytesLeft += FRAGMENT_SIZE;
		return false;
	} else {
		gJITC.currentPage->tcp = gJITC.currentPage->tcf_current->base;
		gJITC.currentPage->bytesLeft = FRAGMENT_SIZE;
		// hardcoded JMP from old to new fragment
		// FIXME: use 0xeb if possible
		tcp_old[0] = 0xe9;
		*((uint32 *)&tcp_old[1]) = gJITC.currentPage->tcp - (tcp_old+5);
		return true;
	}
}

/*
 *	emit one byte of native code
 */
void FASTCALL jitcEmit1(byte b)
{
	jitcDebugLogEmit(&b, 1);
	/*
	 *	We always have to leave at least 5 bytes in the fragment
	 *	to issue a final JMP
	 */
	if (gJITC.currentPage->bytesLeft <= 5) {
		jitcEmitNextFragment();
	}
	*(gJITC.currentPage->tcp++) = b;
	gJITC.currentPage->bytesLeft--;
}

/*
 *	emit native code
 */
void FASTCALL jitcEmit(byte *instr, int size)
{
	jitcDebugLogEmit(instr, size);
	if ((gJITC.currentPage->bytesLeft - size) < 5) {
		jitcEmitNextFragment();
	}
	memcpy(gJITC.currentPage->tcp, instr, size);
	gJITC.currentPage->tcp += size;
	gJITC.currentPage->bytesLeft -= size;
}

/*
 *	Assures that the next instruction will be
 *	emitted in the current fragment
 */
bool FASTCALL jitcEmitAssure(int size)
{
	if ((gJITC.currentPage->bytesLeft - size) < 5) {
		jitcEmitNextFragment();
		return false;
	}
	return true;
}

void FASTCALL jitcEmitAlign(int align)
{
	do {
		int missalign = ((uint)gJITC.currentPage->tcp) % align;
		if (missalign) {
			int bytes = align - missalign;
			if ((gJITC.currentPage->bytesLeft - bytes) < 5) {
				if (jitcEmitNextFragment()) continue;
			}
			gJITC.currentPage->tcp += bytes;
			gJITC.currentPage->bytesLeft -= bytes;
		}
	} while (false);
}
/*
 *	Intern.
 *	Maps ClientPage to base address
 */
static void inline jitcMapClientPage(uint32 baseaddr, ClientPage *cp)
{
	gJITC.clientPages[baseaddr >> 12] = cp;
	cp->baseaddress = baseaddr;
}

/*
 *	Intern.
 *	Unmaps ClientPage at base address
 */
static void inline jitcUnmapClientPage(uint32 baseaddr)
{
	gJITC.clientPages[baseaddr >> 12] = NULL;
}

/*
 *	Intern.
 *	Unmaps ClientPage
 */
static void inline jitcUnmapClientPage(ClientPage *cp)
{
	jitcUnmapClientPage(cp->baseaddress);
}

/*
 *	Moves client page to the end of the LRU list
 *	page *must* be in LRU list before
 */
extern "C" FASTCALL ClientPage *jitcTouchClientPage(ClientPage *cp)
{
	if (cp->moreRU) {
		// there's a page which is used more recently
		if (cp->lessRU) {
			// we've got at least 3 pages and 
			// cp is neither LRU nor MRU 
			cp->moreRU->lessRU = cp->lessRU;
			cp->lessRU->moreRU = cp->moreRU;		
		} else {
			// page is LRU
			gJITC.LRUpage = cp->moreRU;
			gJITC.LRUpage->lessRU = NULL;
		}
		cp->moreRU = NULL;
		cp->lessRU = gJITC.MRUpage;
		gJITC.MRUpage->moreRU = cp;
		gJITC.MRUpage = cp;
	}
	return cp;
}

/*
 *	Puts fragments into the freeFragmentsList
 */
//#include <valgrind/valgrind.h>
void FASTCALL jitcDestroyFragments(TranslationCacheFragment *tcf)
{
	while (tcf) {
//		VALGRIND_DISCARD_TRANSLATIONS(tcf->base, FRAGMENT_SIZE);
		// FIXME: this could be done in O(1) with an additional
		// variable in ClientPage
		TranslationCacheFragment *next = tcf->prev;
		tcf->prev = gJITC.freeFragmentsList;
		gJITC.freeFragmentsList = tcf;
		tcf = next;
	}
}

/*
 *	Unmaps ClientPage and destroys fragments
 */
static void FASTCALL jitcDestroyClientPage(ClientPage *cp)
{
	// assert(cp->tcf_current)
	jitcDestroyFragments(cp->tcf_current);
	memset(cp->entrypoints, 0, sizeof cp->entrypoints);
	cp->tcf_current = NULL;
	jitcUnmapClientPage(cp);
}

/*
 *	Moves client page into the freeClientPages list
 *	(and out of the LRU list)
 *	page *must* be in LRU list before
 */
static void FASTCALL jitcFreeClientPage(ClientPage *cp)
{
	// assert(gJITC.LRUpage)
	// assert(gJITC.MRUpage)
	
	// delete page from LRU list
	if (!cp->lessRU) {
		// cp is LRU
		if (!cp->moreRU) {
			// cp is also MRU
			gJITC.LRUpage = gJITC.MRUpage = NULL;
		} else {
			// assert(cp->moreRU)
			gJITC.LRUpage = cp->moreRU;
			gJITC.LRUpage->lessRU = NULL;
		}
	} else {
		if (!cp->moreRU) {
			// cp is MRU
			// assert(cp->LRUprev)
			gJITC.MRUpage = cp->lessRU;
			gJITC.MRUpage->moreRU = NULL;
		} else {
			cp->moreRU->lessRU = cp->lessRU;
			cp->lessRU->moreRU = cp->moreRU;
		}
	}
	// and move it into the freeClientPages list
	cp->moreRU = gJITC.freeClientPages;
	gJITC.freeClientPages = cp;
}

/*
 *	Destroys and frees ClientPage
 */
extern "C" void FASTCALL jitcDestroyAndFreeClientPage(ClientPage *cp)
{
	gJITC.destroy_write++;
	jitcDestroyClientPage(cp);
	jitcFreeClientPage(cp);
}

/*
 *	Destroys and touches ClientPage
 */
static void FASTCALL jitcDestroyAndTouchClientPage(ClientPage *cp)
{
	gJITC.destroy_oopages++;
	jitcDestroyClientPage(cp);
	jitcTouchClientPage(cp);
}

/*
 *	Removes and returns fragment from top of freeFragmentsList
 */
static TranslationCacheFragment *jitcGetFragment()
{
	TranslationCacheFragment *tcf = gJITC.freeFragmentsList;
	gJITC.freeFragmentsList = tcf->prev;
	tcf->prev = NULL;
	return tcf;
}

/*
 *	Returns free fragment
 *	May destroy a page to make new free fragments
 */
static TranslationCacheFragment *jitcAllocFragment()
{
	if (!gJITC.freeFragmentsList) {
		/*
		 *	There are no free fragments
		 *	-> must free a ClientPage
		 */
		gJITC.destroy_write--;	// destroy and free will increase this
		gJITC.destroy_ootc++;
		jitcDestroyAndFreeClientPage(gJITC.LRUpage);
	}
	return jitcGetFragment();
}

/*
 *	Moves page from freeClientPages at the end of the LRU list if there's
 *	a free page or destroys the LRU page and touches it
 */
extern "C" ClientPage FASTCALL *jitcCreateClientPage(uint32 baseaddr)
{
	ClientPage *cp;
	if (gJITC.freeClientPages) {
		// get page
		cp = gJITC.freeClientPages;
		gJITC.freeClientPages = gJITC.freeClientPages->moreRU;
		// and move to the end of LRU list
		if (gJITC.MRUpage) {
			gJITC.MRUpage->moreRU = cp;
			cp->lessRU = gJITC.MRUpage;
			gJITC.MRUpage = cp;
		} else {
			cp->lessRU = NULL;
			gJITC.LRUpage = gJITC.MRUpage = cp;
		}
		cp->moreRU = NULL;
	} else {
		cp = gJITC.LRUpage;
		jitcDestroyAndTouchClientPage(cp);
		// destroy some more
		if (gJITC.LRUpage) jitcDestroyAndTouchClientPage(gJITC.LRUpage);
		if (gJITC.LRUpage) jitcDestroyAndTouchClientPage(gJITC.LRUpage);
		if (gJITC.LRUpage) jitcDestroyAndTouchClientPage(gJITC.LRUpage);
		if (gJITC.LRUpage) jitcDestroyAndTouchClientPage(gJITC.LRUpage);
	}
	jitcMapClientPage(baseaddr, cp);
	return cp;
}

/*
 *	Returns the ClientPage which maps to baseaddr or
 *	creates a new page that maps to baseaddr
 */
ClientPage FASTCALL *jitcGetOrCreateClientPage(uint32 baseaddr)
{
	ClientPage *cp = gJITC.clientPages[baseaddr >> 12];
	if (cp) {
		return cp;
	} else {
		return jitcCreateClientPage(baseaddr);
	}
}

static inline void jitcCreateEntrypoint(ClientPage *cp, uint32 ofs)
{
	cp->entrypoints[ofs >> 2] = cp->tcp;
}

static inline NativeAddress jitcGetEntrypoint(ClientPage *cp, uint32 ofs)
{
	return cp->entrypoints[ofs >> 2];
}

extern uint64 gJITCCompileTicks;
extern uint64 gJITCRunTicks;
extern uint64 gJITCRunTicksStart;

extern "C" NativeAddress FASTCALL jitcNewEntrypoint(ClientPage *cp, uint32 baseaddr, uint32 ofs)
{
/*
	gJITCRunTicks += jitcDebugGetTicks() - gJITCRunTicksStart;
	uint64 jitcCompileStartTicks = jitcDebugGetTicks();
*/
	jitcDebugLogAdd("=== jitcNewEntrypoint: %08x Beginning jitc ===\n", baseaddr+ofs);
	gJITC.currentPage = cp;
	
	jitcEmitAlign(gJITC.hostCPUCaps.loop_align);
	
	NativeAddress entry = cp->tcp;
	jitcCreateEntrypoint(cp, ofs);

	byte *physpage;
	ppc_direct_physical_memory_handle(baseaddr, physpage);

	gJITC.pc = ofs;
        jitcInvalidateAll();
	gJITC.checkedPriviledge = false;
	gJITC.checkedFloat = false;
	gJITC.checkedVector = false;
	
	// now we've setup gJITC and can start the real compilation

	while (1) {
		gJITC.current_opc = ppc_word_from_BE(*(uint32 *)(&physpage[ofs]));
		jitcDebugLogNewInstruction();
		JITCFlow flow = ppc_gen_opc();
		if (flow == flowContinue) {
			/* nothing to do */
		} else if (flow == flowEndBlock) {
			jitcClobberAll();
			gJITC.checkedPriviledge = false;
			gJITC.checkedFloat = false;
			gJITC.checkedVector = false;
			if (ofs+4 < 4096) {
				jitcCreateEntrypoint(cp, ofs+4);
			}
		} else {
			/* flowEndBlockUnreachable */
			break;
		}
		ofs += 4;
		if (ofs == 4096) {
			/*
			 *	End of page.
			 *	We must use jump to the next page via 
			 *	ppc_new_pc_asm
			 */
			jitcClobberAll();
			asmALURegImm(X86_MOV, EAX, 4096);
			asmJMP((NativeAddress)ppc_new_pc_rel_asm);
			break;
		}
		gJITC.pc += 4;
	}
/*
	gJITCRunTicksStart = jitcDebugGetTicks();
	gJITCCompileTicks += jitcDebugGetTicks() - jitcCompileStartTicks;	
*/
	return entry;
}

extern "C" NativeAddress FASTCALL jitcStartTranslation(ClientPage *cp, uint32 baseaddr, uint32 ofs)
{
	cp->tcf_current = jitcAllocFragment();
	cp->tcp = cp->tcf_current->base;
	cp->bytesLeft = FRAGMENT_SIZE;
	
	return jitcNewEntrypoint(cp, baseaddr, ofs);
}

/*
 *	Called whenever the client PC changes (to a new BB)
 *	Note that entry is a physical address
 */
extern "C" NativeAddress FASTCALL jitcNewPC(uint32 entry)
{
	if (entry > gMemorySize) {
		ht_printf("entry not physical: %08x\n", entry);
		exit(-1);
	}
	uint32 baseaddr = entry & 0xfffff000;
	ClientPage *cp = jitcGetOrCreateClientPage(baseaddr);
	jitcTouchClientPage(cp);
	if (!cp->tcf_current) {
		return jitcStartTranslation(cp, baseaddr, entry & 0xfff);
	} else {
		NativeAddress ofs = jitcGetEntrypoint(cp, entry & 0xfff);
		if (ofs) {
			return ofs;
		} else {
			return jitcNewEntrypoint(cp, baseaddr, entry & 0xfff);
		}
	}
}

extern "C" void FASTCALL jitc_error_msr_unsupported_bits(uint32 a)
{
	ht_printf("JITC msr Error: %08x\n", a);
	exit(1);
}

extern "C" void FASTCALL jitc_error(const char *error)
{
	ht_printf("JITC Error: %s\n", error);
	exit(1);
}

extern "C" void FASTCALL jitc_error_program(uint32 a, uint32 b)
{
	if (a != 0x00020000) {	// Filter out trap exceptions, no need to report them
		ht_printf("JITC Warning: program exception: %08x %08x\n", a, b);
	}
}

extern uint8 jitcFlagsMapping[257];
extern uint8 jitcFlagsMapping2[256];
extern uint8 jitcFlagsMappingCMP_U[257];
extern uint8 jitcFlagsMappingCMP_L[257];

bool jitc_init(int maxClientPages, uint32 tcSize)
{
	memset(&gJITC, 0, sizeof gJITC);

	x86GetCaps(gJITC.hostCPUCaps);

	gJITC.translationCache = (byte*)sys_alloc_read_write_execute(tcSize);
	if (!gJITC.translationCache) return false;
	int maxPages = gMemorySize / 4096;
	gJITC.clientPages = (ClientPage **)malloc(maxPages * sizeof (ClientPage *));
	memset(gJITC.clientPages, 0, maxPages * sizeof (ClientPage *));

	// allocate fragments
	TranslationCacheFragment *tcf = (TranslationCacheFragment *)malloc(sizeof (TranslationCacheFragment));
	gJITC.freeFragmentsList = tcf;
	tcf->base = gJITC.translationCache;
	for (uint32 addr=FRAGMENT_SIZE; addr < tcSize; addr += FRAGMENT_SIZE) {
		tcf->prev = (TranslationCacheFragment *)malloc(sizeof (TranslationCacheFragment));
		tcf = tcf->prev;
		tcf->base = gJITC.translationCache + addr;
	}
	tcf->prev = NULL;
	
	// allocate client pages
	ClientPage *cp = (ClientPage *)malloc(sizeof (ClientPage));
	memset(cp->entrypoints, 0, sizeof cp->entrypoints);
	cp->tcf_current = NULL; // not translated yet
	cp->lessRU = NULL;
	gJITC.LRUpage = NULL;
	gJITC.freeClientPages = cp;
	for (int i=1; i < maxClientPages; i++) {
		cp->moreRU = (ClientPage *)malloc(sizeof (ClientPage));
		cp->moreRU->lessRU = cp;
		cp = cp->moreRU;
		
		memset(cp->entrypoints, 0, sizeof cp->entrypoints);
		cp->tcf_current = NULL; // not translated yet
	}
	cp->moreRU = NULL;
	gJITC.MRUpage = NULL;
	
	// initialize native registers
	NativeRegType *nr = (NativeRegType *)malloc(sizeof (NativeRegType));
	nr->reg = EAX;
	nr->lessRU = NULL;
	gJITC.LRUreg = nr;
	gJITC.nativeRegsList[EAX] = nr;
	for (NativeReg reg = ECX; reg <= EDI; reg=(NativeReg)(reg+1)) {
		if (reg != ESP) {
			nr->moreRU = (NativeRegType *)malloc(sizeof (NativeRegType));
			nr->moreRU->lessRU = nr;
			nr = nr->moreRU;
			nr->reg = reg;
			gJITC.nativeRegsList[reg] = nr;
		}
	}
	nr->moreRU = NULL;
	gJITC.MRUreg = nr;

	for (int i=1; i<9; i++) {
		gJITC.floatRegPerm[i] = i;
		gJITC.floatRegPermInverse[i] = i;
	}

	jitcFlagsMapping[0] = 1<<6; // GT
	jitcFlagsMapping[1] = 1<<7; // LT
	jitcFlagsMapping[1<<8] = 1<<5; // EQ
	jitcFlagsMappingCMP_U[0] = 1<<5; // EQ
	jitcFlagsMappingCMP_U[1] = 1<<6; // GT
	jitcFlagsMappingCMP_U[1<<8] = 1<<7; // LT
	jitcFlagsMappingCMP_L[0] = 1<<1; // EQ
	jitcFlagsMappingCMP_L[1] = 1<<2; // GT
	jitcFlagsMappingCMP_L[1<<8] = 1<<3; // LT
	for (int i=0; i<256; i++) {
		switch (i & 0xc0) {
		case 0x00: // neither zero nor sign
			jitcFlagsMapping2[i] = 1<<6; // GT
			break;
		case 0x40: // zero flag
			jitcFlagsMapping2[i] = 1<<5; // EQ
			break;
		case 0x80: // sign flag
			jitcFlagsMapping2[i] = 1<<7; // LT
			break;
		case 0xc0: // impossible
			jitcFlagsMapping2[i] = 0; 
			break;
		}
	}

	/*
	 *	Note that REG_NO=-1 and PPC_REG_NO=0 so this works 
	 *	by accident.
	 */
	memset(gJITC.clientReg, REG_NO, sizeof gJITC.clientReg);	
	memset(gJITC.nativeReg, PPC_REG_NO, sizeof gJITC.nativeReg);
	memset(gJITC.nativeRegState, rsUnused, sizeof gJITC.nativeRegState);
	
	memset(gJITC.tlb_code_eff, 0xff, sizeof gJITC.tlb_code_eff);
	memset(gJITC.tlb_data_read_eff, 0xff, sizeof gJITC.tlb_data_read_eff);
	memset(gJITC.tlb_data_write_eff, 0xff, sizeof gJITC.tlb_data_write_eff);
	gJITC.tlb_code_hits = 0;
	gJITC.tlb_data_read_hits = 0;
	gJITC.tlb_data_write_hits = 0;
	gJITC.tlb_code_misses = 0;
	gJITC.tlb_data_read_misses = 0;
	gJITC.tlb_data_write_misses = 0;
	return true;
}

void jitc_done()
{
	if (gJITC.translationCache) sys_free_read_write_execute(gJITC.translationCache);
}
