/*
 *	PearPC
 *	promdt.cc
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

#include <cstring>
#include <cstdlib>
#include "debug/tracers.h"
#include "tools/debug.h"
#include "cpu/cpu.h"
#include "cpu/mem.h"
#include "io/graphic/gcard.h"
#include "io/cuda/cuda.h"
#include "io/pic/pic.h"
#include "io/ide/ide.h"
#include "io/3c90x/3c90x.h"
#include "io/rtl8139/rtl8139.h"
#include "system/arch/sysendian.h"
#include "system/keyboard.h"
#include "system/display.h"
#include "prommem.h"
#include "promdt.h"
#include "tools/except.h"

#include "info.h"

PromNode *gPromRoot;
AVLTree *gPromPackages;
AVLTree *gPromInstances;
int gBootPartNum = -1;
int gBootNodeID = -1;

int PromNode::promNodeHandles = 1;

class PromKV: public Object {
public:
	Object *mKey, *mValue;
	PromKV(Object *aKey, Object *aValue)
	{
		mKey = aKey;
		mValue = aValue;
	}

	~PromKV()
	{
		delete mKey;
	}
	
	virtual int compareTo(const Object *obj) const
	{
		return mKey->compareTo(((PromKV*)obj)->mKey);
	}

};

void registerPackage(PromPackageHandle ph, PromNode *pn)
{
	gPromPackages->insert(new PromKV(new UInt(ph), pn));
}

void registerInstance(PromInstanceHandle ih, PromInstance *pi)
{
	gPromInstances->insert(new PromKV(new UInt(ih), pi));
}

void unregisterInstance(PromInstanceHandle ih)
{
	PromKV pio(new UInt(ih), NULL);
	gPromInstances->del(gPromInstances->find(&pio));
}

PromNode *handleToPackage(PromPackageHandle ph)
{
	PromKV pho(new UInt(ph), NULL);
	PromKV *kv = (PromKV *)gPromPackages->get(gPromPackages->find(&pho));
	return kv ? (PromNode *)kv->mValue : 0;
}

PromInstance *handleToInstance(PromInstanceHandle ih)
{
	PromKV pio(new UInt(ih), NULL);
	PromKV *kv = (PromKV *)gPromInstances->get(gPromInstances->find(&pio));
	return kv ? (PromInstance *)kv->mValue : 0;
}

/*
 *
 */
PromObject::PromObject(const char *aName)
	:Object()
{
	name = strdup(aName);
	owner = NULL;
}

PromObject::~PromObject()
{

	free(name);
}

int PromObject::compareTo(const Object *obj) const
{
	return strcmp(name, ((PromNode*)obj)->name);
}

ObjectID PromObject::getObjectID() const
{
	return ATOM_PROM_OBJECT;
}

void PromObject::setOwner(PromNode *aOwner)
{
	owner = aOwner;
}

uint32 PromObject::toPath(char *buf, uint32 buflen)
{
	if (owner) {
		uint32 s = owner->toPath(buf, buflen);
		// XXX
		if (owner != gPromRoot) strcat(buf, "/");
		strcat(buf, name);
		return s+1+strlen(name);
	} else {
		if (buflen > 1) {
			strcpy(buf, "/");
		}
		return 1;
	}     
}

/*
 *
 */
PromNode::PromNode(const char *aName)
	:PromObject(aName)
{
	childs = new AVLTree(true);
	props = new AVLTree(true);
	shortcuts = new AVLTree(true);

	String nodeaddr(aName), nodename, unitaddr;
	nodeaddr.leftSplit('@', nodename, unitaddr);
	PromPropString *pn = new PromPropString("name", nodename);
	addProp(pn);
	mPromNodeHandle = promNodeHandles++;
	mPromNodeInstancesHandles = 0;
	registerPackage(mPromNodeHandle, this);
}

PromNode::~PromNode()
{
	delete childs;
	delete props;
	delete shortcuts;
}

ObjectID PromNode::getObjectID() const
{
	return ATOM_PROM_NODE;
}

bool PromNode::addNode(PromNode *node)
{
	if (childs->insert(node)) {
		node->setOwner(this);
		return true;
	} else {
		return false;
	}
}

bool PromNode::addProp(PromProp *node)
{
	if (props->insert(node)) {
		node->setOwner(this);
		return true;
	} else {
		return false;
	}
}

bool PromNode::addNodeShort(const char *name, const char *node)
{
	KeyValue *kv = new KeyValue(new String(name), new String(node));
	if (shortcuts->insert(kv)) {
		return true;
	} else {
		delete kv;
		return false;
	}
}

PromNode *PromNode::findNode(const char *name)
{
	PromNode empty(name);
	ObjHandle oh = childs->find(&empty);
	if (oh == InvObjHandle) {
		KeyValue empty2(new String(name), NULL);
		oh = shortcuts->find(&empty2);
		if (oh != InvObjHandle) {
			PromNode empty3(((String *)((KeyValue *)shortcuts->get(oh))->mValue)->contentChar());
			oh = childs->find(&empty3);
			return (PromNode*)childs->get(oh);
		}
	} else {
		return (PromNode*)childs->get(oh);
	}
	return NULL;
}

PromProp *PromNode::findProp(const char *name)
{
	PromProp empty(name);
	ObjHandle oh = props->find(&empty);
	return (oh == InvObjHandle) ? NULL : (PromProp*)props->get(oh);
}

PromNode *PromNode::firstChild()
{
	return (PromNode*)childs->get(childs->findFirst());
}

PromNode *PromNode::nextChild(PromNode *p)
{
	ObjHandle oh = childs->find(p);
	return (oh == InvObjHandle) ? NULL : (PromNode*)childs->get(childs->findNext(childs->find(p)));
}

PromProp *PromNode::firstProp()
{
	return (PromProp*)props->get(props->findFirst());
	
}

PromProp *PromNode::nextProp(PromProp *p)
{
	ObjHandle oh = props->find(p);
	return (oh == InvObjHandle) ? NULL : (PromProp*)props->get(props->findNext(props->find(p)));
}

PromInstanceHandle PromNode::open(const String &param)
{
	PromInstance *pi = createInstance(param);
	if (pi) {
		PromInstanceHandle pih = (mPromNodeHandle<<16) | (mPromNodeInstancesHandles++);
		pi->setIHandle(pih);
		registerInstance(pih, pi);
		return pih;
	} else {
		IO_PROM_WARN("%s: can't create instance\n", name);
		return 0;
	}
}

void PromNode::close(PromInstanceHandle pih)
{
	PromInstance *pi = handleToInstance(pih);
	if (pi) {
		unregisterInstance(pih);
		delete pi;
	}
}

PromPackageHandle PromNode::getPHandle()
{
	return mPromNodeHandle;
}

PromInstance *PromNode::createInstance(const String &param)
{
	return NULL;
}

/*
 *
 */
PromProp::PromProp(const char *aName)
	:PromObject(aName)
{
}

ObjectID PromProp::getObjectID() const
{
	return ATOM_PROM_PROP;
}

uint32 PromProp::getValueLen()
{
	return 0;
}

uint32 PromProp::getValue(uint32 buf, uint32 buflen)
{
	return (uint32)-1;
}

uint32 PromProp::setValue(uint32 buf, uint32 buflen)
{
	return (uint32)-1;
}

/*
 *
 */
PromInstance::PromInstance(PromNode *type, const String &param)
{
	mType = type;
}

PromInstance::~PromInstance()
{
}
 
void PromInstance::setIHandle(PromInstanceHandle handle)
{
	mHandle = handle;
}

PromNode *PromInstance::getType()
{
	return mType;
}

void PromInstance::callMethod(const char *method, prom_args *pa)
{
	IO_PROM_ERR("%s: no method '%s'\n", mType->name, method);
}

uint32 PromInstance::write(uint32 buf, int length)
{
	IO_PROM_ERR("%s: write not implemented\n", mType->name);
	return 0;
}

