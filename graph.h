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

#ifndef _GRAPH_H
#define _GRAPH_H

#include <time.h>

#include "uthash.h"
#include "utlist.h"

#define NODE_FILE 0
#define NODE_JOB 1
#define NODE_DEP 2

struct jobs {
	struct node *head;
	struct node *tail;
};

struct nodes {
	struct node **nodes;
	size_t cap;
	size_t len;
};

struct node {
	unsigned int type :2;
	int todo :2;
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

void graph_init(void);
void graph_free(void);
struct node * graph_get(const char *key);
void graph_add_dep(struct node *n, const char *name);

unsigned int graph_compute(struct node **jobs);

void dump_graphviz(FILE *out);

#endif
