/* 
 *	HT Editor
 *	x86opc.h
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

#ifndef __X86OPC_H__
#define __X86OPC_H__

#include "system/types.h"

#define X86_PREFIX_NO		-1

#define X86_PREFIX_LOCK		0	/* f0 */

#define X86_PREFIX_ES		0	/* 26 */
#define X86_PREFIX_CS		1	/* 2e */
#define X86_PREFIX_SS		2	/* 36 */
#define X86_PREFIX_DS		3	/* 3e */
#define X86_PREFIX_FS		4	/* 64 */
#define X86_PREFIX_GS		5	/* 65 */

#define X86_PREFIX_REPNZ		0	/* f2 */
#define X86_PREFIX_REPZ		1	/* f3 */

#define X86_OPSIZEUNKNOWN	-1
#define X86_OPSIZE16		0
#define X86_OPSIZE32		1

#define X86_ADDRSIZEUNKNOWN	-1
#define X86_ADDRSIZE16		0
#define X86_ADDRSIZE32		1

#define X86_OPTYPE_EMPTY		0
#define X86_OPTYPE_IMM		1
#define X86_OPTYPE_REG		2
#define X86_OPTYPE_SEG		3
#define X86_OPTYPE_MEM		4
#define X86_OPTYPE_CRX		5
#define X86_OPTYPE_DRX		6
#define X86_OPTYPE_TRX		7
#define X86_OPTYPE_STX		8
#define X86_OPTYPE_MMX		9
#define X86_OPTYPE_FARPTR	10

// user defined types start here
#define X86_OPTYPE_USER		32

union int_or_ptr {
	int i;
	void *p;
};

#define dword uint32

struct x86_insn_op {
	int type;
	int size;
	union {
		struct {
			dword seg;
			dword offset;
		} farptr;			
		dword imm;
		int reg;
		int seg;
		struct {
			int floatptr;
			int hasdisp;
			dword disp;
			int base;
			int index;
			int scale;
			int addrsize;
		} mem;
		int crx;
		int drx;
		int trx;
		int stx;
		int mmx;
		int_or_ptr user[4];
	};
};

#define TYPE_0			0
#define TYPE_A			1		/* direct address without ModR/M (generally
							   like imm, but can be 16:32 = 48 bit) */
#define TYPE_C			2		/* reg of ModR/M picks control register */
#define TYPE_D			3		/* reg of ModR/M picks debug register */
#define TYPE_E			4		/* ModR/M (general reg or memory) */
#define TYPE_G			5		/* reg of ModR/M picks general register */
#define TYPE_Is		6		/* signed immediate */
#define TYPE_I			7		/* unsigned immediate */
#define TYPE_Ix		8		/* fixed immediate */
#define TYPE_J			9		/* relative branch offset */
#define TYPE_M			10		/* ModR/M (memory only) */
#define TYPE_O			11		/* direct memory without ModR/M */
#define TYPE_P			12		/* reg of ModR/M picks MMX register */
#define TYPE_Q			13		/* ModR/M (MMX reg or memory) */
#define TYPE_R			14		/* rm of ModR/M picks general register */
#define TYPE_Rx		15		/* extra picks register */
#define TYPE_S			16		/* reg of ModR/M picks segment register */
#define TYPE_Sx		17		/* extra picks segment register */
#define TYPE_T			18		/* reg of ModR/M picks test register */
#define TYPE_F			19		/* r/m of ModR/M picks a fpu register */
#define TYPE_Fx		20		/* extra picks a fpu register */

/* when name is == 0, the first op has a special meaning (layout see x86_insn_op_special) */
#define SPECIAL_TYPE_INVALID		0
#define SPECIAL_TYPE_PREFIX 		1
#define SPECIAL_TYPE_GROUP 		2
#define SPECIAL_TYPE_FGROUP 		3

#define SIZE_0			'0'		/* size unimportant */
#define SIZE_B			'b'		/* byte */
#define SIZE_W			'w'		/* word */
#define SIZE_D			'd'		/* dword */
#define SIZE_Q			'q'		/* qword */
#define SIZE_C			'c'		/* byte OR word */
#define SIZE_V			'v'		/* word OR dword */
#define SIZE_P			'p'		/* word:word OR word:dword, memory only ! */
#define SIZE_S			's'		/* short/single real (32-bit) */
#define SIZE_L			'l'		/* long/double real (64-bit) */
#define SIZE_T			't'		/* temp/extended real (80-bit) */
#define SIZE_A			'a'		/* packed decimal (80-bit BCD) */

struct x86opc_insn_op {
	char type;
	char extra;
	char size;
	char extendedsize;
};

struct x86opc_insn_op_special {
	char type;
	char data;
	char res1;
	char res2;
};

struct x86opc_insn {
	char *name;
	x86opc_insn_op op[3];
};

/* this can be a group (group!=0), an insn (group==0) && (insn.name!=0) or
   (otherwise) a reserved instruction. */
struct x86opc_finsn {
	x86opc_insn *group;	
	x86opc_insn insn;
};

#define X86_REG_INVALID	-2
#define X86_REG_NO		-1
#define X86_REG_AX		0
#define X86_REG_CX		1
#define X86_REG_DX		2
#define X86_REG_BX		3
#define X86_REG_SP		4
#define X86_REG_BP		5
#define X86_REG_SI		6
#define X86_REG_DI		7

#define X86_GROUPS		20

extern char *x86_regs[3][8];
extern char *x86_segs[8];
extern x86opc_insn x86_insns[256];
extern x86opc_insn x86_insns_ext[256];
extern x86opc_insn x86_group_insns[X86_GROUPS][8];

extern x86opc_insn x86_modfloat_group_insns[8][8];
extern x86opc_finsn x86_float_group_insns[8][8];

#endif /* __X86OPC_H__ */
