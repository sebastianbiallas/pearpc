/*
 * hfsutils - tools for reading and writing Macintosh HFS volumes
 * Copyright (C) 2000 Klaus Halfmann <klaus.halfmann@t-online.de.de>
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

#ifndef APPLE_H
#include "apple.h"
#endif

typedef struct {
	int numparts;		/* number of partitions found */
	ApplePartition *parray;	/* only used for mem mngmnt */
	ApplePartition **partitions;	/* partition descriptions */
} partition_map;

/*
 * Returns the number of partitions in the given partition map or 0 if no partitions were found
 * @param map	Partition map to get the information from.
 */
int partition_getnparts( partition_map *map);

/*
 * Returns the startblock of the <num>th partition of the given type from the given
 * Partition Map.
 *
 * @param map	Partition map to get the information from
 * @param type	type of the desired partition
 * @param num	number of the desired partition (starting with 1)
 *
 * @return the start block of the partition or 0 if no such partition could be found
 */
UInt32 partition_getStartBlock( partition_map *map, const char *type, int num);

/*
 * Returns the partition map of the given device
 */
int partition_getPartitionMap( partition_map *map, void *fd);

/*
 * cleanup and free allocated memory.
 */
void partition_release( partition_map *map);
