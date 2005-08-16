/*
 *	PearPC
 *	promosi.cc
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

#include "debug/tracers.h"
#include "system/display.h"
#include "system/arch/sysendian.h"
#include "cpu/cpu.h"
#include "cpu/mem.h"
#include "system/sysclk.h"
#include "promdt.h"
#include "prommem.h"
#include "promosi.h"

uint32 gPromOSIEntry;

void prom_service_start_cpu(prom_args *pa)
{
	IO_PROM_ERR("start_cpu()\n");
}

void prom_service_quiesce(prom_args *pa)
{
	IO_PROM_TRACE("quiesce()\n");
	prom_mem_done();
}

/*
 *	.65
 *	Missing is 0 if the service name exists, and -1 if it does not exist.
 */
void prom_service_test(prom_args *pa)
{
	//; of_test(const char *name, int *missing)
	String name;
	prom_get_string(name, pa->args[0]);
	IO_PROM_ERR("test('%y')\n", &name);
}

/*
 *	.66
 *
 */
void prom_service_peer(prom_args *pa)
{
	//; of_peer(int phandle, int *sibling_phandle)
	uint32 phandle = pa->args[0];
	IO_PROM_TRACE("peer(%08x)\n", phandle);
	if (!phandle) {
		pa->args[1] = gPromRoot ? gPromRoot->getPHandle() : 0;
	} else {
		PromNode *p = handleToPackage(phandle);
		if (p) {
			if (!p->owner) {
				pa->args[1] = 0;
			} else {
				p = p->owner->nextChild(p);
				pa->args[1] = p ? p->getPHandle() : 0;
			}
		} else {
			pa->args[1] = 0xffffffff;
		}
	}
	IO_PROM_TRACE("= %08x\n", pa->args[1]);
}
void prom_service_child(prom_args *pa)
{
	//; (int phandle, int *child_phandle)
	uint32 phandle = pa->args[0];
	PromNode *p = handleToPackage(phandle);
	IO_PROM_TRACE("child(%08x, '%s')\n", phandle, p->name);
	PromNode *pn = p ? p->firstChild() : NULL;
	if (pn) {
		pa->args[1] = pn->getPHandle();
	} else {
		pa->args[1] = 0;
	}
	IO_PROM_TRACE("= %08x\n", pa->args[1]);
}
void prom_service_parent(prom_args *pa)
{
	//; of_parent(int phandle, int *parent_phandle)
	uint32 phandle = pa->args[0];
	PromNode *p = handleToPackage(phandle);
	IO_PROM_TRACE("parent(%08x)\n", phandle);
	PromNode *owner = p ? p->owner : NULL;
	pa->args[1] = owner ? owner->getPHandle() : 0;
	IO_PROM_TRACE("= %08x\n", pa->args[1]);
}
void prom_service_instance_to_package(prom_args *pa)
{
	//; of_instance_to_package(int ihandle, int *phandle)
	uint32 ihandle = pa->args[0];
	IO_PROM_TRACE("instance-to-package(%08x)\n", ihandle);
	PromInstance *pi = handleToInstance(ihandle);
	if (pi) {
		pa->args[1] = pi->getType()->getPHandle();
	} else {
		pa->args[1] = 0xffffffff;
	}
	IO_PROM_TRACE("= %08x\n", pa->args[1]);
}

