/*
 *	HT Editor
 *	fileofs.h
 *
 *	Copyright (C) 1999-2003 Sebastian Biallas (sb@biallas.net)
 *	Copyright (C) 1999-2002 Stefan Weyergraf
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

#ifndef __FILEOFS_H__
#define __FILEOFS_H__

typedef uint64 FileOfs; /// A file offset

#define PUT_FILEOFS(st, d) (st).putQWordHex(d, 8, #d)
#define GET_FILEOFS(st, d) d=(st).getQwordHex(8, #d)

#endif /* __FILEOFS_H__ */

