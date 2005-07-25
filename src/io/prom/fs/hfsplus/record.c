/*
 * libhfsp - library for reading and writing Macintosh HFS+ volumes.
 *
 * a record contains a key and a folder or file and is part
 * of a btree. This file conatins various methods to read and
 * write the record related HFS+ structures from/to memory.
 *
 * Copyright (C) 2000-2001 Klaus Halfmann <klaus.halfmann@t-online.de>
 * Original 1996-1998 Robert Leslie <rob@mars.org>
 * Additional work by  Brad Boyer (flar@pants.nu)  
 * Additional work in 2004 by Stefan Weyergraf (stefan@weyergraf.de) for use in PearPC
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
# endif                                                                         

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "libhfsp.h"
#include "hfstime.h"
#include "record.h"
#include "volume.h"
#include "btree.h"
#include "unicode.h"
#include "swab.h"

/* read a hfsp_cat_key from memory, check for correct length
 *
 * @param p   buffer in memory to read from
 * @param buf buffer containing the correctly swapped structure
 * 
 * @return pointer to next byte after structure, NULL on failure
 */
char* record_readkey(char* p, void* buf)
{
    hfsp_cat_key*   key = (hfsp_cat_key*) buf;
    const char*	    check;
    UInt16	    key_length, len,i;
    UInt16*	    cp;

    key->key_length = key_length    = bswabU16_inc(&p);
    check = p;
    key->parent_cnid		    = bswabU32_inc(&p);
    key->name.strlen = len	    = bswabU16_inc(&p);
    cp = key->name.name;
    for (i=0; i < len; i++, cp++)
	*cp			    = bswabU16_inc(&p);
	/* check if keylenght was correct */
    if (key_length != p - check)
	 HFSP_ERROR(EINVAL, "Invalid key length in record_readkey");
    return p;	
  fail:
    return NULL;
}

/* write a hfsp_cat_key back to memory, check for correct length.
 *
 * @param p   buffer in memory to write to
 * @param buf memory containing the (swapped) key.
 *
 * @return pointer to byte after the structure or NULL on failure. 
 *
 */
char* record_writekey(char* p, void* buf)
{
    hfsp_cat_key*   key = (hfsp_cat_key*) buf;
    UInt16	    key_length, len,i;
    UInt16*	    cp;

    key_length = key->key_length;
    len	       = key->name.strlen;
    cp	       = key->name.name;
    if (key_length != (6 + len * 2))
	 HFSP_ERROR(EINVAL, "Invalid key length in record_writekey");

    bstoreU16_inc(&p, key_length);
    bstoreU32_inc(&p, key->parent_cnid);
    bstoreU16_inc(&p, len);
    for (i=0; i < len; i++, cp++)
	bstoreU16_inc(&p, *cp);
    return p;	
  fail:
    return NULL;
}

/* read a hfsp_extent_key from memory */
char* record_extent_readkey(char* p, void* buf)
{
    hfsp_extent_key* key = (hfsp_extent_key*) buf;
    UInt16  key_length;

    key->key_length = key_length    = bswabU16_inc(&p);
    key->fork_type		    = bswabU8_inc(&p);
    key->filler			    = bswabU8_inc(&p);
    if (key_length != 10)
	HFSP_ERROR(-1, "Invalid key length in record_extent_readkey");
    key->file_id		    = bswabU32_inc(&p);
    key->start_block		    = bswabU32_inc(&p);
    return p;	
  fail:
    return NULL;
}

/* write a hfsp_extent_key to memory */
char* record_extent_writekey(char* p, void* buf)
{
    hfsp_extent_key* key = (hfsp_extent_key*) buf;
    UInt16  key_length = key->key_length;
    if (key_length != 10)
	HFSP_ERROR(-1, "Invalid key length in record_extent_writekey");

    bstoreU16_inc (&p, key_length);
    bstoreU8_inc  (&p, key->fork_type);
    bstoreU8_inc  (&p, key->filler);
    bstoreU32_inc (&p, key->file_id);
    bstoreU32_inc (&p, key->start_block);
    return p;
  fail:
    return NULL;
}

/* read posix permission from memory */
static inline char* record_readperm(char *p, hfsp_perm* perm)
{
    perm->owner= bswabU32_inc(&p);
    perm->group= bswabU32_inc(&p);
    perm->mode = bswabU32_inc(&p);
    perm->dev  = bswabU32_inc(&p);
    return p;
}

/* write posix permission to memory */
static inline char* record_writeperm(char *p, hfsp_perm* perm)
{
    bstoreU32_inc (&p, perm->owner);
    bstoreU32_inc (&p, perm->group);
    bstoreU32_inc (&p, perm->mode );
    bstoreU32_inc (&p, perm->dev  );
    return p;
}

/* intialize posix permission from memory.
 *
 * TODO use current umask, user, group, etc.
 */
static inline void record_initperm(hfsp_perm* perm)
{
    perm->owner= 0;
    perm->group= 0;
    perm->mode = 0;
    perm->dev  = 0;
}


/* read directory info */
static inline char* record_readDInfo(char *p, DInfo* info)
{
    info->frRect.top	= bswabU16_inc(&p);
    info->frRect.left	= bswabU16_inc(&p);
    info->frRect.bottom	= bswabU16_inc(&p);
    info->frRect.right	= bswabU16_inc(&p);
    info->frFlags	= bswabU16_inc(&p);
    info->frLocation.v	= bswabU16_inc(&p);
    info->frLocation.h	= bswabU16_inc(&p);
    info->frView	= bswabU16_inc(&p);
    return p;
}

