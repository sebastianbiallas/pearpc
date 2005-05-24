/*
 *	PearPC
 *	forth.cc
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

#include <cstdlib>
#include <cstring>
#include <cstdarg>

#include "system/display.h"
#include "tools/snprintf.h"
#include "prommem.h"
#include "forth.h"
#include "forthtable.h"

ForthPos::ForthPos()
{
	mFpm = fpmLinePos;
	mLine = 1;
	mPos = 1;
	mOffset = 0;
}

void ForthPos::copy(ForthPos &p) 
{
	mFpm = p.mFpm;
	mLine = p.mLine;
	mPos = p.mPos;
	mOffset = p.mOffset;
}

int ForthPos::toString(char *buf, int buflen) const
{
	if (mFpm == fpmOffset) {
		return ht_snprintf(buf, buflen, "%08x", mOffset);
	} else {
		return ht_snprintf(buf, buflen, "%d:%d", mLine, mPos);
	}
}

void ForthPos::clearPos()
{
	mPos = 0;
}

void ForthPos::setMode(ForthPosMode fpm)
{
	mFpm = fpm;
}

void ForthPos::setLinePos(int line, int pos)
{
	mLine = line;
	mPos = pos;
}

void ForthPos::setOffset(uint32 offset)
{
	mOffset = offset;
}

void ForthPos::inc()
{
	if (mFpm == fpmOffset) {
		mOffset++;
	} else {
		mPos++;
	}
}

void ForthPos::inc(int n)
{
	if (mFpm == fpmOffset) {
		mOffset+=n;
	} else {
		mPos+=n;
	}
}

void ForthPos::incLine()
{
	mLine++;
}

ForthException::ForthException()
{
}

ForthInterpreterException::ForthInterpreterException(ForthPos &pos, const char *msg, ...)
{
	char estr2[120];
	va_list va;
	va_start(va, msg);
	ht_vsnprintf(estr2, sizeof estr2, msg, va);
	va_end(va);
	ht_snprintf(estr, sizeof estr, "%s: %s at %y", "Interpreter Exception", estr2, &pos);
}

ForthRunException::ForthRunException(ForthPos &pos, const char *msg, ...)
{
	char estr2[120];
	va_list va;
	va_start(va, msg);
	ht_vsnprintf(estr2, sizeof estr2, msg, va);
	va_end(va);
	ht_snprintf(estr, sizeof estr, "%s: %s at %y", "Run Exception", estr2, &pos);
}

/*
 *
 */
#define STRING_BUFFER_SIZE 120
ForthVM::ForthVM()
{
	codestack = new Stack(true);
	datastack = new Stack(true);
	mGlobalVocalbulary = new AVLTree(true);
	promMalloc(STRING_BUFFER_SIZE, mStringBufferEA[0], (void**)&(mStringBuffer[0]));
	promMalloc(STRING_BUFFER_SIZE, mStringBufferEA[1], (void**)&(mStringBuffer[1]));
	mStringBufferIdx = 0;
	mFCodeBuffer = new String();
	mFCodeBufferIdx = 0;
	forth_build_vocabulary(*mGlobalVocalbulary, *this);
	
	forth_disassemble(*this);
}

ForthVM::~ForthVM()
{
	delete datastack;
	delete codestack;
	delete mGlobalVocalbulary;
}
	
void ForthVM::emitFCode(uint32 fcode)
{
	if (fcode > 0xfff || (fcode >= 0x01 && fcode <= 0x0f)) {
		throw ForthInterpreterException(mErrorPos, "internal: broken fcode %x", fcode);
	}
	if (fcode > 0xff) {
		emitFCodeByte(fcode>>8);
	}
	emitFCodeByte(fcode);
}

void ForthVM::emitFCodeByte(byte b)
{
	*mFCodeBuffer += (char)b;
}

byte ForthVM::getFCodeByte()
{
	if (mFCodeBufferIdx >= mFCodeBuffer->length()) throw ForthRunException(mErrorPos, "unexpected end of program");
	return (*mFCodeBuffer)[mFCodeBufferIdx++];
}

uint32 ForthVM::getFCode()
{
	uint32 fcode = getFCodeByte();
	if (fcode >= 0x01 && fcode <= 0x0f) {
		fcode <<= 8;
		fcode |= getFCodeByte();
	}
	return fcode;
}

