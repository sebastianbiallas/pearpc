/*
 * libhfs - library for reading and writing Macintosh HFS volumes
 *
 * This code checks the btreee structures of a HFS+ volume for correctnes.
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
 * $Id: btreecheck.c,v 1.1 2004/05/05 22:45:39 seppel Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif
 
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <limits.h>
# include <errno.h>
# include <time.h>

# include "libhfsp.h"
# include "volume.h"
# include "fscheck.h"
# include "btree.h"
# include "record.h"
# include "hfstime.h"
# include "unicode.h"
# include "swab.h"

/** helper function to print those Apple 4 byte Signatures */
static inline void print_sig(UInt32 sig)
{
    printf("%c%c%c%c" , 
	((char*)&sig)[0], ((char*)&sig)[1], 
	((char*)&sig)[2], ((char*)&sig)[3]);
}

/* print the key of a record */
static void record_print_key(hfsp_cat_key* key)
{
    char buf[255]; // mh this _might_ overflow 
    unicode_uni2asc(buf, &key->name, 255);   
    printf("parent cnid         : %d\n",   key->parent_cnid);
    printf("name                : %s\n", buf);
}

/* Check the btree header as far as possible.
 */
static int fscheck_checkbtree(btree* bt)
{
    int		result	    = 0;
    btree_head* head	    = &bt->head;
    UInt16	node_size   = head->node_size;
    UInt32	node_count  = head->node_count;
    UInt32	blocksize   = bt->vol->vol.blocksize;
    
    // head->depth  to be checked later
    if (node_size % HFSP_BLOCKSZ)
    {
	printf("node_size %d not a multiple of HFSP_BLOCKSZ %d\n", 
		node_size, HFSP_BLOCKSZ);
	result |= FSCK_ERR; // dont know how to fix that by now
    }
    /* This is quite ok for a newly created extends-tree 
    if (!head->root)
    {
	printf("root node must not be 0\n");
	result |= FSCK_ERR; // dont know how to fix that by now
    }
    */
    if (head->root >= node_count)
    {
	printf("root node out of range %X >= %X\n",
		head->root, node_count);
	result |= FSCK_ERR; // This is really evil
    }
    if (head->leaf_head >= node_count)
    {
	printf("leaf_head out of range %X >= %X\n", 
		head->leaf_head, node_count);
	result |= FSCK_ERR; // dont know how to fix that by now
    }
    if (head->leaf_tail >= node_count)
    {
	printf("leaf_head out of range %X >= %X\n", 
		head->leaf_tail, node_count);
	result |= FSCK_ERR; // dont know how to fix that by now
    }
    if (head->max_key_len < HFSP_CAT_KEY_MIN_LEN)
    {
	printf("max key len small %d < %d\n",
		head->max_key_len, HFSP_CAT_KEY_MIN_LEN);
	result |= FSCK_ERR; // dont know how to fix that by now
    }
    if (head->max_key_len > HFSP_CAT_KEY_MAX_LEN)
    {
	printf("max key to large %d > %d\n", 
		head->max_key_len, HFSP_CAT_KEY_MAX_LEN);
	result |= FSCK_ERR; // dont know how to fix that by now
    }
    if (head->free_nodes >= node_count)
    {
	printf("free_nodes out of range %X >= %X\n", 
		head->free_nodes, node_count);
	result |= FSCK_ERR; // dont know how to fix that by now
    }
    // head->reserved1	nothing to check here
    if (head->clump_size % blocksize)
    {
	printf("clump_size %d not a multiple of blocksize %d\n", 
		head->free_nodes, blocksize);
	result |= FSCK_ERR; // dont know how to fix that by now
    }
    if (head->btree_type != 0)
    {
	printf("Unexpected btree_type %d\n" , head->btree_type);
	result |= FSCK_ERR; // dont know how to fix that by now
    }
    // head->reserved2  nothing to check here
    if (head->attributes & HFSPLUS_TREE_RESERVED)
    {
	printf("Unexpected bits in btree header node attributes %X\n",
	    head->attributes);
	result |= FSCK_ERR; // dont know how to fix that by now
    }

    return result;
}

