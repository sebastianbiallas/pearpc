/*
 * libhfs - library for reading and writing Macintosh HFS volumes.
 *
 * This code checks the structures of a HFS+ volume for correctnes
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
 * $Id: fscheck.h,v 1.1 2004/05/05 22:45:58 seppel Exp $
 */

/* Bitflags for check operations */

/* Show normal output only  */
#define HFSPCHECK_NORMAL    0x0000

/* Try to fix any error autmatically (but not yet ...) */
#define HFSPCHECK_AUTO	    0x0001

/* verbose, show more output about almost everything */
#define HFSPCHECK_VERBOSE    0x0002

/* ignore errros (if possible) default stop after first error */
#define HFSPCHECK_IGNOREERR  0x0004

/* On return this variable is set to a 
 * combination of the flags below */
extern int hfspcheck_error;

/* Id like to include those but found no definitions */
#define FSCK_NOERR  0    // No errors
#define FSCK_FSCORR 1    // File system errors corrected
#define FSCK_REBOOT 2    // System should be rebooted
#define FSCK_ERR    4    // File system errors left uncorrected
#define FSCK_ERROPR 8    // Operational error
#define FSCK_USAGE  16   // Usage or syntax error
#define FSCK_SHARED 128  // Shared library error
#define FSCK_FATAL  (FSCK_ERR | FSCK_ERROPR | FSCK_USAGE | FSCK_SHARED)
	// Will not continue checking when one of these is found

/* Do the minimal check required after an unclean mount.
 *
 * The volume should be mounted readonly.
 *
 * returns the highest cnid found. 0 denotes an error.
 * In case of error the volume should not be used at all.
 */
extern UInt32 minimal_check(volume* vol);

/* Do every check that can be imagined (or more :) 
 *
 * returns the highest cnid found. 0 denotes an error.
 * In case of error the volume may still be useable.
 */
extern int maximum_check(char* path, int flags);

/* Do usefull checks, practice will tell what this is.
 *
 * returns the highest cnid found. 0 denotes an error.
 * In case of error the volume may still be useable.
 */
extern int hfsplus_check(char* path, int flags);

/* Dump all raw fork information to stdout */
extern void print_fork(hfsp_fork_raw* f);

/* Check wether all blocks of a fork are marked as allocated.
 *
 * returns number of errors found.
 */
extern int check_forkalloc(volume* vol, hfsp_fork_raw* fork);

/**** Defined in btreecheck.c ****/

/** Intialize catalog btree */
int fscheck_init_cat(btree* bt, volume* vol, hfsp_fork_raw* fork);

/** Intialize catalog btree */
int fscheck_init_extent(btree* bt, volume* vol, hfsp_fork_raw* fork);
   
/** Check a complete btree by traversing it in-oder */
int fscheck_btree(btree *bt);

/** Check all files in leaf nodes */
int fscheck_files(volume* vol);

/** global data used during fsck */
struct {
    UInt32  maxCnid;
    UInt32  macNow; // current date in mac-offset
    int	    verbose;
    int	    ignoreErr;
} fsck_data;

