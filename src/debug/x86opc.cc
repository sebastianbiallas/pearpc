/* 
 *	HT Editor
 *	x86opc.cc
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf (stefan@weyergraf.de)
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

#include "x86opc.h"

/* 	Percent tokens in strings:
	First char after '%':
		A - direct address
		C - reg of r/m picks control register
		D - reg of r/m picks debug register
		E - r/m picks operand
		F - flags register
		G - reg of r/m picks general register
		I - immediate data (takes extended size, data size)
		J - relative IP offset
		M - r/m picks memory
		O - no r/m, offset only
		P - reg of r/m picks mm register (mm0-mm7)
		Q - r/m picks mm operand (mm0-mm7/mem64)
		R - mod of r/m picks register only
		S - reg of r/m picks segment register
		T - reg of r/m picks test register
		X - DS:ESI
		Y - ES:EDI
		2 - prefix of two-unsigned char opcode
		3 - prefix of 3DNow! opcode
		e - put in 'e' if use32 (second char is part of reg name)
		    put in 'w' for use16 or 'd' for use32 (second char is 'w')
		f - floating point (second char is esc value)
		g - do r/m group n (where n may be one of 0-9,A-Z)
		p - prefix
		s - size override (second char is a,o)
		+ - make default signed
	Second char after '%':
		a - two words in memory (BOUND)
		b - byte
		c - byte or word
		d - dword
		p - 32 or 48 bit pointer
		q - quadword
		s - six unsigned char pseudo-descriptor
		v - word or dword
		w - word
		F - use floating regs in mod/rm
		+ - always sign
		- - sign if negative
		1-8 - group number, esc value, etc
*/

#define Ap	TYPE_A, 0, SIZE_P, SIZE_P
#define Cd	TYPE_C, 0, SIZE_D, SIZE_D
#define Dd	TYPE_D, 0, SIZE_D, SIZE_D
#define E	TYPE_E, 0, SIZE_0, SIZE_0
#define Eb	TYPE_E, 0, SIZE_B, SIZE_B
#define Ew	TYPE_E, 0, SIZE_W, SIZE_W
#define Ed	TYPE_E, 0, SIZE_D, SIZE_D
#define Eq	TYPE_E, 0, SIZE_Q, SIZE_Q
#define Ev	TYPE_E, 0, SIZE_V, SIZE_V
#define Es	TYPE_E, 0, SIZE_S, SIZE_S
#define El	TYPE_E, 0, SIZE_L, SIZE_L
#define Et	TYPE_E, 0, SIZE_T, SIZE_T
#define Ea	TYPE_E, 0, SIZE_A, SIZE_A
#define Gb	TYPE_G, 0, SIZE_B, SIZE_B
#define Gw	TYPE_G, 0, SIZE_W, SIZE_W
#define Gv	TYPE_G, 0, SIZE_V, SIZE_V
#define Ib	TYPE_I, 0, SIZE_B, SIZE_B
#define Iw	TYPE_I, 0, SIZE_W, SIZE_W
#define Iv	TYPE_I, 0, SIZE_V, SIZE_V
#define Ibv	TYPE_I, 0, SIZE_B, SIZE_V
#define sIbv	TYPE_Is,0, SIZE_B, SIZE_V
#define Jb	TYPE_J, 0, SIZE_B, SIZE_B
#define Jv	TYPE_J, 0, SIZE_V, SIZE_V
#define M	TYPE_M, 0, 0, 0
#define Mw	TYPE_M, 0, SIZE_W, SIZE_W
#define Md	TYPE_M, 0, SIZE_D, SIZE_D
#define Mp	TYPE_M, 0, SIZE_P, SIZE_P
#define Mq	TYPE_M, 0, SIZE_Q, SIZE_Q
#define Ms	TYPE_M, 0, SIZE_S, SIZE_S
#define Ml	TYPE_M, 0, SIZE_L, SIZE_L
#define Mt	TYPE_M, 0, SIZE_T, SIZE_T
#define Ma	TYPE_M, 0, SIZE_A, SIZE_A
#define Ob	TYPE_O, 0, SIZE_B, SIZE_B
#define Ov	TYPE_O, 0, SIZE_V, SIZE_V
#define Pd	TYPE_P, 0, SIZE_D, SIZE_D
#define Pq	TYPE_P, 0, SIZE_Q, SIZE_Q
#define Qd	TYPE_Q, 0, SIZE_D, SIZE_D
#define Qq	TYPE_Q, 0, SIZE_Q, SIZE_Q
#define Rb	TYPE_R, 0, SIZE_B, SIZE_B
#define Rw	TYPE_R, 0, SIZE_W, SIZE_W
#define Rd	TYPE_R, 0, SIZE_D, SIZE_D
#define Rv	TYPE_R, 0, SIZE_V, SIZE_V
#define Sw	TYPE_S, 0, SIZE_W, SIZE_W
#define Td	TYPE_T, 0, SIZE_D, SIZE_D

#define Ft	TYPE_F, 0, SIZE_T, SIZE_T

#define __st	TYPE_Fx, 0, SIZE_T, SIZE_T

#define __1	TYPE_Ix, 1, SIZE_B, SIZE_B
#define __3	TYPE_Ix, 3, SIZE_B, SIZE_B		/* for int 3 */

#define __al	TYPE_Rx, 0, SIZE_B, SIZE_B
#define __cl	TYPE_Rx, 1, SIZE_B, SIZE_B
#define __dl	TYPE_Rx, 2, SIZE_B, SIZE_B
#define __bl	TYPE_Rx, 3, SIZE_B, SIZE_B
#define __ah	TYPE_Rx, 4, SIZE_B, SIZE_B
#define __ch	TYPE_Rx, 5, SIZE_B, SIZE_B
#define __dh	TYPE_Rx, 6, SIZE_B, SIZE_B
#define __bh	TYPE_Rx, 7, SIZE_B, SIZE_B

