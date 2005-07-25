/*
 * libhfs - library for reading and writing Macintosh HFS volumes
 *
 * Code to acces the basic volume information of a HFS+ volume.
 *
 * Copyright (C) 2000 Klaus Halfmann <klaus.halfmann@feri.de>
 * Original work by 1996-1998 Robert Leslie <rob@mars.org>
 * other work 2000 from Brad Boyer (flar@pants.nu)  
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

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <stdlib.h>
# include <stdio.h>
# include <string.h>
# include <time.h>
# include <errno.h>

# include "libhfsp.h"
# include "volume.h"
# include "record.h"
# include "btree.h"
# include "blockiter.h"
# include "os.h"
# include "swab.h"
# include "hfstime.h"
# include "partitions.h"

/*
 *  A replacement of the ffs C library function which does not exist
 *  in all C libraries.
 *
 *  ffs finds the first set bit in a four-byte integer
 */
static unsigned long
my_ffs( unsigned long i )
{
  register unsigned long j;

  if( i == 0 ) return( 0 );

  for( j = 1; j <= ( sizeof( unsigned long )*8 ); j++ )
    {
      if( (i>>(j-1))&1 ) return( j );
    }

  return( 0 );
}

#define ffs(_a_)    my_ffs(_a_)

/* Fill a given buffer with the given block in volume.
 */
int volume_readinbuf(volume * vol,void* buf, long block)
{  
    UInt16 blksize_bits;
    ASSERT( block < vol->maxblocks);
    
    blksize_bits = vol->blksize_bits;    
    // printf("Reading from %lx\n", block << blksize_bits);
    if (hfsp_os_seek(&vol->fd, block, blksize_bits) == block) 
	if (1 == hfsp_os_read(&vol->fd, buf, 1, blksize_bits))
	    return 0;
    return -1;
}

/* Write buffer to given block on medium.
 */
int volume_writetobuf(volume * vol,void* buf, long block)
{  
    UInt16 blksize_bits;
    ASSERT( block < vol->maxblocks);
    
    blksize_bits = vol->blksize_bits;    
    // printf("Writing to %lx\n", block << blksize_bits);
    if (hfsp_os_seek(&vol->fd, block, blksize_bits) == block) 
	if (1 == hfsp_os_write(&vol->fd, buf, 1, blksize_bits))
	    return 0;
    return -1;
}

/* read multiple blocks of a fork into given memory.
 *
 * block    realtive index in fork to start with
 * count    number of blocks to read
 * forktype usually HFSP_EXTENT_DATA or HFSP_EXTENT_RSRC  
 * fileId   id (needed) in case extents must be fetched
 * 
 * returns given pinter or NULL on failure.
 */
void* volume_readfromfork(volume* vol, void* buf, 
	hfsp_fork_raw* f, UInt32 block, 
	UInt32 count, UInt8 forktype, UInt32 fileId)
{
    blockiter iter;
    char*     cbuf = buf;

    blockiter_init(&iter, vol, f, forktype, fileId);
    if (blockiter_skip(&iter, block))
	return NULL;
    while (count > 0)
    {
	--count;
	if (volume_readinbuf(vol, cbuf, blockiter_curr(&iter)))
	    return NULL;
	cbuf += vol->blksize;
	if (count > 0 && blockiter_next(&iter)) 
	    return NULL;
    }
    return buf;
}

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
	UInt32 count, UInt8 forktype, UInt32 fileId)
{
    blockiter iter;
    char*     cbuf = buf;

    blockiter_init(&iter, vol, f, forktype, fileId);
    if (blockiter_skip(&iter, block))
	return -1;
    while (count > 0)
    {
	--count;
	if (volume_writetobuf(vol, cbuf, blockiter_curr(&iter)))
	    return -1;
	cbuf += vol->blksize;
	if (count > 0 && blockiter_next(&iter)) 
	    return -1;
    }
    return 0;
}

/* Check in Allocation file if given block is allocated.
 */
int volume_allocated(volume* vol, UInt32 block)
{
    int	    bit		= block & 0x07;
    int	    mask,index;
    char*   bits;
    char    buf[vol->blksize];
    
    // if (block >= vol->maxblocks)
    //	HFSP_ERROR(-1, "Allocation block out of range.");
    block   >>= 3;
    mask = (1 << vol->blksize_bits) -1;	/* Usually 0x0FFF */
    index = block & mask;
    block   >>= vol->blksize_bits;  // block in allocation file
    bits = (char*) volume_readfromfork(vol, buf, &vol->vol.alloc_file, 
	    block, 1, HFSP_EXTENT_DATA, HFSP_ALLOC_CNID);
    if (!bits)
	HFSP_ERROR(-1, "Allocation block not found !?");

    return (bits[index] & (0x80 >> bit)); /* Bit one is 0x80 ! */
    return 0;
fail:
    return -1;
}

