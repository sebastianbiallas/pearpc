/* 
 *	HT Editor
 *	asm.cc
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf
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
#include <cstdio>
#include <cstdarg>

#include "asm.h"
#include "tools/data.h"
#include "system/types.h"
#include "tools/atom.h"
#include "tools/debug.h"

/* FIX8
#include "alphadis.h"
#include "ia64dis.h"
#include "ildis.h"
#include "javadis.h"
#include "x86dis.h"
*/

/*
 *	CLASS Assembler
 */

Assembler::Assembler(bool b)
{
	codes = 0;
	bigendian = b;
}

Assembler::~Assembler()
{
}

asm_insn *Assembler::alloc_insn()
{
	return NULL;
}

void Assembler::deletecode(asm_code *code)
{
	asm_code **p=&codes, *c=codes;
	while (c) {
		if (c==code) {
			*p=c->next;
			delete c;
			return;
		}
		c=c->next;
		p=&(*p)->next;
	}
}

asm_code *Assembler::encode(asm_insn *asm_insn, int _options, CPU_ADDR cur_address)
{
	free_asm_codes();
	error=0;
	options=_options;
	return 0;
}

void Assembler::clearcode()
{
	code.size=0;
}

void Assembler::emitbyte(byte b)
{
	code.data[code.size] = b;
	code.size++;
}

void Assembler::emitword(uint16 w)
{
	if (bigendian) {
		code.data[code.size+1] = (byte)w;
		code.data[code.size+0] = (byte)(w>>8);
	} else {
		code.data[code.size+0] = (byte)w;
		code.data[code.size+1] = (byte)(w>>8);
	}
	code.size += 2;
}

void Assembler::emitdword(uint32 d)
{
	if (bigendian) {
		code.data[code.size+3] = (byte)d;
		code.data[code.size+2] = (byte)(d>>8);
		code.data[code.size+1] = (byte)(d>>16);
		code.data[code.size+0] = (byte)(d>>24);
	} else {
		code.data[code.size+0] = (byte)d;
		code.data[code.size+1] = (byte)(d>>8);
		code.data[code.size+2] = (byte)(d>>16);
		code.data[code.size+3] = (byte)(d>>24);
	}
	code.size += 4;
}

void Assembler::free_asm_codes()
{
	while (codes) {
		asm_code *t=codes->next;
		delete codes;
		codes=t;
	}
}

char *Assembler::get_error_msg()
{
	return error_msg;
}

char *Assembler::get_name()
{
	return "generic asm";
}

void Assembler::newcode()
{
	code.size=0;
}

asm_code *Assembler::shortest(asm_code *codes)
{
	asm_code *best=0;
	uint32 bestv=0xffffffff;
	while (codes) {
		if (codes->size<bestv) {
			best=codes;
			bestv=codes->size;
		}
		codes=codes->next;
	};
	return best;
}

void Assembler::pushcode()
{
	asm_code **t=&codes;
	while (*t) {
		t=&(*t)->next;
	}
	*t=new asm_code;

	memmove(*t, &code, sizeof code);
	(*t)->next=0;
}

int Assembler::translate_str(asm_insn *asm_insn, const char *s)
{
	return 0;
}

void Assembler::set_error_msg(char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	vsprintf(error_msg, format, arg);
	va_end(arg);
	error=1;
}

void Assembler::set_imm_eval_proc(int (*p)(void *context, char **s, uint32 *v), void *c)
{
	imm_eval_proc=p;
	imm_eval_context=c;
}

/*
 *	CLASS disassembler
 */

Disassembler::Disassembler()
{
	disable_highlighting();
}

Disassembler::~Disassembler()
{
}

char* (*addr_sym_func)(CPU_ADDR addr, int *symstrlen, void *context) = NULL;
void* addr_sym_func_context = NULL;

dis_insn *Disassembler::createInvalidInsn()
{
	return NULL;
}

void Disassembler::hexd(char **s, int size, int options, int imm)
{
	char ff[16];
	char *f = (char*)&ff;
	char *t = *s;
	*f++ = '%';
	if ((imm>=0) && (imm<=9)) {
		*s += sprintf(*s, "%d", imm);
	} else if (options & DIS_STYLE_SIGNED) {
		if (!(options & DIS_STYLE_HEX_NOZEROPAD)) f += sprintf(f, "0%d", size);
		*f++ = 'd';
		*f = 0;
		*s += sprintf(*s, ff, imm);
	} else {
		if (options & DIS_STYLE_HEX_CSTYLE) *f++ = '#';
		if (!(options & DIS_STYLE_HEX_NOZEROPAD)) f += sprintf(f, "0%d", size);
		if (options & DIS_STYLE_HEX_UPPERCASE) *f++ = 'X'; else
			*f++ = 'x';
		if (options & DIS_STYLE_HEX_ASMSTYLE) *f++ = 'h';
		*f = 0;
		*s += sprintf(*s, ff, imm);
		if ((options & DIS_STYLE_HEX_NOZEROPAD) && (*t-'0'>9)) {
			memmove(t+1, t, strlen(t)+1);
			*t = '0';
			(*s)++;
		}
	}
}

bool Disassembler::selectNext(dis_insn *disasm_insn)
{
	return false;
}

char *Disassembler::str(dis_insn *disasm_insn, int style)
{
	return strf(disasm_insn, style, DISASM_STRF_DEFAULT_FORMAT);
}

const char *Disassembler::get_cs(AsmSyntaxHighlightEnum style)
{
	const char *highlights[] = {
		ASM_SYNTAX_DEFAULT,
		ASM_SYNTAX_COMMENT,
		ASM_SYNTAX_NUMBER,
		ASM_SYNTAX_SYMBOL,
		ASM_SYNTAX_STRING
	};
	return (highlight) ? highlights[(int)style] : "";
}

void Disassembler::enable_highlighting()
{
	highlight = true;
}

void Disassembler::disable_highlighting()
{
	highlight = false;
}

/* FIX8
BUILDER(ATOM_DISASM_X86, x86dis)
BUILDER(ATOM_DISASM_X86_VXD, x86dis_vxd)
BUILDER(ATOM_DISASM_ALPHA, Alphadis)
BUILDER(ATOM_DISASM_JAVA, javadis)
BUILDER(ATOM_DISASM_IA64, IA64Disassembler)
*/

bool init_asm()
{
/* FIX8
	REGISTER(ATOM_DISASM_X86, x86dis)
	REGISTER(ATOM_DISASM_X86_VXD, x86dis_vxd)
	REGISTER(ATOM_DISASM_ALPHA, Alphadis)
	REGISTER(ATOM_DISASM_JAVA, javadis)
	REGISTER(ATOM_DISASM_IA64, IA64Disassembler)
*/
	return true;
}

void done_asm()
{
/* FIX8
	UNREGISTER(ATOM_DISASM_IA64, IA64Disassembler)
	UNREGISTER(ATOM_DISASM_JAVA, javadis)
	UNREGISTER(ATOM_DISASM_ALPHA, Alphadis)
	UNREGISTER(ATOM_DISASM_X86_VXD, x86dis_vxd)
	UNREGISTER(ATOM_DISASM_X86, x86dis)
*/
}

