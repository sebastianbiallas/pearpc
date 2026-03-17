/*
 *	PearPC
 *	main.cc
 *
 *	Copyright (C) 2003-2005 Sebastian Biallas (sb@biallas.net)
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
#include <csignal>
#include <exception>
#include <execinfo.h>
#include <unistd.h>

#include "info.h"
#include "cpu/cpu.h"
//#include "cpu_generic/ppc_tools.h"
#include "debug/debugger.h"
#include "io/io.h"
#include "io/graphic/gcard.h"
#include "io/ide/ide.h"
#include "io/ide/cd.h"
#include "io/cuda/cuda.h"
#include "io/prom/prom.h"
#include "io/prom/promboot.h"
#include "io/prom/prommem.h"
#include "tools/atom.h"
#include "tools/data.h"
#include "tools/except.h"
#include "tools/snprintf.h"
#include "system/display.h"
#include "system/mouse.h"
#include "system/keyboard.h"
#include "system/sys.h"
#include "system/systhread.h"
#include "configparser.h"

#include "system/gif.h"
#include "system/ui/gui.h"

#include "ppc_font.h"
#include "ppc_img.h"
#include "ppc_button_changecd.h"

void changeCDFunc(void *p)
{
	int *i = (int *)p;
	IDEConfig *idecfg = ide_get_config(*i);
	
	CDROMDevice *dev = (CDROMDevice *)idecfg->device;
	
	dev->acquire();
	
	if (dev->isLocked()) {
		dev->release();
		
		// sys_gui_messagebox("cdrom is locked!");
	} else {
		dev->setReady(false);
		dev->release();
		/*
		 * because we have set ready to false, no one can use
		 * the cdrom now (no medium present)
		 */
		String fn;
		if (sys_gui_open_file_dialog(fn, "title", "*.*", "alle", "testa", true)) {
			dev->acquire();
			((CDROMDeviceFile *)dev)->changeDataSource(fn.contentChar());
			dev->setReady(true);
			dev->release();
		} else {
			/*
			 * the user picked no file / canceled the dialog.
			 * what's better now, to leave the old medium
			 * or to set no medium present?
			 * we choose the second option.
			 */
		}
	}
}

void initMenu()
{
/*	IDEConfig *idecfg = ide_get_config(0);
	if (idecfg->installed && idecfg->protocol == IDE_ATAPI) {
		MemMapFile changeCDButton(ppc_button_changecd, sizeof ppc_button_changecd);
		int *i = new int;
		*i = 0;
		gDisplay->insertMenuButton(changeCDButton, changeCDFunc, i);
	}
	idecfg = ide_get_config(1);
	if (idecfg->installed && idecfg->protocol == IDE_ATAPI) {
		MemMapFile changeCDButton(ppc_button_changecd, sizeof ppc_button_changecd);
		int *i = new int;
		*i = 1;
		gDisplay->insertMenuButton(changeCDButton, changeCDFunc, i);
	}
	gDisplay->finishMenu();*/
}

static const char *textlogo UNUSED = "\e[?7h\e[40m\e[2J\e[40m\n\n\n\n\n\e[0;1m"
"\e[24C\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4  "
"\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4   "
"\xda\xc4\xc4\xc4\xc4\xc4\xc4\n\e[24C\e[0m\xda\xc4\xc4   "
"\xda\xc4\xc4 \xda\xc4\xc4   \xda\xc4\xc4 \xda\xc4\xc4   "
"\xda\xc4\xc4\n\e[24C\e[1;30m\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4  "
"\xda\xc4\xc4\xc4\xc4\xc4\xc4\xc4  "
"\xda\xc4\xc4\n\e[24C\e[34m\xda\xc4\xc4\e[7C\xda\xc4\xc4\e[7C\xda\xc4\xc4   "
"\xda\xc4\xc4\n\e[24C\e[0;34m\xda\xc4\xc4\e[7C\xda\xc4\xc4\e[8C\xda\xc4\xc4\xc4\xc4\xc4\xc4\n\n";

