/* 
 *	HT Editor
 *	configfile.h
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

#ifndef __CONFIGFILE_H__
#define __CONFIGFILE_H__

extern char *systemconfig_file;

enum ConfigFileType {
	CONFIGFILE_BIN = 0,
	CONFIGFILE_TEXT = 1,
	CONFIGFILE_BIN_COMPRESSED = 2
};

/*	SYSTEM CONFIG FILE VERSION HISTORY
 *	Version 2: HT 0.4.4
 *	Version 3: HT 0.4.5
 *	Version 4: HT 0.5.0
 *	Version 5: HT 0.6.0
 *	Version 6: HT 0.8.0
 */

#define systemconfig_magic				"HTCP"
#define systemconfig_fileversion			6

/*	FILE CONFIG FILE VERSION HISTORY
 *	Version 1: HT 0.5.0
 *	Version 2: HT 0.6.0
 *	Version 3: HT 0.7.0
 *	Version 4: HT 0.8.0
 */

#define fileconfig_magic				"HTCF"
#define fileconfig_fileversion			4

/*	PROJECT CONFIG FILE VERSION HISTORY
 *	Version 1: HT 0.7.0
 *	Version 2: HT 0.8.0
 */

#define projectconfig_magic				"HTPR"
#define projectconfig_fileversion			2

class ConfigFileObjectStream: public ObjectStreamLayer {
public:
/**
 *	Construct config file ObjectStream, that can read configuration data (Objects).
 */
		ConfigFileObjectStream(File *readable_file, bool own_rfile, char magic[4], uint32 version);
/**
 *	Construct config file ObjectStream, that can write configuration data (Objects).
 */
		ConfigFileObjectStream(File *writable_file, bool own_wfile, char magic[4], uint32 version, ConfigFileType type);
};

#if 0
#include "common.h"
#include "stream.h"

enum loadstore_result {
	LS_OK,
	LS_ERROR_NOT_FOUND,
	LS_ERROR_READ,
	LS_ERROR_WRITE,
	LS_ERROR_MAGIC,
	LS_ERROR_VERSION,             // sets error_info to version
	LS_ERROR_FORMAT,
	LS_ERROR_CORRUPTED
};



/**/

extern char *systemconfig_file;
loadstore_result save_systemconfig();
bool load_systemconfig(loadstore_result *result, int *error_info);

typedef int (*load_fcfg_func)(ht_object_stream *f, void *context);
typedef void (*store_fcfg_func)(ht_object_stream *f, void *context);

loadstore_result save_fileconfig(char *fileconfig_file, const char *magic, UINT version, store_fcfg_func store_func, void *context);
loadstore_result load_fileconfig(char *fileconfig_file, const char *magic, UINT version, load_fcfg_func load_func, void *context, int *error_info);
#endif

/*
 *	INIT
 */

bool initConfigFile();

/*
 *	DONE
 */

void doneConfigFile();

#endif /* !__CONFIGFILE_H__ */