#define __ax	TYPE_Rx, 0, SIZE_V, SIZE_V
#define __cx	TYPE_Rx, 1, SIZE_V, SIZE_V
#define __dx	TYPE_Rx, 2, SIZE_V, SIZE_V
#define __bx	TYPE_Rx, 3, SIZE_V, SIZE_V
#define __sp	TYPE_Rx, 4, SIZE_V, SIZE_V
#define __bp	TYPE_Rx, 5, SIZE_V, SIZE_V
#define __si	TYPE_Rx, 6, SIZE_V, SIZE_V
#define __di	TYPE_Rx, 7, SIZE_V, SIZE_V

#define __axw	TYPE_Rx, 0, SIZE_W, SIZE_W
#define __dxw	TYPE_Rx, 2, SIZE_W, SIZE_W

#define __axd	TYPE_Rx, 0, SIZE_D, SIZE_D
#define __cxd	TYPE_Rx, 1, SIZE_D, SIZE_D
#define __dxd	TYPE_Rx, 2, SIZE_D, SIZE_D
#define __bxd	TYPE_Rx, 3, SIZE_D, SIZE_D
#define __spd	TYPE_Rx, 4, SIZE_D, SIZE_D
#define __bpd	TYPE_Rx, 5, SIZE_D, SIZE_D
#define __sid	TYPE_Rx, 6, SIZE_D, SIZE_D
#define __did	TYPE_Rx, 7, SIZE_D, SIZE_D

#define __es	TYPE_Sx, 0, SIZE_W, SIZE_W
#define __cs	TYPE_Sx, 1, SIZE_W, SIZE_W
#define __ss	TYPE_Sx, 2, SIZE_W, SIZE_W
#define __ds	TYPE_Sx, 3, SIZE_W, SIZE_W
#define __fs	TYPE_Sx, 4, SIZE_W, SIZE_W
#define __gs	TYPE_Sx, 5, SIZE_W, SIZE_W

#define __st0	TYPE_F, 0, SIZE_T, SIZE_T
#define __st1	TYPE_F, 1, SIZE_T, SIZE_T
#define __st2	TYPE_F, 2, SIZE_T, SIZE_T
#define __st3	TYPE_F, 3, SIZE_T, SIZE_T
#define __st4	TYPE_F, 4, SIZE_T, SIZE_T
#define __st5	TYPE_F, 5, SIZE_T, SIZE_T
#define __st6	TYPE_F, 6, SIZE_T, SIZE_T
#define __st7	TYPE_F, 7, SIZE_T, SIZE_T

char *x86_regs[3][8] = {
{"al",  "cl",  "dl",  "bl",  "ah",  "ch",  "dh",  "bh"},
{"ax",  "cx",  "dx",  "bx",  "sp",  "bp",  "si",  "di"},
{"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"}
};

char *x86_segs[8] = {
"es", "cs", "ss", "ds", "fs", "gs", 0, 0
};

#define GROUP_80		0
#define GROUP_81		1
#define GROUP_83		2
#define GROUP_C0		3
#define GROUP_C1		4
#define GROUP_D0		5
#define GROUP_D1		6
#define GROUP_D2		7
#define GROUP_D3		8
#define GROUP_F6		9
#define GROUP_F7		10
#define GROUP_FE		11
#define GROUP_FF		12
#define GROUP_EXT_00		13
#define GROUP_EXT_01		14
#define GROUP_EXT_71		15
#define GROUP_EXT_72		16
#define GROUP_EXT_73		17
#define GROUP_EXT_BA		18
#define GROUP_EXT_C7		19
//#define GROUP_EXT_AE		20

