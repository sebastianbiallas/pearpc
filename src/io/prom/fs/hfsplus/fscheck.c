/*
 * libhfs - library for reading and writing Macintosh HFS volumes
 *
 * This code checks the structures of a HFS+ volume for correctnes.
 *
 * ToDo: care come about HPFSCHK_IGNOREERR
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
 * $Id: fscheck.c,v 1.1 2004/05/05 22:45:57 seppel Exp $
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
# include "fscheck.h"
# include "volume.h"
# include "btree.h"
# include "record.h"
# include "hfstime.h"
# include "unicode.h"
# include "os.h"
# include "swab.h"

/* Dump all raw fork information to stdout */
void print_fork(hfsp_fork_raw* f)
{
    int		    i;
    hfsp_extent*    e;
    printf("total_size          : %#LX\n"  , f->total_size);
    printf("clump_size          : %#X\n"  , f->clump_size);
    printf("total_blocks        : %#X\n"  , f->total_blocks);
    printf("extents             : ");
    for (i=0; i < 8; i++)
    {
	e = &f->extents[i];
	printf("(%#X+%#X) " , e->start_block,e->block_count);
    }
    printf("\n");
}

/** helper function to print those Apple 4 byte Signatures */
static inline void print_sig(UInt32 sig)
{
    printf("%c%c%c%c" , 
	((char*)&sig)[0], ((char*)&sig)[1], 
	((char*)&sig)[2], ((char*)&sig)[3]);
}

    /* Dump all the volume information to stdout */
void volume_print(hfsp_vh* vh)
{
    printf("signature       : %c%c\n" , ((char*)&vh->signature)[0], 
					((char*)&vh->signature)[1]);
    printf("version         : %u\n"	  , vh->version);
    printf("attributes      : %#X\n"	  , vh->attributes);
    printf("last_mount_vers : "); print_sig(vh->last_mount_vers);
    printf("\nreserved        : %u\n"	  , vh->reserved);
	/* Hmm this is in local, apple time ... */
    printf("create_date     : %s"	  , get_atime(vh->create_date));
    printf("modify_date     : %s"	  , get_atime(vh->modify_date));
    printf("backup_date     : %s"	  , get_atime(vh->backup_date));
    printf("checked_date    : %s"	  , get_atime(vh->checked_date));
    printf("file_count      : %u\n"	  , vh->file_count);
    printf("folder_count    : %u\n"	  , vh->folder_count);
    printf("blocksize       : %X\n"	  , vh->blocksize);
    printf("total_blocks    : %u\n"	  , vh->total_blocks);
    printf("free_blocks     : %u\n"	  , vh->free_blocks);
    printf("next_alloc      : %u\n"	  , vh->next_alloc);
    printf("rsrc_clump_sz   : %u\n"	  , vh->rsrc_clump_sz);
    printf("data_clump_sz   : %u\n"	  , vh->data_clump_sz);
    printf("next_cnid       : %u\n"	  , vh->next_cnid);
    printf("write_count     : %u\n"	  , vh->write_count);
    printf("encodings_bmp   : %#LX\n"	  , vh->encodings_bmp);
    /* vv->finder_info, p, 32); */
    printf("                  Allocation file\n");
    print_fork(&vh->alloc_file);
    printf("                  Extension file\n");
    print_fork(&vh->ext_file);
    printf("                  Catalog file\n");
    print_fork(&vh->cat_file);
    printf("                  Attribute file\n");
    print_fork(&vh->attr_file);
    printf("                  Start file\n");
    print_fork(&vh->start_file);
}

/* Check all fields of the volume header.
 */
