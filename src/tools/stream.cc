/*
 *	HT Editor
 *	stream.cc
 *
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

#include <cerrno>
#include <fcntl.h>
#include <limits.h>
#include <new>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>	/* for mode definitions */
#include <sys/types.h>	/* for mode definitions */
#include <unistd.h>

#include "debug.h"
#include "except.h"
#include "system/sys.h"
#include "snprintf.h"
#include "stream.h"

/*
 *	Listener
 */
#if 0
class Listener: public Object {
public:
	StreamEventListener *listener;
	StreamEvent notify_mask;

	Listener(StreamEventListener *l, StreamEvent nmask)
	{
		listener = l;
		notify_mask = nmask;
	}

/* extends Object */
	virtual int compareTo(const Object *obj) const
	{
		return ((Listener*)obj)->listener == listener ? 0 : 1;
	}
};
#endif
/*
 *	Stream
 */
#define STREAM_COPYBUF_SIZE	(64*1024)

Stream::Stream()
{
	mAccessMode = IOAM_NULL;
//	mListeners = NULL;
}

Stream::~Stream()
{
//	if (mListeners) delete mListeners;
}
/*
void Stream::addEventListener(StreamEventListener *l, StreamEvent mask)
{
	if (!mListeners) mListeners = new Array(true);
	*mListeners += new Listener(l, mask);
}
*/
void Stream::checkAccess(IOAccessMode mask)
{
	if (mAccessMode & mask != mask) throw IOException(EACCES);
}

/**
 *	Copy (whole) stream to another (i.e. copy until EOF)
 *	@param stream Stream to copy this Stream to
 *	@returns number of bytes copied
 */
uint Stream::copyAllTo(Stream *stream)
{
	byte *buf = new byte[STREAM_COPYBUF_SIZE];
	uint result = 0;
	uint r, t;
	do {
		uint k = STREAM_COPYBUF_SIZE;
		r = read(buf, k);
		ASSERT(r <= k);
		t = stream->write(buf, r);
		ASSERT(t <= r);
		result += t;
		if (t != k) break;
	} while (t);
	delete[] buf;
	return result;
}

/**
 *	Copy (partial) stream to another
 *	@param stream Stream to copy this Stream to
 *	@param count maximum number of bytes to copy
 *	@returns number of bytes copied
 */
uint Stream::copyTo(Stream *stream, uint count)
{
	byte *buf = new byte[STREAM_COPYBUF_SIZE];
	uint result = 0;
	while (count) {
		uint k = STREAM_COPYBUF_SIZE;
		if (k > count) k = count;
		uint r = read(buf, k);
		ASSERT(r <= k);
		uint t = stream->write(buf, r);
		ASSERT(t <= r);
		count -= t;
		result += t;
		if (t != k) break;
	}
	delete[] buf;
	return result;
}

/**
 *	Get access-mode
 */
IOAccessMode Stream::getAccessMode() const
{
	return mAccessMode;
}

/**
 *	Get descriptive name
 */
String &Stream::getDesc(String &result) const
{
	result = "";
	return result;
}
/*
void Stream::notifyListeners(StreamEvent event,...)
{
	if (!mListeners) return;
	foreach(Listener, l, *mListeners,
		if (l->notify_mask & event) {
			va_list ap;
			va_start(ap, event);
			l->listener->handleEvent(event, ap);
			va_end(ap);
		}
	);
}
*/
/**
 *	Set access-mode
 */
int Stream::setAccessMode(IOAccessMode mode)
{
	mAccessMode = mode;
	return 0;
}

/**
 *	Read from stream.
 *	Read up to <i>size</i> bytes from stream into <i>buf</i>.
 *	If less than <i>size</i> bytes are read, the exact number is
 *	returned and a (temporary) end-of-file (EOF) is encountered.
 *
 *	@param buf pointer to bytes to read
 *	@param size number of bytes to read
 *	@returns number of bytes read
 *	@throws IOException
 */
uint Stream::read(void *buf, uint size)
{
	return 0;
}

/**
 *	Exact read from stream.
 *	Read exactly <i>size</i> bytes from stream into <i>buf</i>.
 *	If less than <i>size</i> bytes are read, IOException is thrown.
 *
 *	@param buf pointer to bytes to read
 *	@param size number of bytes to read
 *	@throws IOException
 */
