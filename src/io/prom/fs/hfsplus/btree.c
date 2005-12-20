/*
 * libhfs - library for reading and writing Macintosh HFS volumes
 * The fucntions are used to handle the various forms of btrees
 * found on HFS+ volumes.
 *
 * The functions are used to handle the various forms of btrees
 * found on HFS+ volumes.
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

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif
 
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <limits.h>
# include <errno.h>

# include "libhfsp.h"
# include "volume.h"
# include "btree.h"
# include "record.h"
# include "swab.h"

#ifdef HAVE_MEMSET
#    define bzero(_a_,_s_) memset (_a_,0,_s_)
#endif

/* Read the node from the given buffer and swap the bytes.
 *
 * return pointer after reading the structure
 */
char* btree_readnode(btree_node_desc* node, char *p)
{
    node->next	    = bswabU32_inc(&p);
    node->prev	    = bswabU32_inc(&p);
    node->kind	    = bswabU8_inc(&p);
    node->height    = bswabU8_inc(&p);
    node->num_rec   = bswabU16_inc(&p);
    node->reserved  = bswabU16_inc(&p);
    return p;
} 

/* Write the node to the given buffer and swap the bytes.
 *
 * return pointer after writing the structure
 */
char* btree_writenode(btree_node_desc* node, char *p)
{
    bstoreU32_inc(&p, node->next);
    bstoreU32_inc(&p, node->prev);
    bstoreU8_inc (&p, node->kind);
    bstoreU8_inc (&p, node->height);
    bstoreU16_inc(&p, node->num_rec);
    bstoreU16_inc(&p, node->reserved);
    return p;
} 


/* read a btree header from the given buffer and swap the bytes.
 *
 * return pointer after reading the structure
 */
char* btree_readhead(btree_head* head, char *p)
{
    int i;
    head->depth		= bswabU16_inc(&p);
    head->root		= bswabU32_inc(&p);
    head->leaf_count    = bswabU32_inc(&p);
    head->leaf_head	= bswabU32_inc(&p);
    head->leaf_tail	= bswabU32_inc(&p);
    head->node_size	= bswabU16_inc(&p);
    head->max_key_len   = bswabU16_inc(&p);
    head->node_count    = bswabU32_inc(&p);
    head->free_nodes    = bswabU32_inc(&p);
    head->reserved1	= bswabU16_inc(&p);
    head->clump_size    = bswabU32_inc(&p);
    head->btree_type    = bswabU8_inc(&p);
    head->reserved2	= bswabU8_inc(&p);
    head->attributes    = bswabU32_inc(&p);
    for (i=0; i < 16; i++)
	head->reserved3[i] = bswabU32_inc(&p);
    return p;
}

/* read a btree header from the given buffer and swap the bytes.
 *
 * return pointer after reading the structure
 */
char* btree_writehead(btree_head* head, char *p)
{
    int i;
    bstoreU16_inc(&p, head->depth);
    bstoreU32_inc(&p, head->root);
    bstoreU32_inc(&p, head->leaf_count);
    bstoreU32_inc(&p, head->leaf_head);
    bstoreU32_inc(&p, head->leaf_tail);
    bstoreU16_inc(&p, head->node_size);
    bstoreU16_inc(&p, head->max_key_len);
    bstoreU32_inc(&p, head->node_count);
    bstoreU32_inc(&p, head->free_nodes);
    bstoreU16_inc(&p, head->reserved1);
    bstoreU32_inc(&p, head->clump_size);
    bstoreU8_inc (&p, head->btree_type);
    bstoreU8_inc (&p, head->reserved2);
    bstoreU32_inc(&p, head->attributes);
    for (i=0; i < 16; i++)
	bstoreU32_inc(&p, head->reserved3[i]);
    return p;
}

/* Intialize cache with default cache Size, 
 * must call node_cache_close to deallocate memory */
int node_cache_init(node_cache* cache, btree* tree, int size)
{
    int nodebufsize;
    char * buf;

    cache->size		= size;
    cache->currindex	= 0;
    nodebufsize = tree->head.node_size + sizeof(node_buf);
    buf = malloc(size *(sizeof(node_entry) + nodebufsize));
    if (!buf)
	return -1;
    cache -> nodebufsize = nodebufsize;
    cache -> entries = (node_entry*) buf;
    cache -> buffers = (char*) &cache->entries[size];
    bzero(cache->entries, size*sizeof(node_entry));
    return 0;
}

