/* 
 *	HT Editor
 *	stream.h
 *
 *	Copyright (C) 1999-2002 Stefan Weyergraf (stefan@weyergraf.de)
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

#ifndef __STREAM_H__
#define __STREAM_H__

#include <stdarg.h>
#include <stdio.h>

#include "data.h"
#include "str.h"
#include "system/file.h"

#if 0
// just an idea. but it probably sucks...
enum StreamEvent {
	SEV_NULL = 0,
	SEV_CONTENT_MODIFIED = 1		// ()
};

typedef uint StreamEventMask;

/**
 *	A stream event listener
 */
class StreamEventListener: public Object {
public:
	virtual	void handleEvent(StreamEvent aStreamEvent, va_list args) = 0;
};
#endif

class String;

/* Stream access mode */
enum IOAccessModeAtomic {
	IOAM_NULL = 0,
	IOAM_READ = 1,
	IOAM_WRITE = 2
};

typedef uint IOAccessMode;

/**
 *	A stream.
 */
class Stream {
private:
	IOAccessMode mAccessMode;
//	Container *mListeners;
protected:
		void			checkAccess(IOAccessMode mask);
//		void			notifyListeners(StreamEvent event,...);
public:
					Stream();
	virtual				~Stream();
	/* new */
//		void			addEventListener(StreamEventListener *l, StreamEvent mask);
	virtual	uint			copyAllTo(Stream *stream);
	virtual	uint			copyTo(Stream *stream, uint count);
	virtual	IOAccessMode		getAccessMode() const;
	virtual	String &		getDesc(String &result) const;
	virtual	uint			read(void *buf, uint size);
		void			readx(void *buf, uint size);
//		void			removeEventListener(StreamEventListener *l);
	virtual	int			setAccessMode(IOAccessMode mode);
	virtual	uint			write(const void *buf, uint size);
		void			writex(const void *buf, uint size);
};

/**
 *	A stream, layering a stream.
 */
class StreamLayer: public Stream {
protected:
	Stream *mStream;
	bool mOwnStream;

public:

					StreamLayer(Stream *stream, bool own_stream);
	virtual				~StreamLayer();
	/* extends Stream */
	virtual IOAccessMode		getAccessMode() const;
	virtual String &		getDesc(String &result) const;
	virtual uint			read(void *buf, uint size);
	virtual int			setAccessMode(IOAccessMode mode);
	virtual uint			write(const void *buf, uint size);
	/* new */
		Stream *		getLayered() const;
		void			setLayered(Stream *newLayered, bool ownNewLayered);
};

#define	OS_FMT_DEC		0
#define	OS_FMT_HEX		1

/**
 *	A stream-layer, storing/loading |Object|s.
 */
class ObjectStream: public StreamLayer {
public:
				ObjectStream(Stream *stream, bool own_stream);
	/* new */
	virtual void		getBinary(void *buf, uint size, const char *desc) = 0;
	virtual bool		getBool(const char *desc) = 0;
	virtual uint64		getInt(uint size, const char *desc) = 0;
	virtual void		getObject(Object *& object, const char *name, ObjectID id = OBJID_INVALID) = 0;
	virtual char *		getString(const char *desc) = 0;
	virtual byte *		getLenString(int &length, const char *desc) = 0;

	virtual void		putBinary(const void *mem, uint size, const char *desc) = 0;
	virtual void		putBool(bool b, const char *desc) = 0;
	virtual void		putComment(const char *comment) = 0;
	virtual void		putCommentf(const char *comment_format, ...);
	virtual void		putInt(uint64 i, uint size, const char *desc, uint int_fmt_hint = OS_FMT_DEC) = 0;
	virtual void		putObject(const Object *object, const char *name, ObjectID id = OBJID_INVALID) = 0;
	virtual void		putSeparator() = 0;
	virtual void		putString(const char *string, const char *desc) = 0;
	virtual void		putLenString(const byte *string, int length, const char *desc) = 0;
};