x86opc_insn x86_insns[256] = {
/* 00 */
{"add", {{Eb}, {Gb}}},
{"add", {{Ev}, {Gv}}},
{"add", {{Gb}, {Eb}}},
{"add", {{Gv}, {Ev}}},
{"add", {{__al}, {Ib}}},
{"add", {{__ax}, {Iv}}},
{"push", {{__es}}},
{"pop", {{__es}}},
/* 08 */
{"or", {{Eb}, {Gb}}},
{"or", {{Ev}, {Gv}}},
{"or", {{Gb}, {Eb}}},
{"or", {{Gv}, {Ev}}},
{"or", {{__al}, {Ib}}},
{"or", {{__ax}, {Iv}}},
{"push", {{__cs}}},
{0, {{SPECIAL_TYPE_PREFIX}}},			/* prefix */
/* 10 */
{"adc", {{Eb}, {Gb}}},
{"adc", {{Ev}, {Gv}}},
{"adc", {{Gb}, {Eb}}},
{"adc", {{Gv}, {Ev}}},
{"adc", {{__al}, {Ib}}},
{"adc", {{__ax}, {Iv}}},
{"push", {{__ss}}},
{"pop", {{__ss}}},
/* 18 */
{"sbb", {{Eb}, {Gb}}},
{"sbb", {{Ev}, {Gv}}},
{"sbb", {{Gb}, {Eb}}},
{"sbb", {{Gv}, {Ev}}},
{"sbb", {{__al}, {Ib}}},
{"sbb", {{__ax}, {Iv}}},
{"push", {{__ds}}},
{"pop", {{__ds}}},
/* 20 */
{"and", {{Eb}, {Gb}}},
{"and", {{Ev}, {Gv}}},
{"and", {{Gb}, {Eb}}},
{"and", {{Gv}, {Ev}}},
{"and", {{__al}, {Ib}}},
{"and", {{__ax}, {Iv}}},
{0, {{SPECIAL_TYPE_PREFIX}}},		/* es-prefix */
{"daa"},
/* 28 */
{"sub", {{Eb}, {Gb}}},
{"sub", {{Ev}, {Gv}}},
{"sub", {{Gb}, {Eb}}},
{"sub", {{Gv}, {Ev}}},
{"sub", {{__al}, {Ib}}},
{"sub", {{__ax}, {Iv}}},
{0, {{SPECIAL_TYPE_PREFIX}}},		/* cs-prefix */
{"das"},
/* 30 */
{"xor", {{Eb}, {Gb}}},
{"xor", {{Ev}, {Gv}}},
{"xor", {{Gb}, {Eb}}},
{"xor", {{Gv}, {Ev}}},
{"xor", {{__al}, {Ib}}},
{"xor", {{__ax}, {Iv}}},
{0, {{SPECIAL_TYPE_PREFIX}}},		/* ss-prefix */
{"aaa"},
/* 38 */
{"cmp", {{Eb}, {Gb}}},
{"cmp", {{Ev}, {Gv}}},
{"cmp", {{Gb}, {Eb}}},
{"cmp", {{Gv}, {Ev}}},
{"cmp", {{__al}, {Ib}}},
{"cmp", {{__ax}, {Iv}}},
{0, {{SPECIAL_TYPE_PREFIX}}},		/* ds-prefix */
{"aas"},
/* 40 */
{"inc", {{__ax}}},
{"inc", {{__cx}}},
{"inc", {{__dx}}},
{"inc", {{__bx}}},
{"inc", {{__sp}}},
{"inc", {{__bp}}},
{"inc", {{__si}}},
{"inc", {{__di}}},
/* 48 */
{"dec", {{__ax}}},
{"dec", {{__cx}}},
{"dec", {{__dx}}},
{"dec", {{__bx}}},
{"dec", {{__sp}}},
{"dec", {{__bp}}},
{"dec", {{__si}}},
{"dec", {{__di}}},
/* 50 */
{"push", {{__ax}}},
{"push", {{__cx}}},
{"push", {{__dx}}},
{"push", {{__bx}}},
{"push", {{__sp}}},
{"push", {{__bp}}},
{"push", {{__si}}},
{"push", {{__di}}},
/* 58 */
{"pop", {{__ax}}},
{"pop", {{__cx}}},
{"pop", {{__dx}}},
{"pop", {{__bx}}},
{"pop", {{__sp}}},
{"pop", {{__bp}}},
{"pop", {{__si}}},
{"pop", {{__di}}},
/* 60 */
{"pusha"},
{"popa"},
{"bound", {{Gv}, {Mq}}},
{"arpl", {{Ew}, {Rw}}},
{0, {{SPECIAL_TYPE_PREFIX}}},		/* fs-prefix */
{0, {{SPECIAL_TYPE_PREFIX}}},		/* gs-prefix */
{0, {{SPECIAL_TYPE_PREFIX}}},		/* op-size prefix */
{0, {{SPECIAL_TYPE_PREFIX}}},		/* addr-size prefix */
/* 68 */
{"push", {{Iv}}},
{"imul", {{Gv}, {Ev}, {Iv}}},
{"push", {{sIbv}}},
{"imul", {{Gv}, {Ev}, {sIbv}}},
{"insb"},
{"ins%c"},
{"outsb"},
{"outs%c"},
/* 70 */
{"jo", {{Jb}}},
{"jno", {{Jb}}},
{"jc", {{Jb}}},
{"jnc", {{Jb}}},
{"jz", {{Jb}}},
{"jnz", {{Jb}}},
{"jna", {{Jb}}},
{"ja", {{Jb}}},
/* 78 */
{"js", {{Jb}}},
{"jns", {{Jb}}},
{"jp", {{Jb}}},
{"jnp", {{Jb}}},
{"jl", {{Jb}}},		/* aka jnge */
{"jnl", {{Jb}}},	/* aka jge */
{"jng", {{Jb}}},	/* aka jle */
{"jg", {{Jb}}},		/* aka jnle */
/* 80 */
{0, {{SPECIAL_TYPE_GROUP, GROUP_80}}},
{0, {{SPECIAL_TYPE_GROUP, GROUP_81}}},
{0},
{0, {{SPECIAL_TYPE_GROUP, GROUP_83}}},
{"test", {{Eb}, {Gb}}},
{"test", {{Ev}, {Gv}}},
{"xchg", {{Eb}, {Gb}}},
{"xchg", {{Ev}, {Gv}}},
/* 88 */
{"mov", {{Eb}, {Gb}}},
{"mov", {{Ev}, {Gv}}},
{"mov", {{Gb}, {Eb}}},
{"mov", {{Gv}, {Ev}}},
{"mov", {{Ev}, {Sw}}},
{"lea", {{Gv}, {M}}},
{"mov", {{Sw}, {Ev}}},
{"pop", {{Ev}}},
/* 90 */
{"nop"},		/* same as xchg (e)ax, (e)ax */
{"xchg", {{__ax}, {__cx}}},
{"xchg", {{__ax}, {__dx}}},
{"xchg", {{__ax}, {__bx}}},
{"xchg", {{__ax}, {__sp}}},
{"xchg", {{__ax}, {__bp}}},
{"xchg", {{__ax}, {__si}}},
{"xchg", {{__ax}, {__di}}},
/* 98 */
{"cbw"},
{"cwd"},
{"call", {{Ap}}},
{"fwait"},
{"pushf%c"},
{"popf%c"},
{"sahf"},
{"lahf"},
/* A0 */
{"mov", {{__al}, {Ob}}},
{"mov", {{__ax}, {Ov}}},
{"mov", {{Ob}, {__al}}},
{"mov", {{Ov}, {__ax}}},
{"movsb"},
{"movs%c"},
{"cmpsb"},
{"cmps%c"},
/* A8 */
{"test", {{__al}, {Ib}}},
{"test", {{__ax}, {Iv}}},
{"stosb"},
{"stos%c"},
{"lodsb"},
{"lods%c"},
{"scasb"},
{"scas%c"},
/* B0 */
{"mov", {{__al}, {Ib}}},
{"mov", {{__cl}, {Ib}}},
{"mov", {{__dl}, {Ib}}},
{"mov", {{__bl}, {Ib}}},
{"mov", {{__ah}, {Ib}}},
{"mov", {{__ch}, {Ib}}},
{"mov", {{__dh}, {Ib}}},
{"mov", {{__bh}, {Ib}}},
/* B8 */
{"mov", {{__ax}, {Iv}}},
{"mov", {{__cx}, {Iv}}},
{"mov", {{__dx}, {Iv}}},
{"mov", {{__bx}, {Iv}}},
{"mov", {{__sp}, {Iv}}},
{"mov", {{__bp}, {Iv}}},
{"mov", {{__si}, {Iv}}},
{"mov", {{__di}, {Iv}}},
/* C0 */
{0, {{SPECIAL_TYPE_GROUP, GROUP_C0}}},
{0, {{SPECIAL_TYPE_GROUP, GROUP_C1}}},
{"ret", {{Iw}}},
{"ret"},
{"les", {{Gv}, {Mp}}},
{"lds", {{Gv}, {Mp}}},
{"mov", {{Eb}, {Ib}}},
{"mov", {{Ev}, {Iv}}},
/* C8 */
{"enter", {{Iw}, {Ib}}},
{"leave"},
{"retf", {{Iw}}},
{"retf"},
{"int", {{__3}}},
{"int", {{Ib}}},
{"into"},
{"iret%c"},
/* D0 */
{0, {{SPECIAL_TYPE_GROUP, GROUP_D0}}},
{0, {{SPECIAL_TYPE_GROUP, GROUP_D1}}},
{0, {{SPECIAL_TYPE_GROUP, GROUP_D2}}},
{0, {{SPECIAL_TYPE_GROUP, GROUP_D3}}},
{"aam", {{Ib}}},
{"aad", {{Ib}}},
{"setalc"},
{"xlat"},
/* D8 */
{0, {{SPECIAL_TYPE_FGROUP, 0}}},
{0, {{SPECIAL_TYPE_FGROUP, 1}}},
{0, {{SPECIAL_TYPE_FGROUP, 2}}},
{0, {{SPECIAL_TYPE_FGROUP, 3}}},
{0, {{SPECIAL_TYPE_FGROUP, 4}}},
{0, {{SPECIAL_TYPE_FGROUP, 5}}},
{0, {{SPECIAL_TYPE_FGROUP, 6}}},
{0, {{SPECIAL_TYPE_FGROUP, 7}}},
/* E0 */
{"loopnz", {{Jb}}},
{"loopz", {{Jb}}},
{"loop", {{Jb}}},
{"jcxz", {{Jb}}},
{"in", {{__al}, {Ib}}},
{"in", {{__ax}, {Ib}}},
{"out", {{Ib}, {__al}}},
{"out", {{Ib}, {__ax}}},
/* E8 */
{"call", {{Jv}}},
{"jmp", {{Jv}}},
{"jmp", {{Ap}}},
{"jmp", {{Jb}}},
{"in", {{__al}, {__dxw}}},
{"in", {{__ax}, {__dxw}}},
{"out", {{__dxw}, {__al}}},
{"out", {{__dxw}, {__ax}}},
/* F0 */
{0, {{SPECIAL_TYPE_PREFIX}}},		/* lock-prefix */
{"smi"},
{0, {{SPECIAL_TYPE_PREFIX}}},		/* repnz-prefix */
{0, {{SPECIAL_TYPE_PREFIX}}},		/* rep-prefix */
{"hlt"},
{"cmc"},
{0, {{SPECIAL_TYPE_GROUP, GROUP_F6}}},
{0, {{SPECIAL_TYPE_GROUP, GROUP_F7}}},
/* F8 */
{"clc"},
{"stc"},
{"cli"},
{"sti"},
{"cld"},
{"std"},
{0, {{SPECIAL_TYPE_GROUP, GROUP_FE}}},
{0, {{SPECIAL_TYPE_GROUP, GROUP_FF}}},
};

