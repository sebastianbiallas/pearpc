/*
 *	HT Editor
 *	store.cc
 *
 *	Copyright (C) 1999-2003 Sebastian Biallas (sb@biallas.net)
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

#include <cerrno>
#include <cstring>

#include "atom.h"
#include "debug.h"
#include "endianess.h"
#include "snprintf.h"
#include "store.h"
#include "strtools.h"

static char hexchars[]="0123456789abcdef";

char oidchar(ObjectID oID, int byte)
{
	unsigned char c = (oID >> (byte*8)) & 0xff;
	if ((c<32) || (c>0x7f)) c = '?';
	return c;
}

ObjectNotRegisteredException::ObjectNotRegisteredException(ObjectID aID)
	: MsgfException("Object %x/%c%c%c-%x not registered.", aID,
	  oidchar(aID, 3), oidchar(aID, 2), oidchar(aID, 1), aID & 0xff)
{
}

/*
 *	CLASS ObjectStreamInter
 */
 
ObjectStreamInter::ObjectStreamInter(Stream *s, bool own_s)
 : ObjectStream(s, own_s)
{
}

void	ObjectStreamInter::getObject(Object *&object, const char *name, ObjectID id)
{
	if (id == OBJID_INVALID) {
		GET_INT32X(*this, id);
		if (!id) {
			object = NULL;
			return;
		}
	}
	object_builder build = (object_builder)getAtomValue(id);
	if (!build) throw ObjectNotRegisteredException(id);
	object = build();
	object->load(*this);
}

void	ObjectStreamInter::putObject(const Object *object, const char *name, ObjectID id)
{
	if (!object) {
		if (id == OBJID_INVALID) {
			PUTX_INT32X(*this, 0, "id");
			return;
		} else {
			throw IllegalArgumentException(HERE);
		}
	}		
	if (id == OBJID_INVALID) {
		id = object->getObjectID();
		char buf[64];
		char id_str[4];
		id_str[3] = id&0xff;
		id_str[2] = (id>>8)&0xff;
		id_str[1] = (id>>16)&0xff;
		id_str[0] = (id>>24)&0xff;
		escape_special(buf, sizeof buf, id_str, 4);
		putComment(buf);
		PUTX_INT32X(*this, id, "id");
	}
	object_builder build = (object_builder)getAtomValue(id);
	if (!build) {
		throw ObjectNotRegisteredException(id);
	}
	object->store(*this);
}

/*
 *	CLASS ObjectStreamBin
 */
 
ObjectStreamBin::ObjectStreamBin(Stream *s, bool own_s)
 : ObjectStreamInter(s, own_s)
{
}
 
void ObjectStreamBin::getBinary(void *buf, uint size, const char *desc)
{
	mStream->readx(buf, size);
}

bool ObjectStreamBin::getBool(const char *desc)
{
	bool b;
	mStream->readx(&b, 1);
	return b;
}

uint64 ObjectStreamBin::getInt(uint size, const char *desc)
{
	ASSERT(size <= 8);
	byte neta[8];
	mStream->readx(&neta, size);
	return createHostInt64(neta, size, big_endian);
}

char *ObjectStreamBin::getString(const char *desc)
{
	return getstrz(mStream);
}

byte *ObjectStreamBin::getLenString(int &length, const char *desc)
{
	byte b;
	mStream->readx(&b, 1);
	switch (b) {
	case 0xfe: {
		byte neta[2];
		mStream->readx(&neta, 2);
		length = createHostInt(neta, 2, big_endian);
		break;
	}
	case 0xff: {
		byte neta[4];
		mStream->readx(&neta, 4);
		length = createHostInt(neta, 4, big_endian);
		break;
	}
	default:
		length = b;
		break;
	}
	if (length==0) return NULL;
	byte *p = (byte*)malloc(length);
	mStream->readx(p, length);
	return p;
}

void ObjectStreamBin::putBinary(const void *mem, uint size, const char *desc)
{
	mStream->writex(mem, size);
}

