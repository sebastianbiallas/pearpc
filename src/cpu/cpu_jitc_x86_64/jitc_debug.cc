/*
 *	PearPC
 *	jitc_debug.h
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

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "tools/data.h"
#include "tools/str.h"
#include "tools/endianess.h"

#include "debug/x86dis.h"
#include "debug/ppcdis.h"
#include "tools/snprintf.h"
#include "jitc.h"
#include "jitc_asm.h"
#include "jitc_debug.h"

#ifdef JITC_DEBUG

#include "io/prom/promosi.h"

static FILE *gDebugLog;
static AVLTree *symbols;

static char *symbol_lookup(CPU_ADDR addr, int *symstrlen, void *context)
{
/*	foreach(KeyValue, kv, *symbols, {
		ht_printf("lookup %y -> %y\n", kv->mKey, kv->mValue);
	});*/
	
	KeyValue tmp(new UInt64(addr.flat64.addr), NULL);
	
	ObjHandle oh = symbols->find(&tmp);
//		return NULL;
	if (oh != InvObjHandle) {
		KeyValue *kv = (KeyValue*)symbols->get(oh);
//		ht_printf("lookup '%y'\n", kv->mValue);
		if (symstrlen) *symstrlen = ((String *)kv->mValue)->length();
		return ((String *)kv->mValue)->contentChar();
	} else {
//		ht_printf("lookup %08x failed\n", addr.addr32.offset);
//		if (symstrlen) *symstrlen = 1;
		return NULL;
	}
}

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

inline static int disasmX86(const byte *code, uint64 ea, char *result)
{
	x86_64dis dis;
	CPU_ADDR addr;
	addr.flat64.addr = ea;
	addr_sym_func = symbol_lookup;
	dis_insn *ret = dis.decode(code, 15, addr);
	strcpy(result, dis.str(ret, DIS_STYLE_HEX_NOZEROPAD));
	return dis.getSize(ret);
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
	char str[128];
	int size1 = disasmX86(insn, uint64(jitc.currentPage->tcp), str);
	if (size != size1) jitcDebugLogAdd(" kaputt! ");
	jitcDebugLogAdd("  ");
	jitcDebugLogAdd("%p ", jitc.currentPage->tcp);
	for (int i=0; i < 15; i++) {
		if (i < size) {
			jitcDebugLogAdd("%02x", insn[i]);
		} else {
			jitcDebugLogAdd("  ");
		}
	}
	jitcDebugLogAdd("  %s\n", str);
}
                                                  
void jitcDebugInit()
{
//	gDebugLog = stdout;
	gDebugLog = fopen("jitc.log", "w");
	symbols = new AVLTree(true);
#if 0	
	for (int i=0; i<32; i++) {
		String *s = new String();
		s->assignFormat("r%d", i);
		symbols->insert(new KeyValue(new UInt((uint)&gCPU.gpr[i]), s));
	}
	for (int i=0; i<32; i++) {
		String *s = new String();
		s->assignFormat("fr%d (lower)", i);
		symbols->insert(new KeyValue(new UInt((uint)&gCPU.fpr[i]), s));
		s = new String();
		s->assignFormat("fr%d (upper)", i);
		symbols->insert(new KeyValue(new UInt((uint)&gCPU.fpr[i]+4), s));
	}
	for (int i=0; i<32; i++) {
		String *s;
		for (int j=0; j<4; j++) {
			s = new String();
			s->assignFormat("vr%d (%d)", i, j);
			symbols->insert(new KeyValue(new UInt((uint)&gCPU.vr[i]+4*j), s));
		}
	}
	for (int i=0; i<4; i++) {
		String *s = new String();
		s->assignFormat("ibatl%d", i);
		symbols->insert(new KeyValue(new UInt((uint)&gCPU.ibatl[i]), s));
		s = new String();
		s->assignFormat("ibatu%d", i);
		symbols->insert(new KeyValue(new UInt((uint)&gCPU.ibatu[i]), s));
		s = new String();
		s->assignFormat("ibat_bl17_%d", i);
		symbols->insert(new KeyValue(new UInt((uint)&gCPU.ibat_bl17[i]), s));
		s = new String();
		s->assignFormat("dbatl%d", i);
		symbols->insert(new KeyValue(new UInt((uint)&gCPU.dbatl[i]), s));
		s = new String();
		s->assignFormat("dbatu%d", i);
		symbols->insert(new KeyValue(new UInt((uint)&gCPU.dbatu[i]), s));
		s = new String();
		s->assignFormat("dbat_bl17_%d", i);
		symbols->insert(new KeyValue(new UInt((uint)&gCPU.dbat_bl17[i]), s));
	}
	for (int i=0; i<4; i++) {
		String *s = new String();
		s->assignFormat("sprg%d", i);
		symbols->insert(new KeyValue(new UInt((uint)&gCPU.sprg[i]), s));
	}
	for (int i=0; i<16; i++) {
		String *s = new String();
		s->assignFormat("hid%d", i);
		symbols->insert(new KeyValue(new UInt((uint)&gCPU.hid[i]), s));
	}
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.ctr), new String("ctr")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.lr), new String("lr")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.cr), new String("cr")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.cr+1), new String("cr+1")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.cr+2), new String("cr+2")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.cr+3), new String("cr+3")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.xer), new String("xer")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.xer_ca), new String("xer_ca")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.xer+3), new String("xer+3")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.msr), new String("msr")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.srr[0]), new String("srr0")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.srr[1]), new String("srr1")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.dsisr), new String("dsisr")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.dar), new String("dar")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.dec), new String("dec")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.pvr), new String("pvr")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.vrsave), new String("vrsave")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.reserve), new String("reserve")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.temp), new String("tmp")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.vtemp), new String("vtmp")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.current_code_base), new String("current_code_base")));
	symbols->insert(new KeyValue(new UInt((uint)&gCPU.current_opc), new String("current_opc")));
