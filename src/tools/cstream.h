/* 
 *	HT Editor
 *	cstream.cc
 *
 *	Copyright (C) 1999-2002 Sebastian Biallas (sb@web-productions.de)
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
#ifndef __CSTREAM_H__
#define __CSTREAM_H__

#include "stream.h"

/*
 *	NEVER use CompressedStream for both reading and writing!
 */
 
#define COMPRESSED_STREAM_DEFAULT_GRANULARITY 10240

class CompressedStream: public StreamLayer {
protected:
	byte *buffer;
	uint buffersize;
	uint bufferpos;

			bool flush_compressed();
			bool flush_uncompressed();
public:
			CompressedStream(Stream *stream, bool own_stream, uint granularity = COMPRESSED_STREAM_DEFAULT_GRANULARITY);
	virtual		~CompressedStream();
/* extends StreamLayer */
	virtual	uint	read(void *buf, uint size);
	virtual	uint	write(const void *buf, uint size);
};

#endif
