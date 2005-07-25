/*
 * libhfs - library for reading and writing Macintosh HFS volumes.
 *
 * a record contains a key and a folder or file and is part
 * of a btree.
 *
 * Copyright (C) 2000 Klaus Halfmann <klaus.halfmann@feri.de>
 * Original 1996-1998 Robert Leslie <rob@mars.org>
 * Additional work by  Brad Boyer (flar@pants.nu)  
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

/* Compare two cat_keys ... */
extern int record_key_compare(void* k1, void* k2);

/* Compare two extent_keys ... */
extern int record_extent_key_compare(void* k1, void* k2);

/* read a catalog key into a given buffer */
extern char* record_readkey(char* p, void* buf);

/* read an extent key into a given buffer */
extern char* record_extent_readkey(char* p, void* buf);

/* read a hfsp_cat_entry (catalog record) from memory */
extern char* record_readentry(char *p, void* entry);

/* read an extent record from memory */
// For dependency reasons this actually is found in volume.c
extern char* record_extent_readrecord(char *p, void* entry);

/* intialize the record to the first record of the tree
 * which is (per design) the root node.
 */
extern int record_init_root(record* r, btree* tree);

/* intialize the record to the folder given by cnid.
 */
extern int record_init_cnid(record* r, btree* tree, UInt32 cnid);

/* intialize the record to the first record of the parent.
 */
extern int record_init_parent(record* r, record* parent);

extern int record_init_key(record* r, btree* tree, hfsp_cat_key* key);


/* intialize the record to the parent directory of the given record.
 */
extern int record_find_parent(record* r, record* from);

/* intialize the record by searching for the given string in the given folder.
 * 
 * parent and r may be the same.
 */
extern int record_init_string_parent(record* r, record* parent, char* key);

/* initialize a new (catalog) record with given type and (ascii) name.
 * parent must be a HFSP_FOLDER or FOLDER_THREAD
 * You should normally call record_insert afterwards.
 */
extern int record_init_string(record* r, UInt16 type, char* name, record* parent);

/* move record up in folder hierarchy (if possible) */
extern int record_up(record* r);

/* move record foreward to next entry.
 *
 * In case of an error the value of *r is undefined !
 */
extern int record_next(record* r);

/* intialize the extent_record to the extent identified by 
 * a given file */
extern int record_init_file(extent_record* r, btree* tree, 
		    UInt8 forktype, UInt32 fileId, UInt32 blockindex);

/* move foreward to next entent record. */
extern int record_next_extent(extent_record *r);

/* intialize the record with the given index entry in the btree. 
 *
 * needed by fscheck, do not use in normal code.
 */
extern int record_init(record* r, btree* bt, node_buf* buf, UInt16 index);

/* remove record from btree, It does not (yet) care about any
 * forks associated with a file, see below for flags */

extern int record_delete(record* r, int flags);

/* insert record into btree, It does not care about any
 * forks associated with a file (yet) */
extern int record_insert(record* r);

/* Do not care about files/folders/threads, needed internally */
#define RECORD_DELETE_DIRECT	0x0001

/* Similar to the rm -f flag, may not be supported */
#define RECORD_DELETE_FORCE	0x0002

/* Descend recursivly in directories and delete them (like rm -R) 
 * Non-empty directories can not be deleted otherwise */
#define RECORD_DELETE_RECURSE	0x0004

