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
#include "aarch64asm.h"

byte *gTranslationCacheBase = NULL;
extern void jitc_dump_and_exit(int code);

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
 */
static inline void jitcEmitBranch(NativeAddress from, NativeAddress to)
{
    sint32 offset = (sint32)(to - from);
    *(uint32 *)from = a64_B(offset);
}

/*
 *  Called whenever a new fragment is needed.
 *  Returns true if a new fragment was really necessary.
 */
static bool jitcEmitNextFragment(JITC &jitc)
{
    TranslationCacheFragment *tcf_old = jitc.currentPage->tcf_current;
    NativeAddress tcp_old = jitc.currentPage->tcp;

    if (jitc.currentPage->bytesLeft < 0) {
        PPC_CPU_ERR("jitcEmitNextFragment bytesLeft=%d < 0\n", jitc.currentPage->bytesLeft);
    }

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
        if (jitc.currentPage->bytesLeft < 4) {
            PPC_CPU_ERR("no space for linking branch (bytesLeft was %d)\n", (int)(tcp_old - tcf_old->base));
        }
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
    if (currentPage->bytesLeft < 4) {
        PPC_CPU_ERR("emit32 bytesLeft=%d after fragment chain\n", currentPage->bytesLeft);
    }
    // Verify tcp is within the translation cache
    if (currentPage->tcp < translationCache || currentPage->tcp >= translationCache + 64 * 1024 * 1024) {
        PPC_CPU_ERR("emit32 tcp=%p outside translation cache [%p, %p)\n", currentPage->tcp, translationCache,
                    translationCache + 64 * 1024 * 1024);
    }
    // Verify tcp + bytesLeft doesn't exceed the translation cache
    // (accounts for contiguous fragment optimization where tcp may be
    // before the current fragment's base)
    {
        NativeAddress end = currentPage->tcp + currentPage->bytesLeft;
        TranslationCacheFragment *tcf = currentPage->tcf_current;
        if (tcf && end > tcf->base + FRAGMENT_SIZE) {
            PPC_CPU_ERR("emit32 OOB: tcp=%p bytesLeft=%d end=%p fragment_end=%p\n", currentPage->tcp,
                        currentPage->bytesLeft, end, tcf->base + FRAGMENT_SIZE);
        }
    }
    jitcDebugLogEmit(*this, (const byte *)&instr, 4);
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
 *  AArch64 assembler methods
 */

void JITC::asmNOP()
{
    emit32(a64_NOP());
}

void JITC::asmMOV(NativeReg rd, uint32 imm)
{
    emit32(a64_MOVZw(rd, imm & 0xFFFF, 0));
    if (imm >> 16) {
        emit32(a64_MOVKw(rd, (imm >> 16) & 0xFFFF, 16));
    }
}

void JITC::asmMOV64(NativeReg rd, uint64 imm)
{
    emit32(a64_MOVZ(rd, imm & 0xFFFF, 0));
    if (imm >> 16) {
        emit32(a64_MOVK(rd, (imm >> 16) & 0xFFFF, 16));
    }
    if (imm >> 32) {
        emit32(a64_MOVK(rd, (imm >> 32) & 0xFFFF, 32));
    }
    if (imm >> 48) {
        emit32(a64_MOVK(rd, (imm >> 48) & 0xFFFF, 48));
    }
}

void JITC::asmMOV(NativeReg rd, NativeReg rs)
{
    emit32(a64_MOV(rd, rs));
}

void JITC::asmLDRw_cpu(NativeReg rd, uint32 offset)
{
    emit32(a64_LDRw(rd, X20, offset));
}

void JITC::asmSTRw_cpu(NativeReg rs, uint32 offset)
{
    emit32(a64_STRw(rs, X20, offset));
}

void JITC::asmLDR_cpu(NativeReg rd, uint32 offset)
{
    emit32(a64_LDR(rd, X20, offset));
}

void JITC::asmSTR_cpu(NativeReg rs, uint32 offset)
{
    emit32(a64_STR(rs, X20, offset));
}

void JITC::asmBL(NativeAddress to)
{
    emitAssure(a64_bl_size((uint64)to));
    asmMOV64(X16, (uint64)to);
    emit32(a64_BLR(X16));
}

void JITC::asmB(NativeAddress to)
{
    sint64 offset = (sint64)(to - currentPage->tcp);
    sint32 imm26 = (sint32)(offset / 4);
    if (imm26 <= 0x1FFFFFF && imm26 >= -0x2000000) {
        emit32(a64_B(offset));
    } else {
        emitAssure(a64_mov64_size((uint64)to) + 4);
        asmMOV64(X16, (uint64)to);
        asmBR(X16);
    }
}

void JITC::asmBR(NativeReg rn)
{
    emit32(a64_BR(rn));
}

void JITC::asmCALL(NativeAddress to)
{
    asmBL(to);
}

void JITC::asmCALL_cpu(int stubIndex)
{
    uint32 offset = offsetof(PPC_CPU_State, stubs) + stubIndex * sizeof(NativeAddress);
    emitAssure(8);
    asmLDR_cpu(X16, offset);
    emit32(a64_BLR(X16));
}

void JITC::asmRET()
{
    emit32(a64_RET());
}

void JITC::asmADDw(NativeReg rd, NativeReg rn, NativeReg rm)
{
    emit32(a64_ADDw_reg(rd, rn, rm));
}

void JITC::asmSUBw(NativeReg rd, NativeReg rn, NativeReg rm)
{
    emit32(a64_SUBw_reg(rd, rn, rm));
}

void JITC::asmANDw(NativeReg rd, NativeReg rn, NativeReg rm)
{
    emit32(a64_ANDw_reg(rd, rn, rm));
}

void JITC::asmORRw(NativeReg rd, NativeReg rn, NativeReg rm)
{
    emit32(a64_ORRw_reg(rd, rn, rm));
}

void JITC::asmEORw(NativeReg rd, NativeReg rn, NativeReg rm)
{
    emit32(a64_EORw_reg(rd, rn, rm));
}

void JITC::asmMULw(NativeReg rd, NativeReg rn, NativeReg rm)
{
    emit32(a64_MULw(rd, rn, rm));
}

void JITC::asmNEGw(NativeReg rd, NativeReg rm)
{
    emit32(a64_NEGw(rd, rm));
}

void JITC::asmADDw(NativeReg rd, NativeReg rn, uint32 imm12)
{
    emit32(a64_ADDw_imm(rd, rn, imm12));
}

void JITC::asmSUBw(NativeReg rd, NativeReg rn, uint32 imm12)
{
    emit32(a64_SUBw_imm(rd, rn, imm12));
}

void JITC::asmCMPw(NativeReg rn, NativeReg rm)
{
    emit32(a64_CMPw_reg(rn, rm));
}

void JITC::asmCMPw(NativeReg rn, uint32 imm12)
{
    emit32(a64_CMPw_imm(rn, imm12));
}

void JITC::asmSUBSw(NativeReg rd, NativeReg rn, uint32 imm12)
{
    emit32(a64_SUBSw_imm(rd, rn, imm12));
}

void JITC::asmTBZ(NativeReg rt, int bit, sint32 offset)
{
    emit32(a64_TBZ(rt, bit, offset));
}

void JITC::asmTBNZ(NativeReg rt, int bit, sint32 offset)
{
    emit32(a64_TBNZ(rt, bit, offset));
}

void JITC::asmCBZw(NativeReg rt, sint32 offset)
{
    emit32(a64_CBZw(rt, offset));
}

void JITC::asmCBNZw(NativeReg rt, sint32 offset)
{
    emit32(a64_CBNZw(rt, offset));
}

void JITC::asmTSTw(NativeReg rn, int immr, int imms)
{
    emit32(a64_TSTw_imm(rn, immr, imms));
}

void JITC::asmANDw_imm(NativeReg rd, NativeReg rn, int immr, int imms)
{
    emit32(a64_ANDw_imm(rd, rn, immr, imms));
}

void JITC::asmLSRw_imm(NativeReg rd, NativeReg rn, int shift)
{
    emit32(a64_LSRw_imm(rd, rn, shift));
}

void JITC::asmADD(NativeReg rd, NativeReg rn, uint32 imm12)
{
    emit32(a64_ADD_imm(rd, rn, imm12));
}

void JITC::asmLDRw_reg(NativeReg rt, NativeReg rn, NativeReg rm, bool shift)
{
    emit32(a64_LDRw_reg(rt, rn, rm, shift));
}

void JITC::asmLDR_reg(NativeReg rt, NativeReg rn, NativeReg rm, bool shift)
{
    emit32(a64_LDR_reg(rt, rn, rm, shift));
}

void JITC::asmREVw(NativeReg rd, NativeReg rn)
{
    emit32(a64_REVw(rd, rn));
}

void JITC::asmREV16w(NativeReg rd, NativeReg rn)
{
    emit32(a64_REV16w(rd, rn));
}

void JITC::asmBFIw(NativeReg rd, NativeReg rn, int lsb, int width)
{
    emit32(a64_BFIw(rd, rn, lsb, width));
}

void JITC::asmCSELw(NativeReg rd, NativeReg rn, NativeReg rm, A64Cond cond)
{
    emit32(a64_CSELw(rd, rn, rm, cond));
}

void JITC::asmCSINCw(NativeReg rd, NativeReg rn, NativeReg rm, A64Cond cond)
{
    emit32(a64_CSINCw(rd, rn, rm, cond));
}

void JITC::asmCSETw(NativeReg rd, A64Cond cond)
{
    // CSET Wd, cond = CSINC Wd, WZR, WZR, invert(cond)
    emit32(a64_CSINCw(rd, WZR, WZR, (A64Cond)((int)cond ^ 1)));
}

void JITC::asmUDIVw(NativeReg rd, NativeReg rn, NativeReg rm)
{
    emit32(a64_UDIVw(rd, rn, rm));
}
void JITC::asmSDIVw(NativeReg rd, NativeReg rn, NativeReg rm)
{
    emit32(a64_SDIVw(rd, rn, rm));
}
void JITC::asmLSLV(NativeReg rd, NativeReg rn, NativeReg rm)
{
    emit32(a64_LSLV(rd, rn, rm));
}
void JITC::asmLSRV(NativeReg rd, NativeReg rn, NativeReg rm)
{
    emit32(a64_LSRV(rd, rn, rm));
}
void JITC::asmUMULL(NativeReg rd, NativeReg rn, NativeReg rm)
{
    emit32(a64_UMULL(rd, rn, rm));
}
void JITC::asmORNw(NativeReg rd, NativeReg rn, NativeReg rm)
{
    emit32(a64_ORNw(rd, rn, rm));
}
void JITC::asmMVNw(NativeReg rd, NativeReg rm)
{
    emit32(a64_MVNw(rd, rm));
}
void JITC::asmCLZw(NativeReg rd, NativeReg rn)
{
    emit32(a64_CLZw(rd, rn));
}

void JITC::asmADDw_lsr(NativeReg rd, NativeReg rn, NativeReg rm, int shift)
{
    emit32(a64_ADDw_reg_lsr(rd, rn, rm, shift));
}

void JITC::asmCMNw(NativeReg rn, uint32 imm12)
{
    emit32(a64_CMNw_imm(rn, imm12));
}

void JITC::asmLSLVw(NativeReg rd, NativeReg rn, NativeReg rm)
{
    emit32(a64_LSLVw(rd, rn, rm));
}

static void jitcEmitAlign(JITC &jitc, uint align)
{
    do {
        uint missalign = ((uint64)jitc.currentPage->tcp) % align;
        if (missalign) {
            int bytes = align - missalign;
            if (jitc.currentPage->bytesLeft - bytes < 4) {
                if (jitcEmitNextFragment(jitc)) {
                    continue;
                }
            }
            // Fill with NOP instructions (4 bytes each)
            while (bytes >= 4) {
                *(uint32 *)(jitc.currentPage->tcp) = a64_NOP();
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

static void jitcFlushClientPage(ClientPage *cp)
{
    TranslationCacheFragment *tcf = cp->tcf_current;
    while (tcf) {
        __builtin___clear_cache((char *)tcf->base, (char *)(tcf->base + FRAGMENT_SIZE));
        tcf = tcf->prev;
    }
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
        if (jitc.LRUpage) {
            jitcDestroyAndTouchClientPage(jitc, jitc.LRUpage);
        }
        if (jitc.LRUpage) {
            jitcDestroyAndTouchClientPage(jitc, jitc.LRUpage);
        }
        if (jitc.LRUpage) {
            jitcDestroyAndTouchClientPage(jitc, jitc.LRUpage);
        }
        if (jitc.LRUpage) {
            jitcDestroyAndTouchClientPage(jitc, jitc.LRUpage);
        }
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
    extern byte *gTranslationCacheBase;
    if (gTranslationCacheBase &&
        ((byte *)cp->tcp < gTranslationCacheBase || (byte *)cp->tcp >= gTranslationCacheBase + 64 * 1024 * 1024)) {
        PPC_CPU_ERR("entrypoint tcp=%p outside cache for ofs=%x\n", cp->tcp, ofs);
    }
    cp->entrypoints[ofs >> 2] = cp->tcp;
}

static inline NativeAddress jitcGetEntrypoint(ClientPage *cp, uint32 ofs)
{
    return cp->entrypoints[ofs >> 2];
}

extern JITC *gJITC;
FILE *gTraceLog = NULL;
static uint64 gTraceCount = 0;

static void traceInit()
{
    // Dispatch trace disabled — too large for normal runs.
    // Uncomment to re-enable for debugging:
    // if (!gTraceLog) {
    //     gTraceLog = fopen("jitc_trace.log", "w");
    //     if (gTraceLog) setvbuf(gTraceLog, NULL, _IOFBF, 256 * 1024);
    // }
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
            jitc.asmMOV(W0, (uint32)4096);
            jitc.asmCALL_cpu(PPC_STUB_NEW_PC_REL);
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

    // Sanity check: tcp must be within translation cache
    if ((byte *)cp->tcp < jitc.translationCache || (byte *)cp->tcp >= jitc.translationCache + 64 * 1024 * 1024) {
        PPC_CPU_ERR("fragment base %p outside cache [%p, +64MB)\n", cp->tcp, jitc.translationCache);
    }

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
static uint64 jitcHits = 0, jitcNewTranslations = 0, jitcNewEntries = 0;

extern "C" NativeAddress jitcNewPC(JITC &jitc, uint32 entry)
{
    traceInit();
    // Log EA→PA mapping for kernel-range addresses
    {
        extern PPC_CPU_State *gCPU;
        uint32 ea = gCPU->pc;
        if (ea >= 0xc0000000 || entry == 0) {
            if (gTraceLog) {
                fprintf(gTraceLog, "DISPATCH ea=%08x pa=%08x msr=%08x\n", ea, entry, gCPU->msr);
                fflush(gTraceLog);
            }
        }
    }
    static uint64 total = 0;
    total++;
    if (entry >= 0x01400000 && entry < 0x02000000) {
        extern PPC_CPU_State *gCPU;
        extern byte *gMemory;
        static int kernelDisp = 0;
        kernelDisp++;
        static uint32 lastEntry = 0;
        if (kernelDisp <= 20 || (entry != lastEntry && kernelDisp <= 100)) {
            fprintf(stderr, "[JITC] kernel dispatch #%d: ea=%08x pa=%08x msr=%08x r8=%08x\n", kernelDisp, gCPU->pc,
                    entry, gCPU->msr, gCPU->gpr[8]);
        }
        // The hang loop at offset 0xcd8 within some page
        uint32 ofs = entry & 0xfff;
        if (ofs == 0xcd8 && kernelDisp <= 25) {
            uint32 r29 = gCPU->gpr[29];
            uint32 watchAddr = r29 + 0x35b4;
            uint32 val = 0;
            if (watchAddr < 0x08000000) {
                val = ppc_word_from_BE(*(uint32 *)(gMemory + watchAddr));
            }
            fprintf(stderr, "[HANG] r29=%08x [r29+35b4]=%08x (%08x) msr=%08x\n", r29, val, watchAddr, gCPU->msr);
        }
        if (entry == 0x01408360 && kernelDisp <= 25) {
            // Dump what r8 points to
            uint32 r8 = gCPU->gpr[8];
            uint32 val = 0;
            if (r8 < 0x08000000) {
                val = ppc_word_from_BE(*(uint32 *)(gMemory + r8));
            }
            fprintf(stderr, "[LOOP] r8=%08x [r8]=%08x r5=%08x r3=%08x r4=%08x\n", r8, val, gCPU->gpr[5], gCPU->gpr[3],
                    gCPU->gpr[4]);
        }
        lastEntry = entry;
    }
    if (total % 10000000 == 0) {
        fprintf(stderr, "[JITC] %llu dispatches: hits=%llu newTrans=%llu newEntry=%llu\n", total, jitcHits,
                jitcNewTranslations, jitcNewEntries);
    }
    if (gTraceLog) {
        gTraceCount++;
        extern PPC_CPU_State *gCPU;
        fprintf(gTraceLog,
                "%llu pc=%08x msr=%08x cr=%08x lr=%08x ctr=%08x "
                "r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x "
                "dec=%08x\n",
                gTraceCount, entry, gCPU->msr, gCPU->cr, gCPU->lr, gCPU->ctr, gCPU->gpr[0], gCPU->gpr[1], gCPU->gpr[2],
                gCPU->gpr[3], gCPU->gpr[4], gCPU->gpr[5], gCPU->dec);
        if (gTraceCount % 100 == 0) {
            fflush(gTraceLog);
        }
    }
    // Catch dispatch from PROM address range
    {
        extern PPC_CPU_State *gCPU;
        uint32 ccb = gCPU->current_code_base;
        uint32 pc = gCPU->pc;
        if (pc >= 0xBF000000 && pc < 0xC0000000) {
            fprintf(stderr, "[PROM-DISPATCH] pc=%08x pa=%08x msr=%08x lr=%08x ccb=%08x\n", pc, entry, gCPU->msr,
                    gCPU->lr, ccb);
            static int promCount = 0;
            if (++promCount >= 3) {
                if (gTraceLog) {
                    fflush(gTraceLog);
                }
                PPC_CPU_ERR("repeated PROM dispatch: pc=%08x pa=%08x msr=%08x\n", pc, entry, gCPU->msr);
            }
        }
    }
    // Watch for CPU state corruption (aarch64 instructions in PPC registers)
    {
        extern PPC_CPU_State *gCPU;
        // Check multiple fields for aarch64 instruction patterns
        bool corrupt = false;
        const char *field = "";
        uint32 val = 0;
        // gpr[9]==0xFD69 check removed — was a false positive (normal user-mode value)
        // Check if msr looks like an aarch64 instruction (top byte B9/AA/D6/F2/52)
        uint8 msr_top = gCPU->msr >> 24;
        if (msr_top == 0xB9 || msr_top == 0xAA || msr_top == 0xD6 || msr_top == 0xF2 || msr_top == 0x52) {
            corrupt = true;
            field = "msr";
            val = gCPU->msr;
        }
        // Check if current_code_base looks like MOV X0, X20 (0xAA1403E0)
        if (gCPU->current_code_base == 0xAA1403E0) {
            corrupt = true;
            field = "ccb";
            val = gCPU->current_code_base;
        }
        if (corrupt) {
            fprintf(stderr, "[CORRUPT] %s=%08x at dispatch pa=%08x\n", field, val, entry);
            fprintf(stderr, "  pc=%08x npc=%08x ccb=%08x msr=%08x lr=%08x opc=%08x\n", gCPU->pc, gCPU->npc,
                    gCPU->current_code_base, gCPU->msr, gCPU->lr, gCPU->current_opc);
            fprintf(stderr, "  r0=%08x r1=%08x r9=%08x r10=%08x temp=%08x\n", gCPU->gpr[0], gCPU->gpr[1], gCPU->gpr[9],
                    gCPU->gpr[10], gCPU->temp);
            if (gTraceLog) {
                fflush(gTraceLog);
            }
            PPC_CPU_ERR("CPU state corrupt: %s=%08x at dispatch pa=%08x\n", field, val, entry);
        }
    }
    if (entry > gMemorySize) {
        PPC_CPU_ERR("entry not physical: %08x (memsize=%08x)\n", entry, gMemorySize);
    }
    uint32 baseaddr = entry & 0xfffff000;
    ClientPage *cp = jitcGetOrCreateClientPage(jitc, baseaddr);
    jitcTouchClientPage(jitc, cp);

    NativeAddress result;
    if (!cp->tcf_current) {
        /* First translation for this page */
        jitcNewTranslations++;
        result = jitcStartTranslation(jitc, cp, baseaddr, entry & 0xfff);
        // Flush icache for all fragments used by this page
        jitcFlushClientPage(cp);
        pthread_jit_write_protect_np(1);
    } else {
        NativeAddress ofs = jitcGetEntrypoint(cp, entry & 0xfff);
        if (ofs) {
            /* Cache hit — already translated, no flush needed */
            jitcHits++;
            result = ofs;
        } else {
            /* New entrypoint on existing page */
            jitcNewEntries++;
            pthread_jit_write_protect_np(0);
            result = jitcNewEntrypoint(jitc, cp, baseaddr, entry & 0xfff);
            jitcFlushClientPage(cp);
            pthread_jit_write_protect_np(1);
        }
    }

    // Sanity check: result must be in the translation cache
    if ((byte *)result < jitc.translationCache || (byte *)result >= jitc.translationCache + 64 * 1024 * 1024) {
        PPC_CPU_ERR("jitcNewPC returning %p outside cache [%p, +64MB) for PA %08x\n", result, jitc.translationCache,
                    entry);
    }

    return result;
}

extern "C" void jitc_error_bad_native_address()
{
    extern PPC_CPU_State *gCPU;
    PPC_CPU_ERR("jitcNewPC returned a guest address, not native code! "
                "pc=%08x msr=%08x ccb=%08x pc_ofs=%08x\n",
                gCPU->pc, gCPU->msr, gCPU->current_code_base, gCPU->pc_ofs);
}

extern "C" void jitc_error_bad_entrypoint(uint64 addr)
{
    extern PPC_CPU_State *gCPU;
    PPC_CPU_ERR("entrypoint %p is not native code! "
                "pc=%08x msr=%08x ccb=%08x pc_ofs=%08x\n",
                (void *)addr, gCPU->pc, gCPU->msr, gCPU->current_code_base, gCPU->pc_ofs);
}

extern "C" void jitc_error_msr_unsupported_bits(uint32 a)
{
    PPC_CPU_ERR("unsupported MSR bits: %08x\n", a);
}

extern "C" void jitc_error(const char *error)
{
    PPC_CPU_ERR("%s\n", error);
}

extern "C" void FASTCALL jitc_error_singlestep()
{
    PPC_CPU_ERR("singlestep not supported yet\n");
}

extern "C" void FASTCALL jitc_error_unknown_exception()
{
    PPC_CPU_ERR("unknown exception signaled\n");
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
    PPC_CPU_ERR("stack is not aligned\n");
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
    extern byte *gTranslationCacheBase;
    gTranslationCacheBase = translationCache;

    ht_printf("translation cache: %p (aarch64 JIT)\n", translationCache);

    if (!translationCache) {
        return false;
    }
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
    if (translationCache) {
        sys_free_read_write_execute(translationCache);
    }
}