static int fscheck_volume_header(volume * vol, hfsp_vh* vh)
{
    UInt32  attributes = vh->attributes;
    int	    result = 0;
    // vh->signature // already checked in read
    // vh->version	// Current is 4 but I wont check that
    if (attributes & HFSPLUS_VOL_RESERVED1)
	printf("Reserved attribute in use: %X\n", 
			    vh->attributes & HFSPLUS_VOL_RESERVED1);
    if (! (attributes & HFSPLUS_VOL_UNMNT))
	printf("Volume was not cleanly unmounted\n");
    if (fsck_data.verbose && (attributes & HFSPLUS_VOL_SPARE_BLK))
	printf("Volume has spare (bad) blocks\n");
    if (fsck_data.verbose && (attributes & HFSPLUS_VOL_NOCACHE))
	printf("Volume should not be cached (ignored)\n");
    if (attributes & HFSPLUS_VOL_INCNSTNT)
	printf("Volume is inconsistent\n");
    if (attributes & HFSPLUS_VOL_RESERVED2)
	printf("Reserved attribute in use: %X\n", 
			    vh->attributes & HFSPLUS_VOL_RESERVED2);
    if (fsck_data.verbose && (attributes & HFSPLUS_VOL_SOFTLOCK))
	printf("Volume is soft locked");
    if (attributes & HFSPLUS_VOL_RESERVED3)
	printf("Reserved attribute in use: %X\n", 
			    vh->attributes & HFSPLUS_VOL_RESERVED3);
    switch (vh->last_mount_vers)
    {
	case HPAPPLE_SIGNATURE:
	    if (fsck_data.verbose)
	    {
		printf("Volume was last Mounted by Apple:\n");
		print_sig(vh->last_mount_vers);
	    }
	    break;
	case HPLS_SIGNATURE:
	    if (fsck_data.verbose)
	    {
		printf("Volume was last Mounted by hfsplusutils: \n");
		print_sig(vh->last_mount_vers);
	    }
	    break;
	case HPLS_SIGRES1:
	    if (fsck_data.verbose)
	    {
		printf("Volume was last Mounted by hfsplus kernel module: \n");
		print_sig(vh->last_mount_vers);
	    }
	    break;
	default:
	    printf("Volume was last Mounted by unknnown implemenatation: \n");
	    print_sig(vh->last_mount_vers);
    }
    // vh->reserved	// not checked
    // vh->file_count	// To be checked later
    // vh->folder_count	// To be checked later
    if (0 != (vh->blocksize % HFSP_BLOCKSZ)) // must be multiple of BLKSZ
    {
	printf("Invalid Blocksize %X\n", vh->blocksize);
	result = FSCK_ERR; // Wont try to correct that, yet.
    }
    {
	APPLEUInt64 totalbytes    = vh->total_blocks * vh->blocksize;
	APPLEUInt64 expectedbytes = vol->maxblocks << vol->blksize_bits;
	if (totalbytes > expectedbytes)
	    printf("\nInvalid total blocks %X, expected %X", 
		vh->total_blocks, (UInt32)(expectedbytes / vh->blocksize));
    }
    if (vh->free_blocks	> vh->total_blocks)
	printf("More free blocks (%X) than total (%X) ?\n", 
		    vh->free_blocks, vh->total_blocks);
	// Check more later
    // vh->next_alloc	// to be checked later
    // vh->rsrc_clump_sz  // no check needed, is a hint only
    // vh->data_clump_sz  // no check needed, is a hint only
    if (vh->next_cnid <= HFSP_MIN_CNID) // wil hopefully be fixed later
	printf("Invalid next_cnid: %d\n", vh->next_cnid);
	// Check more later
    // vh->write_count	    // no check possible
    // vh->encodings_bmp    // no check needed, is a hint only
    // vh->finder_info	    // not checked (why should I?)
    // vh->alloc_file	    // to be checked later
    // vh->ext_file   	    // to be checked later
    // vh->cat_file   	    // to be checked later
    // vh->attr_file  	    // to be checked later
    // vh->start_file 	    // to be checked later
    return result;
}
	
/* Read the volume from the given buffer and swap the bytes.
 */