/* write directory info */
static inline char* record_writeDInfo(char *p, DInfo* info)
{
    bstoreU16_inc (&p, info->frRect.top	);
    bstoreU16_inc (&p, info->frRect.left	);
    bstoreU16_inc (&p, info->frRect.bottom);
    bstoreU16_inc (&p, info->frRect.right);
    bstoreU16_inc (&p, info->frFlags	);
    bstoreU16_inc (&p, info->frLocation.v);
    bstoreU16_inc (&p, info->frLocation.h);
    bstoreU16_inc (&p, info->frView	);
    return p;
}

/* initialize directory info */
static inline void record_initDInfo(DInfo* info)
{
    // Hope the finder will not choke on these values
    memset(info, 0, sizeof(DInfo));
    /*
    info->frRect.top	= 0;
    info->frRect.left	= 0;
    info->frRect.bottom = 0;
    info->frRect.right	= 0;
    info->frFlags	= 0;
    info->frLocation.v  = 0;
    info->frLocation.h  = 0;
    info->frView        = 0;
    */
}

/* read extra Directory info */
static inline char* record_readDXInfo(char *p, DXInfo* xinfo)
{
    xinfo->frScroll.v  = bswabU16_inc(&p);
    xinfo->frScroll.h  = bswabU16_inc(&p);
    xinfo->frOpenChain = bswabU32_inc(&p);
    xinfo->frUnused    = bswabU16_inc(&p);
    xinfo->frComment   = bswabU16_inc(&p);
    xinfo->frPutAway   = bswabU32_inc(&p);
    return p;
}

/* write extra Directory info */
static inline char* record_writeDXInfo(char *p, DXInfo* xinfo)
{
    bstoreU16_inc (&p, xinfo->frScroll.v );
    bstoreU16_inc (&p, xinfo->frScroll.h );
    bstoreU32_inc (&p, xinfo->frOpenChain);
    bstoreU16_inc (&p, xinfo->frUnused   );
    bstoreU16_inc (&p, xinfo->frComment  );
    bstoreU32_inc (&p, xinfo->frPutAway  );
    return p;
}

/* initialize extra Directory info */
static inline void record_initDXInfo(DXInfo* xinfo)
{
    // Hope the finder will not choke on these values
    memset(xinfo, 0, sizeof(DXInfo));
    /*
    xinfo->frScroll.v = 0;
    xinfo->frScroll.h = 0;
    xinfo->frOpenChain= 0;
    xinfo->frUnused   = 0;
    xinfo->frComment  = 0;
    xinfo->frPutAway  = 0;
    */
}


/* read a hfsp_cat_folder from memory */
static char* record_readfolder(char *p, hfsp_cat_folder* folder)
{
    folder->flags		= bswabU16_inc(&p);
    folder->valence		= bswabU32_inc(&p);
    folder->id			= bswabU32_inc(&p);
    folder->create_date		= bswabU32_inc(&p);
    folder->content_mod_date    = bswabU32_inc(&p);
    folder->attribute_mod_date	= bswabU32_inc(&p);
    folder->access_date		= bswabU32_inc(&p);
    folder->backup_date		= bswabU32_inc(&p);
    p = record_readperm	    (p, &folder->permissions);
    p = record_readDInfo    (p, &folder->user_info);
    p = record_readDXInfo   (p, &folder->finder_info);
    folder->text_encoding	= bswabU32_inc(&p);
    folder->reserved		= bswabU32_inc(&p);
    return p;
}

/* write a hfsp_cat_folder to memory */
static char* record_writefolder(char *p, hfsp_cat_folder* folder)
{
    bstoreU16_inc (&p, folder->flags		);
    bstoreU32_inc (&p, folder->valence		);
    bstoreU32_inc (&p, folder->id		);
    bstoreU32_inc (&p, folder->create_date	);
    bstoreU32_inc (&p, folder->content_mod_date  );
    bstoreU32_inc (&p, folder->attribute_mod_date);
    bstoreU32_inc (&p, folder->access_date	);
    bstoreU32_inc (&p, folder->backup_date	);
    p = record_writeperm     (p, &folder->permissions);
    p = record_writeDInfo    (p, &folder->user_info);
    p = record_writeDXInfo   (p, &folder->finder_info);
    bstoreU32_inc (&p, folder->text_encoding	);
    bstoreU32_inc (&p, folder->reserved		);
    return p;
}

/* initialize a hfsp_cat_folder with given values.
 *
 * @vol is needed to create a new record Id.
 * @return 0 on sucess anything else on error.
 */
static int record_initfolder(volume* vol, hfsp_cat_folder* folder)
{
    UInt32  macNow  = HFSPTIMEDIFF + time(NULL);

    folder->flags		= 0;
    folder->valence		= 0;	// no subfiles/folders yet
    if (! (folder->id = volume_get_nextid(vol))) // oops possible wrap around overflow
	return -1;
    folder->create_date		= macNow;
    folder->content_mod_date    = macNow;
    folder->attribute_mod_date	= macNow;
    folder->access_date		= macNow;
    folder->backup_date		= 0;
    record_initperm	(&folder->permissions);
    record_initDInfo    (&folder->user_info);
    record_initDXInfo   (&folder->finder_info);
    folder->text_encoding	= 0;	// Not supported, sorry
    folder->reserved		= 0;  
    return 0;
}

