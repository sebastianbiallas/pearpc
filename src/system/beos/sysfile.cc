/* 
 *	HT Editor
 *	sysfile.cc - file system functions for POSIX
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf (stefan@weyergraf.de)
 *	Copyright (C) 2004 Francois Revol (revol@free.fr)
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
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <OS.h>
#include <Entry.h>
#include <Path.h>

#include <limits.h>    /* for PAGESIZE */
#ifndef PAGESIZE
#ifdef B_PAGE_SIZE
#define PAGESIZE B_PAGE_SIZE
#else
#define PAGESIZE 4096
#endif
#endif


#define USE_AREAS

#ifndef HAVE_REALPATH
char *realpath(const char *filename, char *result)
{
	BPath p;
	BEntry ent(filename, true); /* traverse symlinks */
	if (ent.InitCheck())
		return NULL;
	if (ent.GetPath(&p))
		return NULL;
	strcpy(result, p.Path());
	return result;
}
#endif

#include "system/file.h"

#include <dirent.h>

struct posixfindstate {
	DIR *fhandle;
};

inline bool sys_filename_is_absolute(const char *filename)
{
	return sys_is_path_delim(filename[0]);
}

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
#ifdef S_ISSOCK
	} else if (S_ISSOCK(mode)) {
		m |= HT_S_IFSOCK;
#endif
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

bool sys_is_path_delim(char c)
{
	return c == '/';
}

int sys_filename_cmp(const char *a, const char *b)
{
	while (*a && *b) {
		if (sys_is_path_delim(*a) && sys_is_path_delim(*b)) {
		} else if (*a != *b) {
			break;
		}
		a++;
		b++;
	}
	return *a - *b;
}

int sys_canonicalize(char *result, const char *filename)
{
	if (!sys_filename_is_absolute(filename)) return ENOENT;
	return (realpath(filename, result)==result) ? 0 : ENOENT;
}

static char sys_find_dirname[HT_NAME_MAX];

int sys_findclose(pfind_t &pfind)
{
	int r = closedir(((posixfindstate*)pfind.findstate)->fhandle);
	free(pfind.findstate);
	return r;
}

int sys_findfirst(pfind_t &pfind, const char *dirname)
{
	if (!sys_filename_is_absolute(dirname)) return ENOENT;
	int r;
	pfind.findstate = malloc(sizeof (posixfindstate));
	posixfindstate *pfs = (posixfindstate*)pfind.findstate;
	if ((pfs->fhandle = opendir(dirname))) {
		strcpy(sys_find_dirname, dirname);
		char *s = sys_find_dirname+strlen(sys_find_dirname);
		if ((s > sys_find_dirname) && (*(s-1) != '/')) {
		    *(s++) = '/';
		    *s = 0;
		}
		r = sys_findnext(pfind);
	} else r = errno ? errno : ENOENT;
	if (r) free(pfind.findstate);
	return r;
}

int sys_findnext(pfind_t &pfind)
{
	posixfindstate *pfs = (posixfindstate*)pfind.findstate;
	struct dirent *d;
	if ((d = readdir(pfs->fhandle))) {
		pfind.name = d->d_name;
		char *s = sys_find_dirname+strlen(sys_find_dirname);
		strcpy(s, d->d_name);
		sys_pstat(pfind.stat, sys_find_dirname);
		*s = 0;
		return 0;
	}
	return ENOENT;
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
	errno = 0;
	int e = lstat(filename, &st);
	if (e) return errno ? errno : ENOENT;
	stat_to_pstat_t(st, s);
	return 0;
}

int sys_pstat_fd(pstat_t &s, int fd)
{
	struct stat st;
	errno = 0;
	int e = fstat(fd, &st);
	if (e) return errno ? errno : ENOENT;
	stat_to_pstat_t(st, s);
	return 0;
}

int sys_truncate(const char *filename, FileOfs ofs)
{
	if (!sys_filename_is_absolute(filename)) return ENOENT;
	int fd = open(filename, O_RDWR, 0);
	if (fd < 0) return errno;
	if (ftruncate(fd, ofs) != 0) return errno;
	return close(fd);
}

int sys_deletefile(const char *filename)
{
	if (!sys_filename_is_absolute(filename)) return ENOENT;
	return remove(filename);
}

int sys_truncate_fd(int fd, FileOfs ofs)
{
	if (ftruncate(fd, ofs) != 0) return errno;
	return 0;
}

void sys_suspend()
{
	snooze(100LL);
}

int sys_get_free_mem()
{
	return 0;
}

SYS_FILE *sys_fopen(const char *filename, int openmode)
{
	if (openmode & SYS_OPEN_CREATE) {
		return (SYS_FILE *)fopen(filename, "w+");
	} else {
		if (openmode & SYS_OPEN_WRITE) {
			return (SYS_FILE *)fopen(filename, "r+");
		} else {
			return (SYS_FILE *)fopen(filename, "r");
		}
	}
}

void sys_fclose(SYS_FILE *file)
{
	fclose((FILE *)file);
}

int sys_fread(SYS_FILE *file, byte *buf, int size)
{
	return fread(buf, 1, size, (FILE *)file);
}

int sys_fwrite(SYS_FILE *file, byte *buf, int size)
{
	return fwrite(buf, 1, size, (FILE *)file);
}

int sys_fseek(SYS_FILE *file, FileOfs newofs, int seekmode)
{
	int r;
	switch (seekmode) {
	case SYS_SEEK_SET: r = _fseek((FILE *)file, newofs, SEEK_SET); break;
	case SYS_SEEK_REL: r = _fseek((FILE *)file, newofs, SEEK_CUR); break;
	case SYS_SEEK_END: r = _fseek((FILE *)file, newofs, SEEK_END); break;
	default: return EINVAL;
	}
	return r ? errno : 0;
}

FileOfs	sys_ftell(SYS_FILE *file)
{
	fpos_t pos;
	int err;

// BeOS *is* 64 bits :p
// ftell and fseek use long !! whoever invented the buffered io stuff should be slapped :^)
// _fseek does work, but not _ftell... use _fgetpos

	err = fgetpos((FILE *)file, &pos);
	if (err < 0)
		return err;
	return pos;
}

void *sys_alloc_read_write_execute(int size)
{
#ifdef USE_AREAS
	area_id id;
	void *addr;
	size = ((size + PAGESIZE-1) & ~(PAGESIZE-1));
	id = create_area("PearPC rwx area", &addr, B_ANY_ADDRESS, size, B_NO_LOCK, B_READ_AREA|B_WRITE_AREA);
	fprintf(stderr, "create_area(, , , %d, , ) = 0x%08lx\n", size, id);
	if (id < B_OK)
		return NULL;
	return addr;
#else
	void *p = malloc(size+PAGESIZE-1);
	if (!p) return NULL;
	
	void *ret = (void *)(((int)p + PAGESIZE-1) & ~(PAGESIZE-1));

#ifdef HAVE_MPROTECT	
	if (mprotect(ret, size, PROT_READ | PROT_WRITE | PROT_EXEC)) {
		free(p);
		return NULL;
	}
#endif
	return ret;
#endif
}

void sys_free_read_write_execute(void *p)
{
#ifdef USE_AREAS
	area_id id;
	id = area_for(p);
	if (id < B_OK)
		return;
	//delete_area(id);
#endif
}