//#include "snprintf.h" 
void Stream::readx(void *buf, uint size)
{
//	File *f = dynamic_cast<File*>(this);
//	FileOfs t = f ? f->tell() : mkfofs(0);
	if (read(buf, size) != size) {
//		FileOfs sz = f ? f->getSize() : mkfofs(0);
//		ht_printf("readx failed, ofs = 0x%qx, size = %d (file size 0x%qx)\n", t, size, sz);
		throw IOException(EIO);
	}	    
}
/*
void Stream::removeEventListener(StreamEventListener *l)
{
	Listener t(l, SEV_NULL);
	bool b = (*mListeners -= &t);
	ASSERT(b);
}
*/
/**
 *	Write to stream.
 *	Write up to <i>size</i> bytes from <i>buf</i> into stream.
 *	If less than <i>size</i> bytes are written, the exact number is
 *	returned and a (temporary) end-of-file (EOF) is encountered.
 *
 *	@param buf pointer to bytes to write
 *	@param size number of bytes to write
 *	@returns number of bytes written
 *	@throws IOException
 */
uint Stream::write(const void *buf, uint size)
{
	return 0;
}

/**
 *	Exact write to stream.
 *	Write exactly <i>size</i> bytes from <i>buf</i> into stream.
 *	If less than <i>size</i> bytes are written, IOException is thrown.
 *
 *	@param buf pointer to bytes to write
 *	@param size number of bytes to write
 *	@throws IOException
 */
void Stream::writex(const void *buf, uint size)
{
	if (write(buf, size) != size) throw IOException(EIO);
}

/*
 *   StreamLayer
 */

StreamLayer::StreamLayer(Stream *s, bool own) : Stream()
{
	mStream = s;
	mOwnStream = own;
}

StreamLayer::~StreamLayer()
{
	if (mOwnStream) delete mStream;
}

IOAccessMode StreamLayer::getAccessMode() const
{
	return mStream->getAccessMode();
}

String &StreamLayer::getDesc(String &result) const
{
	return mStream->getDesc(result);
}

int StreamLayer::setAccessMode(IOAccessMode mode)
{
	return mStream->setAccessMode(mode);
}

uint StreamLayer::read(void *buf, uint size)
{
	return mStream->read(buf, size);
}

uint StreamLayer::write(const void *buf, uint size)
{
	return mStream->write(buf, size);
}

Stream *StreamLayer::getLayered() const
{
	return mStream;
}

void StreamLayer::setLayered(Stream *newLayered, bool ownNewLayered)
{
	mStream = newLayered;
	mOwnStream = ownNewLayered;
}

/*
 *	A File
 */

File::File()
{
	mcount = 0;
}

#define FILE_TRANSFER_BUFSIZE 4*1024
/**
 *	Low-level control function.
 *	@param cmd file control command number
 *	@returns 0 on success, POSIX.1 I/O error code on error
 */
int File::cntl(uint cmd, ...)
{
	va_list vargs;
	va_start(vargs, cmd);
	int ret = vcntl(cmd, vargs);
	va_end(vargs);
	return ret;
}

void fileMove(File *file, FileOfs src, FileOfs dest, FileOfs size)
{
	if (dest < src) {
		char tbuf[FILE_TRANSFER_BUFSIZE];
		while (size != 0) {
			FileOfs k = size;
			if (k > sizeof tbuf) k = sizeof tbuf;
			file->seek(src);
			file->readx(tbuf, k);
			file->seek(dest);
			file->writex(tbuf, k);
			src += k;
			dest += k;
			size -= k;
		}
	} else if (dest > src) {
		src += size;
		dest += size;
		char tbuf[FILE_TRANSFER_BUFSIZE];
		while (size != 0) {
			FileOfs k = size;
			if (k > sizeof tbuf) k = sizeof tbuf;
			src -= k;
			dest -= k;
			file->seek(src);
			file->readx(tbuf, k);
			file->seek(dest);
			file->writex(tbuf, k);
			size -= k;
		}
	}
}

/**
 *	Delete bytes from file.
 *	Delete <i>size</i> bytes at current file pointer and shorten file
 *	accordingly.
 Does not modify the current file pointer.
 *
 *	@param size number of bytes to delete
 *	@throws IOException
 */
void File::del(uint size)
{
	FileOfs t = tell();
	FileOfs o = t+size;
	if (o > getSize()) throw IOException(EINVAL);
	FileOfs s = getSize()-o;
	fileMove(this, o, t, s);
	truncate(getSize()-size);
	seek(t);
}