/* read file info */
static inline char* record_readFInfo(char *p, FInfo* info)
{
    info->fdType	= bswabU32_inc(&p);
    info->fdCreator	= bswabU32_inc(&p);
    info->fdFlags	= bswabU16_inc(&p);
    info->fdLocation.v	= bswabU16_inc(&p);
    info->fdLocation.h	= bswabU16_inc(&p);
    info->fdFldr	= bswabU16_inc(&p);
    return p;
}

/* write file info */
static inline char* record_writeFInfo(char *p, FInfo* info)
{
    bstoreU32_inc (&p, info->fdType	);
    bstoreU32_inc (&p, info->fdCreator	);
    bstoreU16_inc (&p, info->fdFlags	);
    bstoreU16_inc (&p, info->fdLocation.v);
    bstoreU16_inc (&p, info->fdLocation.h);
    bstoreU16_inc (&p, info->fdFldr	);
    return p;
}

/* initialize file info */
static inline void record_initFInfo(FInfo* info)
{
	    // should use something better somehow
    info->fdType	= sig('T','E','X','T');
    info->fdCreator	= HPLS_SIGNATURE;
    info->fdFlags	= 0;	// dunno any finder flags
    info->fdLocation.v	= 0;
    info->fdLocation.h	= 0;
    info->fdFldr	= 0;
}

/* read extra File info */
static inline char* record_readFXInfo(char *p, FXInfo* xinfo)
{
    xinfo->fdIconID	= bswabU16_inc(&p);
    xinfo->fdUnused[0]	= bswabU16_inc(&p);
    xinfo->fdUnused[1]	= bswabU16_inc(&p);
    xinfo->fdUnused[2]	= bswabU16_inc(&p);
    xinfo->fdUnused[3]	= bswabU16_inc(&p);
    xinfo->fdComment	= bswabU16_inc(&p);
    xinfo->fdPutAway	= bswabU32_inc(&p);
    return p;
}

/* write extra File info */
static inline char* record_writeFXInfo(char *p, FXInfo* xinfo)
{
    bstoreU16_inc (&p, xinfo->fdIconID);
    bstoreU16_inc (&p, xinfo->fdUnused[0]);
    bstoreU16_inc (&p, xinfo->fdUnused[1]);
    bstoreU16_inc (&p, xinfo->fdUnused[2]);
    bstoreU16_inc (&p, xinfo->fdUnused[3]);
    bstoreU16_inc (&p, xinfo->fdComment);
    bstoreU16_inc (&p, xinfo->fdPutAway);
    return p;
}

/* initialize extra File info */
static inline void record_initFXInfo(FXInfo* xinfo)
{
    // Hope the finder will not choke on these values
    memset(xinfo, 0, sizeof(FXInfo));
    /*
    xinfo->fdIconID	= 0;
    xinfo->fdUnused[0]	= 0;
    xinfo->fdUnused[1]	= 0;
    xinfo->fdUnused[2]	= 0;
    xinfo->fdUnused[3]	= 0;
    xinfo->fdComment	= 0;
    xinfo->fdPutAway	= 0;
    */
}

/* read a hfsp_cat_file from memory */
static char* record_readfile(char *p, hfsp_cat_file* file)
{
    file->flags			= bswabU16_inc(&p);
    file->reserved1		= bswabU32_inc(&p);
    file->id			= bswabU32_inc(&p);
    file->create_date		= bswabU32_inc(&p);
    file->content_mod_date	= bswabU32_inc(&p);
    file->attribute_mod_date	= bswabU32_inc(&p);
    file->access_date		= bswabU32_inc(&p);
    file->backup_date		= bswabU32_inc(&p);
    p = record_readperm	    (p, &file->permissions);
    p = record_readFInfo    (p, &file->user_info);
    p = record_readFXInfo   (p, &file->finder_info);
    file->text_encoding		= bswabU32_inc(&p);
    file->reserved2		= bswabU32_inc(&p);
    p =	    volume_readfork (p, &file->data_fork);
    return  volume_readfork (p, &file->res_fork);
}

/* write a hfsp_cat_file to memory */
static char* record_writefile(char *p, hfsp_cat_file* file)
{
    bstoreU16_inc(&p, file->flags);
    bstoreU16_inc(&p, file->reserved1);
    bstoreU16_inc(&p, file->id);
    bstoreU16_inc(&p, file->create_date);
    bstoreU16_inc(&p, file->content_mod_date);
    bstoreU16_inc(&p, file->attribute_mod_date);
    bstoreU16_inc(&p, file->access_date);
    bstoreU16_inc(&p, file->backup_date);
    p = record_writeperm    (p, &file->permissions);
    p = record_writeFInfo   (p, &file->user_info);
    p = record_writeFXInfo  (p, &file->finder_info);
    bstoreU16_inc(&p, file->text_encoding);
    bstoreU16_inc(&p, file->reserved2	);
    p =	    volume_writefork (p, &file->data_fork);
    return  volume_writefork (p, &file->res_fork);
}

/* initialize a hfsp_cat_file with given values.
 *
 * vol needed to create new Id
 */
