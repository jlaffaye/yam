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
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

#include <stdio.h>

#include "graph.h"
#include "subprocess.h"
#include "yamfile.h"

int
main(void)
{
	int num_jobs = 2;
	int active_jobs;
	int i;
	int fd;
	pid_t pid;
	ssize_t sz;
	char buf[8192];
	struct pollfd *pfd;
	struct node *jobs;

	graph_init();
	yamfile();
	//dump_graphviz(stdout);
	jobs = graph_compute();

	struct node *n;
	DL_FOREACH(jobs, n) {
		printf("%s\n", n->name);
	}

	graph_free();
	return 0;

	/* num_jobs pipes + 1 unix socket */
	pfd = malloc((num_jobs +1 ) * sizeof(struct pollfd));

	for (i = 0; i < num_jobs; i++) {
		popen2("dmesg", &fd);
		pfd[i].fd = fd;
		pfd[i].events = POLLIN;
	}

	active_jobs = num_jobs;
	while (active_jobs > 0 && poll(pfd, num_jobs, -1) >= 0) {
		for (i = 0; i < num_jobs; i++) {
			if (pfd[0].revents & POLLIN) {
				sz = read(pfd[i].fd, buf, sizeof(buf));
				if (sz <= 0) {
					pclose2(pid, pfd[i].fd);
					pfd[i].fd = -1;
					active_jobs--;
				}
				write(1, buf, sz);
			}
		}
	}

	free(pfd);

	return 0;
}
