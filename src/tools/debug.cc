#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
extern "C" void ht_assert_failed(const char *file, int line, const char *assertion)
{
	fprintf(stderr, "in file %s, line %d: assertion failed: %s\n", file, line, assertion);
#ifndef WIN32
#if 1
	fprintf(stderr, "sending SIGTRAP...");
	raise(SIGTRAP);
#endif
#endif
	exit(1);
}