static int record_initfile(volume* vol, hfsp_cat_file* file)
{
    UInt32  macNow  = HFSPTIMEDIFF + time(NULL);

    file->flags		    	= 0;
    file->reserved1		= 0;	// no subfiles/folders yet
    if (! (file->id = volume_get_nextid(vol))) // oops possible wrap around overflow
	return -1;
    file->create_date		= macNow;
    file->content_mod_date	= macNow;
    file->attribute_mod_date	= macNow;
    file->access_date		= macNow;
    file->backup_date		= 0;
    record_initperm	(&file->permissions);
    record_initFInfo    (&file->user_info);
    record_initFXInfo   (&file->finder_info);
    file->text_encoding		= 0;	// Not supported, sorry
    file->reserved2		= 0;
    volume_initfork(vol,&file->data_fork, HFSP_EXTENT_DATA);
    volume_initfork(vol,&file->res_fork, HFSP_EXTENT_RSRC);
    return 0;
}


/* read a hfsp_cat_thread from memory */
static char* record_readthread(char *p, hfsp_cat_thread* entry)
{
    int	    i;
    UInt16  len;
    UInt16* cp;

    entry->         reserved	= bswabU16_inc(&p);
    entry->	    parentID	= bswabU32_inc(&p);
    entry->nodeName.strlen = len= bswabU16_inc(&p);
    cp = entry->nodeName.name;
    if (len > 255)
	HFSP_ERROR(-1, "Invalid key length in record_thread");
    for (i=0; i < len; i++, cp++)
	*cp			 = bswabU16_inc(&p);
    return p;
  fail:
    return NULL;
}

/* write a hfsp_cat_thread to memory */
static char* record_writethread(char *p, hfsp_cat_thread* entry)
{
    int	    i;
    UInt16  len;
    UInt16* cp;

    bstoreU16_inc(&p,	    entry->reserved);
    bstoreU32_inc(&p,	    entry->parentID);
    /* this is bad style, friends... (SW) */
/*    bstoreU16_inc(&p, len =  entry->nodeName.strlen);*/
    len =  entry->nodeName.strlen;
    bstoreU16_inc(&p, len);
    cp = entry->nodeName.name;
    if (len > 255)
	HFSP_ERROR(-1, "Invalid key length in record_thread");
    for (i=0; i < len; i++, cp++)
	bstoreU16_inc(&p, *cp);
    return p;
  fail:
    return NULL;
}


/* read a hfsp_cat_entry from memory */
char* record_readentry(char *p, void* entry)
{
    UInt16	    type = bswabU16_inc(&p);
    hfsp_cat_entry* e	 = (hfsp_cat_entry*) entry;
    e->type = type;
    switch (type)
    {
	case HFSP_FOLDER:
	    return record_readfolder(p, &e->u.folder);
	case HFSP_FILE:
	    return record_readfile  (p, &e->u.file);
	case HFSP_FOLDER_THREAD:
	case HFSP_FILE_THREAD:
	    return record_readthread(p, &e->u.thread);
	default:
	    HFSP_ERROR(-1, "Unexpected record type in record_readentry");
    } ;
  fail:
    return NULL;
}

/* write a hfsp_cat_entry to memory */
char* record_writeentry(char *p, hfsp_cat_entry* entry)
{
    UInt16 type = entry->type;
    bstoreU16_inc(&p, type);
    switch (type)
    {
	case HFSP_FOLDER:
	    return record_writefolder(p, &entry->u.folder);
	case HFSP_FILE:
	    return record_writefile  (p, &entry->u.file);
	case HFSP_FOLDER_THREAD:
	case HFSP_FILE_THREAD:
	    return record_writethread(p, &entry->u.thread);
	default:
	    HFSP_ERROR(-1, "Unexpected record type in record_writeentry");
    } ;
  fail:
    return NULL;
}

/* read an extent record from memory */
// For dependency reasons this actually is found in volume.h
char* record_extent_readrecord(char *p, void* entry)
{
    return volume_readextent(p, (hfsp_extent*) entry);
}


/* intialize the record with the given index entry in the btree. */
int record_init(record* r, btree* bt, node_buf* buf, UInt16 index)
{
    void *p;
    r-> tree   = bt;
    p = btree_key_by_index(bt,buf,index);
    if (!p)
	return -1;
    p = record_readkey  (p, &r->key);
    if (!p)
	return -1;
    /* void *help = p; // se comment below */
    p    = record_readentry(p,	    &r->record);
    /* This was for testing write cache only 
    void * help;
    help = record_writeentry(help,  &r->record);
    if (p != help)
	HFSP_ERROR(-1, "Error in write entry");
    */
    if (!p)
	return -1;
    r->node_index = buf->index;
    r-> keyind    = index;

    return 0;
    /*
  fail:
    return -1;
    */
}

/* Update the record using its node- and key-index. 
 * 
 * Only use this function with a write lock, directly
 * after reading the record, otherwise use update_bykey().
 */
int record_update(record* r)
{
    btree	*tree= r->tree;
    node_buf	*buf = btree_node_by_index(tree, r->node_index, NODE_DIRTY);
    void	*p   = btree_key_by_index (tree, buf, r->keyind);
    if (!p)
	return -1;
    p = record_writekey  (p, &r->key);
    if (!p)
	return -1;
    p = record_writeentry(p, &r->record);
    if (!p)
	return -1;

    return 0;
}


/* intialize the record with the given index entry in the btree. */
static int record_init_extent(extent_record* r, btree* bt, node_buf* buf, UInt16 index)
{
    char *p;
    r-> tree   = bt;
    p = btree_key_by_index(bt, buf,index);
    if (!p)
	return -1;
    p = record_extent_readkey(p, &r->key);
    if (!p)
	return -1;
    p = volume_readextent(p, r->extent);
    if (!p)
	return -1;
    r->node_index = buf->index;
    r-> keyind    = index;

    return 0;
}