void prom_service_finddevice(prom_args *pa)
{
	//; of_finddevice(const char *device_specifier, int *phandle)
	String device;
	prom_get_string(device, pa->args[0]);
	IO_PROM_TRACE("finddevice('%y')\n", &device);
	PromNode *node = findDevice(device.contentChar(), FIND_DEVICE_FIND, NULL);
	if (node) {
		pa->args[1] = node->getPHandle();
	} else {
		pa->args[1] = 0xffffffff;
	}
	IO_PROM_TRACE("= %08x\n", pa->args[1]);
}
void prom_service_getproplen(prom_args *pa)
{
	//; of_getproplen(int phandle, const char *name, int *proplen)
	uint32 phandle = pa->args[0];
	PromNode *p = handleToPackage(phandle);	
	String name;
	prom_get_string(name, pa->args[1]);
	IO_PROM_TRACE("getproplen(%08x, '%y')\n", phandle, &name);
	PromProp *prop = p ? p->findProp(name.contentChar()) : 0;
	if (!prop) {
		pa->args[2] = 0xffffffff;
	} else {
		pa->args[2] = prop->getValueLen();
	}
	IO_PROM_TRACE("= %08x\n", pa->args[2]);
}
void prom_service_getprop(prom_args *pa)
{
	//; of_getprop(int phandle, const char *name, void *buf, int buflen, int *size)
	uint32 phandle = pa->args[0];
	String name;
	prom_get_string(name, pa->args[1]);
	uint32 buf = pa->args[2];
	uint32 buflen = pa->args[3];
	IO_PROM_TRACE("getprop(%08x, '%y', %08x, %08x)\n", phandle, &name, buf, buflen);
	PromNode *p = handleToPackage(phandle);	
	PromProp *prop = p ? p->findProp(name.contentChar()) : 0;
	if (!prop) {
		pa->args[4] = 0xffffffff;
	} else {
		pa->args[4] = prop->getValue(buf, buflen);
	}
	IO_PROM_TRACE("= %08x\n", pa->args[4]);
}
void prom_service_nextprop(prom_args *pa)
{
	//; of_nextprop(int phandle, const char *previous, void *buf, int *flag)
	uint32 phandle = pa->args[0];
	String previous;
	prom_get_string(previous, pa->args[1]);
	uint32 buf = pa->args[2];
	uint32 *flag = &pa->args[3];
	IO_PROM_TRACE("nextprop(%08x, %08x:'%y', %08x)\n", phandle, pa->args[1], &previous, buf);
	PromNode *pn = handleToPackage(phandle);
	PromProp *prop = NULL;
	if (previous.isEmpty() || !pn) {
		prop = pn ? pn->firstProp() : 0;
		if (!prop) {
			*flag = 0;
		} else {
			*flag = 1;
		}
	} else {
		prop = pn->findProp(previous.contentChar());
		if (prop) {
			prop = pn->nextProp(prop);
			if (prop) {
				*flag = 1;
			} else {
				*flag = 0;
			}
		} else {
			*flag = 0xffffffff;
		}
	}
	uint32 phys;
	if (ppc_prom_effective_to_physical(phys, buf)) {
		if (prop) {
			char b[32];
			ht_snprintf(b, sizeof b, "%s", prop->name);
			ppc_dma_write(phys, b, strlen(b)+1);
		} else {
			char b=0;
			ppc_dma_write(phys, &b, 1);
		}
	} else {
		IO_PROM_ERR("can't access memory in %s\n", __FUNCTION__);
	}
	IO_PROM_TRACE("= %08x\n", *flag);
}
void prom_service_setprop(prom_args *pa)
{
	//; of_setprop(int phandle, const char *name, void *buf, int len, int *size)
	uint32 phandle = pa->args[0];
	String name;
	prom_get_string(name, pa->args[1]);
	uint32 buf = pa->args[2];
	String value;
	uint32 len = pa->args[3];
	uint32 phys;
	if (buf && len && ppc_prom_effective_to_physical(phys, buf)) {
		byte *p = (byte *)malloc(len);
		if (ppc_dma_read(p, phys, len)) {
			value.assign(p, len);
		}
		free(p);		
	}
	PromNode *p = handleToPackage(phandle);	
	IO_PROM_TRACE("setprop(%x=='%s', '%y', '%y', %d)\n", phandle, p->name, &name, &value, len);
/*	if (phandle == 0xdeadbee1) {
		pa->args[4] = len;
//		gSinglestep = true;
		return;
	}*/
	if (p) {
		PromProp *prop = p->findProp(name.contentChar());
		if (!prop) {
			if (buf) {		
				p->addProp(new PromPropMemory(name.contentChar(), value.content(), len));
			} else {
				p->addProp(new PromPropString(name.contentChar(), ""));
			}
			pa->args[4] = len;
		} else {
			//if (!buf && len) IO_PROM_ERR("setprop null bla\n");
			if (buf && len)
				pa->args[4] = prop->setValue(buf, len);
			else
				pa->args[4] = prop->setValue(0, 0);
		}
	} else {
		pa->args[4] = 0;
	}
	IO_PROM_TRACE("= %08x\n", pa->args[4]);
}
void prom_service_canon(prom_args *pa)
{
	//; of_canon(const char *device_specifier, void *buf, int buflen, int *length)
	String device;
	prom_get_string(device, pa->args[0]);
	uint32 buf = pa->args[1];
	uint32 buflen = pa->args[2];
	IO_PROM_TRACE("canon('%y', %08x, %08x)\n", &device, buf, buflen);
	
	uint32 phys;
	if (ppc_prom_effective_to_physical(phys, buf)) {
		if (buflen) buflen--;
		device.crop(buflen);
		ppc_dma_write(phys, device.contentChar(), device.length()+1);
		pa->args[3] = device.length();
	} else {
		pa->args[3] = 0;
	}
	IO_PROM_TRACE("= %08x\n", pa->args[3]);
}
void prom_service_instance_to_path(prom_args *pa)
{
	//; of_instance_to_path(int ihandle, void *buf, int buflen, int *length)
	uint32 ihandle = pa->args[0];
	uint32 buf = pa->args[1];
	uint32 buflen = pa->args[2];
	PromInstance *pi = handleToInstance(ihandle);
	IO_PROM_TRACE("instance-to-path(%08x, %08x, %08x)\n", ihandle, buf, buflen);
	uint32 phys;
	if (pi && buflen && ppc_prom_effective_to_physical(phys, buf)) {
		PromNode *pn = pi->getType();
		char *s = (char *)malloc(buflen);
		pa->args[3] = pn->toPath(s, buflen);
		ppc_dma_write(phys, s, pa->args[3]+1);
		free(s);
	} else {
		pa->args[3] = 0;
	}
	IO_PROM_TRACE("= %08x\n", pa->args[3]);
}
void prom_service_package_to_path(prom_args *pa)
{
	//; of_package_to_path(int phandle, void *buf, int buflen, int *length)
	uint32 phandle = pa->args[0];
	uint32 buf = pa->args[1];
	uint32 buflen = pa->args[2];
	PromNode *p = handleToPackage(phandle);
	IO_PROM_TRACE("package-to-path(%08x, %08x, %08x)\n", phandle, buf, buflen);
	uint32 phys;
	if (p && ppc_prom_effective_to_physical(phys, buf)) {
		char *s = (char *)malloc(buflen);
		pa->args[3] = p->toPath(s, buflen);
		ppc_dma_write(phys, s, pa->args[3]+1);
		free(s);
	} else {
		pa->args[3] = 0;
	}
//	IO_PROM_TRACE("%s\n", prom_ea_string(buf));
	IO_PROM_TRACE("= %08x\n", pa->args[3]);
}
void prom_service_call_method(prom_args *pa)
{
	//; int of_call_method(const char *method, int ihandle, ...); */
	String method;
	prom_get_string(method, pa->args[0]);
	uint32 ihandle = pa->args[1];
	PromInstance *pi = handleToInstance(ihandle);
	IO_PROM_TRACE("call-method('%y', %08x, ...)\n", &method, ihandle);
	if (ihandle == 0xdeadbee2) {
		if (method == "slw_emit") {
//			gDisplay->printf("%c", pa->args[2]);
		} else if (method == "slw_cr") {
//			gDisplay->print("\n");
		} else if (method == "slw_init_keymap") {
			uint32 a = prom_mem_malloc(20);
			prom_mem_set(a, 0x00, 20);
			pa->args[4] = prom_mem_phys_to_virt(a);
			pa->args[3] = 0;
		} else if (method == "slw_update_keymap") {
		} else if (method == "slw_set_output_level") {
		} else if (method == "slw_spin_init") {
		} else if (method == "slw_spin") {
		} else if (method == "slw_pwd") {
			uint32 phandle = pa->args[4];
			uint32 buf = pa->args[3];
			uint32 buflen = pa->args[2];			
			PromNode *obj = handleToPackage(phandle);
			pa->args[5] = 0;
			uint32 phys;
			if (obj && ppc_prom_effective_to_physical(phys, buf)) {
				char *s = (char *)malloc(buflen);
				pa->args[6] = obj->toPath(s, buflen);
				ppc_dma_write(phys, s, pa->args[6]+1);
				free(s);
			} else {
				pa->args[6] = 0;
			}
		} else {
			IO_PROM_ERR("slw: %y not impl\n", &method);
		}
	} else {
		if (pi) pi->callMethod(method.contentChar(), pa);
	}
}
void prom_service_open(prom_args *pa)
{
	//; of_open(const char *device_specifier, int *ihandle)
	String device;
	prom_get_string(device, pa->args[0]);
	IO_PROM_TRACE("open('%y')\n", &device);
	PromInstanceHandle ih;
	PromNode *node = findDevice(device.contentChar(), FIND_DEVICE_OPEN, &ih);
	if (node) {
		pa->args[1] = ih;
	} else {
		pa->args[1] = 0xffffffff;
	}
	IO_PROM_TRACE("= %08x\n", pa->args[1]);
}
void prom_service_close(prom_args *pa)
{
	//; of_close(int ihandle)
	uint32 ihandle = pa->args[0];
	IO_PROM_TRACE("close(%x)\n", ihandle);
	PromInstance *pi = handleToInstance(ihandle);
	if (pi) {
		pi->getType()->close(ihandle);
	}
}
void prom_service_read(prom_args *pa)
{
	//; of_read(int ihandle, void *addr, int len, int *actual)
	uint32 ihandle = pa->args[0];
	uint32 addr = pa->args[1];
	uint32 len = pa->args[2];
	IO_PROM_TRACE("read(%08x, %08x, %08x)\n", ihandle, addr, len);
	PromInstance *pi = handleToInstance(ihandle);
	pa->args[3] = pi ? pi->read(addr, len) : 0;
	IO_PROM_TRACE("= %08x\n", pa->args[3]);
}
void prom_service_write(prom_args *pa)
{
	//; of_write(int ihandle, void *addr, int len, int *actual)
	uint32 ihandle = pa->args[0];
	uint32 addr = pa->args[1];
	uint32 len = pa->args[2];
	IO_PROM_TRACE("write(%08x, %08x, %08x)\n", ihandle, addr, len);
	PromInstance *pi = handleToInstance(ihandle);
	pa->args[3] = pi ? pi->write(addr, len) : 0;
	IO_PROM_TRACE("= %08x\n", pa->args[3]);
}
void prom_service_seek(prom_args *pa)
{
	//; of_seek(int ihandle, int pos_hi, int pos_lo, int *status)
	uint32 ihandle = pa->args[0];
	uint32 pos_hi = pa->args[1];
	uint32 pos_lo = pa->args[2];
	IO_PROM_TRACE("seek(%x, %x%032x)\n", ihandle, pos_hi, pos_lo);
	PromInstance *pi = handleToInstance(ihandle);
	pa->args[3] = pi ? pi->seek((((uint64)pos_hi) << 32) | pos_lo) : 0;
	IO_PROM_TRACE("= %08x\n", pa->args[3]);
}
void prom_service_claim(prom_args *pa)
{
	//; of_claim(void *virt, int size, int align, void **baseaddr)
	uint32 virt = pa->args[0];
	uint32 size = pa->args[1];
	uint32 align = pa->args[2];
	IO_PROM_TRACE("claim(%x, %x, %x)\n", virt, size, align);
	if (!align) {
		pa->args[3] = prom_allocate_mem(size, align, virt);
		IO_PROM_TRACE("= %08x\n", pa->args[3]);
	} else {
		pa->args[3] = prom_allocate_mem(size, align, virt);
		IO_PROM_TRACE("= %08x\n", pa->args[3]);
	}
}
void prom_service_release(prom_args *pa)
{
	//; of_release(void *virt, int size)
	IO_PROM_ERR("release()\n");
}
void prom_service_boot(prom_args *pa)
{
	//; of_boot(const char *bootspec)
	IO_PROM_ERR("boot()\n");
}
void prom_service_enter(prom_args *pa)
{
	//; of_enter(void)
	uint64 start = sys_get_hiresclk_ticks();
	uint64 end = start+(sys_get_hiresclk_ticks_per_second()*3);
	while (sys_get_hiresclk_ticks() < end);
	//IO_PROM_ERR("enter()\n");
}
void prom_service_exit(prom_args *pa)
{
	//; of_exit(void)
	IO_PROM_ERR("exit()\n");
/*	while (1);
	exit(1);*/
}
void prom_service_chain(prom_args *pa)
{
	//; of_chain(void *virt, int size, void *entry, void *args, int len);
	IO_PROM_ERR("chain()\n");
}
#include "io/graphic/gcard.h"
void prom_service_interpret(prom_args *pa)
{
	//; of_(const char *arg, ...);
	String arg;
	prom_get_string(arg, pa->args[0]);
	IO_PROM_TRACE("interpret(%d, %08x, %08x, %08x, %08x, '%y' ...)\n", arg.length(), pa->args[1], pa->args[2], pa->args[3], pa->args[4], &arg);
	switch (arg.length()) {
	case 6: {
		// " 10 ms"
		void prom_service_milliseconds(prom_args *pa);
		uint32 start = (prom_service_milliseconds(pa), pa->args[0]);
		while ((prom_service_milliseconds(pa),pa->args[0]) < start+10);
		break;
	}
	case 32: {
		// GetPackageProperty
		uint32 phandle = pa->args[1];
		uint32 proplen UNUSED = pa->args[2];
		uint32 propname = pa->args[3];
		String n;
		prom_get_string(n, propname);
		PromNode *pn = handleToPackage(phandle);
		IO_PROM_TRACE("GetPackageProperty('%y', %08x:%s)\n", &n, phandle, pn->name);
		PromProp *prop = pn->findProp(n.contentChar());
		if (!prop) {
			pa->args[4] = 0;
			pa->args[5] = 0;
			pa->args[6] = 0;
		} else {
			uint32 m = prom_mem_phys_to_virt(prom_mem_malloc(100));
			IO_PROM_TRACE("m = %08x\n", m);
			int s = prop->getValue(m, 100);
			pa->args[4] = 0;
			pa->args[5] = s;
			pa->args[6] = m;	
		}
		break;
//		IO_PROM_ERR("-1\n");
	}
	case 39: 
	case 81:
	case 89:
		pa->args[2] = 0;
		break;
	case 75: {
		// InitMemoryMap
		PromNode *chosen = (PromNode*)gPromRoot->findNode("chosen");
		PromNode *mm = new PromNode("memory-map");
		chosen->addNode(mm);
		pa->args[1] = 0;
		pa->args[2] = mm->getPHandle();
		break;
	}
	case 593: 
	case 648: 
		// InitDisplay
		pa->args[4] = 0;
		pa->args[5] = IO_GCARD_FRAMEBUFFER_PA_START;
//		gSinglestep = true;
		break;
	case 1967:
	case 1644:
		// Init SLW
		pa->args[1] = 0;
		pa->args[2] = 0xdeadbee2;
		break;
	case 1972: // older BootX
		pa->args[1] = 0;
		pa->args[2] = 0;
		pa->args[3] = 0xdeadbee2;
		break;
	case 47:
	case 11:
	case 21:
	case 36:
	case 116:
	case 152:
	case 10722:
		pa->args[1] = 0;
		pa->args[2] = 0;
		break;
	default: 		
		IO_PROM_ERR("unknown interpret size %d\ninterpret: %y\n", arg.length(), &arg);
		break;
	}
}
void prom_service_set_callback(prom_args *pa)
{
	//; of_set_callback(void *newfunc, void **oldfunc)
	IO_PROM_ERR("callback()\n");
}
void prom_service_set_symbol_lookup(prom_args *pa)
{
	//; of_set_symbol_lookup(void *sym_to_value, void *value_to_sym)
	IO_PROM_ERR("symbol-lookup()\n");
}
void prom_service_milliseconds(prom_args *pa)
{
	static uint32 start  = (sys_get_hiresclk_ticks() / (sys_get_hiresclk_ticks_per_second()/1000));
	//; of_milliseconds(int *ms)
	uint32 millis = (uint32)(sys_get_hiresclk_ticks() / (sys_get_hiresclk_ticks_per_second()/1000)) - start;
	IO_PROM_TRACE("milliseconds()\n");
	pa->args[0] = millis;
	IO_PROM_TRACE("= %08x\n", pa->args[0]);
}

