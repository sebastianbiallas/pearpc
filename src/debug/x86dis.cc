/*
 *	HT Editor
 *	x86dis.cc
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

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "x86dis.h"

#define mkmod(modrm) (((modrm)>>6)&3)
#define mkreg(modrm) (((modrm)>>3)&7)
#define mkrm(modrm) ((modrm)&7)

#define mkscale(modrm) (((modrm)>>6)&3)
#define mkindex(modrm) (((modrm)>>3)&7)
#define mkbase(modrm) ((modrm)&7)

int modrm16_1[8] = { X86_REG_BX, X86_REG_BX, X86_REG_BP, X86_REG_BP,
				 X86_REG_SI, X86_REG_DI, X86_REG_BP, X86_REG_BX};
int modrm16_2[8] = { X86_REG_SI, X86_REG_DI, X86_REG_SI, X86_REG_DI,
				 X86_REG_NO, X86_REG_NO, X86_REG_NO, X86_REG_NO};

int sibbase[8] = { X86_REG_AX, X86_REG_CX, X86_REG_DX, X86_REG_BX,
			    X86_REG_SP, X86_REG_BP, X86_REG_SI, X86_REG_DI };

int sibindex[8] = { X86_REG_AX, X86_REG_CX, X86_REG_DX, X86_REG_BX,
				X86_REG_NO, X86_REG_BP, X86_REG_SI, X86_REG_DI };

int sibscale[4] = {1, 2, 4, 8};

/*
 *	CLASS X86Disassembler
 */

X86Disassembler::X86Disassembler()
{
}

X86Disassembler::X86Disassembler(int _opsize, int _addrsize)
{
	opsize=_opsize;
	addrsize=_addrsize;
}

X86Disassembler::~X86Disassembler()
{
}

dis_insn *X86Disassembler::decode(const byte *code, int Maxlen, CPU_ADDR Addr)
{
	ocodep=code;
/* initialize */
	codep=ocodep;
	maxlen=Maxlen;
	seg=Addr.addr32.seg;
	addr=Addr.addr32.offset;
	modrm=-1;
	sib=-1;
	memset(&insn, 0, sizeof(insn));
	insn.invalid=false;
	insn.eopsize=opsize;
	insn.eaddrsize=opsize;

	prefixes();

	insn.opcode=c;
	decode_insn(&x86_insns[insn.opcode]);

	if (insn.invalid) {
		insn.name="db";
		insn.size=1;
		insn.op[0].type=X86_OPTYPE_IMM;
		insn.op[0].size=1;
		insn.op[0].imm=*code;
		insn.lockprefix=X86_PREFIX_NO;
		insn.repprefix=X86_PREFIX_NO;
		insn.segprefix=X86_PREFIX_NO;
		for (int i=1; i<3; i++) insn.op[i].type=X86_OPTYPE_EMPTY;
	} else {
		insn.size=codep-ocodep;
	}
	return &insn;
}

