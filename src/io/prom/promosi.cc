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
#include "cpu_generic/ppc_cpu.h"
#include "cpu_generic/ppc_mmu.h"
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
	const char *name UNUSED = prom_ea_string(pa->args[0]);
	IO_PROM_ERR("test('%s')\n", name);
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
	const char *device = prom_ea_string(pa->args[0]);
	IO_PROM_TRACE("finddevice('%s')\n", device);
	PromNode *node = findDevice(device, FIND_DEVICE_FIND, NULL);
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
	const char *name = prom_ea_string(pa->args[1]);
	IO_PROM_TRACE("getproplen(%08x, '%s')\n", phandle, name);
	PromProp *prop = p ? p->findProp(name) : 0;
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
	const char *name = prom_ea_string(pa->args[1]);
	uint32 buf = pa->args[2];
	uint32 buflen = pa->args[3];
	IO_PROM_TRACE("getprop(%08x, '%s', %08x, %08x)\n", phandle, name, buf, buflen);
	PromNode *p = handleToPackage(phandle);	
	PromProp *prop = p ? p->findProp(name) : 0;
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
	const char *previous = prom_ea_string(pa->args[1]);
	uint32 buf = pa->args[2];
	uint32 *flag = &pa->args[3];
	IO_PROM_TRACE("nextprop(%08x, %08x:'%s', %08x)\n", phandle, pa->args[1], previous, buf);
	PromNode *pn = handleToPackage(phandle);
	PromProp *prop = NULL;
	if (!previous || !*previous || !pn) {
		prop = pn ? pn->firstProp() : 0;
		if (!prop) {
			*flag = 0;
		} else {
			*flag = 1;
		}
	} else {
		prop = pn->findProp(previous);
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
	if (prop) {
		ht_snprintf(prom_ea_string(buf), 32, "%s", prop->name);
	} else {
		ht_snprintf(prom_ea_string(buf), 32, "");
	}
	IO_PROM_TRACE("= %08x\n", *flag);
}
void prom_service_setprop(prom_args *pa)
{
	//; of_setprop(int phandle, const char *name, void *buf, int len, int *size)
	uint32 phandle = pa->args[0];
	const char *name = prom_ea_string(pa->args[1]);
	uint32 buf = pa->args[2];
	const byte *bufbuf = buf ? (byte*)prom_ea_string(buf) : NULL;
	uint32 len = pa->args[3];
	String value(bufbuf, len);
	PromNode *p = handleToPackage(phandle);	
	IO_PROM_TRACE("setprop(%x=='%s', '%s', '%y', %d)\n", phandle, p->name, name, &value, len);
/*	if (phandle == 0xdeadbee1) {
		pa->args[4] = len;
//		gSinglestep = true;
		return;
	}*/
	if (p) {
		PromProp *prop = p->findProp(name);
		if (!prop) {
			if (buf) {		
				p->addProp(new PromPropMemory(name, value.content(), len));
			} else {
				p->addProp(new PromPropString(name, ""));
			}
			pa->args[4] = len;
		} else {
			if (!buf) IO_PROM_ERR("setprop null bla\n");
			pa->args[4] = prop->setValue(buf, len);
		}
	} else {
		pa->args[4] = 0;
	}
	IO_PROM_TRACE("= %08x\n", pa->args[4]);
}
void prom_service_canon(prom_args *pa)
{
	//; of_canon(const char *device_specifier, void *buf, int buflen, int *length)
	const char *device = prom_ea_string(pa->args[0]);
	uint32 buf = pa->args[1];
	uint32 buflen = pa->args[2];
	IO_PROM_TRACE("canon('%s', %08x, %08x)\n", device, buf, buflen);
	pa->args[3] = ht_snprintf((char*)prom_ea_string(buf), buflen, "%s", device);
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
	if (pi) {
		PromNode *pn = pi->getType();
		pa->args[3] = pn->toPath((char*)prom_ea_string(buf), buflen);
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
	pa->args[3] = p ? p->toPath((char*)prom_ea_string(buf), buflen) : 0;
	IO_PROM_TRACE("%s\n", prom_ea_string(buf));
	IO_PROM_TRACE("= %08x\n", pa->args[3]);
}
void prom_service_call_method(prom_args *pa)
{
	//; int of_call_method(const char *method, int ihandle, ...); */
	const char *method = prom_ea_string(pa->args[0]);
	uint32 ihandle = pa->args[1];
	PromInstance *pi = handleToInstance(ihandle);
	IO_PROM_TRACE("call-method('%s', %08x, ...)\n", method, ihandle);
	if (ihandle == 0xdeadbee2) {
		if (strcmp(method, "slw_emit") == 0) {
//			gDisplay->printf("%c", pa->args[2]);
		} else if (strcmp(method, "slw_cr") == 0) {
//			gDisplay->print("\n");
		} else if (strcmp(method, "slw_init_keymap") == 0) {
			uint32 a = prom_mem_malloc(20);
			prom_mem_set(a, 0x00, 20);
			pa->args[4] = prom_mem_phys_to_virt(a);
			pa->args[3] = 0;
		} else if (strcmp(method, "slw_update_keymap") == 0) {
		} else if (strcmp(method, "slw_set_output_level") == 0) {
		} else if (strcmp(method, "slw_spin_init") == 0) {
		} else if (strcmp(method, "slw_spin") == 0) {
		} else if (strcmp(method, "slw_pwd") == 0) {
			uint32 phandle = pa->args[4];
			uint32 buf = pa->args[3];
			uint32 buflen = pa->args[2];			
			PromNode *obj = handleToPackage(phandle);
			pa->args[5] = 0;
			pa->args[6] = obj ? obj->toPath((char*)prom_ea_string(buf), buflen) : 0;
		} else {
			IO_PROM_ERR("slw: %s not impl\n", method);
		}
	} else {
		if (pi) pi->callMethod(method, pa);
	}
}
void prom_service_open(prom_args *pa)
{
	//; of_open(const char *device_specifier, int *ihandle)
	const char *device = prom_ea_string(pa->args[0]);
	IO_PROM_TRACE("open('%s')\n", device);
	PromInstanceHandle ih;
	PromNode *node = findDevice(device, FIND_DEVICE_OPEN, &ih);
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
	pa->args[3] = pi ? pi->seek(pos_hi, pos_lo) : 0;
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
	IO_PROM_ERR("enter()\n");
}
void prom_service_exit(prom_args *pa)
{
	//; of_exit(void)
	IO_PROM_ERR("exit()\n");
/*	gDisplay->displayShow();
	while (1);
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
	const char *arg = prom_ea_string(pa->args[0]);
	IO_PROM_TRACE("interpret(%d, %08x, '%s' ...)\n", strlen(arg), pa->args[1], arg);
	switch (strlen(arg)) {
	case 6:
		// " 10 ms"
		break;
	case 32: {
		// GetPackageProperty
		uint32 phandle = pa->args[1];
		uint32 proplen UNUSED = pa->args[2];
		uint32 propname = pa->args[3];
		char *n = prom_ea_string(propname);
		PromNode *pn = handleToPackage(phandle);
		IO_PROM_TRACE("GetPackageProperty('%s', %08x:%s)\n", n, phandle, pn->name);
		PromProp *prop = pn->findProp(n);
		if (!prop) {
			pa->args[4] = 0;
			pa->args[5] = 0;
			pa->args[6] = 0;
		} else {
			uint32 m = prom_mem_phys_to_virt(prom_mem_malloc(100));
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
		IO_PROM_ERR("unknown interpret size %d \n", strlen(arg));
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
	//; of_milliseconds(int *ms)
	IO_PROM_TRACE("milliseconds()\n");
	pa->args[0] = 0;
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
	{NULL, NULL}
};

void call_prom_osi()
{
	prom_args pa;
	uint32 pa_s = gCPU.gpr[3];
	if (ppc_read_effective_word(pa_s+0, pa.service)
	 || ppc_read_effective_word(pa_s+4, pa.nargs)
	 || ppc_read_effective_word(pa_s+8, pa.nret)) {
		IO_PROM_ERR("can't read memory at %08x-%08x\n", pa_s+0, pa_s+8);
	}
	pa_s+=12;
	for (uint i=0; i<pa.nargs; i++) {
		if (ppc_read_effective_word(pa_s, pa.args[i])) {
			IO_PROM_ERR("can't read memory at %08x\n", pa.args[i]);
		}
		pa_s += 4;
	}
	const char *service = prom_ea_string(pa.service);
	if (!service) {
		IO_PROM_ERR("unknown service\n");
	}
	int i=0;
	while (prom_service_table[i].name) {
		if (strcmp(prom_service_table[i].name, service)==0) {
			prom_service_table[i].f(&pa);
			goto ok;
		}
		i++;
	}
	// error     
	IO_PROM_ERR("unknown service '%s'\n", service);
ok:
	// write values back
	pa_s = gCPU.gpr[3];
	pa_s+=12;
	for (uint i=0; i<(pa.nargs+pa.nret); i++) {
		if (ppc_write_effective_word(pa_s, pa.args[i])) {
			IO_PROM_ERR("can't write memory at %08x\n", pa.args[i]);
		}
		pa_s += 4;
	}
	// return
	gCPU.gpr[3] = 0;
	gCPU.npc = gCPU.lr;
}