x86opc_insn x86_insns_ext[256] = {
/* 00 */
{0, {{SPECIAL_TYPE_GROUP, GROUP_EXT_00}}},
{0, {{SPECIAL_TYPE_GROUP, GROUP_EXT_01}}},
{"lar", {{Gv}, {Ew}}},
{"lsl", {{Gv}, {Ew}}},
{0},
{0},
{"clts"},
{0},
/* 08 */
{"invd"},
{"wbinvd"},
{0},
{"ud2"},
{0},
{"prefetch", {{Eb}}},
{"femms"},
{0, {{SPECIAL_TYPE_PREFIX}}},
/* 10 */
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
/* 18 */
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
/* 20 */
{"mov", {{Rd}, {Cd}}},
{"mov", {{Rd}, {Dd}}},
{"mov", {{Cd}, {Rd}}},
{"mov", {{Dd}, {Rd}}},
{"mov", {{Rd}, {Td}}},
{0},
{"mov", {{Td}, {Rd}}},
{0},
/* 28 */
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
/* 30 */
{"wrmsr"},
{"rdtsc"},
{"rdmsr"},
{"rdpmc"},
{"sysenter"},
{"sysexit"},
{0},
{0},
/* 38 */
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
/* 40 */
{"cmovo", {{Gv}, {Ev}}},
{"cmovno", {{Gv}, {Ev}}},
{"cmovc", {{Gv}, {Ev}}},
{"cmovnc", {{Gv}, {Ev}}},
{"cmovz", {{Gv}, {Ev}}},
{"cmovnz", {{Gv}, {Ev}}},
{"cmova", {{Gv}, {Ev}}},
{"cmovna", {{Gv}, {Ev}}},
/* 48 */
{"cmovs", {{Gv}, {Ev}}},
{"cmovns", {{Gv}, {Ev}}},
{"cmovp", {{Gv}, {Ev}}},
{"cmovnp", {{Gv}, {Ev}}},
{"cmovl", {{Gv}, {Ev}}},
{"cmovnl", {{Gv}, {Ev}}},
{"cmovng", {{Gv}, {Ev}}},
{"cmovg", {{Gv}, {Ev}}},
/* 50 */
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
/* 58 */
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
/* 60 */
{"punpcklbw", {{Pq}, {Qd}}},
{"punpcklwd", {{Pq}, {Qd}}},
{"punpckldq", {{Pq}, {Qd}}},
{"packsswb", {{Pq}, {Qd}}},
{"pcmpgtb", {{Pq}, {Qd}}},
{"pcmpgtw", {{Pq}, {Qd}}},
{"pcmpgtd", {{Pq}, {Qd}}},
{"packuswb", {{Pq}, {Qd}}},
/* 68 */
{"punpckhbw", {{Pq}, {Qd}}},
{"punpckhwd", {{Pq}, {Qd}}},
{"punpckhdq", {{Pq}, {Qd}}},
{"packssdw", {{Pq}, {Qd}}},
{0},
{0},
{"movd", {{Pd}, {Ed}}},
{"movq", {{Pq}, {Qq}}},
/* 70 */
{0},
{0, {{SPECIAL_TYPE_GROUP, GROUP_EXT_71}}},
{0, {{SPECIAL_TYPE_GROUP, GROUP_EXT_72}}},
{0, {{SPECIAL_TYPE_GROUP, GROUP_EXT_73}}},
{"pcmpeqb", {{Pq}, {Qq}}},
{"pcmpeqw", {{Pq}, {Qq}}},
{"pcmpewd", {{Pq}, {Qq}}},
{"emms"},
/* 78 */
{0},
{0},
{0},
{0},
{0},
{0},
{"movd", {{Ed}, {Pd}}},
{"movq", {{Qq}, {Pq}}},
/* 80 */
{"jo", {{Jv}}},
{"jno", {{Jv}}},
{"jc", {{Jv}}},
{"jnc", {{Jv}}},
{"jz", {{Jv}}},
{"jnz", {{Jv}}},
{"jna", {{Jv}}},
{"ja", {{Jv}}},
/* 88 */
{"js", {{Jv}}},
{"jns", {{Jv}}},
{"jpe", {{Jv}}},
{"jpo", {{Jv}}},
{"jl", {{Jv}}},
{"jnl", {{Jv}}},
{"jng", {{Jv}}},
{"jg", {{Jv}}},
/* 90 */
{"seto", {{Eb}}},
{"setno", {{Eb}}},
{"setc", {{Eb}}},
{"setnc", {{Eb}}},
{"setz", {{Eb}}},
{"setnz", {{Eb}}},
{"setna", {{Eb}}},
{"seta", {{Eb}}},
/* 98 */
{"sets", {{Eb}}},
{"setns", {{Eb}}},
{"setpe", {{Eb}}},
{"setpo", {{Eb}}},
{"setl", {{Eb}}},
{"setnl", {{Eb}}},
{"setng", {{Eb}}},
{"setg", {{Eb}}},
/* A0 */
{"push", {{__fs}}},
{"pop", {{__fs}}},
{"cpuid"},
{"bt", {{Ev}, {Gv}}},
{"shld", {{Ev}, {Gv}, {Ib}}},
{"shld", {{Ev}, {Gv}, {__cl}}},
{0},
{0},
/* A8 */
{"push", {{__gs}}},
{"pop", {{__gs}}},
{"rsm"},
{"bts", {{Ev}, {Gv}}},
{"shrd", {{Ev}, {Gv}, {Ib}}},
{"shrd", {{Ev}, {Gv}, {__cl}}},
{0},
{"imul", {{Gv}, {Ev}}},
/* B0 */
{"cmpxchg", {{Eb}, {Gb}}},
{"cmpxchg", {{Ev}, {Gv}}},
{"lss", {{Gv}, {Mp}}},
{"btr", {{Ev}, {Gv}}},
{"lfs", {{Gv}, {Mp}}},
{"lgs", {{Gv}, {Mp}}},
{"movzx", {{Gv}, {Eb}}},
{"movzx", {{Gv}, {Ew}}},
/* B8 */
{0},
{"ud2"},
{0, {{SPECIAL_TYPE_GROUP, GROUP_EXT_BA}}},
{"btc", {{Ev}, {Gv}}},
{"bsf", {{Gv}, {Ev}}},
{"bsr", {{Gv}, {Ev}}},
{"movsx", {{Gv}, {Eb}}},
{"movsx", {{Gv}, {Ew}}},
/* C0 */
{"xadd", {{Eb}, {Gb}}},
{"xadd", {{Ev}, {Gv}}},
{0},
{0},
{0},
{0},
{0},
{0, {{SPECIAL_TYPE_GROUP, GROUP_EXT_C7}}},
/* C8 */
{"bswap", {{__axd}}},
{"bswap", {{__cxd}}},
{"bswap", {{__dxd}}},
{"bswap", {{__bxd}}},
{"bswap", {{__spd}}},
{"bswap", {{__bpd}}},
{"bswap", {{__sid}}},
{"bswap", {{__did}}},
/* D0 */
{0},
{"psrlw", {{Pq}, {Qq}}},
{"psrld", {{Pq}, {Qq}}},
{"psrlq", {{Pq}, {Qq}}},
{0},
{"pmullw", {{Pq}, {Qq}}},
{0},
{0},
/* D8 */
{"psubusb", {{Pq}, {Qq}}},
{"psubusw", {{Pq}, {Qq}}},
{0},
{"pand", {{Pq}, {Qq}}},
{"paddusb", {{Pq}, {Qq}}},
{"paddusw", {{Pq}, {Qq}}},
{0},
{"pandn", {{Pq}, {Qq}}},
/* E0 */
{0},
{"psraw", {{Pq}, {Qq}}},
{"psrad", {{Pq}, {Qq}}},
{0},
{0},
{"pmulhw", {{Pq}, {Qq}}},
{0},
{0},
/* E8 */
{"psubsb", {{Pq}, {Qq}}},
{"psubsw", {{Pq}, {Qq}}},
{0},
{"por", {{Pq}, {Qq}}},
{"paddsb", {{Pq}, {Qq}}},
{"paddsw", {{Pq}, {Qq}}},
{0},
{"pxor", {{Pq}, {Qq}}},
/* F0 */
{0},
{"psllw", {{Pq}, {Qq}}},
{"pslld", {{Pq}, {Qq}}},
{"psllq", {{Pq}, {Qq}}},
{0},
{"pmuladdwd", {{Pq}, {Qq}}},
{0},
{0},
/* F8 */
{"psubb", {{Pq}, {Qq}}},
{"psubw", {{Pq}, {Qq}}},
{"psubq", {{Pq}, {Qq}}},
{0},
{"paddb", {{Pq}, {Qq}}},
{"paddw", {{Pq}, {Qq}}},
{"paddq", {{Pq}, {Qq}}},
{0}
};