/* Like cache->buffers[i], since size of node_buf is variable */
static inline node_buf* node_buf_get(node_cache* cache, int i)
{
    return (node_buf*) (cache->buffers + (i * cache->nodebufsize));
}


/* write back a node at given node in fork with nodebuf */
static int btree_write_node(btree* bt, int index, char* nodebuf)
{
    UInt16	blkpernode  = bt->blkpernode;
    UInt16	nodeperblk  = bt->nodeperblk;

    if (blkpernode)
    {
	return volume_writetofork(bt->vol, nodebuf, bt->fork, 
		     index * blkpernode, blkpernode, HFSP_EXTENT_DATA, bt->cnid);
    }
    else // use nodeperblk, must reread other blocks, too
    {
	char buf[bt->vol->blksize];
	
	UInt16	node_size = bt->head.node_size;
	UInt16	offset    = (index % nodeperblk) * node_size;
	int	block     = index / nodeperblk;

	// read block including this one 
	void* p  = volume_readfromfork(bt->vol, buf, bt->fork, 
		     block, 1, HFSP_EXTENT_DATA, bt->cnid);
	if (p != buf)	    // usually NULL
	    return -1;	    // evil ...
	p = &nodebuf[offset];   // node is found at that offset
	memcpy(p, nodebuf, node_size);
	if (volume_writetofork(bt->vol, buf, bt->fork, 
		     block, 1, HFSP_EXTENT_DATA, bt->cnid))
	    return -1;	// evil ...
    }
    return 0; // all is fine
}

/* flush the node at cache index */
static int node_cache_flush_node(btree* bt, int index)
{
    node_entry	*e	    = &bt->cache.entries[index];
    int		result	    = 0;
    int		node_index  = e->index;

    // Only write back valid, dirty nodes
    if(e->index && (e->flags & NODE_DIRTY))
    {
	node_buf* b = node_buf_get(&bt->cache, index);
	if (!b->index)
	{
	    hfsp_error = "cache inconsistency in node_cache_flush_node";
	    return -1;
	}
	// Write back node header to cached memory
	btree_writenode(&b->desc, b->node);
	result = btree_write_node(bt, node_index, b->node);
	b->index = 0;	// invalidate block entry
    }
    e->index = 0;	// invalidate cache entry
    return result; // all is fine
}

/* flush the cache */
static int node_cache_flush(btree* bt)
{
    int i, size = bt->cache.size;
    int result = 0;

    for (i=0; i < size; i++)
    {
	node_entry* e = &bt->cache.entries[i];
	if(e->index && (e->flags & NODE_DIRTY))
	    if (node_cache_flush_node(bt, i))
		result = -1;
    }
    return result;
}

static int node_cache_close(btree* bt)
{
    int result = 0;
    if (!bt->cache.entries) // not (fully) intialized ?
	return result;
    result = node_cache_flush(bt);
    free(bt->cache.entries);
    return result;
}

/* Load the cach node indentified by index with 
 * the node identified by node_index.
 *
 * Set the inital flags as given.
 */

static node_buf* node_cache_load_buf
    (btree* bt, node_cache* cache, int index, UInt16 node_index, int flags)
{
    node_buf	*result	    = node_buf_get(cache ,index);
    UInt16	blkpernode  = bt->blkpernode;
    UInt16	nodeperblk  = bt->nodeperblk;
    node_entry	*e	    = &cache->entries[index];
    UInt32	block;
    void	*p;

    if (blkpernode)
    {
	block = node_index * blkpernode;
	p     = volume_readfromfork(bt->vol, result->node, bt->fork, 
		     block, blkpernode, HFSP_EXTENT_DATA, bt->cnid);
	if (!p)
	    return NULL;	// evil ...
	
	btree_readnode(&result->desc, p);
    }
    else // use nodeperblk
    {
	char buf[bt->vol->blksize];
	
	UInt16 node_size = bt->head.node_size;
	UInt16 offset = node_index % nodeperblk * node_size;
	block = node_index / nodeperblk;
	p     = volume_readfromfork(bt->vol, buf, bt->fork, 
		     block, 1, HFSP_EXTENT_DATA, bt->cnid);
	if (p != buf)		// usually NULL
	    return NULL;	// evil ...
	p = &buf[offset];   // node is found at that offset
	memcpy(&result->node, p , node_size);
	btree_readnode(&result->desc, p);
	
    }

    result->index   = node_index;

    e -> priority   = result->desc.height * DEPTH_FACTOR;
    e -> index	    = node_index;
    e -> flags	    = flags;
    return result;
}