/* print btree header node information */
static void btree_printhead(btree_head* head)
{
    UInt32 attr;
    printf("  depth       : %#X\n",  head->depth);
    printf("  root        : %#X\n", head->root);
    printf("  leaf_count  : %#X\n", head->leaf_count);
    printf("  leaf_head   : %#X\n", head->leaf_head);
    printf("  leaf_tail   : %#X\n", head->leaf_tail);
    printf("  node_size   : %#X\n",  head->node_size);
    printf("  max_key_len : %#X\n",  head->max_key_len);
    printf("  node_count  : %#X\n", head->node_count);
    printf("  free_nodes  : %#X\n", head->free_nodes);
    printf("  reserved1   : %#X\n",  head->reserved1);
    printf("  clump_size  : %#X\n", head->clump_size);
    printf("  btree_type  : %#X\n",  head->btree_type);
    attr = head->attributes;
    printf("  reserved2   : %#X\n",  head->reserved2);
    if (attr & HFSPLUS_BAD_CLOSE)
        printf(" HFSPLUS_BAD_CLOSE *** ");
    else
        printf(" !HFSPLUS_BAD_CLOSE");
    if (attr & HFSPLUS_TREE_BIGKEYS)
        printf(" HFSPLUS_TREE_BIGKEYS ");
    else
        printf("  !HFSPLUS_TREE_BIGKEYS");
    if (attr & HFSPLUS_TREE_VAR_NDXKEY_SIZE)
        printf(" HFSPLUS_TREE_VAR_NDXKEY_SIZE");
    else
        printf(" !HFSPLUS_TREE_VAR_NDXKEY_SIZE");
    if (attr & HFSPLUS_TREE_UNUSED)
        printf(" HFSPLUS_TREE_UNUSED ***\n");
    printf("\n");
}

/* Dump a node descriptor to stdout */

static void print_node_desc(UInt32 nodeIndex, btree_node_desc* node)
{
    printf("Node descriptor for Node %d\n", nodeIndex);
    printf("next     : %#X\n", node->next);
    printf("prev     : %#X\n", node->prev);
    printf("height   : %#X\n",  node->height);
    printf("num_rec  : %d\n",   node->num_rec);
    printf("reserved : %#X\n",  node->reserved);
    printf("height   : %#X\n",  node->height);
    switch(node->kind)
    { 
	case HFSP_NODE_NDX  :
	    printf("HFSP_NODE_NDX\n");
	    break;
	case HFSP_NODE_HEAD :
	    printf("HFSP_NODE_HEAD\n");
	    break;
	case HFSP_NODE_MAP  :
	    printf("HFSP_NODE_MAP\n");
	    break;
	case HFSP_NODE_LEAF :
	    printf("HFSP_NODE_LEAF\n");
	    break;
	default:
	    printf("*** Unknown Node type ***\n");
    } 
} 

/** intialize the btree with the first entry in the fork */
static int fscheck_btree_init(btree* bt, volume* vol, hfsp_fork_raw* fork)
{ 
    void		*p;
    char		buf[vol->blksize]; 
    UInt16		node_size;
    btree_node_desc*	node = &bt->head_node;
    int			result;
    int			alloc_size;

    bt->vol	= vol;
    bt->fork	= fork;
    p	= volume_readfromfork(vol, buf, fork, 0, 1,
		 HFSP_EXTENT_DATA, bt->cnid);
    if (!p)
    {
	printf("Unable to read block 1 of b*tree for cnid:%d\n", bt->cnid);
	return FSCK_ERR;
    }
    p = btree_readnode(node, p);
    if (node->prev != 0)
    {
	printf("Backlink of header node is not zero (%X) \n", node->prev);
	return FSCK_ERR; // ToDo: We might ignore it but ???
    }
    if (node->kind != HFSP_NODE_HEAD)
    {
	printf("Unexpected node kind (%d) for node Header\n", node->kind);
	return FSCK_ERR; // ToDo: We might ignore it but ???
    }
    p = btree_readhead(&bt->head, p);

    node_size = bt->head.node_size;
    bt->blkpernode = node_size / vol->blksize;

    if (bt->blkpernode == 0)	// maybe the other way round ?
	bt->nodeperblk = vol->blksize / node_size;
    else
    {
	if (bt->blkpernode * vol->blksize != node_size)
	{
	    printf("node_size (%X) is no multiple of block size (%X)\n", 
		    node_size, bt->blkpernode);
	    return FSCK_ERR; // Thats fatal as of now
	}
    }
    alloc_size = node_size - HEADER_RESERVEDOFFSET; // sizeof(node_desc) + sizeof(header) 
    /* Sometimes the node_size is bigger than the volume-blocksize
     * so here I reread the node when needed */
    { // need new block for allocation
	char nodebuf[node_size];
	if (bt->blkpernode > 1)
	{
	    p = volume_readfromfork(vol, nodebuf, fork, 0, bt->blkpernode,
		 HFSP_EXTENT_DATA, bt->cnid);
	    ((char*) p) += HEADER_RESERVEDOFFSET; // skip header
	}
	
	bt->alloc_bits = malloc(alloc_size);
	if (!bt->alloc_bits)
	    return ENOMEM;
	memcpy(bt->alloc_bits, p, alloc_size);
    }

    result = fscheck_checkbtree(bt);
    if (fsck_data.verbose)
	btree_printhead(&bt->head);

    node_cache_init(&bt->cache, bt, bt->head.depth + EXTRA_CACHESIZE);

    return result;
}

