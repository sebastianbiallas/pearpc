/*
 *  PearPC
 *  jitc.cc
 *
 *  Copyright (C) 2026 Sebastian Biallas (sb@biallas.net)
 */

#include <cstdlib>
#include <cstring>
#include <pthread.h>

#include "system/sysvm.h"
#include "tools/data.h"
#include "tools/snprintf.h"

#include "debug/tracers.h"
#include "jitc.h"
#include "jitc_debug.h"
#include "jitc_asm.h"

#include "ppc_dec.h"
#include "ppc_mmu.h"
#include "ppc_tools.h"

static TranslationCacheFragment *jitcAllocFragment(JITC &jitc);

/*
 *  W^X helpers for macOS ARM64.
 *  On ARM64, we must toggle between write and execute mode.
 */
static inline void jitcEnableWrite()
{
#if defined(__aarch64__) && defined(__APPLE__)
    pthread_jit_write_protect_np(0);
#endif
}

static inline void jitcEnableExecute()
{
#if defined(__aarch64__) && defined(__APPLE__)
    pthread_jit_write_protect_np(1);
#endif
}

static inline void jitcFlushIcache(void *start, size_t size)
{
#if defined(__aarch64__)
    __builtin___clear_cache((char *)start, (char *)start + size);
#endif
}

/*
 *  Emit a B (unconditional branch) instruction to link fragments.
 *  AArch64 B instruction: 0x14000000 | (imm26)
 *  offset is in bytes, divided by 4 for encoding.
 */
static inline void jitcEmitBranch(NativeAddress from, NativeAddress to)
{
    sint32 offset = (sint32)(to - from);
    uint32 instr = 0x14000000 | (((uint32)(offset / 4)) & 0x03FFFFFF);
    *(uint32 *)from = instr;
}

/*
 *  Called whenever a new fragment is needed.
 *  Returns true if a new fragment was really necessary.
 */
static bool jitcEmitNextFragment(JITC &jitc)
{
    TranslationCacheFragment *tcf_old = jitc.currentPage->tcf_current;
    NativeAddress tcp_old = jitc.currentPage->tcp;

    jitc.currentPage->tcf_current = jitcAllocFragment(jitc);
    jitc.currentPage->tcf_current->prev = tcf_old;
    if (uint64(jitc.currentPage->tcf_current->base - jitc.currentPage->tcp) < 20) {
        // next fragment directly follows
        jitc.currentPage->bytesLeft += FRAGMENT_SIZE;
        return false;
    } else {
        jitc.currentPage->tcp = jitc.currentPage->tcf_current->base;
        jitc.currentPage->bytesLeft = FRAGMENT_SIZE;
        // emit B instruction from old to new fragment
        jitcEmitBranch(tcp_old, jitc.currentPage->tcp);
        return true;
    }
}

/*
 *  Emit one 32-bit AArch64 instruction.
 */
void JITC::emit32(uint32 instr)
{
    /*
     *  We always leave at least 4 bytes (one instruction)
     *  in the fragment for a linking B instruction.
     */
    if (currentPage->bytesLeft <= 4) {
        jitcEmitNextFragment(*this);
    }
    *(uint32 *)(currentPage->tcp) = instr;
    currentPage->tcp += 4;
    currentPage->bytesLeft -= 4;
}

/*
 *  Emit raw bytes of native code.
 */
void JITC::emit(byte *instr, uint size)
{
    if (int(currentPage->bytesLeft) - int(size) < 4) {
        jitcEmitNextFragment(*this);
    }
    memcpy(currentPage->tcp, instr, size);
    currentPage->tcp += size;
    currentPage->bytesLeft -= size;
}

/*
 *  Assures that the next instruction will be
 *  emitted in the current fragment.
 */
bool JITC::emitAssure(uint size)
{
    if (int(currentPage->bytesLeft) - int(size) < 4) {
        jitcEmitNextFragment(*this);
        return false;
    }
    return true;
}

/*
 *  AArch64 emit helpers
 */

void JITC::emitNOP()
{
    emit32(0xD503201F);
}

void JITC::emitMOV32(NativeReg rd, uint32 imm)
{
    // MOVZ Wd, #lo16
    emit32(0x52800000 | (((uint32)(imm & 0xFFFF)) << 5) | rd);
    if (imm >> 16) {
        // MOVK Wd, #hi16, LSL #16
        emit32(0x72A00000 | (((uint32)((imm >> 16) & 0xFFFF)) << 5) | rd);
    }
}