#define PUTX_BINARY(st, d, size, dstr)	(st).putBinary(d, size, dstr)
#define PUTX_BOOL(st, d, dstr)		(st).putBool(d, dstr)
#define PUTX_INT(st, d, size, dstr)	(st).putInt(d, size, dstr)
#define PUTX_INTD(st, d, size, dstr)	(st).putInt(d, size, dstr, OS_FMT_DEC)
#define PUTX_INTX(st, d, size, dstr)	(st).putInt(d, size, dstr, OS_FMT_HEX)
#define PUTX_INT32(st, d, dstr)		(st).putInt(d, 4, dstr)
#define PUTX_INT32D(st, d, dstr)	(st).putInt(d, 4, dstr, OS_FMT_DEC)
#define PUTX_INT32X(st, d, dstr)	(st).putInt(d, 4, dstr, OS_FMT_HEX)
#define PUTX_INT64(st, d, dstr)		(st).putInt(d, 8, dstr)
#define PUTX_INT64D(st, d, dstr)	(st).putInt(d, 8, dstr, OS_FMT_DEC)
#define PUTX_INT64X(st, d, dstr)	(st).putInt(d, 8, dstr, OS_FMT_HEX)
#define PUTX_OBJECT(st, d, dstr)	(st).putObject(d, dstr)
#define PUTX_STRING(st, d, dstr)	(st).putString(d, dstr)
#define PUTX_LSTRING(st, d, len, dstr)	(st).putLenString(d, len, dstr)

#define PUT_BINARY(st, d, size)		PUTX_BINARY(st, d, size, #d)
#define PUT_BOOL(st, d)			PUTX_BOOL(st, d, #d)
#define PUT_INT(st, d, size)		PUTX_INT(st, d, size, #d)
#define PUT_INTD(st, d, size)		PUTX_INTD(st, d, size, #d)
#define PUT_INTX(st, d, size)		PUTX_INTX(st, d, size, #d)
#define PUT_INT32(st, d)		PUTX_INT32(st, d, #d)
#define PUT_INT32D(st, d)		PUTX_INT32D(st, d, #d)
#define PUT_INT32X(st, d)		PUTX_INT32X(st, d, #d)
#define PUT_INT64(st, d)		PUTX_INT64(st, d, #d)
#define PUT_INT64D(st, d)		PUTX_INT64D(st, d, #d)
#define PUT_INT64X(st, d)		PUTX_INT64X(st, d, #d)
#define PUT_OBJECT(st, d)		PUTX_OBJECT(st, d, #d)
#define PUT_STRING(st, d)		PUTX_STRING(st, d, #d)
#define PUT_LSTRING(st, d, len)		PUTX_LSTRING(st, d, len, #d)

#define GETX_BINARY(st, d, size, dstr)	(st).getBinary(d, size, dstr)
#define GETX_BOOL(st, dstr)		(st).getBool(dstr)
#define GETX_INT(st, size, dstr)	(st).getInt(size, dstr)
#define GETX_INTD(st, size, dstr)       GETX_INT(st, size, dstr)
#define GETX_INTX(st, size, dstr)       GETX_INT(st, size, dstr)
#define GETX_INT32(st, dstr)		(st).getInt(4, dstr)
#define GETX_INT32D(st, dstr)		GETX_INT32(st, dstr)
#define GETX_INT32X(st, dstr)		GETX_INT32(st, dstr)
#define GETX_INT64(st, dstr)		(st).getInt(8, dstr)
#define GETX_INT64D(st, dstr)		GETX_INT64(st, dstr)
#define GETX_INT64X(st, dstr)		GETX_INT64(st, dstr)
#define GETX_STRING(st, dstr)		(st).getString(dstr)
#define GETX_LSTRING(st, size, dstr)	(st).getLenString(size, dstr)
#define GETX_OBJECT(st, d, dstr)	(st).getObject((Object*&)((void*&)d), dstr)