/**
 *	Extend file.
 *	Extend file to new size <i>newsize</i>.
 *	The current file pointer is undefined (but valid) after this operation.
 *
 *	@param newsize new extended file size
 *	@throws IOException
 */
void File::extend(FileOfs newsize)
{
	if (getSize() > newsize) throw IOException(EINVAL);
	if (getSize() == newsize) return;

	FileOfs save_ofs = tell();
	int e = 0;

	IOAccessMode oldmode = getAccessMode();
	if (!(oldmode & IOAM_WRITE)) {
		int f = setAccessMode(oldmode | IOAM_WRITE);
		if (f) throw IOException(f);
	}

	FileOfs s = getSize();
	char buf[FILE_TRANSFER_BUFSIZE];
	memset(buf, 0, sizeof buf);
	newsize -= s;
	seek(s);
	while (newsize != 0) {
		uint k = MIN(sizeof buf, newsize);
		uint l = write(buf, k);
		if (l != k) {
			e = EIO;
			break;
		}
		newsize -= l;
	}

	if (!(oldmode & IOAM_WRITE)) {
		int f = setAccessMode(oldmode);
		if (f) e = f;
	}
	if (e) throw IOException(e);
	seek(save_ofs);
}

/**
 *	Get filename.
 *
 *	@param result String that receives the filename
 *	@returns its argument
 */
String &File::getFilename(String &result) const
{
	result = "";
	return result;
}

/**
 *	Get file size.
 *
 *	@returns file size
 */
FileOfs File::getSize() const
{
	return 0;
}

#define FILE_INSERT_BUFSIZE 4*1024
/**
 *	Insert bytes into file.
 *	Insert <i>size</i> bytes from <i>buf</i> at the current file pointer
 *	into the file and extend file accordingly.
 *
 *	@param buf pointer to buffer that holds at least <i>size</i> bytes
 *	@param size number of bytes to insert
 *	@throws IOException
 */
void File::insert(const void *buf, uint size)
{
	FileOfs t = tell();
	FileOfs s = getSize()-t;
	extend(getSize()+size);
	fileMove(this, t, t+size, s);
	seek(t);
	writex(buf, size);
}

/**
 *	Get file status in a portable way.
 *	@param s structure that receives the file status
 */
void File::pstat(pstat_t &s) const
{
	s.caps = 0;
}

/**
 *	Set current file pointer.
 *	@param offset new value for current file pointer
 */
void File::seek(FileOfs offset)
{
	throw NotImplementedException(HERE);
}

/**
 *	Get current file pointer.
 *	@returns current file pointer
 */
FileOfs File::tell() const
{
	return 0;
}

/**
 *	Truncate file.
 *	Truncate file to new size <i>newsize</i>.
 *	The current file pointer is undefined (but valid) after this operation.
 *
 *	@param newsize new truncated file size
 *	@throws IOException
 */
void File::truncate(FileOfs newsize)
{
	if (getSize() < newsize) throw IOException(EINVAL);
	if (getSize() == newsize) return;

	throw NotImplementedException(HERE);
}

/**
 *	Vararg wrapper for cntl()
 */
int File::vcntl(uint cmd, va_list vargs)
{
	switch (cmd) {
		case FCNTL_GET_MOD_COUNT: {	// int &mcount
			int *mc = va_arg(vargs, int *);
			*mc = mcount;
			return 0;
		}
	}
	return ENOSYS;
}

/*
 *	FileLayer
 */
FileLayer::FileLayer(File *f, bool own_f) : File()
{
	mFile = f;
	mOwnFile = own_f;
}

FileLayer::~FileLayer()
{
	if (mOwnFile) delete mFile;
}

void FileLayer::del(uint size)
{
	return mFile->del(size);
}

void FileLayer::extend(FileOfs newsize)
{
	return mFile->extend(newsize);
}

IOAccessMode FileLayer::getAccessMode() const
{
	return mFile->getAccessMode();
}

String &FileLayer::getDesc(String &result) const
{
	return mFile->getDesc(result);
}

String &FileLayer::getFilename(String &result) const
{
	return mFile->getFilename(result);
}

FileOfs FileLayer::getSize() const
{
	return mFile->getSize();
}

void FileLayer::insert(const void *buf, uint size)
{
	return mFile->insert(buf, size);
}

void FileLayer::pstat(pstat_t &s) const
{
	return mFile->pstat(s);
}

uint FileLayer::read(void *buf, uint size)
{
	return mFile->read(buf, size);
}

void FileLayer::seek(FileOfs offset)
{
	return mFile->seek(offset);
}

