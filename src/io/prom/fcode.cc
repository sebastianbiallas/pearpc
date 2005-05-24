/*
 *	PearPC
 *	fcode.cc
 *
 *	Copyright (C) 2003 Sebastian Biallas (sb@biallas.net)
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

#include "fcode.h"

// .104
void fcode_opc_ex(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_sharp(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .105
void fcode_opc_sharpgt(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_mul(ForthVM &vm)
{
	uint32 a = vm.dataPop();
	uint32 b = vm.dataPop();
	vm.dataPush(b*a);
}
void fcode_opc_plus(ForthVM &vm)
{
	uint32 a = vm.dataPop();
	uint32 b = vm.dataPop();
	vm.dataPush(b+a);
}
void fcode_opc_plusex(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_comma(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_minus(ForthVM &vm)
{
	uint32 a = vm.dataPop();
	uint32 b = vm.dataPop();
	vm.dataPush(b-a);
}
void fcode_opc_dot(ForthVM &vm)
{
	uint32 t = vm.dataPop();
	vm.outf("%d ", t);
}
// .106
void fcode_opc_div(ForthVM &vm)
{
	sint32 a = vm.dataPop();
	sint32 b = vm.dataPop();
	if (a==0) throw ForthRunException(vm.mErrorPos, "division by zero");
	vm.dataPush(b/a);
}
void fcode_opc_lt(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ltsharp(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ltequal(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ltgt(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_equal(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_gt(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_gtequal(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .107
void fcode_opc_at(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_0(ForthVM &vm)
{
	vm.dataPush(0);
}
void fcode_opc_0lt(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_0ltequal(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_0ltgt(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_0equal(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_0gt(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .108
void fcode_opc_0gtequal(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_1(ForthVM &vm)
{
	vm.dataPush(1);
}
void fcode_opc_minus1(ForthVM &vm)
{
	vm.dataPush((uint)-1);
}
void fcode_opc_2(ForthVM &vm)
{
	vm.dataPush(2);
}
void fcode_opc_2ex(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_2mul(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_2div(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_2at(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_3(ForthVM &vm)
{
	vm.dataPush(3);
}
// .109
void fcode_opc_gtgtA(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ABORT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ABS(ForthVM &vm)
{
	sint32 a = vm.dataPop();
	vm.dataPush((a<0)?-a:a);
}
// .111
void fcode_opc_ALARM(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ALIGNED(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ALLOC_MEM(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .112
void fcode_opc_AND(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .113
void fcode_opc_Bstring(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Btick(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Bcolon(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Bsemincolon(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .114
void fcode_opc_BASE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BBRANCH(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .115
void fcode_opc_BqBRANCH(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BBUFFERcol(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BCASE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .116
void fcode_opc_BCONSTANT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BCREATE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BDEFER(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BDO(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BqDO(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .117
void fcode_opc_BEHAVIOR(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BELL(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BENDCASE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .118
void fcode_opc_BENDOF(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BETWEEN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BFIELD(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BL(ForthVM &vm)
{
	vm.outf(" ");
}
void fcode_opc_BLEAVE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BLINK_SCREEN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .119
void fcode_opc_BLIT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BLJOIN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BLOOP(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BplusLOOP(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .120
void fcode_opc_BltMARK(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BODYgt(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_gtBODY(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BOF(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .121
void fcode_opc_BOUNDS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BgtRESOLVE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .122
void fcode_opc_BTO(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BVALUE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BVARIABLE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_BWJOIN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .123
void fcode_opc_BYTE_LOAD(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Cex(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Ccomma(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .124
void fcode_opc_slashC(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Cat(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_CAplus(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .125
void fcode_opc_dCALL_METHOD(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_CALL_PACKAGE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_dCALL_PARENT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .126
void fcode_opc_CATCH(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_CELLplus(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_CELLS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_CHARplus(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .127
void fcode_opc_CHAR_HEIGHT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_CHARS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_CHAR_WIDTH(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_CHILD(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .128
void fcode_opc_CLOSE_PACKAGE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_sharpCOLUMN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_sharpCOLUMNS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_COMP(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .129
void fcode_opc_COMPILEcomma(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .130
void fcode_opc_COUNT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_CPEEK(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_CPOKE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_CR(ForthVM &vm)
{
	vm.outf("\n");
}
void fcode_opc_bracketCR(ForthVM &vm)
{
	vm.outf("\r");
}
// .131
void fcode_opc_Dplus(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Dminus(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .132
void fcode_opc_DECODE_INT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_DECODE_PHYS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_DECODE_STRING(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_DEFAULT_FONT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .133
void fcode_opc_DELETE_CHARS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_DELETE_LINES(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_DELETE_PROPERTY(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_DEPTH(ForthVM &vm)
{
	vm.dataPush(vm.dataDepth());
}
// .134
void fcode_opc_DEVICE_NAME(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_DEVICE_TYPE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .135
void fcode_opc_DIAGNOSTIC_MODEq(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_DIGIT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .138
void fcode_opc_DRAW_CHARACTER(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_DRAW_LOGO(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_DROP(ForthVM &vm)
{
	vm.dataPop();
}
void fcode_opc_2DROP(ForthVM &vm)
{
	vm.dataPop();
	vm.dataPop();
}
// .139
void fcode_opc_DUP(ForthVM &vm)
{
	uint32 t = vm.dataGet();
	vm.dataPush(t);
}
void fcode_opc_2DUP(ForthVM &vm)
{
	uint32 t1 = vm.dataGet(0);
	uint32 t2 = vm.dataGet(1);
	vm.dataPush(t2);
	vm.dataPush(t1);
}
void fcode_opc_qDUP(ForthVM &vm)
{
	uint32 t = vm.dataGet();
	if (t) vm.dataPush(t);
}
void fcode_opc_EMIT(ForthVM &vm)
{
	vm.outf("%c", vm.dataPop());
}
void fcode_opc_ENCODEplus(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ENCODE_BYTES(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .140
void fcode_opc_ENCODE_INT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ENCODE_PHYS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ENCODE_STRING(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .141
void fcode_opc_END0(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_END1(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .142
void fcode_opc_ERASE_SCREEN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_EVALUATE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_EXECUTE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_EXIT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .143
void fcode_opc_EXPECT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_EXTERNAL_TOKEN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .144
void fcode_opc_FB8_BLINK_SCREEN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FB8_DELETE_CHARS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FB8_DELETE_LINES(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FB8_DRAW_CHARACTER(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FB8_DRAW_LOGO(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FB8_ERASE_SCREEN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FB8_INSERT_CHARACTERS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FB8_INSERT_LINES(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FB8_INSTALL(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FB8_INVERT_SCREEN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .145
void fcode_opc_FB8_RESET_SCREEN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FB8_TOGGLE_CURSOR(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FCODE_REVISION(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FERROR(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .146
void fcode_opc_FILL(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_dFIND(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FIND_METHOD(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FIND_PACKAGE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .147
void fcode_opc_FINISH_DEVICE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_gtFONT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FONTBYTES(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FRAME_BUFFER_ADR(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FREE_MEM(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_FREE_VIRTUAL(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .148
void fcode_opc_GET_INHERITED_PROPERTIY(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_GET_MSECS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_GET_MY_PROPERTY(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_GET_PACKAGE_PROPERTY(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_GET_TOKEN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .151
void fcode_opc_HERE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .150
void fcode_opc_HOLD(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_I(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_IHANDLEgtPHANDLE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .151
void fcode_opc_INSERT_CHARACTERS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_INSERT_LINES(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .152
void fcode_opc_INSTANCE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .153
void fcode_opc_INVERSEq(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_INVERSE_SCREENq(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_INVERT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .154
void fcode_opc_INVERT_SCREEN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_IS_INSTALL(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_IS_REMOVE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_IS_SELFTEST(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_bIS_USER_WORDd(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .155
void fcode_opc_J(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_KEY(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_KEYq(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Lex(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Lcomma(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Lat(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_divL(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_divLmul(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_LAplus(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_LA1plus(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .156
void fcode_opc_LBFLIP(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_LBFLIPS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_LBSPLIT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_LCC(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_LEFT_PARSE_STRING(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .157
void fcode_opc_LINEsharp(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_sharpLINE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_sharpLINES(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .159
void fcode_opc_LPEEK(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_LPOKE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_LSHIFT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_LWFLIP(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_LWFLIPS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_LWSPLIT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_MAC_ADDRESS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .160
void fcode_opc_MAP_LOW(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_MASK(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .161
void fcode_opc_MAX(ForthVM &vm)
{
	sint32 a = vm.dataPop();
	sint32 b = vm.dataPop();
	vm.dataPush(MAX(a,b));
}
void fcode_opc_MEMORY_TEST_SUITE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_MIN(ForthVM &vm)
{
	sint32 a = vm.dataPop();
	sint32 b = vm.dataPop();
	vm.dataPush(MIN(a,b));
}
// .162
void fcode_opc_MOD(ForthVM &vm)
{
	sint32 a = vm.dataPop();
	sint32 b = vm.dataPop();
	if (a==0) throw ForthRunException(vm.mErrorPos, "division by zero");
	vm.dataPush(b%a);
}
void fcode_opc_divMOD(ForthVM &vm)
{
	sint32 a = vm.dataPop();
	sint32 b = vm.dataPop();
	if (a==0) throw ForthRunException(vm.mErrorPos, "division by zero");
	vm.dataPush(b%a);
	vm.dataPush(b/a);
}
void fcode_opc_MODEL(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_MOVE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_MS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .163
void fcode_opc_MY_ADDRESS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_MY_ARGS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_MY_PARENT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_MY_SELF(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_MY_SPACE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_MY_UNIT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_slashN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .164
void fcode_opc_NAplus(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .165
void fcode_opc_NAMED_TOKEN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_NEGATE(ForthVM &vm)
{
	vm.dataPush(-vm.dataPop());
}
void fcode_opc_NEW_DEVICE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .166
void fcode_opc_NEW_TOKEN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_NEXT_PROPERTY(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_NIP(ForthVM &vm)
{
	uint32 t = vm.dataPop();
	vm.dataPop();
	vm.dataPush(t);
}
void fcode_opc_NOOP(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .167
void fcode_opc_dNUMBER(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .169
void fcode_opc_OFF(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .170
void fcode_opc_OFFSET16(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ON(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_OPEN_PACKAGE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .171
void fcode_opc_dOPEN_PACKAGE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_OR(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_sharpOUT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_OVER(ForthVM &vm)
{
	vm.dataPush(vm.dataGet(1));
}
void fcode_opc_2OVER(ForthVM &vm)
{
	uint32 t1 = vm.dataGet(2);
	uint32 t2 = vm.dataGet(3);
	vm.dataPush(t2);
	vm.dataPush(t1);
}
void fcode_opc_PACK(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .172
void fcode_opc_PARSE_2INT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .173
void fcode_opc_PEER(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_PICK(ForthVM &vm)
{
	uint32 u = vm.dataPop();
	uint32 t = vm.dataGet(u);
	vm.dataPush(t);
}
// .174
void fcode_opc_PROPERTY(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Rgt(ForthVM &vm)
{
	vm.dataPush(vm.codePop());
}
void fcode_opc_Rat(ForthVM &vm)
{
	vm.dataPush(vm.codeGet());
}
void fcode_opc_dotR(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_gtR(ForthVM &vm)
{
	vm.codePush(vm.dataPop());
}
// .175
void fcode_opc_RBex(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_RBat(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .177
void fcode_opc_REG(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .179
void fcode_opc_RESET_SCREEN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_RLex(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .180
void fcode_opc_RLat(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ROLL(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ROT(ForthVM &vm)
{
	uint32 t3 = vm.dataPop();
	uint32 t2 = vm.dataPop();
	uint32 t1 = vm.dataPop();
	vm.dataPush(t2);
	vm.dataPush(t3);
	vm.dataPush(t1);
}
void fcode_opc_mROT(ForthVM &vm)
{
	uint32 t3 = vm.dataPop();
	uint32 t2 = vm.dataPop();
	uint32 t1 = vm.dataPop();
	vm.dataPush(t3);
	vm.dataPush(t1);
	vm.dataPush(t2);
}
void fcode_opc_2ROT(ForthVM &vm)
{
	uint32 t6 = vm.dataPop();
	uint32 t5 = vm.dataPop();
	uint32 t4 = vm.dataPop();
	uint32 t3 = vm.dataPop();
	uint32 t2 = vm.dataPop();
	uint32 t1 = vm.dataPop();
	vm.dataPush(t3);
	vm.dataPush(t4);
	vm.dataPush(t5);
	vm.dataPush(t6);
	vm.dataPush(t1);
	vm.dataPush(t2);
}
void fcode_opc_RSHIFT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_RWex(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .181
void fcode_opc_RWat(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_sharpS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_dotS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .182
void fcode_opc_SBUS_INTRgtCPU(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_SCREEN_HEIGHT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_SCREEN_WIDTH(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .184
void fcode_opc_SET_ARGS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .185
void fcode_opc_SET_FONT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_SIGN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_SPAN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .187
void fcode_opc_START0(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_START1(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_START2(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_START4(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_STATE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .189
void fcode_opc_SUSPEND_FCODE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_SWAP(ForthVM &vm)
{
	uint32 t2 = vm.dataPop();
	uint32 t1 = vm.dataPop();
	vm.dataPush(t2);
	vm.dataPush(t1);
}
void fcode_opc_2SWAP(ForthVM &vm)
{
	uint32 t4 = vm.dataPop();
	uint32 t3 = vm.dataPop();
	uint32 t2 = vm.dataPop();
	uint32 t1 = vm.dataPop();
	vm.dataPush(t3);
	vm.dataPush(t4);
	vm.dataPush(t1);
	vm.dataPush(t2);
}
// .190
void fcode_opc_THROW(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .191
void fcode_opc_TOGGLE_CURSOR(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_TUCK(ForthVM &vm)
{
	uint32 t1 = vm.dataGet(0);
	uint32 t2 = vm.dataGet(1);
	vm.dataPush(t1);
	vm.dataPush(t2);
	vm.dataPush(t1);	
}
void fcode_opc_TYPE(ForthVM &vm)
{
	uint32 len = vm.dataPop();
	uint32 str = vm.dataPop();
	const byte *p = (const byte*)vm.dataStr(str, true);
	String bla(p, len);
	vm.outf("%y", &bla);
}
// .192
void fcode_opc_Usharp(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Usharpgt(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_UsharpS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Udot(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Ult(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Ultequal(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Ugt(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Ugtequal(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_U2div(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_UMmul(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_UMdivMOD(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .193
void fcode_opc_UdivMOD(ForthVM &vm)
{
	uint32 a = vm.dataPop();
	uint32 b = vm.dataPop();
	if (a==0) throw ForthRunException(vm.mErrorPos, "division by zero");
	vm.dataPush(b%a);
	vm.dataPush(b/a);
}
void fcode_opc_UNLOOP(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_UPC(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_UdotR(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .194
void fcode_opc_USER_ABORT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_VERSION1(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Wex(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Wcomma(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_Wat(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_slashW(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .195
void fcode_opc_slashWmul(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_ltWat(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_WAplus(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_WA1plus(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_WBFLIP(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_WBFLIPS(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_WBSPLIT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_WINDOW_LEFT(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_WINDOW_TOP(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_WITHIN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
// .196
void fcode_opc_WLJOIN(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_WPEEK(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_WPOKE(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}
void fcode_opc_XOR(ForthVM &vm)
{
	throw ForthRunException(vm.mErrorPos, "not implemented: '%s'", vm.mCurToken);
}


/*
 *	...
 */
void fcode_opc_CLEAR(ForthVM &vm)
{
	vm.dataClear();
}

ForthWordTick::ForthWordTick(const char *name)
	:ForthWord(name)
{
}

void ForthWordTick::compile(ForthVM &vm)
{
}

void ForthWordTick::interprete(ForthVM &vm)
{
}
