/*
 *	PearPC
 *	sysbt.cc
 *
 *	Copyright (C) 2026 Sebastian Biallas (sb@biallas.net)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */

#include <execinfo.h>
#include <cstdio>
#include <unistd.h>

#include "system/sysbt.h"

void sys_print_backtrace(int max_frames)
{
	if (max_frames > 128) max_frames = 128;
	void *bt[128];
	int n = backtrace(bt, max_frames);
	fprintf(stderr, "  Backtrace (%d frames):\n", n);
	backtrace_symbols_fd(bt, n, STDERR_FILENO);
}
