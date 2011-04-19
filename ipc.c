/*
 * Copyright (c) 2011, Julien P. Laffaye <jlaffaye@FreeBSD.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "err.h"
#include "ipc.h"

int
ipc_listen(void)
{
	struct sockaddr_un saun;
	char template[] = "/tmp/yam-socket.XXXXXX";
	char *path;
	int fd;

	path = mktemp(template);

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		err(1, "socket()");

	saun.sun_family = AF_UNIX;
	strlcpy(saun.sun_path, path, sizeof(saun.sun_path));

	if (bind(fd, (struct sockaddr *)&saun, sizeof(struct sockaddr_un)) < 0)
		err(1, "bind()");

	if (listen(fd, 1) < 0)
		err(1, "liten()");

	setenv("YAM_IPC", path, 1);
	/* FIXME */
	putenv("LD_PRELOAD=/home/jlaffaye/proj/yam/wrapper/libwrp.so");

	return fd;
}

void
ipc_close(int fd)
{
	close(fd);
	unlink(getenv("YAM_IPC"));
}

FILE *
ipc_accept(int fd)
{
	FILE *fp;
	int c;

	if ((c = accept(fd, NULL, NULL)) < 0)
		err(1, "accept()");

	if ((fp = fdopen(c, "r")) == NULL)
		err(1, "fdopen()");

	return fp;
}