/** Intialize catalog btree */
int fscheck_init_cat(btree* bt, volume* vol, hfsp_fork_raw* fork)
{ 
    int result = fscheck_btree_init(bt,vol,fork);	// super (...)
    bt->cnid  = HFSP_CAT_CNID;
    bt->kcomp = record_key_compare;
    bt->kread = record_readkey;
    bt->rread = record_readentry;
    bt->max_rec_size = sizeof(hfsp_cat_entry);
    return result;
}

/** Intialize catalog btree */
int fscheck_init_extent(btree* bt, volume* vol, hfsp_fork_raw* fork)
{ 
    int result = fscheck_btree_init(bt,vol,fork);	// super (...)
    bt->cnid   = HFSP_EXT_CNID;
    bt->kcomp  = record_extent_key_compare;
    bt->kread  = record_extent_readkey;
    bt->rread = record_extent_readrecord;
    bt->max_rec_size = sizeof(hfsp_extent);
    return result;
}

/* Used to create the extents btree */
int fscheck_create_extents_tree(volume* vol)
{
    btree* result = (btree*) ALLOC(btree*, sizeof(btree));
    int    retval = 0;
    if (!result)
    {
	printf("No memory for extents btree\n");
	return FSCK_ERR;
    }
    if (FSCK_FATAL & (retval =
	fscheck_init_extent(result, vol, &vol->vol.ext_file)))
    {
	vol->extents = NULL;    
	return retval;
    }
    vol->extents = result;
    return retval;
}

/* returns pointer to key given by index in current node.
 * Same as btree_key_by_index, but with more checks etc.
 *
 * Assumes that current node is not NODE_HEAD ...
 * index may be == num_rec in whcih case a pointer
 * to the first free byte is returned ...
 */   
static void* checkbtree_key_by_index(btree* bt, UInt32 node, node_buf* buf, UInt16 index)
{
    UInt16		node_size, off_pos;
    btree_record_offset offset;

    if (index > buf->desc.num_rec)	// oops out of range
    {
	fprintf(stderr,"checkbtree_key_by_index: index out of range %u > %u\n",
		    index, buf->desc.num_rec);
	return NULL;
    }
    node_size	    = bt->head.node_size; 
	// The offsets are found at the end of the node ...
    off_pos	    = node_size - (index +1) * sizeof(btree_record_offset);
 	// position of offset at end of node
   if (off_pos >= node_size)	// oops out of range
    {
	fprintf(stderr,"checkbtree_key_by_index: off_pos out of range "
		"%X >= %X\n", off_pos, node_size);
	return NULL;
    }
    offset = *((btree_record_offset*) (buf->node + off_pos));
    if (offset >= node_size)	// oops out of range
    {
	fprintf (stderr, "checkbtree_key_by_index: offset out of range %X >= %X\n",
		offset, node_size);
	return NULL;
    }
    if (fsck_data.verbose)
    {
	printf("Node %4d, Record %2d is at pos %04X,"
	       "Backptr is at offset %04X\n", node, index, offset, off_pos);
    }
    // now we have the offset and can read the key ...
#if BYTE_ORDER == LITTLE_ENDIAN
    return buf->node + bswap_16(offset);
#else
    return buf->node + offset;
#endif
}

