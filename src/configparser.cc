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

#include <ctype.h>
#include "configparser.h"
#include "tools/except.h"
#include "tools/snprintf.h"

ConfigParser *gConfig;

ConfigEntry::ConfigEntry(const String &aName, bool mandatory)
{
    mName = new String(aName);
    mMandatory = mandatory;
    mInitialized = false;
    mSet = false;
}

ConfigEntry::~ConfigEntry()
{
    delete mName;
}

ConfigType ConfigEntry::getType() const
{
    throw Exception();
}

int ConfigEntry::asInt() const
{
    throw MsgfException("cannot interprete config entry %y as integer", mName);
}

String &ConfigEntry::asString(String &result) const
{
    throw MsgfException("cannot interprete config entry %y as string", mName);
}

int ConfigEntry::compareTo(const Object *obj) const
{
    return mName->compareTo(((ConfigEntry *)obj)->mName);
}

bool ConfigEntry::isSet() const
{
    return mSet;
}

bool ConfigEntry::isInitialized() const
{
    return mInitialized;
}


class ConfigEntryInt : public ConfigEntry {
    uint64 value;

public:
    ConfigEntryInt(const String &aName, bool mandatory) : ConfigEntry(aName, mandatory), value(0) {}

    ConfigEntryInt(const String &aName, uint64 defaultvalue) : ConfigEntry(aName, false), value(defaultvalue)
    {
        mInitialized = true;
    }

    virtual ConfigType getType() const
    {
        return configTypeInt;
    }

    void set(uint64 v)
    {
        value = v;
        mInitialized = true;
        mSet = true;
    }

    virtual int asInt() const
    {
        return (int)value;
    }

    uint32 asUInt() const
    {
        return (uint32)value;
    }
};

class ConfigEntryString : public ConfigEntry {
    String value;

public:
    ConfigEntryString(const String &aName, bool mandatory) : ConfigEntry(aName, mandatory) {}

    ConfigEntryString(const String &aName, const String &defaultvalue) : ConfigEntry(aName, false), value(defaultvalue)
    {
        mInitialized = true;
    }

    virtual ConfigType getType() const
    {
        return configTypeString;
    }

    void set(const String &s)
    {
        value = s;
        mInitialized = true;
        mSet = true;
    }

    virtual String &asString(String &result) const
    {
        result = value;
        return result;
    }
};

ConfigParser::ConfigParser()
{
    entries = new AVLTree(true);
}

ConfigParser::~ConfigParser()
{
    delete entries;
}

void ConfigParser::acceptConfigEntryInt(const String &mName, bool mandatory)
{
    ConfigEntry *entry = new ConfigEntryInt(mName, mandatory);
    if (!entries->insert(entry)) {
        throw MsgfException("duplicate config entry '%y'", &mName);
    }
}

void ConfigParser::acceptConfigEntryString(const String &mName, bool mandatory)
{
    ConfigEntry *entry = new ConfigEntryString(mName, mandatory);
    if (!entries->insert(entry)) {
        throw MsgfException("duplicate config entry '%y'", &mName);
    }
}

void ConfigParser::acceptConfigEntryIntDef(const String &mName, int d)
{
    ConfigEntry *entry = new ConfigEntryInt(mName, (uint64)d);
    if (!entries->insert(entry)) {
        throw MsgfException("duplicate config entry '%y'", &mName);
    }
}

void ConfigParser::acceptConfigEntryStringDef(const String &mName, const String &d)
{
    ConfigEntry *entry = new ConfigEntryString(mName, d);
    if (!entries->insert(entry)) {
        throw MsgfException("duplicate config entry '%y'", &mName);
    }
}

bool ConfigParser::skipWhite(Stream &in)
{
    while (cur == ' ' || cur == '\t' || cur == '\r') {
        if (!in.read(&cur, 1)) {
            return false;
        }
    }
    return true;
}

#define INV 0xff
byte mapchar[] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 10,  ' ', ' ', ' ', ' ', ' ', // 0-15
                  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // 16-31
                  ' ', '!', '"', '#', '$', '%', '&', 39,  '(', ')', '*', '+', ',', '-', '.', '/', // 32-47
                  '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', ':', ';', '<', '=', '>', '?', // 48-63
                  '@', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', // 64-79
                  'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', '[', 92,  ']', '^', '_', // 80-95
                  INV, 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', // 96-111
                  'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', '{', '|', '}', '~', INV, // 112-127
                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV};

void ConfigParser::loadConfig(Stream &in)
{
    read(in);
}

void ConfigParser::validateConfig()
{
    foreach (ConfigEntry, e, *entries, {
        if (e->mMandatory && !e->isInitialized()) {
            throw MsgfException("config entry '%y' is not set.", e->mName);
        }
    })
        ;
}

void ConfigParser::setConfigEntry(const String &name, const String &value)
{
    ConfigEntry *e = getEntry(name);
    if (!e) {
        throw MsgfException("unknown config key '%y'.", &name);
    }
    if (e->getType() == configTypeInt) {
        uint64 u;
        if (!value.toInt64(u)) {
            throw MsgfException("invalid integer value for '%y'.", &name);
        }
        ((ConfigEntryInt *)e)->set(u);
    } else {
        ((ConfigEntryString *)e)->set(value);
    }
}

