/*
 *	The HT Editor
 *	asm.h
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

#ifndef __ASM_H__
#define __ASM_H__

#include "tools/data.h"

#define CPU_X86	1

#define MAX_INSN_SIZE	16

struct CPU_ADDR {
	union {
		struct {
			uint16 seg;
			uint32 offset;
		} addr32;
		struct {
			uint64 addr;
		} flat64;
	};
};

struct asm_code {
	asm_code *next;
	uint32 size;
	byte data[MAX_INSN_SIZE];
	void *context;
};

typedef void dis_insn;
typedef void asm_insn;

/*
 *	CLASS assembler
 */

class Assembler: public Object {
protected:
	int (*imm_eval_proc)(void *context, char **s, uint32 *v);
	void *imm_eval_context;
	
	asm_code *codes;
	asm_code code;
	char error_msg[256];
	bool error;
	int options;
	bool bigendian;

			void emitbyte(byte b);
			void emitword(uint16 w);
			void emitdword(uint32 d);
			void free_asm_codes();
			void deletecode(asm_code *c);
			void clearcode();
			void newcode();
			void pushcode();
public:
			Assembler(bool bigendian);
			~Assembler();
/* new */
	virtual	asm_insn *alloc_insn();
	virtual	asm_code *encode(asm_insn *asm_insn, int options, CPU_ADDR cur_address);
			char *get_error_msg();
	virtual	char *get_name();
	virtual	int translate_str(asm_insn *asm_insn, const char *s);
			void set_error_msg(char *format, ...);
			void set_imm_eval_proc(int (*imm_eval_proc)(void *context, char **s, uint32 *v), void *imm_eval_context);
			asm_code *shortest(asm_code *codes);
};

/*
 *	CLASS disassembler
 */

/* generic disassembler styles */
#define DIS_STYLE_HIGHLIGHT			0x80000000		/* create highlighting information in strf() */
#define DIS_STYLE_HEX_CSTYLE			0x40000000		/* IF SET: mov eax, 0x12345678 		ELSE: mov eax, 12345678 */
#define DIS_STYLE_HEX_ASMSTYLE		0x20000000		/* IF SET: mov eax, 12345678h 		ELSE: mov eax, 12345678 */
#define DIS_STYLE_HEX_UPPERCASE		0x10000000		/* IF SET: mov eax, 5678ABCD	 		ELSE: mov eax, 5678abcd */
#define DIS_STYLE_HEX_NOZEROPAD		0x08000000		/* IF SET: mov eax, 8002344	 		ELSE: mov eax, 008002344 */
#define DIS_STYLE_SIGNED				0x04000000		/* IF SET: mov eax, -1	 			ELSE: mov eax, 0ffffffffh */

#define DIS_STYLE_TABSIZE			8

extern char* (*addr_sym_func)(CPU_ADDR addr, int *symstrlen, void *context);
extern void* addr_sym_func_context;

enum AsmSyntaxHighlightEnum {
	e_cs_default=0,
	e_cs_comment,
	e_cs_number,
	e_cs_symbol,
	e_cs_string
};

class Disassembler: public Object {
protected:
	int options;
	bool highlight;
	
			const char *get_cs(AsmSyntaxHighlightEnum style);
			void hexd(char **s, int size, int options, int imm);
			void enable_highlighting();
			void disable_highlighting();
public:
			Disassembler();
			~Disassembler();
/* new */
	virtual	dis_insn *createInvalidInsn();
	virtual	dis_insn *decode(const byte *code, int maxlen, CPU_ADDR cur_address)=0;
	virtual	dis_insn *duplicateInsn(dis_insn *disasm_insn)=0;
	virtual	void	getOpcodeMetrics(int &min_length, int &max_length, int &min_look_ahead, int &avg_look_ahead, int &addr_align)=0;
	virtual	byte getSize(dis_insn *disasm_insn)=0;
	virtual	char *getName()=0;
	virtual	bool selectNext(dis_insn *disasm_insn);
	virtual	char *str(dis_insn *disasm_insn, int style);
	virtual	char *strf(dis_insn *disasm_insn, int style, char *format)=0;
	virtual	bool validInsn(dis_insn *disasm_insn)=0;
};

/*****************************************************************************
 *	The strf() format                                                       *
 *****************************************************************************
	String	Action
    --------------------------------------------------
	%x		substitute expression with symbol "x"
	?xy...y	if symbol "x" is undefined leave out the whole expression,
			otherwise subsitute expression with string between the two "y"s

	Symbol	Desc
    --------------------------------------------------
	p 		prefix
	n 		name
	1 		first operand
	2 		second operand
	3 		third operand
*/

#define DISASM_STRF_VAR			'%'
#define DISASM_STRF_COND			'?'

#define DISASM_STRF_PREFIX		'p'
#define DISASM_STRF_NAME			'n'
#define DISASM_STRF_FIRST		'1'
#define DISASM_STRF_SECOND		'2'
#define DISASM_STRF_THIRD		'3'

#define DISASM_STRF_DEFAULT_FORMAT	"?p#%p #%n\t%1?2#, %2?3/, %3/#"
#define DISASM_STRF_SMALL_FORMAT	"?p#%p #%n?1- %1?2#,%2?3/,%3/#-"

#define ATOM_DISASM_X86 MAGICD("DIS\x01")
#define ATOM_DISASM_ALPHA MAGICD("DIS\x02")
#define ATOM_DISASM_JAVA MAGICD("DIS\x03")
#define ATOM_DISASM_IA64 MAGICD("DIS\x04")
#define ATOM_DISASM_IL MAGICD("DIS\x05")
#define ATOM_DISASM_X86_VXD MAGICD("DIS\x06")

#define ASM_SYNTAX_DEFAULT "\\@d"
#define ASM_SYNTAX_COMMENT "\\@#"
#define ASM_SYNTAX_NUMBER "\\@n"
#define ASM_SYNTAX_SYMBOL "\\@c"
#define ASM_SYNTAX_STRING "\\@s"

bool init_asm();
void done_asm();

#endif /* __ASM_H__ */