int FileLayer::setAccessMode(IOAccessMode mode)
{
	return mFile->setAccessMode(mode);
}

File *FileLayer::getLayered() const
{
	return mFile;
}

void FileLayer::setLayered(File *newLayered, bool ownNewLayered)
{
	mFile = newLayered;
	mOwnFile = ownNewLayered;
}

FileOfs FileLayer::tell() const
{
	return mFile->tell();
}

void FileLayer::truncate(FileOfs newsize)
{
	return mFile->truncate(newsize);
}

int FileLayer::vcntl(uint cmd, va_list vargs)
{
	return mFile->vcntl(cmd, vargs);
}

uint FileLayer::write(const void *buf, uint size)
{
	return mFile->write(buf, size);
}

#if 0
/*
 *	LocalFileFD
 */

/**
 *	create open file
 */
LocalFileFD::LocalFileFD(const String &aFilename, IOAccessMode am, FileOpenMode om)
 : File(), mFilename(aFilename)
{
	mOpenMode = om;
	fd = -1;
	own_fd = false;
	int e = setAccessMode(am);
	if (e) throw IOException(e);
	mOpenMode = FOM_EXISTS;
}

/**
 *	map a file descriptor [fd]
 */
LocalFileFD::LocalFileFD(int f, bool own_f, IOAccessMode am)
 : File()
{
	mFilename = NULL;
	fd = f;
	own_fd = own_f;
	offset = 0;
	int e = File::setAccessMode(am);
	if (e) throw IOException(e);
}

LocalFileFD::~LocalFileFD()
{
	if (own_fd && (fd>=0)) ::close(fd);
}

String &LocalFileFD::getDesc(String &result) const
{
	result = mFilename;
	return result;
}

String &LocalFileFD::getFilename(String &result) const
{
	result = mFilename;
	return result;
}

FileOfs LocalFileFD::getSize() const
{
	uint t = tell();
	uint r = ::lseek(fd, 0, SEEK_END);
	lseek(fd, t, SEEK_SET);
	return r;
}

uint LocalFileFD::read(void *buf, uint size)
{
	if (!(getAccessMode() & IOAM_READ)) throw IOException(EACCES);
	errno = 0;
	uint r = ::read(fd, buf, size);
	int e = errno;
	if (e) {
		off_t r = ::lseek(fd, 0, SEEK_SET);
		offset = r;
		if (e != EAGAIN) throw IOException(e);
		return 0;
	} else {
		offset += r;
		return r;
	}		
}

void LocalFileFD::seek(FileOfs o)
{
	if (o == offset) return;
	off_t r = ::lseek(fd, o, SEEK_SET);
	offset = r;
	if (offset != o) throw IOException(EIO);
}

int LocalFileFD::setAccessMode(IOAccessMode am)
{
	IOAccessMode orig_access_mode = getAccessMode();
	int e = setAccessModeInternal(am);
	if (e && setAccessModeInternal(orig_access_mode))
		throw IOException(e);
	return e;
}

int LocalFileFD::setAccessModeInternal(IOAccessMode am)
{
//RETRY:
	if (getAccessMode() == am) return 0;
	if (fd >= 0) {
		// must own fd to change its access mode cause we can't
		// reopen a fd. right ?
		if (!own_fd) throw NotImplementedException(HERE);
		// FIXME: race condition here, how to reopen a fd atomically ?
		close(fd);
		fd = -1;
	}
	File::setAccessMode(IOAM_NULL);

	int mode = 0;

	if ((am & IOAM_READ) && (am & IOAM_WRITE)) mode = O_RDWR;
	else if (am & IOAM_READ) mode = O_RDONLY;
	else if (am & IOAM_WRITE) mode = O_WRONLY;

//     mode |= O_BINARY;

	switch (mOpenMode) {
		case FOM_APPEND:
			mode |= O_APPEND;
			break;
		case FOM_CREATE:
			mode |= O_CREAT | O_TRUNC;
			break;
		case FOM_EXISTS:
			;
	}

	int e = 0;
	if (am != IOAM_NULL) {
		pstat_t s;
		fd = ::open(mFilename, mode);
		if (fd < 0) e = errno;
		if (!e) {
			own_fd = true;
			e = sys_pstat_fd(s, fd);
			if (!e) {
				if (HT_S_ISDIR(s.mode)) {
					e = EISDIR;
				} else if (!HT_S_ISREG(s.mode) && !HT_S_ISBLK(s.mode)) {
					e = EINVAL;
				}
			}
		}
	}
	return e ? e : File::setAccessMode(am);
}