void X86Disassembler::decode_modrm(x86_insn_op *op, char size, int allow_reg, int allow_mem, int mmx)
{
	int modrm=getmodrm();
	int mod=mkmod(modrm);
	int rm=mkrm(modrm);
	if (mod==3) {
/* reg */
		if (!allow_reg) {
			invalidate();
			return;
		}
		if (mmx) {
			op->type=X86_OPTYPE_MMX;
			op->size=8;
			op->mmx=rm;
		} else {
			op->type=X86_OPTYPE_REG;
			op->size=esizeop(size);
			op->reg=rm;
		}
	} else {
/* mem */
		if (!allow_mem) {
			invalidate();
			return;
		}
		op->mem.addrsize=insn.eaddrsize;
		if (insn.eaddrsize==X86_ADDRSIZE16) {
			op->type=X86_OPTYPE_MEM;
			op->size=esizeop(size);
			op->mem.floatptr=isfloat(size);
			if ((mod==0) && (rm==6)) {
				op->mem.hasdisp=1;
				op->mem.disp=getword();
				op->mem.base=X86_REG_NO;
				op->mem.index=X86_REG_NO;
				op->mem.scale=0;
			} else {
				op->mem.base=modrm16_1[rm];
				op->mem.index=modrm16_2[rm];
				op->mem.scale=1;
				switch (mod) {
					case 0:
						op->mem.hasdisp=0;
						op->mem.disp=0;
						break;
					case 1:
						op->mem.hasdisp=1;
						op->mem.disp=(char)getbyte();
						break;
					case 2:
						op->mem.hasdisp=1;
						op->mem.disp=(short)getword();
						break;
				}
			}
		} else {
			op->type=X86_OPTYPE_MEM;
			op->size=esizeop(size);
			op->mem.floatptr=isfloat(size);
			if ((mod==0) && (rm==5)) {
				op->mem.hasdisp=1;
				op->mem.disp=getdword();
				op->mem.base=X86_REG_NO;
				op->mem.index=X86_REG_NO;
				op->mem.scale=0;
			} else if (rm==4) {
				decode_sib(op, mod);
			} else {
				op->mem.base=rm;
				op->mem.index=X86_REG_NO;
				op->mem.scale=1;
				switch (mod) {
					case 0:
						op->mem.hasdisp=0;
						op->mem.disp=0;
						break;
					case 1:
						op->mem.hasdisp=1;
						op->mem.disp=(char)getbyte();
						break;
					case 2:
						op->mem.hasdisp=1;
						op->mem.disp=getdword();
						break;
				}
			}
		}
	}
}

void X86Disassembler::decode_insn(x86opc_insn *xinsn)
{
	if (!xinsn->name) {
		x86opc_insn_op_special special=*((x86opc_insn_op_special*)(&xinsn->op[0]));
		switch (special.type) {
			case SPECIAL_TYPE_INVALID:
				invalidate();
				break;
			case SPECIAL_TYPE_PREFIX:
				switch (c) {
					case 0x0f:
						if (insn.opcodeclass == X86DIS_OPCODE_CLASS_STD) {
							insn.opcode = getbyte();
							insn.opcodeclass = X86DIS_OPCODE_CLASS_EXT;
							decode_insn(&x86_insns_ext[insn.opcode]);
							break;
						}							    
					default:
						invalidate();
						break;
				}
				break;
			case SPECIAL_TYPE_GROUP: {
				int m = mkreg(getmodrm());
				insn.opcode |= m<<8;
				decode_insn(&x86_group_insns[(int)special.data][m]);
				break;
			}
			case SPECIAL_TYPE_FGROUP: {
				int m = getmodrm();
				if (mkmod(m) == 3) {
					x86opc_finsn f = x86_float_group_insns[(int)special.data][mkreg(m)];
/*					fprintf(stderr, "special.data=%d, m=%d, mkreg(m)=%d, mkrm(m)=%d\n", special.data, m, mkreg(m), mkrm(m));*/
					if (f.group) {
						decode_insn(&f.group[mkrm(m)]);
					} else if (f.insn.name) {
						decode_insn(&f.insn);
					} else invalidate();
				} else {
					decode_insn(&x86_modfloat_group_insns[(int)special.data][mkreg(m)]);
				}
				break;
			}
		}
	} else {
		insn.name=xinsn->name;
		for (int i=0; i<3; i++) {
			decode_op(&insn.op[i], &xinsn->op[i]);
		}
	}
}