void JITC::emitMOV64(NativeReg rd, uint64 imm)
{
    // MOVZ Xd, #imm[15:0]
    emit32(0xD2800000 | (((uint32)(imm & 0xFFFF)) << 5) | rd);
    if (imm >> 16) {
        emit32(0xF2A00000 | (((uint32)((imm >> 16) & 0xFFFF)) << 5) | rd);
    }
    if (imm >> 32) {
        emit32(0xF2C00000 | (((uint32)((imm >> 32) & 0xFFFF)) << 5) | rd);
    }
    if (imm >> 48) {
        emit32(0xF2E00000 | (((uint32)((imm >> 48) & 0xFFFF)) << 5) | rd);
    }
}

void JITC::emitLDR32_cpu(NativeReg rd, uint32 offset)
{
    // LDR Wd, [X20, #offset]  (X20 = CPU state pointer)
    uint32 uoff = (offset / 4) & 0xFFF;
    emit32(0xB9400000 | (uoff << 10) | (20 << 5) | rd);
}

void JITC::emitSTR32_cpu(NativeReg rs, uint32 offset)
{
    // STR Ws, [X20, #offset]
    uint32 uoff = (offset / 4) & 0xFFF;
    emit32(0xB9000000 | (uoff << 10) | (20 << 5) | rs);
}

void JITC::emitLDR64_cpu(NativeReg rd, uint32 offset)
{
    // LDR Xd, [X20, #offset]
    uint32 uoff = (offset / 8) & 0xFFF;
    emit32(0xF9400000 | (uoff << 10) | (20 << 5) | rd);
}

void JITC::emitSTR64_cpu(NativeReg rs, uint32 offset)
{
    // STR Xs, [X20, #offset]
    uint32 uoff = (offset / 8) & 0xFFF;
    emit32(0xF9000000 | (uoff << 10) | (20 << 5) | rs);
}

void JITC::emitBLR(NativeAddress to)
{
    // Load address into X16 (IP0), then BLR X16
    emitMOV64((NativeReg)16, (uint64)to);
    emit32(0xD63F0000 | (16 << 5)); // BLR X16
}

void JITC::emitBR(NativeReg rn)
{
    emit32(0xD61F0000 | (rn << 5));
}

void JITC::emitRET()
{
    emit32(0xD65F03C0); // RET X30
}

NativeAddress JITC::emitBxxFixup()
{
    // Emit a placeholder B instruction (unconditional, offset 0).
    // Returns the address of the instruction so it can be patched later.
    NativeAddress here = currentPage->tcp;
    emit32(0x14000000); // B #0 (placeholder, will be patched)
    return here;
}

void JITC::resolveFixup(NativeAddress at, NativeAddress to)
{
    // Patch the B instruction at 'at' to branch to 'to' (or current tcp if to==0).
    if (to == 0) to = currentPage->tcp;
    sint64 offset = (sint64)(to - at);
    // B instruction: imm26 field, offset in units of 4 bytes
    sint32 imm26 = (sint32)(offset / 4);
    uint32 insn = 0x14000000 | (imm26 & 0x03FFFFFF);
    *(uint32 *)at = insn;
}

static void jitcEmitAlign(JITC &jitc, uint align)
{
    do {
        uint missalign = ((uint64)jitc.currentPage->tcp) % align;
        if (missalign) {
            int bytes = align - missalign;
            if (jitc.currentPage->bytesLeft - bytes < 4) {
                if (jitcEmitNextFragment(jitc))
                    continue;
            }
            // Fill with NOP instructions (4 bytes each)
            while (bytes >= 4) {
                *(uint32 *)(jitc.currentPage->tcp) = 0xD503201F; // NOP
                jitc.currentPage->tcp += 4;
                jitc.currentPage->bytesLeft -= 4;
                bytes -= 4;
            }
        }
    } while (false);
}

static void inline jitcMapClientPage(JITC &jitc, uint32 baseaddr, ClientPage *cp)
{
    jitc.clientPages[baseaddr >> 12] = cp;
    cp->baseaddress = baseaddr;
}

static void inline jitcUnmapClientPage(JITC &jitc, uint32 baseaddr)
{
    jitc.clientPages[baseaddr >> 12] = NULL;
}

static void inline jitcUnmapClientPage(JITC &jitc, ClientPage *cp)
{
    jitcUnmapClientPage(jitc, cp->baseaddress);
}