/* Mark in Allocation file a given block as allocated.
 * 
 * ToDo: optimize for adjacent blocks ...
 *       use cache directly
 */
int volume_allocate(volume* vol, UInt32 block)
{
    int	    bit		= block & 0x07;
    int	    mask,index;
    char*   bits;
    char    buf[vol->blksize];
    int	    shift = 0x80 >> bit;    /* Bit one is 0x80 */
    
    // if (block >= vol->maxblocks)
    //	HFSP_ERROR(-1, "Allocation block out of range.");
    block   >>= 3;
    mask = (1 << vol->blksize_bits) -1;	/* Usually 0x0FFF */
    index = block & mask;
    block   >>= vol->blksize_bits;  // block in allocation file
    bits = (char*) volume_readfromfork(vol, buf, &vol->vol.alloc_file, 
	    block, 1, HFSP_EXTENT_DATA, HFSP_ALLOC_CNID);
    if (!bits)
	HFSP_ERROR(-1, "Allocation block not found !?");

    if (bits[index] & shift) 
	HFSP_ERROR(-1, "volume_allocate: Block already allocated");
    bits[index] |= shift;
    return volume_writetofork(vol, buf, &vol->vol.alloc_file, 
	    block, 1, HFSP_EXTENT_DATA, HFSP_ALLOC_CNID);
fail:
    return -1;
}

/* Mark in Allocation file a given block as freee.
 * 
 * ToDo: optimize for adjacent blocks ...
 *       use cache directly
 */
int volume_deallocate(volume* vol, UInt32 block)
{
    int	    bit		= block & 0x07;
    int	    mask,index;
    char*   bits;
    char    buf[vol->blksize];
    int	    shift = 0x80 >> bit;    /* Bit one is 0x80 */
    
    // if (block >= vol->maxblocks)
    //	HFSP_ERROR(-1, "Allocation block out of range.");
    block   >>= 3;
    mask = (1 << vol->blksize_bits) -1;	/* Usually 0x0FFF */
    index = block & mask;
    block   >>= vol->blksize_bits;  // block in allocation file
    bits = (char*) volume_readfromfork(vol, buf, &vol->vol.alloc_file, 
	    block, 1, HFSP_EXTENT_DATA, HFSP_ALLOC_CNID);
    if (!bits)
	HFSP_ERROR(-1, "Allocation block not found !?");

    if (!(bits[index] & shift)) 
	HFSP_ERROR(-1, "volume_allocate: Block already free");
    bits[index] &= ~shift;
    return volume_writetofork(vol, buf, &vol->vol.alloc_file, 
	    block, 1, HFSP_EXTENT_DATA, HFSP_ALLOC_CNID);
fail:
    return -1;
}

/* Initialize a raw hfsp_extent_rec.
 */
static void volume_initextent(hfsp_extent_rec er)
{
    memset(er, 0, 8 * sizeof(hfsp_extent));
    /*
    int		    i;
    hfsp_extent*    e;
    for (i=0; i < 8; i++)
    {
	e = &er[i];
	e->start_block = 0;
	e->block_count = 0;
    }
    */
}

/** Initalize an (empty !) fork, you may later request additional space
 */

void volume_initfork(volume* vol, hfsp_fork_raw* f, UInt16 fork_type)
{
    f->total_size   = 0;
    if (fork_type == HFSP_EXTENT_DATA)
	f->clump_size   = vol->vol.data_clump_sz;
    else
	f->clump_size   = vol->vol.rsrc_clump_sz;
    f->total_blocks = 0;
    volume_initextent(f->extents);
}

/* Read a raw hfsp_extent_rec from memory.
 * 
 * return pointer right after the structure.
 */
char* volume_readextent(char *p, hfsp_extent_rec er)
{
    int		    i;
    hfsp_extent*    e;
    for (i=0; i < 8; i++)
    {
	e = &er[i];
	e->start_block = bswabU32_inc(&p);
	e->block_count = bswabU32_inc(&p);
    }
    return p;
}

/* Write a raw hfsp_extent_rec to memory.
 * 
 * return pointer right after the structure.
 */