#define GET_BINARY(st, d, size)		GETX_BINARY(st, d, size, #d)
#define GET_BOOL(st, d)			d=GETX_BOOL(st, #d)
#define GET_INT(st, d, size)		d=GETX_INT(st, size, #d)
#define GET_INTD(st, d, size)		d=GET_INT(st, d, size)
#define GET_INTX(st, d, size)		d=GET_INT(st, d, size)
#define GET_INT32(st, d)		d=GETX_INT32(st, #d)
#define GET_INT32D(st, d)		d=GETX_INT32D(st, #d)
#define GET_INT32X(st, d)		d=GETX_INT32X(st, #d)
#define GET_INT64(st, d)		d=GETX_INT64(st, #d)
#define GET_INT64D(st, d)		d=GETX_INT64D(st, #d)
#define GET_INT64X(st, d)		d=GETX_INT64X(st, #d)
#define GET_STRING(st, d)		d=GETX_STRING(st, #d)
#define GET_LSTRING(st, d, size)	d=GETX_LSTRING(st, size, #d)
#define GET_OBJECT(st, d)		GETX_OBJECT(st, d, #d)

/**
 *	A object-stream-layer.
 */
class ObjectStreamLayer: public ObjectStream {
protected:
	ObjectStream	*mObjStream;
public:
				ObjectStreamLayer(ObjectStream *aObjStream, bool own_ostream);
	/* extends ObjectStream */
	virtual void		getBinary(void *buf, uint size, const char *desc);
	virtual bool		getBool(const char *desc);
	virtual uint64		getInt(uint size, const char *desc);
	virtual void		getObject(Object *& object, const char *name, ObjectID id = OBJID_INVALID);
	virtual char *		getString(const char *desc);
	virtual byte *		getLenString(int &length, const char *desc);

	virtual void		putBinary(const void *mem, uint size, const char *desc);
	virtual void		putBool(bool b, const char *desc);
	virtual void		putComment(const char *comment);
	virtual void		putInt(uint64 i, uint size, const char *desc, uint int_fmt_hint = OS_FMT_DEC);
	virtual void		putObject(const Object *object, const char *name, ObjectID id = OBJID_INVALID);
	virtual void		putSeparator();
	virtual void		putString(const char *string, const char *desc);
	virtual void		putLenString(const byte *string, int length, const char *desc);
};

/* cntl cmd */
#define FCNTL_MODS_INVD			0x00000001
#define FCNTL_MODS_FLUSH		0x00000002
#define FCNTL_MODS_IS_DIRTY		0x00000003	// const FileOfs &offset, const FileOfs &range, bool &isdirty
//#define FCNTL_MODS_CLEAR_DIRTY		0x00000004
//#define FCNTL_MODS_CLEAR_DIRTY_RANGE	0x00000005	// const FileOfs &offset, const FileOfs &range
#define FCNTL_FLUSH_STAT		0x00000006
#define FCNTL_GET_RELOC			0x00000007	// bool &enabled
#define FCNTL_SET_RELOC			0x00000008	// bool enable
#define FCNTL_GET_FD			0x00000009	// int &fd

// Return a "modification count" that changes everytime the file state ( content, size, pstat )
// changes.
// While identical mod-counts imply identical file states,
// different mod-counts do not necessarily imply different file states !
#define FCNTL_GET_MOD_COUNT		0x0000000a	// int &mcount

#define IS_DIRTY_SINGLEBIT		0x80000000

/* File open mode */
enum FileOpenMode {
	FOM_EXISTS,
	FOM_CREATE,
	FOM_APPEND
};

/**
 *	A file.
 */
class File: public Stream {
protected:
	int mcount;
public:
		File();
	/* new */
		int			cntl(uint cmd, ...);
	virtual void			del(uint size);
	virtual void			extend(FileOfs newsize);
	virtual String &		getFilename(String &result) const;
	virtual FileOfs			getSize() const;
	virtual void			insert(const void *buf, uint size);
	virtual void			pstat(pstat_t &s) const;
	virtual void			seek(FileOfs offset);
	virtual FileOfs			tell() const;
	virtual void			truncate(FileOfs newsize);
	virtual int			vcntl(uint cmd, va_list vargs);
};