/* Mark node at given index dirty in cache.
 */
inline static void btree_dirty_node(btree* bt, UInt16 index)
{
    node_cache*	cache	= &bt->cache;
    node_entry	*e	= &cache->entries[index];
    e->flags |= NODE_DIRTY;
}

/* Read node at given index, using cache.
 *
 * Make node with given flag, usually NODE_CLEAN/NODE_DIRTY
 */
node_buf* btree_node_by_index(btree* bt, UInt16 index, int flags)
{
    node_cache*	cache = &bt->cache;
    int		oldindex, lruindex;
    int		currindex = cache->currindex;
    UInt32	prio;
    node_entry	*e;

    // Shortcut acces to current node, will not change priorities
    if (cache->entries[currindex].index == index)
    {
	cache->entries[currindex].flags |= flags;
	return node_buf_get(cache ,currindex);
    }
    oldindex = currindex;
    if (currindex == 0)
	currindex = cache->size;
    currindex--;
    lruindex = oldindex;	    // entry to be flushed when needed
    prio     = 0;		    // current priority
    while (currindex != oldindex)   // round robin
    {
	e = &cache->entries[currindex];
	if (e->index == index)	    // got it
	{
	    if (e->priority != 0)   // already top, uuh
		e->priority--;
	    cache->currindex = currindex;
	    e -> flags	    |= flags;
	    return node_buf_get(cache ,currindex);
	}
	else
	{
	    if (!e->index) // free entry, well
	    {
		lruindex = currindex;
		prio     = UINT_MAX; // remember empty entry
	    }
	    if (e->priority != UINT_MAX) // already least, uuh
		e->priority++;
	}
	if (prio < e->priority)
	{
	    lruindex = currindex;
	    prio     = e->priority;
	}
	if (currindex == 0)
	    currindex = cache->size;
	currindex--;
    }
    e = &cache->entries[lruindex];
    cache->currindex = lruindex;
    if (e->flags & NODE_DIRTY)
           node_cache_flush_node(bt, lruindex);
    return node_cache_load_buf  (bt, cache, lruindex, index, flags);
}

/** intialize the btree with the first entry in the fork */
static int btree_init(btree* bt, volume* vol, hfsp_fork_raw* fork)
{ 
    char		*p;
    char		buf[vol->blksize]; 
    UInt16		node_size;
    btree_node_desc*	node = &bt->head_node;
    int			alloc_size;

    bt->vol	= vol;
    bt->fork	= fork;
    p	= volume_readfromfork(vol, buf, fork, 0, 1,
		 HFSP_EXTENT_DATA, bt->cnid);
    if (!p)
	return -1;
    p = btree_readnode(node, p);
    if (node->kind != HFSP_NODE_HEAD)
	return -1;   // should not happen ?
    p = btree_readhead(&bt->head, p);
    node_size	= bt->head.node_size;

    bt->blkpernode = node_size / vol->blksize;

    if (bt->blkpernode == 0)	// maybe the other way round ?
	bt->nodeperblk = vol->blksize / node_size;

    alloc_size = node_size - HEADER_RESERVEDOFFSET; // sizeof(node_desc) + sizeof(header) 
    /* Sometimes the node_size is bigger than the volume-blocksize
     * so here I reread the node when needed */
    { // need new block for allocation
	char nodebuf[node_size];
	if (bt->blkpernode > 1)
	{
	    p = volume_readfromfork(vol, nodebuf, fork, 0, bt->blkpernode,
		 HFSP_EXTENT_DATA, bt->cnid);
	    p += HEADER_RESERVEDOFFSET; // skip header
	}
	
	bt->alloc_bits = malloc(alloc_size);
	if (!bt->alloc_bits)
	    return ENOMEM;
	memcpy(bt->alloc_bits, p, alloc_size);
    }

    /*** for debugging ***/ 
    bt->attributes = 0;

    if (node_cache_init(&bt->cache, bt, bt->head.depth + EXTRA_CACHESIZE))
	return -1;

    return 0;
}

/** Intialize catalog btree, so that btree_close can safely be called. */
void btree_reset(btree* bt)
{
    bt->alloc_bits    = NULL;
    bt->cache.entries = NULL;
}
 
