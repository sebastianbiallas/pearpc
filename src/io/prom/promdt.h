/*
 *	PearPC
 *	promdt.h
 *
 *	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
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

#ifndef __PROMDT_H__
#define __PROMDT_H__

#include "tools/data.h"
#include "io/prom/fs/part.h"
#include "promosi.h"

class PromNode;
class PromProp;
class PromInstance;

extern PromNode *gPromRoot;
// FIXME: HACK!!!
extern int	gBootPartNum;
extern int	gBootNodeID;

typedef uint32 PromInstanceHandle;
typedef uint32 PromPackageHandle;

#define ATOM_PROM_OBJECT	MAGIC32("PRM\x00")
#define ATOM_PROM_NODE		MAGIC32("PRM\x01")
#define ATOM_PROM_PROP		MAGIC32("PRM\x02")

/*
 *	This is the base class for PromNode and PromProps, i.e. everything that is 
 *	in the device tree.
 */
class PromObject: public Object {
public:
	PromNode *owner;
	char *name;

					PromObject(const char *name);
	virtual				~PromObject();
	virtual	int			compareTo(const Object *obj) const;
	virtual	ObjectID		getObjectID() const;
	virtual	void			setOwner(PromNode *aOwner);

	virtual	uint32			toPath(char *buf, uint32 buflen);
};

class PromNode: public PromObject {
protected:
	Container *childs;
	Container *props;
	Container *shortcuts;
	int mPromNodeHandle;
	int mPromNodeInstancesHandles;
public:
	static int promNodeHandles;
					PromNode(const char *name);
	virtual				~PromNode();
	virtual	ObjectID		getObjectID() const;

	virtual	bool			addNode(PromNode *node);
	virtual	bool			addProp(PromProp *node);
	virtual	bool			addNodeShort(const char *name, const char *nodename);
	virtual PromNode *		findNode(const char *name);
	virtual PromProp *		findProp(const char *name);
	virtual PromNode *		firstChild();
	virtual PromNode *		nextChild(PromNode *p);
	virtual	PromProp *		firstProp();
	virtual	PromProp *		nextProp(PromProp *p);

		PromInstanceHandle	open(const String &param);
		void			close(PromInstanceHandle pih);
	        PromPackageHandle	getPHandle();
protected:
	virtual PromInstance *		createInstance(const String &param);
};

class PromProp: public PromObject {
public:
					PromProp(const char *name);
	virtual	ObjectID		getObjectID() const;

	virtual	uint32			getValue(uint32 buf, uint32 buflen);
	virtual	uint32			getValueLen();
	virtual	uint32			setValue(uint32 buf, uint32 buflen);
};

/*
 *	This is the base class for instances of PromNodes.
 */
class PromInstance: public Object {
protected:
	PromNode *mType;
	PromInstanceHandle mHandle;
public:
					PromInstance(PromNode *type, const String &param);
	virtual				~PromInstance();
	
		PromNode		*getType();
		void			setIHandle(PromInstanceHandle handle);
		PromInstanceHandle	getIHandle();
	virtual	void			callMethod(const char *method, prom_args *pa);
	virtual	uint32			read(uint32 buf, int length);
	virtual uint32			seek(uint64 pos);
	virtual	uint32			write(uint32 buf, int length);
};

class PromPropLink: public PromProp {
public:
	PromInstanceHandle value;
					PromPropLink(const char *name, PromInstanceHandle value);
	virtual				~PromPropLink();

	virtual	uint32			getValueLen();
	virtual	uint32			getValue(uint32 buf, uint32 buflen);
};

class PromPropInt: public PromProp {
public:
	uint32 value;
					PromPropInt(const char *name, uint32 value);
	virtual				~PromPropInt();

	virtual	uint32			getValueLen();
	virtual	uint32			getValue(uint32 buf, uint32 buflen);
};

class PromPropMemory: public PromProp {
public:
	void *buf;
	int size;
					PromPropMemory(const char *name, const void *buf, int size);
	virtual				~PromPropMemory();

