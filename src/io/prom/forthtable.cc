/*
 *	PearPC
 *	forthtable.cc
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

#include "forthtable.h"
#include "forth.h"
#include "fcode.h"

struct forth_word {
	const char *name;
	uint32 fcode;
	FCodeFunction func;
};

forth_word forth_word_table[] = {
{"!", FCODE_ex, fcode_opc_ex},
{"#", FCODE_sharp, fcode_opc_sharp},
{"#>", FCODE_sharpgt, fcode_opc_sharpgt},
{"*", FCODE_mul, fcode_opc_mul},
{"+", FCODE_plus, fcode_opc_plus},
{"+!", FCODE_plusex, fcode_opc_plusex},
{",", FCODE_comma, fcode_opc_comma},
{"-", FCODE_minus, fcode_opc_minus},
{".", FCODE_dot, fcode_opc_dot},
{"/", FCODE_div, fcode_opc_div},
{"<", FCODE_lt, fcode_opc_lt},
{"<#", FCODE_ltsharp, fcode_opc_ltsharp},
{"<=", FCODE_ltequal, fcode_opc_ltequal},
{"<>", FCODE_ltgt, fcode_opc_ltgt},
{"=", FCODE_equal, fcode_opc_equal},
{">", FCODE_gt, fcode_opc_gt},
{">=", FCODE_gtequal, fcode_opc_gtequal},
{"@", FCODE_at, fcode_opc_at},
{"0", FCODE_0, fcode_opc_0},
{"0<", FCODE_0lt, fcode_opc_0lt},
{"0<=", FCODE_0ltequal, fcode_opc_0ltequal},
{"0<>", FCODE_0ltgt, fcode_opc_0ltgt},
{"0=", FCODE_0equal, fcode_opc_0equal},
{"0>", FCODE_0gt, fcode_opc_0gt},
{"0>=", FCODE_0gtequal, fcode_opc_0gtequal},
{"1", FCODE_1, fcode_opc_1},
{"-1", FCODE_minus1, fcode_opc_minus1},
{"2", FCODE_2, fcode_opc_2},
{"2!", FCODE_2ex, fcode_opc_2ex},
{"2*", FCODE_2mul, fcode_opc_2mul},
{"2/", FCODE_2div, fcode_opc_2div},
{"2@", FCODE_2at, fcode_opc_2at},
{"3", FCODE_3, fcode_opc_3},
{">>a", FCODE_gtgtA, fcode_opc_gtgtA},
{"abort", FCODE_ABORT, fcode_opc_ABORT},
{"abs", FCODE_ABS, fcode_opc_ABS},
{"alarm", FCODE_ALARM, fcode_opc_ALARM},
{"aligned", FCODE_ALIGNED, fcode_opc_ALIGNED},
{"alloc-mem", FCODE_ALLOC_MEM, fcode_opc_ALLOC_MEM},
{"and", FCODE_AND, fcode_opc_AND},
{"behavior", FCODE_BEHAVIOR, fcode_opc_BEHAVIOR},
{"bell", FCODE_BELL, fcode_opc_BELL},
{"between", FCODE_BETWEEN, fcode_opc_BETWEEN},
{"bl", FCODE_BL, fcode_opc_BL},
{"blink-screen", FCODE_BLINK_SCREEN, fcode_opc_BLINK_SCREEN},
{"bljoin", FCODE_BLJOIN, fcode_opc_BLJOIN},
{"body>", FCODE_BODYgt, fcode_opc_BODYgt},
{">body", FCODE_gtBODY, fcode_opc_gtBODY},
{"bounds", FCODE_BOUNDS, fcode_opc_BOUNDS},
{"bs", FCODE_BS, fcode_opc_BS},
{"bwjoin", FCODE_BWJOIN, fcode_opc_BWJOIN},
{"byte-load", FCODE_BYTE_LOAD, fcode_opc_BYTE_LOAD},
{"c!", FCODE_Cex, fcode_opc_Cex},
{"c,", FCODE_Ccomma, fcode_opc_Ccomma},
{"/c", FCODE_slashC, fcode_opc_slashC},
{"c@", FCODE_Cat, fcode_opc_Cat},
{"ca+", FCODE_CAplus, fcode_opc_CAplus},
{"$call-method", FCODE_dCALL_METHOD, fcode_opc_dCALL_METHOD},
{"call-package", FCODE_CALL_PACKAGE, fcode_opc_CALL_PACKAGE},
{"$call-parent", FCODE_dCALL_PARENT, fcode_opc_dCALL_PARENT},
{"catch", FCODE_CATCH, fcode_opc_CATCH},
{"cell+", FCODE_CELLplus, fcode_opc_CELLplus},
{"cells", FCODE_CELLS, fcode_opc_CELLS},
{"char+", FCODE_CHARplus, fcode_opc_CHARplus},
{"char-height", FCODE_CHAR_HEIGHT, fcode_opc_CHAR_HEIGHT},
{"chars", FCODE_CHARS, fcode_opc_CHARS},
{"char-width", FCODE_CHAR_WIDTH, fcode_opc_CHAR_WIDTH},
{"child", FCODE_CHILD, fcode_opc_CHILD},
{"close-package", FCODE_CLOSE_PACKAGE, fcode_opc_CLOSE_PACKAGE},
{"#column", FCODE_sharpCOLUMN, fcode_opc_sharpCOLUMN},
{"#columns", FCODE_sharpCOLUMNS, fcode_opc_sharpCOLUMNS},
{"comp", FCODE_COMP, fcode_opc_COMP},
{"compile,", FCODE_COMPILEcomma, fcode_opc_COMPILEcomma},
{"count", FCODE_COUNT, fcode_opc_COUNT},
{"cpeek", FCODE_CPEEK, fcode_opc_CPEEK},
{"cpoke", FCODE_CPOKE, fcode_opc_CPOKE},
{"cr", FCODE_CR, fcode_opc_CR},
{"(cr", FCODE_bracketCR, fcode_opc_bracketCR},
{"d+", FCODE_Dplus, fcode_opc_Dplus},
{"d-", FCODE_Dminus, fcode_opc_Dminus},
{"decode-int", FCODE_DECODE_INT, fcode_opc_DECODE_INT},
{"decode-phys", FCODE_DECODE_PHYS, fcode_opc_DECODE_PHYS},
{"decode-string", FCODE_DECODE_STRING, fcode_opc_DECODE_STRING},
{"default-font", FCODE_DEFAULT_FONT, fcode_opc_DEFAULT_FONT},
{"delete-chars", FCODE_DELETE_CHARS, fcode_opc_DELETE_CHARS},
{"delete-lines", FCODE_DELETE_LINES, fcode_opc_DELETE_LINES},
{"delete-property", FCODE_DELETE_PROPERTY, fcode_opc_DELETE_PROPERTY},
{"depth", FCODE_DEPTH, fcode_opc_DEPTH},
{"device-name", FCODE_DEVICE_NAME, fcode_opc_DEVICE_NAME},
{"device-type", FCODE_DEVICE_TYPE, fcode_opc_DEVICE_TYPE},
{"diagnostic-mode?", FCODE_DIAGNOSTIC_MODEq, fcode_opc_DIAGNOSTIC_MODEq},
{"digit", FCODE_DIGIT, fcode_opc_DIGIT},
{"draw-character", FCODE_DRAW_CHARACTER, fcode_opc_DRAW_CHARACTER},
{"draw-logo", FCODE_DRAW_LOGO, fcode_opc_DRAW_LOGO},
{"drop", FCODE_DROP, fcode_opc_DROP},
{"2drop", FCODE_2DROP, fcode_opc_2DROP},
{"dup", FCODE_DUP, fcode_opc_DUP},
{"2dup", FCODE_2DUP, fcode_opc_2DUP},
{"?dup", FCODE_qDUP, fcode_opc_qDUP},
{"emit", FCODE_EMIT, fcode_opc_EMIT},
{"encode+", FCODE_ENCODEplus, fcode_opc_ENCODEplus},
{"encode-bytes", FCODE_ENCODE_BYTES, fcode_opc_ENCODE_BYTES},
{"encode-int", FCODE_ENCODE_INT, fcode_opc_ENCODE_INT},
{"encode-phys", FCODE_ENCODE_PHYS, fcode_opc_ENCODE_PHYS},
{"encode-string", FCODE_ENCODE_STRING, fcode_opc_ENCODE_STRING},
{"end0", FCODE_END0, fcode_opc_END0},
{"end1", FCODE_END1, fcode_opc_END1},
{"erase-screen", FCODE_ERASE_SCREEN, fcode_opc_ERASE_SCREEN},
{"evaluate", FCODE_EVALUATE, fcode_opc_EVALUATE},
{"execute", FCODE_EXECUTE, fcode_opc_EXECUTE},
{"exit", FCODE_EXIT, fcode_opc_EXIT},
{"expect", FCODE_EXPECT, fcode_opc_EXPECT},
{"fb8-blink-screen", FCODE_FB8_BLINK_SCREEN, fcode_opc_FB8_BLINK_SCREEN},
{"fb8-delete-chars", FCODE_FB8_DELETE_CHARS, fcode_opc_FB8_DELETE_CHARS},
{"fb8-delete-lines", FCODE_FB8_DELETE_LINES, fcode_opc_FB8_DELETE_LINES},
{"fb8-draw-character", FCODE_FB8_DRAW_CHARACTER, fcode_opc_FB8_DRAW_CHARACTER},
{"fb8-draw-logo", FCODE_FB8_DRAW_LOGO, fcode_opc_FB8_DRAW_LOGO},
{"fb8-erase-screen", FCODE_FB8_ERASE_SCREEN, fcode_opc_FB8_ERASE_SCREEN},
{"fb8-insert-characters", FCODE_FB8_INSERT_CHARACTERS, fcode_opc_FB8_INSERT_CHARACTERS},
{"fb8-insert_lines", FCODE_FB8_INSERT_LINES, fcode_opc_FB8_INSERT_LINES},
{"fb8-install", FCODE_FB8_INSTALL, fcode_opc_FB8_INSTALL},
{"fb8-invert-screen", FCODE_FB8_INVERT_SCREEN, fcode_opc_FB8_INVERT_SCREEN},
{"fb8-reset-screen", FCODE_FB8_RESET_SCREEN, fcode_opc_FB8_RESET_SCREEN},
{"fb8-toggle-cursor", FCODE_FB8_TOGGLE_CURSOR, fcode_opc_FB8_TOGGLE_CURSOR},
{"fcode-revision", FCODE_FCODE_REVISION, fcode_opc_FCODE_REVISION},
{"ferror", FCODE_FERROR, fcode_opc_FERROR},
{"fill", FCODE_FILL, fcode_opc_FILL},
{"$find", FCODE_dFIND, fcode_opc_dFIND},
{"find-method", FCODE_FIND_METHOD, fcode_opc_FIND_METHOD},
{"find-package", FCODE_FIND_PACKAGE, fcode_opc_FIND_PACKAGE},
{"finish-device", FCODE_FINISH_DEVICE, fcode_opc_FINISH_DEVICE},
{">font", FCODE_gtFONT, fcode_opc_gtFONT},
{"fontbytes", FCODE_FONTBYTES, fcode_opc_FONTBYTES},
{"frame-buffer-adr", FCODE_FRAME_BUFFER_ADR, fcode_opc_FRAME_BUFFER_ADR},
{"free-mem", FCODE_FREE_MEM, fcode_opc_FREE_MEM},
{"free-virtual", FCODE_FREE_VIRTUAL, fcode_opc_FREE_VIRTUAL},
{"get-inherited_propertiy", FCODE_GET_INHERITED_PROPERTIY, fcode_opc_GET_INHERITED_PROPERTIY},
{"get-msecs", FCODE_GET_MSECS, fcode_opc_GET_MSECS},
{"get-my-property", FCODE_GET_MY_PROPERTY, fcode_opc_GET_MY_PROPERTY},
{"get-package_property", FCODE_GET_PACKAGE_PROPERTY, fcode_opc_GET_PACKAGE_PROPERTY},
{"get-token", FCODE_GET_TOKEN, fcode_opc_GET_TOKEN},
{"here", FCODE_HERE, fcode_opc_HERE},
{"hold", FCODE_HOLD, fcode_opc_HOLD},
{"i", FCODE_I, fcode_opc_I},
{"ihandle>phandle", FCODE_IHANDLEgtPHANDLE, fcode_opc_IHANDLEgtPHANDLE},
{"insert-characters", FCODE_INSERT_CHARACTERS, fcode_opc_INSERT_CHARACTERS},
{"insert-lines", FCODE_INSERT_LINES, fcode_opc_INSERT_LINES},
{"instance", FCODE_INSTANCE, fcode_opc_INSTANCE},
{"inverse?", FCODE_INVERSEq, fcode_opc_INVERSEq},
{"inverse-screen?", FCODE_INVERSE_SCREENq, fcode_opc_INVERSE_SCREENq},
{"invert", FCODE_INVERT, fcode_opc_INVERT},
{"invert-screen", FCODE_INVERT_SCREEN, fcode_opc_INVERT_SCREEN},
{"is-install", FCODE_IS_INSTALL, fcode_opc_IS_INSTALL},
{"is-remove", FCODE_IS_REMOVE, fcode_opc_IS_REMOVE},
{"is-selftest", FCODE_IS_SELFTEST, fcode_opc_IS_SELFTEST},
{"(is-user-word)", FCODE_bIS_USER_WORDd, fcode_opc_bIS_USER_WORDd},
{"j", FCODE_J, fcode_opc_J},
{"key", FCODE_KEY, fcode_opc_KEY},
{"key?", FCODE_KEYq, fcode_opc_KEYq},
{"l!", FCODE_Lex, fcode_opc_Lex},
{"l,", FCODE_Lcomma, fcode_opc_Lcomma},
{"l@", FCODE_Lat, fcode_opc_Lat},
{"/l", FCODE_divL, fcode_opc_divL},
{"/l*", FCODE_divLmul, fcode_opc_divLmul},
{"la+", FCODE_LAplus, fcode_opc_LAplus},
{"la1+", FCODE_LA1plus, fcode_opc_LA1plus},
{"lbflip", FCODE_LBFLIP, fcode_opc_LBFLIP},
{"lbflips", FCODE_LBFLIPS, fcode_opc_LBFLIPS},
{"lbsplit", FCODE_LBSPLIT, fcode_opc_LBSPLIT},
{"lcc", FCODE_LCC, fcode_opc_LCC},
{"left-parse-string", FCODE_LEFT_PARSE_STRING, fcode_opc_LEFT_PARSE_STRING},
{"#line", FCODE_sharpLINE, fcode_opc_sharpLINE},
{"#lines", FCODE_sharpLINES, fcode_opc_sharpLINES},
{"lpeek", FCODE_LPEEK, fcode_opc_LPEEK},
{"lpoke", FCODE_LPOKE, fcode_opc_LPOKE},
{"lshift", FCODE_LSHIFT, fcode_opc_LSHIFT},
{"lwflip", FCODE_LWFLIP, fcode_opc_LWFLIP},
{"lwflips", FCODE_LWFLIPS, fcode_opc_LWFLIPS},
{"lwsplit", FCODE_LWSPLIT, fcode_opc_LWSPLIT},
{"mac-address", FCODE_MAC_ADDRESS, fcode_opc_MAC_ADDRESS},
{"map-low", FCODE_MAP_LOW, fcode_opc_MAP_LOW},
{"mask", FCODE_MASK, fcode_opc_MASK},
{"max", FCODE_MAX, fcode_opc_MAX},
{"memory-test-suite", FCODE_MEMORY_TEST_SUITE, fcode_opc_MEMORY_TEST_SUITE},
{"min", FCODE_MIN, fcode_opc_MIN},
{"mod", FCODE_MOD, fcode_opc_MOD},
{"/mod", FCODE_divMOD, fcode_opc_divMOD},
{"model", FCODE_MODEL, fcode_opc_MODEL},
{"move", FCODE_MOVE, fcode_opc_MOVE},
{"ms", FCODE_MS, fcode_opc_MS},
{"my-address", FCODE_MY_ADDRESS, fcode_opc_MY_ADDRESS},
{"my-args", FCODE_MY_ARGS, fcode_opc_MY_ARGS},
{"my-parent", FCODE_MY_PARENT, fcode_opc_MY_PARENT},
{"my-self", FCODE_MY_SELF, fcode_opc_MY_SELF},
{"my-space", FCODE_MY_SPACE, fcode_opc_MY_SPACE},
{"my-unit", FCODE_MY_UNIT, fcode_opc_MY_UNIT},
{"/n", FCODE_slashN, fcode_opc_slashN},
{"na+", FCODE_NAplus, fcode_opc_NAplus},
{"named-token", FCODE_NAMED_TOKEN, fcode_opc_NAMED_TOKEN},
{"negate", FCODE_NEGATE, fcode_opc_NEGATE},
{"new-device", FCODE_NEW_DEVICE, fcode_opc_NEW_DEVICE},
{"next-property", FCODE_NEXT_PROPERTY, fcode_opc_NEXT_PROPERTY},
{"nip", FCODE_NIP, fcode_opc_NIP},
{"noop", FCODE_NOOP, fcode_opc_NOOP},
{"$number", FCODE_dNUMBER, fcode_opc_dNUMBER},
{"off", FCODE_OFF, fcode_opc_OFF},
{"offset16", FCODE_OFFSET16, fcode_opc_OFFSET16},
{"on", FCODE_ON, fcode_opc_ON},
{"open-package", FCODE_OPEN_PACKAGE, fcode_opc_OPEN_PACKAGE},
{"$open-package", FCODE_dOPEN_PACKAGE, fcode_opc_dOPEN_PACKAGE},
{"or", FCODE_OR, fcode_opc_OR},
{"#out", FCODE_sharpOUT, fcode_opc_sharpOUT},
{"over", FCODE_OVER, fcode_opc_OVER},
{"2over", FCODE_2OVER, fcode_opc_2OVER},
{"pack", FCODE_PACK, fcode_opc_PACK},
{"parse-2int", FCODE_PARSE_2INT, fcode_opc_PARSE_2INT},
{"peer", FCODE_PEER, fcode_opc_PEER},
{"pick", FCODE_PICK, fcode_opc_PICK},
{"property", FCODE_PROPERTY, fcode_opc_PROPERTY},
{"r>", FCODE_Rgt, fcode_opc_Rgt},
{"r@", FCODE_Rat, fcode_opc_Rat},
{".r", FCODE_dotR, fcode_opc_dotR},
{">r", FCODE_gtR, fcode_opc_gtR},
{"rb!", FCODE_RBex, fcode_opc_RBex},
{"rb@", FCODE_RBat, fcode_opc_RBat},
{"reg", FCODE_REG, fcode_opc_REG},
{"reset-screen", FCODE_RESET_SCREEN, fcode_opc_RESET_SCREEN},
{"rl!", FCODE_RLex, fcode_opc_RLex},
{"rl@", FCODE_RLat, fcode_opc_RLat},
{"roll", FCODE_ROLL, fcode_opc_ROLL},
{"rot", FCODE_ROT, fcode_opc_ROT},
{"-rot", FCODE_mROT, fcode_opc_mROT},
{"2rot", FCODE_2ROT, fcode_opc_2ROT},
{"rshift", FCODE_RSHIFT, fcode_opc_RSHIFT},
{"rw!", FCODE_RWex, fcode_opc_RWex},
{"rw@", FCODE_RWat, fcode_opc_RWat},
{"#s", FCODE_sharpS, fcode_opc_sharpS},
{".s", FCODE_dotS, fcode_opc_dotS},
{"sbus-intr>cpu", FCODE_SBUS_INTRgtCPU, fcode_opc_SBUS_INTRgtCPU},
{"screen-height", FCODE_SCREEN_HEIGHT, fcode_opc_SCREEN_HEIGHT},
{"screen-width", FCODE_SCREEN_WIDTH, fcode_opc_SCREEN_WIDTH},
{"set-args", FCODE_SET_ARGS, fcode_opc_SET_ARGS},
{"set-font", FCODE_SET_FONT, fcode_opc_SET_FONT},
{"sign", FCODE_SIGN, fcode_opc_SIGN},
{"start0", FCODE_START0, fcode_opc_START0},
{"start1", FCODE_START1, fcode_opc_START1},
{"start2", FCODE_START2, fcode_opc_START2},
{"start4", FCODE_START4, fcode_opc_START4},
{"state", FCODE_STATE, fcode_opc_STATE},
{"suspend-fcode", FCODE_SUSPEND_FCODE, fcode_opc_SUSPEND_FCODE},
{"swap", FCODE_SWAP, fcode_opc_SWAP},
{"2swap", FCODE_2SWAP, fcode_opc_2SWAP},
{"throw", FCODE_THROW, fcode_opc_THROW},
{"toggle-cursor", FCODE_TOGGLE_CURSOR, fcode_opc_TOGGLE_CURSOR},
{"tuck", FCODE_TUCK, fcode_opc_TUCK},
{"type", FCODE_TYPE, fcode_opc_TYPE},
{"u#", FCODE_Usharp, fcode_opc_Usharp},
{"u#>", FCODE_Usharpgt, fcode_opc_Usharpgt},
{"u#s", FCODE_UsharpS, fcode_opc_UsharpS},
{"u.", FCODE_Udot, fcode_opc_Udot},
{"u<", FCODE_Ult, fcode_opc_Ult},
{"u<=", FCODE_Ultequal, fcode_opc_Ultequal},
{"u>", FCODE_Ugt, fcode_opc_Ugt},
{"u>=", FCODE_Ugtequal, fcode_opc_Ugtequal},
{"u2/", FCODE_U2div, fcode_opc_U2div},
{"um*", FCODE_UMmul, fcode_opc_UMmul},
{"um/mod", FCODE_UMdivMOD, fcode_opc_UMdivMOD},
{"u/mod", FCODE_UdivMOD, fcode_opc_UdivMOD},
{"unloop", FCODE_UNLOOP, fcode_opc_UNLOOP},
{"upc", FCODE_UPC, fcode_opc_UPC},
{"u.r", FCODE_UdotR, fcode_opc_UdotR},
{"user-abort", FCODE_USER_ABORT, fcode_opc_USER_ABORT},
{"version1", FCODE_VERSION1, fcode_opc_VERSION1},
{"w!", FCODE_Wex, fcode_opc_Wex},
{"w,", FCODE_Wcomma, fcode_opc_Wcomma},
{"w@", FCODE_Wat, fcode_opc_Wat},
{"/w", FCODE_slashW, fcode_opc_slashW},
{"/w*", FCODE_slashWmul, fcode_opc_slashWmul},
{">w@", FCODE_ltWat, fcode_opc_ltWat},
{"wa+", FCODE_WAplus, fcode_opc_WAplus},
{"wa1+", FCODE_WA1plus, fcode_opc_WA1plus},
{"wbflip", FCODE_WBFLIP, fcode_opc_WBFLIP},
{"wbflips", FCODE_WBFLIPS, fcode_opc_WBFLIPS},
{"wbsplit", FCODE_WBSPLIT, fcode_opc_WBSPLIT},
{"window-left", FCODE_WINDOW_LEFT, fcode_opc_WINDOW_LEFT},
{"window-top", FCODE_WINDOW_TOP, fcode_opc_WINDOW_TOP},
{"within", FCODE_WITHIN, fcode_opc_WITHIN},
{"wljoin", FCODE_WLJOIN, fcode_opc_WLJOIN},
{"wpeek", FCODE_WPEEK, fcode_opc_WPEEK},
{"wpoke", FCODE_WPOKE, fcode_opc_WPOKE},
{"xor", FCODE_XOR, fcode_opc_XOR},

{"base", FCODE_BASE, fcode_opc_XOR},
};

forth_word forth_internal_word_table[] = {
{"b(\")", FCODE_Bstring, fcode_opc_Bstring},
{"b(')", FCODE_Btick, fcode_opc_Btick},
{"b(:)", FCODE_Bcolon, fcode_opc_Bcolon},
{"b(;)", FCODE_Bsemincolon, fcode_opc_Bsemincolon},
{"bbranch", FCODE_BBRANCH, fcode_opc_BBRANCH},
{"b?branch", FCODE_BqBRANCH, fcode_opc_BqBRANCH},
{"b(buffer:)", FCODE_BBUFFERcol, fcode_opc_BBUFFERcol},
{"b(case)", FCODE_BCASE, fcode_opc_BCASE},
{"b(constant)", FCODE_BCONSTANT, fcode_opc_BCONSTANT},
{"b(create)", FCODE_BCREATE, fcode_opc_BCREATE},
{"b(defer)", FCODE_BDEFER, fcode_opc_BDEFER},
{"b(do)", FCODE_BDO, fcode_opc_BDO},
{"b(?do)", FCODE_BqDO, fcode_opc_BqDO},
{"b(endcase)", FCODE_BENDCASE, fcode_opc_BENDCASE},
{"b(endof)", FCODE_BENDOF, fcode_opc_BENDOF},
{"b(field)", FCODE_BFIELD, fcode_opc_BFIELD},
{"b(leave)", FCODE_BLEAVE, fcode_opc_BLEAVE},
{"b(lit)", FCODE_BLIT, fcode_opc_BLIT},
{"b(loop)", FCODE_BLOOP, fcode_opc_BLOOP},
{"b(+loop)", FCODE_BplusLOOP, fcode_opc_BplusLOOP},
{"b(<mark)", FCODE_BltMARK, fcode_opc_BltMARK},
{"b(of)", FCODE_BOF, fcode_opc_BOF},
{"b(>resolve)", FCODE_BgtRESOLVE, fcode_opc_BgtRESOLVE},
{"b(to)", FCODE_BTO, fcode_opc_BTO},
{"b(value)", FCODE_BVALUE, fcode_opc_BVALUE},
{"b(variable)", FCODE_BVARIABLE, fcode_opc_BVARIABLE},
{"external-token", FCODE_EXTERNAL_TOKEN, fcode_opc_EXTERNAL_TOKEN},
{"new-token", FCODE_NEW_TOKEN, fcode_opc_NEW_TOKEN},
};

static char *forth_aliases = ""
" : << lshift ;"
" : >> rshift ;"
" : 1+ 1 + ;"
" : 1- 1 - ;"
" : 2+ 2 + ;"
" : 2- 2 - ;"
" : 3+ 3 + ;"
" : 3- 3 - ;"
" : accept span @ -rot expect span @ swap span ! ;"
" : blank bl fill ;"
" : /c* chars ;"
" : ca1+ char+ ;"
" : .d base @ swap 10 base ! . base ! ;"
" : decimal 10 base ! ;"
" : decode-bytes >r over r@ + swap r@ - rot r> ;"
" : 3drop 2drop drop ;"
" : 3dup 2 pick 2 pick 2 pick ;"
" : eval evaluate ;"
" : false 0 ;"
" : .h base @ swap 16 base ! . base ! ;"
" : hex 16 base ! ;"
" : /n* cells ;"
" : na1+ cell+ ;"
" : not invert ;"
" : space bl emit ;"
" : spaces 0 max 0 ?do space loop ;"
" :  ;"
" :  ;"
" :  ;"
" :  ;"
;

/*
 *	Initialize stuff
 */
