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

#include "yam.h"

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
	assert(n->type == NODE_JOB);

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
node_compute(struct graph *g, struct node *n)
{
	struct stat st;
	size_t i;
	unsigned int nb = 0;

	assert(n->type == NODE_JOB);

	if (n->visited == 1) {
		return 0;
	}

	/*
	 * Mark it as visited early to avoid cycles
	 */
	n->visited = 1;

	/* depth first */
	for (i = 0; i < n->children.len; i++) {
		if (n->children.nodes[i]->type == NODE_JOB) {
			nb += node_compute(g, n->children.nodes[i]);
		}
	}

	/*
	 * The current node may have been marked to do during the depth
	 * traversal.
	 */
	if (n->todo != 0)
		return nb;

	/*
	 * If the command has changed, mark it to do now and avoid stat(2)
	 * calls.
	 */
	if (n->new_cmd == 1)
		return node_mark_todo(n);

	NODE_STAT(n, st);

	/*
	 * Test if this target is newer than the log file.
	 * If it is the case, we rebuild it to get another chance to discover its
	 * dependencies.
	 * If the mtime of the log file is < 0, there was no log file.
	 */
	if (g->log_mtime > 0 && n->mtime > g->log_mtime) {
		fprintf(stderr, "marking %s to do because it is newer than the log "
				"file (should not happen)\n", n->name);
		return node_mark_todo(n);
	}

	for (i = 0; i < n->children.len; i++) {
		NODE_STAT(n->children.nodes[i], st);
		if (n->children.nodes[i]->mtime > n->mtime)
			return node_mark_todo(n);
	}

	return 0;
}

void
graph_init(struct graph *g)
{
	g->index = NULL;
	g->subdirs = NULL;
	g->to_visit = NULL;
}

void
graph_free(struct graph *g)
{
	struct node *n, *tmp;
	struct subdir *subdir;

	HASH_ITER(hh, g->index, n, tmp) {
		HASH_DEL(g->index, n);
		free(n->name);
		free(n->cmd);
		free(n->children.nodes);
		free(n->parents.nodes);
		free(n);
	}

	while (g->subdirs != NULL) {
		subdir = g->subdirs;
		LL_DELETE(g->subdirs, subdir);
		free(subdir);
	}
}

struct node *
graph_get(struct graph *g, const char *key, bool create)
{
	struct node *n;
	char *name;

	name = (char *)key;

	HASH_FIND_STR(g->index, name, n);
	if (n == NULL && create == true) {
		n = calloc(1, sizeof(struct node));
		n->name = strdup(name);
		HASH_ADD_KEYPTR(hh, g->index, n->name, strlen(n->name), n);
	}

	return n;
}

void
graph_add_dep(struct graph *g, struct node *n, const char *name, int type)
{
	struct node *dep;

	dep = graph_get(g, name, true);

	/*
	 * If an implicit dep point to a job, it should be an explicit one.
	 * Do not add this one because it will confuse the user (and the lint
	 * option) if the build succeed with a missing dependency.
	 */
	if (dep->type == NODE_JOB && type == NODE_DEP_IMPLICIT)
		return;

	/* Do not overwrite the type */
	if (dep->type == NODE_UNKNOWN)
		dep->type = type;

	nodes_add(&n->children, dep);
	nodes_add(&dep->parents, n);
}

unsigned int
graph_compute(struct graph *g, struct node **jobs)
{
	struct node *n;
	struct node *tmp;
	unsigned int nb = 0;

	*jobs = NULL;

	HASH_ITER(hh, g->index, n, tmp) {
		if (n->todo != 0 || n->type != NODE_JOB)
			continue;

		nb += node_compute(g, n);
	}

	HASH_ITER(hh, g->index, n, tmp) {
		if (n->type == NODE_JOB && n->todo == 1 && n->waiting == 0)
			DL_APPEND(*jobs, n);
	}

	return nb;
}

int
graph_dump_log(struct graph *g, FILE *log)
{
	struct node *n;
	struct node *tmp;
	struct node *dep;
	size_t i;

	HASH_ITER(hh, g->index, n, tmp) {
		if (n->type == NODE_JOB && n->todo == 0) {
			log_entry_start(log, n->name, n->cmd);
			for (i = 0; i < n->children.len; i++) {
				dep = n->children.nodes[i];
				if (dep->type == NODE_DEP_IMPLICIT) {
					log_entry_dep(log, dep->name);
				}
			}
			log_entry_finish(log);
		}
	}
	return 0;
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
		if (n->children.len == 1) {
			fprintf(out, "\"%p\" -> \"%p\" [label=\" %s\"];\n",
					n->children.nodes[0], n, n->cmd);
		} else if (n->children.len > 1) {
			fprintf(out, "\"%p!\" [shape=ellipse, label=\"%s\"];\n", n, n->cmd);
			for (i = 0; i < n->children.len; i++) {
				fprintf(out, "\"%p\" -> \"%p!\";\n", n->children.nodes[i],
						n);
			}
			fprintf(out, "\"%p!\" -> \"%p\";", n, n);
		}
	}

	fprintf(out, "}\n");
}