char* volume_writeextent(char *p, hfsp_extent_rec er)
{
    int		    i;
    hfsp_extent*    e;
    for (i=0; i < 8; i++)
    {
	e = &er[i];
	bstoreU32_inc(&p, e->start_block );
	bstoreU32_inc(&p, e->block_count );
    }
    return p;
}

/* Read a raw hfsp_fork from memory.
 * 
 * return pointer right after the structure.
 */
char* volume_readfork(char *p, hfsp_fork_raw* f)
{
    f->total_size   = bswabU64_inc(&p);
    f->clump_size   = bswabU32_inc(&p);
    f->total_blocks = bswabU32_inc(&p);
    return volume_readextent(p, f->extents);
}

/* Write a raw hfsp_fork to memory.
 * 
 * return pointer right after the structure.
 */
char* volume_writefork(char *p, hfsp_fork_raw* f)
{
    bstoreU64_inc(&p, f->total_size  );
    bstoreU32_inc(&p, f->clump_size  );
    bstoreU32_inc(&p, f->total_blocks);
    return volume_writeextent(p, f->extents);
}

/* Read the volume from the given buffer and swap the bytes.
 */
static int volume_readbuf(hfsp_vh* vh, char* p)
{
    if ( (vh->signature	= bswabU16_inc(&p)) != HFSP_VOLHEAD_SIG) 
	HFSP_ERROR(-1, "This is not a HFS+ volume");
    vh->version	    	= bswabU16_inc(&p);
    vh->attributes   	= bswabU32_inc(&p); 
    vh->last_mount_vers	= bswabU32_inc(&p); 
    vh->reserved	= bswabU32_inc(&p);
    vh->create_date	= bswabU32_inc(&p);
    vh->modify_date	= bswabU32_inc(&p);
    vh->backup_date	= bswabU32_inc(&p);
    vh->checked_date	= bswabU32_inc(&p);
    vh->file_count	= bswabU32_inc(&p);
    vh->folder_count	= bswabU32_inc(&p);
    vh->blocksize	= bswabU32_inc(&p);
    vh->total_blocks	= bswabU32_inc(&p);
    vh->free_blocks	= bswabU32_inc(&p);
    vh->next_alloc	= bswabU32_inc(&p);
    vh->rsrc_clump_sz	= bswabU32_inc(&p);
    vh->data_clump_sz	= bswabU32_inc(&p);
    vh->next_cnid	= bswabU32_inc(&p);
    vh->write_count	= bswabU32_inc(&p);
    vh->encodings_bmp	= bswabU64_inc(&p);
    memcpy(vh->finder_info, p, 32); 
    p += 32; // finderinfo is not used by now
    p = volume_readfork(p, &vh->alloc_file );
    p = volume_readfork(p, &vh->ext_file   );
    p = volume_readfork(p, &vh->cat_file   );
    p = volume_readfork(p, &vh->attr_file  );
    p = volume_readfork(p, &vh->start_file );
    return 0;
  fail:
    return -1;
}
	
/* Write the volume to the given buffer and swap the bytes.
 */
static int volume_writebuf(hfsp_vh* vh, char* p)
{
    bstoreU16_inc(&p, vh->signature	);
    bstoreU16_inc(&p, vh->version	);
    bstoreU32_inc(&p, vh->attributes   	); 
    bstoreU32_inc(&p, vh->last_mount_vers); 
    bstoreU32_inc(&p, vh->reserved	);
    bstoreU32_inc(&p, vh->create_date	);
    bstoreU32_inc(&p, vh->modify_date	);
    bstoreU32_inc(&p, vh->backup_date	);
    bstoreU32_inc(&p, vh->checked_date	);
    bstoreU32_inc(&p, vh->file_count	);
    bstoreU32_inc(&p, vh->folder_count	);
    bstoreU32_inc(&p, vh->blocksize	);
    bstoreU32_inc(&p, vh->total_blocks	);
    bstoreU32_inc(&p, vh->free_blocks	);
    bstoreU32_inc(&p, vh->next_alloc	);
    bstoreU32_inc(&p, vh->rsrc_clump_sz	);
    bstoreU32_inc(&p, vh->data_clump_sz	);
    bstoreU32_inc(&p, vh->next_cnid	);
    bstoreU32_inc(&p, vh->write_count	);
    bstoreU64_inc(&p, vh->encodings_bmp	);
    memcpy(p, vh->finder_info, 32); 
    p += 32; // finderinfo is not used by now
    p = volume_writefork(p, &vh->alloc_file );
    p = volume_writefork(p, &vh->ext_file   );
    p = volume_writefork(p, &vh->cat_file   );
    p = volume_writefork(p, &vh->attr_file  );
    p = volume_writefork(p, &vh->start_file );
    return 0;
}