void X86Disassembler::decode_op(x86_insn_op *op, x86opc_insn_op *xop)
{
	switch (xop->type) {
		case TYPE_0:
			return;
		case TYPE_A: {
/* direct address without ModR/M */
			op->type=X86_OPTYPE_FARPTR;
			op->size=esizeop(xop->size);
			switch (op->size) {
				case 4:
					op->farptr.offset=getword();
					op->farptr.seg=getword();
					break;
				case 6:
					op->farptr.offset=getdword();
					op->farptr.seg=getword();
					break;
			}
			break;
		}
		case TYPE_C: {
/* reg of ModR/M picks control register */
			op->type=X86_OPTYPE_CRX;
			op->size=esizeop(xop->size);
			op->crx=mkreg(getmodrm());
			break;
		}
		case TYPE_D: {
/* reg of ModR/M picks debug register */
			op->type=X86_OPTYPE_DRX;
			op->size=esizeop(xop->size);
			op->drx=mkreg(getmodrm());
			break;
		}
		case TYPE_E: {
/* ModR/M (general reg or memory) */
			decode_modrm(op, xop->size, (xop->size!=SIZE_P), 1, 0);
			break;
		}
		case TYPE_F: {
/* r/m of ModR/M picks a fpu register */
			op->type=X86_OPTYPE_STX;
			op->size=10;
			op->stx=mkrm(getmodrm());
			break;
		}
		case TYPE_Fx: {
/* extra picks a fpu register */
			op->type=X86_OPTYPE_STX;
			op->size=10;
			op->stx=xop->extra;
			break;
		}
		case TYPE_G: {
/* reg of ModR/M picks general register */
			op->type=X86_OPTYPE_REG;
			op->size=esizeop(xop->size);
			op->reg=mkreg(getmodrm());
			break;
		}
		case TYPE_Is: {
/* signed immediate */
			op->type=X86_OPTYPE_IMM;
			op->size=esizeop(xop->extendedsize);
			int s=esizeop(xop->size);
			switch (s) {
				case 1:
					op->imm=(char)getbyte();
					break;
				case 2:
					op->imm=(short)getword();
					break;
				case 4:
					op->imm=getdword();
					break;
			}
			switch (op->size) {
				case 1:
					op->imm&=0xff;
					break;
				case 2:
					op->imm&=0xffff;
					break;
			}
			break;
		}
		case TYPE_I: {
/* unsigned immediate */
			op->type=X86_OPTYPE_IMM;
			op->size=esizeop(xop->extendedsize);
			int s=esizeop(xop->size);
			switch (s) {
				case 1:
					op->imm=getbyte();
					break;
				case 2:
					op->imm=getword();
					break;
				case 4:
					op->imm=getdword();
					break;
			}
			break;
		}
		case TYPE_Ix: {
/* fixed immediate */
			op->type=X86_OPTYPE_IMM;
			op->size=esizeop(xop->extendedsize);
			op->imm=xop->extra;
			break;
		}
		case TYPE_J: {
/* relative branch offset */
			op->type=X86_OPTYPE_IMM;
//			op->size=esizeaddr(xop->extendedsize);
			op->size=(addrsize==X86_ADDRSIZE32) ? 4 : 2;
			int s=esizeop(xop->size);
			switch (s) {
				case 1:
					op->imm=(char)getbyte()+addr;
					break;
				case 2:
					op->imm=(short)getword()+addr;
					break;
				case 4:
					op->imm=getdword()+addr;
					break;
			}
			break;
		}
		case TYPE_M: {
/* ModR/M (memory only) */
			decode_modrm(op, xop->size, 0, 1, 0);
			break;
		}
		case TYPE_O: {
/* direct memory without ModR/M */
			op->type=X86_OPTYPE_MEM;
			op->size=esizeop(xop->size);
			op->mem.floatptr=isfloat(xop->size);
			op->mem.addrsize=insn.eaddrsize;
			switch (insn.eaddrsize) {
				case X86_ADDRSIZE16:
					op->mem.hasdisp=1;
					op->mem.disp=getword();
					break;
				case X86_ADDRSIZE32:
					op->mem.hasdisp=1;
					op->mem.disp=getdword();
					break;
			}
			op->mem.base=X86_REG_NO;
			op->mem.index=X86_REG_NO;
			op->mem.scale=1;
			break;
		}
		case TYPE_P: {
/* reg of ModR/M picks MMX register */
			op->type=X86_OPTYPE_MMX;
			op->size=8;
			op->mmx=mkreg(getmodrm());
			break;
		}
		case TYPE_Q: {
/* ModR/M (MMX reg or memory) */
			decode_modrm(op, xop->size, 1, 1, 1);
			break;
		}
		case TYPE_R: {
/* rm of ModR/M picks general register */
			op->type=X86_OPTYPE_REG;
			op->size=esizeop(xop->size);
			op->reg=mkrm(getmodrm());
			break;
		}
		case TYPE_Rx: {
/* extra picks register */
			op->type=X86_OPTYPE_REG;
			op->size=esizeop(xop->size);
			op->reg=xop->extra;
			break;
		}
		case TYPE_S: {
/* reg of ModR/M picks segment register */
			op->type=X86_OPTYPE_SEG;
			op->size=esizeop(xop->size);
			op->seg=mkreg(getmodrm());
			if (op->seg>5) invalidate();
			break;
		}
		case TYPE_Sx: {
/* extra picks segment register */
			op->type=X86_OPTYPE_SEG;
			op->size=esizeop(xop->size);
			op->seg=xop->extra;
			if (op->seg>5) invalidate();
			break;
		}
		case TYPE_T: {
/* reg of ModR/M picks test register */
			op->type=X86_OPTYPE_TRX;
			op->size=esizeop(xop->size);
			op->trx=mkreg(getmodrm());
			break;
		}
	}
}

