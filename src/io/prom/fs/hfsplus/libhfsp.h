/*
 * libhfs - library for reading and writing Macintosh HFS volumes
 *
 * Copyright (C) 2000 Klaus Halfmann (klaus.halfmann@feri.de)
 * Original work by 1996-1998 Robert Leslie (rob@mars.org)
 *
 * This file defines constants,structs etc needed for this library.
 * Everything found here is usually not related to Apple defintions.
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

#ifndef APPLE_H
# include "apple.h"
#endif

# include "hfs.h"
# include "hfsp.h"

extern int errno;
/* Last error is eventually found here */
extern const char *hfsp_error; 

# define HFSP_ERROR(code, str)  \
    do { hfsp_error = (str), errno = (code); goto fail; } while (0)

# ifdef DEBUG
#  define ASSERT(cond)	do { if (! (cond)) abort(); } while (0)
# else
#  define ASSERT(cond)	/* nothing */
# endif

# define SIZE(type, n)		((size_t) (sizeof(type) * (n)))
# define ALLOC(type, n)		((type *) malloc(SIZE(type, n)))
# define ALLOCX(type, n)	((n) ? ALLOC(type, n) : (type *) 0)
# define FREE(ptr)		((ptr) ? (void) free((void *) ptr) : (void) 0)

# define REALLOC(ptr, type, n)  \
    ((type *) ((ptr) ? realloc(ptr, SIZE(type, n)) : malloc(SIZE(type, n))))
# define REALLOCX(ptr, type, n)  \
    ((n) ? REALLOC(ptr, type, n) : (FREE(ptr), (type *) 0))

# define STRINGIZE(x)		#x
# define STR(x)			STRINGIZE(x)

/* These macros will eventually be filled for a kernel-module or
 * a multithreaded environement */
#define	HFSP_READLOCK	    1
#define	HFSP_WRITELOCK	    2
    
#define HFSP_SYNC_START(mode, whatever)	    /* to be filled */
#define HFSP_SYNC_END(mode, whatever)	    /* to be filled */

/* Flags for volume header */
/* used by internal routines to specify the open modes */
# define HFSP_MODE_RDONLY        0x00
# define HFSP_MODE_RDWR          0x01
    /** need to write backup volume header, due to significant change */
# define HFSP_BACKUP_DIRTY	 0x02

/* Signatures registered with Apple to identify this driver */
    /* Identifies the userland implementation */
# define HPLS_SIGNATURE 0x482B4C58	// 'H+LX' 
# define HFSPLUS_VERS   0x0004		// Current version of HFS+ Specification
    /* Identifies the kernel module by Brad Boyer (flar@pants.nu) */
# define HPLS_SIGRES1	0x482B4C78	// 'H+Lx'
    /* not jet in use ... */
# define HPLS_SIGRES2	0x482B6C78	// 'H+lx'
    /* Signature used by Apple */
# define HPAPPLE_SIGNATURE  0x382e3130	// '8.10'

/* Version used for this implementation of HFS+. This is not related
 * to the VERSION file found at the top-level of this package,
 * but designates the version of the low level code */
#define HPLS_VERSION	1   /* must fit in a short */

    
/** helper function to create those Apple 4 byte Signatures */
static inline UInt32 sig(char c0, char c1, char c2, char c3)
{
    UInt32 sig;
    ((char*)&sig)[0] = c0;
    ((char*)&sig)[1] = c1;
    ((char*)&sig)[2] = c2;
    ((char*)&sig)[3] = c3;
    return sig;
}



/* Other Signatures may follow for informational purposes */
    
/* prototype for key comparing functions. */
typedef int (*hfsp_key_compare) (void* key1, void* key2);

/* The read functions alaways retur the positiion right after
 * reading (and swapping) the variable amount of bytes needed 
 * prototype for key reading (necessary for byte swapping) */
typedef char* (*hfsp_key_read) (char* p, void* key);

/* prototype for record reading (including byte swapping) */
typedef char* (*hfsp_rec_read) (char* p, void* key);

struct volume; /* foreward declaration for btree needed */

/* Structures for a node cache. The cache is an array
 * with linear search. (So making it to big may make
 * things slower). It is searched in a round robin 
 * fashion.
 */

typedef struct 
{
    UInt32		priority;	
	// as lower this number as higher the priority.
	// decremetned on any sucessfull usage
	// incremented else, intial value height*DEPTHFACTOR
    UInt16		index;	// of node in fork
	// 0 means empty, since first node is node header
    UInt16		flags;	// like DIRTY etc.
} node_entry;