int ForthVM::outf(const char *m, ...)
{
	char b[1024];
	va_list va;
	va_start(va, m);
	int a = ht_vsnprintf(b, sizeof b, m, va);
	va_end(va);
	gDisplay->print(b);
	return a;
}

bool ForthVM::getChar()
{
	if (input->read(&currentChar, 1) != 1) {
//		ht_printf("getChar: false\n");
		return false;
	}
	if (currentChar == 10) {
		mPos.incLine(); 
		mPos.clearPos();
	}
	mPos.inc();
//	ht_printf("getChar: %d '%c'\n", currentChar, currentChar);
	return true;
}

String &ForthVM::getToken(const String &delimiters)
{
	
}

bool ForthVM::consumeSpace(bool except)
{
	return false;	
}

bool ForthVM::skipWhite()
{
	do {
		switch (currentChar) {
		case 9:
		case ' ':
			if (!getChar()) return false;
			continue;
		}
	} while (0);
	return true;
}

bool ForthVM::skipWhiteCR()
{
	while (currentChar == ' ' || currentChar == 10 || currentChar == 13 || currentChar == 9) {
		if (!getChar()) return false;
	}
	return true;
}

void ForthVM::interprete(Stream &in, Stream &out)
{
	input = &in;
	output = &out;	
	mPos.setLinePos(1, 1);
	mPos.setMode(fpmLinePos);
	if (!getChar()) return;
	mMode = fmInterprete;
	while (1) {
		// get a token
		if (!skipWhiteCR()) break;
		int i=0;
		mErrorPos.copy(mPos);
		do {
			if (i==sizeof mCurToken) throw ForthInterpreterException(mErrorPos, "token too long");
			mCurToken[i++] = currentChar;
			if (!getChar()) break;
			if (currentChar==9 || currentChar==10 || currentChar==13 || currentChar==' ') {
				break;
			}
		} while (1);
		mCurToken[i] = 0;
		ForthWordBuildIn fwbi(mCurToken, 0, NULL);
		ForthWord *fw = (ForthWord*)mGlobalVocalbulary->get(mGlobalVocalbulary->find(&fwbi));
		if (fw) {
			if (mMode == fmCompile) {
				fw->compile(*this);
			} else {
				fw->interprete(*this);
			}
		} else {
			throw ForthInterpreterException(mErrorPos, "unkown word '%s'", mCurToken);
		}
	}
}
		
/*
 * data stack
 */
void ForthVM::dataPush(uint32 value)
{
	datastack->push(new UInt(value));
}

uint32 ForthVM::dataPop()
{
	UInt *u = (UInt*)datastack->pop();
	if (!u) {
		throw ForthRunException(mErrorPos, "Stack underflow");
	}
	return u->value;
}

bool ForthVM::dataEmpty()
{
	return datastack->isEmpty();
}

uint32 ForthVM::dataGet(uint n)
{
	UInt *u;
	if (datastack->isEmpty() || !((u = (UInt*)(*datastack)[datastack->count() - n - 1]))) {
		throw ForthRunException(mErrorPos, "Stack underflow");
	}
	return u->value;	
}

void ForthVM::dataClear()
{
	datastack->delAll();
}

uint32 ForthVM::dataDepth()
{
	return datastack->count();
}

void *ForthVM::dataStr(uint32 u, bool exc)
{
	void *p = NULL;//prom_mem_eaptr(u);
	if (!p) throw ForthRunException(mErrorPos, "invalid address");
	return p;
}

/*
 * code stack
 */
void ForthVM::codePush(uint32 value)
{
	codestack->push(new UInt(value));
}

uint32 ForthVM::codePop()
{
	UInt *u = (UInt*)codestack->pop();
	if (!u) {
		throw ForthRunException(mErrorPos, "Codestack underflow");
	}
	return u->value;
}

bool ForthVM::codeEmpty()
{
	return codestack->isEmpty();
}

uint32 ForthVM::codeGet(uint n)
{
	UInt *u;
	if (codestack->isEmpty() || !((u = (UInt*)(*codestack)[codestack->count() - n - 1]))) {
		throw ForthRunException(mErrorPos, "Codestack underflow");
	}
	return u->value;
}

void ForthVM::codeClear()
{
	codestack->delAll();
}

uint32 ForthVM::codeDepth()
{
	return codestack->count();
}

/*
 *	memory
 */