void X86Disassembler::decode_sib(x86_insn_op *op, int mod)
{
	int sib=getsib();
	int scale=mkscale(sib);
	int index=mkindex(sib);
	int base=mkbase(sib);
	int disp=mod;
	if (base==5) {
		if (mod==0) {
			op->mem.base=X86_REG_NO;
			disp=2;
		} else {
			op->mem.base=X86_REG_BP;
		}
		op->mem.index=sibindex[index];
		op->mem.scale=sibscale[scale];
	} else {
		op->mem.base=sibbase[base];
		op->mem.index=sibindex[index];
		op->mem.scale=sibscale[scale];
	}
	switch (disp) {
		case 0:
			op->mem.hasdisp=0;
			op->mem.disp=0;
			break;
		case 1:
			op->mem.hasdisp=1;
			op->mem.disp=(char)getbyte();
			break;
		case 2:
			op->mem.hasdisp=1;
			op->mem.disp=getdword();
			break;
	}
}

dis_insn *X86Disassembler::duplicateInsn(dis_insn *disasm_insn)
{
	x86dis_insn *insn = (x86dis_insn *)malloc(sizeof (x86dis_insn));
	*insn = *(x86dis_insn *)disasm_insn;
	return insn;
}

int X86Disassembler::esizeaddr(char c)
{
	switch (c) {
		case SIZE_B:
			return 1;
		case SIZE_W:
			return 2;
		case SIZE_D:
			return 4;
		case SIZE_Q:
			return 8;
		case SIZE_S:
			return 4;
		case SIZE_L:
			return 8;
		case SIZE_T:
			return 10;
		case SIZE_C:
			if (insn.eaddrsize==X86_ADDRSIZE16) return 1; else return 2;
		case SIZE_V:
			if (insn.eaddrsize==X86_ADDRSIZE16) return 2; else return 4;
		case SIZE_P:
			if (insn.eaddrsize==X86_ADDRSIZE16) return 4; else return 6;
	}
	return 0;
}

int X86Disassembler::esizeop(char c)
{
	switch (c) {
		case SIZE_B:
			return 1;
		case SIZE_W:
			return 2;
		case SIZE_D:
			return 4;
		case SIZE_Q:
			return 8;
		case SIZE_S:
			return 4;
		case SIZE_L:
			return 8;
		case SIZE_T:
			return 10;
		case SIZE_C:
			if (insn.eopsize==X86_OPSIZE16) return 1; else return 2;
		case SIZE_V:
			if (insn.eopsize==X86_OPSIZE16) return 2; else return 4;
		case SIZE_P:
			if (insn.eopsize==X86_OPSIZE16) return 4; else return 6;
	}
	return 0;
}

byte X86Disassembler::getbyte()
{
	if (codep-ocodep+1<=maxlen) {
		addr+=1;
		return *(codep++);
	} else {
		invalidate();
		return 0;
	}
}

