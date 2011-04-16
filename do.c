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
#include <sys/uio.h>

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

#include <stdio.h>

#include "do.h"
#include "graph.h"
#include "subprocess.h"
#include "yamfile.h"

struct proc_info {
	pid_t pid;
	char *buf;
	struct node *node;
};

int
do_jobs(int num_proc)
{
	struct node *jobs;
	struct node *n;
	unsigned int num_jobs;
	unsigned int num_done = 0;
	int num_active = 0;
	int i;
	int fd;
	ssize_t sz;
	char buf[8192];
	struct proc_info *pi;
	struct pollfd *pfd;

	num_jobs = graph_compute(&jobs);

	pi = malloc(num_proc * sizeof(struct proc_info));

	/* num_proc pipes  + 1 unix socket*/
	pfd = malloc((num_proc + 1) * sizeof(struct pollfd));
	for (i = 0; i < (num_proc + 1); i++) {
		pfd[i].fd = -1;
		pfd[i].events = POLLIN;
	}

	while (num_done != num_jobs) {

		/* launch new jobs */
		while (num_active <= num_proc && jobs != NULL) {
			n = jobs;

			if ((pi[i].pid = popen2(n->cmd, &fd)) < 0)
				err(1, "popen2()");

			/* find the first empty slot */
			for (i = 0; i < num_proc; i++) {
				if (pfd[i].fd == -1) {
					pfd[i].fd = fd;
					fd = -1;
					break;
				}
			}
			assert(fd == -1);

			DL_DELETE(jobs, n);
			num_active++;
			pi[i].node = n;

			printf("[%d/%d] %s\n", num_done + num_active, num_jobs, n->name);
		}

		assert(num_active > 0);

		/* poll running jobs */
		if (poll(pfd, num_proc + 1, -1) < 0)
			err(1, "poll()");

		for (i = 0; i < num_proc; i++) {
			if (pfd[i].revents & POLLIN) {
				sz = read(pfd[i].fd, buf, sizeof(buf));
				if (sz <= 0) {
					/* TODO: report error code != 0 */
					pclose2(pi[i].pid, pfd[i].fd);
					pfd[i].fd = -1;
					num_active--;
					num_done++;
				}
				write(1, buf, sz); /* TODO: store in pi[i].buf */
			}
		}
	}

	free(pfd);
	return 0;
}
