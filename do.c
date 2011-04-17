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

struct state {
	struct node *jobs;
	unsigned int num_jobs;
	unsigned int num_done;
	int num_proc;
	int num_active;
	struct proc_info *pi;
	struct pollfd *pfd;
};

static int
start_job(struct state *s)
{
	struct node *n;
	int fd;
	int i;

	/* find the first empty slot */
	for (i = 0; i < s->num_proc; i++)
		if (s->pfd[i].fd == -1)
			break;
	assert(s->pfd[i].fd == -1);

	n = s->jobs;
	if ((s->pi[i].pid = popen2(n->cmd, i, &fd)) < 0) {
		perror("popen2()");
		return 1;
	}

	s->pfd[i].fd = fd;
	s->pi[i].node = n;
	s->num_active++;

	DL_DELETE(s->jobs, n);

	printf("[%d/%d] %s\n", s->num_done + s->num_active, s->num_jobs, n->name);
	return 0;
}

static int
finish_job(struct state *s, int i)
{
	struct node *n;
	struct node *np;
	int ret;
	size_t j;

	ret = pclose2(s->pi[i].pid, s->pfd[i].fd);

	s->pfd[i].fd = -1;
	s->num_active--;

	if (ret != 0) {
		fprintf(stderr, "*** Error code %d\n", ret);
		return 1;
	}

	s->num_done++;

	/*
	 * Add jobs that were waiting for the current job to finish
	 */
	n = s->pi[i].node;
	for (j = 0; j < n->parents.len; j++) {
		np = n->parents.nodes[j];
		np->waiting--;
		if (np->waiting == 0 && np->todo == 1)
			DL_APPEND(s->jobs, np);
	}
	return 0;
}

static int
read_pipe(struct state *s, int i)
{
	ssize_t sz;
	char buf[8192];

	if ((sz = read(s->pfd[i].fd, buf, sizeof(buf))) < 0) {
		perror("read()");
		finish_job(s, i);
		return 1;
	}

	/*
	 * If `sz' is 0, this is EOF so the process is terminated
	 */
	if (sz == 0)
		return finish_job(s, i);
	else
		write(1, buf, sz); /* TODO: store in pi[i].buf */

	return 0;
}

static int
ipc(struct state *s)
{
	/* TODO */
	return 0;
}

int
do_jobs(int num_proc)
{
	struct state s = { NULL, 0, 0, num_proc, 0, NULL, NULL};
	int i;
	int error = 0;

	s.num_jobs = graph_compute(&s.jobs);

	s.pi = malloc(num_proc * sizeof(struct proc_info));

	/* `num_proc' pipes + 1 unix socket*/
	s.pfd = malloc((s.num_proc + 1) * sizeof(struct pollfd));
	for (i = 0; i < (s.num_proc + 1); i++) {
		s.pfd[i].fd = -1;
		s.pfd[i].events = POLLIN;
	}

	/*
	 * Iterate as long as there are jobs to do/being done.
	 * If there is an error, we still want to wait for running jobs to finish.
	 */
	while (s.num_done != s.num_jobs && (error == 0 || s.num_active > 0)) {

		/*
		 * Launch new jobs if we have empty slots and if we have pending jobs.
		 * If there is an error, we do not want to launch new jobs.
		 */
		while (s.num_active <= s.num_proc && s.jobs != NULL && error == 0)
			start_job(&s);

		assert(s.num_active > 0);

		/* poll running jobs + unix socket */
		if (poll(s.pfd, num_proc + 1, -1) < 0)
			err(1, "poll()");

		for (i = 0; i < num_proc; i++)
			if (s.pfd[i].revents & POLLIN)
				error += read_pipe(&s, i);

		/* special case for the unix socket */
		if (s.pfd[num_proc].revents & POLLIN)
			ipc(&s);
	}

	free(s.pfd);
	free(s.pi);
	return error;
}