uint32 PromInstance::read(uint32 buf, int length)
{
	IO_PROM_ERR("%s: read not implemented\n", mType->name);
	return 0;
}

uint32 PromInstance::seek(uint64 pos)
{
	IO_PROM_ERR("%s: seek not implemented\n", mType->name);
	return (uint32)-1;
}

/*
 *
 */
PromNodeATY::PromNodeATY(const char *name)
	:PromNode(name)
{
}

PromInstance *PromNodeATY::createInstance(const String &param)
{
	return new PromInstanceATY(this, param);
}
/*
 *
 */
PromInstanceATY::PromInstanceATY(PromNode *type, const String &param)
	:PromInstance(type, param)
{
}

void PromInstanceATY::callMethod(const char *method, prom_args *pa)
{
//	va->args[] = ;
	if (strcmp(method, "color!") == 0) {
		pa->args[6] = 0;
	} else if (strcmp(method, "draw-rectangle") == 0) {
		uint32 data = pa->args[6];
		uint32 x = pa->args[5];
		uint32 y = pa->args[4];
		uint32 width = pa->args[3];
		uint32 height = pa->args[2];
		uint32 bpp = gDisplay->mClientChar.bytesPerPixel;
		byte *f = gFrameBuffer + (y*gDisplay->mClientChar.width + x)*bpp;
		for (uint iy = 0; iy < height; iy++) {
			for (uint ix = 0; ix < width; ix++) {
				uint32 phys;
				ppc_prom_effective_to_physical(phys, data);
				byte v[4];
				ppc_dma_read(v, phys, bpp);
				for (uint i=0; i<bpp; i++) *(f++) = v[i];
/*				switch (bpp) {
				case 1: {
					uint8 v;
					ppc_read_effective_byte(data, v);
					*(f++) = v;
					break;
				}
				case 2: {
					uint16 v;
					ppc_read_effective_half(data, v);
					*(f++) = v>>8;
					*(f++) = v;
					break;
				}
				case 4: {
					uint32 v;
					ppc_read_effective_word(data, v);
					*(f++) = v>>24;
					*(f++) = v>>16;
					*(f++) = v>>8;
					*(f++) = v;
					break;
				}
				}*/
				data += bpp;
			}
			f += gDisplay->mClientChar.scanLineLength - width*bpp;
		}
		damageFrameBufferAll();
		pa->args[7] = 0;
	} else if (strcmp(method, "fill-rectangle") == 0) {
		uint32 color = pa->args[6];
		uint32 x = pa->args[5];
		uint32 y = pa->args[4];
		uint32 width = pa->args[3];
		uint32 height = pa->args[2];
		uint32 bpp = gDisplay->mClientChar.bytesPerPixel;
		uint32 f = y*(gDisplay->mClientChar.width + x)*bpp;
		for (uint iy=0; iy < height; iy++) {
			for (uint ix=0; ix < width; ix++) {
				if (bpp > 2) {
					gFrameBuffer[f++] = color >> 24;
					gFrameBuffer[f++] = color >> 16;
				}
				if (bpp > 1) gFrameBuffer[f++] = color >> 8;
				gFrameBuffer[f++] = color;
			}
			f += gDisplay->mClientChar.scanLineLength - width*bpp;
		}
		damageFrameBufferAll();
		pa->args[7] = 0;
	} else {
		IO_PROM_ERR("%s: %s not implemented\n", mType->name, method);
	}
}

uint32 PromInstanceATY::write(uint32 buf, int length)
{
	uint32 phys;
	ppc_prom_effective_to_physical(phys, buf);
	byte *mbuf = (byte *)malloc(length);
	ppc_dma_read(mbuf, phys, length);
	String s(mbuf, length);
	free(mbuf);
	gDisplay->printf("%y", &s);
	return length;
}

/*
 *
 */
PromNodeKBD::PromNodeKBD(const char *name)
	:PromNode(name)
{
}

PromInstance *PromNodeKBD::createInstance(const String &param)
{
	return new PromInstanceKBD(this, param);
}
/*
 *
 */
PromInstanceKBD::PromInstanceKBD(PromNode *type, const String &param)
	:PromInstance(type, param)
{
}

uint32 PromInstanceKBD::read(uint32 buf, int length)
{
	uint32 phys;

	if (ppc_prom_effective_to_physical(phys, buf)) {
		uint32 key;
		char chr;
		if (cuda_prom_get_key(key) 
		    && !(key & 0x80)	// ignore KeyUp events
		    && gKeyboard->adbKeyToAscii(chr, key)) 
		{
		  ppc_dma_write(phys, &chr, 1);
		  return 1;
		}
	}
	return 0;
}

/*
 *
 */
PromNodeRTAS::PromNodeRTAS(const char *name)
	:PromNode(name)
{
}

PromInstance *PromNodeRTAS::createInstance(const String &param)
{
	return new PromInstanceRTAS(this, param);
}

/*
 *
 */
PromInstanceRTAS::PromInstanceRTAS(PromNode *type, const String &param)
	:PromInstance(type, param)
{
}

uint32 PromInstanceRTAS::write(uint32 buf, int length)
{
//	String s((byte*)prom_ea_string(buf), length);
//	IO_PROM_TRACE("RTAS: '%y'\n", &s);
	return length;
}

void PromInstanceRTAS::callMethod(const char *method, prom_args *pa)
{
//	va->args[] = ;
	if (strcmp(method, "instantiate-rtas") == 0) {
	// instantiate-rtas(void *rtas_data, int ret1, int rtas_entry)
		pa->args[3] = 0;
		pa->args[4] = 0;
	} else {
	}	
}

/*
 *
 */
PromNodeMMU::PromNodeMMU(const char *name)
	:PromNode(name)
{
}

PromInstance *PromNodeMMU::createInstance(const String &param)
{
	return new PromInstanceMMU(this, param);
}

/*
 *
 */
PromInstanceMMU::PromInstanceMMU(PromNode *type, const String &param)
	:PromInstance(type, param)
{
}

void PromInstanceMMU::callMethod(const char *method, prom_args *pa)
{
	if (strcmp(method, "translate") == 0) {
		uint32 virt = pa->args[2];
		IO_PROM_TRACE("mmu->translate(%08x)\n", virt);
		uint32 phys;
		if (!ppc_prom_effective_to_physical(phys, virt)) {
			IO_PROM_ERR("translate failed\n");
		} else {
			pa->args[4] = 0;
			pa->args[5] = 0;
			pa->args[6] = 0;
			pa->args[7] = phys;
		}
		IO_PROM_TRACE("=%08x\n", phys);
//		gSinglestep = true;
		return;
	} else if (strcmp(method, "claim") == 0) {
		// claim virtual memory
		// mmu->claim(ihandle, "claim", virt, size, align, *addr);
		uint32 virt = pa->args[4];
		uint32 size UNUSED = pa->args[3];
		uint32 align UNUSED = pa->args[2];
		IO_PROM_TRACE("mmu->claim(%08x, %08x, %08x)\n", virt, size, align);
		pa->args[5] = 0;
		pa->args[6] = virt;
		return;
	} else if (strcmp(method, "map") == 0) {
		//
		uint32 phys = pa->args[5];
		uint32 virt = pa->args[4];
		uint32 size = pa->args[3];
		uint32 mode UNUSED = pa->args[2];
		IO_PROM_TRACE("mmu->map(%08x, %08x, %08x, %08x)\n", phys, virt, size, mode);
		for (uint32 p=phys; p<(phys+size); p+=4096) {
			ppc_prom_page_create(virt, p);
			virt += 4096;
		}
		pa->args[6] = 0;
		return;
	} else if (strcmp(method, "unmap") == 0) {
		uint32 virt UNUSED = pa->args[3];
		uint32 size UNUSED = pa->args[2];
		IO_PROM_TRACE("mmu->unmap(%08x, %08x)\n", virt, size);
		pa->args[4] = 0;
		return;	
	}		
	IO_PROM_ERR("mmu: %s not impl\n", method);
}