static const vcp CONSOLE_BG = VC_BLACK;

void drawLogo()
{
	MemMapFile img(ppc_img, sizeof ppc_img);
	Gif g;
	g.loadFromByteStream(img);
	gDisplay->fillRGB(0, 0, gDisplay->mClientChar.width,
		gDisplay->mClientChar.height, MK_RGB(0xff, 0xff, 0xff));
	g.draw(gDisplay, (gDisplay->mClientChar.width-g.mWidth)/2, (gDisplay->mClientChar.height >= 600) ? (150-g.mHeight)/2 : 0);
	gDisplay->setAnsiColor(VCP(VC_BLUE, CONSOLE_BG));
	gDisplay->fillAllVT(VCP(VC_BLUE, CONSOLE_BG), ' ');
//	gDisplay->print(textlogo);
	gDisplay->setAnsiColor(VCP(VC_LIGHT(VC_BLUE), VC_TRANSPARENT));
	gDisplay->print("\e[H" APPNAME " " APPVERSION " " COPYRIGHT"\n\n");
}

void tests()
{
/*	while (true) {
		DisplayEvent ev;
		if (gDisplay->getEvent(ev)) {
			if (ev.type == evMouse) {
				gDisplay->printf("%x, %x  ", ev.mouseEvent.relx, ev.mouseEvent.rely);
				gDisplay->printf("%x\n", ev.mouseEvent.button1);
			} else {
				gDisplay->printf("%x %d\n", ev.keyEvent.keycode, ev.keyEvent.keycode);
			}
		}
	}*/
}

#include "io/prom/forth.h"
void testforth()
{

#if 0
		ForthVM vm;
		gCPU.msr = MSR_IR | MSR_DR | MSR_FP;
//		LocalFile in("test/test.f2", IOAM_READ, FOM_EXISTS);
//		vm.interprete(in, in);
		do {
			try {
				MemoryFile in(0);
				char buf[1024];
				fgets(buf, sizeof buf, stdin);
				in.write(buf, strlen(buf));
				in.seek(0);
				vm.interprete(in, in);
			} catch (const ForthException &fe) {
				String res;
				fe.reason(res);
				ht_printf("exception: %y\n", &res);
			}
		} while (1);

#endif
}

/*
 *
 */
static bool gHeadless = false;

void usage()
{
	ht_printf("usage: ppc [--headless] configfile\n");
	exit(1);
}

#ifdef main
// Get rid of stupid SDL main redefinitions
#undef main
extern "C" int SDL_main(int argc, char *argv[])
{
	return 0;
}
#endif

static void crash_handler(int sig, siginfo_t *info, void *ctx)
{
	const char *signame = (sig == SIGILL) ? "SIGILL" : (sig == SIGSEGV) ? "SIGSEGV" : (sig == SIGBUS) ? "SIGBUS" : "SIGNAL";
	fprintf(stderr, "\n*** %s at %p (signal %d) ***\n", signame, info->si_addr, sig);

#ifdef __aarch64__
	ucontext_t *uc = (ucontext_t *)ctx;
	fprintf(stderr, "  pc=%p  lr=%p  sp=%p\n",
		(void *)uc->uc_mcontext->__ss.__pc,
		(void *)uc->uc_mcontext->__ss.__lr,
		(void *)uc->uc_mcontext->__ss.__sp);
	if (sig == SIGILL) {
		uint32 insn = *(uint32 *)info->si_addr;
		fprintf(stderr, "  Instruction word: %08x\n", insn);
	}
#endif

	// Stack trace
	void *bt[64];
	int n = backtrace(bt, 64);
	fprintf(stderr, "  Backtrace (%d frames):\n", n);
	backtrace_symbols_fd(bt, n, STDERR_FILENO);

	// Flush trace log
	extern FILE *gTraceLog;
	if (gTraceLog) fflush(gTraceLog);

	_exit(128 + sig);
}