/* Try to fix a node when the backpointers are broken.
 *
 * This is done by crawling forward as long as the keys
 * are valid and checking/adjusting the backpointers.
 * This may result in the loss of records in case a
 * key (or record) is damaged. 
 */
static int fscheck_fix_node(btree* bt, UInt32 nodeIndex)
{
    int		     result  = FSCK_NOERR;
    node_buf*	     node    = btree_node_by_index(bt, nodeIndex, NODE_CLEAN);
    btree_node_desc* desc    = &node->desc;
    UInt16 	     num_rec = desc->num_rec;
    UInt16	     i;
    int		     isindex = desc->kind == HFSP_NODE_NDX;
    void*	     current = node->node + 0x0E; // sizeof (btree_node_desc)
    char	     kbuf[bt->head.max_key_len]; // dummy key to skip over
    char	     buf[bt->max_rec_size]; 
    fprintf(stderr, "Node %u with %u records is damaged trying to fix ***\n",
		nodeIndex, num_rec);
    for (i=0; i < num_rec; i++)
    {
	void *p	= checkbtree_key_by_index(bt, nodeIndex, node, i);
	if (!p)
	    return result | FSCK_ERR;
	if (p != current)
	{
	    fprintf(stderr, 
		"Key %u in Node %u is damaged "
		"rest of keys will be droppend ***\n", i,nodeIndex);
	    break;
	}
	p = bt->kread(p, kbuf); // Read the key
	if (!isindex)
	    p = bt->rread(p, buf);
    }
    if (i < num_rec)
    {
	fprintf(stderr, 
	    "Code to drop damaged record not yet implemented ***.\n");
    }
    // ToDo: check for pointer to free area, too
    return result;
}


/* recursive function to check a node (given by its index).
 *
 * In case of an index node is descends into the subnodes.
 */
static int fscheck_btree_node(btree* bt, UInt32 nodeIndex, hfsp_key** key1, hfsp_key** key2)
{
    int		     result  = FSCK_NOERR;
    node_buf*	     node    = btree_node_by_index(bt, nodeIndex, NODE_CLEAN);
    btree_node_desc* desc    = &node->desc;
    UInt16 	     num_rec = desc->num_rec;
    UInt16	     i;
    int		     isindex = desc->kind == HFSP_NODE_NDX;
    hfsp_key*	     tmp;
    void*	     previous = ((char*)node) + 0x0E; // sizeof btree_node_desc

    if (fsck_data.verbose)
	print_node_desc(nodeIndex, desc);
    for (i=0; i<num_rec; i++)
    {
	void	*p;
	UInt32	index;

	// node may become invalid due to cache flushing
	node	= btree_node_by_index(bt, nodeIndex, NODE_CLEAN);
	p	= checkbtree_key_by_index(bt, nodeIndex, node, i);
	if (!p)
	    return result | FSCK_ERR;
	if (p < previous)
	{   // This may happen when the cache entry was flushed, but per
	    // design of the cache this should not happen, mmh
	    printf("Backpointers in Node %d index %d out of order "
		   "(%p >= %p)\n", nodeIndex, i, p, previous);
	    result |= FSCK_FSCORR;	// Hope we can correct that later
	}
	previous = p;
	p = bt->kread(p, *key1); // Read the key
	if (!p)
	{
	    result |= FSCK_ERR;

	    // Lets try to fix that Error ....
	    fscheck_fix_node(bt, nodeIndex);
	    if (fsck_data.ignoreErr)
		continue; // Hope this will work
	    return result;
	}
	if ((*key2)->key_length)
	{
	    int comp = bt->kcomp(*key1, *key2);
	    if (comp > 0)
	    {
		printf("Invalid key order in node %d record %d\n key1=",
			nodeIndex, i);
		record_print_key((hfsp_cat_key*) *key1);
		printf("Invalid key order key2=\n");
		record_print_key((hfsp_cat_key*) *key2);
		result |= FSCK_FSCORR;	// Hope we can correct that later
	    }
	    if (comp == 0 && i > 0) // equal to key in parent node is ok
	    {
		printf("Duplicate key in node %d record %d key1=\n",
			nodeIndex, i);
		record_print_key((hfsp_cat_key*) *key1);
		printf("Duplicate key key2=\n");
		record_print_key((hfsp_cat_key*) *key2);
		result |= FSCK_FSCORR;	// Hope we can correct that later
	    }
	}
	tmp   = *key1;	// Swap buffers for next compare
	*key1 = *key2;
	*key2 = tmp;
	if (isindex)
	{
	    index = bswabU32_inc(p);
	    result |= fscheck_btree_node(bt, index, key1, key2);
	}
	if (result & FSCK_FATAL)
	    break;
    }
    return result;
}

