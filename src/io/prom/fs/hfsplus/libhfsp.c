/*
 * libhfsp - library for reading and writing Macintosh HFS+ volumes
 *
 * Copyright (C) 2000-2001 Klaus Halfmann <klaus.halfmann@t-online.de>
 * Original 1996-1998 Robert Leslie
 *
 * Thi file contains utitlity functions to manage the features of
 * the hfs+ library.
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
 * $Id: libhfsp.c,v 1.2 2004/05/11 16:11:12 steveman Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include "libhfsp.h"

const char *hfsp_error = "no error";       /* static error string */   

/** helper function to create those Apple 4 byte Signatures */
UInt32 sig(char c0, char c1, char c2, char c3)
{
    UInt32 sig;
    ((char*)&sig)[0] = c0;
    ((char*)&sig)[1] = c1;
    ((char*)&sig)[2] = c2;
    ((char*)&sig)[3] = c3;
    return sig;
}
