/*
 * libhfsp - library for reading and writing Macintosh HFS+ volumes
 *
 * This file contains defintions that are special for Apple.
 * The names match the defintions found in Apple Header files.
 * 
 * Copyright (C) 2000 Klaus Halfmann <klaus.halfmann@feri.de>
 * Original code 1996-1998 by Robert Leslie <rob@mars.rog>
 * other work 2000 from Brad Boyer (flar@pants.nu) 
 * Additional work in 2004 by Stefan Weyergraf for use in PearPC
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
#define APPLE_H

#include "system/types.h"
typedef signed char	    Char;
typedef unsigned char	    UChar;
typedef sint8		SInt8;
typedef uint8		UInt8;
typedef sint16		SInt16;
typedef uint16		UInt16;
typedef sint32		SInt32;
typedef uint32		UInt32;
typedef uint32		OSType;
typedef uint64		APPLEUInt64;

#define PARTITION_SIG	    0x504d   /* 'PM' */

typedef struct {
	UInt16 pmSig;		/* partition signature: should be 'PM' */
	UInt16 pmSigPad;	/* reserved stuff */
	UInt32 pmMapBlkCnt;	/* number of blocks in partition map */
	UInt32 pmPyPartStart;	/* startblock of the partition */
	UInt32 pmPartBlkCnt;	/* number of blocks in partition */
	char pmPartName[ 32];	/* partition name */
	char pmPartType[ 32];	/* partition type */
	UInt32 pmLgDataStart;	/* first logical block of data area */
	UInt32 pmDataCnt;	/* number of blocks in data area */
	UInt32 pmPartStatus;	/* partition status information */
	UInt32 pmLgBootStart;	/* first logical block of boot code */
	UInt32 pmBootSize;	/* size of boot code, in bytes */
	UInt32 pmBootAddr;	/* boot code load address */
	UInt32 pmBootAddr2;	/* reserved */
	UInt32 pmBootEntry;	/* boot code entry point */
	UInt32 pmBootEntry2;	/* reserved */
	UInt32 pmBootCksum;	/* boot code checksum */
	char pmProcessor[ 16];	/* processor type */
	UInt16 pmPad[ 188];	/* reserved, sums up with the rest to 512 */
} ApplePartition;

/* A point, normally used by Quickdraw, 
 * but found in Finderinformation, too 
 */
typedef struct {
  SInt16	v;		/* vertical coordinate */
  SInt16	h;		/* horizontal coordinate */
} Point;

/* A rectancle, normally used by Quickdraw, 
 * but found in Finderinformation, too.
 */
typedef struct {
  SInt16	top;		/* top edge of rectangle */
  SInt16	left;		/* left edge */
  SInt16	bottom;		/* bottom edge */
  SInt16	right;		/* right edge */
} Rect;

/* Information about the location and size of a folder 
 * used by the Finder. 
 */
typedef struct {
  Rect		frRect;		/* folder's rectangle */
  SInt16	frFlags;	/* flags */
  Point		frLocation;	/* folder's location */
  SInt16	frView;		/* folder's view */
} DInfo;

/* Extended folder information used by the Finder ...
 */
typedef struct {
  Point		frScroll;	/* scroll position */
  SInt32	frOpenChain;	/* directory ID chain of open folders */
  SInt16	frUnused;	/* reserved */
  SInt16	frComment;	/* comment ID */
  SInt32	frPutAway;	/* directory ID */
} DXInfo;

/* Finder information for a File
 */
typedef struct {
  OSType	fdType;		/* file type */
  OSType	fdCreator;	/* file's creator */
  SInt16	fdFlags;	/* flags */
  Point		fdLocation;	/* file's location */
  SInt16	fdFldr;		/* file's window */
} FInfo;

/* Extendend Finder Information for a file
 */
typedef struct {
  SInt16	fdIconID;	/* icon ID */
  SInt16	fdUnused[4];	/* reserved */
  SInt16	fdComment;	/* comment ID */
  SInt32	fdPutAway;	/* home directory ID */
} FXInfo;

/* Flagvalues for FInfo and DInfo */
# define HFS_FNDR_ISONDESK              (1 <<  0)
# define HFS_FNDR_COLOR                 0x0e
# define HFS_FNDR_COLORRESERVED         (1 <<  4)
# define HFS_FNDR_REQUIRESSWITCHLAUNCH  (1 <<  5)
# define HFS_FNDR_ISSHARED              (1 <<  6)
# define HFS_FNDR_HASNOINITS            (1 <<  7)
# define HFS_FNDR_HASBEENINITED         (1 <<  8)
# define HFS_FNDR_RESERVED              (1 <<  9)
# define HFS_FNDR_HASCUSTOMICON         (1 << 10)
# define HFS_FNDR_ISSTATIONERY          (1 << 11)
# define HFS_FNDR_NAMELOCKED            (1 << 12)
# define HFS_FNDR_HASBUNDLE             (1 << 13)
# define HFS_FNDR_ISINVISIBLE           (1 << 14)
# define HFS_FNDR_ISALIAS               (1 << 15)

#endif