FileOfs LocalFileFD::tell() const
{
	return offset;
}

void LocalFileFD::truncate(FileOfs newsize)
{
	errno = 0;
	int e = sys_truncate_fd(fd, newsize);
	if (errno) e = errno;
	if (e) throw IOException(e);
}

int LocalFileFD::vcntl(uint cmd, va_list vargs)
{
	switch (cmd) {
		case FCNTL_FLUSH_STAT: {
			IOAccessMode m = getAccessMode();
			int e, f;
			e = setAccessMode(IOAM_NULL);
			f = setAccessMode(m);
			return e ? e : f;
		}
		case FCNTL_GET_FD: {	// (int &fd)
			int *pfd = va_arg(vargs, int*);
			*pfd = fd;
			return 0;
		}
	}
	return File::vcntl(cmd, vargs);
}

uint LocalFileFD::write(const void *buf, uint size)
{
	if (!(getAccessMode() & IOAM_WRITE)) throw IOException(EACCES);
	errno = 0;
	uint r = ::write(fd, buf, size);
	int e = errno;
	if (e) {
		off_t r = ::lseek(fd, 0, SEEK_SET);
		offset = r;
		if (e != EAGAIN) throw IOException(e);
		return 0;
	} else {
		offset += r;
		return r;
	}		
}
#endif

/*
 *	StdIoFile
 */

/**
 *	create open file
 */
LocalFile::LocalFile(const String &aFilename, IOAccessMode am, FileOpenMode om)
 : File(), mFilename(aFilename)
{
	mOpenMode = om;
	file = NULL;
	own_file = false;
	offset = 0;
	int e = setAccessMode(am);
	if (e) throw IOException(e);
	mOpenMode = FOM_EXISTS;
}

/**
 *	map a file stream [FILE*]
 */
LocalFile::LocalFile(FILE *f, bool own_f, IOAccessMode am)
 : File()
{
	mFilename = NULL;
	file = f;
	own_file = own_f;
	int e = LocalFile::setAccessMode(am);
	if (e) throw IOException(e);
}

LocalFile::~LocalFile()
{
	if (own_file && file) fclose(file);
}

String &LocalFile::getDesc(String &result) const
{
	result = mFilename;
	return result;
}

String &LocalFile::getFilename(String &result) const
{
	result = mFilename;
	return result;
}

FileOfs LocalFile::getSize() const
{
	int t = tell();
	fseek(file, 0, SEEK_END);
	int r = ftell(file);
	fseek(file, t, SEEK_SET);
	return r;
}

void LocalFile::pstat(pstat_t &s) const
{
	sys_pstat(s, mFilename.contentChar());
}

uint LocalFile::read(void *buf, uint size)
{
	if (!(getAccessMode() & IOAM_READ)) throw IOException(EACCES);
	errno = 0;
	uint r = fread(buf, 1, size, file);
	if (errno) throw IOException(errno);
	offset += r;
	return r;
}

void LocalFile::seek(FileOfs o)
{
	if (o == offset) return;
	int e = fseek(file, o, SEEK_SET);
	if (e) throw IOException(e);
	offset = o;					// unreliable for DJGPP
}

int LocalFile::setAccessMode(IOAccessMode am)
{
	IOAccessMode orig_access_mode = getAccessMode();
	int e = setAccessModeInternal(am);
	if (e && setAccessModeInternal(orig_access_mode))
		throw IOException(e);
	return e;
}

int LocalFile::setAccessModeInternal(IOAccessMode am)
{
//RETRY:
	if (getAccessMode() == am) return 0;
	char *mode = NULL;

	switch (mOpenMode) {
		case FOM_APPEND:
			mode = "ab+";
			break;
		case FOM_CREATE:
			if (am & IOAM_WRITE) mode = "wb";
			if (am & IOAM_READ) mode = "wb+";
			break;
		case FOM_EXISTS:
			if (am & IOAM_READ) mode = "rb";
			if (am & IOAM_WRITE) mode = "rb+";
			break;
	}

	int e = 0;
	if (am != IOAM_NULL) {
		pstat_t s;
		if (file) {
			file = freopen(mFilename.contentChar(), mode, file);
			if (!file) setAccessMode(IOAM_NULL);
		} else {
			file = fopen(mFilename.contentChar(), mode);
		}
		if (!file) e = errno;
		if (!e) {
			own_file = true;
			e = sys_pstat_fd(s, fileno(file));
			if (!e) {
				if (HT_S_ISDIR(s.mode)) {
					e = EISDIR;
				} else if (!HT_S_ISREG(s.mode) && !HT_S_ISBLK(s.mode)) {
					e = EINVAL;
				}
			}
		}
	}
	return e ? e : File::setAccessMode(am);
}