x86opc_insn x86_group_insns[X86_GROUPS][8] = {
/* 0 - GROUP_80 */
{
{"add", {{Eb}, {Ib}}},
{"or", {{Eb}, {Ib}}},
{"adc", {{Eb}, {Ib}}},
{"sbb", {{Eb}, {Ib}}},
{"and", {{Eb}, {Ib}}},
{"sub", {{Eb}, {Ib}}},
{"xor", {{Eb}, {Ib}}},
{"cmp", {{Eb}, {Ib}}}
},
/* 1 - GROUP_81 */
{
{"add", {{Ev}, {Iv}}},
{"or", {{Ev}, {Iv}}},
{"adc", {{Ev}, {Iv}}},
{"sbb", {{Ev}, {Iv}}},
{"and", {{Ev}, {Iv}}},
{"sub", {{Ev}, {Iv}}},
{"xor", {{Ev}, {Iv}}},
{"cmp", {{Ev}, {Iv}}}
},
/* 2 - GROUP_83 */
{
{"add", {{Ev}, {sIbv}}},
{"or", {{Ev}, {sIbv}}},
{"adc", {{Ev}, {sIbv}}},
{"sbb", {{Ev}, {sIbv}}},
{"and", {{Ev}, {sIbv}}},
{"sub", {{Ev}, {sIbv}}},
{"xor", {{Ev}, {sIbv}}},
{"cmp", {{Ev}, {sIbv}}}
},
/* 3 - GROUP_C0 */
{
{"rol", {{Eb}, {Ib}}},
{"ror", {{Eb}, {Ib}}},
{"rcl", {{Eb}, {Ib}}},
{"rcr", {{Eb}, {Ib}}},
{"shl", {{Eb}, {Ib}}},
{"shr", {{Eb}, {Ib}}},
{"sal", {{Eb}, {Ib}}},
{"sar", {{Eb}, {Ib}}}
},
/* 4 - GROUP_C1 */
{
{"rol", {{Ev}, {Ib}}},
{"ror", {{Ev}, {Ib}}},
{"rcl", {{Ev}, {Ib}}},
{"rcr", {{Ev}, {Ib}}},
{"shl", {{Ev}, {Ib}}},
{"shr", {{Ev}, {Ib}}},
{"sal", {{Ev}, {Ib}}},
{"sar", {{Ev}, {Ib}}}
},
/* 5 - GROUP_D0 */
{
{"rol", {{Eb}, {__1}}},
{"ror", {{Eb}, {__1}}},
{"rcl", {{Eb}, {__1}}},
{"rcr", {{Eb}, {__1}}},
{"shl", {{Eb}, {__1}}},
{"shr", {{Eb}, {__1}}},
{"sal", {{Eb}, {__1}}},
{"sar", {{Eb}, {__1}}}
},
/* 6 - GROUP_D1 */
{
{"rol", {{Ev}, {__1}}},
{"ror", {{Ev}, {__1}}},
{"rcl", {{Ev}, {__1}}},
{"rcr", {{Ev}, {__1}}},
{"shl", {{Ev}, {__1}}},
{"shr", {{Ev}, {__1}}},
{"sal", {{Ev}, {__1}}},
{"sar", {{Ev}, {__1}}}
},
/* 7 - GROUP_D2 */
{
{"rol", {{Eb}, {__cl}}},
{"ror", {{Eb}, {__cl}}},
{"rcl", {{Eb}, {__cl}}},
{"rcr", {{Eb}, {__cl}}},
{"shl", {{Eb}, {__cl}}},
{"shr", {{Eb}, {__cl}}},
{"sal", {{Eb}, {__cl}}},
{"sar", {{Eb}, {__cl}}}
},
/* 8 - GROUP_D3 */
{
{"rol", {{Ev}, {__cl}}},
{"ror", {{Ev}, {__cl}}},
{"rcl", {{Ev}, {__cl}}},
{"rcr", {{Ev}, {__cl}}},
{"shl", {{Ev}, {__cl}}},
{"shr", {{Ev}, {__cl}}},
{"sal", {{Ev}, {__cl}}},
{"sar", {{Ev}, {__cl}}}
},
/* 9 - GROUP_F6 */
{
{"test", {{Eb}, {Ib}}},
//{"test", {{Eb}, {Ib}}},	unsure...
{0},
{"not", {{Eb}}},
{"neg", {{Eb}}},
{"mul", {{__al}, {Eb}}},
{"imul", {{__al}, {Eb}}},
{"div", {{__al}, {Eb}}},
{"idiv", {{__al}, {Eb}}}
},
/* 10 - GROUP_F7 */
{
{"test", {{Ev}, {Iv}}},
{"test", {{Ev}, {Iv}}},
{"not", {{Ev}}},
{"neg", {{Ev}}},
{"mul", {{__ax}, {Ev}}},
{"imul", {{__ax}, {Ev}}},
{"div", {{__ax}, {Ev}}},
{"idiv", {{__ax}, {Ev}}}
},
/* 11 - GROUP_FE */
{
{"inc", {{Eb}}},
{"dec", {{Eb}}},
{0},
{0},
{0},
{0},
{0},
{0}
},
/* 12 - GROUP_FF */
{
{"inc", {{Ev}}},
{"dec", {{Ev}}},
{"call", {{Ev}}},
{"call", {{Mp}}},
{"jmp", {{Ev}}},
{"jmp", {{Mp}}},
{"push", {{Ev}}},
{0}
},
/* 13 - GROUP_EXT_00 */
{
{"sldt", {{Ew}}},
{"str", {{Ew}}},
{"lldt", {{Ew}}},
{"ltr", {{Ew}}},
{"verr", {{Ew}}},
{"verw", {{Ew}}},
{0},
{0}
},
/* 14 - GROUP_EXT_01 */
{
{"sgdt", {{M}}},
{"sidt", {{M}}},
{"lgdt", {{M}}},
{"lidt", {{M}}},
{"smsw", {{Ew}}},
{0},
{"lmsw", {{Ew}}},
{0}
},
/* 15 - GROUP_EXT_71 */
{
{0},
{0},
{"psrlw", {{Pq}, {Ib}}},
{0},
{"psraw", {{Pq}, {Ib}}},
{0},
{"psslw", {{Pq}, {Ib}}},
{0}
},
/* 16 - GROUP_EXT_72 */
{
{0},
{0},
{"psrld", {{Pq}, {Ib}}},
{0},
{"psrad", {{Pq}, {Ib}}},
{0},
{"pssld", {{Pq}, {Ib}}},
{0}
},
/* 17 - GROUP_EXT_73 */
{
{0},
{0},
{"psrlq", {{Pq}, {Ib}}},
{0},
{"psraq", {{Pq}, {Ib}}},
{0},
{"psslq", {{Pq}, {Ib}}},
{0}
},
/* 18 - GROUP_EXT_BA */
{
{0},
{0},
{0},
{0},
{"bt", {{Ev}, {Ib}}},
{"bts", {{Ev}, {Ib}}},
{"btr", {{Ev}, {Ib}}},
{"btc", {{Ev}, {Ib}}}
},
/* 19 - GROUP_EXT_C7 */
{
{0},
{"cmpxchg8b", {{Eq}}},
{0},
{0},
{0},
{0},
{0},
{0}
}
/*
,
/ * 20 - GROUP_EXT_AE * /
{
{"fxsave", {{Eb}}},
{"fxrstor", {{Eb}}},
{"ldmxcsr", {{Md}}},
{"stmxcsr", {{Md}}},
{0},
{0},
{0},
{"sfence"}
}*/

};

