/*
 *	HT Editor
 *	store.h
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

#ifndef STORE_H
#define STORE_H

#include "data.h"
#include "sys/types.h"

typedef Object *(*object_builder)();

#include "stream.h"
#include "except.h"

class ObjectNotRegisteredException: public MsgfException {
	ObjectID mID;
public:
	ObjectNotRegisteredException(ObjectID aID);
};

class ObjectStreamInter: public ObjectStream {
public:
				ObjectStreamInter(Stream *s, bool own_stream);
	/* extends ObjectStream */
	virtual void	getObject(Object *&object, const char *name, ObjectID id = OBJID_INVALID);
	virtual void	putObject(const Object *object, const char *name, ObjectID id = OBJID_INVALID);
};

class ObjectStreamBin: public ObjectStreamInter {
public:
				ObjectStreamBin(Stream *s, bool own_s);
	/* extends ObjectStream */
	virtual void		getBinary(void *buf, uint size, const char *desc);
	virtual bool		getBool(const char *desc);
	virtual uint64		getInt(uint size, const char *desc);
	virtual char *		getString(const char *desc);
	virtual byte *		getLenString(int &length, const char *desc);

	virtual void		putBinary(const void *mem, uint size, const char *desc);
	virtual void		putBool(bool b, const char *desc);
	virtual void		putComment(const char *comment);
	virtual void		putCommentf(const char *comment_format, ...);
	virtual void		putInt(uint64 i, uint size, const char *desc, uint int_fmt_hint = OS_FMT_DEC);
	virtual void		putSeparator();
	virtual void		putString(const char *string, const char *desc);
	virtual void		putLenString(const byte *string, int length, const char *desc);
};

class ObjectStreamText: public ObjectStreamInter {
protected:
	char		cur;
	int		line;
	int		errorline;
	int		indent;
public:
	   
					ObjectStreamText(Stream *s, bool own_stream);
	/* extends ObjectStreamInter */
	virtual void		getObject(Object *&object, const char *name, ObjectID id = OBJID_INVALID);
	virtual void		putObject(const Object *object, const char *name, ObjectID id = OBJID_INVALID);
	/* extends ObjectStream */
	virtual void		getBinary(void *buf, uint size, const char *desc);
	virtual bool		getBool(const char *desc);
	virtual uint64		getInt(uint size, const char *desc);
	virtual char *		getString(const char *desc);
	virtual byte *		getLenString(int &length, const char *desc);
	
	virtual void		putBinary(const void *mem, uint size, const char *desc);
	virtual void		putBool(bool b, const char *desc);
	virtual void		putComment(const char *comment);
	virtual void		putInt(uint64 i, uint size, const char *desc, uint int_fmt_hint = OS_FMT_DEC);
	virtual void		putSeparator();
	virtual void		putString(const char *string, const char *desc);
	virtual void		putLenString(const byte *string, int length, const char *desc);

		   void		setSyntaxError();
		   int		getErrorLine();
private:
/* io */
		   void	expect(char c);
		   void	skipWhite();
		   char	readChar();
		   void	readDesc(const char *desc);

		   
		   void	putDesc(const char *desc);
		   void	putIndent();
		   void	putChar(char c);
		   void	putS(const char *s);
};

/*
 *   ObjectStreamNative View:set/getData() methods
 *	(endian-dependend)
 */

#define DATABUF_BOOL(name)		bool		name __htpacked
#define DATABUF_UINT(name)		uint		name __htpacked
#define DATABUF_PTR(type, name)		type*		name __htpacked

class ObjectStreamNative: public ObjectStream {
protected:
	bool duplicate;
	Array allocd;

		   void	*duppa(const void *p, int size);
public:
				ObjectStreamNative(Stream *s, bool own_s, bool duplicate);
/* extends ObjectStream */
	virtual void		getBinary(void *buf, uint size, const char *desc);
	virtual bool		getBool(const char *desc);
	virtual uint64		getInt(uint size, const char *desc);
	virtual void		getObject(Object *&object, const char *name, ObjectID id = OBJID_INVALID);
	virtual char *		getString(const char *desc);
	virtual byte *		getLenString(int &length, const char *desc);

	virtual void		putBinary(const void *mem, uint size, const char *desc);
	virtual void		putBool(bool b, const char *desc);
	virtual void		putComment(const char *comment);
	virtual void		putInt(uint64 i, uint size, const char *desc, uint int_fmt_hint = OS_FMT_DEC);
	virtual void		putObject(const Object *object, const char *name, ObjectID id = OBJID_INVALID);
	virtual void		putSeparator();
	virtual void		putString(const char *string, const char *desc);
	virtual void		putLenString(const byte *string, int length, const char *desc);
};

#endif