static int fscheck_volume_readbuf(volume * vol, hfsp_vh* vh, void* p)
{
    if ( (vh->signature	= bswabU16_inc(p)) != HFSP_VOLHEAD_SIG) 
    {
	printf("Unexpected Volume signature '%2s' expected 'H+'\n",
		(char*) &vh->signature);
	HFSP_ERROR(-1, "This is not a HFS+ volume");
    }
    vh->version	    	= bswabU16_inc(p);
    vh->attributes   	= bswabU32_inc(p); 
    vh->last_mount_vers	= bswabU32_inc(p); 
    vh->reserved	= bswabU32_inc(p);
    vh->create_date	= bswabU32_inc(p);
    vh->modify_date	= bswabU32_inc(p);
    vh->backup_date	= bswabU32_inc(p);
    vh->checked_date	= bswabU32_inc(p);
    vh->file_count	= bswabU32_inc(p);
    vh->folder_count	= bswabU32_inc(p);
    vh->blocksize	= bswabU32_inc(p);
    vh->total_blocks	= bswabU32_inc(p);
    vh->free_blocks	= bswabU32_inc(p);
    vh->next_alloc	= bswabU32_inc(p);
    vh->rsrc_clump_sz	= bswabU32_inc(p);
    vh->data_clump_sz	= bswabU32_inc(p);
    vh->next_cnid	= bswabU32_inc(p);
    vh->write_count	= bswabU32_inc(p);
    vh->encodings_bmp	= bswabU64_inc(p);
    memcpy(vh->finder_info, p, 32); 
    ((char*) p) += 32; // So finderinfo must be swapped later, ***
    p = volume_readfork(p, &vh->alloc_file );
    p = volume_readfork(p, &vh->ext_file   );
    p = volume_readfork(p, &vh->cat_file   );
    p = volume_readfork(p, &vh->attr_file  );
    p = volume_readfork(p, &vh->start_file );

    if (fsck_data.verbose)
	volume_print(vh);

    return 0;
  fail:
    return -1;
}
	
/* Read the volume from the given block */
static int fscheck_volume_read(volume * vol, hfsp_vh* vh, UInt32 block)
{
    char buf[vol->blksize];
    if (volume_readinbuf(vol, buf, block))
	return -1;
    return fscheck_volume_readbuf(vol, vh, buf);
}


/* Find out wether the volume is wrapped and unwrap it eventually */
static int fscheck_read_wrapper(volume * vol, hfsp_vh* vh)
{
#if 0
    UInt16  signature;
    char    buf[vol->blksize];
    void    *p = buf;
    if( volume_readinbuf(vol, buf, 2) ) // Wrapper or volume header starts here
        return -1;

    signature	= bswabU16_inc(p);
    if (signature == HFS_VOLHEAD_SIG)	/* Wrapper */
    {
	UInt32  drAlBlkSiz;		/* size (in bytes) of allocation blocks */
	UInt32	sect_per_block;		/* how may block build an hfs sector */
	UInt16  drAlBlSt;	        /* first allocation block in volume */
  
	UInt16	embeds, embedl;		/* Start/lenght of embedded area in blocks */
	
	if (fsck_data.verbose)
	    printf("Volume is wrapped in HFS volume "
		   " (use hfsck to check this)\n");

	((char*) p) += 0x12;		/* skip unneded HFS vol fields */
	drAlBlkSiz = bswabU32_inc(p);	/* offset 0x14 */
	((char*) p) += 0x4;		/* skip unneded HFS vol fields */
	drAlBlSt    = bswabU16_inc(p);	/* offset 0x1C */
	
	((char*) p) += 0x5E;		/* skip unneded HFS vol fields */
	signature = bswabU16_inc(p);	/* offset 0x7C, drEmbedSigWord */
	if (signature != HFSP_VOLHEAD_SIG)
	    HFSP_ERROR(-1, "This looks like a normal HFS volume");
	embeds = bswabU16_inc(p);
	embedl = bswabU16_inc(p);
	sect_per_block =  (drAlBlkSiz / HFSP_BLOCKSZ);  
	if ((sect_per_block * HFSP_BLOCKSZ) != drAlBlkSiz)
	{
	    printf("HFS Blocksize %X is not multiple of %X\n", 
		    drAlBlkSiz, HFSP_BLOCKSZ);
	    return FSCK_ERR; // Cant help it (for now)
	}
	// end is absolute (not relative to HFS+ start)
	vol->maxblocks	= embedl * sect_per_block;
	os_offset = ((APPLEUInt64) (drAlBlSt + embeds * sect_per_block))
		    << HFS_BLOCKSZ_BITS;
	/* Now we can try to read the embedded HFS+ volume header */
	if (fsck_data.verbose)
	    printf("Embedded HFS+ volume at 0x%LX (0x%X) of 0x%X sized Blocks\n",
		    os_offset, vol->maxblocks, HFSP_BLOCKSZ);
	return fscheck_volume_read(vol,vh,2);
    }
    else if (signature == HFSP_VOLHEAD_SIG) /* Native HFS+ volume */
    {
	if (fsck_data.verbose)
	    printf("This HFS+ volume is not wrapped.\n");
	p = buf; // Restore to begin of block
	return fscheck_volume_readbuf(vol, vh, p);
    } else
	 HFSP_ERROR(-1, "Neither Wrapper nor native HFS+ volume header found");
    
fail:
    return FSCK_ERR;
#endif
    return FSCK_ERR;
}

