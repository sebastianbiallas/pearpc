#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "system/types.h"

extern "C" void  NORETURN ht_assert_failed(const char *file, int line, const char *assertion)
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

void debugDumpMem(void *buf, int len)
{
	byte *p = (byte*)buf;
	while (len) {
		uint w = 16;
		uint m = w;
		if (m>len) m = len;
		for (uint i=0; i<m; i++) {
			printf("%02x ", *p);
			p++;
		}
		for (uint i=0; i<w-m; i++) {
			printf("   ");
		}
		p-=m;
		for (uint i=0; i<m; i++) {
			printf("%c", ((*p < 32) || (*p > 0x80)) ? '.' : *p);
			p++;
		}
		printf("\n");
		len -= m;
	}
}
