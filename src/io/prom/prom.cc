/* 
 *	PearPC
 *	prom.cc
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


#include "tools/debug.h"
#include "debug/tracers.h"
#include "configparser.h"
#include "promdt.h"
#include "prommem.h"
#include "prom.h"

PromBootMethod gPromBootMethod;
String gPromBootPath;

/************************************************************************
 *
 */

#define PROM_KEY_BOOTMETHOD "prom_bootmethod"
#define PROM_KEY_ENV_BOOTPATH "prom_env_bootpath"
#define PROM_KEY_ENV_BOOTARGS "prom_env_bootargs"
#define PROM_KEY_ENV_MACHARGS "prom_env_machargs"
#define PROM_KEY_ENV_LOADFILE "prom_loadfile"
#define PROM_KEY_DRIVER_GRAPHIC "prom_driver_graphic"

void prom_init()
{
	prom_init_device_tree();
	PromNode *chosen = findDevice("/chosen", FIND_DEVICE_FIND, NULL);
	ASSERT(chosen);
	String bootmethod;
	gConfig->getConfigString(PROM_KEY_BOOTMETHOD, bootmethod);
	if (bootmethod == (String)"auto") {
		gPromBootMethod = prombmAuto;
	} else if (bootmethod == (String)"select") {
		gPromBootMethod = prombmSelect;
	} else if (bootmethod == (String)"force") {
		gPromBootMethod = prombmForce;
	} else {
		IO_PROM_ERR("unknown bootmethod '%y'\n", &bootmethod);
	}
	if (gConfig->haveKey(PROM_KEY_ENV_BOOTPATH)) {
		String bootpath;
		gConfig->getConfigString(PROM_KEY_ENV_BOOTPATH, bootpath);
		chosen->addProp(new PromPropString("bootpath", bootpath.contentChar()));
		gPromBootPath.assign(bootpath);
	}
	String bootargs;
	gConfig->getConfigString(PROM_KEY_ENV_BOOTARGS, bootargs);
	chosen->addProp(new PromPropString("bootargs", bootargs.contentChar()));
	String machargs;
	gConfig->getConfigString(PROM_KEY_ENV_MACHARGS, machargs);
	chosen->addProp(new PromPropString("machargs", machargs.contentChar()));

	if (gConfig->haveKey(PROM_KEY_DRIVER_GRAPHIC)) {
		String filename;
		gConfig->getConfigString(PROM_KEY_DRIVER_GRAPHIC, filename);
		FILE *dfile = fopen(filename.contentChar(), "rb");
		if (!dfile) IO_PROM_ERR("%s: can't open %y\n", PROM_KEY_DRIVER_GRAPHIC, &filename);
		fseek(dfile, 0, SEEK_END);
		int dsize = ftell(dfile);
		fseek(dfile, 0, SEEK_SET);
		byte *d = (byte*)malloc(dsize);
		fread(d, 1, dsize, dfile);
		fclose(dfile);
		PromNode *screen = findDevice("screen", FIND_DEVICE_FIND, NULL);
		if (screen) {
			screen->addProp(new PromPropMemory("driver,AAPL,MacOS,PowerPC", d, dsize));
		} else {
			IO_PROM_ERR("'screen' package not found.\n");
		}
	}
	
	prom_mem_init();
}

void prom_init_config()
{
	gConfig->acceptConfigEntryStringDef(PROM_KEY_BOOTMETHOD, "auto");
	gConfig->acceptConfigEntryString(PROM_KEY_ENV_BOOTPATH, false);
	gConfig->acceptConfigEntryStringDef(PROM_KEY_ENV_BOOTARGS, "");
	gConfig->acceptConfigEntryStringDef(PROM_KEY_ENV_MACHARGS, "");
	gConfig->acceptConfigEntryString(PROM_KEY_ENV_LOADFILE, false);
	gConfig->acceptConfigEntryStringDef(PROM_KEY_DRIVER_GRAPHIC, "");
}

void prom_quiesce()
{
	prom_done();
}

void prom_done()
{
	prom_done_device_tree();
	prom_mem_done();
}
