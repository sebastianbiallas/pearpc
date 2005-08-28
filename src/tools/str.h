/*
 *	HT Editor
 *	str.h
 *
 *	Copyright (C) 2002 Stefan Weyergraf
 *	Copyright (C) 2002, 2003 Sebastian Biallas (sb@biallas.net)
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

#ifndef __STR_H__
#define __STR_H__

#include "data.h"

enum StringCase {
	stringCaseLower,
	stringCaseUpper,
	stringCaseCaps
};

/**
 *	Class for easy string handling.
 */
class String: public Object {
protected:
	int mLength;
	byte *mContent;
public:
				String();
				String(const char *s);
				String(const String *s);
				String(const String &s);
				String(const byte *s, int aLength);
				String(char c, int count = 1);
	virtual			~String();

		void		assign(const String *s);
		void		assign(const String &s);
		void		assign(const char *s);
		void		assign(const byte *s, int aLength);
		void		assign(char c, int count = 1);
		void		assignFormat(const char *s, ...);

		void		append(const String &s);
		void		append(const char *s);
		void		appendChar(char c);
	inline	char &		at(int aIndex) const;
	inline	bool		chop();
		void		clear();
	virtual	String *	clone() const;
	virtual	int		compareChar(char c1, char c2) const;
	virtual	int		compareTo(const Object *o) const;
		int		compare(const String &s) const;
		int		compare(const String &s, int aMax) const;
	inline	byte *		content() const;
	inline	char *		contentChar() const;
		void		crop(int aNewLength);
		void		del(int pos, int aLength);
		void		escape(const char *aSpecialChars, bool bit7 = true);
	virtual	int		findFirstChar(char c, int start = -1) const;
	virtual	int		findFirstString(const String &s, int start = -1) const;
	virtual	int		findLastChar(char c, int start = -1) const;
	virtual	int		findLastString(const String &s, int start = -1) const;
	inline	char		firstChar() const;
		void		insert(const String &s, int pos);
#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	inline	bool		isEmpty() const;
	virtual	ObjectID	getObjectID() const;
#endif
	inline	char		lastChar() const;
	inline	int		length() const;
		bool		leftSplit(char chr, String &initial, String &rem) const;
		void		prepend(const String &s);
//		bool		regexMatch(const String &aRegEx, Container *resultStrings = NULL, int maxRegExMatches = 32) const;
//		bool		regexReplace(const String &aRegEx, Container *resultStrings = NULL) const;
		int		replace(const String &what, const String &with, int start = 0, int maxReplacements = -1);
		bool		rightSplit(char chr, String &initial, String &rem) const;
		int		subString(int aStart, int aLength, String &result) const;
		void		transformCase(StringCase c);
		void		translate(const String &inAlpha, const String &outAlpha);
	virtual	int		toArray(byte *buf, int buflen) const;
		bool		toInt(int &i, int defaultbase=10) const;
		bool		toInt32(uint32 &u32, int defaultbase=10) const;
		bool		toInt64(uint64 &u64, int defaultbase=10) const;
	virtual	int		toString(char *buf, int buflen) const;
	virtual	char *		toString() const;
		void		unescape();

	inline	char &	operator [](int aIndex) const;

	inline	String &	operator =(const String &s);
	inline	String &	operator =(const char *s);
	inline	String &	operator +=(const String &s);
	inline	String &	operator +=(const char *s);
		String &	operator +=(char c);
			
	inline	bool		operator < (const String &s) const;
	inline	bool		operator > (const String &s) const;
	inline	bool		operator <=(const String &s) const;
	inline	bool		operator >=(const String &s) const;
	inline	bool		operator ==(const String &s) const;
	inline	bool		operator !=(const String &s) const;

	inline	bool		operator < (const char *s) const;
	inline	bool		operator > (const char *s) const;
	inline	bool		operator <=(const char *s) const;
	inline	bool		operator >=(const char *s) const;
	inline	bool		operator ==(const char *s) const;
	inline	bool		operator !=(const char *s) const;

protected:
		int		compare(const char *s) const;
		void		realloc(int aNewSize);
};

String operator +(const String &s1, const String &s2);
String operator +(const char *s1, const String &s2);

/**
 *	case-insensitive string.
 */
class IString: public String {
public:
				IString();

	virtual	IString *	clone() const;
	virtual	int		compareChar(char c1, char c2) const;
#ifdef HAVE_HT_OBJECTS
	virtual	bool		instanceOf(ObjectID id) const;
	virtual	ObjectID	getObjectID() const;
#endif
};

/*
 *	inline functions
 */

//class MsgException;

/**
 *	@returns char at position |aIndex|
 *	@throws exception if aIndex out of bounds
 */
inline char &String::at(int aIndex) const
{
//	if ((uint)aIndex >= (uint)mLength) throw new MsgException("index out of bounds");
	return (char &)mContent[aIndex];
}

/**
 *	Removes the last character of the string if string length is non-zero.
 */
inline bool String::chop()
{
	if (mLength) {
		crop(mLength-1);
		return true;
	} else {
		return false;
	}
}

/**
 *	@returns a string content ptr
 */
inline byte *String::content() const
{
	return mContent;
}
 
/**
 *	@returns a string content ptr (as char*)
 */
inline char *String::contentChar() const
{
	return (char*)mContent;
}
 
/**
 *	@returns first character of string
 *	@throws exception if string is empty
 */
inline char String::firstChar() const
{
	return at(0);
}

/**
 *	@returns true if string is empty.
 */
inline bool String::isEmpty() const
{
	return mLength == 0;
}

/**
 *	@returns last character of string
 *	@throws exception if string is empty
 */
inline char String::lastChar() const
{
	return at(mLength-1);
}

/**
 *	@returns length of string
 */
inline int String::length() const
{
	return mLength;
}

inline char &String::operator [](int aIndex) const
{
	return at(aIndex);
}

inline String &String::operator =(const String &s)
{
	assign(s);
	return *this;
}

inline String &String::operator =(const char *s)
{
	assign(s);
	return *this;
}

inline String &String::operator +=(const String &s)
{
	append(s);
	return *this;
}

inline String &String::operator +=(const char *s)
{
	append(s);
	return *this;
}

inline bool String::operator < (const String &s) const
{
	return compare(s) < 0;
}

inline bool String::operator <= (const String &s) const
{
	return compare(s) <= 0;
}

inline bool String::operator > (const String &s) const
{
	return compare(s) > 0;
}

inline bool String::operator >= (const String &s) const
{
	return compare(s) >= 0;
}

inline bool String::operator == (const String &s) const
{
	return compare(s) == 0;
}

inline bool String::operator != (const String &s) const
{
	return compare(s) != 0;
}

inline bool String::operator < (const char *s) const
{
	return compare(s) < 0;
}

inline bool String::operator <= (const char *s) const
{
	return compare(s) <= 0;
}

inline bool String::operator > (const char *s) const
{
	return compare(s) > 0;
}

inline bool String::operator >= (const char *s) const
{
	return compare(s) >= 0;
}

inline bool String::operator == (const char *s) const
{
	return compare(s) == 0;
}

inline bool String::operator != (const char *s) const
{
	return compare(s) != 0;
}

#endif
