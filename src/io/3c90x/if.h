/*
 *	PearPC
 *	if.h
 *
 *	code taken from Mac-on-Linux 0.9.68:
 *	Copyright (C) 1999-2002 Samuel Rydh (samuel@ibrium.se)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __IF_H__
#define __IF_H__

#include "system/types.h"

typedef struct {
	struct packet_driver *pd;

	int		fd;
	int		packet_pad;		/* #bytes before the eth packet */

	byte		eth_addr[6];
	char		drv_name[26];
	char		iface_name[16];

//	ulong		ipfilter;		/* sheep private, for save/load state only */
} enet_iface_t;

typedef struct packet_driver {
	char		*name;
	int		packet_driver_id;	/* f_xxx */

	int		(*open)			(enet_iface_t *is, char *intf_name, int *sigio_capable, byte mac[6]);
	void		(*close)		(enet_iface_t *is );
	int		(*add_multicast)	(enet_iface_t *is, char *addr );
	int		(*del_multicast)	(enet_iface_t *is, char *addr );
	int		(*load_save_state)	(enet_iface_t *is, enet_iface_t *load_is, int index, int loading );
} packet_driver_t;

#endif /* __IF_H__ */
