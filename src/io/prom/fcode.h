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

#ifndef __FCODE_H__
#define __FCODE_H__

#include "system/types.h"
#include "forth.h"

#define FCODEHEADER_FORMAT 8

// .49
struct FCodeHeader {
	uint8	start;
	uint8	format;
	uint8	chksum_high;
	uint8	chksum_low;
	uint8	len[4];
};

class ForthWordTick: public ForthWord {
public:
			ForthWordTick(const char *name);
	virtual void	compile(ForthVM &vm);
	virtual void	interprete(ForthVM &vm);
};

class ForthWordIF: public ForthWord {
public:
			ForthWordIF();
	virtual void	compile(ForthVM &vm);
	virtual void	interprete(ForthVM &vm);
};

class ForthWordELSE: public ForthWord {
public:
			ForthWordELSE();
	virtual void	compile(ForthVM &vm);
	virtual void	interprete(ForthVM &vm);
};

class ForthWordTHEN: public ForthWord {
public:
			ForthWordTHEN();
	virtual void	compile(ForthVM &vm);
	virtual void	interprete(ForthVM &vm);
};



// .104
void fcode_opc_ex(ForthVM &vm);
void fcode_opc_sharp(ForthVM &vm);
// .105
void fcode_opc_sharpgt(ForthVM &vm);
void fcode_opc_mul(ForthVM &vm);
void fcode_opc_plus(ForthVM &vm);
void fcode_opc_plusex(ForthVM &vm);
void fcode_opc_comma(ForthVM &vm);
void fcode_opc_minus(ForthVM &vm);
void fcode_opc_dot(ForthVM &vm);
// .106
void fcode_opc_div(ForthVM &vm);
void fcode_opc_lt(ForthVM &vm);
void fcode_opc_ltsharp(ForthVM &vm);
void fcode_opc_ltequal(ForthVM &vm);
void fcode_opc_ltgt(ForthVM &vm);
void fcode_opc_equal(ForthVM &vm);
void fcode_opc_gt(ForthVM &vm);
void fcode_opc_gtequal(ForthVM &vm);
// .107
void fcode_opc_at(ForthVM &vm);
void fcode_opc_0(ForthVM &vm);
void fcode_opc_0lt(ForthVM &vm);
void fcode_opc_0ltequal(ForthVM &vm);
void fcode_opc_0ltgt(ForthVM &vm);
void fcode_opc_0equal(ForthVM &vm);
void fcode_opc_0gt(ForthVM &vm);
// .108
void fcode_opc_0gtequal(ForthVM &vm);
void fcode_opc_1(ForthVM &vm);
void fcode_opc_minus1(ForthVM &vm);
void fcode_opc_2(ForthVM &vm);
void fcode_opc_2ex(ForthVM &vm);
void fcode_opc_2mul(ForthVM &vm);
void fcode_opc_2div(ForthVM &vm);
void fcode_opc_2at(ForthVM &vm);
void fcode_opc_3(ForthVM &vm);
// .109
void fcode_opc_gtgtA(ForthVM &vm);
void fcode_opc_ABORT(ForthVM &vm);
void fcode_opc_ABS(ForthVM &vm);
// .111
void fcode_opc_ALARM(ForthVM &vm);
void fcode_opc_ALIGNED(ForthVM &vm);
void fcode_opc_ALLOC_MEM(ForthVM &vm);
// .112
void fcode_opc_AND(ForthVM &vm);
// .113
void fcode_opc_Bstring(ForthVM &vm);
void fcode_opc_Btick(ForthVM &vm);
void fcode_opc_Bcolon(ForthVM &vm);
void fcode_opc_Bsemincolon(ForthVM &vm);
// .114
void fcode_opc_BASE(ForthVM &vm);
void fcode_opc_BBRANCH(ForthVM &vm);
// .115
void fcode_opc_BqBRANCH(ForthVM &vm);
void fcode_opc_BBUFFERcol(ForthVM &vm);
void fcode_opc_BCASE(ForthVM &vm);
// .116
void fcode_opc_BCONSTANT(ForthVM &vm);
void fcode_opc_BCREATE(ForthVM &vm);
void fcode_opc_BDEFER(ForthVM &vm);
void fcode_opc_BDO(ForthVM &vm);
void fcode_opc_BqDO(ForthVM &vm);
// .117
void fcode_opc_BEHAVIOR(ForthVM &vm);
void fcode_opc_BELL(ForthVM &vm);
void fcode_opc_BENDCASE(ForthVM &vm);
// .118
void fcode_opc_BENDOF(ForthVM &vm);
void fcode_opc_BETWEEN(ForthVM &vm);
void fcode_opc_BFIELD(ForthVM &vm);
void fcode_opc_BL(ForthVM &vm);
void fcode_opc_BLEAVE(ForthVM &vm);
void fcode_opc_BLINK_SCREEN(ForthVM &vm);
// .119
void fcode_opc_BLIT(ForthVM &vm);
void fcode_opc_BLJOIN(ForthVM &vm);
void fcode_opc_BLOOP(ForthVM &vm);
void fcode_opc_BplusLOOP(ForthVM &vm);
// .120
void fcode_opc_BltMARK(ForthVM &vm);
void fcode_opc_BODYgt(ForthVM &vm);
void fcode_opc_gtBODY(ForthVM &vm);
void fcode_opc_BOF(ForthVM &vm);
// .121
void fcode_opc_BOUNDS(ForthVM &vm);
void fcode_opc_BgtRESOLVE(ForthVM &vm);
void fcode_opc_BS(ForthVM &vm);
// .122
void fcode_opc_BTO(ForthVM &vm);
void fcode_opc_BVALUE(ForthVM &vm);
void fcode_opc_BVARIABLE(ForthVM &vm);
void fcode_opc_BWJOIN(ForthVM &vm);
// .123
void fcode_opc_BYTE_LOAD(ForthVM &vm);
void fcode_opc_Cex(ForthVM &vm);
void fcode_opc_Ccomma(ForthVM &vm);
// .124
void fcode_opc_slashC(ForthVM &vm);
void fcode_opc_Cat(ForthVM &vm);
void fcode_opc_CAplus(ForthVM &vm);
// .125
void fcode_opc_dCALL_METHOD(ForthVM &vm);
void fcode_opc_CALL_PACKAGE(ForthVM &vm);
void fcode_opc_dCALL_PARENT(ForthVM &vm);
// .126
void fcode_opc_CATCH(ForthVM &vm);
void fcode_opc_CELLplus(ForthVM &vm);
void fcode_opc_CELLS(ForthVM &vm);
void fcode_opc_CHARplus(ForthVM &vm);
// .127
void fcode_opc_CHAR_HEIGHT(ForthVM &vm);
void fcode_opc_CHARS(ForthVM &vm);
void fcode_opc_CHAR_WIDTH(ForthVM &vm);
void fcode_opc_CHILD(ForthVM &vm);
// .128
void fcode_opc_CLOSE_PACKAGE(ForthVM &vm);
void fcode_opc_sharpCOLUMN(ForthVM &vm);
void fcode_opc_sharpCOLUMNS(ForthVM &vm);
void fcode_opc_COMP(ForthVM &vm);
// .129
void fcode_opc_COMPILEcomma(ForthVM &vm);
// .130
void fcode_opc_COUNT(ForthVM &vm);
void fcode_opc_CPEEK(ForthVM &vm);
void fcode_opc_CPOKE(ForthVM &vm);
void fcode_opc_CR(ForthVM &vm);
void fcode_opc_bracketCR(ForthVM &vm);
// .131
void fcode_opc_Dplus(ForthVM &vm);
void fcode_opc_Dminus(ForthVM &vm);
// .132
void fcode_opc_DECODE_INT(ForthVM &vm);
void fcode_opc_DECODE_PHYS(ForthVM &vm);
void fcode_opc_DECODE_STRING(ForthVM &vm);
void fcode_opc_DEFAULT_FONT(ForthVM &vm);
// .133
void fcode_opc_DELETE_CHARS(ForthVM &vm);
void fcode_opc_DELETE_LINES(ForthVM &vm);
void fcode_opc_DELETE_PROPERTY(ForthVM &vm);
void fcode_opc_DEPTH(ForthVM &vm);
// .134
void fcode_opc_DEVICE_NAME(ForthVM &vm);
void fcode_opc_DEVICE_TYPE(ForthVM &vm);
// .135
void fcode_opc_DIAGNOSTIC_MODEq(ForthVM &vm);
void fcode_opc_DIGIT(ForthVM &vm);
// .138
void fcode_opc_DRAW_CHARACTER(ForthVM &vm);
void fcode_opc_DRAW_LOGO(ForthVM &vm);
void fcode_opc_DROP(ForthVM &vm);
void fcode_opc_2DROP(ForthVM &vm);
// .139
void fcode_opc_DUP(ForthVM &vm);
void fcode_opc_2DUP(ForthVM &vm);
void fcode_opc_qDUP(ForthVM &vm);
void fcode_opc_EMIT(ForthVM &vm);
void fcode_opc_ENCODEplus(ForthVM &vm);
void fcode_opc_ENCODE_BYTES(ForthVM &vm);
// .140
void fcode_opc_ENCODE_INT(ForthVM &vm);
void fcode_opc_ENCODE_PHYS(ForthVM &vm);
void fcode_opc_ENCODE_STRING(ForthVM &vm);
// .141
void fcode_opc_END0(ForthVM &vm);
void fcode_opc_END1(ForthVM &vm);
// .142
void fcode_opc_ERASE_SCREEN(ForthVM &vm);
void fcode_opc_EVALUATE(ForthVM &vm);
void fcode_opc_EXECUTE(ForthVM &vm);
void fcode_opc_EXIT(ForthVM &vm);
// .143
void fcode_opc_EXPECT(ForthVM &vm);
void fcode_opc_EXTERNAL_TOKEN(ForthVM &vm);
// .144
void fcode_opc_FB8_BLINK_SCREEN(ForthVM &vm);
void fcode_opc_FB8_DELETE_CHARS(ForthVM &vm);
void fcode_opc_FB8_DELETE_LINES(ForthVM &vm);
void fcode_opc_FB8_DRAW_CHARACTER(ForthVM &vm);
void fcode_opc_FB8_DRAW_LOGO(ForthVM &vm);
void fcode_opc_FB8_ERASE_SCREEN(ForthVM &vm);
void fcode_opc_FB8_INSERT_CHARACTERS(ForthVM &vm);
void fcode_opc_FB8_INSERT_LINES(ForthVM &vm);
void fcode_opc_FB8_INSTALL(ForthVM &vm);
void fcode_opc_FB8_INVERT_SCREEN(ForthVM &vm);
// .145
void fcode_opc_FB8_RESET_SCREEN(ForthVM &vm);
void fcode_opc_FB8_TOGGLE_CURSOR(ForthVM &vm);
void fcode_opc_FCODE_REVISION(ForthVM &vm);
void fcode_opc_FERROR(ForthVM &vm);
// .146
void fcode_opc_FILL(ForthVM &vm);
void fcode_opc_dFIND(ForthVM &vm);
void fcode_opc_FIND_METHOD(ForthVM &vm);
void fcode_opc_FIND_PACKAGE(ForthVM &vm);
// .147
void fcode_opc_FINISH_DEVICE(ForthVM &vm);
void fcode_opc_gtFONT(ForthVM &vm);
void fcode_opc_FONTBYTES(ForthVM &vm);
void fcode_opc_FRAME_BUFFER_ADR(ForthVM &vm);
void fcode_opc_FREE_MEM(ForthVM &vm);
void fcode_opc_FREE_VIRTUAL(ForthVM &vm);
// .148
void fcode_opc_GET_INHERITED_PROPERTIY(ForthVM &vm);
void fcode_opc_GET_MSECS(ForthVM &vm);
void fcode_opc_GET_MY_PROPERTY(ForthVM &vm);
void fcode_opc_GET_PACKAGE_PROPERTY(ForthVM &vm);
void fcode_opc_GET_TOKEN(ForthVM &vm);
// .151
void fcode_opc_HERE(ForthVM &vm);
// .150
void fcode_opc_HOLD(ForthVM &vm);
void fcode_opc_I(ForthVM &vm);
void fcode_opc_IHANDLEgtPHANDLE(ForthVM &vm);
// .151
void fcode_opc_INSERT_CHARACTERS(ForthVM &vm);
void fcode_opc_INSERT_LINES(ForthVM &vm);
// .152
void fcode_opc_INSTANCE(ForthVM &vm);
// .153
void fcode_opc_INVERSEq(ForthVM &vm);
void fcode_opc_INVERSE_SCREENq(ForthVM &vm);
void fcode_opc_INVERT(ForthVM &vm);
// .154
void fcode_opc_INVERT_SCREEN(ForthVM &vm);
void fcode_opc_IS_INSTALL(ForthVM &vm);
void fcode_opc_IS_REMOVE(ForthVM &vm);
void fcode_opc_IS_SELFTEST(ForthVM &vm);
void fcode_opc_bIS_USER_WORDd(ForthVM &vm);
// .155
void fcode_opc_J(ForthVM &vm);
void fcode_opc_KEY(ForthVM &vm);
void fcode_opc_KEYq(ForthVM &vm);
void fcode_opc_Lex(ForthVM &vm);
void fcode_opc_Lcomma(ForthVM &vm);
void fcode_opc_Lat(ForthVM &vm);
void fcode_opc_divL(ForthVM &vm);
void fcode_opc_divLmul(ForthVM &vm);
void fcode_opc_LAplus(ForthVM &vm);
void fcode_opc_LA1plus(ForthVM &vm);
// .156
void fcode_opc_LBFLIP(ForthVM &vm);
void fcode_opc_LBFLIPS(ForthVM &vm);
void fcode_opc_LBSPLIT(ForthVM &vm);
void fcode_opc_LCC(ForthVM &vm);
void fcode_opc_LEFT_PARSE_STRING(ForthVM &vm);
// .157
void fcode_opc_LINEsharp(ForthVM &vm);
void fcode_opc_sharpLINE(ForthVM &vm);
void fcode_opc_sharpLINES(ForthVM &vm);
// .159
void fcode_opc_LPEEK(ForthVM &vm);
void fcode_opc_LPOKE(ForthVM &vm);
void fcode_opc_LSHIFT(ForthVM &vm);
void fcode_opc_LWFLIP(ForthVM &vm);
void fcode_opc_LWFLIPS(ForthVM &vm);
void fcode_opc_LWSPLIT(ForthVM &vm);
void fcode_opc_MAC_ADDRESS(ForthVM &vm);
// .160
void fcode_opc_MAP_LOW(ForthVM &vm);
void fcode_opc_MASK(ForthVM &vm);
// .161
void fcode_opc_MAX(ForthVM &vm);
void fcode_opc_MEMORY_TEST_SUITE(ForthVM &vm);
void fcode_opc_MIN(ForthVM &vm);
// .162
void fcode_opc_MOD(ForthVM &vm);
void fcode_opc_divMOD(ForthVM &vm);
void fcode_opc_MODEL(ForthVM &vm);
void fcode_opc_MOVE(ForthVM &vm);
void fcode_opc_MS(ForthVM &vm);
// .163
void fcode_opc_MY_ADDRESS(ForthVM &vm);
void fcode_opc_MY_ARGS(ForthVM &vm);
void fcode_opc_MY_PARENT(ForthVM &vm);
void fcode_opc_MY_SELF(ForthVM &vm);
void fcode_opc_MY_SPACE(ForthVM &vm);
void fcode_opc_MY_UNIT(ForthVM &vm);
void fcode_opc_slashN(ForthVM &vm);
// .164
void fcode_opc_NAplus(ForthVM &vm);
// .165
void fcode_opc_NAMED_TOKEN(ForthVM &vm);
void fcode_opc_NEGATE(ForthVM &vm);
void fcode_opc_NEW_DEVICE(ForthVM &vm);
// .166
void fcode_opc_NEW_TOKEN(ForthVM &vm);
void fcode_opc_NEXT_PROPERTY(ForthVM &vm);
void fcode_opc_NIP(ForthVM &vm);
void fcode_opc_NOOP(ForthVM &vm);
// .167
void fcode_opc_dNUMBER(ForthVM &vm);
// .169
void fcode_opc_OFF(ForthVM &vm);
// .170
void fcode_opc_OFFSET16(ForthVM &vm);
void fcode_opc_ON(ForthVM &vm);
void fcode_opc_OPEN_PACKAGE(ForthVM &vm);
// .171
void fcode_opc_dOPEN_PACKAGE(ForthVM &vm);
void fcode_opc_OR(ForthVM &vm);
void fcode_opc_sharpOUT(ForthVM &vm);
void fcode_opc_OVER(ForthVM &vm);
void fcode_opc_2OVER(ForthVM &vm);
void fcode_opc_PACK(ForthVM &vm);
// .172
void fcode_opc_PARSE_2INT(ForthVM &vm);
// .173
void fcode_opc_PEER(ForthVM &vm);
void fcode_opc_PICK(ForthVM &vm);
// .174
void fcode_opc_PROPERTY(ForthVM &vm);
void fcode_opc_Rgt(ForthVM &vm);
void fcode_opc_Rat(ForthVM &vm);
void fcode_opc_dotR(ForthVM &vm);
void fcode_opc_gtR(ForthVM &vm);
// .175
void fcode_opc_RBex(ForthVM &vm);
void fcode_opc_RBat(ForthVM &vm);
// .177
void fcode_opc_REG(ForthVM &vm);
// .179
void fcode_opc_RESET_SCREEN(ForthVM &vm);
void fcode_opc_RLex(ForthVM &vm);
// .180
void fcode_opc_RLat(ForthVM &vm);
void fcode_opc_ROLL(ForthVM &vm);
void fcode_opc_ROT(ForthVM &vm);
void fcode_opc_mROT(ForthVM &vm);
void fcode_opc_2ROT(ForthVM &vm);
void fcode_opc_RSHIFT(ForthVM &vm);
void fcode_opc_RWex(ForthVM &vm);
// .181
void fcode_opc_RWat(ForthVM &vm);
void fcode_opc_sharpS(ForthVM &vm);
void fcode_opc_dotS(ForthVM &vm);
// .182
void fcode_opc_SBUS_INTRgtCPU(ForthVM &vm);
void fcode_opc_SCREEN_HEIGHT(ForthVM &vm);
void fcode_opc_SCREEN_WIDTH(ForthVM &vm);
// .184
void fcode_opc_SET_ARGS(ForthVM &vm);
// .185
void fcode_opc_SET_FONT(ForthVM &vm);
void fcode_opc_SET_TOKEN(ForthVM &vm);
// .186
void fcode_opc_SIGN(ForthVM &vm);
void fcode_opc_SPAN(ForthVM &vm);
// .187
void fcode_opc_START0(ForthVM &vm);
void fcode_opc_START1(ForthVM &vm);
void fcode_opc_START2(ForthVM &vm);
void fcode_opc_START4(ForthVM &vm);
void fcode_opc_STATE(ForthVM &vm);
// .189
void fcode_opc_SUSPEND_FCODE(ForthVM &vm);
void fcode_opc_SWAP(ForthVM &vm);
void fcode_opc_2SWAP(ForthVM &vm);
// .190
void fcode_opc_THROW(ForthVM &vm);
// .191
void fcode_opc_TOGGLE_CURSOR(ForthVM &vm);
void fcode_opc_TUCK(ForthVM &vm);
void fcode_opc_TYPE(ForthVM &vm);
// .192
void fcode_opc_Usharp(ForthVM &vm);
void fcode_opc_Usharpgt(ForthVM &vm);
void fcode_opc_UsharpS(ForthVM &vm);
void fcode_opc_Udot(ForthVM &vm);
void fcode_opc_Ult(ForthVM &vm);
void fcode_opc_Ultequal(ForthVM &vm);
void fcode_opc_Ugt(ForthVM &vm);
void fcode_opc_Ugtequal(ForthVM &vm);
void fcode_opc_U2div(ForthVM &vm);
void fcode_opc_UMmul(ForthVM &vm);
void fcode_opc_UMdivMOD(ForthVM &vm);
// .193
void fcode_opc_UdivMOD(ForthVM &vm);
void fcode_opc_UNLOOP(ForthVM &vm);
void fcode_opc_UPC(ForthVM &vm);
void fcode_opc_UdotR(ForthVM &vm);
// .194
void fcode_opc_USER_ABORT(ForthVM &vm);
void fcode_opc_VERSION1(ForthVM &vm);
void fcode_opc_Wex(ForthVM &vm);
void fcode_opc_Wcomma(ForthVM &vm);
void fcode_opc_Wat(ForthVM &vm);
void fcode_opc_slashW(ForthVM &vm);
// .195
void fcode_opc_slashWmul(ForthVM &vm);
void fcode_opc_ltWat(ForthVM &vm);
void fcode_opc_WAplus(ForthVM &vm);
void fcode_opc_WA1plus(ForthVM &vm);
void fcode_opc_WBFLIP(ForthVM &vm);
void fcode_opc_WBFLIPS(ForthVM &vm);
void fcode_opc_WBSPLIT(ForthVM &vm);
void fcode_opc_WINDOW_LEFT(ForthVM &vm);
void fcode_opc_WINDOW_TOP(ForthVM &vm);
void fcode_opc_WITHIN(ForthVM &vm);
// .196
void fcode_opc_WLJOIN(ForthVM &vm);
void fcode_opc_WPEEK(ForthVM &vm);
void fcode_opc_WPOKE(ForthVM &vm);
void fcode_opc_XOR(ForthVM &vm);

