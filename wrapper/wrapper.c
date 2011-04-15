#include <sys/types.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

void send_data_ipc(void);

//FILE * fopen(const char *, const char *);
//int open(const char *, int, ...);
int close(int);
ssize_t read(int, void *, size_t);

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

	if (fp != NULL) {
		/* Do not bother with writes */
		if (mode[0] == 'r' || mode[1] == '+') {
			printf("fopen: %d -> %s\n", fileno(fp), path);
		}
	}

	return fp;
}

int
fclose(FILE *fp)
{
	static int (*func)(FILE *);
	int fd;
	int r;

	if (func == NULL)
		func = dlsym(RTLD_NEXT, "fclose");

	fd = fileno(fp);
	r = func(fp);

	if (r == 0) {
		printf("fclose: %d\n", fd);
	}

	return r;
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

	if (fd > 0)
		/* Do not bother with writes */
		if ((flags & O_RDONLY) == O_RDONLY || (flags & O_RDWR) == O_RDWR)
			printf("open: %d -> %s\n", fd, path);

	return fd;
}

int
close(int fd)
{
	static int (*func)(int);
	int r;

	if(func == NULL)
		func = dlsym(RTLD_NEXT, "close");

	r = func(fd);

	if (r == 0) {
		printf("close: %d\n", fd);
	}

	return r;
}

ssize_t
read(int fd, void *buf, size_t nbytes)
{
	ssize_t (*func)(int, void *, size_t) = NULL;

	if (func == NULL) {
		func = dlsym(RTLD_NEXT, "read");
	}

	printf("read: %d\n", fd);

	return func(fd, buf, nbytes);
}

void
send_data_ipc(void)
{
	/* TODO */
}