void ObjectStreamBin::putBool(bool b, const char *desc)
{
	b = (b) ? 1 : 0;
	mStream->writex(&b, 1);
}

void ObjectStreamBin::putCommentf(const char *comment_format, ...)
{
	// NOP
}

void ObjectStreamBin::putComment(const char *comment)
{
	// NOP
}

void ObjectStreamBin::putInt(uint64 i, uint size, const char *desc, uint int_fmt_hint)
{
	ASSERT(size <= 8);
	byte neta[8];
	createForeignInt64(neta, i, size, big_endian);
	mStream->writex(neta, size);
}

void ObjectStreamBin::putSeparator()
{
	// NOP
}

void ObjectStreamBin::putString(const char *string, const char *desc)
{
	putstrz(this, string);
}

void ObjectStreamBin::putLenString(const byte *string, int len, const char *desc)
{
	byte bLen;
	if (len < 0xfe) {
		bLen = len;
		mStream->writex(&bLen, 1);
	} else if (len <= 0xffff) {
		byte neta[2];
		createForeignInt(neta, len, 2, big_endian);
		bLen = 0xfe;
		mStream->writex(&bLen, 1);
		mStream->writex(neta, 2);
	} else {
		byte neta[4];
		createForeignInt(neta, len, 4, big_endian);
		bLen = 0xff;
		mStream->writex(&bLen, 1);
		mStream->writex(neta, 4);
	}
	mStream->writex(string, len);
}

/*
 *	CLASS ObjectStreamText
 */
 
ObjectStreamText::ObjectStreamText(Stream *s, bool own_s)
 : ObjectStreamInter(s, own_s)
{
	indent = 0;
	cur = ' ';	// must be initialized to a whitespace char
	line = 1;
	errorline = 0;
}

void	ObjectStreamText::getBinary(void *buf, uint size, const char *desc)
{
	readDesc(desc);
	expect('=');
	expect('[');
	byte *pp=(byte *)buf;
	for (uint i=0; i<size; i++) {
		skipWhite();

		int bb;
		if ((bb = hexdigit(cur))==-1) setSyntaxError();
		int b = bb*16;

		readChar();
		if ((bb = hexdigit(cur))==-1) setSyntaxError();
		b += bb;

		*pp++=b;

		readChar();
	}
	expect(']');
}

bool ObjectStreamText::getBool(const char *desc)
{
	readDesc(desc);
	expect('=');
	skipWhite();
	if (cur=='f') {
		readDesc("false");
		return false;
	} else {
		readDesc("true");
		return true;
	}
}

static char mapchar(char c)
{
	return 0;
}

uint64 ObjectStreamText::getInt(uint size, const char *desc)
{
	readDesc(desc);
	expect('=');
	skipWhite();
	if (mapchar(cur)!='0') setSyntaxError();
	char str[40];
	char *s=str;
	do {
		*s++ = cur;
		if (s-str >= 39) setSyntaxError();
		readChar();
	} while (mapchar(cur)=='0' || mapchar(cur)=='A');
	*s=0; s=str;
	uint64 a;
	const char *s2 = s;
	if (!parseIntStr(s2, a, 10)) setSyntaxError();
	return a;
}

void	ObjectStreamText::getObject(Object *&object, const char *name, ObjectID id)
{
	readDesc(name);
	expect('=');
	expect('{');
	ObjectStreamInter::getObject(object, name, id);
	expect('}');
}

char *ObjectStreamText::getString(const char *desc)
{
	readDesc(desc);
	expect('=');
	skipWhite();
	if (cur=='"') {
		String s;
		do {
			readChar();
			s += cur;
			if (cur=='\\') {
				readChar();
				s += cur;
				cur = 0; // hackish
			}
		} while (cur != '"');
		readChar();
		int str2l = s.length();
		char *str2 = (char *)malloc(str2l);
		unescape_special_str(str2, str2l, s);
		return str2;
	} else {
		readDesc("NULL");
		return NULL;
	}
}