/**
 *	A file, layering a file.
 */
class FileLayer: public File {
protected:
	File *mFile;
	bool mOwnFile;
public:
					FileLayer(File *file, bool own_file);
	virtual 			~FileLayer();
	/* extends File */
	virtual void			del(uint size);
	virtual void			extend(FileOfs newsize);
	virtual IOAccessMode		getAccessMode() const;
	virtual String &		getDesc(String &result) const;
	virtual String &		getFilename(String &result) const;
	virtual FileOfs			getSize() const;
	virtual void			insert(const void *buf, uint size);
	virtual void			pstat(pstat_t &s) const;
	virtual uint			read(void *buf, uint size);
	virtual void			seek(FileOfs offset);
	virtual int			setAccessMode(IOAccessMode mode);
	virtual FileOfs			tell() const;
	virtual void			truncate(FileOfs newsize);
	virtual int			vcntl(uint cmd, va_list vargs);
	virtual uint			write(const void *buf, uint size);
	/* new */
		File *			getLayered() const;
		void			setLayered(File *newLayered, bool ownNewLayered);
};

/**
 *	A local file (file descriptor [fd]).
 */
class LocalFileFD: public File {
protected:
	String		mFilename;
	FileOpenMode	mOpenMode;

	int fd;
	bool own_fd;

	FileOfs offset;

		int		setAccessModeInternal(IOAccessMode mode);
public:

				LocalFileFD(const String &aFilename, IOAccessMode mode, FileOpenMode aOpenMode);
				LocalFileFD(int fd, bool own_fd, IOAccessMode mode);
	virtual 		~LocalFileFD();
	/* extends File */
	virtual String &	getDesc(String &result) const;
	virtual String &	getFilename(String &result) const;
	virtual FileOfs		getSize() const;
	virtual uint		read(void *buf, uint size);
	virtual void		seek(FileOfs offset);
	virtual int		setAccessMode(IOAccessMode mode);
	virtual FileOfs 	tell() const;
	virtual void		truncate(FileOfs newsize);
	virtual int		vcntl(uint cmd, va_list vargs);
	virtual uint		write(const void *buf, uint size);
};

/**
 *	A local file (file stream [FILE*]).
 */
class LocalFile: public File {
protected:
	String		mFilename;
	FileOpenMode	mOpenMode;

	FILE *		file;
	bool		own_file;

	FileOfs		offset;

		int		setAccessModeInternal(IOAccessMode mode);
public:
				LocalFile(const String &aFilename, IOAccessMode mode=IOAM_READ, FileOpenMode aOpenMode=FOM_EXISTS);
				LocalFile(FILE *file, bool own_file, IOAccessMode mode);
	virtual			~LocalFile();
	/* extends File */
	virtual String &	getDesc(String &result) const;
	virtual String &	getFilename(String &result) const;
	virtual FileOfs		getSize() const;
	virtual void		pstat(pstat_t &s) const;
	virtual uint		read(void *buf, uint size);
	virtual void		seek(FileOfs offset);
	virtual int		setAccessMode(IOAccessMode mode);
	virtual FileOfs 	tell() const;
	virtual void		truncate(FileOfs newsize);
	virtual int		vcntl(uint cmd, va_list vargs);
	virtual uint		write(const void *buf, uint size);
};

/**
 *	A temporary file.
 */
class TempFile: public LocalFile {
public:
				TempFile(IOAccessMode mode);
	/* extends File */
	virtual String &	getDesc(String &result) const;
	virtual void		pstat(pstat_t &s) const;
};

/**
 *	A fixed-size, read-only file, mapping a area of memory.
 */
class ConstMemMapFile: public File {
protected:
	FileOfs pos;
	uint size;
	const void *buf;
public:
				ConstMemMapFile(const void *buf, uint size);
	/* extends File */
	virtual String &	getDesc(String &result) const;
	virtual FileOfs	getSize() const;
	virtual uint		read(void *buf, uint size);
	virtual void		seek(FileOfs offset);
	virtual FileOfs 	tell() const;
};