uint32 PromInstanceMMU::write(uint32 buf, int length)
{
	return 0;
}

/*
 *
 */
PromNodeMemory::PromNodeMemory(const char *name)
	:PromNode(name)
{
}

PromInstance *PromNodeMemory::createInstance(const String &param)
{
	return new PromInstanceMemory(this, param);
}

/*
 *
 */
PromInstanceMemory::PromInstanceMemory(PromNode *type, const String &param)
	:PromInstance(type, param)
{
}

void PromInstanceMemory::callMethod(const char *method, prom_args *pa)
{
	if (strcmp(method, "claim") == 0) {
		// claim physical memory
		// memory->claim(ihandle, "claim", virt, size, align, *addr);
		uint32 virt = pa->args[4];
		uint32 size = pa->args[3];
		uint32 align UNUSED = pa->args[2];
		IO_PROM_TRACE("memory->claim(%08x, %08x, %08x)\n", virt, size, align);
		if (!prom_claim_pages(virt, size)) {
			IO_PROM_ERR("memory->claim failed!\n");
		}
		pa->args[5] = 0;
		pa->args[6] = virt;
		return;
	}
	IO_PROM_ERR("memory: %s not impl\n", method);
}

/*
 *
 */
PromNodeDisk::PromNodeDisk(const char *name, int aNumber)
	:PromNode(name)
{
	mNumber = aNumber;

	IDEConfig *ic = ide_get_config(mNumber);
	File *rawFile = ic->device->promGetRawFile();
	pm = partitions_get_map(rawFile, ic->device->getBlockSize());
	delete rawFile;
	
	mDevice = NULL;
	mFS = NULL;

	String type;
	pm->getType(type);
}

PromNodeDisk::~PromNodeDisk()
{
	delete pm;
	delete mFS;
	delete mDevice;
}

PromInstance *PromNodeDisk::createInstance(const String &param)
{
	IO_PROM_TRACE("PromNodeDisk::createInstance, param='%s'\n", param.contentChar());
	String part, file;
	if (param.findFirstChar(',') != -1) {
		param.leftSplit(',', part, file);
	} else if (param.length()) {
		if ((param.firstChar() >= '0') && (param.firstChar() <= '9')) {
			part.assign(param);
		} else {
			file.assign(param);
		}
	}
	IO_PROM_TRACE("part = '%y', file = '%y'\n", &part, &file);
	uint32 partNumber;
	if (!part.toInt32(partNumber)) partNumber = 0;
	if (file.length() == 0) {
		return new PromInstanceDiskPart(this, partNumber);
	} else {
		if (partNumber == 0) {
			if (getPHandle() == gBootNodeID) {
				partNumber = gBootPartNum;	// FIXME: HACK. we don't know how to do this correctly...
			} else {
				IO_PROM_WARN("can't determine boot partition number\n");
				return NULL;
			}
		}
		PartitionEntry *pe = (PartitionEntry*)((*pm->getPartitions())[partNumber]);
		char *f = file.contentChar();
		file.translate("\\", "/");
		int flen = file.length();
		if (!pe || !pe->mInstantiateFileSystem) {
			IO_PROM_WARN("can't instantiate file system\n");
			return NULL;
		}
		// FIXME: HACK
		IDEConfig *ic = ide_get_config(mNumber);
		if (!mDevice) mDevice = ic->device->promGetRawFile();
		if (!mFS) mFS = pe->mInstantiateFileSystem(mDevice, partNumber);
		String filename;
		File *file = NULL;
		if ((flen >= 2) && ((f[0] == '/') && (f[1] == '/'))) {
			IO_PROM_TRACE("FS: in boot path\n");
			if (mFS->getBlessedPath(filename)) {
				filename.append(f+2);
			}
		} else {
			// FIXME: won't work
			filename.assign(f);
		}
		IO_PROM_TRACE("FS: opening '%y'\n", &filename);
		file = mFS->open(filename);
		if (file) {
			IO_PROM_TRACE("FS: file found!\n");
			return new PromInstanceDiskFile(this, file);
		} else {
			IO_PROM_TRACE("FS: file NOT found!\n");
		}
	}
	return NULL;
}

/*
 *
 */
PromInstanceDiskFile::PromInstanceDiskFile(PromNode *type, File *file)
	:PromInstance(type, "")
{
	mFile = file;
}

void PromInstanceDiskFile::callMethod(const char *method, prom_args *pa)
{
/*	if (strcmp(method, "block-size") == 0) {
		IO_PROM_TRACE("disk->block-size()\n");
		pa->args[2] = 0;
		IDEConfig *ic = ide_get_config(mNumber);
		pa->args[3] = ic->bps;
	} else if (strcmp(method, "dma-alloc") == 0) {
		uint32 size UNUSED = pa->args[4];
		uint32 a2 UNUSED = pa->args[3];
		uint32 a1 UNUSED = pa->args[2];
		IO_PROM_TRACE("disk->dma-alloc(%x, %x, %x)\n", a1, a2, size);
		pa->args[5] = 0;
		pa->args[6] = 1;
	} else if (strcmp(method, "dma-free") == 0) {
		uint32 size UNUSED = pa->args[5];
		uint32 addr UNUSED = pa->args[4];
		uint32 a2 UNUSED = pa->args[3];
		uint32 a1 UNUSED = pa->args[2];
		IO_PROM_TRACE("disk->dma-free(%x, %x, %x, %x)\n", a1, a2, addr, size);
		pa->args[6] = 0;
	} else {*/
		IO_PROM_ERR("diskFile:%s not impl.\n", method);
//	}
}

uint32 PromInstanceDiskFile::read(uint32 buf, int length)
{
	byte *buffer = (byte *)malloc(length);
	uint32 result = mFile->read(buffer, length);
	uint32 phys;
	ppc_prom_effective_to_physical(phys, buf);
	ppc_dma_write(phys, buffer, result);
	free(buffer);
    	return result;
}

uint32 PromInstanceDiskFile::write(uint32 buf, int length)
{
	IO_PROM_WARN("will not write to disk file..\n");
	return 0;
}

uint32 PromInstanceDiskFile::seek(uint64 pos)
{
	try {
	    	mFile->seek(pos);
	} catch (IOException *x) {
		delete x;
	}
	return 0;
}

/*
 *
 */
PromInstanceDiskPart::PromInstanceDiskPart(PromNode *type, uint partnum)
	:PromInstance(type, "")
{
	mNumber = ((PromNodeDisk *)type)->mNumber;

	PartitionEntry *pe = (PartitionEntry*)((*((PromNodeDisk *)type)->pm->getPartitions())[partnum]);
	mOffset = pe ? pe->mOffset : 0;
}

void PromInstanceDiskPart::callMethod(const char *method, prom_args *pa)
{
	if (strcmp(method, "block-size") == 0) {
		IO_PROM_TRACE("disk->block-size()\n");
		pa->args[2] = 0;
		IDEConfig *ic = ide_get_config(mNumber);
		pa->args[3] = ic->bps;
	} else if (strcmp(method, "dma-alloc") == 0) {
		uint32 size UNUSED = pa->args[4];
		uint32 a2 UNUSED = pa->args[3];
		uint32 a1 UNUSED = pa->args[2];
		IO_PROM_TRACE("disk->dma-alloc(%x, %x, %x)\n", a1, a2, size);
		pa->args[5] = 0;
		pa->args[6] = 1;
	} else if (strcmp(method, "dma-free") == 0) {
		uint32 size UNUSED = pa->args[5];
		uint32 addr UNUSED = pa->args[4];
		uint32 a2 UNUSED = pa->args[3];
		uint32 a1 UNUSED = pa->args[2];
		IO_PROM_TRACE("disk->dma-free(%x, %x, %x, %x)\n", a1, a2, addr, size);
		pa->args[6] = 0;
	} else {
		IO_PROM_ERR("partDisk:%s not impl.\n", method);
	}
}