/* Check wether all blocks of a fork are marked as allocated.
 *
 * returns number of errors found.
 */

int check_forkalloc(volume* vol, hfsp_fork_raw* fork)
{
    int		    i;
    hfsp_extent* e;
    UInt32	    block, count;
    int		    errcount = 0;

    for (i=0; i < 8; i++)
    {
	e = &fork->extents[i];
	block = e->start_block;
	count = e->block_count;
	while (count > 0)
	{
	    if (!volume_allocated(vol, block))
	    {
		printf("Warning block %X not marked as allocated\n",block);
		errcount++;
	    }
	    count --;
	    block ++;
	}
    }	    // Hope we can correct that some time
    return errcount > 0 ? FSCK_FSCORR : FSCK_NOERR;
}

/* Check allocation of the volume (part1).
   The raw blocks in the voume header are checked. */

static int check_alloc1(volume* vol)
{
    hfsp_vh*	vh	= &vol->vol;
    int		result	= 0;
    result |= check_forkalloc(vol, &vh->alloc_file);
    result |= check_forkalloc(vol, &vh->ext_file);
    result |= check_forkalloc(vol, &vh->cat_file);
    result |= check_forkalloc(vol, &vh->attr_file);
    result |= check_forkalloc(vol, &vh->start_file);

    return result;
}

/* internal function used to create the extents btree */
static int fscheck_create_extents_tree(volume* vol)
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

/* Open the device, read and verify the volume header, 
 * check the extents and catalog b-tree. */