word X86Disassembler::getword()
{
	if (codep-ocodep+2<=maxlen) {
		word w;
		addr += 2;
		w = codep[0] | (codep[1]<<8);
		codep += 2;
		return w;
	} else {
		invalidate();
		return 0;
	}
}

dword X86Disassembler::getdword()
{
	if (codep-ocodep+4<=maxlen) {
		dword w;
		addr += 4;
		w = codep[0] | (codep[1]<<8) | (codep[2]<<16) | (codep[3]<<24);
		codep += 4;
		return w;
	} else {
		invalidate();
		return 0;
	}
}

void X86Disassembler::getOpcodeMetrics(int &min_length, int &max_length, int &min_look_ahead, int &avg_look_ahead, int &addr_align)
{
	min_length = 1;
	max_length = 15;
	min_look_ahead = 120;    // 1/2/3/4/5/6/8/10/12/15
	avg_look_ahead = 24;     // 1/2/3/4/6/8/12/24
	addr_align = 1;
}

int X86Disassembler::getmodrm()
{
	if (modrm==-1) modrm=getbyte();
	return modrm;
}

char *X86Disassembler::getName()
{
	return "x86/Disassembler";
}

int X86Disassembler::getsib()
{
	if (sib==-1) sib=getbyte();
	return sib;
}

byte X86Disassembler::getSize(dis_insn *disasm_insn)
{
	return ((x86dis_insn*)disasm_insn)->size;
}

void X86Disassembler::invalidate()
{
	insn.invalid=1;
}

int X86Disassembler::isfloat(char c)
{
	switch (c) {
		case SIZE_S:
		case SIZE_L:
		case SIZE_T:
			return 1;
	}
	return 0;
}

void X86Disassembler::prefixes()
{
	bool notprefix=0;
	insn.lockprefix=X86_PREFIX_NO;
	insn.repprefix=X86_PREFIX_NO;
	insn.segprefix=X86_PREFIX_NO;
	while ((codep-ocodep<15) && (!notprefix)) {
		c=getbyte();
		switch (c) {
			case 0x26:
				insn.segprefix=X86_PREFIX_ES;
				break;
			case 0x2e:
				insn.segprefix=X86_PREFIX_CS;
				break;
			case 0x36:
				insn.segprefix=X86_PREFIX_SS;
				break;
			case 0x3e:
				insn.segprefix=X86_PREFIX_DS;
				break;
			case 0x64:
				insn.segprefix=X86_PREFIX_FS;
				break;
			case 0x65:
				insn.segprefix=X86_PREFIX_GS;
				break;
			case 0x66:
				insn.eopsize=(opsize==X86_OPSIZE16) ? X86_OPSIZE32 : X86_OPSIZE16;
				break;
			case 0x67:
				insn.eaddrsize=(addrsize==X86_ADDRSIZE16) ? X86_ADDRSIZE32 : X86_ADDRSIZE16;
				break;
			case 0xf0:
				insn.lockprefix=X86_PREFIX_LOCK;
				break;
			case 0xf2:
				insn.repprefix=X86_PREFIX_REPNZ;
				break;
			case 0xf3:
				insn.repprefix=X86_PREFIX_REPZ;
				break;
			default:
				notprefix=1;
		}
	}
}

int X86Disassembler::special_param_ambiguity(x86dis_insn *disasm_insn)
{
	int regc=0, memc=0, segc=0;
	for (int i=0; i<3; i++) {
		switch (disasm_insn->op[i].type) {
			case X86_OPTYPE_SEG:
				segc++;
			case X86_OPTYPE_REG:
			case X86_OPTYPE_CRX:
			case X86_OPTYPE_DRX:
			case X86_OPTYPE_TRX:
			case X86_OPTYPE_STX:
			case X86_OPTYPE_MMX:
				regc++;
				break;
			case X86_OPTYPE_MEM:
				memc++;
				break;
		}
	}
	return (memc && !regc)
		|| (memc && segc)
		|| (strcmp(disasm_insn->name, "movzx") == 0)
		|| (strcmp(disasm_insn->name, "movsx") == 0);
}