uint32 PromInstanceDiskPart::read(uint32 buf, int length)
{
	IDEConfig *ic = ide_get_config(mNumber);
	byte *buffer = (byte *)malloc(length);
	uint32 result = ic->device->promRead(buffer, length);
	uint32 phys;
	ppc_prom_effective_to_physical(phys, buf);
	ppc_dma_write(phys, buffer, result);
	free(buffer);
    	return result;
}

uint32 PromInstanceDiskPart::write(uint32 buf, int length)
{
	IO_PROM_WARN("will not write to part disk..\n");
	return 0;
}

uint32 PromInstanceDiskPart::seek(uint64 pos)
{
	IDEConfig *ic = ide_get_config(mNumber);
	ic->device->promSeek(pos + mOffset);
	return 0;
}

/*
 *
 */
PromPropLink::PromPropLink(const char *aName, PromInstanceHandle aValue)
	:PromProp(aName)
{
	value = aValue;
}

PromPropLink::~PromPropLink()
{
}

uint32 PromPropLink::getValueLen()
{
	return 4;
}

uint32 PromPropLink::getValue(uint32 buf, uint32 buflen)
{
	uint32 phys;
	ppc_prom_effective_to_physical(phys, buf);
	uint32 v = ppc_word_to_BE(value);
	ppc_dma_write(phys, &v, 4);
	return 4;
}

/*
 *
 */
PromPropInt::PromPropInt(const char *aName, uint32 aValue)
	:PromProp(aName)
{
	value = aValue;
}

PromPropInt::~PromPropInt()
{
}

uint32 PromPropInt::getValueLen()
{
	return 4;
}

uint32 PromPropInt::getValue(uint32 buf, uint32 buflen)
{
	uint32 phys;
	ppc_prom_effective_to_physical(phys, buf);
	uint32 v = ppc_word_to_BE(value);
	ppc_dma_write(phys, &v, 4);
	return 4;
}

/*
 *
 */

PromPropMemory::PromPropMemory(const char *name, const void *aBuf, int aSize)
	:PromProp(name)
{
	size = aSize;
	buf = malloc(size);
	memcpy(buf, aBuf, size);
}

PromPropMemory::~PromPropMemory()
{
	if (buf) free(buf);
}

uint32 PromPropMemory::getValueLen()
{
	return buf ? size : 0;
}

uint32 PromPropMemory::getValue(uint32 aBuf, uint32 buflen)
{
	if (buf && buflen && aBuf) {
		uint32 s = MIN(buflen, (uint32)size);
		uint32 phys;
		ppc_prom_effective_to_physical(phys, aBuf);
		ppc_dma_write(phys, buf, s);
		return s;
	} else {
		return 0;
	}
}

uint32 PromPropMemory::setValue(uint32 aBuf, uint32 buflen)
{
	if (buf) free(buf);
	if (aBuf && buflen) {
		uint32 phys;
		ppc_prom_effective_to_physical(phys, aBuf);
		size = buflen;
		buf = malloc(size);
		ppc_dma_read(buf, phys, size);
		return size;
	} else {
		buf = NULL;
		return 0;
	}
}

/*
 *
 */
PromPropString::PromPropString(const char *aName, const char *aValue)
	:PromProp(aName)
{
	value = strdup(aValue);
}

PromPropString::~PromPropString()
{
	if (value) free(value);
}

uint32 PromPropString::getValueLen()
{
	return value ? (strlen(value)+1) : 0;
}

uint32 PromPropString::getValue(uint32 buf, uint32 buflen)
{
	if (value && buflen && buf) {
		buflen--;
		uint slen = strlen(value);
		uint s = MIN(buflen, slen);
        	uint32 phys;
		ppc_prom_effective_to_physical(phys, buf);
		ppc_dma_write(phys, value, s);
		byte null=0;
		ppc_dma_write(phys+s, &null, 1);
		return s+1;
	} else {
		return 0;
	}
}

uint32 PromPropString::setValue(uint32 buf, uint32 buflen)
{
	if (value) free(value);
	if (buf && buflen) {
		String bufchar;
		prom_get_string(bufchar, buf);
		value = (char*)malloc(buflen+1);
		memcpy(value, bufchar.contentChar(), buflen);
		value[buflen] = 0;
		return buflen;
	} else {
		value = NULL;
		return 0;
	}
}

/*******************************************************************************
 *
 */

PromNode *findDevice(const char *aPathName, int type, PromInstanceHandle *ret)
{
	// .39
	String pathname(aPathName);
	if (!pathname.length()) return NULL;
	String component, tmp, nodeaddr, arguments, nodename, unitaddr;
	if (!(pathname[0] == '/')) {
		PromNode *aliases = gPromRoot->findNode("aliases");
		if (aliases) {
			pathname.leftSplit('/', component, tmp);
			component.leftSplit(':', nodeaddr, arguments);
			nodeaddr.leftSplit('@', nodename, unitaddr);
			PromPropString *a = (PromPropString *)aliases->findProp(nodename);
			if (a) {
				pathname.assign(a->value);
				pathname += ((unitaddr!=(String)"")?("@"+unitaddr):(String)"")+":"+arguments;
			} else return NULL;
		} else return NULL;
	}
	PromNode *pn = gPromRoot;
	pathname.del(0, 1);
	while (pathname.length()) {
		pathname.leftSplit('/', component, tmp);
		pathname = tmp;
		component.leftSplit(':', nodeaddr, arguments);
		nodeaddr.leftSplit('@', nodename, unitaddr);
		pn = pn->findNode(nodename.contentChar());
		if (!pn) return NULL;		
	}
	if (type == FIND_DEVICE_OPEN) {
		*ret = pn->open(arguments);
	}
	return pn;
}

#define UINT32(v) ((uint32)(v)>>24),((uint32)(v)>>16),((uint32)(v)>>8),((uint8)(uint32)(v))
#define PHANDLE(v) UINT32((v)->getPHandle())