/** Check a complete btree by traversing it in-oder */
int fscheck_btree(btree *bt)
{
    UInt16	maxkeylen = bt->head.max_key_len;
    int		result = FSCK_NOERR;
    char	keybuf1[maxkeylen];
    char	keybuf2[maxkeylen];
    hfsp_key*	key1 = (hfsp_key*) keybuf1;	// Alternating buffers
    hfsp_key*	key2 = (hfsp_key*) keybuf2;	// for key Compare
    
    key2->key_length = 0; // So first compare can be skipped

    result = fscheck_btree_node(bt, bt->head.root, &key1, &key2);

    return result;
}
    
/* print Quickdraw Point */
static void record_print_Point(Point* p)
{
    printf("[ v=%d, h=%d ]", p->v, p->h);
}

/* print Quickdraw Rect */
static void record_print_Rect(Rect* r)
{
    printf("[ top=%d, left=%d, bottom=%d, right=%d  ]",
	     r->top, r->left, r->bottom, r->right);
}

/* print permissions */
static void record_print_perm(hfsp_perm* perm)
{
    printf("owner               : %d\n",  perm->owner);
    printf("group               : %d\n",  perm->group);
    printf("perm                : 0x%X\n",perm->mode);
    printf("dev                 : %d\n",  perm->dev);
}

/* print Directory info */
static void record_print_DInfo(DInfo* dinfo)
{
    printf(  "frRect              : ");    record_print_Rect(&dinfo->frRect);
    printf("\nfrFlags             : 0X%X\n",    dinfo->frFlags);
    printf(  "frLocation          : ");    record_print_Point(&dinfo->frLocation);
    printf("\nfrView              : 0X%X\n",    dinfo->frView);
}

/* print extended Directory info */
static void record_print_DXInfo(DXInfo* xinfo)
{
    printf(  "frScroll            : ");    record_print_Point(&xinfo->frScroll);
    printf("\nfrOpenChain         : %d\n",  xinfo->frOpenChain);
    printf(  "frUnused            : %d\n",   xinfo->frUnused);
    printf(  "frComment           : %d\n",   xinfo->frComment);
    printf(  "frPutAway           : %d\n",  xinfo->frPutAway);
}

static void record_print_folder(hfsp_cat_folder* folder)
{
    printf("flags               : 0x%X\n",	folder->flags);
    printf("valence             : 0x%X\n",	folder->valence);
    printf("id                  : %d\n",	folder->id);
    printf("create_date         : %s",	get_atime(folder->create_date));
    printf("content_mod_date    : %s",	get_atime(folder->content_mod_date));
    printf("attribute_mod_date  : %s",	get_atime(folder->attribute_mod_date));
    printf("access_date         : %s",	get_atime(folder->access_date));
    printf("backup_date         : %s",	get_atime(folder->backup_date));
    record_print_perm	(&folder->permissions);
    record_print_DInfo	(&folder->user_info);
    record_print_DXInfo	(&folder->finder_info);
    printf("text_encoding       : 0x%X\n",	folder->text_encoding);
    printf("reserved            : 0x%X\n",	folder->reserved);
}

/* print File info */
static void record_print_FInfo(FInfo* finfo)
{
    printf(  "fdType              : %4.4s\n", (char*) &finfo->fdType);
    printf(  "fdCreator           : %4.4s\n", (char*) &finfo->fdCreator);
    printf(  "fdFlags             : 0X%X\n", finfo->fdFlags);
    printf(  "fdLocation          : ");     record_print_Point(&finfo->fdLocation);
    printf("\nfdFldr              : %d\n",  finfo->fdFldr);
}
 
