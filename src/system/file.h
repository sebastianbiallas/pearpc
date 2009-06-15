/*
 *	HT Editor
 *	file.h
 *
 *	file system functions
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

#ifndef __FILE_H__
#define __FILE_H__

/* Note: all functions only take absolute dir/filenames ! */

#include "types.h"
#include "fileofs.h"

//#include <dirent.h>
#include <sys/types.h>
#include <time.h>

#ifdef PATH_MAX
#define HT_NAME_MAX PATH_MAX	/* DJGPP at least */
#else
#ifdef MAXNAMLEN
#define HT_NAME_MAX MAXNAMLEN	/* some BSD... */
#else
#ifdef NAME_MAX
#define HT_NAME_MAX NAME_MAX	/* POSIX and friends... */
#else
#define HT_NAME_MAX 260		/* unknown... */
#endif
#endif
#endif

#define HT_S_IFREG         	0x1000
#define HT_S_IFBLK         	0x2000
#define HT_S_IFCHR         	0x3000
#define HT_S_IFDIR         	0x4000
#define HT_S_IFFIFO        	0x5000
#define HT_S_IFLNK         	0x6000
#define HT_S_IFSOCK        	0x7000

#define HT_S_IFMT		0xf000

#define HT_S_ISREG(m)		(((m) & HT_S_IFMT) == HT_S_IFREG)
#define HT_S_ISBLK(m)		(((m) & HT_S_IFMT) == HT_S_IFBLK)
#define HT_S_ISCHR(m)		(((m) & HT_S_IFMT) == HT_S_IFCHR)
#define HT_S_ISDIR(m)		(((m) & HT_S_IFMT) == HT_S_IFDIR)
#define HT_S_ISFIFO(m)		(((m) & HT_S_IFMT) == HT_S_IFFIFO)
#define HT_S_ISLNK(m)		(((m) & HT_S_IFMT) == HT_S_IFLNK)
#define HT_S_ISSOCK(m)		(((m) & HT_S_IFMT) == HT_S_IFSOCK)

#define HT_S_IRUSR		0x0100
#define HT_S_IRGRP		0x0020
#define HT_S_IROTH		0x0004
#define HT_S_IWUSR		0x0080
#define HT_S_IWGRP		0x0010
#define HT_S_IWOTH		0x0002
#define HT_S_IXUSR		0x0040
#define HT_S_IXGRP		0x0008
#define HT_S_IXOTH		0x0001
#define HT_S_IRWXU		(HT_S_IRUSR || HT_S_IWUSR || HT_S_IXUSR)
#define HT_S_IRWXG		(HT_S_IRGRP || HT_S_IWGRP || HT_S_IXGRP)
#define HT_S_IRWXO		(HT_S_IROTH || HT_S_IWOTH || HT_S_IXOTH)

#define pstat_ctime		0x00000001
#define pstat_mtime		0x00000002
#define pstat_atime		0x00000004
#define pstat_uid		0x00000008
#define pstat_gid		0x00000010
#define pstat_mode_usr		0x00000020
#define pstat_mode_grp		0x00000040
#define pstat_mode_oth		0x00000080
#define pstat_mode_r          	0x00000100
#define pstat_mode_w          	0x00000200
#define pstat_mode_x         	0x00000400
#define pstat_mode_type		0x00000800
#define pstat_size		0x00001000
#define pstat_inode		0x00002000
#define pstat_cluster		0x00004000
#define pstat_fsid		0x00008000
#define pstat_desc		0x00010000

#define pstat_mode_all		(pstat_mode_usr|pstat_mode_grp|pstat_mode_oth|pstat_mode_r|pstat_mode_w|pstat_mode_x|pstat_mode_type)

struct pstat_t {
	uint32	caps;
	time_t	ctime;
	time_t	mtime;
	time_t	atime;
	uint		uid;
	uint		gid;
	mode_t	mode;	// S_ISUID, S_ISGID, S_I[RWX](USR|GRP|OTH)
	uint64	size;
	union {
		uint	inode;
		uint	cluster;
		uint	fsid;
	};
	char desc[32];
};

struct pfind_t {
	const char *name;
	pstat_t stat;
	void *findstate;
};

typedef bool (*is_path_delim)(char c);

#define	SYS_OPEN_READ   1
#define	SYS_OPEN_WRITE  2
#define	SYS_OPEN_CREATE 4

#define	SYS_SEEK_SET 1
#define	SYS_SEEK_REL 2
#define	SYS_SEEK_END 3

#define	SYS_FILE void


/* system-independent (implementation in sys.cc) */
int		sys_basename(char *result, const char *filename);
int		sys_dirname(char *result, const char *filename);
int		sys_relname(char *result, const char *filename, const char *cwd);
int		sys_common_canonicalize(char *result, const char *in_name, const char *cwd, is_path_delim delim);
const char *	sys_filename_suffix(const char *fn);
int		sys_tmpfile_fd();

/* system-dependent (implementation in $MYSYSTEM/ *.cc) */
int		sys_canonicalize(char *result, const char *filename);
int		sys_file_mode(int mode);
int		sys_findclose(pfind_t &pfind);
int		sys_findfirst(pfind_t &pfind, const char *dirname);
int		sys_findnext(pfind_t &pfind);
int		sys_pstat(pstat_t &s, const char *filename);
int		sys_pstat_fd(pstat_t &s, int fd);
int		sys_truncate(const char *filename, FileOfs ofs);
int		sys_truncate_fd(int fd, FileOfs ofs);
int		sys_deletefile(const char *filename);
bool		sys_is_path_delim(char c);
int		sys_filename_cmp(const char *a, const char *b);
bool		sys_filename_is_absolute(const char *filename);

SYS_FILE *	sys_fopen(const char *filename, int openmode);
void		sys_fclose(SYS_FILE *file);
int		sys_fread(SYS_FILE *file, byte *buf, int size);
int		sys_fwrite(SYS_FILE *file, byte *buf, int size);
int		sys_fseek(SYS_FILE *file, FileOfs newofs, int seekmode = SYS_SEEK_SET);
FileOfs		sys_ftell(SYS_FILE *file);
void		sys_flush(SYS_FILE *file);
//int		sys_geterror();

#endif /* __FILE_H__ */
