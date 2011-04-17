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
#include <err.h> /* TODO: remove as BSD-specific */
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
	int error = 0;

	num_jobs = graph_compute(&jobs);

	pi = malloc(num_proc * sizeof(struct proc_info));

	/* `num_proc' pipes + 1 unix socket*/
	pfd = malloc((num_proc + 1) * sizeof(struct pollfd));
	for (i = 0; i < (num_proc + 1); i++) {
		pfd[i].fd = -1;
		pfd[i].events = POLLIN;
	}

	/*
	 * Iterate as long as there are jobs to do/being done.
	 * If there is an error, we still want to wait for running jobs to finish.
	 */
	while (num_done != num_jobs && (error == 0 || num_active > 0)) {

		/*
		 * Launch new jobs if we have empty slots and if we have pending jobs.
		 * If there is an error, we do not want to launch new jobs.
		 */
		while (num_active <= num_proc && jobs != NULL && error == 0) {
			/* find the first empty slot */
			for (i = 0; i < num_proc; i++)
				if (pfd[i].fd == -1)
					break;
			assert(pfd[i].fd == -1);

			n = jobs;
			if ((pi[i].pid = popen2(n->cmd, i, &fd)) < 0)
				err(1, "popen2()");

			pfd[i].fd = fd;
			pi[i].node = n;
			num_active++;

			DL_DELETE(jobs, n);

			printf("[%d/%d] %s\n", num_done + num_active, num_jobs, n->name);
		}

		assert(num_active > 0);

		/* poll running jobs */
		if (poll(pfd, num_proc + 1, -1) < 0)
			err(1, "poll()");

		for (i = 0; i < num_proc; i++) {
			if (pfd[i].revents & POLLIN) {
				if ((sz = read(pfd[i].fd, buf, sizeof(buf))) < 0) {
					error = 1;
					perror("read()");
				}
				/*
				 * If `sz' is 0, this is EOF so the process is terminated
				 */
				if (sz <= 0) {
					/* TODO: report error code != 0 */
					if (pclose2(pi[i].pid, pfd[i].fd) != 0) {
						error = 1;
					}
					pfd[i].fd = -1;
					num_active--;
					num_done++;
				}
				write(1, buf, sz); /* TODO: store in pi[i].buf */
			}
		}

		/* special case for the unix socket */
		if (pfd[num_proc].revents & POLLIN) {
		}
	}

	free(pfd);
	free(pi);
	return error;
}