/* print extended File info */
static void record_print_FXInfo(FXInfo* xinfo)
{
    printf(  "fdIconID            : %d\n",   xinfo->fdIconID);
    // xinfo -> fdUnused;
    printf(  "fdComment           : %d\n",   xinfo->fdComment);
    printf(  "fdPutAway           : %d\n",  xinfo->fdPutAway);
} 

/* print file entry */
static void record_print_file(hfsp_cat_file* file)
{
    printf("flags               : 0x%X\n",	file->flags);
    printf("reserved1           : 0x%X\n",	file->reserved1);
    printf("id                  : %d\n",	file->id);
    printf("create_date         : %s",	get_atime(file->create_date));
    printf("content_mod_date    : %s",	get_atime(file->content_mod_date));
    printf("attribute_mod_date  : %s",	get_atime(file->attribute_mod_date));
    printf("access_date         : %s",	get_atime(file->access_date));
    printf("backup_date         : %s",	get_atime(file->backup_date));
    record_print_perm	(&file->permissions);
    record_print_FInfo	(&file->user_info);
    record_print_FXInfo	(&file->finder_info);
    printf("text_encoding       : 0x%X\n",	file->text_encoding);
    printf("reserved            : 0x%X\n",	file->reserved2);
    printf("Datafork:\n");
    print_fork (&file->data_fork);
    printf("Rsrcfork:\n");
    print_fork (&file->res_fork);
}

/* print info for a file or folder thread */
static void record_print_thread(hfsp_cat_thread* entry)
{
    char buf[255]; // mh this _might_ overflow 
    unicode_uni2asc(buf, &entry->nodeName, 255);   
    printf("parent cnid         : %d\n", entry->parentID);
    printf("name                : %s\n" , buf);
}

/* print the information for a record */
static void record_print_entry(hfsp_cat_entry* entry)
{
    switch (entry->type)
    {
	case HFSP_FOLDER:
	    printf("=== Folder ===\n");
	    return record_print_folder(&entry->u.folder);
	case HFSP_FILE:
	    printf("=== File ===\n");
	    return record_print_file  (&entry->u.file);
	case HFSP_FOLDER_THREAD:
	    printf("=== Folder Thread ===\n");
	    return record_print_thread(&entry->u.thread);
	case HFSP_FILE_THREAD:
	    printf("=== File Thread ==\n");
	    return record_print_thread(&entry->u.thread);
	default:
	    printf("=== Unknown Record Type ===\n");
    };
}

/* Dump all the record information to stdout */
void record_print(record* r)
{
    printf ("*** Key index       : %u\n", r->keyind);
    record_print_key  (&r->key);
    record_print_entry(&r->record);
}

/** Check the key of a catalog record */
static int fscheck_unistr255(hfsp_unistr255* name)
{
    int result = FSCK_NOERR;

    if (name->strlen > 255)
    {
	printf("strlen in name %d > 255\n", name->strlen);
	result |= FSCK_FSCORR; // hope we can fix that some time
    }

    return result;
}

/** Check the key of a catalog record */
static int fscheck_cat_key(record* r)
{
    int		    result  = FSCK_NOERR;
    hfsp_cat_key*   key	    = &r->key;
    hfsp_unistr255* name    = &key->name;
    volume*	    vol	    = r->tree->vol;
    UInt32	    cnid    = vol->vol.next_cnid;

    result |= fscheck_unistr255(name);

    if (key->key_length != ((name->strlen << 1) + 6))
    {
	printf("key_length in key %3d does not match %3d name\n", 
		    key->key_length, name->strlen);
	result |= FSCK_FSCORR; // hope we can fix that some time
    }

    if (key->parent_cnid >= cnid)
    {
	printf("parent_cnid %d >= volume next cnid %d\n", 
		    key->parent_cnid, cnid);
	result |= FSCK_FSCORR; // hope we can fix that some time
    }
    
    return result;
}

/** Check a macintosh time 
 *
 *  errname is the anem of the field to show on errror
 */