/*
 *	The ModR/M byte is < 0xC0
 */

x86opc_insn x86_modfloat_group_insns[8][8] = {
/* prefix D8 */
{
{"fadd", {{Ms}}},
{"fmul", {{Ms}}},
{"fcom", {{Ms}}},
{"fcomp", {{Ms}}},
{"fsub", {{Ms}}},
{"fsubr", {{Ms}}},
{"fdiv", {{Ms}}},
{"fdivr", {{Ms}}}
},
/* prefix D9 */
{
{"fld", {{Ms}}},
{0},
{"fst", {{Ms}}},
{"fstp", {{Ms}}},
{"fldenv", {{M}}},
{"fldcw", {{Mw}}},
{"fstenv", {{M}}},
{"fstcw", {{Mw}}}
},
/* prefix DA */
{
{"fiadd", {{Md}}},
{"fimul", {{Md}}},
{"ficom", {{Md}}},
{"ficomp", {{Md}}},
{"fisub", {{Md}}},
{"fisubr", {{Md}}},
{"fidiv", {{Md}}},
{"fidivr", {{Md}}}
},
/* prefix DB */
{
{"fild", {{Md}}},
{0},
{"fist", {{Md}}},
{"fistp", {{Md}}},
{0},
{"fld", {{Mt}}},
{0},
{"fstp", {{Mt}}}
},
/* prefix DC */
{
{"fadd", {{Ml}}},
{"fmul", {{Ml}}},
{"fcom", {{Ml}}},
{"fcomp", {{Ml}}},
{"fsub", {{Ml}}},
{"fsubr", {{Ml}}},
{"fdiv", {{Ml}}},
{"fdivr", {{Ml}}}
},
/* prefix DD */
{
{"fld", {{Ml}}},
{0},
{"fst", {{Ml}}},
{"fstp", {{Ml}}},
{"frstor", {{M}}},
{0},
{"fsave", {{M}}},
{"fstsw", {{Mw}}}
},
/* prefix DE */
{
{"fiadd", {{Mw}}},
{"fimul", {{Mw}}},
{"ficom", {{Mw}}},
{"ficomp", {{Mw}}},
{"fisub", {{Mw}}},
{"fisubr", {{Mw}}},
{"fidiv", {{Mw}}},
{"fidivr", {{Mw}}}
},
/* prefix DF */
{
{"fild", {{Mw}}},
{0},
{"fist", {{Mw}}},
{"fistp", {{Mw}}},
{"fbld", {{Ma}}},
{"fild", {{Mq}}},
{"fbstp", {{Ma}}},
{"fistp", {{Mq}}}
}

};