void X86Disassembler::str_op(char *opstr, int *opstrlen, x86dis_insn *insn, x86_insn_op *op, bool explicit_params)
{
	const char *cs_default = get_cs(e_cs_default);
	const char *cs_number = get_cs(e_cs_number);
	const char *cs_symbol = get_cs(e_cs_symbol);
	
	*opstrlen=0;
	switch (op->type) {
		case X86_OPTYPE_IMM: {
			CPU_ADDR a;
			// FIXME: hack
			if ((insn->name[0]=='j') || strcmp(insn->name, "call")==0) {
				a.addr32.seg = seg;
			} else {
				a.addr32.seg = 0;
			}
			a.addr32.offset = op->imm;
			int slen;
			char *s=(addr_sym_func) ? addr_sym_func(a, &slen, addr_sym_func_context) : NULL;
			if (s) {
				memmove(opstr, s, slen);
				opstr[slen]=0;
				*opstrlen=slen;
			} else {
				char *g=opstr;
				strcpy(g, cs_number); g += strlen(cs_number);
				switch (op->size) {
					case 1:
						hexd(&g, 2, options, op->imm);
						break;
					case 2:
						hexd(&g, 4, options, op->imm);
						break;
					case 4:
						hexd(&g, 8, options, op->imm);
						break;
				}
				strcpy(g, cs_default); g += strlen(cs_default);
			}
			break;
		}
		case X86_OPTYPE_REG: {
			int j;
			switch (op->size) {
				case 1: j=0; break;
				case 2: j=1; break;
				case 4: j=2; break;
				default:
					j=-1;
			}
			if (j!=-1) sprintf(opstr, x86_regs[j][op->reg]);
			break;
		}
		case X86_OPTYPE_SEG:
			if (x86_segs[op->seg]) {
			    sprintf(opstr, x86_segs[op->seg]);
			}
			break;
		case X86_OPTYPE_CRX:
			sprintf(opstr, "cr%d", op->crx);
			break;
		case X86_OPTYPE_DRX:
			sprintf(opstr, "dr%d", op->drx);
			break;
		case X86_OPTYPE_TRX:
			sprintf(opstr, "tr%d", op->trx);
			break;
		case X86_OPTYPE_STX:
			if (op->stx) {
				sprintf(opstr, "st(%d)", op->stx);
			} else {
				strcpy(opstr, "st");
			}
			break;
		case X86_OPTYPE_MMX:
			sprintf(opstr, "mm%d", op->mmx);
			break;
		case X86_OPTYPE_MEM: {
			char *d=opstr;
			int reg;
			if (explicit_params) {
				if (op->mem.floatptr) {
					switch (op->size) {
						case 4:
							d+=sprintf(d, "single ptr ");
							break;
						case 8:
							d+=sprintf(d, "double ptr ");
							break;
						case 10:
							d+=sprintf(d, "extended ptr ");
							break;
					}
				} else {
					switch (op->size) {
						case 1:
							d+=sprintf(d, "byte ptr ");
							break;
						case 2:
							d+=sprintf(d, "word ptr ");
							break;
						case 4:
							d+=sprintf(d, "dword ptr ");
							break;
						case 6:
							d+=sprintf(d, "pword ptr ");
							break;
						case 8:
							d+=sprintf(d, "qword ptr ");
							break;
					}
				}
			}
			if (insn->eaddrsize==X86_ADDRSIZE16) reg=1; else reg=2;
			if (insn->segprefix!=X86_PREFIX_NO) d+=sprintf(d, "%s%s:%s", x86_segs[insn->segprefix], cs_symbol, cs_default);
			strcpy(d, cs_symbol); d += strlen(cs_symbol);
			*(d++)='[';
			strcpy(d, cs_default); d += strlen(cs_default);

			int optimize_addr=options & X86DIS_STYLE_OPTIMIZE_ADDR;
			int first=1;
			if (optimize_addr && (op->mem.base!=X86_REG_NO) && (op->mem.base==op->mem.index)) {
				d+=sprintf(d, "%s%s*%s%d%s", x86_regs[reg][op->mem.index], cs_symbol, cs_number, op->mem.scale+1, cs_default);
				first=0;
			} else {
				if (op->mem.base!=X86_REG_NO) {
					d+=sprintf(d, "%s", x86_regs[reg][op->mem.base]);
					first=0;
				}
				if (op->mem.index!=X86_REG_NO) {
					if (!first) *(d++)='+';
					if ((op->mem.scale==1)) {
						d+=sprintf(d, "%s", x86_regs[reg][op->mem.index]);
					} else {
						d+=sprintf(d, "%s%s*%s%d%s", x86_regs[reg][op->mem.index], cs_symbol, cs_number, op->mem.scale, cs_default);
					}
					first=0;
				}
			}
			if ((!optimize_addr && op->mem.hasdisp) || (optimize_addr && op->mem.disp) || first) {
				CPU_ADDR a;
				a.addr32.seg = 0; // FIXME: not ok
				a.addr32.offset=op->mem.disp;
				int slen;
				char *s=(addr_sym_func) ? addr_sym_func(a, &slen, addr_sym_func_context) : 0;
				if (s) {
					if (!first) {
						strcpy(d, cs_symbol); d += strlen(cs_symbol);
						*(d++)='+';
						strcpy(d, cs_default); d += strlen(cs_default);
					}
					memmove(d, s, slen);
					d+=slen;
					*opstrlen=d-opstr;
				} else {
					dword q;
					switch (op->mem.addrsize) {
						case X86_ADDRSIZE16:
							q=op->mem.disp;
							if (!first) {
								strcpy(d, cs_symbol); d += strlen(cs_symbol);
								if (op->mem.disp&0x80000000) {
									*(d++)='-';
									q=-q;
								} else *(d++)='+';
							}
							strcpy(d, cs_number); d += strlen(cs_number);
							hexd(&d, 4, options, q);
							strcpy(d, cs_default); d += strlen(cs_default);
							break;
						case X86_ADDRSIZE32:
							q=op->mem.disp;
							if (!first) {
								strcpy(d, cs_symbol); d += strlen(cs_symbol);
								if (op->mem.disp&0x80000000) {
									*(d++)='-';
									q=-q;
								} else *(d++)='+';
							}
							strcpy(d, cs_number); d += strlen(cs_number);
							hexd(&d, 8, options, q);
							strcpy(d, cs_default); d += strlen(cs_default);
							break;
					}
				}
			}
			strcpy(d, cs_symbol); d += strlen(cs_symbol);
			*(d++)=']';
			strcpy(d, cs_default); d += strlen(cs_default);
			if (*opstrlen)
				*opstrlen += strlen(cs_symbol) + 1 + strlen(cs_default);
			*d=0;
			break;
		}
		case X86_OPTYPE_FARPTR: {
			CPU_ADDR a;
/*			a.addr32.seg = op->farptr.seg;
			a.addr32.offset = op->farptr.offset;
*/
			a.addr32.seg = op->farptr.seg;
			a.addr32.offset = op->farptr.offset;
			int slen;
			char *s=(addr_sym_func) ? addr_sym_func(a, &slen, addr_sym_func_context) : 0;
			if (s) {
				memmove(opstr, s, slen);
				opstr[slen]=0;
				*opstrlen=slen;
			} else {
				char *g=opstr;
				hexd(&g, 4, options, op->farptr.seg);
				strcpy(g, cs_symbol); g += strlen(cs_symbol);
				*(g++)=':';
				strcpy(g, cs_default); g += strlen(cs_default);
				switch (op->size) {
					case 4:
						hexd(&g, 4, options, op->farptr.offset);
						break;
					case 6:
						hexd(&g, 8, options, op->farptr.offset);
						break;
				}
			}
			break;
		}
		default:
			opstr[0]=0;
	}
}