/* Read the volume from the given block */
static int volume_read(volume * vol, hfsp_vh* vh, UInt32 block)
{
    char buf[vol->blksize];
    if (volume_readinbuf(vol, buf, block))
	return -1;
    return volume_readbuf(vh, buf);
}

/* Find out whether the volume is wrapped and unwrap it eventually */
static int volume_read_wrapper(volume * vol, hfsp_vh* vh)
{
    UInt16  signature;
    char    buf[vol->blksize];
    char    *p = buf;
    if( volume_readinbuf(vol, buf, 2) ) // Wrapper or volume header starts here
        return -1;

    signature	= bswabU16_inc(&p);
    if (signature == HFS_VOLHEAD_SIG)	/* Wrapper */
    {
	UInt32  drAlBlkSiz;		/* size (in bytes) of allocation blocks */
	UInt32	sect_per_block;		/* how may block build an hfs sector */
	UInt16  drAlBlSt;	        /* first allocation block in volume */
  
	UInt16	embeds, embedl;		/* Start/lenght of embedded area in blocks */
	
	p += 0x12;			/* skip unneeded HFS vol fields */
	drAlBlkSiz = bswabU32_inc(&p);	/* offset 0x14 */
	p += 0x4;			/* skip unneeded HFS vol fields */
	drAlBlSt    = bswabU16_inc(&p);	/* offset 0x1C */
	
	p += 0x5E;			/* skip unneeded HFS vol fields */
	signature = bswabU16_inc(&p);	/* offset 0x7C, drEmbedSigWord */
	if (signature != HFSP_VOLHEAD_SIG)
	    HFSP_ERROR(-1, "This looks like a normal HFS volume");
	embeds = bswabU16_inc(&p);
	embedl = bswabU16_inc(&p);
	sect_per_block =  (drAlBlkSiz / HFSP_BLOCKSZ);  
	// end is absolute (not relative to HFS+ start)
	vol->maxblocks	= embedl * sect_per_block;
	hfsp_set_offset(&vol->fd, hfsp_get_offset(&vol->fd)
		+ (((APPLEUInt64) (drAlBlSt + embeds * sect_per_block)) << HFS_BLOCKSZ_BITS));
	/* Now we can try to read the embedded HFS+ volume header */
	return volume_read(vol,vh,2);
    }
    else if (signature == HFSP_VOLHEAD_SIG) /* Native HFS+ volume */
    {
	p = buf; // Restore to begin of block
	return volume_readbuf(vh, p);
    } else
	 HFSP_ERROR(-1, "Neither Wrapper nor native HFS+ volume header found");
    
fail:
    return -1;
}

/** Mark the volume as modified by setting its modfied date */
void volume_modified(volume* vol)
{
    time_t	now;
    hfsp_vh*	head;
    
    gmtime(&now);
    head = &vol->vol;
    head->modify_date = now + HFSPTIMEDIFF;
}

/** Mark this volume as used by Linux by modifying the header */
void volume_linux_mark(volume* vol)
{
    hfsp_vh* head = &vol->vol;

    // *** Debugging ***
    vol ->flags		    |= HFSP_BACKUP_DIRTY;

    // MacOS does not like that :(
    // head->version	    = HPUTILS_VERS;
    head->last_mount_vers   = HPLS_SIGNATURE;
    // For now I always mark the volume as inconsistent ...
    head->attributes   |= HFSPLUS_VOL_INCNSTNT;
    volume_modified(vol);
}

#include <stdio.h>
/* Open the device, read and verify the volume header
   (and its backup) */
