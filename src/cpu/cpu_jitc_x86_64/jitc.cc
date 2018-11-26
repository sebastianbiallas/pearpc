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
#include <cassert>

#include "system/sysvm.h"
#include "tools/data.h"
#include "tools/snprintf.h"
#include "tools/except.h"

#include "jitc.h"
#include "jitc_debug.h"
#include "jitc_asm.h"

#include "ppc_dec.h"
#include "ppc_mmu.h"
#include "ppc_tools.h"

static TranslationCacheFragment *jitcAllocFragment(JITC &jitc);

/*
 *	Intern
 *	Called whenever a new fragment is needed
 *	returns true if a new fragment was really necessary
 */
static bool jitcEmitNextFragment(JITC &jitc)
{
	// save old
	TranslationCacheFragment *tcf_old = jitc.currentPage->tcf_current;
	NativeAddress tcp_old = jitc.currentPage->tcp;
	// alloc new
	jitc.currentPage->tcf_current = jitcAllocFragment(jitc);
	jitc.currentPage->tcf_current->prev = tcf_old;
	if (uint64(jitc.currentPage->tcf_current->base - jitc.currentPage->tcp) < 20) {
		// next Fragment directly follows
		jitc.currentPage->bytesLeft += FRAGMENT_SIZE;
		return false;
	} else {
		jitc.currentPage->tcp = jitc.currentPage->tcf_current->base;
		jitc.currentPage->bytesLeft = FRAGMENT_SIZE;
		// hardcoded JMP from old to new fragment
		// FIXME: use 0xeb if possible
		tcp_old[0] = 0xe9;
		*((uint32 *)&tcp_old[1]) = jitc.currentPage->tcp - (tcp_old+5);
		return true;
	}
}

/*
 *	emit one byte of native code
 */
void JITC::emit1(byte b)
{
	jitcDebugLogEmit(*this, &b, 1);
	/*
	 *	We always have to leave at least 5 bytes in the fragment
	 *	to issue a final JMP
	 */
	if (currentPage->bytesLeft <= 5) {
		jitcEmitNextFragment(*this);
	}
	*(currentPage->tcp++) = b;
	currentPage->bytesLeft--;
}

/*
 *	emit native code
 */
void JITC::emit(byte *instr, uint size)
{
	jitcDebugLogEmit(*this, instr, size);
	if (int(currentPage->bytesLeft) - int(size) < 5) {
		jitcEmitNextFragment(*this);
	}
	memcpy(currentPage->tcp, instr, size);
	currentPage->tcp += size;
	currentPage->bytesLeft -= size;
}

/*
 *	Assures that the next instruction will be
 *	emitted in the current fragment
 */
bool JITC::emitAssure(uint size)
{
	if (int(currentPage->bytesLeft) - int(size) < 5) {
		jitcEmitNextFragment(*this);
		return false;
	}
	return true;
}

static void jitcEmitAlign(JITC &jitc, uint align)
{
	do {
		uint missalign = ((uint64)jitc.currentPage->tcp) % align;
		if (missalign) {
			int bytes = align - missalign;
			if (jitc.currentPage->bytesLeft - bytes < 5) {
				if (jitcEmitNextFragment(jitc)) continue;
			}
			jitc.currentPage->tcp += bytes;
			jitc.currentPage->bytesLeft -= bytes;
		}
	} while (false);
}
/*
 *	Intern.
 *	Maps ClientPage to base address
 */
static void inline jitcMapClientPage(JITC &jitc, uint32 baseaddr, ClientPage *cp)
{
	jitc.clientPages[baseaddr >> 12] = cp;
	cp->baseaddress = baseaddr;
}

/*
 *	Intern.
 *	Unmaps ClientPage at base address
 */
static void inline jitcUnmapClientPage(JITC &jitc, uint32 baseaddr)
{
	jitc.clientPages[baseaddr >> 12] = NULL;
}

/*
 *	Intern.
 *	Unmaps ClientPage
 */
static void inline jitcUnmapClientPage(JITC &jitc, ClientPage *cp)
{
	jitcUnmapClientPage(jitc, cp->baseaddress);
}

/*
 *	Moves client page to the end of the LRU list
 *	page *must* be in LRU list before
 */