static int fscheck_mactime(UInt32 time, char* errname)
{
    /* This happens so often that be better ignore it
    if (!time)
	printf("Warning %s is 0\n", errname);
    */
    if (time > fsck_data.macNow)
	printf("Warning %21.21s is in the future: (%X) %s", 
		errname, time, get_atime(time));

    return FSCK_NOERR;	// Those are not really bad, just annoying
}

/** Check the file part of a catalog record */
static int fscheck_file(btree* tree, hfsp_cat_file* file)
{
    volume* vol	    = tree->vol;
    int	    result  = FSCK_NOERR;
    UInt32  cnid    = vol->vol.next_cnid;

    if (file->flags & HFSP_FILE_RESERVED)
	printf("Warning unknown file flags: %X\n", file->flags);

    if (fsck_data.maxCnid < file->id)
	fsck_data.maxCnid = file->id;

    // file->reserved1	// Nothing to check here
    if (file->id >= cnid)
    {
	printf("file id %d >= volume next cnid %d\n", 
		    file->id, cnid);
	result |= FSCK_FSCORR; // hope we can fix that some time
    }
    result |= fscheck_mactime(file->create_date,	"file create_date");
    result |= fscheck_mactime(file->content_mod_date,	"file content_mod_date");
    result |= fscheck_mactime(file->attribute_mod_date,	"file attribute_mod_date");
    result |= fscheck_mactime(file->access_date,	"file access_date");
    result |= fscheck_mactime(file->backup_date,	"file backup_date");
    /*
    // folder->permissions // dont know how tho check these
    Nothing to be checked here (but finder may become confused, hmm)
    file->user_info;
    file->finder_info;
    file->text_encoding;
    file->reserved;
    */
    result |= check_forkalloc(vol, &file->data_fork);
    result |= check_forkalloc(vol, &file->res_fork);
    return result;
}

/** Check the folder part of a catalog record */
static int fscheck_folder(btree* tree, hfsp_cat_folder* folder)
{
    UInt32  cnid    = tree->vol->vol.next_cnid;
    int	    result  = FSCK_NOERR;

    if (folder->flags & HFSP_FOLDER_RESERVED)
	printf("Warning unknown folder flags: %X\n", folder->flags);

    if (fsck_data.maxCnid < folder->id)
	fsck_data.maxCnid = folder->id;

    // folder->valence	// to be checked later
    if (folder->id >= cnid)
    {
	printf("Folder id %d >= volume next cnid %d\n", 
		    folder->id, cnid);
	result |= FSCK_FSCORR; // hope we can fix that some time
    }
    result |= fscheck_mactime(folder->create_date,	 "folder create_date");
    result |= fscheck_mactime(folder->content_mod_date,  "folder content_mod_date");
    result |= fscheck_mactime(folder->attribute_mod_date,"folder attribute_mod_date");
    result |= fscheck_mactime(folder->access_date,	 "folder access_date");
    result |= fscheck_mactime(folder->backup_date,	 "folder backup_date");
    /*
    // folder->permissions // dont know how tho check these
    Nothing to be checked here (but finder may become confused, hmm)
    folder->user_info;
    folder->finder_info;
    folder->text_encoding;
    folder->reserved;
    */
    
    return result;
}

/** Check the entry part of a catalog record */
static int fscheck_thread(btree* tree, hfsp_cat_thread* thread)
{
    UInt32  cnid    = tree->vol->vol.next_cnid;
    int	    result  = fscheck_unistr255(&thread->nodeName);

    if (thread->parentID >= cnid)
    {
	printf("Thread parentID %d >= volume next cnid %d\n", 
		    thread->parentID, cnid);
	result |= FSCK_FSCORR; // hope we can fix that some time
    }
    return result;
}
 

/** Check the entry part of a catalog record */
static int fscheck_entry(record* r)
{
    hfsp_cat_entry  *entry = &r->record;
    btree	    *tree  = r->tree;
    switch (entry->type)
    {
	case HFSP_FOLDER:
	    return fscheck_folder(tree, &entry->u.folder);
	case HFSP_FILE:
	    return fscheck_file  (tree, &entry->u.file);
	case HFSP_FOLDER_THREAD:
	    return fscheck_thread(tree, &entry->u.thread);
	case HFSP_FILE_THREAD:
	    return fscheck_thread(tree, &entry->u.thread);
	default:
	    printf("Unknown Record Type %X\n", entry->type);
	    return FSCK_FSCORR; // Hope we can fix it some time
    }
}