FileOfs LocalFile::tell() const
{
	return offset;
}

void LocalFile::truncate(FileOfs newsize)
{
	errno = 0;

	IOAccessMode old_am = getAccessMode();
	int e;
	e = setAccessMode(IOAM_NULL);
	if (!e) {
		e = sys_truncate(mFilename.contentChar(), newsize);
		if (errno) e = errno;
	}
	if (!e) e = setAccessMode(old_am);
	if (e) throw IOException(e);
}

int LocalFile::vcntl(uint cmd, va_list vargs)
{
	switch (cmd) {
		case FCNTL_FLUSH_STAT: {
			IOAccessMode m = getAccessMode();
			int e, f;
			e = setAccessMode(IOAM_NULL);
			f = setAccessMode(m);
			return e ? e : f;
		}
		case FCNTL_GET_FD: {	// (int &fd)
			if (file) {
				int *pfd = va_arg(vargs, int*);
				*pfd = fileno(file);
				return 0;
			}
			break;
		}
	}
	return File::vcntl(cmd, vargs);
}

uint LocalFile::write(const void *buf, uint size)
{
	if (!(getAccessMode() & IOAM_WRITE)) throw IOException(EACCES);
	errno = 0;
	uint r = fwrite(buf, 1, size, file);
	if (errno) throw IOException(errno);
	offset += r;
	return r;
}

/*
 *	TempFile
 */
TempFile::TempFile(uint am) : LocalFile(tmpfile(), true, am)
{
}

String &TempFile::getDesc(String &result) const
{
	result = "temporary file";
	return result;
}

void	TempFile::pstat(pstat_t &s) const
{
	s.caps = pstat_size;
	s.size = getSize();
}

/*
 *	MemMapFile
 */
MemMapFile::MemMapFile(void *b, uint s) : ConstMemMapFile(b, s)
{
}

uint MemMapFile::write(const void *b, uint size)
{
	if (pos > this->size) return 0;	// or throw exception?
	if (pos+size > this->size) size = this->size - pos;
	memmove(((byte*)buf)+pos, b, size);
	pos += size;
	return size;
}

/*
 *	ConstMemMapFile
 */
ConstMemMapFile::ConstMemMapFile(const void *b, uint s)
: File()
{
	buf = b;
	pos = 0;
	size = s;
}

String &ConstMemMapFile::getDesc(String &result) const
{
	result = "MemMapFile";
	return result;
}

FileOfs ConstMemMapFile::getSize() const
{
	return size;
}

uint ConstMemMapFile::read(void *b, uint size)
{
	if (pos > this->size) return 0;
	if (pos+size > this->size) size = this->size - pos;
	memmove(b, (const byte*)buf+pos, size);
	pos += size;
	return size;
}

void ConstMemMapFile::seek(FileOfs offset)
{
	pos = offset;
}

FileOfs ConstMemMapFile::tell() const
{
	return pos;
}

/*
 *	A file layer, representing a cropped version of a file
 */
CroppedFile::CroppedFile(File *file, bool own_file, FileOfs aCropStart, FileOfs aCropSize)
: FileLayer(file, own_file)
{
	mCropStart = aCropStart;
	mHasCropSize = true;
	mCropSize = aCropSize;
	seek(0);
}

CroppedFile::CroppedFile(File *file, bool own_file, FileOfs aCropStart)
: FileLayer(file, own_file)
{
	mCropStart = aCropStart;
	mHasCropSize = false;
	seek(0);
}

void CroppedFile::extend(FileOfs newsize)
{
	throw IOException(ENOSYS);
}

String &CroppedFile::getDesc(String &result) const
{
	String s;
	if (mHasCropSize) {
		result.assignFormat("[->0x%qx,0x%qx] of %y", mCropStart, mCropSize, &FileLayer::getDesc(s));
	} else {
		result.assignFormat("[->0x%qx] of %y", mCropStart, &FileLayer::getDesc(s));
	}
	return result;
}

FileOfs	CroppedFile::getSize() const
{
	FileOfs lsize = FileLayer::getSize();
	if (lsize < mCropStart) return 0;
	lsize -= mCropStart;
	if (mHasCropSize) {
		if (lsize > mCropSize) lsize = mCropSize;
	}
	return lsize;
}