static ClientPage *jitcTouchClientPage(JITC &jitc, ClientPage *cp)
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
			jitc.LRUpage = cp->moreRU;
			jitc.LRUpage->lessRU = NULL;
		}
		cp->moreRU = NULL;
		cp->lessRU = jitc.MRUpage;
		jitc.MRUpage->moreRU = cp;
		jitc.MRUpage = cp;
	}
	return cp;
}

/*
 *	Puts fragments into the freeFragmentsList
 */
//#include <valgrind/valgrind.h>
static void jitcDestroyFragments(JITC &jitc, TranslationCacheFragment *tcf)
{
	// FIXME: this could be done in O(1) with an additional
	// variable in ClientPage
	while (tcf) {
		//VALGRIND_DISCARD_TRANSLATIONS(tcf->base, FRAGMENT_SIZE);
		TranslationCacheFragment *next = tcf->prev;
		tcf->prev = jitc.freeFragmentsList;
		jitc.freeFragmentsList = tcf;
		tcf = next;
	}
}

/*
 *	Unmaps ClientPage and destroys fragments
 */
static void jitcDestroyClientPage(JITC &jitc, ClientPage *cp)
{
	// assert(cp->tcf_current)
	jitcDestroyFragments(jitc, cp->tcf_current);
	cp->destroy();
	jitcUnmapClientPage(jitc, cp);
}

/*
 *	Moves client page into the freeClientPages list
 *	(and out of the LRU list)
 *	page *must* be in LRU list before
 */
static void jitcFreeClientPage(JITC &jitc, ClientPage *cp)
{
	// assert(jitc.LRUpage)
	// assert(jitc.MRUpage)
	
	// delete page from LRU list
	if (!cp->lessRU) {
		// cp is LRU
		if (!cp->moreRU) {
			// cp is also MRU
			jitc.LRUpage = jitc.MRUpage = NULL;
		} else {
			// assert(cp->moreRU)
			jitc.LRUpage = cp->moreRU;
			jitc.LRUpage->lessRU = NULL;
		}
	} else {
		if (!cp->moreRU) {
			// cp is MRU
			// assert(cp->LRUprev)
			jitc.MRUpage = cp->lessRU;
			jitc.MRUpage->moreRU = NULL;
		} else {
			cp->moreRU->lessRU = cp->lessRU;
			cp->lessRU->moreRU = cp->moreRU;
		}
	}
	// and move it into the freeClientPages list
	cp->moreRU = jitc.freeClientPages;
	jitc.freeClientPages = cp;
}

/*
 *	Destroys and frees ClientPage
 */
extern "C" void jitcDestroyAndFreeClientPage(JITC &jitc, ClientPage *cp)
{
	jitc.destroy_write++;
	jitcDestroyClientPage(jitc, cp);
	jitcFreeClientPage(jitc, cp);
}

/*
 *	Destroys and touches ClientPage
 */
static void jitcDestroyAndTouchClientPage(JITC &jitc, ClientPage *cp)
{
	jitc.destroy_oopages++;
	jitcDestroyClientPage(jitc, cp);
	jitcTouchClientPage(jitc, cp);
}

/*
 *	Removes and returns fragment from top of freeFragmentsList
 */
static TranslationCacheFragment *jitcGetFragment(JITC &jitc)
{
	TranslationCacheFragment *tcf = jitc.freeFragmentsList;
	jitc.freeFragmentsList = tcf->prev;
	tcf->prev = NULL;
	return tcf;
}

/*
 *	Returns free fragment
 *	May destroy a page to make new free fragments
 */
static TranslationCacheFragment *jitcAllocFragment(JITC &jitc)
{
	if (!jitc.freeFragmentsList) {
		/*
		 *	There are no free fragments
		 *	-> must free a ClientPage
		 */
		jitc.destroy_write--;	// destroy and free will increase this
		jitc.destroy_ootc++;
		jitcDestroyAndFreeClientPage(jitc, jitc.LRUpage);
	}
	return jitcGetFragment(jitc);
}

/*
 *	Moves page from freeClientPages at the end of the LRU list if there's
 *	a free page or destroys the LRU page and touches it
 */