int volume_open(volume* vol, const void *devicehandle, int partition, int rw)
{
    hfsp_vh backup;    /* backup volume found at second to last block */
    int	    shift;
    int	    blksize_bits;

    vol->blksize_bits	= HFSP_BLOCKSZ_BITS;
    vol->flags		= 0;
    vol->blksize	= HFSP_BLOCKSZ;
    vol->maxblocks	= 3;
	 /* this should be enough until we find the volume descriptor */
    vol->extents	= NULL; /* Thanks to Jeremias Sauceda */

    btree_reset(&vol->catalog);

    if (hfsp_os_open(&vol->fd, devicehandle, rw))
	return -1;

    hfsp_set_offset(&vol->fd, 0);

    /* set the offset to the first block of the given partition */
    if (partition != 0) {
      partition_map map;
      int           block;
      if( partition_getPartitionMap( &map, vol->fd)== -1)
	 HFSP_ERROR(-1, "No Apple partition map found");
      block = partition_getStartBlock( &map, "Apple_HFS", partition);
      if (block == 0)
	 HFSP_ERROR(-1, "No valid Apple_HFS partition found");
      hfsp_set_offset(&vol->fd, ((APPLEUInt64)block) << HFSP_BLOCKSZ_BITS);
    } 

    vol->flags |= rw & HFSP_MODE_RDWR;

    if (volume_read_wrapper(vol, &vol->vol))
	return -1;

    if (volume_read(vol, &backup, vol->maxblocks - 2))
	/*return -1*/;	/* ignore lacking backup */
//    fprintf(stderr, "HFS+ has no backup volume header. skipping...\n");

    /* Now switch blksize from HFSP_BLOCKSZ (512) to value given in header
       and adjust depend values accordingly, after that a block always
       means a HFS+ allocation size */

    /* Usually blocksize is 4096 */
    blksize_bits = ffs(vol->vol.blocksize) -1;
    shift	 = blksize_bits - vol->blksize_bits;
    vol -> blksize	= vol->vol.blocksize;
    vol -> blksize_bits = blksize_bits;
    vol -> maxblocks    = vol->vol.total_blocks;	/* cant calculate via shift ? */

    if (vol->flags & HFSP_MODE_RDWR)
    {
	char buf[HFSP_BLOCKSZ];
	void *p = buf;

	volume_linux_mark(vol);

	// write back (dirty) volume header
	if (volume_writebuf(&vol->vol, p))
	    return -1;	// evil, but will never happen

	volume_writetobuf(vol, buf, 2);	    // This is always block 2
    }

    if (btree_init_cat(&vol->catalog, vol, &vol->vol.cat_file))
	return -1;

    return 0;
  fail:
    return -1;    
}

/* Write back all data eventually cached and close the device */
int volume_close(volume* vol)
{
    btree_close(&vol->catalog);
    if (vol->extents)
    {
	btree_close(vol->extents);
	FREE(vol->extents);
    }
    if (vol->flags & HFSP_MODE_RDWR) // volume was opened for writing, 
    {
	/* Switch back to HFSP_BLOCKSZ (512) */
	int shift = vol->blksize_bits - HFSP_BLOCKSZ_BITS;
	char buf[HFSP_BLOCKSZ];
	void *p = buf;

	hfsp_vh* head = &vol->vol;

	// Clear inconsistent flag
	head->attributes   &= ~HFSPLUS_VOL_INCNSTNT;
	// set Unmounted flag
	head->attributes   |= HFSPLUS_VOL_UNMNT;

	vol->blksize_bits = HFSP_BLOCKSZ_BITS;

	vol -> maxblocks   <<= shift;	/* cant calculate via shift ? */

	if (volume_writebuf(&vol->vol, p))
	    return -1;	// evil, but will never happen

	volume_writetobuf(vol, buf, 2);	    // This is always block 2

	if (vol->flags & HFSP_BACKUP_DIRTY) // need to write backup block, too
	{
	    // !!! Need to check this with larger volumes, too !!!
	    volume_writetobuf(vol, buf, vol->maxblocks-2);
	}
    }
    return hfsp_os_close(&vol->fd);
}

/* internal fucntion used to create the extents btree,
   is called by inline function when needed */
void volume_create_extents_tree(volume* vol)
{
    btree* result = (btree*) ALLOC(btree*, sizeof(btree));
    if (!result)
	HFSP_ERROR(ENOMEM, "No memory for extents btree");
    if (!btree_init_extent(result, vol, &vol->vol.ext_file))
    {
	vol->extents = result;
	return;
    }
  fail:
    vol->extents = NULL;    
}

/* return new Id for files/folder and check for overflow.
 *
 * retun 0 on error .
 */
UInt32 volume_get_nextid(volume* vol)
{
    UInt32 result = vol->vol.next_cnid;
    if (result < HFSP_MIN_CNID) // oops possible wrap around overflow
    {
	hfsp_error = "Maximum number of node IDs exhausted, sorry";
	return 0;
    }
    vol->vol.next_cnid = 1 + result;
    return result;
}
