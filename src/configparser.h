/*
 *	PearPC
 *	configparser.cc
 *
 *	Copyright (C) 2003 Sebastian Biallas (sb@biallas.net)
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

#ifndef __CONFIGPARSER_H__
#define __CONFIGPARSER_H__

#include "tools/data.h"
#include "tools/str.h"
#include "tools/stream.h"

enum ConfigType {
    configTypeInt,
    configTypeString,
};

class ConfigEntry: public Object {
public:
	String *mName;
	bool mMandatory;
	bool mInitialized;
	bool mSet;
	
			ConfigEntry(const String &aName, bool mandatory);
	virtual 	~ConfigEntry();
	virtual int	asInt() const;
	virtual String	&asString(String &result) const;
	virtual ConfigType getType() const;
	virtual bool	isSet() const;
	virtual bool	isInitialized() const;
	virtual	int	compareTo(const Object *obj) const;
};

class ConfigParser: public Object {
	Container *entries;
	byte cur;
	int line;
public:
			ConfigParser();
	virtual		~ConfigParser();

		void	acceptConfigEntryInt(const String &mName, bool mandatory);
		void	acceptConfigEntryString(const String &mName, bool mandatory);
		void	acceptConfigEntryIntDef(const String &mName, int d);
		void	acceptConfigEntryStringDef(const String &mName, const String &d);
		void	loadConfig(Stream &in);

		ConfigEntry *getEntry(const String &name);
		bool	haveKey(const String &name);

		// these will throw an exception if key isn't set!
		int	getConfigInt(const String &name);
		String &getConfigString(const String &name, String &result);
protected:
		bool	skipWhite(Stream &in);
		void	read(Stream &in);
};

extern ConfigParser *gConfig;

#endif