static ClientPage *jitcCreateClientPage(JITC &jitc, uint32 baseaddr)
{
	ClientPage *cp;
	if (jitc.freeClientPages) {
		// get page
		cp = jitc.freeClientPages;
		jitc.freeClientPages = jitc.freeClientPages->moreRU;
		// and move to the end of LRU list
		if (jitc.MRUpage) {
			jitc.MRUpage->moreRU = cp;
			cp->lessRU = jitc.MRUpage;
			jitc.MRUpage = cp;
		} else {
			cp->lessRU = NULL;
			jitc.LRUpage = jitc.MRUpage = cp;
		}
		cp->moreRU = NULL;
	} else {
		cp = jitc.LRUpage;
		jitcDestroyAndTouchClientPage(jitc, cp);
		// destroy some more
		if (jitc.LRUpage) jitcDestroyAndTouchClientPage(jitc, jitc.LRUpage);
		if (jitc.LRUpage) jitcDestroyAndTouchClientPage(jitc, jitc.LRUpage);
		if (jitc.LRUpage) jitcDestroyAndTouchClientPage(jitc, jitc.LRUpage);
		if (jitc.LRUpage) jitcDestroyAndTouchClientPage(jitc, jitc.LRUpage);
	}
	jitcMapClientPage(jitc, baseaddr, cp);
	return cp;
}

/*
 *	Returns the ClientPage which maps to baseaddr or
 *	creates a new page that maps to baseaddr
 */
static ClientPage *jitcGetOrCreateClientPage(JITC &jitc, uint32 baseaddr)
{
	ClientPage *cp = jitc.clientPages[baseaddr >> 12];
	if (cp) {
		return cp;
	} else {
		return jitcCreateClientPage(jitc, baseaddr);
	}
}

static inline void jitcCreateEntrypoint(byte *translationCache, ClientPage *cp, uint32 ofs)
{
	const ptrdiff_t cacheOffset = cp->tcp - translationCache;
	assert(cacheOffset > 0);
	cp->entrypoints[ofs >> 2] = static_cast<uint32_t>(cacheOffset);
}

static inline NativeAddress jitcGetEntrypoint(byte *translationCache, ClientPage *cp, uint32 ofs)
{
	if (const auto entrypoint = cp->entrypoints[ofs >> 2]) {
		return translationCache + cp->entrypoints[ofs >> 2];
	} else {
		return NULL;
	}
}

extern uint64 jitcCompileTicks;
extern uint64 jitcRunTicks;
extern uint64 jitcRunTicksStart;

extern JITC *gJITC;