typedef void (*prom_service_function)(prom_args *pa);

struct prom_service_desc {
	const char *name;
	prom_service_function f;
};

prom_service_desc prom_service_table[] = {
	{"start-cpu", &prom_service_start_cpu},
	{"quiesce", &prom_service_quiesce},
/* 6.3.2.1 Client interface */
	{"test", &prom_service_test},      //; of_test(const char *name, int *missing)
/* 6.3.2.2 Device tree */
	{"peer", &prom_service_peer},      //; of_peer(int phandle, int *sibling_phandle)
	{"child", &prom_service_child},    //; (int phandle, int *child_phandle)
	{"parent", &prom_service_parent},  //; of_parent(int phandle, int *parent_phandle)
	{"instance-to-package", &prom_service_instance_to_package}, //; of_instance_to_package(int ihandle, int *phandle)
	{"getproplen", &prom_service_getproplen}, //; of_getproplen(int phandle, const char *name, int *proplen)
	{"getprop", &prom_service_getprop}, //; of_getprop(int phandle, const char *name, void *buf, int buflen, int *size)
	{"nextprop", &prom_service_nextprop}, //; of_nextprop(int phandle, const char *previous, void *buf, int *flag)
	{"setprop", &prom_service_setprop},   //; of_setprop(int phandle, const char *name, void *buf, int len, int *size)
	{"canon", &prom_service_canon},   //; of_canon(const char *device_specifier, void *buf, int buflen, int *length)
	{"finddevice", &prom_service_finddevice}, //; of_finddevice(const char *device_specifier, int *phandle)
	{"instance-to-path", &prom_service_instance_to_path}, //; of_instance_to_path(int ihandle, void *buf, int buflen, int *length)
	{"package-to-path", &prom_service_package_to_path}, //; of_package_to_path(int phandle, void *buf, int buflen, int *length)
	{"call-method", &prom_service_call_method}, //; int of_call_method(const char *method, int ihandle, ...); */
/* 6.3.2.3 Device I/O */
	{"open", &prom_service_open},       //; of_open(const char *device_specifier, int *ihandle)
	{"close", &prom_service_close},     //; of_close(int ihandle)
	{"read", &prom_service_read},       //; of_read(int ihandle, void *addr, int len, int *actual)
	{"write", &prom_service_write},     //; of_write(int ihandle, void *addr, int len, int *actual)
	{"seek", &prom_service_seek},       //; of_seek(int ihandle, int pos_hi, int pos_lo, int *status)
/* 6.3.2.4 Memory */
	{"claim", &prom_service_claim},     //; of_claim(void *virt, int size, int align, void **baseaddr)
	{"release", &prom_service_release}, //; of_release(void *virt, int size)
/* 6.3.2.5 Control transfer */
	{"boot", &prom_service_boot},       //; of_boot(const char *bootspec)
	{"enter", &prom_service_enter},     //; of_enter(void)
	{"exit", &prom_service_exit},       //; of_exit(void)
	{"chain", &prom_service_chain},     //; of_chain(void *virt, int size, void *entry, void *args, int len);
/* 6.3.2.6 User interface */
	{"interpret", &prom_service_interpret}, //; of_(const char *arg, ...);
	{"set-callback", &prom_service_set_callback}, //; of_set_callback(void *newfunc, void **oldfunc)
	{"set-symbol-lookup", &prom_service_set_symbol_lookup}, //; of_set_symbol_lookup(void *sym_to_value, void *value_to_sym)
/* 6.3.2.7 Time */
	{"milliseconds", &prom_service_milliseconds}, //; of_milliseconds(int *ms)
	{"get-msecs",    &prom_service_milliseconds}, //; of_milliseconds(int *ms)
	{NULL, NULL}
};

