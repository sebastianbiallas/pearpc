/*
 *	HT Editor
 *	strtools.cc
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf
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

#include "atom.h"
#include "debug.h"
#include "except.h"
#include "snprintf.h"
#include "stream.h"
#include "strtools.h"
#include "system/types.h"

#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

char hexchars[17]="0123456789abcdef";

char *ht_strdup(const char *str)
{
	if (str) {
		int len = strlen(str)+1;
		char *s = (char*)malloc(len);
		memmove(s, str, len);
		return s;
	} else {
		return NULL;
	}
}


/**
 *	Like ht_strdup but dups a maximum of |maxlen| characters of |str|.
 *	@returns new string
 */
char *ht_strndup(const char *str, size_t maxlen)
{
	maxlen ++;
	if (str) {
		uint len = strlen(str)+1;
		len = MIN(len, maxlen);
		char *s = (char*)malloc(len);
		memmove(s, str, len);
		s[len-1] = 0;
		return s;
	} else {
		return NULL;
	}
}

/**
 *	Like strcpy but copies a maximum of |maxlen| characters
 *	(including trailing zero).
 *	The operation is performed in a way that the trailing zero
 *	is always written if maxlen is > 0.
 *	@returns number of characters copied (without trailing zero)
 */
int ht_strncpy(char *s1, const char *s2, size_t maxlen)
{
	if (maxlen <= 0) return 0;
	char *os1 = s1;
	while (maxlen && *s2) {
		*s1 = *s2;
		maxlen--;
		s1++;
	}
	s1[-1] = 0;
	return s1-os1-1;
}

int ht_strncmp(const char *s1, const char *s2, size_t max)
{
	if (!s1) return s2 ? -1 : 0;
	if (!s2) return s1 ? 1 : 0;
	while (max--) {
		if (!*s1) return *s2 ? -1 : 0;
		if (!*s2) return *s1 ? 1 : 0;
		if (*s1>*s2) {
			return 1;
		} else if (*s1<*s2) {
			return -1;
		}
		s1++;s2++;
	}
	return 0;
}

int ht_strnicmp(const char *s1, const char *s2, size_t max)
{
	if (!s1) return s2 ? -1 : 0;
	if (!s2) return s1 ? 1 : 0;
	while (max--) {
		if (!*s1) return *s2 ? -1 : 0;
		if (!*s2) return *s1 ? 1 : 0;
		char c1=tolower(*s1), c2=tolower(*s2);
		if (c1>c2) {
			return 1;
		} else if (c1<c2) {
			return -1;
		}
		s1++;s2++;
	}
	return 0;
}

int ht_stricmp(const char *s1, const char *s2)
{
	if (!s1) return s2 ? -1 : 0;
	if (!s2) return s1 ? 1 : 0;
	while (1) {
		if (!*s1) return *s2 ? -1 : 0;
		if (!*s2) return *s1 ? 1 : 0;
		char c1=tolower(*s1), c2=tolower(*s2);
		if (c1>c2) {
			return 1;
		} else if (c1<c2) {
			return -1;
		}
		s1++;s2++;
	}
	return 0;
}

int strccomm(const char *s1, const char *s2)
{
	if (!s1 || !s2) return 0;
	int r=0;
	while (*s1 && *s2 && (*s1==*s2)) { s1++; s2++; r++; }
	return r;
}

int strcicomm(const char *s1, const char *s2)
{
	if (!s1 || !s2) return 0;
	int r=0;
	while (*s1 && *s2 && (tolower(*s1)==tolower(*s2))) { s1++; s2++; r++; }
	return r;
}

int escape_special_str(char *result, int resultmaxlen, const char *s, const char *specialchars, bool bit7)
{
	return escape_special(result, resultmaxlen, s, strlen(s), specialchars, bit7);
}