void ConfigParser::printUsage()
{
    ht_printf("Available config keys:\n");
    foreach (ConfigEntry, e, *entries, {
        const char *type = (e->getType() == configTypeInt) ? "int" : "string";
        if (e->isInitialized()) {
            if (e->getType() == configTypeInt) {
                int v = e->asInt();
                if (v > 0xffff || v < 0) {
                    ht_printf("  %-40y  %-6s  (default: 0x%x)\n", e->mName, type, v);
                } else {
                    ht_printf("  %-40y  %-6s  (default: %d)\n", e->mName, type, v);
                }
            } else {
                String val;
                e->asString(val);
                ht_printf("  %-40y  %-6s  (default: \"%y\")\n", e->mName, type, &val);
            }
        } else {
            ht_printf("  %-40y  %-6s\n", e->mName, type);
        }
    })
        ;
}

void ConfigParser::read(Stream &in)
{
    if (!in.read(&cur, 1)) {
        return;
    }
    line = 1;
    while (true) {
        if (!skipWhite(in)) {
            return;
        }
        if (cur == '#') {
            // skip comment
            do {
                if (!in.read(&cur, 1)) {
                    return;
                }
            } while (cur != '\n');
            if (!in.read(&cur, 1)) {
                return;
            }
            line++;
            continue;
        }
        if (cur == '\r') {
            if (!in.read(&cur, 1)) {
                return;
            }
            continue;
        }
        if (cur == '\n') {
            if (!in.read(&cur, 1)) {
                return;
            }
            line++;
            continue;
        }
        byte m = mapchar[cur];
        String ident;
        if (m != 'A' && m != '_') {
            throw MsgfException("invalid character '%c' (%02x) in line %d.", cur, cur, line);
        }
        do {
            ident += cur;
            if (!in.read(&cur, 1)) {
                throw MsgfException("syntax error in line %d.", line);
            }
            m = mapchar[cur];
        } while (m == 'A' || m == '0' || m == '_');

        ConfigEntry *e = getEntry(ident);
        if (!e) {
            throw MsgfException("unknown identifier '%y' in line %d.", &ident, line);
        }
        if (e->isSet()) {
            throw MsgfException("config entry '%y' is already set in line %d.", e->mName, line);
        }

        if (!skipWhite(in)) {
            throw MsgfException("%s expected in line %d.", "'='", line);
        }
        if (cur != '=') {
            throw MsgfException("%s expected in line %d.", "'='", line);
        }
        if (!in.read(&cur, 1) || !skipWhite(in)) {
            throw MsgfException("syntax error in line %d.", line);
        }
        m = mapchar[cur];

        if (e->getType() == configTypeInt) {
            if (m != '0') {
                throw MsgfException("%s expected in line %d.", "integer", line);
            }
            String n;
            do {
                n += cur;
                if (!in.read(&cur, 1)) {
                    cur = ' ';
                }
                m = mapchar[cur];
            } while (m == '0' || m == 'A');
            if (cur == '\n' || cur == '\r' || cur == ' ' || cur == '\t' || cur == '#') {
                // nothing to do
            } else {
                cur = tolower(cur);
                if (cur == 'h' || cur == 'o' || cur == 'b' || cur == 'd') {
                    n += cur;
                    if (!in.read(&cur, 1)) {
                        cur = ' ';
                    }
                } else {
                    throw MsgfException("%s expected in line %d.", "integer", line);
                }
            }
            uint64 u;
            if (!n.toInt64(u)) {
                throw MsgfException("%s expected in line %d.", "integer", line);
            }
            ((ConfigEntryInt *)e)->set(u);
        } else {
            if (m != '"') {
                throw MsgfException("%s expected in line %d.", "'\"'", line);
            }
            String s;
            int oldline = line;
            do {
                s += cur;
                if (!in.read(&cur, 1)) {
                    throw MsgfException("unterminated string in line %d (starts in line %d).", line, oldline);
                }
                m = mapchar[cur];
                if (m == '\n') {
                    line++;
                }
            } while (m != '"');
            s.del(0, 1);
            ((ConfigEntryString *)e)->set(s);
            if (!in.read(&cur, 1)) {
                cur = ' ';
            }
        }
        if (!skipWhite(in)) {
            return;
        }
        if (cur == '#') {
            continue;
        }
        if (cur != '\n') {
            throw MsgfException("syntax error in line %d.", line);
        }
    }
}

ConfigEntry *ConfigParser::getEntry(const String &name)
{
    ConfigEntry empty(name, false);
    return (ConfigEntry *)entries->get(entries->find(&empty));
}

int ConfigParser::getConfigInt(const String &name)
{
    ConfigEntry empty(name, false);
    ConfigEntry *entry = (ConfigEntry *)entries->get(entries->find(&empty));
    if (!entry) {
        throw MsgfException("unknown entry '%y'", &name);
    }
    if (!entry->isInitialized()) {
        throw MsgfException("%y is not set!", &name);
    }
    return entry->asInt();
}

uint32 ConfigParser::getConfigUInt(const String &name)
{
    ConfigEntry empty(name, false);
    ConfigEntry *entry = (ConfigEntry *)entries->get(entries->find(&empty));
    if (!entry) {
        throw MsgfException("unknown entry '%y'", &name);
    }
    if (!entry->isInitialized()) {
        throw MsgfException("%y is not set!", &name);
    }
    return ((ConfigEntryInt *)entry)->asUInt();
}

String &ConfigParser::getConfigString(const String &name, String &result)
{
    ConfigEntry empty(name, false);
    ConfigEntry *entry = (ConfigEntry *)entries->get(entries->find(&empty));
    if (!entry) {
        throw MsgfException("unknown entry '%y'", &name);
    }
    if (!entry->isInitialized()) {
        throw MsgfException("%y is not set!", &name);
    }
    return entry->asString(result);
}

bool ConfigParser::haveKey(const String &name)
{
    ConfigEntry empty(name, false);
    ConfigEntry *entry = (ConfigEntry *)entries->get(entries->find(&empty));
    return entry && entry->isSet();
}
