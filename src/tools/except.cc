/* 
 *	HT Editor
 *	except.cc
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
#include <cstdarg>
#include <cstring>

#include "except.h"
#include "snprintf.h"

/*
 *	class Exception
 */

Exception::Exception() throw()
{
}

Exception::~Exception()
{
}

String &Exception::reason(String &result) const
{
	result = "Unknown Exception";
	return result;
}

/*
 *	class MsgException
 */
MsgException::MsgException() throw()
{
}

MsgException::MsgException(const char *e) throw()
{
	strncpy(estr, e, sizeof estr-1);
	estr[sizeof estr-1] = 0;
}

String &MsgException::reason(String &result) const
{
	result = estr;
	return result;
}

/*
 *	class MsgfException
 */
MsgfException::MsgfException(const char *e,...) throw()
{
	va_list va;
	va_start(va, e);
	ht_vsnprintf(estr, sizeof estr, e, va);
	va_end(va);
}

/*
 *	class IOException
 */

//#include <signal.h>
IOException::IOException(int aPosixErrno) throw()
{
	mPosixErrno = aPosixErrno;
	errstr = strerror(mPosixErrno);
//	raise(SIGTRAP);
}

IOException::~IOException()
{
}

String &IOException::reason(String &result) const
{
	result = "I/O error: " + errstr;
	return result;
}

/*
 *	class NotImplementedException
 */

NotImplementedException::NotImplementedException(const String &filename, int line_number) throw()
{
	if (line_number) {
		location.assignFormat("%y:%d", &filename, line_number);
	} else {
		location = filename;
	}
}

String &NotImplementedException::reason(String &result) const
{
	result = "Function not implemented";
	if (!location.isEmpty()) result += ": "+location;
	return result;
}

/*
 *	class IllegalArgumentException
 */

IllegalArgumentException::IllegalArgumentException(const String &filename, int line_number) throw()
{
	if (line_number) {
		location.assignFormat("%y:%d", &filename, line_number);
	} else {
		location = filename;
	}
}

String &IllegalArgumentException::reason(String &result) const
{
	result = "Illegal argument";
	if (!location.isEmpty()) result += ": "+location;
	return result;
}

/*
 *	class TypeCastException
 */

TypeCastException::TypeCastException(const String &cast_type, const String &obj_type) throw()
{
	aresult.assignFormat("(%y) %y", &cast_type, &obj_type);
}

String &TypeCastException::reason(String &result) const
{
	result = "Bad type cast";
	if (!aresult.isEmpty()) result += ": "+aresult;
	return result;
}

