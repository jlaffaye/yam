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
#include <stdlib.h>
#define _WITH_GETLINE
#include <stdio.h>
#include <unistd.h>
#include <poll.h>

#include "utstring.h"

#include "err.h"
#include "do.h"
#include "graph.h"
#include "ipc.h"
#include "log.h"
#include "subprocess.h"
#include "yamfile.h"

struct proc_info {
	pid_t pid;
	int fd;
	int retcode;
	UT_string *output;
	struct node *node;
	struct file *files;
};

struct state {
	struct node *jobs;
	unsigned int num_jobs;
	unsigned int num_done;
	int num_proc;
	int num_active;
	struct proc_info *pi;
	struct pollfd *pfd;
	const char *root;
	FILE *log;
};

struct file {
	char *path;
	unsigned char mode;
	struct file *next;
};

static int
start_job(struct state *s)
{
	struct proc_info *pi;
	struct node *n;
	int i;

	assert(s->jobs != NULL);

	/* find the first empty slot */
	for (i = 0; i < s->num_proc; i++)
		if (s->pi[i].fd == -1)
			break;
	assert(i < s->num_proc);
	assert(s->pi[i].fd == -1);

	pi = &s->pi[i];
	n = s->jobs;
	assert(n->type == NODE_JOB);
	if ((pi->pid = popen2(n->cmd, i, &pi->fd)) < 0) {
		perror("popen2()");
		return 1;
	}

	s->pfd[i + 1].fd = pi->fd;
	pi->node = n;
	s->num_active++;

	DL_DELETE(s->jobs, n);

	printf("[%d/%d] %s\n", s->num_done + s->num_active, s->num_jobs, n->name);

	return 0;
}

static int
finish_job(struct state *s, int i)
{
	struct proc_info *pi = &s->pi[i];
	struct node *n;
	struct node *np;
	struct file *f;
	size_t j;

	pi->retcode = pclose2(pi->pid, pi->fd);

	pi->pid = -1;
	pi->fd = s->pfd[i + 1].fd = -1;
	s->num_active--;

	if (pi->retcode != 0)
		return 1;

	/*
	 * Add jobs that were waiting for the current job to finish
	 */
	n = pi->node;
	for (j = 0; j < n->parents.len; j++) {
		np = n->parents.nodes[j];
		np->waiting--;
		if (np->waiting == 0 && np->todo == 1)
			DL_APPEND(s->jobs, np);
	}

	/*
	 * Add an entry to the log
	 */
	log_entry_start(s->log, n->name, n->cmd);
	LL_FOREACH(pi->files, f) {
		if (f->mode == 'r')
			log_entry_dep(s->log, f->path);
	}
	log_entry_finish(s->log);

	s->num_done++;
	pi->node = NULL;
	if (pi->output != NULL)
		utstring_clear(pi->output);
	while (pi->files != NULL) {
		f = pi->files;
		LL_DELETE(pi->files, f);
		free(f->path);
		free(f);
	}

	return 0;
}

static int
read_pipe(struct state *s, int i)
{
	ssize_t sz;
	char buf[8192];

	if ((sz = read(s->pi[i].fd, buf, sizeof(buf))) < 0) {
		perror("read()");
		finish_job(s, i);
		return 1;
	}

	/*
	 * Some systems return POLLIN when the pipe is closed, and the application
	 * has to check for EOF on the fd.
	 */
	if (sz == 0)
		return finish_job(s, i);
	else {
		if (s->pi[i].output == NULL)
			utstring_new(s->pi[i].output);
		utstring_bincpy(s->pi[i].output, buf, sz);
	}

	return 0;
}