void ForthVM::promMalloc(uint32 size, uint32 &ea, void **p)
{
//	ea = prom_mem_malloc(size);
//	*p = prom_mem_ptr(ea);
//	ea = prom_mem_phys_to_virt(ea);
}

/*
 *
 */
ForthWord::ForthWord(const char *n)
	:Object()
{
	mName = strdup(n);
}

ForthWord::~ForthWord()
{
	free(mName);
}
			
int ForthWord::compareTo(const Object *obj) const
{
	return strcmp(mName, ((ForthWord*)obj)->mName);
}

void ForthWord::compile(ForthVM &vm)
{
	throw ForthInterpreterException(vm.mErrorPos, "internal: no compile method for '%s'", mName);
}

uint32 ForthWord::getExecToken(ForthVM &vm)
{
	throw ForthInterpreterException(vm.mErrorPos, "cannot tick '%s'", mName);
}

void ForthWord::interprete(ForthVM &vm)
{
	throw ForthInterpreterException(vm.mErrorPos, "internal: no interprete method for %s", mName);
}

int ForthWord::toString(char *buf, int buflen) const
{
	return ht_snprintf(buf, buflen, "[WORD:'%s']", mName);
}

ForthWordBuildIn::ForthWordBuildIn(const char *name, uint32 fcode, FCodeFunction func)
	:ForthWord(name)
{
	mFCode = fcode;
	mFunc = func;
}

void ForthWordBuildIn::compile(ForthVM &vm)
{
	vm.emitFCode(mFCode);
}

uint32 ForthWordBuildIn::getExecToken(ForthVM &vm)
{
	return mFCode;
}

void ForthWordBuildIn::interprete(ForthVM &vm)
{
	mFunc(vm);
}

/*
 *
 */
 
ForthWordAlias::ForthWordAlias(const char *name, int n, ...)
	:ForthWord(name)
{
	va_list ap;
	mFCodes = (uint16*)malloc(n*sizeof (uint16));
	for (int i=0; i<n; i++) {
		mFCodes[i] = va_arg(ap, int);
	}
	va_end(ap);
	mNumFCodes = n;
}

void ForthWordAlias::compile(ForthVM &vm)
{
	for (int i=0; i<mNumFCodes; i++) {
		vm.emitFCode(mFCodes[i]);
	}
}

void ForthWordAlias::interprete(ForthVM &vm)
{
}

/*
 *
 */
ForthWordString::ForthWordString(const char *name, ForthWordStringType fwst)
	:ForthWord(name)
{
	mFwst = fwst;
}

void ForthWordString::compile(ForthVM &vm)
{
}

String &ForthWordString::get(ForthVM &vm, String &s)
{
	s = "";
	if (vm.currentChar == 10 || vm.currentChar == 13) return s;
	while (1) {
		if (!vm.getChar()) throw ForthInterpreterException(vm.mErrorPos, "unterminated string");
		switch (mFwst) {
		case fwstString:
		case fwstStringPrint:
			if (vm.currentChar=='"') {
				vm.getChar();
				return s;
			}
			break;
		case fwstStringWithHex:
			if (vm.currentChar=='"') {
				if (!vm.getChar()) return s;
				if (vm.currentChar=='(') {
					// start hex mode and wait for ')'
				} else {
					return s;
				}
			}
			break;
		case fwstStringPrintBracket:
			if (vm.currentChar==')') {
				vm.getChar();
				return s;
			}
			break;
		}
		s += vm.currentChar;
	}
}

void ForthWordString::interprete(ForthVM &vm)
{
	String s;
	get(vm, s);
	switch (mFwst) {
	case fwstString:
	case fwstStringWithHex: {
		int len = s.length();
		memmove(vm.mStringBuffer[vm.mStringBufferIdx], s.content(), MIN(len, STRING_BUFFER_SIZE));
		vm.dataPush(vm.mStringBufferEA[vm.mStringBufferIdx]);
		vm.dataPush(len);
		vm.mStringBufferIdx ^= 1;
		break;
	}
	case fwstStringPrint:
	case fwstStringPrintBracket:
		vm.outf("%y", &s);
		break;
	}
}

ForthVar::ForthVar(const char *name, uint32 address)
	: ForthWord(name)
{
}

void ForthVar::compile(ForthVM &vm)
{
}

uint32 ForthVar::getExecToken(ForthVM &vm)
{
}

void ForthVar::interprete(ForthVM &vm)
{
}