byte *ObjectStreamText::getLenString(int &len, const char *desc)
{
	readDesc(desc);
	expect('=');
	skipWhite();
	if (cur=='"') {
		String s;
		do {
			readChar();
			s += cur;
			if (cur=='\\') {
				readChar();
				s += cur;
				cur = 0; // hackish
			}
		} while (cur != '"');
		readChar();
		len = s.length()-1;
		if (!len) return NULL;
		byte *str2 = (byte *)malloc(len);
		unescape_special(str2, len, s);
		return str2;
	} else {
		readDesc("NULL");
		return NULL;
	}
}

void ObjectStreamText::putBinary(const void *mem, uint size, const char *desc)
{
	putDesc(desc);
	putChar('[');
	for (uint i=0; i<size; i++) {
		byte a = *((byte *)mem+i);
		putChar(hexchars[(a & 0xf0) >> 4]);
		putChar(hexchars[(a & 0x0f)]);
		if (i+1<size) putChar(' ');
	}
	putS("]\n");
}

void ObjectStreamText::putBool(bool b, const char *desc)
{
	putDesc(desc);
	if (b) putS("true"); else putS("false");
	putChar('\n');
}

void ObjectStreamText::putComment(const char *comment)
{
	putIndent();
	putS("# ");
	putS(comment);
	putChar('\n');
}

void ObjectStreamText::putInt(uint64 i, uint size, const char *desc, uint int_fmt_hint)
{
	putDesc(desc);
	char number[40];
	switch (int_fmt_hint) {
		case OS_FMT_DEC:
			ht_snprintf(number, sizeof number, "%qd\n", i);
			break;
		case OS_FMT_HEX:
		default:
			ht_snprintf(number, sizeof number, "0x%qx\n", i);
			break;
	}
	putS(number);
}

void ObjectStreamText::putObject(const Object *object, const char *name, ObjectID id)
{
	putDesc(name);
	putS("{\n");
	indent++;
	ObjectStreamInter::putObject(object, name, id);
	indent--;
	putIndent();
	putS("}\n");
}

void ObjectStreamText::putSeparator()
{
	putIndent();
	putS("# ------------------------ \n");
}

void ObjectStreamText::putString(const char *string, const char *desc)
{
	putDesc(desc);
	if (string) {
		int strl=strlen(string)*4+1;
		char *str = (char*)malloc(strl);
		putChar('"');
		escape_special_str(str, strl, string, "\"");
		putS(str);
		putChar('"');
		free(str);
	} else {
		putS("NULL");
	}
	putChar('\n');
}

void ObjectStreamText::putLenString(const byte *string, int len, const char *desc)
{
	putDesc(desc);
	if (string) {
		int strl=len*4+1;
		char *str = (char*)malloc(strl);
		putChar('"');
		escape_special(str, strl, string, len, "\"");
		putS(str);
		putChar('"');
		free(str);
	} else {
		putS("NULL");
	}
	putChar('\n');
}

class TextSyntaxError: public MsgfException {
public:
	TextSyntaxError::TextSyntaxError(uint line)
		: MsgfException("syntax error in line %d", line)
	{
	}
};

void	ObjectStreamText::setSyntaxError()
{
// FIXME: errorline still usable ?
	if (!errorline) {
		errorline = line;
		throw TextSyntaxError(line);
	}
}

int	ObjectStreamText::getErrorLine()
{
	return errorline;
}

void	ObjectStreamText::expect(char c)
{
	skipWhite();
	if (cur!=c) setSyntaxError();
	readChar();
}

void	ObjectStreamText::skipWhite()
{
	while (1) {
		switch (mapchar(cur)) {
			case '\n':
				line++;  // fallthrough
			case ' ':
				readChar();
				break;
			case '#':
				do {
					readChar();
				} while (cur!='\n');
				break;
			default: return;
		}
	}
}

char	ObjectStreamText::readChar()
{
	mStream->readx(&cur, 1);
	return cur;
}

