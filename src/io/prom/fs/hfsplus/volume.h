/*
 * libhfs - library for reading and writing Macintosh HFS volumes
 *
 * Copyright (C) 2000 Klaus Halfmann <klaus.halfmann@feri.de>
 * Original 1996-1998 Robert Leslie <rob@mars.org>
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

/* Open the device, read and verify the volume header
   (and its backup) */
extern int volume_open(volume* vol, const void *devicehandle, int partition, int rw);

/* Write back all data eventually cached and close the device. */
extern int volume_close(volume* vol);

/* read multiple blocks into given memory.
 *
 * returns given pointer or NULL on failure.
 */
extern void* volume_readfromfork(volume* vol, void* buf, 
	hfsp_fork_raw* f, UInt32 block, 
	UInt32 count, UInt8 forktype, UInt32 fileId);

/* write multiple blocks of a fork buf to medium.
 * The caller is responsible for allocating a suffient
 * large fork and eventually needed extends records for now.
 *
 * block    realtive index in fork to start with
 * count    number of blocks to write
 * forktype usually HFSP_EXTENT_DATA or HFSP_EXTENT_RSRC  
 * fileId   id (needed) in case extents must be written
 *
 * returns value != 0 on error.
 */
int volume_writetofork(volume* vol, void* buf, 
	hfsp_fork_raw* f, UInt32 block, 
	UInt32 count, UInt8 forktype, UInt32 fileId);

/* Fill a given buffer with the given block in volume.
 */
int volume_readinbuf(volume * vol,void* buf, long block);

/* Check in Allocation file if given block is allocated. */
extern int volume_allocated(volume* v, UInt32 block);

/* Read a raw hfsp_extent_rec from memory. */
extern char* volume_readextent(char *p, hfsp_extent_rec er);

/* Read fork information from raw memory */ 
extern char* volume_readfork(char *p, hfsp_fork_raw* f);

/* Write fork information to raw memory */ 
extern char* volume_writefork(char *p, hfsp_fork_raw* f);

/* Initialize for to all zero, you may allocate later */ 
void volume_initfork(volume* vol, hfsp_fork_raw* f, UInt16 fork_type);

/* internal function used to create the extents btree,
   is called by following inline function when needed */
extern void volume_create_extents_tree(volume* vol);

/* accessor for entends btree, is created on demand */
static inline btree* volume_get_extents_tree(volume* vol) 
{
    if (!vol->extents)
	volume_create_extents_tree(vol);
    return vol->extents;
}

/* return new Id for files/folder and check for overflow.
 *
 * retun 0 on error .
 */
extern UInt32 volume_get_nextid(volume* vol);