/** Check a record as a directory, file, extent_node etc. */
static int fscheck_record(record* r)
{
    int	    result  = fscheck_cat_key(r);
    
    result |= fscheck_entry(r);
    if (fsck_data.verbose)
	record_print(r);

    return  result;
}

/* find correct node record for given node and *pindex.
 *
 * index of record in this (or next) node
 */
static node_buf* fscheck_prepare_next(btree* tree, UInt16 node_index, 
				      UInt16* pindex, int* fsckerr)
{
    node_buf*		buf = btree_node_by_index(tree, node_index, NODE_CLEAN);
    btree_node_desc*	desc;
    UInt32		numrec;

    if (!buf)
	return buf;
    desc   = &buf->desc;
    numrec = desc->num_rec;
    if (*pindex >= numrec) // move on to next node
    {
	UInt16 next = desc->next;
	*pindex = 0;
	if (!next   /* is there a next node ? */
	||  !( buf = btree_node_by_index(tree, next, NODE_CLEAN)))
	    return NULL;
        if (!btree_check_nodealloc(tree, next))
	{
	    printf("node %d not allocated in node Map\n", next);
	    *fsckerr = *fsckerr | FSCK_FSCORR; /* Maybe we can correct that one time */
	}
    }
    return buf;
}

/* intialize the catalog record with the given index entry in the btree. 
 *
 * Special version to check for consistency of backpointers.
 *
 * r	the record used as read buffer.
 * bt	the btree we care for
 */
int fscheck_record_init(record* r, btree* bt, node_buf* buf, UInt16 index)
{
    void *p,*p1,*p2;
    int diff;
    r-> tree   = bt;
    p = p1 = checkbtree_key_by_index(bt,r->node_index,buf,index);
    if (!p)
	return -1;
    p = record_readkey  (p, &r->key);
    if (!p)
	return -1;
    p = record_readentry(p, &r->record);
    if (!p)
	return -1;
    r->node_index = buf->index;
    r-> keyind    = index;
    p2 = checkbtree_key_by_index(bt,r->node_index,buf,index+1);
    diff = (int) (p2 - p);
    if (diff)	// The difference may still be correct in case of a hole in the
    {		// structure (should happen while debugging only)
	fprintf(stderr, 
	    "Unexpected difference in Node %d, Record %d "
	    ": %d (%d/%d) (%p,%p)\n",
	    r->node_index, index, diff , p - p1, p2 - p1, p, p2);
	record_print(r);
    }

    return 0;
}


/* move record foreward to next entry in leaf nodes.
 *
 * In case of an error the value of *r is undefined ! 
 */
int fscheck_record_next(record* r, int* fsckerr)
{
    btree*	tree	= r->tree;
    UInt16	index	= r->keyind +1;
    UInt32	parent;
    node_buf*	buf	= fscheck_prepare_next(
	    tree, r->node_index, &index, fsckerr);
    
    if (!buf)
	return ENOENT;	// No (more) such file or directory
    
    parent = r->key.parent_cnid;

    if (fscheck_record_init(r, tree, buf, index))
    {
	printf("Unable to read record %d in node %d"
		,index, r->node_index);
	return -1;
    }

    return 0;
}

/** Check all files in leaf nodes */
int fscheck_files(volume* vol)
{
    int		result  = FSCK_NOERR;
    btree*	catalog = &vol->catalog;
    node_buf*	buf	= 
	btree_node_by_index(catalog,catalog->head.leaf_head, NODE_CLEAN);
    // void*	p	= btree_key_by_index(catalog,buf,0);
    record	r;

    if (!btree_check_nodealloc(catalog, catalog->head.leaf_head))
    {
	printf("leaf_head %d not allocated in node Map\n",
		catalog->head.leaf_head);
	result |= FSCK_FSCORR; /* Maybe we can correct that one time */
    }

    if (fscheck_record_init(&r, catalog, buf, 0))
    {
	printf("Unable to read initial leaf record\n");
	return FSCK_ERR;
    }

    do {
	result |= fscheck_record(&r);
	if (result & FSCK_FATAL)
	    return result;
    } while (!fscheck_record_next(&r, &result));

    return result;
}