int escape_special(char *result, int resultmaxlen, const void *S, int len, const char *specialchars, bool bit7)
{
	byte *s = (byte*)S;
	if (!s) return 0;
	char *old = result;
	if (!specialchars) specialchars="";
	while (len--) {
		if (*s && strchr(specialchars, *s)) {
			*result++='\\';
			if (--resultmaxlen<2) break;
			*result++=*s;
			if (--resultmaxlen<2) break;
		} else if (*s<32 || (bit7 && (*s>0x7f)) || *s=='\\') {
			*result++='\\';
			if (--resultmaxlen<2) break;
			switch (*s) {
				case '\0':
					*result++='0';
					break;
				case '\a':
					*result++='a';
					break;
				case '\b':
					*result++='b';
					break;
				case '\e':
					*result++='e';
					break;
				case '\f':
					*result++='f';
					break;
				case '\n':
					*result++='n';
					break;
				case '\r':
					*result++='r';
					break;
				case '\t':
					*result++='t';
					break;
				case '\v':
					*result++='v';
					break;
				case '\\':
					*result++='\\';
					break;
				case '\"':
					*result++='"';
					break;
				default:
					*result++='x';
					if (--resultmaxlen<2) break;
					*result++=hexchars[((*s & 0xf0) >> 4)];
					if (--resultmaxlen<2) break;
					*result++=hexchars[(*s & 0x0f)];
			}
			if (--resultmaxlen<2) break;
		} else {
			*result++ = *s;
			if (--resultmaxlen<2) break;
		}
		s++;
	}
	*result = 0;
	return result-old;
}

int unescape_special_str(char *result, int resultmaxlen, const char *s)
{
	int l=unescape_special(result, resultmaxlen-1, s);
	result[l]=0;
	return l;
}

int unescape_special(void *Result, int resultmaxlen, const char *s)
{
	char *result = (char*)Result;
	char *old = result;
	while (s && *s) {
		if (*s == '\\') {
			s++;
			switch (*s) {
				case '0':
					*result++='\0';
					break;
				case 'a':
					*result++='\a';
					break;
				case 'b':
					*result++='\b';
					break;
				case 'e':
					*result++='\e';
					break;
				case 'f':
					*result++='\f';
					break;
				case 'n':
					*result++='\n';
					break;
				case 'r':
					*result++='\r';
					break;
				case 't':
					*result++='\t';
					break;
				case 'v':
					*result++='\v';
					break;
				case '\\':
					*result++='\\';
					break;
				case '\"':
					*result++='"';
					break;
				case 'x':
					s++;
					byte v=hexdigit(*s)*16;
					s++;
					v+=hexdigit(*s);
					*result++=(char)v;
			}
			if (!--resultmaxlen) break;
		} else {
			*result++ = *s;
			if (!--resultmaxlen) break;
		}
		s++;
	}
	return result-old;
}

int bin2str(char *result, const void *S, int len)
{
	byte *s = (byte*)S;
	while (len--) {
//		if (*s<32) *result=' '; else *result=*s;
		if (*s==0) *result=' '; else *result=*s;
		result++;
		s++;
	}
	*result=0;
	return len;
}

void wide_char_to_multi_byte(char *result, const byte *Unicode, int maxlen)
{
	if (!maxlen) return;
	struct doof {
		char c1;
		char c2;
	};
	doof *unicode = (doof*)Unicode;
	for (int i=0; i<maxlen-1; i++) {
		if (unicode->c2) {
			*result++ = '?';
		} else {
			if (!unicode->c1) break;
			*result++ = unicode->c1;
		}
		unicode++;
	}
	*result=0;
}

void memdowncase(byte *buf, int len)
{
	for (int i=0; i<len; i++) {
		if ((buf[i]>='A') && (buf[i]<='Z')) buf[i]+=32;
	}
}

byte *ht_memmem(const byte *haystack, int haystack_len, const byte *needle, int needle_len)
{
	while (haystack_len && (haystack_len >= needle_len)) {
		if (memcmp(haystack, needle, needle_len) == 0) return (byte*)haystack;
		haystack++;
		haystack_len--;
	}
	return NULL;
}

/* common string parsing functions */
void whitespaces(const char *&str)
{
	while ((unsigned char)*str<=32) {
		if (!*str) return;
		str++;
	}
}

