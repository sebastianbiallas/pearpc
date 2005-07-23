/*
 * hfsutils - tools for reading and writing Macintosh HFS volumes
 * Copyright (C) 2000 Klaus Halfmann <klaus.halfmann@t-online.de>
 *
 * Structures and Functions for accessing the mac partition map
 * Copyright (C) 2002 Michael Schulze <mike.s@genion.de>
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

#include "swab.h"
#include "partitions.h"
#include "os.h"
#include "libhfsp.h"

#include <stdlib.h>
#include <string.h>

/*
 * Returns the number of partitions in the given partition map.
 * probably we don't need this since we have map->numparts
 */
int partition_getnparts( partition_map *map) {
  return map->partitions[ 0]->pmMapBlkCnt;
}

/*
 * Copies <length> bytes from the beginning of source to dest.
 * source will be incrementet to the byte after length.
 */
static int splitCharArray( char **source, char *dest, int length) {
  memcpy( dest, *source, length);
  dest[ length- 1]= 0;	    /* force last byte to null */

  (*source)+= length;

  return 0;
}

int partition_fillstruct( ApplePartition *p, char *buf) {
  p->pmSig	    = bswabU16_inc(&buf);
  p->pmSigPad	    = bswabU16_inc(&buf);
  p->pmMapBlkCnt    = bswabU32_inc(&buf);
  p->pmPyPartStart  = bswabU32_inc(&buf);
  p->pmPartBlkCnt   = bswabU32_inc(&buf);
  splitCharArray( &buf, p->pmPartName, 32);
  splitCharArray( &buf, p->pmPartType, 32);
  p->pmLgDataStart  = bswabU32_inc(&buf);
  p->pmDataCnt	    = bswabU32_inc(&buf);
  p->pmPartStatus   = bswabU32_inc(&buf);
  p->pmLgBootStart  = bswabU32_inc(&buf);
  p->pmBootSize	    = bswabU32_inc(&buf);
  p->pmBootAddr	    = bswabU32_inc(&buf);
  p->pmBootAddr2    = bswabU32_inc(&buf);
  p->pmBootEntry    = bswabU32_inc(&buf);
  p->pmBootEntry2   = bswabU32_inc(&buf);
  p->pmBootCksum    = bswabU32_inc(&buf);

  return 0;
}

/* sort the partitions according to their occurance on disc
 * hasi: we'd better use qsort instead of reinventing the wheel ....*/
void partition_sort( partition_map *map) {
  
  ApplePartition **partitions = map->partitions;
  ApplePartition *min;
  int i, j, numparts= map->numparts;

  for( i= 0; i < numparts; i++) {
    for( j= i+ 1; j< numparts; j++) {
      if( partitions[ j]->pmPyPartStart< partitions[ i]->pmPyPartStart) {
        min= partitions[ j];
        partitions[ j]= partitions[ i];
        partitions[ i]= min;
      }
    }
  }
}

/*
 * Returns the partition map of the given device
 *
 * @param fd filedescripator (see os.h) must already be opened.
 */
int partition_getPartitionMap( partition_map *map, void *fd) {
  char buf[ HFSP_BLOCKSZ];
  ApplePartition first; /* we use that to get the number of partitions in the map */
  int i, numparts;

  if( hfsp_os_seek( &fd, 1, HFSP_BLOCKSZ_BITS)!= 1)
  	return -1;

  /* read the first partition info from the map */
  if( hfsp_os_read( &fd, buf, 1, HFSP_BLOCKSZ_BITS)!= 1)
    return -1;
  if( partition_fillstruct( &first, buf))
    return -1;

  /* check if this is a partition map */
  if( first.pmSig!= PARTITION_SIG) {
    map->numparts= 0;
    return 0;
  }

  /* set the number of partitions and allocate memory */
  map->numparts   = numparts = first.pmMapBlkCnt;
  map->parray	  = ( ApplePartition *)malloc( numparts* sizeof( ApplePartition));
  map->partitions = ( ApplePartition **)malloc( numparts* sizeof( void *));

  /* copy the first partition info to map */
  memcpy( map->parray, &first, sizeof( ApplePartition));
  map->partitions[ 0]= map->parray;

  for( i= 1; i < numparts; i++) {
    if( ( hfsp_os_read( &fd, buf, 1, HFSP_BLOCKSZ_BITS)!= 1) ||
        ( partition_fillstruct( &( map->parray[ i]), buf))) {
      free( map->partitions);
      free( map->parray);
      map->numparts = 0; // so partition_release() will work correctly
      return -1;
    }

    map->partitions[ i]= &( map->parray[ i]);
  }

//  partition_sort( map);

  return numparts;
}


/*
 *	Works like you would expect just by looking at the signature. It's not
 *	necessary to consult this comment.
 *
 *						-- Sebastian
 */
#include <stdio.h>
UInt32 partition_getStartBlock(partition_map *map, const char *type, int num)
{
	ApplePartition **partitions = map->partitions;
	int i;
	for (i=0; i < map->numparts; i++) {
		printf("getStartBlock: %d %s\n", i, partitions[num]->pmPartType);
	}
	if (num >= map->numparts) return 0;
	if (strcmp(partitions[num]->pmPartType, type)) return 0;
	return partitions[num]->pmPyPartStart;
}

/*
 * Returns the startblock of the <num>th partition of the given type from the given
 * Partition Map.
 * @param map Partition map to get the information from
 * @param type type of the desired partition
 * @param num number of the desired partition (starting with 1)
 * @return the start block of the partition or 0 if no such partition could be found
 */
/*UInt32 partition_getStartBlock(partition_map *map, const char *type, int num)
{  
  int	    i, startblock   = 0;
  ApplePartition **partitions    = map->partitions;

  for (i= 0; i< map->numparts && num> 0; i++) {
    if (!strcmp(partitions[i]->pmPartType, type)) {
      startblock= partitions[i]->pmPyPartStart;
      num--;
    }
  }

  return num ? 0 : startblock;
}*/

/*
 * cleanup and free allocated memory.
 */
void partition_release(partition_map *map) 
{
  if (map->numparts > 0) {
    free(map->partitions);
    free(map->parray);
    map->numparts = 0;
  }
}