// .104
#define FCODE_ex 0x72
#define FCODE_sharp 0xc7
// .105
#define FCODE_sharpgt 0xc9
#define FCODE_mul 0x20
#define FCODE_plus 0x1e
#define FCODE_plusex 0x6c
#define FCODE_comma 0xd3
#define FCODE_minus 0x1f
#define FCODE_dot 0x9d
// .106
#define FCODE_div 0x21
#define FCODE_lt 0x3a
#define FCODE_ltsharp 0x96
#define FCODE_ltequal 0x43
#define FCODE_ltgt 0x3d
#define FCODE_equal 0x3c
#define FCODE_gt 0x3b
#define FCODE_gtequal 0x42
// .107
#define FCODE_at 0x6d
#define FCODE_0 0xa5
#define FCODE_0lt 0x36
#define FCODE_0ltequal 0x37
#define FCODE_0ltgt 0x35
#define FCODE_0equal 0x34
#define FCODE_0gt 0x38
// .108
#define FCODE_0gtequal 0x39
#define FCODE_1 0xa6
#define FCODE_minus1 0xa4
#define FCODE_2 0xa7
#define FCODE_2ex 0x77
#define FCODE_2mul 0x59
#define FCODE_2div 0x57
#define FCODE_2at 0x76
#define FCODE_3 0xa8
// .109
#define FCODE_gtgtA 0x29
#define FCODE_ABORT 0x216
#define FCODE_ABS 0x2d
// .111
#define FCODE_ALARM 0x213
#define FCODE_ALIGNED 0xae
#define FCODE_ALLOC_MEM 0x8b
// .112
#define FCODE_AND 0x23
// .113
#define FCODE_Bstring 0x12
#define FCODE_Btick 0x11
#define FCODE_Bcolon 0xb7
#define FCODE_Bsemincolon 0xc2
// .114
#define FCODE_BASE 0xa0
#define FCODE_BBRANCH 0x13
// .115
#define FCODE_BqBRANCH 0x14
#define FCODE_BBUFFERcol 0xbd
#define FCODE_BCASE 0xc4
// .116
#define FCODE_BCONSTANT 0xba
#define FCODE_BCREATE 0xbb
#define FCODE_BDEFER 0xbc
#define FCODE_BDO 0x17
#define FCODE_BqDO 0x18
// .117
#define FCODE_BEHAVIOR 0xde
#define FCODE_BELL 0xab
#define FCODE_BENDCASE 0xc5
// .118
#define FCODE_BENDOF 0xc6
#define FCODE_BETWEEN 0x44
#define FCODE_BFIELD 0xbe
#define FCODE_BL 0xa9
#define FCODE_BLEAVE 0x1b
#define FCODE_BLINK_SCREEN 0x15b
// .119
#define FCODE_BLIT 0x10
#define FCODE_BLJOIN 0x7f
#define FCODE_BLOOP 0x15
#define FCODE_BplusLOOP 0x16
// .120
#define FCODE_BltMARK 0xb1
#define FCODE_BODYgt 0x85
#define FCODE_gtBODY 0x86
#define FCODE_BOF 0x1c
// .121
#define FCODE_BOUNDS 0xac
#define FCODE_BgtRESOLVE 0xb2
#define FCODE_BS 0xaa
// .122
#define FCODE_BTO 0xc3
#define FCODE_BVALUE 0xb8
#define FCODE_BVARIABLE 0xb9
#define FCODE_BWJOIN 0xb0
// .123
#define FCODE_BYTE_LOAD 0x23e
#define FCODE_Cex 0x75
#define FCODE_Ccomma 0xd0
// .124
#define FCODE_slashC 0x5a
#define FCODE_Cat 0x71
#define FCODE_CAplus 0x5e
// .125
#define FCODE_dCALL_METHOD 0x20e
#define FCODE_CALL_PACKAGE 0x208
#define FCODE_dCALL_PARENT 0x209
// .126
#define FCODE_CATCH 0x217
#define FCODE_CELLplus 0x65
#define FCODE_CELLS 0x69
#define FCODE_CHARplus 0x62
// .127
#define FCODE_CHAR_HEIGHT 0x16c
#define FCODE_CHARS 0x66
#define FCODE_CHAR_WIDTH 0x16d
#define FCODE_CHILD 0x23b
// .128
#define FCODE_CLOSE_PACKAGE 0x206
#define FCODE_sharpCOLUMN 0x153
#define FCODE_sharpCOLUMNS 0x151
#define FCODE_COMP 0x7a
// .129
#define FCODE_COMPILEcomma 0xdd
// .130
#define FCODE_COUNT 0x84
#define FCODE_CPEEK 0x220
#define FCODE_CPOKE 0x223
#define FCODE_CR 0x92
#define FCODE_bracketCR 0x91
// .131
#define FCODE_Dplus 0xd8
#define FCODE_Dminus 0xd9
// .132
#define FCODE_DECODE_INT 0x21b
#define FCODE_DECODE_PHYS 0x128
#define FCODE_DECODE_STRING 0x21c
#define FCODE_DEFAULT_FONT 0x16a
// .133
#define FCODE_DELETE_CHARS 0x15e
#define FCODE_DELETE_LINES 0x160
#define FCODE_DELETE_PROPERTY 0x21e
#define FCODE_DEPTH 0x51
// .134
#define FCODE_DEVICE_NAME 0x201
#define FCODE_DEVICE_TYPE 0x11a
// .135
#define FCODE_DIAGNOSTIC_MODEq 0x120
#define FCODE_DIGIT 0xa3
// .138
#define FCODE_DRAW_CHARACTER 0x157
#define FCODE_DRAW_LOGO 0x161
#define FCODE_DROP 0x46
#define FCODE_2DROP 0x52
// .139
#define FCODE_DUP 0x47
#define FCODE_2DUP 0x53
#define FCODE_qDUP 0x50
#define FCODE_EMIT 0x8f
#define FCODE_ENCODEplus 0x112
#define FCODE_ENCODE_BYTES 0x115
// .140
#define FCODE_ENCODE_INT 0x111
#define FCODE_ENCODE_PHYS 0x113
#define FCODE_ENCODE_STRING 0x114
// .141
#define FCODE_END0 0x00
#define FCODE_END1 0xff
// .142
#define FCODE_ERASE_SCREEN 0x15a
#define FCODE_EVALUATE 0xcd
#define FCODE_EXECUTE 0x1d
#define FCODE_EXIT 0x33
// .143
#define FCODE_EXPECT 0x8a
#define FCODE_EXTERNAL_TOKEN 0xca
// .144
#define FCODE_FB8_BLINK_SCREEN 0x184
#define FCODE_FB8_DELETE_CHARS 0x187
#define FCODE_FB8_DELETE_LINES 0x189
#define FCODE_FB8_DRAW_CHARACTER 0x180
#define FCODE_FB8_DRAW_LOGO 0x18a
#define FCODE_FB8_ERASE_SCREEN 0x183
#define FCODE_FB8_INSERT_CHARACTERS 0x186
#define FCODE_FB8_INSERT_LINES 0x188
#define FCODE_FB8_INSTALL 0x18b
#define FCODE_FB8_INVERT_SCREEN 0x185
// .145
#define FCODE_FB8_RESET_SCREEN 0x181
#define FCODE_FB8_TOGGLE_CURSOR 0x182
#define FCODE_FCODE_REVISION 0x87
#define FCODE_FERROR 0xfc
// .146
#define FCODE_FILL 0x79
#define FCODE_dFIND 0xcb
#define FCODE_FIND_METHOD 0x207
#define FCODE_FIND_PACKAGE 0x204
// .147
#define FCODE_FINISH_DEVICE 0x127
#define FCODE_gtFONT 0x16e
#define FCODE_FONTBYTES 0x16f
#define FCODE_FRAME_BUFFER_ADR 0x162
#define FCODE_FREE_MEM 0x8c
#define FCODE_FREE_VIRTUAL 0x105
// .148
#define FCODE_GET_INHERITED_PROPERTIY 0x21d
#define FCODE_GET_MSECS 0x125
#define FCODE_GET_MY_PROPERTY 0x21a
#define FCODE_GET_PACKAGE_PROPERTY 0x21f
#define FCODE_GET_TOKEN 0xda
// .151
#define FCODE_HERE 0xad
// .150
#define FCODE_HOLD 0x95
#define FCODE_I 0x19
#define FCODE_IHANDLEgtPHANDLE 0x20b
// .151
#define FCODE_INSERT_CHARACTERS 0x15d
#define FCODE_INSERT_LINES 0x15f
// .152
#define FCODE_INSTANCE 0xc0
// .153
#define FCODE_INVERSEq 0x154
#define FCODE_INVERSE_SCREENq 0x155
#define FCODE_INVERT 0x26
// .154
#define FCODE_INVERT_SCREEN 0x15c
#define FCODE_IS_INSTALL 0x11c
#define FCODE_IS_REMOVE 0x11d
#define FCODE_IS_SELFTEST 0x11e
#define FCODE_bIS_USER_WORDd 0x214
// .155
#define FCODE_J 0x1a
#define FCODE_KEY 0x8e
#define FCODE_KEYq 0x8d
#define FCODE_Lex 0x73
#define FCODE_Lcomma 0xd2
#define FCODE_Lat 0x6e
#define FCODE_divL 0x5c
#define FCODE_divLmul 0x68
#define FCODE_LAplus 0x60
#define FCODE_LA1plus 0x64
// .156
#define FCODE_LBFLIP 0x227
#define FCODE_LBFLIPS 0x228
#define FCODE_LBSPLIT 0x7e
#define FCODE_LCC 0x82
#define FCODE_LEFT_PARSE_STRING 0x240
// .157
#define FCODE_LINEsharp 0x152
#define FCODE_sharpLINE 0x94 //var
#define FCODE_sharpLINES 0x150
// .159
#define FCODE_LPEEK 0x222
#define FCODE_LPOKE 0x225
#define FCODE_LSHIFT 0x27
#define FCODE_LWFLIP 0x226
#define FCODE_LWFLIPS 0x237
#define FCODE_LWSPLIT 0x7c
#define FCODE_MAC_ADDRESS 0x1a4
// .160
#define FCODE_MAP_LOW 0x130
#define FCODE_MASK 0x124
// .161
#define FCODE_MAX 0x2f
#define FCODE_MEMORY_TEST_SUITE 0x122
#define FCODE_MIN 0x2e
// .162
#define FCODE_MOD 0x22
#define FCODE_divMOD 0x2a
#define FCODE_MODEL 0x119
#define FCODE_MOVE 0x78
#define FCODE_MS 0x126
// .163
#define FCODE_MY_ADDRESS 0x102
#define FCODE_MY_ARGS 0x202
#define FCODE_MY_PARENT 0x20a
#define FCODE_MY_SELF 0x203
#define FCODE_MY_SPACE 0x103
#define FCODE_MY_UNIT 0x20d
#define FCODE_slashN 0x5d
// .164
#define FCODE_NAplus 0x61
// .165
#define FCODE_NAMED_TOKEN 0xb6
#define FCODE_NEGATE 0x2c
#define FCODE_NEW_DEVICE 0x11f
// .166
#define FCODE_NEW_TOKEN 0xb5
#define FCODE_NEXT_PROPERTY 0x23d
#define FCODE_NIP 0x4d
#define FCODE_NOOP 0x7b
// .167
#define FCODE_dNUMBER 0xa2
// .169
#define FCODE_OFF 0x6b
// .170
#define FCODE_OFFSET16 0xcc
#define FCODE_ON 0x6a
#define FCODE_OPEN_PACKAGE 0x205
// .171
#define FCODE_dOPEN_PACKAGE 0x20f
#define FCODE_OR 0x24
#define FCODE_sharpOUT 0x93 // var
#define FCODE_OVER 0x48
#define FCODE_2OVER 0x54
#define FCODE_PACK 0x83
// .172
#define FCODE_PARSE_2INT 0x11b
// .173
#define FCODE_PEER 0x23c
#define FCODE_PICK 0x4e
// .174
#define FCODE_PROPERTY 0x110
#define FCODE_Rgt 0x31
#define FCODE_Rat 0x32
#define FCODE_dotR 0x9e
#define FCODE_gtR 0x30
// .175
#define FCODE_RBex 0x231
#define FCODE_RBat 0x230
// .177
#define FCODE_REG 0x116
// .179
#define FCODE_RESET_SCREEN 0x158
#define FCODE_RLex 0x235
// .180
#define FCODE_RLat 0x234
#define FCODE_ROLL 0x4f
#define FCODE_ROT 0x4a
#define FCODE_mROT 0x4b
#define FCODE_2ROT 0x56
#define FCODE_RSHIFT 0x28
#define FCODE_RWex 0x233
// .181
#define FCODE_RWat 0x232
#define FCODE_sharpS 0xc8
#define FCODE_dotS 0x9f
// .182
#define FCODE_SBUS_INTRgtCPU 0x131
#define FCODE_SCREEN_HEIGHT 0x163
#define FCODE_SCREEN_WIDTH 0x164
// .184
#define FCODE_SET_ARGS 0x23f
// .185
#define FCODE_SET_FONT 0x16b
#define FCODE_SET_TOKEN 0xdb
// .186
#define FCODE_SIGN 0x98
#define FCODE_SPAN 0x88 // var
// .187
#define FCODE_START0 0xf0
#define FCODE_START1 0xf1
#define FCODE_START2 0xf2
#define FCODE_START4 0xf3
#define FCODE_STATE 0xdc // var
// .189
#define FCODE_SUSPEND_FCODE 0x215
#define FCODE_SWAP 0x49
#define FCODE_2SWAP 0x55
// .190
#define FCODE_THROW 0x218
// .191
#define FCODE_TOGGLE_CURSOR 0x159
#define FCODE_TUCK 0x4c
#define FCODE_TYPE 0x90
// .192
#define FCODE_Usharp 0x99
#define FCODE_Usharpgt 0x97
#define FCODE_UsharpS 0x9a
#define FCODE_Udot 0x9b
#define FCODE_Ult 0x40
#define FCODE_Ultequal 0x3f
#define FCODE_Ugt 0x3e
#define FCODE_Ugtequal 0x41
#define FCODE_U2div 0x58
#define FCODE_UMmul 0xd4
#define FCODE_UMdivMOD 0xd5
// .193
#define FCODE_UdivMOD 0x2b
#define FCODE_UNLOOP 0x89
#define FCODE_UPC 0x81
#define FCODE_UdotR 0x9c
// .194
#define FCODE_USER_ABORT 0x219
#define FCODE_VERSION1 0xfd
#define FCODE_Wex 0x74
#define FCODE_Wcomma 0xd1
#define FCODE_Wat 0x6f
#define FCODE_slashW 0x5b
// .195
#define FCODE_slashWmul 0x67
#define FCODE_ltWat 0x70
#define FCODE_WAplus 0x5f
#define FCODE_WA1plus 0x63
#define FCODE_WBFLIP 0x80
#define FCODE_WBFLIPS 0x236
#define FCODE_WBSPLIT 0xaf
#define FCODE_WINDOW_LEFT 0x166
#define FCODE_WINDOW_TOP 0x165
#define FCODE_WITHIN 0x45
// .196
#define FCODE_WLJOIN 0x7d
#define FCODE_WPEEK 0x221
#define FCODE_WPOKE 0x224
#define FCODE_XOR 0x25
#endif