void non_whitespaces(const char *&str)
{
	while ((unsigned char)*str>32) {
		str++;
	}
}

bool waitforchar(const char *&str, char b)
{
	while (*str != b) {
		if (!*str) return false;
		str++;
	}
	return true;
}

static bool bnstr2int(const char *&str, uint64 &u64, int base)
{
	u64 = 0;
	uint64 ubase = base;
	int i = 0;
	do {
		int c = hexdigit(*str);
		if ((c == -1) || (c >= base)) return (i == 0) ? false : true;
		u64 *= ubase;
		u64 += c;
		str++;
		i++;
	} while (*str);
	return true;
}

bool parseIntStr(const char *&str, uint64 &u64, int defaultbase)
{
	int base = defaultbase;
	if ((base == 10) && strncmp("0x", str, 2) == 0) {
		str += 2;
		base = 16;
	}
	return bnstr2int(str, u64, base);
}

/* hex/string functions */
int hexdigit(char a)
{
	if ((a>='0') && (a<='9')) {
		return a-'0';
	} else if ((a>='a') && (a<='f')) {
		return a-'a'+10;
	} else if ((a>='A') && (a<='F')) {
		return a-'A'+10;
	}
	return -1;
}

bool hexb_ex(uint8 &result, const char *s)
{
	int v, w;
	v = hexdigit(s[0]);
	w = hexdigit(s[1]);
	if ((v < 0) || (w < 0)) return false;
	result = (v<<4) | w;
	return true;
}

bool hexw_ex(uint16 &result, const char *s)
{
	uint8 v, w;
	if (!hexb_ex(v, s) || !hexb_ex(w, s+2)) return false;
	result = (v<<8) | w;
	return true;
}

bool hexd_ex(uint32 &result, const char *s)
{
	uint16 v, w;
	if (!hexw_ex(v, s) || !hexw_ex(w, s+4)) return false;
	result = (v<<16) | w;
	return true;
}

char *mkhexb(char *buf, uint8 d)
{
	*buf++=hexchars[(d>>4)&0xf];
	*buf++=hexchars[d&0xf];
	return buf;
}

char *mkhexw(char *buf, uint16 d)
{
	*buf++=hexchars[(d>>12)&0xf];
	*buf++=hexchars[(d>>8)&0xf];
	*buf++=hexchars[(d>>4)&0xf];
	*buf++=hexchars[d&0xf];
	return buf;
}

char *mkhexd(char *buf, uint32 d)
{
	*buf++=hexchars[(d>>28)&0xf];
	*buf++=hexchars[(d>>24)&0xf];
	*buf++=hexchars[(d>>20)&0xf];
	*buf++=hexchars[(d>>16)&0xf];
	*buf++=hexchars[(d>>12)&0xf];
	*buf++=hexchars[(d>>8)&0xf];
	*buf++=hexchars[(d>>4)&0xf];
	*buf++=hexchars[d&0xf];
	return buf;
}

char *mkhexq(char *buf, uint64 u)
{
	*buf++ = hexchars[(u>>60)&0xf];
	*buf++ = hexchars[(u>>56)&0xf];
	*buf++ = hexchars[(u>>52)&0xf];
	*buf++ = hexchars[(u>>48)&0xf];
	*buf++ = hexchars[(u>>44)&0xf];
	*buf++ = hexchars[(u>>40)&0xf];
	*buf++ = hexchars[(u>>36)&0xf];
	*buf++ = hexchars[(u>>32)&0xf];
	*buf++ = hexchars[(u>>28)&0xf];
	*buf++ = hexchars[(u>>24)&0xf];
	*buf++ = hexchars[(u>>20)&0xf];
	*buf++ = hexchars[(u>>16)&0xf];
	*buf++ = hexchars[(u>>12)&0xf];
	*buf++ = hexchars[(u>>8)&0xf];
	*buf++ = hexchars[(u>>4)&0xf];
	*buf++ = hexchars[(u>>0)&0xf];
	return buf;
}
