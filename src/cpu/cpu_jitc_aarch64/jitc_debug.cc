/*
 *  PearPC
 *  jitc_debug.cc
 *
 *  Copyright (C) 2004 Sebastian Biallas (sb@biallas.net)
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

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "tools/data.h"
#include "tools/str.h"
#include "tools/endianess.h"

#include "debug/ppcdis.h"
#include "tools/snprintf.h"
#include "jitc.h"
#include "jitc_asm.h"
#include "jitc_debug.h"

#ifdef JITC_DEBUG

#include "io/prom/promosi.h"

static FILE *gDebugLog;
static AVLTree *symbols;

inline static void disasmPPC(uint32 code, uint32 ea, char *result)
{
    PPCDisassembler dis(PPC_MODE_32);
    CPU_ADDR addr;
    addr.addr32.offset = ea;
    addr_sym_func = NULL;
    byte code_buf[4];
    createForeignInt(code_buf, code, 4, big_endian);
    strcpy(result, dis.str(dis.decode(code_buf, 4, addr), 0));
}

void jitcDebugLogAdd(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    ht_vfprintf(gDebugLog, fmt, ap);
    va_end(ap);
    fflush(gDebugLog);
}

void jitcDebugLogNewInstruction(JITC &jitc)
{
    char str[128];
    disasmPPC(jitc.current_opc, jitc.pc, str);
    jitcDebugLogAdd("%08x   %08x  %s\n", jitc.pc, jitc.current_opc, str);
}

void jitcDebugLogEmit(JITC &jitc, const byte *insn, int size)
{
    // Log the address and raw instruction word(s)
    for (int ofs = 0; ofs < size; ofs += 4) {
        uint32 word = *(uint32 *)(insn + ofs);
        jitcDebugLogAdd("  %p  %08x\n", (byte *)jitc.currentPage->tcp + ofs, word);
    }
}

void jitcDebugInit()
{
    gDebugLog = fopen("jitc.log", "w");
    symbols = new AVLTree(true);
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_write_effective_byte_asm)), new String("ppc_write_effective_byte_asm")));
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_write_effective_half_asm)), new String("ppc_write_effective_half_asm")));
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_write_effective_word_asm)), new String("ppc_write_effective_word_asm")));
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_write_effective_dword_asm)), new String("ppc_write_effective_dword_asm")));
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_read_effective_byte_asm)), new String("ppc_read_effective_byte_asm")));
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_read_effective_half_z_asm)), new String("ppc_read_effective_half_z_asm")));
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_read_effective_half_s_asm)), new String("ppc_read_effective_half_s_asm")));
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_read_effective_word_asm)), new String("ppc_read_effective_word_asm")));
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_read_effective_dword_asm)), new String("ppc_read_effective_dword_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&ppc_opc_icbi_asm)), new String("ppc_opc_icbi_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&ppc_opc_stswi_asm)), new String("ppc_opc_stswi_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&ppc_opc_lswi_asm)), new String("ppc_opc_lswi_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&ppc_isi_exception_asm)), new String("ppc_isi_exception_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&ppc_dsi_exception_asm)), new String("ppc_dsi_exception_asm")));
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_program_exception_asm)), new String("ppc_program_exception_asm")));
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_no_fpu_exception_asm)), new String("ppc_no_fpu_exception_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&ppc_sc_exception_asm)), new String("ppc_sc_exception_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&ppc_flush_flags_asm)), new String("ppc_flush_flags_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&ppc_new_pc_asm)), new String("ppc_new_pc_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&ppc_new_pc_rel_asm)), new String("ppc_new_pc_rel_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&ppc_set_msr_asm)), new String("ppc_set_msr_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&ppc_mmu_tlb_invalidate_all_asm)),
                                 new String("ppc_mmu_tlb_invalidate_all_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&ppc_start_jitc_asm)), new String("ppc_start_jitc_asm")));
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_new_pc_this_page_asm)), new String("ppc_new_pc_this_page_asm")));
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_heartbeat_ext_rel_asm)), new String("ppc_heartbeat_ext_rel_asm")));
    symbols->insert(
        new KeyValue(new UInt64(uint64(&ppc_flush_flags_signed_0_asm)), new String("ppc_flush_flags_signed_0_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&ppc_flush_flags_unsigned_0_asm)),
                                 new String("ppc_flush_flags_unsigned_0_asm")));
    symbols->insert(new KeyValue(new UInt64(uint64(&call_prom_osi)), new String("call_prom_osi")));
}

void jitcDebugDone()
{
    fclose(gDebugLog);
}

#endif