void prom_init_device_tree()
{
	gPromPackages = new AVLTree(true);
	gPromInstances = new AVLTree(true);
	
	// root
	gPromRoot = new PromNode("device-tree");

	// must have
	PromNode *aliases = new PromNode("aliases");
	PromNode *chosen = new PromNode("chosen");
	PromNode *cpus = new PromNode("cpus");
	PromNode *memory = new PromNodeMemory("memory");
	PromNode *openprom = new PromNode("openprom");
	PromNode *options = new PromNode("options");
	PromNode *packages = new PromNode("packages");
	PromNode *rtas = new PromNodeRTAS("rtas");
	PromNode *mmu = new PromNodeMMU("mmu");
	PromNode *kbd = new PromNodeKBD("keyboard");

	PromNode *rom = new PromNode("rom@ff800000");
	PromNode *pci = new PromNode("pci@80000000");

	gPromRoot->addProp(new PromPropString("model", EMULATOR_MODEL));
	gPromRoot->addProp(new PromPropString("compatible", "PowerMac1,2\0PowerMac1,1\0MacRISC\0Power Macintosh"));
	gPromRoot->addProp(new PromPropString("copyright", COPYRIGHT));
	gPromRoot->addProp(new PromPropString("device_type", "bootrom"));
	gPromRoot->addProp(new PromPropString("system-id", "42"));
	gPromRoot->addProp(new PromPropInt("#address-cells", 1));
	gPromRoot->addProp(new PromPropInt("#size-cells", 1));
	gPromRoot->addProp(new PromPropInt("clock-frequency", 1));
	
	gPromRoot->addNode(aliases);
	gPromRoot->addNode(chosen);
	gPromRoot->addNode(cpus);
	gPromRoot->addNode(kbd);

	PromNode *cpu = new PromNode("PowerPC,G4");
	cpus->addNode(cpu);
	cpus->addProp(new PromPropInt("#size-cells", 0));
	cpu->addProp(new PromPropString("device_type", "cpu"));
	cpu->addProp(new PromPropInt("reg", 0));
	cpu->addProp(new PromPropInt("cpu-version", ppc_cpu_get_pvr(0)));
	cpu->addProp(new PromPropString("state", "running"));
	cpu->addProp(new PromPropInt("clock-frequency", ppc_get_clock_frequency(0)));
	cpu->addProp(new PromPropInt("timebase-frequency", ppc_get_timebase_frequency(0)));
	cpu->addProp(new PromPropInt("bus-frequency", ppc_get_bus_frequency(0)));
	cpu->addProp(new PromPropInt("reservation-granule-size", 0x20));
	cpu->addProp(new PromPropInt("tlb-sets", 0x40));
	cpu->addProp(new PromPropInt("tlb-size", 0x80));
	cpu->addProp(new PromPropInt("d-cache-size", 0x8000));
	cpu->addProp(new PromPropInt("i-cache-size", 0x8000));
	cpu->addProp(new PromPropInt("d-cache-sets", 0x80));
	cpu->addProp(new PromPropInt("i-cache-sets", 0x80));
	cpu->addProp(new PromPropInt("i-cache-block-size", 0x20));
	cpu->addProp(new PromPropInt("d-cache-block-size", 0x20));
	cpu->addProp(new PromPropString("graphics", ""));
	cpu->addProp(new PromPropString("performance-monitor", ""));
	cpu->addProp(new PromPropString("data-streams", ""));
	
	PromNode *cache = new PromNode("cache");
	cpu->addProp(new PromPropInt("l2-cache", cache->getPHandle()));
	cache->addProp(new PromPropString("device_type", "cache"));
	cache->addProp(new PromPropInt("i-cache-size", 0x100000));
	cache->addProp(new PromPropInt("d-cache-size", 0x100000));
	cache->addProp(new PromPropInt("i-cache-sets", 0x2000));
	cache->addProp(new PromPropInt("d-cache-sets", 0x2000));
	cache->addProp(new PromPropInt("i-cache-line-size", 0x40));
	cache->addProp(new PromPropInt("d-cache-line-size", 0x40));
//	cache->addProp(new PromPropString("cache-unified", ""));
	cpu->addNode(cache);
    	
	gPromRoot->addNode(memory);
	gPromRoot->addNode(openprom);
	openprom->addProp(new PromPropString("model", "OpenFirmware3"));
	openprom->addProp(new PromPropString("relative-addressing", ""));
	gPromRoot->addNode(options);
	gPromRoot->addNode(packages);
	gPromRoot->addNode(pci);
	gPromRoot->addNodeShort("pci", "pci@80000000");
	gPromRoot->addNode(rom);
	gPromRoot->addNodeShort("rom", "rom@ff800000");	
	byte regrom[] = {0xff,0x80,0x00,0x00, 0x00,0x00,0x00,0x00};
	rom->addProp(new PromPropMemory("reg", &regrom, sizeof regrom));
	byte rangesrom[] = {0xff,0x80,0x00,0x00, 0x00,0x80,0x00,0x00, 0xff,0x80,0x00,0x00};
	rom->addProp(new PromPropMemory("ranges", &rangesrom, sizeof rangesrom));
	rom->addProp(new PromPropInt("#address-cells", 1));
	PromNode *bootrom = new PromNode("boot-rom@fff00000");
	rom->addNode(bootrom);
	rom->addNodeShort("boot-rom", "boot-rom@fff00000");
	byte regbootrom[] = {0xff,0xf0,0x00,0x00, 0x00,0x10,0x00,0x00};
	bootrom->addProp(new PromPropMemory("reg", &regbootrom, sizeof regbootrom));	
	bootrom->addProp(new PromPropString("write-characteristic", "flash"));
//	bootrom->addProp(new PromPropString("model", EMULATOR_MODEL" BootROM"));
	bootrom->addProp(new PromPropString("BootROM-version", "f2"));
	bootrom->addProp(new PromPropInt("result", 0));
	byte bootrominfo[] = {
	0xff,0xf0,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x01,0x12,0xf2,0x19,0x99,0x08,0x19,
	0x94,0x4e,0x73,0x27,0xff,0xf0,0x80,0x00,0x00,0x07,0x80,0x01,0x00,0x01,0x12,0xf2,
	0x19,0x99,0x08,0x19,0xd7,0xf3,0xfc,0x17,0xff,0xf8,0x00,0x00,0x00,0x08,0x00,0x02,
	0x00,0x01,0x12,0xf2,0x19,0x99,0x08,0x19,0xbb,0x10,0xfc,0x17};
	bootrom->addProp(new PromPropMemory("info", &bootrominfo, sizeof bootrominfo));
	
	gPromRoot->addNode(rtas);
	gPromRoot->addNode(mmu);
	memory->addProp(new PromPropString("device_type", "memory"));
	uint32 memorySize = ppc_get_memory_size();
	byte reg[] = {0, 0, 0, 0, memorySize>>24, memorySize>>16, memorySize>>8, memorySize, 0, 0, 0, 0, 0, 0, 0, 0};
	memory->addProp(new PromPropMemory("reg", &reg, sizeof reg));
	byte av[] = {0, 0xa0, 0, 0, 1, 0x40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	memory->addProp(new PromPropMemory("available", &av, sizeof av));
	PromNode *pic = new PromNode("interrupt-controller@10");
	PromNode *bridge = new PromNode("pci-bridge@d");
	pci->addNode(bridge);
	pci->addNodeShort("pci-bridge", "pci-bridge@d");
	pci->addProp(new PromPropString("device_type", "pci"));	
	pci->addProp(new PromPropString("model", "MOT,MPC106"));
	pci->addProp(new PromPropString("compatible", "grackle"));
	pci->addProp(new PromPropString("built-in", ""));
	pci->addProp(new PromPropString("used-by-rtas", ""));
	byte reg47[] = {0x80,0x00,0x00,0x00, 0x7f,0x00,0x00,0x00};
	pci->addProp(new PromPropMemory("reg", &reg47, sizeof reg47));
	pci->addProp(new PromPropInt("#address-cells", 3));
	pci->addProp(new PromPropInt("#interrupt-cells", 1));
	pci->addProp(new PromPropInt("#size-cells", 2));
	pci->addProp(new PromPropInt("clock-frequency", ppc_get_bus_frequency(0)));
	pci->addProp(new PromPropInt("bus-master-capable", 0x12000));
	byte ranges23[] = {
	0x01,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0xfe,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x80,0x00,0x00, 0x02,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0xfd,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x01,0x00,0x00,0x00,
	0x02,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x80,0x00,0x00,0x00, 0x80,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x7d,0x00,0x00,0x00
	};
	pci->addProp(new PromPropMemory("ranges", &ranges23, sizeof ranges23));
	byte busranges23[] = {0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x01};
	pci->addProp(new PromPropMemory("bus-range", &busranges23, sizeof busranges23));
	byte interruptmap23[] = {
	// aty
	0x00,0x00,0x38,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	PHANDLE(pic),        0x00,0x00,0x00,IO_PIC_IRQ_GCARD};
	pci->addProp(new PromPropMemory("interrupt-map", &interruptmap23, sizeof interruptmap23));
	byte interruptmapmask23[] = {
	0x00,0x00,0xf8,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00};
	pci->addProp(new PromPropMemory("interrupt-map-mask", &interruptmapmask23, sizeof interruptmapmask23));

	PromNode *macio = new PromNode("mac-io@5");
	bridge->addNode(macio);
	bridge->addNodeShort("macio", "mac-io@5");
	bridge->addProp(new PromPropInt("vendor-id", 0x1011));
	bridge->addProp(new PromPropInt("device-id", 0x0026));
	bridge->addProp(new PromPropInt("revision-id", 2));
	bridge->addProp(new PromPropInt("class-code", 0x60400));
	bridge->addProp(new PromPropInt("devsel-speed", 1));
	bridge->addProp(new PromPropString("fast-back-to-back", ""));
	bridge->addProp(new PromPropString("device_type", "pci"));
	byte reg5[] = {
	//   bus  dev
	0x00,0x00,0x68,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00};
	bridge->addProp(new PromPropMemory("reg", &reg5, sizeof reg5));
	bridge->addProp(new PromPropInt("#address-cells", 3));
	bridge->addProp(new PromPropInt("#size-cells", 2));
	bridge->addProp(new PromPropInt("#interrupt-cells", 1));
	bridge->addProp(new PromPropInt("clock-frequency", ppc_get_bus_frequency(0)));
	bridge->addProp(new PromPropString("model", "DEC,21154"));
	bridge->addProp(new PromPropString("compatible", "DEC,21154.pci-bridge"));
	bridge->addProp(new PromPropInt("bus-master-capable", 0x7f));
	byte ranges22[] = {
	0x82,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x80,0x80,0x00,0x00, 
	0x82,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x80,0x80,0x00,0x00, 
	0x00,0x00,0x00,0x00, 0x00,0x20,0x00,0x00, 
	0x81,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x10,0x00, 
	0x81,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x10,0x00, 
	0x00,0x00,0x00,0x00, 0x00,0x00,0x10,0x00};
	bridge->addProp(new PromPropMemory("ranges", &ranges22, sizeof ranges22));

	// FIXME: why are not all devices described here? (e.g. gcard, cuda?)
	byte interruptmap22[] = {
/*	//
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	PHANDLE(pic),       0x00,0x00,0x00,0x15,
	//
	0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	PHANDLE(pic),       0x00,0x00,0x00,0x17,*/
	// ide
	0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	PHANDLE(pic),       0x00,0x00,0x00,IO_PIC_IRQ_IDE0,
	// macio
	0x00,0x00,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	PHANDLE(pic),       0x00,0x00,0x00,0x18,
	// eth0
	0x00,0x00,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	PHANDLE(pic),       0x00,0x00,0x00,IO_PIC_IRQ_ETHERNET0,
	// eth1
	0x00,0x00,0x68,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	PHANDLE(pic),       0x00,0x00,0x00,IO_PIC_IRQ_ETHERNET1,
	// usb
	0x00,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	PHANDLE(pic),       0x00,0x00,0x00,IO_PIC_IRQ_USB
	};

	bridge->addProp(new PromPropMemory("interrupt-map", &interruptmap22, sizeof interruptmap22));
	byte interruptmapmask22[] = {
	0x00,0x00,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	bridge->addProp(new PromPropMemory("interrupt-map-mask", &interruptmapmask22, sizeof interruptmapmask22));
	
	PromNode *pci_ata = new PromNode("pci-ata@1");
	bridge->addNode(pci_ata);
	bridge->addNodeShort("pci-ata", "pci-ata@1");
	pci_ata->addProp(new PromPropInt("vendor-id", 0x1095));
	pci_ata->addProp(new PromPropInt("device-id", 0x0646));
	pci_ata->addProp(new PromPropInt("revision-id", 7));
	pci_ata->addProp(new PromPropInt("interrupts", 1));
	pci_ata->addProp(new PromPropInt("class-code", 0x1018f));
	pci_ata->addProp(new PromPropString("device_type", "pci-ide"));
	pci_ata->addProp(new PromPropInt("#address-cells", 1));
	pci_ata->addProp(new PromPropInt("#size-cells", 0));
	byte reg2010[] = {
	0x00,0x01,0x08,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 
	0x01,0x01,0x08,0x10, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x10, 
	0x01,0x01,0x08,0x14, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x10, 
	0x01,0x01,0x08,0x18, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x10, 
	0x01,0x01,0x08,0x1c, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x10, 
	0x01,0x01,0x08,0x20, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 
	0x00,0x00,0x00,0x10};
	pci_ata->addProp(new PromPropMemory("reg", &reg2010, sizeof reg2010));
	byte aa2010[] = {
	0x81,0x01,0x08,0x10, 0x00,0x00,0x00,0x00, 0x00,0x00,0x1c,0x40, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x10, 
	0x81,0x01,0x08,0x14, 0x00,0x00,0x00,0x00, 0x00,0x00,0x1c,0x30, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x10, 
	0x81,0x01,0x08,0x18, 0x00,0x00,0x00,0x00, 0x00,0x00,0x1c,0x20, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x10,
	0x81,0x01,0x08,0x1c, 0x00,0x00,0x00,0x00, 0x00,0x00,0x1c,0x10, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x10,
	0x81,0x01,0x08,0x20, 0x00,0x00,0x00,0x00, 0x00,0x00,0x1c,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x10};	
	pci_ata->addProp(new PromPropMemory("assigned-addresses", &aa2010, sizeof aa2010));
	IDEConfig *ic[2] = {ide_get_config(0), ide_get_config(1)};
	if (ic[0]->installed || ic[1]->installed) {
		PromNode *ata4 = new PromNode("ata-4");
		pci_ata->addNode(ata4);
		ata4->addProp(new PromPropString("device_type", "ata"));
		ata4->addProp(new PromPropInt("#address-cells", 1));
		ata4->addProp(new PromPropInt("#size-cells", 0));
		ata4->addProp(new PromPropInt("reg", 0));
		ata4->addProp(new PromPropString("compatible", "cmd646-ata"));
		if (ic[0]->installed) {
			PromNode *disk1 = new PromNodeDisk("disk0@0", 0);
			ata4->addNode(disk1);
			ata4->addNodeShort("disk", "disk0@0");
			ata4->addNodeShort("disk0", "disk0@0");
			disk1->addProp(new PromPropInt("device-id", 0));
			disk1->addProp(new PromPropInt("reg", 0));
			disk1->addProp(new PromPropString("device_type", "block"));
	    		disk1->addProp(new PromPropString("category", "hd"));
		}
		if (ic[1]->installed) {
			PromNode *disk2 = new PromNodeDisk("disk1@1", 1);
			ata4->addNode(disk2);
			ata4->addNodeShort("disk1", "disk1@1");
			disk2->addProp(new PromPropInt("device-id", 1));
			disk2->addProp(new PromPropInt("reg", 1));
			disk2->addProp(new PromPropString("device_type", "block"));
			disk2->addProp(new PromPropString("category", "hd"));
		}
	}
	macio->addProp(new PromPropString("device_type", "mac-io"));
	macio->addProp(new PromPropInt("vendor-id", 0x106b));
	macio->addProp(new PromPropInt("device-id", 0x0017));
	macio->addProp(new PromPropInt("revision-id", 0));
	macio->addProp(new PromPropInt("class-code", 0xff0000));
	macio->addProp(new PromPropString("model", "AAPL,343S1211"));
	macio->addProp(new PromPropMemory("compatible", "paddington\0heathrow", 19));
	byte reg4[] = {
	//   bus   dev
	0x00,0x01,0x28,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,

	0x02,0x01,0x28,0x10, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x08,0x00,0x00};
	macio->addProp(new PromPropMemory("reg", &reg4, sizeof reg4));
	byte aa2[] = {
	0x82,0x01,0x28,0x10, 0x00,0x00,0x00,0x00, 0x80,0x80,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x08,0x00,0x00};
	macio->addProp(new PromPropMemory("assigned-addresses", &aa2, sizeof aa2));
	byte ranges2[] = {
	0x00,0x00,0x00,0x00,0x82,0x01,0x28,0x10,0x00,0x00,0x00,0x00,0x80,0x80,0x00,0x00,
	0x00,0x08,0x00,0x00};
	macio->addProp(new PromPropMemory("ranges", &ranges2, sizeof ranges2));
	macio->addProp(new PromPropInt("#address-cells", 1));
	macio->addProp(new PromPropInt("#size-cells", 1));
/*	
	PromNode *ata3 = new PromNode("ata-3");
	macio->addNode(ata3);
	ata3->addProp(new PromPropString("device_type", "ata"));
	ata3->addProp(new PromPropString("compatible", "heathrow-ata"));
	ata3->addProp(new PromPropInt("#address-cells", 1));
	ata3->addProp(new PromPropInt("#size-cells", 0));
	byte regata3[] = {
	0x00,0x02,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x8b,0x00,0x00,0x00,0x01,0x00};
	ata3->addProp(new PromPropMemory("reg", &regata3, sizeof regata3));
	byte interrupts2[] = {0, 0, 0, 0xd, 0, 0, 0, 2};
	ata3->addProp(new PromPropMemory("interrupts", &interrupts2, sizeof interrupts2));
	PromNode *disk2 = new PromNode("disk");
	ata3->addNode(disk2);
	disk2->addProp(new PromPropInt("device-id", 0));
	disk2->addProp(new PromPropInt("reg", 0));
	disk2->addProp(new PromPropString("device_type", "block"));
	disk2->addProp(new PromPropString("category", "hd")); 
*/
	macio->addNode(pic);
	macio->addNodeShort("interrupt-controller", "interrupt-controller@10");
	pic->addProp(new PromPropString("device_type", "interrupt-controller"));
	pic->addProp(new PromPropMemory("compatible", "heathrow\0mac-risc\0", 18));
	byte reg2[] = {0, 0, 0, 0x10, 0, 0, 0, 0x20};
	pic->addProp(new PromPropMemory("reg", &reg2, sizeof reg2));
	pic->addProp(new PromPropInt("#interrupt-cells", 1));
	pic->addProp(new PromPropString("interrupt-controller", ""));
	PromNode *via = new PromNode("via-cuda@16000");
	macio->addNode(via);
	macio->addNodeShort("via-cuda", "via-cuda@16000");
	PromNode *adb = new PromNode("adb");
	via->addNode(adb);
	adb->addProp(new PromPropString("device_type", "adb"));
	PromNode *keyboard = new PromNode("keyboard"); 
	adb->addNode(keyboard);
	keyboard->addProp(new PromPropString("device_type", "keyboard"));
	PromNode *mouse = new PromNode("mouse"); 
	adb->addNode(mouse);
	mouse->addProp(new PromPropString("device_type", "mouse"));
	mouse->addProp(new PromPropInt("#buttons", 3));
	mouse->addProp(new PromPropInt("reg", 3));
//	byte regmouse[] = {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
//	mouse->addProp(new PromPropMemory("reg", &regmouse, sizeof regmouse));
	via->addProp(new PromPropString("compatible", "cuda"));
	via->addProp(new PromPropString("device_type", "via-cuda"));

	byte reg3[] = {0x00,0x01,0x60,0x00, 0x00,0x00,0x20,0x00};
	via->addProp(new PromPropMemory("reg", &reg3, sizeof reg3));

	via->addProp(new PromPropInt("interrupts", 0x12));
	via->addProp(new PromPropLink("interrupt-parent", pic->getPHandle()));
	PromNode *rtc = new PromNode("rtc");
	via->addNode(rtc);
	rtc->addProp(new PromPropString("device_type", "rtc"));
	PromNode *nvram = new PromNode("nvram@60000");
	macio->addNode(nvram);
	macio->addNodeShort("nvram", "nvram@60000");
	nvram->addProp(new PromPropString("device_type", "nvram"));
	byte regnv[] = {0x00,0x06,0x00,0x00,0x00,0x02,0x00,0x00};
	nvram->addProp(new PromPropMemory("reg", &regnv, sizeof regnv));
	nvram->addProp(new PromPropInt("#bytes", 0x2000));
	PromNode *powermgt = new PromNode("power-mgt");
	macio->addNode(powermgt);
	powermgt->addProp(new PromPropString("device_type", "power-mgt"));
	powermgt->addProp(new PromPropString("compatible", "cuda"));
	powermgt->addProp(new PromPropString("mgt-kind", "min-consumption-pwm-led"));
	byte regmgt[] = {
	0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00};
	powermgt->addProp(new PromPropMemory("reg", &regmgt, sizeof regmgt));

	/*
	 *	Eth0
	 */
	if (_3c90x_installed) {
		PromNode *eth0 = new PromNode("pci10b7,9200@4");
		bridge->addNode(eth0);
		bridge->addNodeShort("pci10b7,9200", "pci10b7,9200@4");
		eth0->addProp(new PromPropMemory("compatible", 
		"pci10b7,9200\x00pciclass,020000\x00", 29));
//		eth0->addProp(new PromPropString("device_type", "network"));
		eth0->addProp(new PromPropInt("vendor-id", 0x10b7));
		eth0->addProp(new PromPropInt("device-id", 0x9200));
		eth0->addProp(new PromPropInt("revision-id", 0x0));
		eth0->addProp(new PromPropInt("class-code", 0x020000));
		eth0->addProp(new PromPropInt("interrupts", 1));
		eth0->addProp(new PromPropInt("min-grant", 0));
		eth0->addProp(new PromPropInt("max-latency", 0));
		eth0->addProp(new PromPropInt("subsystem-vendor-id", 0x10b7));
		eth0->addProp(new PromPropInt("subsystem-id", 0x9200));
		eth0->addProp(new PromPropInt("devsel-speed", 1));
		eth0->addProp(new PromPropString("fast-back-to-back", ""));

		byte eth0reg[] = {
		0x00,0x01,0x60,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x01,0x01,0x60,0x10, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
		0x00,0x00,0x01,0x00,
		};
		eth0->addProp(new PromPropMemory("reg", &eth0reg, sizeof eth0reg));

		byte eth0aa[] = {
		0x81,0x01,0x60,0x10,
		0x00,0x00,0x00,0x00,
		/* 4 bytes address from 3c90x/3c90x.cc: */ 0x00,0x00,0x10,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x01,0x00,
		};
		eth0->addProp(new PromPropMemory("assigned-addresses", &eth0aa, sizeof eth0aa));
	}

	/*
	 *	Eth1
	 */
	if (rtl8139_installed) {
		PromNode *eth1 = new PromNode("pci10ec,8139@4");
		bridge->addNode(eth1);
		bridge->addNodeShort("pci10ec,8139", "pci10ec,8139@4");
		eth1->addProp(new PromPropMemory("compatible", 
			"pci10ec,8139\x00pciclass,020000\x00", 29));
//		eth1->addProp(new PromPropString("device_type", "network"));
		eth1->addProp(new PromPropInt("vendor-id", 0x10ec));
		eth1->addProp(new PromPropInt("device-id", 0x8139));
		eth1->addProp(new PromPropInt("revision-id", 0x0));
		eth1->addProp(new PromPropInt("class-code", 0x020000));
		eth1->addProp(new PromPropInt("interrupts", 1));
		eth1->addProp(new PromPropInt("min-grant", 0));
		eth1->addProp(new PromPropInt("max-latency", 0));
		eth1->addProp(new PromPropInt("subsystem-vendor-id", 0x10ec));
		eth1->addProp(new PromPropInt("subsystem-id", 0x8139));
		eth1->addProp(new PromPropInt("devsel-speed", 1));
		eth1->addProp(new PromPropString("fast-back-to-back", ""));

		byte eth1reg[] = {
		0x00,0x01,0x68,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,

		0x01,0x01,0x68,0x10,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x01,0x00,
		};
		eth1->addProp(new PromPropMemory("reg", &eth1reg, sizeof eth1reg));

		byte eth1aa[] = {
		0x81,0x01,0x68,0x10,
		0x00,0x00,0x00,0x00,
		/* 4 bytes address from rtl8139/rtl8139.cc: */ 0x00,0x00,0x18,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x01,0x00,
		};
		eth1->addProp(new PromPropMemory("assigned-addresses", &eth1aa, sizeof eth1aa));
	}

	/*
	 *	USB
	 */
	PromNode *usb = new PromNode("usb@6");
	bridge->addNode(usb);
	bridge->addNodeShort("usb", "usb@6");
	usb->addProp(new PromPropInt("vendor-id", 0x1045));
	usb->addProp(new PromPropInt("device-id", 0xc861));
	usb->addProp(new PromPropInt("revision-id", 0x10));
	usb->addProp(new PromPropInt("class-code", 0x0c0310));
	usb->addProp(new PromPropInt("interrupts", 1));
	usb->addProp(new PromPropInt("min-grant", 0));
	usb->addProp(new PromPropInt("max-latency", 0));
	usb->addProp(new PromPropInt("subsystem-vendor-id", 0x1045));
	usb->addProp(new PromPropInt("subsystem-id", 0xc861));
	usb->addProp(new PromPropInt("devsel-speed", 1));
	usb->addProp(new PromPropString("fast-back-to-back", ""));
	usb->addProp(new PromPropString("device_type", "usb"));
	byte usbreg[] = {
	0x00,0x01,0x30,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,

	0x02,0x01,0x30,0x10,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x10,0x00
	};

	usb->addProp(new PromPropMemory("reg", &usbreg, sizeof usbreg));
	byte usbaa[] = {
	0x82,0x01,0x30,0x10, 0x00,0x00,0x00,0x00,
	/* 4 bytes address from usb/usb.c:*/ 0x80,0x88,0x10,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x00,0x10,0x00};

	usb->addProp(new PromPropInt("#address-cells", 1));
	usb->addProp(new PromPropInt("#size-cells", 0));
	usb->addProp(new PromPropMemory("compatible", 
		"pci1045,c861\x00pciclass,0c0310\x00", 29));
	usb->addProp(new PromPropMemory("assigned-addresses", &usbaa, sizeof usbaa));

	PromNode *usbhub = new PromNode("hub@1");
	usb->addNode(usbhub);
	usb->addNodeShort("hub", "hub@1");
	usbhub->addProp(new PromPropInt("assigned-addresses", 1));
	usbhub->addProp(new PromPropString("device_type", "hub"));
	usbhub->addProp(new PromPropInt("#address-cells", 1));
	usbhub->addProp(new PromPropInt("#size-cells", 0));

	/*
	 *	Video
	 */
	PromNode *aty = new PromNodeATY("PearPCVideo");
	pci->addNode(aty);
//	aty->addProp(new PromPropInt("vendor-id", 0x1002));
//	aty->addProp(new PromPropInt("device-id", 0x5245));
//	aty->addProp(new PromPropInt("revision-id", 0));
	aty->addProp(new PromPropInt("interrupts", 1));
	aty->addProp(new PromPropInt("mol-irq", IO_PIC_IRQ_GCARD));
	aty->addProp(new PromPropInt("vendor-id", 0x6666));
	aty->addProp(new PromPropInt("device-id", 0x6666));
	aty->addProp(new PromPropInt("revision-id", 0x0));
	aty->addProp(new PromPropInt("class-code", 0x30000));
	aty->addProp(new PromPropString("device_type", "display"));
	aty->addProp(new PromPropString("model", "PearPC,display"));
	aty->addProp(new PromPropString("Ignore VBL", "yes"));
	aty->addProp(new PromPropInt("width", gDisplay->mClientChar.width));
	aty->addProp(new PromPropInt("height", gDisplay->mClientChar.height));
	aty->addProp(new PromPropInt("depth", gDisplay->mClientChar.bytesPerPixel * 8));
	aty->addProp(new PromPropInt("linebytes", gDisplay->mClientChar.width * gDisplay->mClientChar.bytesPerPixel));
	aty->addProp(new PromPropString("character-set", "ISO8859-1"));
/*
	byte atyreg[] = {
	0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x02,0x00,0x80,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x42,0x00,0x80,0x10,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x02,0x00,0x80,0x18,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,
	};
	aty->addProp(new PromPropMemory("reg", &atyreg, sizeof atyreg));
	byte assigned_addresses[] = {
	0xc2,0x00,0x80,0x10, 0x00,0x00,0x00,0x00, 0x84,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x04,0x00,0x00,0x00, 0x82,0x00,0x80,0x30, 0x00,0x00,0x00,0x00, 0x80,0xa2,0x00,0x00,
	0x00,0x00,0x00,0x00, 0x00,0x02,0x00,0x00, 0x82,0x00,0x80,0x18, 0x00,0x00,0x00,0x00,
	0x80,0xa0,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x40,0x00};
	aty->addProp(new PromPropMemory("assigned-addresses", &assigned_addresses, sizeof assigned_addresses));
*/	
	aty->addProp(new PromPropInt("address", IO_GCARD_FRAMEBUFFER_PA_START));
	byte atyreg[] = {
	0x00,0x00,0x38,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00, 
	0x02,0x00,0x38,0x10, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
	0x01,0x00,0x00,0x00,
	};
	aty->addProp(new PromPropMemory("reg", &atyreg, sizeof atyreg));
	byte assigned_addresses[] = {
	0x82,0x00,0x38,0x10, 0x00,0x00,0x00,0x00, UINT32(IO_GCARD_FRAMEBUFFER_PA_START),
	0x00,0x00,0x00,0x00, 0x01,0x00,0x00,0x00,
	};
	aty->addProp(new PromPropMemory("assigned-addresses", &assigned_addresses, sizeof assigned_addresses));

	aliases->addProp(new PromPropString("pci", "/pci"));
	aliases->addProp(new PromPropString("bridge", "/pci/pci-bridge"));
	aliases->addProp(new PromPropString("macio", "/pci/pci-bridge/macio"));
	aliases->addProp(new PromPropString("screen", "/pci@80000000/PearPCVideo"));
	int cdromcount=0, hdcount=0;
	for (int i=0; i<2; i++) {
		if (ic[i]->installed) {
			String alias, location;
			if (ic[i]->protocol == IDE_ATA) {
				alias.assignFormat("disk%d", hdcount);
				location.assignFormat("/pci/pci-bridge/pci-ata/ata-4/disk%d@%d", i, i);
				aliases->addProp(new PromPropString(alias, location));
				if (!hdcount) {
					aliases->addProp(new PromPropString("disk", location));
					aliases->addProp(new PromPropString("hd", location));
				}
				hdcount++;
			} else {
				alias.assignFormat("cdrom%d", cdromcount);
				location.assignFormat("/pci/pci-bridge/pci-ata/ata-4/disk%d@%d", i, i);
				aliases->addProp(new PromPropString(alias, location));
				if (!cdromcount) {
					aliases->addProp(new PromPropString("cdrom", location));
					aliases->addProp(new PromPropString("cd", location));
				}
				cdromcount++;
			}
		}
	}
	// insert links
//	chosen->addProp(new PromPropString("bootpath", "cd:,ofwboot"));
//	chosen->addProp(new PromPropString("bootpath", "disk:,ofwboot"));
//	chosen->addProp(new PromPropString("bootargs", "ide=nodma root=/dev/hda1"));
//	chosen->addProp(new PromPropString("bootargs", "/3.4/macppc/bsd.rd"));
	
	PromInstanceHandle pih = mmu->open("");
	chosen->addProp(new PromPropLink("mmu", pih));
	pih = aty->open("");
	chosen->addProp(new PromPropLink("stdout", pih));
	pih = kbd->open("");
	chosen->addProp(new PromPropLink("stdin", pih));
	pih = memory->open("");
	chosen->addProp(new PromPropLink("memory", pih));
}

void prom_done_device_tree()
{
	if (gPromRoot) {
		delete gPromRoot;
		gPromRoot = NULL;
	}
}
