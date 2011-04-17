#include <sys/types.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

void send_data_ipc(void);

static int send_data_ipc_registered = 0;

FILE *
fopen(const char *path, const char *mode)
{
	static FILE * (*func)(const char *, const char *);
	FILE *fp;

	if (send_data_ipc_registered == 0) {
		send_data_ipc_registered = 1;
		atexit(send_data_ipc);
	}

	if (func == NULL)
		func = dlsym(RTLD_NEXT, "fopen");

	fp = func(path, mode);

	if (fp != NULL && mode[0] == 'r') {
		printf("fopen: %s (%s)\n", path, mode);
	}

	return fp;
}

int
open(const char *path, int flags, ...)
{
	static int (*func)(const char *, int, ...);
	int fd;
	int mode;
	va_list list;

	if (send_data_ipc_registered == 0) {
		send_data_ipc_registered = 1;
		atexit(send_data_ipc);
	}

	if (func == NULL)
		func = dlsym(RTLD_NEXT, "open");

	if (flags & O_CREAT) {
		va_start(list, flags);
		mode = va_arg(list, int);
		va_end(list);

		fd = func(path, flags, mode);
	} else
		fd = func(path, flags);

	if (fd > 0 && ((flags & O_RDONLY) == O_RDONLY || (flags & O_RDWR) == O_RDWR) &&
		(flags & O_CREAT) != O_CREAT && (flags & O_TRUNC) != O_TRUNC) {
		printf("open: %d -> %s\n", fd, path);
	}

	return fd;
}

void
send_data_ipc(void)
{
	/* TODO */
}