/** Intialize catalog btree */
int btree_init_cat(btree* bt, volume* vol, hfsp_fork_raw* fork)
{ 
    int result = btree_init(bt,vol,fork);	// super (...)
    bt->cnid  = HFSP_CAT_CNID;
    bt->kcomp = record_key_compare;
    bt->kread = record_readkey;
    bt->rread = record_readentry;
    bt->max_rec_size = sizeof(hfsp_cat_entry);
	// this is a bit too large due to alignment/padding
    return result;
}

/** Intialize catalog btree */
int btree_init_extent(btree* bt, volume* vol, hfsp_fork_raw* fork)
{ 
    int result = btree_init(bt,vol,fork);	// super (...)
    bt->cnid  = HFSP_EXT_CNID;
    bt->kcomp = record_extent_key_compare;
    bt->kread = record_extent_readkey;
    bt->rread = record_extent_readrecord;
    bt->max_rec_size = sizeof(hfsp_extent);
    return result;
}

/** close the btree and free any resources */
int btree_close(btree* bt)
{
    int result = 0;
    
    node_cache_close(bt);
    // a dirty header without alloc_bits must not happen, mmh
    if ((bt->attributes & BTREE_HEADDIRTY) && bt->alloc_bits)
    {
	btree_head*	    head	= &bt->head;
	btree_node_desc*    node	= &bt->head_node;
	UInt16		    node_size   = head->node_size; 
	UInt16		    alloc_size  = node_size - HEADER_RESERVEDOFFSET; 
	char		    buf[node_size];
	void		    *p;
	
	p = btree_writenode(node, buf);
	p = btree_writehead(head, p);
	memcpy(p, bt->alloc_bits, alloc_size);
	result = btree_write_node(bt, 0, buf);
    }
    if (bt->alloc_bits)
	free(bt->alloc_bits);
    return result;
}

/* returns pointer to key given by index in current node.
 *
 * Assumes that current node is not NODE_HEAD ...
 * index may be == num_rec in whcih case a pointer
 * to the first free byte is returned ...
 */   
void* btree_key_by_index(btree* bt, node_buf* buf, UInt16 index)
{
    UInt16		node_size, off_pos;
    btree_record_offset offset;

    if (index > buf->desc.num_rec)	// oops out of range
    {
	hfsp_error = "btree_key_by_index: index out of range";
	return NULL;
    }
    node_size	    = bt->head.node_size; 
	// The offsets are found at the end of the node ...
    off_pos	    = node_size - (index +1) * sizeof(btree_record_offset);
 	// position of offset at end of node
   if (off_pos >= node_size)	// oops out of range
    {
	hfsp_error = "btree_key_by_index: off_pos out of range";
	return NULL;
    }
    offset = bswabU16(*((btree_record_offset*) (buf->node + off_pos)));
    if (offset >= node_size)	// oops out of range
    {
	hfsp_error = "btree_key_by_index: offset out of range";
	return NULL;
    }
  
    // now we have the offset and can read the key ...
    return buf->node + offset;
}

/** return allocation status of node given by index in btree */

int btree_check_nodealloc(btree* bt, UInt16 node)
{
    btree_head* head        = &bt->head;
    btree_node_desc* desc   = &bt->head_node;
    UInt16      node_size   = head->node_size;
    UInt16	node_num    = desc->next;
    UInt32      node_count  = head->node_count;
    char*	alloc_bits  = bt->alloc_bits + 128; // skip reserved node
    int		bit	    = node & 0x07;
    int		alloc_size  = node_size - 256; 
	    // sizeof(node_desc) + sizeof(header) + 128 - 8
    node_buf*	nbuf	    = NULL;
      
    if (node >= node_count)
    	HFSP_ERROR(-1, "Node index out of range.");
    node  >>= 3;
    // First must check bits in saved allocation bits from node header
    if (node < alloc_size)
	return (alloc_bits[node] & (0x80 >> bit)); /* Bit one is 0x80 ! */
    // Now load matching node by iterating over MAP-Nodes
    node -= alloc_size; 
    while (node_num && node >= node_size)
    {
	nbuf = btree_node_by_index(bt, node_num, NODE_CLEAN);
	if (!nbuf)
	    HFSP_ERROR(-1, "Unable to fetch map node");
	desc = &nbuf->desc;
	if (desc->kind != HFSP_NODE_MAP)
	    HFSP_ERROR(-1, "Invalid chain of map nodes");
	node -= node_size;
	node  = desc->next;
    }
    if (!nbuf)
	HFSP_ERROR(-1, "Oops this code is wrong");  // Should not happen, oops
    return (nbuf->node[node] & (0x80 >> bit));	    /* Bit one is 0x80 ! */
fail:
    return -1;
}