static char *forth_prepend = ""
" 10 base !"
" 1 to line#"
" 0 span !" 
;

void forth_build_vocabulary(Container &vocabulary, ForthVM &vm)
{
	for (uint i=0; i < sizeof forth_word_table/sizeof (forth_word); i++) {
		vocabulary.insert(new ForthWordBuildIn(forth_word_table[i].name, forth_word_table[i].fcode, forth_word_table[i].func));
	}
	vocabulary.insert(new ForthWordString("\"", fwstString));
	vocabulary.insert(new ForthWordString("s\"", fwstStringWithHex));
	vocabulary.insert(new ForthWordString(".\"", fwstStringPrint));
	vocabulary.insert(new ForthWordString(".(", fwstStringPrintBracket));
	vocabulary.insert(new ForthWordTick("'"));
	vocabulary.insert(new ForthWordTick("[']"));
	
	vocabulary.insert(new ForthVar("base",0));
	// , FCODE_BASE
	vocabulary.insert(new ForthVar("line#",0));
	// , FCODE_LINEsharp
	vocabulary.insert(new ForthVar("span",0));
	// , FCODE_SPAN
}

#include <cstdlib>
void forth_disassemble(ForthVM &vm)
{
	String s;
	vm.mFCodeBuffer = &s;
	vm.mFCodeBufferIdx = 8;
	bool offset16 = true;
	while (1) {
		uint32 fcode = vm.getFCode();
		switch (fcode) {
		case FCODE_Bstring: {
			byte len = vm.getFCodeByte();
			printf("b(\") \"");
			for (int i=0; i<len; i++) {
				printf("%c", vm.getFCodeByte());
			}
			printf("\" ");
			goto ok;
		}
		case FCODE_BLIT: {
			uint32 a = 0;
			a |= vm.getFCodeByte(); a<<=8;
			a |= vm.getFCodeByte(); a<<=8;
			a |= vm.getFCodeByte(); a<<=8;
			a |= vm.getFCodeByte();
			printf("0x%08x ", a);
			goto ok;
		}
		}
		for (uint i=0; i < sizeof forth_word_table/sizeof (forth_word); i++) {
			if (forth_word_table[i].fcode == fcode) {
				printf("%s ", forth_word_table[i].name);
				goto ok;
			}
		}
		for (uint i=0; i < sizeof forth_internal_word_table/sizeof (forth_word); i++) {
			if (forth_internal_word_table[i].fcode == fcode) {
				printf("%s ", forth_internal_word_table[i].name);
				goto ok2;
			}
		}
		goto test;
		ok2:
		switch (fcode) {
		case FCODE_BqDO:
		case FCODE_BDO:
		case FCODE_BLOOP:
		case FCODE_BOF:
		case FCODE_BENDOF:
		case FCODE_BqBRANCH:
		case FCODE_BBRANCH: {
			if (offset16) {
				uint16 a = 0;
				a |= vm.getFCodeByte(); a<<=8;
				a |= vm.getFCodeByte();
				printf("%04x ", a);
			} else {
				uint8 a = vm.getFCodeByte();
				printf("%02x ", a);
			}
			break;
		}
		case FCODE_Btick:
		case FCODE_NEW_TOKEN:
			fcode = vm.getFCode();
			printf("0x%x ", fcode);
			break;
		case FCODE_EXTERNAL_TOKEN:
		case FCODE_Bstring: {
			byte len = vm.getFCodeByte();
			printf("\"");
			for (int i=0; i<len; i++) {
				printf("%c", vm.getFCodeByte());
			}
			printf("\" ");
			if (fcode == FCODE_EXTERNAL_TOKEN) {
				fcode = vm.getFCode();
				printf("0x%x ", fcode);
			}
			break;
		}
		}
		goto ok;
		test:
		if (fcode >= 0x800) {
			printf("user(0x%x) ", fcode);
			goto ok;
		}
		printf("%x unknown\n", fcode);
		exit(1);
		ok:;
	}
}