static ClientPage *jitcTouchClientPage(JITC &jitc, ClientPage *cp)
{
    if (cp->moreRU) {
        if (cp->lessRU) {
            cp->moreRU->lessRU = cp->lessRU;
            cp->lessRU->moreRU = cp->moreRU;
        } else {
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

static void jitcDestroyFragments(JITC &jitc, TranslationCacheFragment *tcf)
{
    while (tcf) {
        TranslationCacheFragment *next = tcf->prev;
        tcf->prev = jitc.freeFragmentsList;
        jitc.freeFragmentsList = tcf;
        tcf = next;
    }
}

static void jitcDestroyClientPage(JITC &jitc, ClientPage *cp)
{
    jitcDestroyFragments(jitc, cp->tcf_current);
    memset(cp->entrypoints, 0, sizeof cp->entrypoints);
    cp->tcf_current = NULL;
    jitcUnmapClientPage(jitc, cp);
}

static void jitcFreeClientPage(JITC &jitc, ClientPage *cp)
{
    if (!cp->lessRU) {
        if (!cp->moreRU) {
            jitc.LRUpage = jitc.MRUpage = NULL;
        } else {
            jitc.LRUpage = cp->moreRU;
            jitc.LRUpage->lessRU = NULL;
        }
    } else {
        if (!cp->moreRU) {
            jitc.MRUpage = cp->lessRU;
            jitc.MRUpage->moreRU = NULL;
        } else {
            cp->moreRU->lessRU = cp->lessRU;
            cp->lessRU->moreRU = cp->moreRU;
        }
    }
    cp->moreRU = jitc.freeClientPages;
    jitc.freeClientPages = cp;
}

extern "C" void jitcDestroyAndFreeClientPage(JITC &jitc, ClientPage *cp)
{
    jitc.destroy_write++;
    jitcDestroyClientPage(jitc, cp);
    jitcFreeClientPage(jitc, cp);
}

static void jitcDestroyAndTouchClientPage(JITC &jitc, ClientPage *cp)
{
    jitc.destroy_oopages++;
    jitcDestroyClientPage(jitc, cp);
    jitcTouchClientPage(jitc, cp);
}

static TranslationCacheFragment *jitcGetFragment(JITC &jitc)
{
    TranslationCacheFragment *tcf = jitc.freeFragmentsList;
    jitc.freeFragmentsList = tcf->prev;
    tcf->prev = NULL;
    return tcf;
}

static TranslationCacheFragment *jitcAllocFragment(JITC &jitc)
{
    if (!jitc.freeFragmentsList) {
        jitc.destroy_write--;
        jitc.destroy_ootc++;
        jitcDestroyAndFreeClientPage(jitc, jitc.LRUpage);
    }
    return jitcGetFragment(jitc);
}

static ClientPage *jitcCreateClientPage(JITC &jitc, uint32 baseaddr)
{
    ClientPage *cp;
    if (jitc.freeClientPages) {
        cp = jitc.freeClientPages;
        jitc.freeClientPages = jitc.freeClientPages->moreRU;
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
        if (jitc.LRUpage)
            jitcDestroyAndTouchClientPage(jitc, jitc.LRUpage);
        if (jitc.LRUpage)
            jitcDestroyAndTouchClientPage(jitc, jitc.LRUpage);
        if (jitc.LRUpage)
            jitcDestroyAndTouchClientPage(jitc, jitc.LRUpage);
        if (jitc.LRUpage)
            jitcDestroyAndTouchClientPage(jitc, jitc.LRUpage);
    }
    jitcMapClientPage(jitc, baseaddr, cp);
    return cp;
}

static ClientPage *jitcGetOrCreateClientPage(JITC &jitc, uint32 baseaddr)
{
    ClientPage *cp = jitc.clientPages[baseaddr >> 12];
    if (cp) {
        return cp;
    } else {
        return jitcCreateClientPage(jitc, baseaddr);
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

extern JITC *gJITC;
static FILE *gTraceLog = NULL;
static uint64 gTraceCount = 0;

static void traceInit()
{
    if (!gTraceLog) {
        gTraceLog = fopen("jitc_trace.log", "w");
        if (gTraceLog) setvbuf(gTraceLog, NULL, _IOFBF, 256 * 1024);
    }
}

static NativeAddress jitcNewEntrypoint(JITC &jitc, ClientPage *cp, uint32 baseaddr, uint32 ofs)
{
    jitcDebugLogAdd("=== jitcNewEntrypoint: %08x Beginning jitc ===\n", baseaddr + ofs);
    if (gTraceLog) {
        fprintf(gTraceLog, "TRANSLATE %08x\n", baseaddr + ofs);
    }
    jitc.currentPage = cp;

    // Enable write mode for code emission (W^X on macOS)
    pthread_jit_write_protect_np(0);

    // Align to 16 bytes (cache line friendly on ARM64)
    jitcEmitAlign(jitc, 16);

    NativeAddress entry = cp->tcp;
    jitcCreateEntrypoint(cp, ofs);

    byte *physpage;
    ppc_direct_physical_memory_handle(baseaddr, physpage);

    jitc.pc = ofs;
    jitc.invalidateAll();
    jitc.checkedPriviledge = false;
    jitc.checkedFloat = false;
    jitc.checkedVector = false;

    while (1) {
        jitc.current_opc = ppc_word_from_BE(*(uint32 *)&physpage[ofs]);
        jitcDebugLogNewInstruction(jitc);

        JITCFlow flow = ppc_gen_opc(jitc);
        if (flow == flowContinue) {
            /* nothing to do */
        } else if (flow == flowEndBlock) {
            jitc.clobberAll();

            jitc.checkedPriviledge = false;
            jitc.checkedFloat = false;
            jitc.checkedVector = false;
            if (ofs + 4 < 4096) {
                jitcCreateEntrypoint(cp, ofs + 4);
            }
        } else {
            /* flowEndBlockUnreachable */
            break;
        }
        ofs += 4;
        if (ofs == 4096) {
            /*
             *  End of page.
             *  Jump to the next page via ppc_new_pc_rel_asm.
             *  Load 4096 into W0, then branch.
             */
            jitc.clobberAll();
            // MOV W0, #4096 = MOVZ W0, #4096
            jitc.emit32(0x52820000); // MOVZ W0, #0x1000
            // B ppc_new_pc_rel_asm
            // FIXME: for now, use BR with address loaded
            // This needs proper long-range branching
            break;
        }
        jitc.pc += 4;
    }

    return entry;
}

extern "C" NativeAddress jitcStartTranslation(JITC &jitc, ClientPage *cp, uint32 baseaddr, uint32 ofs)
{
    /* Enable JIT write access before allocating/writing fragment (macOS W^X) */
    pthread_jit_write_protect_np(0);

    cp->tcf_current = jitcAllocFragment(jitc);
    cp->tcp = cp->tcf_current->base;
    cp->bytesLeft = FRAGMENT_SIZE;

    return jitcNewEntrypoint(jitc, cp, baseaddr, ofs);
}

/*
 *  Called whenever the client PC changes (to a new BB).
 *  Note that entry is a physical address.
 *
 *  Before returning to execute generated code, we must:
 *  1. Re-enable execute protection (W^X on macOS)
 *  2. Flush the instruction cache
 */
extern "C" NativeAddress jitcNewPC(JITC &jitc, uint32 entry)
{
    traceInit();
    if (gTraceLog) {
        gTraceCount++;
        // Log every dispatch: count, physical entry, key CPU state
        PPC_CPU_State *cpu = (PPC_CPU_State *)((byte *)&jitc - offsetof(PPC_CPU_State, jitc));
        // Actually jitc pointer is stored differently - use the global
        extern PPC_CPU_State *gCPU;
        fprintf(gTraceLog, "%llu pc=%08x cr=%08x lr=%08x ctr=%08x r3=%08x r5=%08x r30=%08x r31=%08x\n",
                gTraceCount, entry,
                gCPU->cr, gCPU->lr, gCPU->ctr,
                gCPU->gpr[3], gCPU->gpr[5],
                gCPU->gpr[30], gCPU->gpr[31]);
        if (gTraceCount % 100 == 0) fflush(gTraceLog);
    }
    if (entry > gMemorySize) {
        ht_printf("entry not physical: %08x\n", entry);
        exit(-1);
    }
    uint32 baseaddr = entry & 0xfffff000;
    ClientPage *cp = jitcGetOrCreateClientPage(jitc, baseaddr);
    jitcTouchClientPage(jitc, cp);

    NativeAddress result;
    if (!cp->tcf_current) {
        result = jitcStartTranslation(jitc, cp, baseaddr, entry & 0xfff);
    } else {
        NativeAddress ofs = jitcGetEntrypoint(cp, entry & 0xfff);
        if (ofs) {
            result = ofs;
        } else {
            pthread_jit_write_protect_np(0);
            result = jitcNewEntrypoint(jitc, cp, baseaddr, entry & 0xfff);
        }
    }

    __builtin___clear_cache((char *)jitc.translationCache, (char *)jitc.translationCache + 64 * 1024 * 1024);
    pthread_jit_write_protect_np(1);
    return result;
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
    if (a != 0x00020000) {
        ht_printf("JITC Warning: program exception: %08x %08x\n", a, b);
    }
}

void JITC::clobberAll()
{
    for (uint i = 0; i < sizeof nativeReg / sizeof nativeReg[0]; i++) {
        nativeReg[i] = PPC_REG_NO;
        nativeRegState[i] = rsUnused;
    }
    for (uint i = 0; i < sizeof clientReg / sizeof clientReg[0]; i++) {
        clientReg[i] = REG_NO;
    }
    nativeFlagsState = rsUnused;
    nativeCarryState = rsUnused;
}

void JITC::invalidateAll()
{
    clobberAll();
    nativeFlags = PPC_NO_CRx;
}

extern "C" void jitc_error_stack_align()
{
    ht_printf("JITC Error: Stack is not aligned.\n");
    exit(1);
}

bool JITC::init(uint maxClientPages, uint32 tcSize)
{
    memset(this, 0, sizeof *this);

    // AArch64 instructions must be 4-byte aligned; align loops to 16 bytes
    loop_align = 16;

    /*
     *  On macOS AArch64, sys_alloc_read_write_execute should use
     *  MAP_JIT flag in mmap() to enable W^X toggling via
     *  pthread_jit_write_protect_np().
     */
    translationCache = (byte *)sys_alloc_read_write_execute(tcSize);

    ht_printf("translation cache: %p (aarch64 JIT)\n", translationCache);

    if (!translationCache)
        return false;
    int maxPages = gMemorySize / 4096;
    clientPages = ppc_malloc(maxPages * sizeof(ClientPage *));
    memset(clientPages, 0, maxPages * sizeof(ClientPage *));

    // allocate fragments
    TranslationCacheFragment *tcf = ppc_malloc(sizeof(TranslationCacheFragment));
    freeFragmentsList = tcf;
    tcf->base = translationCache;
    for (uint32 addr = FRAGMENT_SIZE; addr < tcSize; addr += FRAGMENT_SIZE) {
        tcf->prev = ppc_malloc(sizeof(TranslationCacheFragment));
        tcf = tcf->prev;
        tcf->base = translationCache + addr;
    }
    tcf->prev = NULL;

    // allocate client pages
    ClientPage *cp = ppc_malloc(sizeof(ClientPage));
    memset(cp->entrypoints, 0, sizeof cp->entrypoints);
    cp->tcf_current = NULL;
    cp->lessRU = NULL;
    LRUpage = NULL;
    freeClientPages = cp;
    for (uint i = 1; i < maxClientPages; i++) {
        cp->moreRU = ppc_malloc(sizeof(ClientPage));
        cp->moreRU->lessRU = cp;
        cp = cp->moreRU;

        memset(cp->entrypoints, 0, sizeof cp->entrypoints);
        cp->tcf_current = NULL;
    }
    cp->moreRU = NULL;
    MRUpage = NULL;

    // Initialize native register LRU list
    // AArch64 has 31 GPRs; we use a subset for register allocation.
    // For now, initialize with X0-X15, X21-X28 (skip X16-X20, X29-X31)
    int allocRegs[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 21, 22, 23, 24, 25, 26, 27, 28};
    int numAllocRegs = sizeof(allocRegs) / sizeof(allocRegs[0]);

    NativeRegType *nr = ppc_malloc(sizeof(NativeRegType));
    nr->reg = (NativeReg)allocRegs[0];
    nr->lessRU = NULL;
    LRUreg = nr;
    nativeRegsList[allocRegs[0]] = nr;
    for (int i = 1; i < numAllocRegs; i++) {
        nr->moreRU = ppc_malloc(sizeof(NativeRegType));
        nr->moreRU->lessRU = nr;
        nr = nr->moreRU;
        nr->reg = (NativeReg)allocRegs[i];
        nativeRegsList[allocRegs[i]] = nr;
    }
    nr->moreRU = NULL;
    MRUreg = nr;

    for (uint i = 0; i < sizeof clientReg / sizeof clientReg[0]; i++) {
        clientReg[i] = REG_NO;
    }
    for (uint i = 0; i < sizeof nativeReg / sizeof nativeReg[0]; i++) {
        nativeReg[i] = PPC_REG_NO;
    }
    memset(nativeRegState, rsUnused, sizeof nativeRegState);

    return true;
}

void JITC::done()
{
    if (translationCache)
        sys_free_read_write_execute(translationCache);
}