x86opc_insn fgroup_12[8] = {
{"fnop"},
{0},
{0},
{0},
{0},
{0},
{0},
{0}
};

x86opc_insn fgroup_14[8] = {
{"fchs"},
{"fabs"},
{0},
{0},
{"ftst"},
{"fxam"},
{0},
{0}
};

x86opc_insn fgroup_15[8] = {
{"fld1"},
{"fldl2t"},
{"fldl2e"},
{"fldpi"},
{"fldlg2"},
{"fldln2"},
{"fldz"},
{0}
};

x86opc_insn fgroup_16[8] = {
{"f2xm1"},
{"fyl2x"},
{"fptan"},
{"fpatan"},
{"fxtract"},
{"fprem1"},
{"fdecstp"},
{"fincstp"}
};

x86opc_insn fgroup_17[8] = {
{"fprem"},
{"fyl2xp1"},
{"fsqrt"},
{"fsincos"},
{"frndint"},
{"fscale"},
{"fsin"},
{"fcos"}
};

x86opc_insn fgroup_25[8] = {
{0},
{"fucompp"},
{0},
{0},
{0},
{0},
{0},
{0}
};

x86opc_insn fgroup_34[8] = {
{0},
{0},
{"fclex"},
{"finit"},
{0},
{0},
{0},
{0}
};

