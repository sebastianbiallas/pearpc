/* 
 *	HT Editor
 *	x86dis.h
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

#ifndef __X86DIS_H__
#define __X86DIS_H__

#include "asm.h"
#include "x86opc.h"
#include "tools/data.h"

#define word uint16

#define X86DIS_OPCODE_CLASS_STD		0		/* no prefix */
#define X86DIS_OPCODE_CLASS_EXT		1		/* 0F */
#define X86DIS_OPCODE_CLASS_EXTEXT		2		/* 0F0F */

/* x86-specific styles */
#define X86DIS_STYLE_EXPLICIT_MEMSIZE	0x00000001		/* IF SET: mov word ptr [0000], ax 	ELSE: mov [0000], ax */
#define X86DIS_STYLE_OPTIMIZE_ADDR		0x00000002		/* IF SET: mov [eax*3], ax 			ELSE: mov [eax+eax*2+00000000], ax */
/*#define X86DIS_STYLE_USE16			0x00000004
#define X86DIS_STYLE_USE32			0x00000008*/

struct x86dis_insn {
	bool invalid;
	sint8 lockprefix;
	sint8 repprefix;
	sint8 segprefix;
	byte size;
	int opcode;
	int opcodeclass;
	int eopsize;
	int eaddrsize;
	char *name;
	x86_insn_op op[3];
};

/*
 *	CLASS x86dis
 */

class X86Disassembler: public Disassembler {
public:
	int opsize, addrsize;
protected:
	x86dis_insn insn;
	char insnstr[256];
/* initme! */
	const byte *codep, *ocodep;
	int seg;
	int addr; // FIXME: int??
	byte c;
	int modrm;
	int sib;
	int maxlen;

/* new */
			void decode_insn(x86opc_insn *insn);
			void decode_modrm(x86_insn_op *op, char size, int allow_reg, int allow_mem, int mmx);
			void decode_op(x86_insn_op *op, x86opc_insn_op *xop);
			void decode_sib(x86_insn_op *op, int mod);
			int esizeaddr(char c);
			int esizeop(char c);
			byte getbyte();
			word getword();
			dword getdword();
			int getmodrm();
			int getsib();
			void invalidate();
			int isfloat(char c);
			void prefixes();
			int special_param_ambiguity(x86dis_insn *disasm_insn);
			void str_format(char **str, char **format, char *p, char *n, char *op[3], int oplen[3], char stopchar, int print);
	virtual	void str_op(char *opstr, int *opstrlen, x86dis_insn *insn, x86_insn_op *op, bool explicit_params);
public:
	X86Disassembler();
	X86Disassembler(int opsize, int addrsize);
	virtual ~X86Disassembler();

/* overwritten */
	virtual dis_insn *decode(const byte *code, int maxlen, CPU_ADDR addr);
	virtual dis_insn *duplicateInsn(dis_insn *disasm_insn);
	virtual void getOpcodeMetrics(int &min_length, int &max_length, int &min_look_ahead, int &avg_look_ahead, int &addr_align);
	virtual char *getName();
	virtual byte getSize(dis_insn *disasm_insn);
	virtual char *str(dis_insn *disasm_insn, int options);
	virtual char *strf(dis_insn *disasm_insn, int options, char *format);
	virtual bool validInsn(dis_insn *disasm_insn);
};


#endif /* __X86DIS_H__ */