void X86Disassembler::str_format(char **str, char **format, char *p, char *n, char *op[3], int oplen[3], char stopchar, int print)
{
	const char *cs_default = get_cs(e_cs_default);
	const char *cs_symbol = get_cs(e_cs_symbol);

	char *f=*format;
	char *s=*str;
	while (*f) {
		if (*f==stopchar) break;
		switch (*f) {
			case '\t':
				if (print) do *(s++)=' '; while ((s-insnstr) % DIS_STYLE_TABSIZE);
				break;
			case DISASM_STRF_VAR:
				f++;
				if (print) {
					char *t=0;
					int tl=0;
					switch (*f) {
						case DISASM_STRF_PREFIX:
							t=p;
							break;
						case DISASM_STRF_NAME:
							t=n;
							break;
						case DISASM_STRF_FIRST:
							t=op[0];
							tl=oplen[0];
							break;
						case DISASM_STRF_SECOND:
							t=op[1];
							tl=oplen[1];
							break;
						case DISASM_STRF_THIRD:
							t=op[2];
							tl=oplen[2];
							break;
					}
					if (tl) {
						memmove(s, t, tl);
						s+=tl;
						*s=0;
					} else {
						strcpy(s, t);
						s += strlen(s);
					}
				}
				break;
			case DISASM_STRF_COND: {
				char *t=0;
				f++;
				switch (*f) {
					case DISASM_STRF_PREFIX:
						t=p;
						break;
					case DISASM_STRF_NAME:
						t=n;
						break;
					case DISASM_STRF_FIRST:
						t=op[0];
						break;
					case DISASM_STRF_SECOND:
						t=op[1];
						break;
					case DISASM_STRF_THIRD:
						t=op[2];
						break;
				}
				f+=2;
				if ((t) && (t[0])) {
					str_format(&s, &f, p, n, op, oplen, *(f-1), 1);
				} else {
					str_format(&s, &f, p, n, op, oplen, *(f-1), 0);
				}
				break;
			}
			default:
				if (print) {
					bool x = (strchr(",.-=+-*/[]()", *f) != NULL) && *f;
					if (x) { strcpy(s, cs_symbol); s += strlen(cs_symbol); }
					*(s++) = *f;
					if (x) { strcpy(s, cs_default); s += strlen(cs_default); }
				}
		}
		f++;
	}
	*s=0;
	*format=f;
	*str=s;
}