void CroppedFile::pstat(pstat_t &s) const
{
	FileLayer::pstat(s);
	if (s.caps & pstat_size) {
		s.size = getSize();
	}
}

uint CroppedFile::read(void *buf, uint size)
{
	FileOfs offset = FileLayer::tell();
	if (offset<mCropStart) return 0;
	if (mHasCropSize) {
		if (offset >= mCropStart+mCropSize) return 0;
		if (offset+size >= mCropStart+mCropSize) size = mCropStart+mCropSize-offset;
	}
//	ht_printf("CroppedFile::read 0x%08x bytes @ 0x%08qx\n", size, offset);
	return FileLayer::read(buf, size);
}

void CroppedFile::seek(FileOfs offset)
{
/*	if (mHasCropSize) {
...
		if (offset>mCropStart) throw IOException(EIO);
	}*/
	FileLayer::seek(offset+mCropStart);
}

FileOfs CroppedFile::tell() const
{
	FileOfs offset = FileLayer::tell();
	if (offset<mCropStart) throw IOException(EIO);
	return offset - mCropStart;
}

void CroppedFile::truncate(FileOfs newsize)
{
	// not implemented because not considered safe
	throw IOException(ENOSYS);
}

uint CroppedFile::write(const void *buf, uint size)
{
	FileOfs offset = FileLayer::tell();
	if (offset<mCropStart) return 0;
	if (mHasCropSize) {
		if (offset >= mCropStart+mCropSize) return 0;
		if (offset+size >= mCropStart+mCropSize) size = mCropStart+mCropSize-offset;
	}
	return FileLayer::write(buf, size);
}

/*
 *	NullFile
 */
NullFile::NullFile() : File()
{
}

void NullFile::extend(FileOfs newsize)
{
	if (newsize!=0) throw IOException(EINVAL);
}

String &NullFile::getDesc(String &result) const
{
	result = "null device";
	return result;
}

FileOfs NullFile::getSize() const
{
	return 0;
}

void NullFile::pstat(pstat_t &s) const
{
	s.caps = pstat_size;
	s.size = getSize();
}

uint NullFile::read(void *buf, uint size)
{
	return 0;
}

void NullFile::seek(FileOfs offset)
{
	if (offset!=0) throw IOException(EINVAL);
}

int NullFile::setAccessMode(IOAccessMode am)
{
	return (am == getAccessMode()) ? 0 : EACCES;
}

FileOfs NullFile::tell() const
{
	return 0;
}

void NullFile::truncate(FileOfs newsize)
{
	if (newsize!=0) throw IOException(EINVAL);
}

uint NullFile::write(const void *buf, uint size)
{
	return 0;
}

/*
 *	MemoryFile
 */

#define MEMORYFILE_GROW_FACTOR_NUM		4
#define MEMORYFILE_GROW_FACTOR_DENOM		3
#define MEMORYFILE_MIN_BUFSIZE			32

MemoryFile::MemoryFile(FileOfs o, uint size, IOAccessMode mode) : File()
{
	ofs = o;
	dsize = size;
	buf = NULL;
	ibufsize = size;
	if (ibufsize < MEMORYFILE_MIN_BUFSIZE) ibufsize = MEMORYFILE_MIN_BUFSIZE;
	resizeBuf(ibufsize);
	memset(buf, 0, dsize);
	mcount = 0;

	pos = 0;
	int e = setAccessMode(mode);
	if (e) throw IOException(e);
}

MemoryFile::~MemoryFile()
{
	free(buf);
}

byte *MemoryFile::getBufPtr() const
{
	return buf;
}

void MemoryFile::extend(FileOfs newsize)
{
	// MemoryFiles may not be > 2G
	if (newsize > 0x7fffffff) throw IOException(EINVAL);
	if (newsize < getSize()) throw IOException(EINVAL);
	if (newsize == getSize()) return;
	while (bufsize<newsize) extendBuf();
	memset(buf+dsize, 0, newsize-dsize);
	dsize = newsize;
	mcount++;
}

void MemoryFile::extendBuf()
{
	resizeBuf(extendBufSize(bufsize));
}

uint MemoryFile::extendBufSize(uint bufsize)
{
	return bufsize * MEMORYFILE_GROW_FACTOR_NUM / MEMORYFILE_GROW_FACTOR_DENOM;
}

