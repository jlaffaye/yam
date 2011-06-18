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
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "graph.h"

static void
nodes_add(struct nodes *ns, struct node *n)
{
	if (ns->len >= ns->cap) {
		if (ns->cap == 0)
			ns->cap = 2;
		else
			ns->cap *= 2;
		ns->nodes = realloc(ns->nodes, sizeof(struct node) * ns->cap);
	}
	ns->nodes[ns->len] = n;
	ns->len++;
}

static unsigned int
node_mark_todo(struct node *n)
{
	size_t i;
	unsigned int nb = 1;

	/*
	 * If we mark a node more than once, we will increment its parents more
	 * than needed!
	 */
	assert(n->todo != 1);

	n->todo = 1;

	for (i = 0; i < n->parents.len; i++) {
		if (n->parents.nodes[i]->todo != 1)
			nb += node_mark_todo(n->parents.nodes[i]);

		n->parents.nodes[i]->waiting++;
	}

	return nb;
}

/* stat(2) only if necessary */
#define NODE_STAT(n, st)													\
	if (n->mtime == 0) {													\
		errno = 0;															\
		if (stat(n->name, &st) == 0)										\
			n->mtime = st.st_mtime;											\
		else if (errno == ENOENT)											\
			n->mtime = -1;													\
		else																\
			perror("stat()");												\
	}																		\

static unsigned int
node_compute(struct node *n)
{
	struct stat st;
	size_t i;
	unsigned int nb = 0;

	/* depth first */
	for (i = 0; i < n->childs.len; i++) {
		nb += node_compute(n->childs.nodes[i]);
	}

	if (n->todo != 0)
		return nb;

	NODE_STAT(n, st);

	for (i = 0; i < n->childs.len; i++) {
		NODE_STAT(n->childs.nodes[i], st);
		if (n->childs.nodes[i]->mtime > n->mtime)
			return node_mark_todo(n);
	}

	n->todo = -1;
	return 0;
}

void
graph_init(struct graph *g)
{
	g->index = NULL;
}

void
graph_free(struct graph *g)
{
	struct node *n, *tmp;

	HASH_ITER(hh, g->index, n, tmp) {
		HASH_DEL(g->index, n);
		free(n->name);
		free(n->cmd);
		free(n->childs.nodes);
		free(n->parents.nodes);
		free(n);
	}
}

struct node *
graph_get(struct graph *g, const char *key)
{
	struct node *n;
	char *name;

	name = (char *)key;

	HASH_FIND_STR(g->index, name, n);
	if (n == NULL) {
		n = calloc(1, sizeof(struct node));
		n->name = strdup(name);
		HASH_ADD_KEYPTR(hh, g->index, n->name, strlen(n->name), n);
	}
	return n;
}

void
graph_add_dep(struct graph *g, struct node *n, const char *name)
{
	struct node *dep;

	dep = graph_get(g, name);

	nodes_add(&n->childs, dep);
	nodes_add(&dep->parents, n);
}

unsigned int
graph_compute(struct graph *g, struct node **jobs)
{
	struct node *n;
	unsigned int nb = 0;

	*jobs = NULL;

	for (n = g->index; n != NULL; n = n->hh.next) {
		if (n->todo != 0)
			continue;

		nb += node_compute(n);

		if (n->todo == 1 && n->waiting == 0)
			DL_APPEND(*jobs, n);

	}

	return nb;
}

void
dump_graphviz(struct graph *g, FILE *out)
{
	struct node *n;
	size_t i;

	fprintf(out, "digraph yam {\n");
	fprintf(out, "node [fontsize=10, shape=box, height=0.25]\n");
	fprintf(out, "edge [fontsize=10]\n");

	for (n = g->index; n != NULL; n = n->hh.next) {
		fprintf(out, "\"%p\" [label=\"%s\"];\n", n, n->name);
		if (n->childs.len == 1) {
			fprintf(out, "\"%p\" -> \"%p\" [label=\" %s\"];\n",
					n->childs.nodes[0], n, n->cmd);
		} else if (n->childs.len > 1) {
			fprintf(out, "\"%p!\" [shape=ellipse, label=\"%s\"];\n", n, n->cmd);
			for (i = 0; i < n->childs.len; i++) {
				fprintf(out, "\"%p\" -> \"%p!\";\n", n->childs.nodes[i],
						n);
			}
			fprintf(out, "\"%p!\" -> \"%p\";", n, n);
		}
	}

	fprintf(out, "}\n");
}
