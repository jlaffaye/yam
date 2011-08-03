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

#ifndef _YAM_H
#define _YAM_H

#include <stdint.h>
#include <stdio.h> /* for FILE */
#include <time.h> /* for time_t */
#include <unistd.h> /* for pid_t */

#include "utlist.h"
#include "uthash.h"

struct flags {
	unsigned int clean :1;
	uint8_t verbose;
	unsigned int lint :1;
	unsigned int graphviz :1;
	int jobs;
};

extern struct flags flags;

#define NODE_UNKNOWN 0
#define NODE_JOB 1
#define NODE_DEP_EXPLICIT 2
#define NODE_DEP_IMPLICIT 3

struct jobs {
	struct node *head;
	struct node *tail;
};

struct graph {
	struct node *index;
};

struct nodes {
	struct node **nodes;
	size_t cap;
	size_t len;
};

struct node {
	unsigned int type :2;
	unsigned int todo :1;
	unsigned int visited :1;
	char *name;
	char *cmd;

	/*
	 * If > 0 this is the actual mtime.
	 * If 0, we don't know yet.
	 * If < 0, this file does not exist.
	 */
	time_t mtime;

	/*
	 * Represent the number of node that needs to be built before this node
	 * can be built.
	 * If 0, this node can be built.
	 */
	int waiting;

	/* Adjency list */
	struct nodes parents;
	struct nodes childs;

	/* Linked list of waiting jobs */
	struct node *next;
	struct node *prev;

	/* This structure is hashable to maintain an index in the root */
	UT_hash_handle hh;
};

/* graph */
void graph_init(struct graph *g);
void graph_free(struct graph *g);
struct node * graph_get(struct graph *g, const char *key);
void graph_add_dep(struct graph *g, struct node *n, const char *name, int type);

unsigned int graph_compute(struct graph *g, struct node **jobs);

int graph_dump_log(struct graph *g, FILE *log);

void dump_graphviz(struct graph *g, FILE *out);

/* yamfile */
void yamfile(struct graph *g, const char *root);

/* do */
int do_jobs(struct graph *g, char *root);

/* subprocess */
pid_t popen2(const char *cmd, int child_id, int *fd);
int pclose2(pid_t pid, int fd);

/* ipc */
int ipc_listen(int num_clients);
void ipc_close(int fd);
FILE * ipc_accept(int fd);

/* log */
FILE * log_open(const char *dir);
int log_entry_start(FILE *log, const char *name, const char *cmd);
int log_entry_dep(FILE *log, const char *path);
int log_entry_finish(FILE *log);
int log_close(FILE *fp, const char *dir);

int log_load(const char *dir, struct graph *g);

/* err */
void perrorf(const char *fmt, ...);
void die(const char *fmt, ...);
void diex(const char *fmt, ...);
void info(uint8_t level, const char *fmt, ...);

#endif
