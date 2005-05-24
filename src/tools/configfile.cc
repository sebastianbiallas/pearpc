/*
 *	HT Editor
 *	configfile.cc
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
 
#include <cstdlib>
#include <cstring> 

// FIXME: needed for gAppFilename
#include "system/sys.h"

#include "cstream.h"
#include "configfile.h"
#include "tools/debug.h"
#include "tools/endianess.h"
#include "tools/except.h"
#include "tools/strtools.h"

/* VERSION 2 (for ht-0.4.4 and later) */

/* NOTE: as of Version 2 ALL integers in HT config-files are
   stored in big-endian (network-order, non-intel) format...  */

struct ConfigFileHeader {
	char magic[4] PACKED;
	char version[4] PACKED;
	char type[2] PACKED;
};

ObjectStream *createObjectStream(File *file, bool own_file, ConfigFileType type)
{
	switch (type) {
		case CONFIGFILE_BIN: return new ObjectStreamBin(file, own_file);
		case CONFIGFILE_TEXT: return new ObjectStreamText(file, own_file);
		case CONFIGFILE_BIN_COMPRESSED: {
			CompressedStream *cs = new CompressedStream(file, own_file);
			return new ObjectStreamBin(cs, true);
		}
		default: return NULL;
	}
}

static ObjectStream *createReadConfigFile(File *rfile, bool own_rfile, char magic[4], uint32 version)
{
	ConfigFileHeader header;
	String filedesc;
	rfile->getDesc(filedesc);
	rfile->readx(&header, sizeof header);

	// magic
	if (memcmp(magic, header.magic, 4) != 0) throw MsgfException(
		"Invalid config-file magic in '%y' (found '%c%c%c%c', expected '%c%c%c%c')",
		&filedesc, header.magic[0], header.magic[1], header.magic[2],
		header.magic[3], magic[0], magic[1], magic[2], magic[3]);

	// version
	uint16 read_version;
	if (!hexw_ex(read_version, header.version)) throw MsgfException(
		"Corrupted config-file header in'%y' (invalid version '%c%c%c%c', must be lowercase-hex)",
		&filedesc, header.version[0], header.version[1], header.version[2], header.version[3]);
	if (read_version != version) {
		char *rela;
		char *hint;
		if (read_version < version) {
			rela = "old";
			hint = "The file has probably been left over by an older version of the program. You might want to delete it.";
		} else {
			rela = "NEW";
			hint = "You're probably using an old version of the program. You might want to update.";
		}
		throw MsgfException(
		"Config-file is too %s in '%y' (found version %d/0x%x, expected version %d/0x%x).\n%s",
		rela, &filedesc, read_version, read_version, version, version, hint);
	}

	// type
	uint8 read_type;
	if (!hexb_ex(read_type, header.type)) throw MsgfException(
		"Corrupted config-file header in'%y' (invalid type '%c%c', must be lowercase-hex)",
		&filedesc, header.type[0], header.type[1]);

	ObjectStream *o = createObjectStream(rfile, own_rfile, (ConfigFileType)read_type);
	if (!o) throw MsgfException(
		"Corrupted config-file in '%y' (invalid type %d/0x%x)",
		&filedesc, read_type, read_type);

	return o;
}

static ObjectStream *createWriteConfigFile(File *wfile, bool own_wfile, char magic[4], uint32 version, ConfigFileType type)
{
	ObjectStream *o = createObjectStream(wfile, own_wfile, type);
	ConfigFileHeader header;
	char scratch[16];
	// magic
	memcpy(header.magic, magic, 4);
	
	// version
	sprintf(scratch, "%04x", version);
	memcpy(header.version, scratch, 4);
	
	// type
	sprintf(scratch, "%02x", type);
	memcpy(header.type, scratch, 2);

	o->writex(&header, sizeof header);

	if (type == CONFIGFILE_TEXT) {
		o->writex((void*)"\n#\n#\tThis is a generated file!\n#\n", 33);
	}		

	return o;
}

ConfigFileObjectStream::ConfigFileObjectStream(File *readable_file, bool own_rfile, char magic[4], uint32 version)
: ObjectStreamLayer(createReadConfigFile(readable_file, own_rfile, magic, version), true)
{
	ASSERT(readable_file->getAccessMode() & IOAM_READ);
}

ConfigFileObjectStream::ConfigFileObjectStream(File *writable_file, bool own_wfile, char magic[4], uint32 version, ConfigFileType type)
: ObjectStreamLayer(createWriteConfigFile(writable_file, own_wfile, magic, version, type), true)
{
	ASSERT(writable_file->getAccessMode() & IOAM_WRITE);
}

/*
 *	INIT
 */
 
char *systemconfig_file;

bool initConfigFile()
{
#if defined(MSDOS) || defined(DJGPP) || defined(WIN32) || defined(__WIN32__)
#define SYSTEM_CONFIG_FILE_NAME "ht.cfg"
	char d[1024];	/* FIXME: !!!! */
	sys_dirname(d, gAppFilename);
	char *b = "\\"SYSTEM_CONFIG_FILE_NAME;
	systemconfig_file = (char*)malloc(strlen(d)+strlen(b)+1);
	strcpy(systemconfig_file, d);
	strcat(systemconfig_file, b);
#else
#define SYSTEM_CONFIG_FILE_NAME ".htcfg"
	char *home = getenv("HOME");
	char *b = "/"SYSTEM_CONFIG_FILE_NAME;
	if (!home) home = "";
	systemconfig_file = (char*)malloc(strlen(home)+strlen(b)+1);
	strcpy(systemconfig_file, home);
	strcat(systemconfig_file, b);
#endif
	return true;
}

/*
 *	DONE
 */

void doneConfigFile()
{
	free(systemconfig_file);
}