/* intialize the record to the first record of the tree
 * which is (per design) the root node.
 */
int record_init_root(record* r, btree* tree)
{
    // Position to first leaf node ...
    UInt32 leaf_head = tree->head.leaf_head;
    node_buf* buf = btree_node_by_index(tree, leaf_head, NODE_CLEAN);
    if (!buf)
	return -1;
    return record_init(r, tree, buf, 0);
}

/* initialize a (catalog) record with given type and (ascii) name.
 * parent must be a HFSP_FOLDER or FOLDER_THREAD
 * You should normally call record_insert afterwards.
 */
int record_init_string(record* r, UInt16 type, char* name, record* parent)
{
    int		    result = 0;
    hfsp_cat_key*   key   = &r->key;
    hfsp_cat_entry* entry = &r->record;
    UInt16	    ptype = parent->record.type;

    memset(r, 0, sizeof *r);   // **** Debugging only

    r->tree	    = parent->tree;
    r->node_index   = 0;
    r->keyind	    = 0;
    key->key_length = 6 + 2 * unicode_asc2uni(&key->name,name); 
			// 6 for minumum size
    if (ptype == HFSP_FOLDER)
	key->parent_cnid= parent->record.u.folder.id; 
    else if (ptype == HFSP_FOLDER_THREAD)
	key->parent_cnid= parent->key.parent_cnid; 
    else    // no kind of folder ??
    {
	hfsp_error = "parent for record_init_string is not a folder.";
	return EINVAL;
    }

    switch(type)
    {
	case HFSP_FOLDER	:
	    entry->type = type;
	    record_initfolder(parent->tree->vol, &entry->u.folder);
	    break;
	case HFSP_FILE		:
	    entry->type = type;
	    record_initfile(parent->tree->vol, &entry->u.file);
	    break;
	    // Those are unsupported use the types above instead
	    // record_init will care about the threads
	case HFSP_FOLDER_THREAD :
	case HFSP_FILE_THREAD   :
	default:
	    hfsp_error = "Unsupported type for record_init_string()";
	    result = -1;
    }
    
    return result;
}

/* initialize a (catalog) record thread by its original record
 * used internally by record_insert.
 */
static int record_init_thread(record* r, record* template)
{
    int			result	= 0;
    hfsp_cat_key*	key	= &r->key;
    hfsp_cat_entry*	entry	= &r->record;
    UInt16		type	= template->record.type;
    hfsp_cat_thread*	thread;

    r->tree	    = template->tree;
    r->node_index   = 0;
    r->keyind	    = 0;
    key->key_length = 6; // empty name is ok for a thread
    key->parent_cnid= template->record.u.folder.id; 
    
    thread = &entry->u.thread;
    switch(type)
    {
	case HFSP_FOLDER	:
	case HFSP_FILE		:
	    entry->type = type + HFSP_THREAD_OFFSET;
	    thread->reserved = 0;
	    thread->parentID = template->key.parent_cnid;
	    thread->nodeName = template->key.name;
	    break;
	case HFSP_FOLDER_THREAD :
	case HFSP_FILE_THREAD   :
	default:
	    hfsp_error = "Cannot create a thread for a thread";
	    result = -1;
    }
    
    return result;
}
 
/* Compare two cat_keys ... */
int record_key_compare(void* k1, void* k2)
{
    hfsp_cat_key* key1 = (hfsp_cat_key*) k1;
    hfsp_cat_key* key2 = (hfsp_cat_key*) k2;
    int diff = key2->parent_cnid - key1->parent_cnid;
    if (!diff) // same parent
	diff = fast_unicode_compare(&key1->name, &key2->name); 
    return diff;
}

/* Compare two extent_keys ... */
int record_extent_key_compare(void* k1, void* k2)
{
    hfsp_extent_key* key1 = (hfsp_extent_key*) k1;
    hfsp_extent_key* key2 = (hfsp_extent_key*) k2;
    int diff = key2->fork_type - key1->fork_type;
    if (!diff) // same type
    {
	diff = key2->file_id - key1->file_id;
	if (!diff) // same file
	    diff = key2->start_block - key1->start_block;
    }
    return diff;
}

/* Position node in btree so that key might be inside */
static node_buf* record_find_node(btree* tree, void *key)
{
    int			start, end, mid, comp;  // components of a binary search
    char		*p = NULL;
    char		curr_key[tree->head.max_key_len];
		    // The current key under examination
    hfsp_key_read	readkey	    = tree->kread;
    hfsp_key_compare	key_compare = tree->kcomp;
    UInt32		index;
    node_buf*		node = 
	    btree_node_by_index(tree, tree->head.root, NODE_CLEAN);
    HFSP_SYNC_START(HFSP_READLOCK, node);
    if (!node)
	HFSP_ERROR(-1, "record_find_node: Cant position to root node");
    while (node->desc.kind == HFSP_NODE_NDX)
    {
	mid = start = 0;
	end  = node->desc.num_rec;
	comp = -1;
	while (start < end)
	{
	    mid = (start + end) >> 1;
	    p = btree_key_by_index(tree, node, mid);
	    if (!p)
		HFSP_ERROR(-1, "record_find_node: unexpected error");
	    p = readkey  (p, curr_key);
	    if (!p)
		HFSP_ERROR(-1, "record_find_node: unexpected error");
	    comp = key_compare(curr_key, key);
	    if (comp > 0)
		start = mid + 1;
	    else if (comp < 0)
		end = mid;
	    else 
		break;
	}
	if (!p) // Empty tree, fascinating ...
	    HFSP_ERROR(-1, "record_find_node: unexpected empty node");
	if (comp < 0)	// mmh interesting key is before this key ...
	{
	    if (mid == 0)
		return NULL;  // nothing before this key ..
	    p = btree_key_by_index(tree, node, mid-1);
	    if (!p)
		HFSP_ERROR(-1, "record_find_node: unexpected error");
	    p = readkey  (p, curr_key);
	    if (!p)
		HFSP_ERROR(-1, "record_find_node: unexpected error");
	}
	    
	index = bswabU32_inc(&p);
	node = btree_node_by_index(tree, index, NODE_CLEAN);
    }
    HFSP_SYNC_END(HFSP_READLOCK, node);
    return node;	// go on and use the found node
  fail:
    HFSP_SYNC_END(HFSP_READLOCK, node);
    return NULL;
}

