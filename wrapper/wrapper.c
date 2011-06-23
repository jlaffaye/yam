#define _GNU_SOURCE /* required for dlfcn.h with glibc */`
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "utlist.h"

struct file {
	char *path;
	unsigned char mode;
	struct file *next;
};

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static struct file *files = NULL;

static void
ipc(void)
{
	struct file *f;
	struct sockaddr_un saun;
	char *path;
	char *child_id;
	FILE *fp;
	int fd;

	if ((path = getenv("YAM_IPC")) == NULL) {
		fprintf(stderr, "no YAM_IPC env var\n");
		exit(1);
	}

	if ((child_id = getenv("YAM_CHILD_ID")) == NULL) {
		fprintf(stderr, "no YAM_CHILD_ID env var\n");
		exit(1);
	}

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket()");
		exit(1);
	}

	bzero(&saun, sizeof(struct sockaddr_un));
	saun.sun_family = AF_UNIX;
	strcpy(saun.sun_path, path);

	if (connect(fd, (struct sockaddr *)&saun, sizeof(struct sockaddr_un)) < 0) {
		perror("connect()");
		exit(1);
	}

	fp = fdopen(fd, "r+");

	fprintf(fp, "%s\n", child_id);
	for (f = files; f != NULL; f = f->next)
		fprintf(fp, "%c %s\n", f->mode, f->path);

	fclose(fp);
}

static void
register_ipc(void)
{
	static int done;
	if (done == 0) {
		atexit(ipc);
		done = 1;
	}
}

static void
add_file(const char *path, unsigned char mode)
{
	struct file *f;

	f = calloc(1, sizeof(struct file));
	f->path = realpath(path, NULL);
	f->mode = mode;
	LL_PREPEND(files, f);
}

FILE *
fopen(const char *path, const char *mode)
{
	static FILE * (*func)(const char *, const char *);
	FILE *fp;

	pthread_mutex_lock(&m);
	register_ipc();

	if (func == NULL)
		func = dlsym(RTLD_NEXT, "fopen");

	fp = func(path, mode);

	if (fp != NULL) {
		if (mode[0] == 'r')
			add_file(path, 'r');
		else
			add_file(path, 'w');
	}

	pthread_mutex_unlock(&m);
	return fp;
}

int
open(const char *path, int flags, ...)
{
	static int (*func)(const char *, int, ...);
	int fd;
	int mode;
	va_list list;

	pthread_mutex_lock(&m);
	register_ipc();

	if (func == NULL)
		func = dlsym(RTLD_NEXT, "open");

	if (flags & O_CREAT) {
		va_start(list, flags);
		mode = va_arg(list, int);
		va_end(list);

		fd = func(path, flags, mode);
	} else
		fd = func(path, flags);

	if (fd > 0) {
		if (((flags & O_RDONLY) == O_RDONLY || (flags & O_RDWR) == O_RDWR) &&
		(flags & O_CREAT) != O_CREAT && (flags & O_TRUNC) != O_TRUNC)
			add_file(path, 'r');
		else
			add_file(path, 'w');
	}

	pthread_mutex_unlock(&m);
	return fd;
}
