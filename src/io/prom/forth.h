/*
 *	PearPC
 *	forth.h
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

#ifndef __IO_FORTH_H__
#define __IO_FORTH_H__

#include "system/types.h"
#include "tools/data.h"
#include "tools/stream.h"
#include "tools/except.h"

enum ForthPosMode {
    fpmOffset,
    fpmLinePos,
};

class ForthPos: public Object {
	uint32 mOffset;
	int mLine, mPos;
	ForthPosMode mFpm;
public:
			ForthPos();
	virtual	int	toString(char *buf, int buflen) const;
		void	copy(ForthPos &p);
		void	setMode(ForthPosMode fpm);
		void	setLinePos(int line, int pos);
		void	clearPos();
		void	setOffset(uint32 offset);
		void	inc();
		void	inc(int n);
		void	incLine();
};

class ForthException: public MsgException {
public:
			ForthException();
};

class ForthInterpreterException: public ForthException {
public:
			ForthInterpreterException(ForthPos &pos, const char *err, ...);
};

class ForthRunException: public ForthException {
public:
			ForthRunException(ForthPos &pos, const char *err, ...);
};

enum ForthVMMode {
	fmInterprete,
	fmCompile,
};

class ForthVM;

typedef void (*FCodeFunction)(ForthVM &vm);

class ForthVM: public Object {
private:
	Stack	*datastack;
	Stack	*codestack;
	Container *mGlobalVocalbulary;
public:
	char currentChar;
	char mCurToken[50];
	ForthPos mPos;
	ForthPos mErrorPos;
	Stream	*input, *output;
	
	ForthVMMode mMode;
	
	int	mStringBufferIdx;
	char	*mStringBuffer[2];
	uint32	mStringBufferEA[2];
	
	int	mFCodeBufferIdx;
	String	*mFCodeBuffer;
	
	FCodeFunction mFCodes[0xfff];
	
			ForthVM();
			~ForthVM();
			
		void	interprete(Stream &input, Stream &output);
		
		// compile
		void	emitFCode(uint32 fcode);
		void	emitFCodeByte(byte b);
		byte	getFCodeByte();
		uint32	getFCode();
		
		// data stack
		void	dataPush(uint32 value);
		uint32	dataPop();
		bool	dataEmpty();
		uint32	dataGet(uint n=0);
		void	dataClear();
		uint32	dataDepth();
		void *  dataStr(uint32 u, bool exc);

		// code stack
		void	codePush(uint32 value);
		uint32	codePop();
		bool	codeEmpty();
		uint32	codeGet(uint n=0);
		void	codeClear();
		uint32	codeDepth();
		
		// io
		int	outf(const char *m, ...);
		bool	getChar();
		bool	consumeSpace(bool except);
		String &getToken(const String &delimiters);
		bool	skipWhite();
		bool	skipWhiteCR();
		
		// memory
		void	promMalloc(uint32 size, uint32 &ea, void **p);
};

class ForthWord: public Object {
	char *mName;
public:
			ForthWord(const char *name);
			~ForthWord();
			
	virtual	int	compareTo(const Object *obj) const;
	virtual void	compile(ForthVM &vm);
	virtual uint32	getExecToken(ForthVM &vm);
	virtual void	interprete(ForthVM &vm);
	virtual	int	toString(char *buf, int buflen) const;
};

class ForthVar: public ForthWord {
public:
			ForthVar(const char *name, uint32 address);
	virtual void	compile(ForthVM &vm);
	virtual uint32	getExecToken(ForthVM &vm);
	virtual void	interprete(ForthVM &vm);
};

class ForthValue: public ForthWord {
public:
			ForthValue(const char *name, uint32 address);
	virtual void	compile(ForthVM &vm);
	virtual uint32	getExecToken(ForthVM &vm);
	virtual void	interprete(ForthVM &vm);
};

class ForthWordBuildIn: public ForthWord {
	uint32 mFCode;
	FCodeFunction mFunc;
public:
			ForthWordBuildIn(const char *name, uint32 fcode, FCodeFunction func);
	virtual void	compile(ForthVM &vm);
	virtual uint32	getExecToken(ForthVM &vm);
	virtual void	interprete(ForthVM &vm);
};

class ForthWordAlias: public ForthWord {
	int	mNumFCodes;
	uint16 *mFCodes;
public:
			ForthWordAlias(const char *name, int n, ...);
	virtual void	compile(ForthVM &vm);
	virtual void	interprete(ForthVM &vm);
};

enum ForthWordStringType {
	fwstString,
	fwstStringWithHex,
	fwstStringPrint,
	fwstStringPrintBracket,
};

class ForthWordString: public ForthWord {
	ForthWordStringType mFwst;
public:
			ForthWordString(const char *name, ForthWordStringType fwst);
	virtual void	compile(ForthVM &vm);
		String &get(ForthVM &vm, String &s);
	virtual void	interprete(ForthVM &vm);
};

#endif