IOAccessMode MemoryFile::getAccessMode() const
{
	return Stream::getAccessMode();
}

String &MemoryFile::getDesc(String &result) const
{
	result = "MemoryFile";
	return result;
}

FileOfs MemoryFile::getSize() const
{
	return dsize;
}

void MemoryFile::pstat(pstat_t &s) const
{
	s.caps = pstat_size;
	s.size = getSize();
}

uint MemoryFile::read(void *b, uint size)
{
	if ((pos+size) > dsize) {
		if (pos >= dsize) return 0;
		size = dsize-pos;
	}
	memcpy(b, buf+pos, size);
	pos += size;
	return size;
}

void MemoryFile::resizeBuf(uint newsize)
{
	bufsize = newsize;

	ASSERT(dsize <= bufsize);

	buf = (byte*)realloc(buf, bufsize ? bufsize : 1);
	if (!buf) throw std::bad_alloc();
}

void MemoryFile::seek(FileOfs o)
{
	if (o<ofs) throw IOException(EINVAL);
	pos = o-ofs;
}

int	MemoryFile::setAccessMode(IOAccessMode mode)
{
	int e = Stream::setAccessMode(mode);
	if (e) return e;
	seek(ofs);
	return 0;
}

uint MemoryFile::shrinkBufSize(uint bufsize)
{
	return bufsize * MEMORYFILE_GROW_FACTOR_DENOM / MEMORYFILE_GROW_FACTOR_NUM;
}

void MemoryFile::shrinkBuf()
{
	resizeBuf(shrinkBufSize(bufsize));
}

FileOfs MemoryFile::tell() const
{
	return pos+ofs;
}

void MemoryFile::truncate(FileOfs newsize)
{
	dsize = newsize;

	uint s = ibufsize;
	while (s<dsize) s = extendBufSize(s);
	
	resizeBuf(s);
	mcount++;
}

uint MemoryFile::write(const void *b, uint size)
{
	while (pos+size >= bufsize) extendBuf();
	memmove(((byte*)buf)+pos, b, size);
	pos += size;
	if (pos>dsize) dsize = pos;
	mcount++;
	return size;
}

/*
 *	string stream functions
 */

char *fgetstrz(File *file)
{
	FileOfs o = file->tell();
/* get string size */
	char buf[64];
	int s, z = 0;
	bool found = false;
	while (!found) {
		s = file->read(buf, 64);
		for (int i=0; i<s; i++) {
			z++;
			if (buf[i] == 0) {
				found = true;
				break;
			}
		}
	}
/* read string */
	char *str = (char*)malloc(z);
	if (!str) throw std::bad_alloc();
	file->seek(o);
	file->readx(str, z);
	return str;
}

// FIXME: more dynamical solution appreciated
#define REASONABLE_STRING_LIMIT	1024
 
char *getstrz(Stream *stream)
{
/* get string size */
	char buf[REASONABLE_STRING_LIMIT];
	int z = 0;
	while (1) {
		stream->readx(buf+z, 1);
		z++;
		if (z >= REASONABLE_STRING_LIMIT) {
			z = REASONABLE_STRING_LIMIT;
			break;
		}
		if (buf[z-1] == 0) break;
	}
	if (!z) return NULL;
	char *str = (char*)malloc(z);
	if (!str) throw std::bad_alloc();
	memmove(str, buf, z-1);
	str[z-1] = 0;
	return str;
}

void putstrz(Stream *stream, const char *str)
{
	stream->writex(str, strlen(str)+1);
}

char *getstrp(Stream *stream)
{
	unsigned char l;
	stream->readx(&l, 1);
	char *str = (char*)malloc(l+1);
	if (!str) throw std::bad_alloc();
	stream->readx(str, l);
	*(str+l) = 0;
	return str;
}

void putstrp(Stream *stream, const char *str)
{
	unsigned char l = strlen(str);
	stream->writex(&l, 1);
	stream->writex(str, l);
}

char *getstrw(Stream *stream)
{
	short t;
	byte lbuf[2];
	stream->readx(lbuf, 2);
	int l = lbuf[0] | lbuf[1] << 8;
	char	*a = (char*)malloc(l+1);
	if (!a) throw std::bad_alloc();
	for (int i=0; i<l; i++) {
		stream->readx(&t, 2);
		a[i] = (char)t;
	}
	a[l] = 0;
	return a;
}

void putstrw(Stream *stream, const char *str)
{
	/* FIXME: someone implement me ? */
	throw NotImplementedException(HERE);
}