#define U32(dest) (*(uint32 *)(void *)(dest))
static NativeAddress jitcNewEntrypoint(JITC &jitc, ClientPage *cp, uint32 baseaddr, uint32 ofs)
{
/*
	jitcRunTicks += jitcDebugGetTicks() - jitcRunTicksStart;
	uint64 jitcCompileStartTicks = jitcDebugGetTicks();
*/
	jitcDebugLogAdd("=== jitcNewEntrypoint: %08x Beginning jitc ===\n", baseaddr+ofs);
	jitc.currentPage = cp;
	
	jitcEmitAlign(jitc, jitc.hostCPUCaps.loop_align);

	NativeAddress entry = cp->tcp;
	jitcCreateEntrypoint(jitc.translationCache, cp, ofs);

	byte *physpage;
	ppc_direct_physical_memory_handle(baseaddr, physpage);

	jitc.pc = ofs;
        jitc.invalidateAll();
	jitc.checkedPriviledge = false;
	jitc.checkedFloat = false;
	jitc.checkedVector = false;
	
	// now we've setup jitc and can start the real compilation

	byte instr[8] = {0x48, 0x3b, 0x3c, 0x25};
	U32(instr + 4) = uint32(uint64(&gJITC));
	while (1) {
		jitc.current_opc = ppc_word_from_BE(*(uint32 *)&physpage[ofs]);
		jitcDebugLogNewInstruction(jitc);
		
//		jitc.clobberAll();
//		jitc.clobberCarryAndFlags();
		// <<
#if 0
		jitc.clobberRegister(NATIVE_REG | RDI);

		jitc.asmALU64(X86_MOV, RDI, curCPU(jitc));
		jitc.emit(instr, sizeof instr);
		NativeAddress f = jitc.asmJxxFixup(X86_E);
		jitc.asmALU32(X86_XOR, RAX, RAX);
		jitc.asmALU32(X86_MOV, RAX, 0, RAX);
		jitc.asmResolveFixup(f);
#endif
		// >>
		
		JITCFlow flow = ppc_gen_opc(jitc);
		if (flow == flowContinue) {
			/* nothing to do */
		} else if (flow == flowEndBlock) {
			jitc.clobberAll();

			jitc.checkedPriviledge = false;
			jitc.checkedFloat = false;
			jitc.checkedVector = false;
			if (ofs+4 < 4096) {
				jitcCreateEntrypoint(jitc.translationCache, cp, ofs+4);
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
			jitc.clobberAll();
			jitc.asmALU32(X86_MOV, RAX, 4096);
			jitc.asmJMP((NativeAddress)ppc_new_pc_rel_asm);
			break;
		}
		jitc.pc += 4;
	}
/*
	jitcRunTicksStart = jitcDebugGetTicks();
	jitcCompileTicks += jitcDebugGetTicks() - jitcCompileStartTicks;	
*/
	return entry;
}

extern "C" NativeAddress jitcStartTranslation(JITC &jitc, ClientPage *cp, uint32 baseaddr, uint32 ofs)
{
	cp->tcf_current = jitcAllocFragment(jitc);
	cp->tcp = cp->tcf_current->base;
	cp->bytesLeft = FRAGMENT_SIZE;
	
	return jitcNewEntrypoint(jitc, cp, baseaddr, ofs);
}

/*
 *	Called whenever the client PC changes (to a new BB)
 *	Note that entry is a physical address
 */
extern "C" NativeAddress jitcNewPC(JITC &jitc, uint32 entry)
{
	if (entry > gMemorySize) {
		ht_printf("entry not physical: %08x\n", entry);
		exit(-1);
	}
	uint32 baseaddr = entry & 0xfffff000;
	ClientPage *cp = jitcGetOrCreateClientPage(jitc, baseaddr);
	jitcTouchClientPage(jitc, cp);
	if (!cp->tcf_current) {
		return jitcStartTranslation(jitc, cp, baseaddr, entry & 0xfff);
	} else {
		NativeAddress ofs = jitcGetEntrypoint(jitc.translationCache, cp, entry & 0xfff);
		if (ofs) {
			return ofs;
		} else {
			return jitcNewEntrypoint(jitc, cp, baseaddr, entry & 0xfff);
		}
	}
}

extern "C" void jitc_error_msr_unsupported_bits(uint32 a)
{
	ht_printf("JITC msr Error: %08x\n", a);
	exit(1);
}

extern "C" void jitc_error(const char *error)
{
	ht_printf("JITC Error: %s\n", error);
	exit(1);
}

extern "C" void FASTCALL jitc_error_singlestep()
{
	ht_printf("JITC Error: Singlestep not supported yet\n");
	exit(1);
}

extern "C" void FASTCALL jitc_error_unknown_exception()
{
        ht_printf("JITC Error: Unknown exception signaled\n");
        exit(1);
}

extern "C" void jitc_error_program(uint32 a, uint32 b)
{
	if (a != 0x00020000) {	// Filter out trap exceptions, no need to report them
		ht_printf("JITC Warning: program exception: %08x %08x\n", a, b);
	}
}

extern "C" void jitc_error_stack_align()
{
	ht_printf("JITC Error: Stack is not aligned.\n");
	exit(1);
}

#if 0
extern uint8 jitcFlagsMapping[257];
extern uint8 jitcFlagsMapping2[256];
extern uint8 jitcFlagsMappingCMP_U[257];
extern uint8 jitcFlagsMappingCMP_L[257];
#endif

bool JITC::init(size_t maxClientPages, size_t tcSize)
{
	x86GetCaps(hostCPUCaps);

	// Limit translation cache size to 2^31 so jmp/call can use 32-bit relative addresses
	if (tcSize > INT32_MAX) {
		tcSize = INT32_MAX;
	}
	
	translationCache = (byte*)sys_alloc_read_write_execute(tcSize);
	if (!translationCache) {
		throw MsgfException("Failed to allocate translation cache of %dMB", tcSize / 1024 / 1024);
	}
	
	// Reserve the initial fragments of the translation cache for jump table entries
	static_assert(kJumpTableSize % FRAGMENT_SIZE == 0, "kJumpTableSize must be a multiple of FRAGMENT_SIZE");
	const size_t jumpTableFragments = (kJumpTableSize / FRAGMENT_SIZE);
	
	// Reserve at least 1 fragment (at offset 0) as this is used to indicate an entry in
	// ClientPage.entrypoints that has not yet been translated
	const size_t firstFragment = std::max<size_t>(1, jumpTableFragments);
	const size_t numFragments = (tcSize / FRAGMENT_SIZE) - firstFragment;

	jumpTableStart = jumpTableCurr = translationCache;

	// Allocate fragments in contiguous memory and point freeFragmentsList into this memory
	fragmentArray = std::vector<TranslationCacheFragment>(numFragments, TranslationCacheFragment());
	
	TranslationCacheFragment *tcf = &fragmentArray.at(0);
	freeFragmentsList = tcf;
	tcf->base = translationCache + firstFragment * FRAGMENT_SIZE;
	for (size_t index = 1; index < numFragments; ++index) {
		tcf->prev = &fragmentArray.at(index);
		tcf = tcf->prev;
		tcf->base = translationCache + (firstFragment + index) * FRAGMENT_SIZE;
	}
	tcf->prev = NULL;

	const size_t maxPages = gMemorySize / 4096;
	clientPages = ppc_malloc(maxPages * sizeof (ClientPage *));
	memset(clientPages, 0, maxPages * sizeof (ClientPage *));

	// Allocate client pages in contiguous memory and point freeClientPages into this memory
	const size_t numClientPages = std::min<size_t>(maxPages, maxClientPages);
	clientPageArray = std::vector<ClientPage>(numClientPages, ClientPage());
	
	ClientPage *cp = &clientPageArray.at(0);
	LRUpage = NULL;
	freeClientPages = cp;
	
	for (size_t i = 1; i < numClientPages; i++) {
		cp->moreRU = &clientPageArray.at(i);
		cp->moreRU->lessRU = cp;
		cp = cp->moreRU;
	}
	MRUpage = NULL;
	
	ht_printf("translation cache    : %u MB @ 0x%p\n", tcSize / 1024 / 1024, translationCache);
	ht_printf("client pages         : %llu (%llu MB)\n", clientPageArray.size(), (clientPageArray.size() * sizeof(clientPageArray[0])) / 1024 / 1024);
	ht_printf("translation fragments: %llu (%llu MB)\n", fragmentArray.size(), (fragmentArray.size() * sizeof(fragmentArray[0])) / 1024 / 1024);
	ht_printf("jump table fragments : %u (%ld B)\n", static_cast<uint32_t>(jumpTableFragments), getJumpTableFreeBytes());

	// initialize native registers
	NativeRegType *nr = ppc_malloc(sizeof (NativeRegType));
	nr->reg = RAX;
	nr->lessRU = NULL;
	LRUreg = nr;
	nativeRegsList[RAX] = nr;
	for (NativeReg reg = RCX; reg <= R15; reg=(NativeReg)(reg+1)) {
		if (reg != RSP && reg != R15) {
			nr->moreRU = ppc_malloc(sizeof (NativeRegType));
			nr->moreRU->lessRU = nr;
			nr = nr->moreRU;
			nr->reg = reg;
			nativeRegsList[reg] = nr;
		}
	}
	nr->moreRU = NULL;
	MRUreg = nr;
#if 0
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
#endif

	for (uint i = XMM0; i<= XMM_SENTINEL; i++) {
		LRUvregs[i] = (NativeVectorReg)(i-1);
		MRUvregs[i] = (NativeVectorReg)(i+1);
	}

	LRUvregs[XMM0] = XMM_SENTINEL;
	MRUvregs[XMM_SENTINEL] = XMM0;

	nativeVectorReg = VECTREG_NO;

	for (uint i=0; i < sizeof clientReg / sizeof clientReg[0]; i++) {
		clientReg[i] = REG_NO;
	}
	for (uint i=0; i < sizeof nativeReg / sizeof nativeReg[0]; i++) {
		nativeReg[i] = PPC_REG_NO;
	}
	memset(nativeRegState, rsUnused, sizeof nativeRegState);

	memset(n2cVectorReg, PPC_REG_NO, sizeof n2cVectorReg);
	memset(c2nVectorReg, VECTREG_NO, sizeof c2nVectorReg);
	memset(nativeVectorRegState, rsUnused, sizeof nativeVectorRegState);

	/*
	 *	This -1 register is to be read-only, and only used when
	 *		needed, and must ALWAYS stay this way!
	 *
	 *	It's absolutely fundamental to doing NOT's with SSE
	 */
//	memset(&gCPU.vr[JITC_VECTOR_NEG1], 0xff, sizeof gCPU.vr[0]);
//	FIX64: put this somewhere else
	return true;
}

void JITC::done()
{
	if (translationCache) sys_free_read_write_execute(translationCache);
}