	virtual	uint32			getValueLen();
	virtual	uint32			setValue(uint32 buf, uint32 buflen);
	virtual	uint32			getValue(uint32 buf, uint32 buflen);
};

class PromNodeATY: public PromNode {
public:
					PromNodeATY(const char *name);
protected:
	virtual PromInstance *		createInstance(const String &param);
};


class PromInstanceATY: public PromInstance {
public:
					PromInstanceATY(PromNode *type, const String &param);
	
	virtual	void			callMethod(const char *method, prom_args *pa);
	virtual	uint32			write(uint32 buf, int length);
};

class PromNodeKBD: public PromNode {
public:
					PromNodeKBD(const char *name);
protected:
	virtual PromInstance *		createInstance(const String &param);
};

class PromInstanceKBD: public PromInstance {
public:
					PromInstanceKBD(PromNode *type, const String &param);
	
	virtual	uint32			read(uint32 buf, int length);
};

class PromNodeRTAS: public PromNode {
public:
						PromNodeRTAS(const char *name);
protected:
	virtual PromInstance *		createInstance(const String &param);
};

class PromInstanceRTAS: public PromInstance {
public:
					PromInstanceRTAS(PromNode *type, const String &param);
	
	virtual	void			callMethod(const char *method, prom_args *pa);
	virtual	uint32			write(uint32 buf, int length);
};

class PromNodeMMU: public PromNode {
public:
					PromNodeMMU(const char *name);
protected:
	virtual PromInstance *		createInstance(const String &param);
};

class PromInstanceMMU: public PromInstance {
public:
					PromInstanceMMU(PromNode *type, const String &param);
	
	virtual	void			callMethod(const char *method, prom_args *pa);
	virtual	uint32			write(uint32 buf, int length);
};

class PromNodeMemory: public PromNode {
public:
					PromNodeMemory(const char *name);
protected:
	virtual PromInstance *		createInstance(const String &param);
};

class PromInstanceMemory: public PromInstance {
public:
					PromInstanceMemory(PromNode *type, const String &param);
	
	virtual	void			callMethod(const char *method, prom_args *pa);
};

class PromNodeDisk: public PromNode {
public:
	int mNumber;
	PartitionMap *pm;
	File *mDevice;
	FileSystem *mFS;

					PromNodeDisk(const char *name, int aNumber);
	virtual				~PromNodeDisk();
protected:
	virtual PromInstance *		createInstance(const String &param);
};

class PromInstanceDiskPart: public PromInstance {
	int mNumber;
	uint64 mOffset;
public:

					PromInstanceDiskPart(PromNode *type, uint partnum);
	virtual	void			callMethod(const char *method, prom_args *pa);
	virtual	uint32			read(uint32 buf, int length);
	virtual uint32			seek(uint64 pos);
	virtual	uint32			write(uint32 buf, int length);
};

class PromInstanceDiskFile: public PromInstance {
public:
	File *	mFile;

					PromInstanceDiskFile(PromNode *type, File *file);
	virtual	void			callMethod(const char *method, prom_args *pa);
	virtual	uint32			read(uint32 buf, int length);
	virtual uint32			seek(uint64 pos);
	virtual	uint32			write(uint32 buf, int length);
};

class PromPropString: public PromProp {
public:
	char *value;
					PromPropString(const char *name, const char *value);
	virtual				~PromPropString();
	
	virtual	uint32			getValueLen();
	virtual	uint32			getValue(uint32 buf, uint32 buflen);
	virtual uint32			setValue(uint32 buf, uint32 buflen);
};

#define FIND_DEVICE_FIND 0
#define FIND_DEVICE_OPEN 1
PromNode *findDevice(const char *aPathName, int type, PromInstanceHandle *ret);
void prom_init_device_tree();
void prom_done_device_tree();

PromNode *handleToPackage(PromPackageHandle ph);
PromInstance *handleToInstance(PromInstanceHandle ih);

#endif