/* an Entry is dirty and must be written back */
#define NODE_DIRTY	    0x0001
/* opposite of dirty */
#define NODE_CLEAN	    0x0000

typedef struct
{
    UInt32		index;	    // duplicate of above
    btree_node_desc	desc;	    // header of node
    char		node[0];    // size is actual node_size
	// contents of node in original byte order
} node_buf;

typedef struct 
{
    int		size;	     // number of nodes in the cache
    int		currindex;   // round robin index
    int		nodebufsize; // size of complete node_buf, including node
    node_entry	*entries;	
    char	*buffers;   // actually *node_buf
} node_cache;

typedef struct 
{   // "virtual" member functions
    hfsp_key_compare	kcomp;	
	/* function used for key compare in _this_ btree */
    hfsp_key_read	kread;
	/* function used to read a key in _this_ btree */
    hfsp_rec_read	rread;
	/* function used to read a record in _this_ btree */
    btree_node_desc	head_node;
	/* Node descriptor for header node */
    btree_head		head;

    struct volume*	vol;	/* pointer to volume this tree is part of */
    hfsp_fork_raw*	fork;	/* pointer to fork this tree is part of */
    UInt32		cnid;	/* (pseudo) file id for the fork */
    UInt32	        attributes; /* see bits below */
	/* Header node (contains most important parts of btree) */
    char*		alloc_bits;
	/* All the rest of node 0 including the reserved area,
	 * the first part of the allocation bit-map found in Map Nodes
	 * and the two bakpointers needed for the header node, so this
	 * memory + head represent the header node */
    UInt16		blkpernode; 
	 /* Number of volume blocks per node (usually 0-4) 
 	    0 indicates, that nodeperblk should be used */
    UInt16		nodeperblk; 
	 /* Sometimes a block may contain more than one node */
    UInt16		max_rec_size;
	/* Maximum Size of a leaf record */
    UInt16		filler;
    node_cache		cache; 
} btree;

/* The Btree header is dirty and must be written back */
#define BTREE_HEADDIRTY	    0x0001

/* The reserved Record starts at this offset in the header header node */
#define HEADER_RESERVEDOFFSET   120 

/* The first map node eventually starts at this offset in the 
 * header node */
#define HEADER_MAPOFFSET	248

/* Function on btrees are defined in btree.h */

/* A Wrapper around the raw hfs+ volume header for additional information
 * needed by this library.
 */

typedef struct volume
{
    void	*fd;		/* OS dependend reference to device	    */
    UInt16	blksize_bits;   /* blocksize of device = 1 << blksize_bits  */
    UInt16	flags;		/* as of now only HFSP_MODE_RDWR	    */
    UInt32	blksize;	/* always 1 << blksize_bits		    */
    UInt32  	maxblocks;	/* maximum number of blocks in device */
    hfsp_vh	vol;		/* raw volume data */
    
    btree*	extents;	/* is NULL by default and intialized when needed */
    btree	catalog;	/* This is always neeeded */
} volume; 

/* Functions on volumes are defined in volume.h */

typedef struct {    // may not be used as found here 
    btree*		tree;	// tree where this record is contained in.
    UInt16		node_index; /* index of node in btree */
    UInt16		keyind;	/* index of current key in node */
	/* Warning node_index and keyind are private. They can become
	 * invalid when the btree changes */
    hfsp_cat_key	key;	/* current key */
    UInt32		child;	/* child node belonging to this key */
} index_record;

typedef struct {   
    btree*		tree;	// tree where this record is contained in.
    UInt16		node_index; /* index of node in btree */
    UInt16		keyind;	/* index of current key in node */
	/* Warning node_index and keyind are private. They can become
	 * invalid when the btree changes */
    hfsp_extent_key	key;	/* current key */
    hfsp_extent_rec	extent; /* The payload carried around */
} extent_record;

typedef struct {
    btree*		tree;	// tree where this record is contained in.
    UInt16		node_index; /* index of node in btree */
    UInt16		keyind;	/* index of current key in node */
	/* Warning node_index and keyind are private. They can become
	 * invalid when the btree changes */
    hfsp_cat_key	key;	/* current key */
    hfsp_cat_entry	record;	/* current record */
} record;

/* Functions on records are defined in record.h */



