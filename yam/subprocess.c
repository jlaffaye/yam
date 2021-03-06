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
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "yam.h"

pid_t
popen2(const char *cmd, const char *cwd, int child_id, int *fd)
{
	int fildes[2];
	pid_t pid;

	if (fd == NULL)
		return -1;

	if (pipe(fildes) != 0) {
		perror("pipe()");
		return -1;
	}

	pid = fork();

	if (pid < 0) {
		perror("fork()");
		close(fildes[0]);
		close(fildes[1]);
		return -1;
	}

	/* child */
	if (pid == 0) {
		char e[20];
		close(fildes[0]);
		dup2(fildes[1], 1); /* stdout */
		dup2(fildes[1], 2); /* stderr */

		snprintf(e, sizeof(e), "YAM_CHILD_ID=%d", child_id);
		putenv(e);

		if (chdir(cwd) != 0)
			die("chdir(%s)", cwd);

		execl("/bin/sh", "sh", "-c", cmd, NULL);
		perror("execl()");
		exit(1);
	}

	close(fildes[1]);
	*fd = fildes[0];

	return pid;
}

int
pclose2(pid_t pid, int fd)
{
	int status;

	close(fd);
	waitpid(pid, &status, 0);

	return WEXITSTATUS(status);
}