/* search for the given key in the btree.
 * 
 * returns pointer to memory just after key or NULL.
 *
 * *keyind	recives the index where the key was found
 *		(or could be inserted.)
 * *node_index	is the index of the node where key was found/might
 *		be inserted before
 */
char* record_find_key(btree* tree, void* key, int* keyind, UInt16* node_index)
{
    node_buf* buf = record_find_node(tree, key);
    if (buf)
    {
	int		    comp  = -1;
	int		    start = 0; // components of a binary search
	int		    end   = buf->desc.num_rec;
	int		    mid   = -1;
	char		    *p    = NULL;
	char		    curr_key[tree->head.max_key_len];
	hfsp_key_read	    readkey	= tree->kread;
	hfsp_key_compare    key_compare = tree->kcomp;
	HFSP_SYNC_START(HFSP_READLOCK, node);
	while (start < end)
	{
	    mid = (start + end) >> 1;
	    p = btree_key_by_index(tree, buf, mid);
	    if (!p)
		HFSP_ERROR(-1, "record_find_key: unexpected error");
	    p = readkey  (p, curr_key);
	    if (!p)
		goto fail;
	    comp = key_compare(curr_key, key);
	    if (comp > 0)
		start = mid + 1;
	    else if (comp < 0)
		end = mid;
	    else 
		break;
	}
	if (!p) // Empty tree, fascinating ...
	    HFSP_ERROR(ENOENT, "record_find_key: unexpected empty node");
	*node_index = buf->index;
	if (!comp)	// found something ...
	{
	    *keyind = mid;  // Thats where we found it
	    HFSP_SYNC_END(HFSP_READLOCK, node);
	    return p;
	}
	*keyind = end;	    // Here we can insert a new key
    }
    HFSP_ERROR(ENOENT, NULL);
  fail:
    HFSP_SYNC_END(HFSP_READLOCK, node);
    return NULL;
}

/* intialize the record by searching for the given key in the btree.
 * 
 * r is umodified on error.
 */
int record_init_key(record* r, btree* tree, hfsp_cat_key* key)
{
    int	    keyind;
    UInt16  node_index;
    char    *p = record_find_key(tree, key, &keyind, &node_index);
    
    if (p)
    {
	r -> tree      = tree;
	r -> node_index= node_index;
	r -> keyind    = keyind;
	r -> key       = *key; // Better use a record_key_copy ...
	p = record_readentry(p, &r->record);
	if (!p)
	    HFSP_ERROR(-1, "record_init_key: unexpected error");
	return 0;
    }
  fail:
    return -1;
}

/* intialize the extent_record to the extent identified by the
 * (first) blockindex.
 *
 * forktype: either HFSP_EXTEND_DATA or HFSP_EXTEND_RSRC
 */
int record_init_file(extent_record* r, btree* tree, 
		    UInt8 forktype, UInt32 fileId, UInt32 blockindex)
{
    int		    keyind;
    UInt16	    node_index;
    hfsp_extent_key key = { 10, forktype, 0, fileId, blockindex };
    char	    *p = record_find_key(tree, &key, &keyind, &node_index);
    
    if (p)
    {
	r -> tree      = tree;
	r -> node_index= node_index;
	r -> keyind    = keyind;
	r -> key       = key; // Better use a record_key_copy ...
	p =  volume_readextent(p, r->extent);
	if (!p)
	    HFSP_ERROR(-1, "record_init_file: unexpected error");
	return 0;
    }
  fail:
    return -1;
}

/* intialize the record to the folder identified by cnid
 */
int record_init_cnid(record* r, btree* tree, UInt32 cnid)
{
    hfsp_cat_key    thread_key;	    // the thread is the first record
    
    thread_key.key_length = 6;	    // null name (like '.' in unix )
    thread_key.parent_cnid = cnid;
    thread_key.name.strlen = 0;

    return record_init_key(r, tree, &thread_key);
}

/* intialize the record to the first record of the parent.
 */
int record_init_parent(record* r, record* parent)
{
    if (parent->record.type == HFSP_FOLDER)
	return record_init_cnid(r, parent->tree, parent->record.u.folder.id);
    else if(parent->record.type == HFSP_FOLDER_THREAD)
    {
	if (r != parent)
	    *r = *parent; // The folder thread is in fact the first entry, like '.'
	return 0;
    }
    HFSP_ERROR(EINVAL, 
	"record_init_parent: parent is neither folder nor folder thread.");

  fail:
    return EINVAL;
}