void	ObjectStreamText::readDesc(const char *desc)
{
	skipWhite();
	if (!desc) desc="data";
	while (*desc) {
		if (*desc!=cur) setSyntaxError();
		readChar();
		desc++;
	}
}

void ObjectStreamText::putDesc(const char *desc)
{
	putIndent();
	if (desc) putS(desc); else putS("data");
	putChar('=');
}

void ObjectStreamText::putIndent()
{
	for(int i=0; i<indent; i++) putChar(' ');
}

void ObjectStreamText::putChar(char c)
{
	mStream->writex(&c, 1);
}

void ObjectStreamText::putS(const char *s)
{
	uint len=strlen(s);
	if (mStream->write(s, len) != len) setSyntaxError();
}

/*
 *   ObjectStreamNative View:set/getData() methods
 *	(endian-dependend)
 */

ObjectStreamNative::ObjectStreamNative(Stream *s, bool own_s, bool d)
: ObjectStream(s, own_s), allocd(true)
{
	duplicate = d;
}

void	*ObjectStreamNative::duppa(const void *p, int size)
{
	if (duplicate) {
		MemArea *m = new MemArea(p, size, true);
		allocd += m;
		return m->ptr;
	} else {
		// FIXME: un-const'ing p
		return (void*)p;
	}
}

void ObjectStreamNative::getBinary(void *buf, uint size, const char *desc)
{
	void *pp;
	mStream->readx(&pp, sizeof pp);
	memmove(buf, pp, size);
}

bool ObjectStreamNative::getBool(const char *desc)
{
	bool b;
	mStream->readx(&b, sizeof b);
	return b;
}

uint64 ObjectStreamNative::getInt(uint size, const char *desc)
{
	switch (size) {
		case 1:case 2:case 4: {
			uint i = 0;
			mStream->readx(&i, size);
			return i;
		}
		case 8: {
			uint64 i;
			mStream->readx(&i, size);
			return i;
		}
	}
	throw IllegalArgumentException(HERE);
}

void ObjectStreamNative::getObject(Object *&object, const char *name, ObjectID id)
{
	Object *pp;
	mStream->readx(&pp, sizeof pp);
	object = pp;
}

char	*ObjectStreamNative::getString(const char *desc)
{
	char *pp;
	mStream->readx(&pp, sizeof pp);
	return pp;
}

byte	*ObjectStreamNative::getLenString(int &len, const char *desc)
{
	byte *pp;
	mStream->readx(&pp, sizeof pp);
	// FIXME?
	if (pp) len = strlen((char*)pp); else len = 0;
	return pp;
}

void	ObjectStreamNative::putBinary(const void *mem, uint size, const char *desc)
{
	void *pp = mem ? duppa(mem, size) : NULL;
	mStream->writex(&pp, sizeof pp);
}

void	ObjectStreamNative::putBool(bool b, const char *desc)
{
	mStream->writex(&b, sizeof b);
}

void	ObjectStreamNative::putComment(const char *comment)
{
	// NOP
}

void	ObjectStreamNative::putInt(uint64 i, uint size, const char *desc, uint int_fmt_hint)
{
	switch (size) {
		case 1:case 2:case 4: {
			uint x = i;
			mStream->writex(&x, size);
			return;
		}
		case 8: {
			mStream->writex(&i, size);
			return;
		}
	}
	throw IllegalArgumentException(HERE);
}

void ObjectStreamNative::putObject(const Object *object, const char *name, ObjectID id)
{
	Object *d = duplicate ? d->clone() : d;
	mStream->write(&d, sizeof d);
}

void	ObjectStreamNative::putSeparator()
{
	// NOP
}

void	ObjectStreamNative::putString(const char *string, const char *desc)
{
	const char *pp = string ? (const char*)duppa(string, strlen(string)+1) : NULL;
	mStream->write(&pp, sizeof pp);
}

void	ObjectStreamNative::putLenString(const byte *string, int len, const char *desc)
{
	const char *pp = string ? (const char*)duppa(string, len+1) : NULL;
	mStream->write(&pp, sizeof pp);
}