char *X86Disassembler::str(dis_insn *disasm_insn, int options)
{
	return strf(disasm_insn, options, DISASM_STRF_DEFAULT_FORMAT);
}

char *X86Disassembler::strf(dis_insn *disasm_insn, int opt, char *format)
{
	x86dis_insn *insn = (x86dis_insn*)disasm_insn;
	char prefix[64];
	char *p = prefix;
	options = opt;
	*p = 0;
	if (insn->lockprefix == X86_PREFIX_LOCK) p+=sprintf(p, "lock ");
	if (insn->repprefix == X86_PREFIX_REPZ) {
		p += sprintf(p, "repz ");
	} else if (insn->repprefix == X86_PREFIX_REPNZ) {
		p += sprintf(p, "repnz ");
	}
	if ((p != prefix) && (*(p-1) == ' ')) {
		p--;
		*p = 0;
	}

	bool explicit_params = ((options & X86DIS_STYLE_EXPLICIT_MEMSIZE) ||
		special_param_ambiguity(insn));

	char ops[3][512];	/* FIXME: possible buffer overflow ! */
	char *op[3];
	int oplen[3];

	if (options & DIS_STYLE_HIGHLIGHT) enable_highlighting();
	for (int i=0; i<3; i++) {
		op[i]=(char*)&ops[i];
		str_op(op[i], &oplen[i], insn, &insn->op[i], explicit_params);
	}
	char *s=insnstr;
	char n[32];
	switch (insn->eopsize) {
		case X86_OPSIZE16:
			sprintf(n, insn->name, 'w');
			break;
		case X86_OPSIZE32:
			sprintf(n, insn->name, 'd');
			break;
		default:
			strcpy(n, insn->name);
			break;
	}
	str_format(&s, &format, prefix, n, op, oplen, 0, 1);
	disable_highlighting();
	return insnstr;
}

bool X86Disassembler::validInsn(dis_insn *disasm_insn)
{
	return !((x86dis_insn *)disasm_insn)->invalid;
}