/* intialize the record to the parent directory of the given record.
 */
int record_find_parent(record* r, record* from)
{ 
    UInt16	    type    = from->record.type;
    btree*	    bt	    = from->tree;
    hfsp_cat_key    parentKey;
    if (type == HFSP_FOLDER || type == HFSP_FILE)
	if (record_init_cnid(r, bt, from->key.parent_cnid))
	    goto fail;
    // r now is the folder thread, position to the real folder
    parentKey.key_length    = 6 + r->record.u.thread.nodeName.strlen * 2;
    parentKey.parent_cnid   = r->record.u.thread.parentID;
    parentKey.name	    = r->record.u.thread.nodeName;
    if (record_init_key(r, bt, &parentKey))
	goto fail;

    return 0;
  fail:
    return -1;

}

/* find correct node record for given node and *pindex.
 *
 * index of record in this (or next) node
 */
static node_buf* prepare_next(btree* tree, UInt16 node_index, UInt16* pindex)
{
    node_buf*	     buf    = btree_node_by_index(tree, node_index, NODE_CLEAN);
    btree_node_desc* desc   = &buf->desc;
    UInt32	     numrec = desc->num_rec;
    if (*pindex >= numrec) // move on to next node
    {
	UInt16 next = desc->next;
	*pindex = 0;
	if (!next   /* is there a next node ? */
	||  !( buf = btree_node_by_index(tree, next, NODE_CLEAN)))
	    return NULL;
    }
    return buf;
}
/* move record foreward to next entry. 
 *
 * In case of an error the value of *r is undefined ! 
 */
int record_next(record* r)
{
    btree*	tree	= r->tree;
    UInt16	index	= r->keyind +1;
    UInt32	parent;
    node_buf*	buf	= prepare_next(tree, r->node_index, &index);
    
    if (!buf)
	return ENOENT;	// No (more) such file or directory
    
    parent = r->key.parent_cnid;

    if (record_init(r, tree, buf, index))
	return -1;

    if (r->key.parent_cnid != parent || // end of current directory
	index != r->keyind)		// internal error ?
	return ENOENT;	// No (more) such file or directory

    return 0;
}

/* move record foreward to next extent record. 
 *
 * In case of an error the value of *r is undefined ! 
 */
int record_next_extent(extent_record* r)
{
    btree*	tree   = r->tree;
    UInt16	index  = r->keyind +1;
    UInt32	file_id;
    UInt8	fork_type;
    node_buf*	buf	= prepare_next(tree, r->node_index, &index);

    if (!buf)
	return ENOENT;	// No (more) such file or directory
    
    file_id	= r->key.file_id;
    fork_type	= r->key.fork_type;

    if (record_init_extent(r, tree, buf, index))
	return -1;

    if (r->key.file_id	 != file_id ||	    // end of current file
	r->key.fork_type != fork_type ||    // end of current fork
	index != r->keyind)		    // internal error ?
	return ENOENT;	// No (more) such file or directory

    return 0;
}

/* intialize the record by searching for the given string in the given folder.
 * 
 * parent and r may be the same.
 */
int record_init_string_parent(record* r, record* parent, char* name)
{
    hfsp_cat_key key;
    
    if (parent->record.type == HFSP_FOLDER)
	key.parent_cnid = parent->record.u.folder.id;
    else if(parent->record.type == HFSP_FOLDER_THREAD)
	key.parent_cnid = parent->key.parent_cnid;
    else
	HFSP_ERROR(-1, "record_init_string_parent: parent is not a folder.");

    key.key_length = 6 + unicode_asc2uni(&key.name,name); // 6 for minumum size
    return record_init_key(r, parent->tree, &key);

  fail:
    return -1;
}

/* move record up in folder hierarchy (if possible) */
int record_up(record* r)
{
    if (r->record.type == HFSP_FOLDER)
    {
	// locate folder thread
	if (record_init_cnid(r, r->tree, r->record.u.folder.id))
	    return -1;
    }
    else if(r->record.type == HFSP_FOLDER_THREAD)
    {
	// do nothing were are already where we want to be
    }
    else
	HFSP_ERROR(-1, "record_up: record is neither folder nor folder thread.");

    if(r->record.type != HFSP_FOLDER_THREAD)
	HFSP_ERROR(-1, "record_up: unable to locate parent");
    return record_init_cnid(r, r->tree, r->record.u.thread.parentID);

  fail:
    return -1;
}

/* Delete the record from the tree but dont care about its type.
 * helper function for record_delete */

static int record_delete_direct(record *r)
{    
    btree	    *bt = r->tree;
    record	    parent; // Parent folder of the deleted record
    UInt16	    type = r->record.type;
    hfsp_vh	    *volheader;

    HFSP_SYNC_START(HFSP_WRITELOCK, bt);

    // Must reload record it may bave become invalid..
    if (record_init_key(r, bt, &r->key))
	goto fail;
   
    btree_remove_record(bt, r->node_index, r->keyind);

    // Care about valence only when not using threads ...
    if (type <= HFSP_THREAD_OFFSET)
    {
	if (record_find_parent(&parent, r))
	    goto fail;
	if (parent.record.u.folder.valence == 0) {
//	    fprintf(stderr, "Deleting item from folder with 0 items !?\n");
	} else {
	    parent.record.u.folder.valence --;
	    parent.record.u.folder.content_mod_date = HFSPTIMEDIFF + time(NULL);
	    // write back that folder ...
	    record_update(&parent);
	}
    }

    volheader = &bt->vol->vol;
    HFSP_SYNC_END(HFSP_WRITELOCK, bt);

    // Update header depending on type
    if (type == HFSP_FOLDER_THREAD)
	volheader->folder_count--;
    else if (type == HFSP_FILE)
	volheader->file_count--;

    return 0;
  fail:
    HFSP_SYNC_END(HFSP_WRITELOCK, bt);
    return -1;
}

