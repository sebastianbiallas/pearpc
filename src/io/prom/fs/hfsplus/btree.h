/*
 * libhfs - library for reading and writing Macintosh HFS volumes.
 *
 * The fucntions are used to handle the various forms of btrees
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

/* Read the node from the given buffer and swap the bytes.
 *
 * return pointer after reading the structure
 */
char* btree_readnode(btree_node_desc* node, char *p);

/* read a btree header from the given buffer and swap the bytes.
 *
 * return pointer after reading the structure
 */
char* btree_readhead(btree_head* head, char *p);

/** Intialize catalog btree, so that btree_close can safely be called. */
extern void btree_reset(btree* bt);

/** Intialize catalog btree */
extern int btree_init_cat(btree* bt, volume* vol, hfsp_fork_raw* fork);

/** Intialize extents btree */
extern int btree_init_extent(btree* bt, volume* vol, hfsp_fork_raw* fork);

/** close the btree and free any resources */
extern int btree_close(btree* bt);

/* Read node at given index.
 *
 * Make node with given flag, usually NODE_CLEAN/NODE_DIRTY
 */
extern node_buf* btree_node_by_index(btree* bt, UInt16 index, int flags);

/* returns pointer to key given by index in current node */
extern void* btree_key_by_index(btree* bt, node_buf* buf, UInt16 index);

/* remove the key and record from the btree.
 * Warning, you must WRITELOCK the btree before calling this */
extern int btree_remove_record(btree* bt, UInt16 node_index, UInt16 recind);

/* insert a key and an (eventual) record into the btree.
 * Warning, you must WRITELOCK the btree before calling this */
extern int btree_insert_record(btree* bt, UInt16 node_index, UInt16 keyind,
			void* key, int len);

// --------------- Cache Handling ------------

/* Priority of the depth of the node compared to LRU value.
 * Should be the average number of keys per node but these vary. */
#define DEPTH_FACTOR	1000

/* Cache size is height of tree + this value 
 * Really big numbers wont help in case of ls -R
 * Must be enough to cache all nodes (+ Map nodes !)
 * used during tree reorganizations to avoid
 * inconsistent states before flush
 */
#define EXTRA_CACHESIZE	3 

/* Intialize cache with default cache Size, 
 * must call node_cache_close to deallocate memory */
extern int node_cache_init(node_cache* cache, btree* tree, int size);

/** return allocation status of node given by index in btree */
extern int btree_check_nodealloc(btree* bt, UInt16 node);