/* Remove the key and an (eventual) record from the btree.
 * Warning, you must WRITELOCK the btree before calling this 
 */
int btree_remove_record(btree* bt, UInt16 node_index, UInt16 keyind)
{
    node_buf*	     node    = btree_node_by_index(bt, node_index, NODE_DIRTY);
    btree_node_desc* desc    = &node->desc;
    int		     num_rec = desc->num_rec;
    void*	     curr    = btree_key_by_index(bt, node, keyind);

    if (keyind != num_rec) // Last record needs no special action
    {
	void	*next	  = btree_key_by_index(bt, node, keyind+1);
	void	*last	  = btree_key_by_index(bt, node, num_rec);
	// difference needed to update the backpointers
	int	diff	  = ((char*) next) - ((char*) curr);
	// hfsp_key* key  = (hfsp_key*) curr; 
	// UInt16  help	  = key->key_length;
	// size of the block to move "down"
	int	size	  = ((char*) last) - ((char*) next);
	UInt16	node_size = bt->head.node_size;
	int   i,n, off_pos;
	btree_record_offset* offsets;
	btree_record_offset old; 

	memmove(curr, next, size);

	// Now care about the backpointers,
	off_pos = node_size - (num_rec + 1) * sizeof(btree_record_offset);
	offsets = (btree_record_offset*) (node->node + off_pos);

	// fprintf(stderr, 
	//    "moving backpointers in node %d by %4x (%d)\n",
	//    node_index, diff, help);

	// The backpointer at keyind is already correct
	n   = num_rec - keyind;
	old = 0xffff; 
	    // will override former index to free area, thats ok
	for (i=0; i <= n; i++)
	{
	    btree_record_offset off = offsets[i];
	    // fprintf(stderr, "moving backpointers %4x, %4x\n", off, old);
	    offsets[i] = old;
	    
	    old = off - diff; // adjust the backpointer
	}
    }
    desc->num_rec = num_rec - 1;

    if (desc->kind == HFSP_NODE_LEAF)
    {
	bt->head.leaf_count --;
	bt->attributes |= BTREE_HEADDIRTY;
    }
    
    return 0;
}

/* insert a key and an (eventual) record into the btree.
 * Warning, you must WRITELOCK the btree before calling this.
 * keyind may well be == num_rec indicating an append.
 *
 * node_index number of the node in the tree.
 * keyind   the index where the key/record should be insert in the node
 * key	    the key (+ record) to append
 * len	    the lenght of the key or key + record
 */
int btree_insert_record(btree* bt, UInt16 node_index, UInt16 keyind,
			void* key, int len)
{
    node_buf*		node    = btree_node_by_index(bt, node_index, NODE_DIRTY);
    btree_node_desc*	desc    = &node->desc;
    int			num_rec = desc->num_rec;
    UInt16		node_size= bt->head.node_size;
    void		*curr,*last;	// Pointer for calculations
    int			size, total; 
    int			i,n,off_pos;
    btree_record_offset* offsets;

    curr    = btree_key_by_index(bt, node, keyind);
    last    = btree_key_by_index(bt, node, num_rec);
	// size of the block to move "up"
    size     = ((char*) last) - ((char*) curr);
    total    = ((char*) last) - node->node; 
    
		// account for backpointers, too 
    if ((total + len) > (node_size - num_rec * sizeof(btree_record_offset)))
	return -1;  // need to split/repartition node first, NYI

    memmove(curr + len, curr, size);
    // printf("Moving to [%p %p]\n", curr+len, curr+len+size);
    memcpy (curr      , key , len);	// Copy the key / record

    num_rec ++;
    // Now care about the backpointers,
    off_pos = node_size - (num_rec + 1) * sizeof(btree_record_offset);
    offsets = (btree_record_offset*) (node->node + off_pos);

    // Recalculate backpointers
    n = num_rec - keyind;
    // printf("Copying to [%p %p]\n", offsets, &offsets[n]);
    for (i=0; i < n; i++)
	offsets[i] = offsets[i+1] + len;
    desc->num_rec = num_rec; // Adjust node descriptor

    if (desc->kind == HFSP_NODE_LEAF)
    {
	bt->head.leaf_count ++;
	bt->attributes |= BTREE_HEADDIRTY;
    }
    
    return 0;
}
