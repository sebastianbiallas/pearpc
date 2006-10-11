#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

int canonicalize(char *buf, int bufsize, char *cwd, char *fn)
{
	int cwdsize = strlen(cwd);
	int fnsize = strlen(fn);
	if (fn[0] == '/') {
		if (fnsize +1 > bufsize) return ERANGE;
		strcpy(buf, fn);
		return 0;
	}
	if (cwdsize + 1 + fnsize + 1 > bufsize) return ERANGE;
	strcpy(buf, cwd);
	strcat(buf, "/");
	strcat(buf, fn);
	return 0;
}

int main(int argc, char *argv[])
{
	setuid(0);
	char *relfilename = "ifppc_down";
	int relfilenamesize = strlen(relfilename);
	char cwdbuf[2048];
	if (!getcwd(cwdbuf, sizeof cwdbuf)) {
		printf("CWD name too long (>%d bytes). move to a higher level directory.\n", (int)sizeof cwdbuf);
		return 1;
	}
	char filename[2048+128];
	if (canonicalize(filename, sizeof filename, cwdbuf, argv[0])) {
		printf("unable to determine absolute executable filename. "
			"probably absolute filename too long (>%d bytes). "
			"move to a higher level directory.\n", (int) sizeof cwdbuf);
		return 1;
	}
	char *bs = strrchr(filename, '/');
	if (!bs) {
		printf("???\n");
		return 1;
	}
	int pathsize = bs-filename;
	if (pathsize + 1 + relfilenamesize + 1 > sizeof filename) {
		printf("absolute pathname too long (>%d bytes). "
			"move to a higher level directory.\n", (int) sizeof cwdbuf);
		return 1;
	}
	strcpy(bs+1, relfilename);
	printf("filename = %s\n", filename);
	struct stat s;
	if (stat(filename, &s)) {
		printf("can't stat file '%s': %s\n", filename, strerror(errno));
		return 1;
	}
	if (s.st_uid != 0) {
		printf("script '%s' must be owned by root (UID %d instead)\n", filename, s.st_uid);
		return 1;
	}
	if (s.st_mode & S_IWGRP) {
		printf("script '%s' must not be group-writable\n", filename);
		return 1;
	}
	if (s.st_mode & S_IWOTH) {
		printf("script '%s' must not be world-writable\n", filename);
		return 1;
	}
	execl("/bin/sh", "/bin/sh", filename, (char*)NULL);
	return 0;
}
