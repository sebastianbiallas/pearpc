/* 
 *	PearPC
 *	sysfile.cc - file system functions for Win32
 *
 *	Copyright (C) 1999-2004 Sebastian Biallas (sb@biallas.net)
 *	Copyright (C) 1999-2002 Stefan Weyergraf (stefan@weyergraf.de)
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

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>

#include "system/file.h"
#include "system/sys.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

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


struct winfindstate {
	HANDLE fhandle;
	WIN32_FIND_DATA find_data;
};

int sys_file_mode(int mode)
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

bool sys_filename_is_absolute(const char *filename)
{
	if (strlen(filename) < 3) return false;
	return (isalpha(filename[0]) && (filename[1] == ':') &&
		sys_is_path_delim(filename[2]));
}

int sys_canonicalize(char *result, const char *filename)
{
	if (!sys_filename_is_absolute(filename)) return ENOENT;
	char *dunno;
	return (GetFullPathName(filename, HT_NAME_MAX, result, &dunno) > 0) ? 0 : ENOENT;
}

uint filetime_to_ctime(FILETIME f)
{
	uint64 q;
	q = ((uint64)f.dwHighDateTime<<32)|((uint64)f.dwLowDateTime);
	q = q / 10000000ULL;		// 100 nano-sec to full sec
	return q + 1240431886;	// MAGIC: this is 1.1.1970 minus 1.1.1601 in seconds
}

void sys_findfill(pfind_t &pfind)
{
	/*DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD    nFileSizeHigh;
	DWORD    nFileSizeLow;
	DWORD    dwReserved0;
	DWORD    dwReserved1;
	TCHAR    cFileName[ MAX_PATH ];
	TCHAR    cAlternateFileName[ 14 ];*/
	winfindstate *wfs=(winfindstate*)pfind.findstate;
	pfind.name = (char *)&wfs->find_data.cFileName;
	pfind.stat.caps = pstat_ctime|pstat_mtime|pstat_atime|pstat_size|pstat_mode_type;

	pfind.stat.size = wfs->find_data.nFileSizeLow | (((uint64)wfs->find_data.nFileSizeHigh) << 32);

	pfind.stat.mode = (wfs->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? HT_S_IFDIR : HT_S_IFREG;
	pfind.stat.ctime = filetime_to_ctime(wfs->find_data.ftCreationTime);
	pfind.stat.mtime = filetime_to_ctime(wfs->find_data.ftLastWriteTime);
	pfind.stat.atime = filetime_to_ctime(wfs->find_data.ftLastAccessTime);
}

int sys_findfirst(pfind_t &pfind, const char *dirname)
{
	if (!sys_filename_is_absolute(dirname)) return ENOENT;
	char *Dirname = (char *)malloc(strlen(dirname)+5);
	strcpy(Dirname, dirname);
	int dnl=strlen(dirname);
	if ((dirname[dnl-1]!='\\') && (dirname[dnl-1]!='/')) {
		Dirname[dnl]='\\';
		Dirname[dnl+1]=0;
	}
	char *s=Dirname;
	while ((s=strchr(s, '/'))) *s='\\';
	strcat(Dirname, "*.*");

	pfind.findstate=malloc(sizeof (winfindstate));
	winfindstate *wfs=(winfindstate*)pfind.findstate;

	wfs->fhandle = FindFirstFile(Dirname, &wfs->find_data);
	free(Dirname);
	if (wfs->fhandle == INVALID_HANDLE_VALUE) {
		free(pfind.findstate);
		return ENOENT;
	}
	sys_findfill(pfind);
	return 0;
}

int sys_findnext(pfind_t &pfind)
{
	winfindstate *wfs=(winfindstate*)pfind.findstate;
	
	if (!FindNextFile(wfs->fhandle, &wfs->find_data)) {
		return ENOENT;
	}
	sys_findfill(pfind);
	return 0;
}

int sys_findclose(pfind_t &pfind)
{
	int r=FindClose(((winfindstate*)pfind.findstate)->fhandle);
	free(pfind.findstate);
	return r ? ENOENT : 0;
}

static void stat_to_pstat_t(const struct stat &st, pstat_t &s)
{
	s.caps = pstat_ctime|pstat_mtime|pstat_atime|pstat_uid|pstat_gid|pstat_mode_all|pstat_size|pstat_inode;
	s.ctime = st.st_ctime;
	s.mtime = st.st_mtime;
	s.atime = st.st_atime;
	s.gid = st.st_uid;
	s.uid = st.st_gid;
	s.mode = sys_file_mode(st.st_mode);
	s.size = st.st_size;
	s.fsid = st.st_ino;
}

