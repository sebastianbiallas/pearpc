/* 
 *	HT Editor
 *	sysexcept.h
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

#ifndef __SYSEXCEPT_H__
#define __SYSEXCEPT_H__

#define SYS_EXCEPTION_STR_MAX 256

#include <exception>

class sys_exception: public std::exception {
private:
	char str[SYS_EXCEPTION_STR_MAX];
public:
				sys_exception(char *s);
	virtual	const char *	what() const throw();
};

#endif /* __SYSEXCEPT_H__ */