/**
 *	A fixed-size file, mapping a area of memory.
 */
class MemMapFile: public ConstMemMapFile {
public:
				MemMapFile(void *buf, uint size);
	/* extends Stream */
	virtual uint		write(const void *buf, uint size);
};

/**
 *	A file layer, representing a cropped version of a file
 */
class CroppedFile: public FileLayer {
protected:
	FileOfs mCropStart;
	bool	mHasCropSize;
	FileOfs mCropSize;
public:
				// crop [start; start+size-1]
				CroppedFile(File *file, bool own_file, FileOfs aCropStart, FileOfs aCropSize);
				// no size, just start
				CroppedFile(File *file, bool own_file, FileOfs aCropStart);
	/* extends FileLayer */
	virtual void		extend(FileOfs newsize);
	virtual String &	getDesc(String &result) const;
	virtual FileOfs	getSize() const;
	virtual void		pstat(pstat_t &s) const;
	virtual uint		read(void *buf, uint size);
	virtual void		seek(FileOfs offset);
	virtual FileOfs 	tell() const;
	virtual void		truncate(FileOfs newsize);
	virtual uint		write(const void *buf, uint size);
};

/**
 *	A (read-only) file with zero-content.
 */
class NullFile: public File {
public:
				NullFile();
	/* extends File */
	virtual void		extend(FileOfs newsize);
	virtual String &	getDesc(String &result) const;
	virtual FileOfs	getSize() const;
	virtual void		pstat(pstat_t &s) const;
	virtual uint		read(void *buf, uint size);
	virtual void		seek(FileOfs offset);
	virtual int		setAccessMode(IOAccessMode mode);
	virtual FileOfs 	tell() const;
	virtual void		truncate(FileOfs newsize);
	virtual uint		write(const void *buf, uint size);
};

/**
 *	A file, existing only in memory.
 */
class MemoryFile: public File {
protected:
	FileOfs ofs;
	FileOfs pos;
	uint bufsize, dsize, ibufsize;
	byte *buf;

	virtual	uint		extendBufSize(uint bufsize);
	virtual	uint		shrinkBufSize(uint bufsize);
		void		extendBuf();
		void		shrinkBuf();
		void		resizeBuf(uint newsize);
public:
				MemoryFile(FileOfs ofs = 0, uint size = 0, IOAccessMode mode = IOAM_READ | IOAM_WRITE);
	virtual 		~MemoryFile();
	/* extends File */
	virtual void		extend(FileOfs newsize);
	virtual IOAccessMode	getAccessMode() const;
	virtual String &	getDesc(String &result) const;
	virtual FileOfs		getSize() const;
	virtual void		pstat(pstat_t &s) const;
	virtual uint		read(void *buf, uint size);
	virtual void		seek(FileOfs offset);
	virtual int		setAccessMode(IOAccessMode mode);
	virtual FileOfs 	tell() const;
	virtual void		truncate(FileOfs newsize);
	virtual uint		write(const void *buf, uint size);
	/* new */
		byte *		getBufPtr() const;
};

void fileMove(File *file, FileOfs src, FileOfs dest, FileOfs size);

/** read string from file (zero-terminated, 8-bit chars) */
char *fgetstrz(File *file);
/** read string from stream (zero-terminated, 8-bit chars) */
char *getstrz(Stream *stream);
/** write string into stream (zero-terminated, 8-bit chars) */
void putstrz(Stream *stream, const char *str);

/** read string from stream (8-bit length followed by content aka. Pascal-style) */
char *getstrp(Stream *stream);
/** write string into stream (8-bit length followed by content aka. Pascal-style) */
void putstrp(Stream *stream, const char *str);

/** read string from stream (zero-terminated, 16-bit chars) */
char *getstrw(Stream *stream);
/** write string into stream (zero-terminated, 16-bit chars) */
void putstrw(Stream *stream, const char *str);

#endif /* __STREAM_H__ */
