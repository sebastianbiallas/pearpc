/*
 *	HT Editor
 *	sys.h
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

#ifndef __SYS_H__
#define __SYS_H__

extern char gAppFilename[260];

/* system-dependent (implementation in $MYSYSTEM/ *.cc) */
/* Note: all functions only take absolute dir/filenames! */
#define	SYSCAP_IPC			1
#define	SYSCAP_NBIPC			2
#define	SYSCAP_NATIVE_CLIPBOARD		4
int		sys_get_caps();

int		sys_ht_mode(int mode);

#define	SYS_DRIVER_DESC_LEN		128
void		sys_get_driver_desc(char *buf);

// return time slice to system
void		sys_suspend();

bool		sys_native_clipboard_read(void *buf, int bufsize);
bool		sys_native_clipboard_write(const void *buf, int bufsize);
int		sys_native_clipboard_get_size();

void *		sys_alloc_read_write_execute(int size);
void		sys_free_read_write_execute(void *p);

bool initOSAPI();
void doneOSAPI();

#endif /* __SYS_H__ */