#endif
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_write_effective_byte_asm)), new String("ppc_write_effective_byte_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_write_effective_half_asm)), new String("ppc_write_effective_half_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_write_effective_word_asm)), new String("ppc_write_effective_word_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_write_effective_dword_asm)), new String("ppc_write_effective_dword_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_read_effective_byte_asm)), new String("ppc_read_effective_byte_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_read_effective_half_z_asm)), new String("ppc_read_effective_half_z_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_read_effective_half_s_asm)), new String("ppc_read_effective_half_s_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_read_effective_word_asm)), new String("ppc_read_effective_word_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_read_effective_dword_asm)), new String("ppc_read_effective_dword_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_opc_icbi_asm)), new String("ppc_opc_icbi_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_opc_stswi_asm)), new String("ppc_opc_stswi_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_opc_lswi_asm)), new String("ppc_opc_lswi_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_isi_exception_asm)), new String("ppc_isi_exception_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_dsi_exception_asm)), new String("ppc_dsi_exception_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_program_exception_asm)), new String("ppc_program_exception_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_no_fpu_exception_asm)), new String("ppc_no_fpu_exception_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_sc_exception_asm)), new String("ppc_sc_exception_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_flush_flags_asm)), new String("ppc_flush_flags_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_new_pc_asm)), new String("ppc_new_pc_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_new_pc_rel_asm)), new String("ppc_new_pc_rel_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_set_msr_asm)), new String("ppc_set_msr_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_mmu_tlb_invalidate_all_asm)), new String("ppc_mmu_tlb_invalidate_all_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_start_jitc_asm)), new String("ppc_start_jitc_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_new_pc_this_page_asm)), new String("ppc_new_pc_this_page_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_heartbeat_ext_rel_asm)), new String("ppc_heartbeat_ext_rel_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_flush_flags_signed_0_asm)), new String("ppc_flush_flags_signed_0_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_flush_flags_unsigned_0_asm)), new String("ppc_flush_flags_unsigned_0_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_flush_flags_signed_odd_asm)), new String("ppc_flush_flags_signed_odd_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&ppc_flush_flags_signed_even_asm)), new String("ppc_flush_flags_signed_even_asm")));
	symbols->insert(new KeyValue(new UInt64(uint64(&call_prom_osi)), new String("call_prom_osi")));
}

void jitcDebugDone()
{
	fclose(gDebugLog);
}

void jitcDebugAddJumpTableEntry(NativeAddress to, NativeAddress entry)
{
	CPU_ADDR addr;
	addr.flat64.addr = (uint64)to;

	String *entryName = new String();
	
	if (const char *sym = symbol_lookup(addr, NULL, NULL)) {
		entryName->assignFormat("jump_table[%s]", sym);
	} else {
		entryName->assignFormat("jump_table[%p]", to);
	}
	
	symbols->insert(new KeyValue(new UInt64(uint64(entry)), entryName));
}

#endif
