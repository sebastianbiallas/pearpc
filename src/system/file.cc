/* 
 *	HT Editor
 *	file.cc
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

#include "file.h"

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

/*
 *	COMMON SYS
 */

#ifndef S_IFMT
#define S_IFMT 0xf000
#endif

#ifndef S_ISREG
#	ifndef S_IFREG
#		define S_ISREG(m) (0)
#	else
#		define S_ISREG(m) (((m) & S_IFMT)==S_IFREG)
#	endif
#endif

#ifndef S_ISBLK
#	ifndef S_IFBLK
#		define S_ISBLK(m) (0)
#	else
#		define S_ISBLK(m) (((m) & S_IFMT)==S_IFBLK)
#	endif
#endif


#ifndef S_ISCHR
#	ifndef S_IFCHR
#		define S_ISCHR(m) (0)
#	else
#		define S_ISCHR(m) (((m) & S_IFMT)==S_IFCHR)
#	endif
#endif

#ifndef S_ISDIR
#	ifndef S_IFDIR
#		define S_ISDIR(m) (0)
#	else
#		define S_ISDIR(m) (((m) & S_IFMT)==S_IFDIR)
#	endif
#endif

#ifndef S_ISFIFO
#	ifndef S_IFFIFO
#		define S_ISFIFO(m) (0)
#	else
#		define S_ISFIFO(m) (((m) & S_IFMT)==S_IFFIFO)
#	endif
#endif

#ifndef S_ISLNK
#	ifndef S_IFLNK
#		define S_ISLNK(m) (0)
#	else
#		define S_ISLNK(m) (((m) & S_IFMT)==S_IFLNK)
#	endif
#endif

#ifndef S_ISSOCK
#	ifndef S_IFSOCK
#		define S_ISSOCK(m) (0)
#	else
#		define S_ISSOCK(m) (((m) & S_IFMT)==S_IFSOCK)
#	endif
#endif

#ifndef S_IRUSR
#define S_IRUSR 0
#endif
#ifndef S_IRGRP
#define S_IRGRP 0
#endif
#ifndef S_IROTH
#define S_IROTH 0
#endif

#ifndef S_IWUSR
#define S_IWUSR 0
#endif
#ifndef S_IWGRP
#define S_IWGRP 0
#endif
#ifndef S_IWOTH
#define S_IWOTH 0
#endif

#ifndef S_IXUSR
#define S_IXUSR 0
#endif
#ifndef S_IXGRP
#define S_IXGRP 0
#endif
#ifndef S_IXOTH
#define S_IXOTH 0
#endif

int sys_basename(char *result, const char *filename)
{
// FIXME: use is_path_delim
	char *slash1 = strrchr(filename, '/');
	char *slash2 = strrchr(filename, '\\');
	char *slash=(slash1>slash2) ? slash1 : slash2;
	if (slash) {
		int l=strlen(filename);
		strncpy(result, slash+1, l-(slash-filename)-1);
		result[l-(slash-filename)-1]=0;
		return 0;
	}
	strcpy(result, filename);
	return 0;
}

int sys_dirname(char *result, const char *filename)
{
// FIXME: use is_path_delim
	char *slash1 = strrchr(filename, '/');
	char *slash2 = strrchr(filename, '\\');
	char *slash = (slash1>slash2) ? slash1 : slash2;
	if (slash) {
		strncpy(result, filename, slash-filename);
		result[slash-filename] = 0;
		return 0;
	}
	strcpy(result, ".");
	return 0;
}

/* filename and pathname must be canonicalized */
int sys_relname(char *result, const char *filename, const char *cwd)
{
	const char *f = filename, *p = cwd;
	while ((*f == *p) && (*f)) {
		f++;
		p++;
	}
	if (*f == '/') f++;
	const char *last = f, *h = f;
	while (*h) {
		if (*h == '/') {
			*(result++) = '.';
			*(result++) = '.';
			*(result++) = '/';
			last = h+1;
		}
		h++;
	}
	while (f<last) {
		*(result++) = *f;
		f++;
	}
	*result = 0;
	strcat(result, last);
	return 0;
}

int sys_ht_mode(int mode)
{
	int m = 0;
	if (S_ISREG(mode)) {
		m |= HT_S_IFREG;
	} else if (S_ISBLK(mode)) {
		m |= HT_S_IFBLK;
	} else if (S_ISCHR(mode)) {
		m |= HT_S_IFCHR;
	} else if (S_ISDIR(mode)) {
		m |= HT_S_IFDIR;
	} else if (S_ISFIFO(mode)) {
		m |= HT_S_IFFIFO;
	} else if (S_ISLNK(mode)) {
		m |= HT_S_IFLNK;
	} else if (S_ISSOCK(mode)) {
		m |= HT_S_IFSOCK;
	}
	if (mode & S_IRUSR) m |= HT_S_IRUSR;
	if (mode & S_IRGRP) m |= HT_S_IRGRP;
	if (mode & S_IROTH) m |= HT_S_IROTH;
	
	if (mode & S_IWUSR) m |= HT_S_IWUSR;
	if (mode & S_IWGRP) m |= HT_S_IWGRP;
	if (mode & S_IWOTH) m |= HT_S_IWOTH;
	
	if (mode & S_IXUSR) m |= HT_S_IXUSR;
	if (mode & S_IXGRP) m |= HT_S_IXGRP;
	if (mode & S_IXOTH) m |= HT_S_IXOTH;
	return m;
}

static char *next_delim(char *s, is_path_delim delim)
{
	while (*s) {
		s++;
		if (delim(*s)) return s;
	}
	return NULL;
}

static int flatten_path(char *path, is_path_delim delim)
{
	if (!path || !*path)
		return 0;
	char *q = next_delim(path, delim);
	int pp = flatten_path(q, delim);
	int ll = q ? (q-path-1) : strlen(path)-1;
	if ((ll == 2) && (strncmp(path+1, "..", 2) == 0)) {
		if (q) memmove(path, q, strlen(q)+1); else *path = 0;
		pp++;
	} else if ((ll == 1) && (strncmp(path+1, ".", 1) == 0)) {
		if (q) memmove(path, q, strlen(q)+1); else *path = 0;
	} else if (pp) {
		if (q) memmove(path, q, strlen(q)+1); else *path = 0;
		pp--;
	}
	return pp;
}

bool sys_path_is_absolute(const char *filename, is_path_delim delim)
{
	return delim(filename[0]) || (isalpha(filename[0]) && (filename[1] == ':'));
}

int sys_common_canonicalize(char *result, const char *filename, const char *cwd, is_path_delim delim)
{
	char *o = result;
	if (!sys_path_is_absolute(filename, delim)) {
		if (cwd) strcpy(o, cwd); else return EINVAL;
		int ol = strlen(o);
		if (ol && !delim(o[ol-1])) {
			o[ol] = '/';
			o[ol+1] = 0;
		}
	} else *o = 0;
	strcat(o, filename);
	int k = flatten_path(o, delim);
	return (k == 0) ? 0 : EINVAL;
}

char *sys_filename_suffix(const char *fn)
{
	const char *s = NULL;
	while (fn && *fn) {
		if (sys_is_path_delim(*fn)) s = fn+1;
		fn++;
	}
	char *p = s ? strrchr(s, '.') : NULL;
	return p ? p+1 : NULL;
}

int sys_tmpfile_fd()
{
#if 1
// FIXME: this might leak something...
	FILE *f = tmpfile();
	return fileno(f);
#else
// is this better ?
	return open(tmpnam(NULL), O_RDWR | O_EXCL | O_CREAT);
#endif
}
