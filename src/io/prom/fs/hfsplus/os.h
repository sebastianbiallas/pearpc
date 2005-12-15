/*
 * libhfs - library for reading and writing Macintosh HFS volumes
 * Copyright (C) 1996-1998 Robert Leslie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*
 * NAME:	os->open()
 * DESCRIPTION:	open and lock a new descriptor from the given path and mode
 */
int hfsp_os_open(void **priv, const void *devicehandle, int mode);

/*
 * NAME:	os->close()
 * DESCRIPTION:	close an open descriptor
 */
int hfsp_os_close(void **priv);

/*
 * NAME:	os->same()
 * DESCRIPTION:	return 1 iff path is same as the open descriptor
 */
int hfsp_os_same(void **priv, const char *path);


/** Offset that is silently used by os_seek.
 *  Needed to transparaently support things like partitions */
/*extern APPLEUInt64 os_offset;*/
uint64 hfsp_get_offset(void **priv);
void hfsp_set_offset(void **priv, uint64 ofs);

/*
 * NAME:	os->seek()
 * DESCRIPTION:	set a descriptor's seek pointer (offset in blocks)
 */
uint64 hfsp_os_seek(void **priv, uint64 offset, int blksize_bits);

/*
 * NAME:	os->read()
 * DESCRIPTION:	read blocks from an open descriptor
 */
unsigned long hfsp_os_read(void **priv, void *buf, unsigned long len, int blksize_bits);

/*
 * NAME:	os->write()
 * DESCRIPTION:	write blocks to an open descriptor
 */
unsigned long hfsp_os_write(void **priv, const void *buf, unsigned long len, int blksize_bits);