int main(int argc, char *argv[])
{
	// Install signal handlers for crash diagnostics
	struct sigaction sa;
	sa.sa_sigaction = crash_handler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGILL, &sa, NULL);
	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);

	const char *configfile = NULL;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--headless") == 0) {
			gHeadless = true;
		} else if (!configfile) {
			configfile = argv[i];
		} else {
			usage();
		}
	}
	if (!configfile) usage();

	setvbuf(stdout, 0, _IONBF, 0);

	sys_gui_init();
	
 	if (sizeof(uint8) != 1) {
		ht_printf("sizeof(uint8) == %d != 1\n", sizeof(uint8)); exit(-1);
	}
	if (sizeof(uint16) != 2) {
		ht_printf("sizeof(uint16) == %d != 2\n", sizeof(uint16)); exit(-1);
	}
	if (sizeof(uint32) != 4) {
		ht_printf("sizeof(uint32) == %d != 4\n", sizeof(uint32)); exit(-1);
	}
	if (sizeof(uint64) != 8) {
		ht_printf("sizeof(uint64) == %d != 8\n", sizeof(uint64)); exit(-1);
	}

#if defined(WIN32) || defined(__WIN32__)
#else
	strncpy(gAppFilename, argv[0], sizeof gAppFilename);
#endif

	if (!initAtom()) return 3;
	if (!initData()) return 4;
	if (!initOSAPI()) return 5;
	try {
		gConfig = new ConfigParser();
		gConfig->acceptConfigEntryStringDef("ppc_start_resolution", "800x600x15");
		gConfig->acceptConfigEntryIntDef("ppc_start_full_screen", 0);
		gConfig->acceptConfigEntryIntDef("memory_size", 128*1024*1024);
		gConfig->acceptConfigEntryIntDef("page_table_pa", 0x00300000);
		gConfig->acceptConfigEntryIntDef("redraw_interval_msec", 20);
		gConfig->acceptConfigEntryStringDef("key_compose_dialog", "F11");
		gConfig->acceptConfigEntryStringDef("key_change_cd_0", "none");
		gConfig->acceptConfigEntryStringDef("key_change_cd_1", "none");
		gConfig->acceptConfigEntryStringDef("key_toggle_mouse_grab", "F12");
		gConfig->acceptConfigEntryStringDef("key_toggle_full_screen", "Ctrl+Alt+Return");

		prom_init_config();
		io_init_config();
		ppc_cpu_init_config();
		debugger_init_config();

		try {
			LocalFile *config;
			config = new LocalFile(configfile);
			gConfig->loadConfig(*config);
			delete config;
		} catch (const Exception &e) {
			String res;
			e.reason(res);
			ht_printf("%s: %y\n", configfile, &res);
			usage();
			exit(1);
		}

		ht_printf("This program is free software; you can redistribute it and/or modify\n"
			"it under the terms of the GNU General Public License version 2 as published by\n"
			"the Free Software Foundation.\n"
			"\n"
			"This program is distributed in the hope that it will be useful,\n"
			"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
			"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
			"GNU General Public License for more details.\n"
			"\n"
			"You should have received a copy of the GNU General Public License\n"
			"along with this program; if not, write to the Free Software\n"
			"Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA\n\n");


		if (gConfig->getConfigInt("memory_size") < 64*1024*1024) {
			ht_printf("%s: 'memory_size' must be >= 64MB.", configfile);
			exit(1);
		}
		int msec = gConfig->getConfigInt("redraw_interval_msec");
		if (msec < 10 || msec > 500) {
			ht_printf("%s: 'redraw_interval_msec' must be between 10 and 500 (inclusive).", configfile);
			exit(1);
		}

		String key_compose_dialog_string;
		String key_toggle_mouse_grab_string;
		String key_toggle_full_screen_string;
		KeyboardCharacteristics keyConfig;
		gConfig->getConfigString("key_compose_dialog", key_compose_dialog_string);		
		gConfig->getConfigString("key_toggle_mouse_grab", key_toggle_mouse_grab_string);
		gConfig->getConfigString("key_toggle_full_screen", key_toggle_full_screen_string);
		if (!SystemKeyboard::convertStringToKeycode(keyConfig.key_compose_dialog, key_compose_dialog_string)) {
			ht_printf("%s: invalid '%s'\n", configfile, "key_compose_dialog");
			exit(1);
		}
		if (!SystemKeyboard::convertStringToKeycode(keyConfig.key_toggle_mouse_grab, key_toggle_mouse_grab_string)) {
			ht_printf("%s: invalid '%s'\n", configfile, "key_toggle_mouse_grab");
			exit(1);
		}
		if (!SystemKeyboard::convertStringToKeycode(keyConfig.key_toggle_full_screen, key_toggle_full_screen_string)) {
			ht_printf("%s: invalid '%s'\n", configfile, "key_toggle_full_screen");
			exit(1);
		}
		
		
		gcard_init_modes();
		
		String chr;
		DisplayCharacteristics gm;
		bool fullscreen;
		gConfig->getConfigString("ppc_start_resolution", chr);
		fullscreen = gConfig->getConfigInt("ppc_start_full_screen");
		if (!displayCharacteristicsFromString(gm, chr)) {
			ht_printf("%s: invalid '%s'\n", configfile, "ppc_start_resolution");
			exit(1);
		}
		switch (gm.bytesPerPixel) {
		/*
		 *	Are we confusing bytesPerPixel with bitsPerPixel?
		 *	Yes! And I am proud of it!
		 */
		case 15:
			gm.bytesPerPixel = 2;
			break;
		case 32:
			gm.bytesPerPixel = 4;
			break;
		default:
			ht_printf("%s: invalid depth in '%s'\n", configfile, "ppc_start_resolution");
			exit(1);
		}
		if (!gcard_finish_characteristic(gm)) {
			ht_printf("%s: invalid '%s'\n", configfile, "ppc_start_resolution");
			exit(1);
		}
		gcard_add_characteristic(gm);


		/*
		 *	begin hardware init
		 */

		if (!ppc_init_physical_memory(gConfig->getConfigInt("memory_size"))) {
			ht_printf("cannot initialize memory.\n");
			exit(1);
		}
		if (!ppc_cpu_init()) {
			ht_printf("cpu_init failed! Out of memory?\n");
			exit(1);
		}

		ht_printf("[DBG] cuda_pre_init...\n");
		cuda_pre_init();

		if (!gHeadless) {
			initUI(APPNAME " " APPVERSION, gm, msec, keyConfig, fullscreen);
		} else {
			/*
			 * Headless mode: create a minimal stub display so that
			 * PROM device tree init and gcard can read display params.
			 * No actual rendering happens.
			 */
			class HeadlessDisplay : public SystemDisplay {
			public:
				HeadlessDisplay(const DisplayCharacteristics &chr, int redraw_ms)
					: SystemDisplay(chr, redraw_ms) {}
				void displayShow() override {}
				void convertCharacteristicsToHost(DisplayCharacteristics &h, const DisplayCharacteristics &c) override { h = c; }
				bool changeResolution(const DisplayCharacteristics &) override { return true; }
				void getHostCharacteristics(Container &) override {}
				void updateTitle() override {}
				void setMouseGrab(bool) override {}
				void finishMenu() override {}
				int toString(char *buf, int buflen) const override { return snprintf(buf, buflen, "headless"); }
			};
			gDisplay = new HeadlessDisplay(gm, msec);
			// Allocate framebuffer for headless mode (needed by gcard)
			gFrameBuffer = (byte*)malloc(gm.width * gm.height * gm.bytesPerPixel);
			memset(gFrameBuffer, 0, gm.width * gm.height * gm.bytesPerPixel);
		}

		ht_printf("[DBG] io_init...\n");
		io_init();

		if (!gHeadless) {
			gcard_init_host_modes();
			gcard_set_mode(gm);

			if (fullscreen) gDisplay->setFullscreenMode(true);

			MemMapFile font(ppc_font, sizeof ppc_font);
			// FIXME: ..
			if (gDisplay->mClientChar.height >= 600) {
				int width = (gDisplay->mClientChar.width-40)/8;
				int height = (gDisplay->mClientChar.height-170)/15;
				if (!gDisplay->openVT(width, height, (gDisplay->mClientChar.width-width*8)/2, 150, font)) {
					ht_printf("Can't open virtual terminal.\n");
					exit(1);
				}
			} else {
				if (!gDisplay->openVT(77, 25, 12, 100, font)) {
					ht_printf("Can't open virtual terminal.\n");
					exit(1);
				}
			}

			initMenu();
			drawLogo();

			gDisplay->printf("CPU: PVR=%08x\n", ppc_cpu_get_pvr(0));
			gDisplay->printf("%d MiB RAM\n", ppc_get_memory_size() / (1024*1024));

			tests();
		}

		if (gHeadless) {
			// Open VT for headless mode too (PROM prints to it)
			MemMapFile font(ppc_font, sizeof ppc_font);
			gDisplay->openVT(80, 25, 0, 0, font);
		}

		ht_printf("CPU: PVR=%08x\n", ppc_cpu_get_pvr(0));
		ht_printf("%d MiB RAM\n", ppc_get_memory_size() / (1024*1024));

		// initialize initial paging (for prom)
		uint32 PAGE_TABLE_ADDR = gConfig->getConfigInt("page_table_pa");
		ht_printf("initializing initial page table at %08x\n", PAGE_TABLE_ADDR);

 		// 256 Kbytes Pagetable, 2^15 Pages, 2^12 PTEGs
		if (!ppc_prom_set_sdr1(PAGE_TABLE_ADDR+0x03, false)) {
			ht_printf("internal error setting sdr1.\n");
			return 1;
		}		
		
		// clear pagetable
		if (!ppc_dma_set(PAGE_TABLE_ADDR, 0, 256*1024)) {
			ht_printf("cannot access page table.\n");
			return 1;
		}

		// init prom
		ht_printf("[DBG] prom_init...\n");
		prom_init();
		
		// lock pagetable
		for (uint32 pa = PAGE_TABLE_ADDR; pa < (PAGE_TABLE_ADDR + 256*1024); pa += 4096) {
			if (!prom_claim_page(pa)) {
				ht_printf("cannot claim page table memory.\n");
				exit(1);
			}
		}

		testforth();

		ht_printf("[DBG] prom_load_boot_file...\n");
		if (!prom_load_boot_file()) {
			ht_printf("cannot find boot file.\n");
			return 1;
		}

		// this was your last chance to visit the config..
		delete gConfig;

		ppc_cpu_map_framebuffer(IO_GCARD_FRAMEBUFFER_PA_START, IO_GCARD_FRAMEBUFFER_EA);

		if (!gHeadless) {
			gDisplay->print("now starting client...");
			gDisplay->setAnsiColor(VCP(VC_WHITE, CONSOLE_BG));
		}
		ht_printf("[DBG] starting client PC=%08x...\n", ppc_cpu_get_pc(0));

		if (gHeadless) {
			// Headless: run CPU directly on main thread
			ppc_cpu_run();
		} else {
			// GUI: CPU in background thread, UI event loop on main thread
			sys_thread cpuThread;
			if (sys_create_thread(&cpuThread, 0, [](void *) -> void * {
				ppc_cpu_run();
				return NULL;
			}, NULL)) {
				ht_printf("can't create CPU thread!\n");
				exit(1);
			}
			runUI();
		}

		io_done();

	} catch (const std::exception &e) {
		ht_printf("main() caught exception: %s\n", e.what());
		return 1;
	} catch (const Exception &e) {
		String res;
		e.reason(res);
		ht_printf("main() caught exception: %y\n", &res);
		return 1;
	}

	if (!gHeadless) doneUI();
	doneOSAPI();
	doneData();
	doneAtom();
	return 0;
}