void call_prom_osi()
{
	prom_args pa;
	memset(&pa, 0, sizeof pa);
	uint32 pa_s = ppc_cpu_get_gpr(0, 3);
	uint32 phys;
	if (!ppc_prom_effective_to_physical(phys, pa_s)
	|| !ppc_dma_read(&pa.service, phys+0, 4)
	|| !ppc_dma_read(&pa.nargs, phys+4, 4)
	|| !ppc_dma_read(&pa.nret, phys+8, 4)) {
		IO_PROM_ERR("can't read memory at %08x-%08x\n", pa_s+0, pa_s+8);
	}
	pa.service = ppc_word_from_BE(pa.service);
	pa.nargs = ppc_word_from_BE(pa.nargs);
	pa.nret = ppc_word_from_BE(pa.nret);
	phys += 12;
	for (uint i=0; i<pa.nargs; i++) {
		if (!ppc_dma_read(&pa.args[i], phys, 4)) {
			IO_PROM_ERR("can't read memory at %08x\n", pa.args[i]);
		}
		pa.args[i] = ppc_word_from_BE(pa.args[i]);
		phys += 4;
	}
	String service;
	prom_get_string(service, pa.service);
	int i=0;
	while (prom_service_table[i].name) {
		if (strcmp(prom_service_table[i].name, service.contentChar())==0) {
			prom_service_table[i].f(&pa);
			goto ok;
		}
		i++;
	}
	// error     
	IO_PROM_ERR("unknown service '%y'\n", &service);
ok:
	// write values back
	pa_s = ppc_cpu_get_gpr(0, 3);
	pa_s += 12;
	for (uint i=0; i<(pa.nargs+pa.nret); i++) {
		uint32 phys;
		if (!ppc_prom_effective_to_physical(phys, pa_s)) {
			IO_PROM_ERR("can't write memory at %08x\n", pa.args[i]);
		}
		uint32 w = ppc_word_to_BE(pa.args[i]);
		if (!ppc_dma_write(phys, &w, 4)) {
			IO_PROM_ERR("can't write memory at %08x\n", pa.args[i]);
		}
		pa_s += 4;
	}
	ppc_cpu_set_gpr(0, 3, 0);
	// return
//	gCPU.npc = gCPU.lr;
}