int fscheck_volume_open(volume* vol, char* devname, int rw)
{
    hfsp_vh backup;    /* backup volume found at second to last block */
    long    sect_per_block;
    int	    shift;
    int	    result = 0;
   
    vol->blksize_bits	= HFSP_BLOCKSZ_BITS;
    vol->blksize	= HFSP_BLOCKSZ;
    vol->extents	= NULL; /* Thanks to Jeremias Sauceda */
    /* vol->maxblocks	= os_seek(&vol->fd, -1, HFSP_BLOCKSZ_BITS); */
    /* This wont work for /dev/... but we do not really need it */
    vol->maxblocks	= 3;
    btree_reset(&vol->catalog);
    
    if (hfsp_os_open(&vol->fd, devname, rw))
	return FSCK_ERR | result;

    printf("*** Checking Volume Header:\n");
    if (fscheck_read_wrapper(vol, &vol->vol))
	return FSCK_ERR | result;
    result |= fscheck_volume_header(vol, &vol->vol);
    printf("\t\t\t\t\t\tDone ***\n");
    printf("*** Checking Backup Volume Header:\n");
    if (fscheck_volume_read(vol, &backup, vol->maxblocks - 2))
	return FSCK_ERR | result;
    result |= fscheck_volume_header(vol, &backup);
    printf("\t\t\t\t\t\tDone ***\n");

    if (result & FSCK_FATAL)
    {
	printf("\t\t\t\t*** Check stopped ***\n");
	return result;	// Further checking impossible
    }

    /* Now switch blksize from HFSP_BLOCKSZ (512) to value given in header
       and adjust depend values accordingly, after that a block always
       means a HFS+ allocation size */

	/* Usually	    4096	/ 512  == 8  */
    sect_per_block = vol->vol.blocksize / HFSP_BLOCKSZ;
    shift = 0;
    if (sect_per_block > 1)
    {
	shift = 1;
	while (sect_per_block > 2)
	{
	    sect_per_block >>=1;
	    shift++;
	}	/* shift = 3 */
    }
    vol -> blksize_bits += shift;  
    vol -> blksize	= 1 << vol->blksize_bits;
    vol -> maxblocks     = vol->vol.total_blocks;	/* cant calculate via shift ? */
    
    result |= check_alloc1(vol); // This should not be fatal ..
   
    // For fschek the extents tree is created first 
    printf("*** Checking Extents Btree:\n");
    result |= fscheck_create_extents_tree(vol);
    if ( !(result & FSCK_FATAL))
	result |= fscheck_btree(vol->extents);
    if ((result & FSCK_FATAL) && !fsck_data.ignoreErr)
    {
	printf("\t\t\t\t*** Check stopped ***\n");
	return result;	// Further checking impossible
    }
    printf("\t\t\t\t\t\tDone ***\n");

    printf("*** Checking Catalog Btree:\n");
    result |= fscheck_init_cat(&vol->catalog, vol, &vol->vol.cat_file);
    if ( !(result & FSCK_FATAL) || !fsck_data.ignoreErr)
	result |= fscheck_btree(&vol->catalog);
    if ((result & FSCK_FATAL) && !fsck_data.ignoreErr)
    {
	printf("\t\t\t\t*** Check stopped ***\n");
	return result;	// Further checking impossible
    }
    printf("\t\t\t\t\t\tDone ***\n");

    return result;
}

/* Initialize fsck_data to some default values */
static void fsck_init()
{
    fsck_data.maxCnid	= 0;
    fsck_data.verbose	= 0;
    fsck_data.ignoreErr = 0;
}

/* Do the minimal check required after an unclean mount.
 *
 * The volume should be mounted readonly.
 *
 * returns the highest cnid found. 0 denotes an error.
 * In case of error the volume should not be used at all.
 */
UInt32 minimal_check(volume* vol)
{
    fsck_init();    // minimal check is not verbose

    // An additional check for allocation wont hurt
    if (fscheck_files(vol))
	fsck_data.maxCnid = 0;

    return fsck_data.maxCnid;
}

/* Do usefull checks, practice will tell what this is.
 *
 * returns the highest cnid found. 0 denotes an error.
 * In case of error the volume may still be useable.
 */
int hfsplus_check(char* device, int flags)
{
    volume  vol;
    int	    result;

    fsck_init();
    fsck_data.verbose	= 0 != (flags & HFSPCHECK_VERBOSE);
    fsck_data.ignoreErr = 0 != (flags & HFSPCHECK_IGNOREERR);
    fsck_data.macNow	= HFSPTIMEDIFF + time(NULL);
    
    result = fscheck_volume_open(&vol, device, HFSP_MODE_RDONLY);
    if (! (result & FSCK_FATAL))
    {
	printf("*** Checking files and directories in catalog:\n");
	result |= fscheck_files(&vol);
    }
    volume_close(&vol);

    return result;
}

/* Do every check that can be imagined (or more :) 
 *
 * returns the highest cnid found. 0 denotes an error.
 * In case of error the volume may still be useable.
 */
int maximum_check(char* device, int flags)
{
    volume  vol;
    int	    result;

    fsck_init();
    fsck_data.verbose = 0 != (flags & HFSPCHECK_VERBOSE);
    fsck_data.macNow  = HFSPTIMEDIFF + time(NULL);
    
    result = fscheck_volume_open(&vol, device, HFSP_MODE_RDONLY);
    if (! (result & FSCK_FATAL))
    {
	printf("*** Checking files and directories in catalog:\n");
	result |= fscheck_files(&vol);
    }
    volume_close(&vol);

    return result;
}