int sys_pstat(pstat_t &s, const char *filename)
{
	if (!sys_filename_is_absolute(filename)) return ENOENT;
	struct stat st;
	int e = stat(filename, &st);
	if (e) return errno ? errno : ENOENT;
	stat_to_pstat_t(st, s);
	return 0;
}

int sys_pstat_fd(pstat_t &s, int fd)
{
	struct stat st;
	int e = fstat(fd, &st);
	if (e) return errno ? errno : ENOENT;
	stat_to_pstat_t(st, s);
	return 0;
}

void sys_suspend()
{
	Sleep(0);
}

/*int sys_get_free_mem()
{
	return 0;
}*/

int sys_truncate(const char *filename, FileOfs ofs)
{
	if (!sys_filename_is_absolute(filename)) return ENOENT;
	HANDLE hfile = CreateFile(filename, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hfile == INVALID_HANDLE_VALUE) {
		return EIO;
	}
	// FIXME: uint64
	if (SetFilePointer(hfile, ofs, NULL, FILE_BEGIN)==0xffffffff) {
		CloseHandle(hfile);
		return EIO;
	}
	if (!SetEndOfFile(hfile)) {
		CloseHandle(hfile);
		return EIO;
	}
	CloseHandle(hfile);
	return 0;
}

int sys_truncate_fd(int fd, FileOfs ofs)
{
	return ENOSYS;
}

int sys_deletefile(const char *filename)
{
	if (!sys_filename_is_absolute(filename)) return ENOENT;
	if (DeleteFile(filename)) {
		return 0;
	}
	return EIO;
}

bool sys_is_path_delim(char c)
{
	return (c == '\\');
}

int sys_filename_cmp(const char *a, const char *b)
{
	while (*a && *b) {
		if (sys_is_path_delim(*a) && sys_is_path_delim(*b)) {
		} else if (tolower(*a) != tolower(*b)) {
			break;
		} else if (*a != *b) {
			break;
		}
		a++;
		b++;
	}
	return tolower(*a) - tolower(*b);
}

SYS_FILE *sys_fopen(const char *filename, int openmode)
{
	HANDLE *f = (HANDLE *)malloc(sizeof (HANDLE));
	if (openmode & SYS_OPEN_CREATE) {
		*f = CreateFile(filename, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
	} else {
		if (openmode & SYS_OPEN_WRITE) {
			*f = CreateFile(filename, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
		} else {
			*f = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
		}
	}
	if (*f == INVALID_HANDLE_VALUE) {
		free(f);
		return NULL;
	}
	return (SYS_FILE *)f;
}

void sys_fclose(SYS_FILE *file)
{
	CloseHandle(*(HANDLE *)file);
	free(file);
}

int sys_fread(SYS_FILE *file, byte *buf, int size)
{
	DWORD read;
	ReadFile(*(HANDLE *)file, buf, size, &read, NULL);
	return read;
}

int sys_fwrite(SYS_FILE *file, byte *buf, int size)
{
	DWORD written;
	WriteFile(*(HANDLE *)file, buf, size, &written, NULL);
	return written;
}

int sys_fseek(SYS_FILE *file, FileOfs newofs, int seekmode)
{
	DWORD m;
	switch (seekmode) {
	case SYS_SEEK_SET: m = FILE_BEGIN; break;
	case SYS_SEEK_REL: m = FILE_CURRENT; break;
	case SYS_SEEK_END: m = FILE_END; break;
	default: return EINVAL;
	}
	LONG newofs_h = newofs >> 32;
	if (SetFilePointer(*(HANDLE *)file, newofs, &newofs_h, m) == 0xffffffff) {
		return EINVAL;
	} else {
		return 0;
	}
}

void sys_flush(SYS_FILE *file)
{
	FlushFileBuffers(*(HANDLE *)file);
}

FileOfs	sys_ftell(SYS_FILE *file)
{
	LONG a, b=0;
	a = SetFilePointer(*(HANDLE *)file, 0, &b, FILE_CURRENT);
	return (((FileOfs)b)<<32)+((uint32)a);
}

void *sys_alloc_read_write_execute(int size)
{
	return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}

void sys_free_read_write_execute(void *p)
{
	VirtualFree(p, 0, MEM_DECOMMIT | MEM_RELEASE);
}