static int
ipc(struct state *s)
{
	FILE *fp;
	char *line = NULL;
	size_t cap = 0;
	ssize_t len;
	int child_id = -1;
	struct node *n;
	struct node *dep;
	int found;
	struct file *f;
	unsigned char mode;
	char *path;

	fp = ipc_accept(s->pfd[0].fd);

	while ((len = getline(&line, &cap, fp)) > 0) {
		if (line[len - 1] == '\n')
			line[len - 1] = '\0';

		if (child_id == -1) {
			child_id = (int)strtol(line, NULL, 10);
			n = s->pi[child_id].node;
		} else {
			mode = line[0];
			assert(mode == 'r' || mode == 'w');
			path = line + 2;

			/* ignore if outside root */
			if (strncmp(path, s->root, strlen(s->root)) != 0)
				continue;

			/* ignore if already an explicit dep */
			found = 0;
			for (size_t i = 0; i < n->childs.len; i++) {
				dep = n->childs.nodes[i];
				if (dep->type != NODE_DEP_IMPLICIT && strcmp(dep->name, path) == 0) {
					found = 1;
					break;
				}
			}
			if (found == 1)
				continue;

			/* find if this file is in the list */
			for (f = s->pi[child_id].files; f != NULL; f = f->next)
				if (strcmp(f->path, path) == 0)
					break;

			if (f != NULL) {
				if (mode != 'r')
					f->mode = mode;
				continue;
			}

			f = calloc(1, sizeof(struct file));
			f->path = strdup(path);
			f->mode = mode;
			LL_PREPEND(s->pi[child_id].files, f);
		}
	}
	if (ferror(fp))
		perror("getline()");
	fclose(fp);
	free(line);

	return 0;
}

int
do_jobs(struct graph *g, int num_proc, char *root)
{
	struct state s = { NULL, 0, 0, num_proc, 0, NULL, NULL, root, NULL};
	struct proc_info *pi;
	int i;
	int error = 0;

	log_load(root, g);
	s.num_jobs = graph_compute(g, &s.jobs);

	/*
	 * Nothing to do, exit early
	 */
	if (s.num_jobs == 0)
		return 0;

	s.pi = calloc(num_proc, sizeof(struct proc_info));
	for (i = 0; i < num_proc; i++)
		s.pi[i].fd = -1;

	/* `num_proc' pipes + 1 unix socket*/
	s.pfd = malloc((s.num_proc + 1) * sizeof(struct pollfd));
	for (i = 0; i < (s.num_proc + 1); i++) {
		s.pfd[i].fd = -1;
		s.pfd[i].events = POLLIN;
	}

	s.log = log_open(root);
	if (s.log == NULL)
		die("can not open log file for writing");

	s.pfd[0].fd = ipc_listen(num_proc);

	/*
	 * Iterate as long as there are jobs to do/being done.
	 * If there is an error, we still want to wait for running jobs to finish.
	 */
	while (s.jobs != NULL || s.num_active > 0) {
		/*
		 * Launch new jobs if we have empty slots and if we have pending jobs.
		 * If there is an error, we do not want to launch new jobs.
		 */
		while (s.num_active < s.num_proc && s.jobs != NULL && error == 0)
			start_job(&s);

		assert(s.num_active > 0);

		/* poll running jobs + unix socket */
		if (poll(s.pfd, num_proc + 1, -1) < 0)
			die("poll()");

		/* special case for the unix socket */
		if (s.pfd[0].revents & POLLIN) {
			do {
				ipc(&s);
			} while (poll(s.pfd, 1, 0) > 0);
		}

		for (i = 1; i <= num_proc; i++)
			if (s.pfd[i].revents & POLLHUP)
				error += finish_job(&s, i - 1);
			else if (s.pfd[i].revents & POLLIN)
				error += read_pipe(&s, i - 1);
	}

	/*
	 * There is a cycle in the graph
	 */
	if (s.num_done != s.num_jobs) {
		fprintf(stderr, "There is a cycle in the graph!\n");
	}

	/*
	 * Print output generated by jobs that failed
	 */
	for (i = 0; i < num_proc; i++) {
		pi = &s.pi[i];
		if (pi->retcode != 0) {
			fprintf(stderr, "%s\n", pi->node->cmd);
			if (pi->output != NULL)
				fprintf(stderr, "%s\n", utstring_body(pi->output));
			fprintf(stderr, "*** Error code %d\n\n", pi->retcode);
		}
		if (pi->output != NULL)
			utstring_free(pi->output);
	}

	/*
	 * Finalize and close the log file
	 */
	graph_dump_log(g, s.log);
	log_close(s.log, root);

	ipc_close(s.pfd[0].fd);
	free(s.pfd);
	free(s.pi);
	return error;
}