/* recursivly remove contents of folder from btree 
 *
 * r must be a folder thread.
 */
static int record_delete_recurse(record* r, int flags)
{
    record iter	    = *r;	// iterator for entries
    int	   result   = 0;
    if (r->record.type != HFSP_FOLDER_THREAD)
	return -1;  // should not happen !
    
    while (!result && !record_next(&iter))
    {
	if (flags & RECORD_DELETE_RECURSE)
	    result = record_delete(&iter, flags);
	    // Mmh, this will fail as soon as the b*tree is reorganized :(
	else
	    return ENOTEMPTY; // must not delete non-empty directory
	iter = *r;	// reset iterator 
    }

    return 0;
}

/* remove record from btree, It does not care about any
 * forks associated with a file (yet) */
int record_delete(record* r, int flags)
{
    btree	    *bt = r->tree;
    record	    parent; // Parent folder of the deleted record
    hfsp_cat_key    parentKey;
    UInt16	    type = r->record.type;
    int		    result = 0;

    if (type == HFSP_FOLDER && !(flags & RECORD_DELETE_DIRECT))
    {
	record thread;
	// locate folder thread and delete it
	result = record_init_cnid(&thread, bt, r->record.u.folder.id);
	if (!result)
	    result = record_delete(&thread, flags | RECORD_DELETE_DIRECT);
	// failing to delete the folder now will leave the
	// btree inconsistant, but this should not happen any more
    }
 
    if (type == HFSP_FOLDER_THREAD)
    {
	// will result in error in case folder is not empty
	result = record_delete_recurse(r, flags & ~RECORD_DELETE_DIRECT);
	
	if (!result && !(flags & RECORD_DELETE_DIRECT))
	{
	    record	    folder;
	    hfsp_cat_key    folderKey;
	    // locate folder for thread 
	    folderKey.key_length    = 6 + r->record.u.thread.nodeName.strlen * 2;
	    folderKey.parent_cnid   = r->record.u.thread.parentID;
	    folderKey.name	    = r->record.u.thread.nodeName;
	    result = record_init_key(&parent, bt, &parentKey);
	    if (!result)    // shortcut recursive call
		result = record_delete_direct(&folder); 
	    // failing to delete the folder thread now will leave the
	    // btree inconsistant, but this should not happen any more
	}
    }

    if (!result)	
	result = record_delete_direct(r);
    
    return result;
}

/* insert record into btree, It does not care about any
 * forks associated with a file (yet) 
 * ToDo: Care about parent and header counts
 * 
 */
int record_insert(record* r)
{
    btree	    *bt = r->tree;
    record	    parent; // Parent folder of the new record
    UInt16	    type = r->record.type;
    int		    result = 0;
    char	    buf[sizeof(record)];   // a bit too long, well
    char*	    p = buf;
    int		    len;		    // actual len of buffer used
    int		    keyind;		    // index where key should be inserted
    UInt16	    nodeind;		    // node where record should be inserted
    hfsp_vh	    *volheader;

    // now insert thread for file/folder
    if (type == HFSP_FOLDER || type == HFSP_FILE)
    {
	record thread;
	// create folder thread and insert it
	result = record_init_thread(&thread,  r);
	if (!result)
	    result = record_insert(&thread);
    }

    HFSP_SYNC_START(HFSP_WRITELOCK, bt);

    // Find out where to insert the record
    if (record_find_key(bt, &r->key, &keyind, &nodeind)) {
	hfsp_error = "File/Folder already exists";
	HFSP_ERROR(EEXIST, hfsp_error);
    }

    // Create memory image
    p = record_writekey  (p, &r->key);
    if (!p)
	return -1;  // ????
    p = record_writeentry(p, &r->record);
    if (!p)
	return -1;  // ????
    // Insert the buffer
    len = p - buf;
    if (len > bt->max_rec_size) // Emergency bail out, sorry
    {
/*	fprintf(stderr,"Unexpected Buffer overflow in record_insert %d > %d",
		len, sizeof(bt->max_rec_size));*/
//	exit(-1);
	goto fail;
    } 
    if (btree_insert_record(bt,nodeind,keyind,buf,len))
	HFSP_ERROR(ENOSPC, "Unable to insert record into tree (volume full ?)");

    // Ignore threads for valence and file/folder counts
    if (type == HFSP_FOLDER || type == HFSP_FILE)
    {
	// Update parent valence
	if (record_find_parent(&parent, r))
	    goto fail;

	parent.record.u.folder.valence ++;
	parent.record.u.folder.content_mod_date = HFSPTIMEDIFF + time(NULL);
	// write back that folder ...
	record_update(&parent);

	volheader = &bt->vol->vol;

	// Update header depending on type
	if (type == HFSP_FOLDER)
	    volheader->folder_count++;
	else if (type == HFSP_FILE)
	    volheader->file_count++;
    }
    HFSP_SYNC_END(HFSP_WRITELOCK, bt);
    return result;
  fail:
    HFSP_SYNC_END(HFSP_WRITELOCK, bt);
    return -1;
}