x86opc_insn fgroup_63[8] = {
{0},
{"fcompp"},
{0},
{0},
{0},
{0},
{0},
{0}
};

x86opc_insn fgroup_74[8] = {
{"fstsw", {{__axw}}},
{0},
{0},
{0},
{0},
{0},
{0},
{0}
};

/*
 *	The ModR/M byte is >= 0xC0
 */

x86opc_finsn x86_float_group_insns[8][8] = {
/* prefix D8 */
{
{0, {"fadd", {{__st}, {Ft}}}},
{0, {"fmul", {{__st}, {Ft}}}},
{0, {"fcom", {{__st}, {Ft}}}},
{0, {"fcomp", {{__st}, {Ft}}}},
{0, {"fsub", {{__st}, {Ft}}}},
{0, {"fsubr", {{__st}, {Ft}}}},
{0, {"fdiv", {{__st}, {Ft}}}},
{0, {"fdivr", {{__st}, {Ft}}}}
},
/* prefix D9 */
{
{0, {"fld", {{__st}, {Ft}}}},
{0, {"fxch", {{__st}, {Ft}}}},
{(x86opc_insn *)&fgroup_12},
{0},
{(x86opc_insn *)&fgroup_14},
{(x86opc_insn *)&fgroup_15},
{(x86opc_insn *)&fgroup_16},
{(x86opc_insn *)&fgroup_17}
},
/* prefix DA */
{
{0, {"fcmovb", {{__st}, {Ft}}}},
{0, {"fcmove", {{__st}, {Ft}}}},
{0, {"fcmovbe", {{__st}, {Ft}}}},
{0, {"fcmovu", {{__st}, {Ft}}}},
{0},
{(x86opc_insn *)&fgroup_25},
{0},
{0}
},
/* prefix DB */
{
{0, {"fcmovnb", {{__st}, {Ft}}}},
{0, {"fcmovne", {{__st}, {Ft}}}},
{0, {"fcmovnbe", {{__st}, {Ft}}}},
{0, {"fcmovnu", {{__st}, {Ft}}}},
{(x86opc_insn*)&fgroup_34},
{0, {"fucomi", {{__st}, {Ft}}}},
{0, {"fcomi", {{__st}, {Ft}}}},
{0}
},
/* prefix DC */
{
{0, {"fadd", {{Ft}, {__st}}}},
{0, {"fmul", {{Ft}, {__st}}}},
{0},
{0},
{0, {"fsubr", {{Ft}, {__st}}}},
{0, {"fsub", {{Ft}, {__st}}}},
{0, {"fdivr", {{Ft}, {__st}}}},
{0, {"fdiv", {{Ft}, {__st}}}}
},
/* prefix DD */
{
{0, {"ffree", {{Ft}}}},
{0},
{0, {"fst", {{Ft}}}},
{0, {"fstp", {{Ft}}}},
{0, {"fucom", {{Ft}, {__st}}}},
{0, {"fucomp", {{Ft}}}},
{0},
{0}
},
/* prefix DE */
{
{0, {"faddp", {{Ft}, {__st}}}},
{0, {"fmulp", {{Ft}, {__st}}}},
{0},
{(x86opc_insn*)&fgroup_63},
{0, {"fsubrp", {{Ft}, {__st}}}},
{0, {"fsubp", {{Ft}, {__st}}}},
{0, {"fdivrp", {{Ft}, {__st}}}},
{0, {"fdivp", {{Ft}, {__st}}}}
},
/* prefix DF */
{
{0},
{0},
{0},
{0},
{(x86opc_insn*)&fgroup_74},
{0, {"fucomip", {{__st}, {Ft}}}},
{0, {"fcomip", {{__st}, {Ft}}}},
{0}
}

};
