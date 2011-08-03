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

#include <sys/param.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "yam.h"

struct flags flags;

static void
clean(struct graph *g)
{
	struct node *n;

	for (n = g->index; n != NULL; n = n->hh.next) {
		/* Skip non jobs */
		if (n->type != NODE_JOB)
			continue;
		if (unlink(n->name) != 0) {
			if (errno != ENOENT && errno != ENOTDIR) {
				perrorf("unlink(%s)", n->name);
			}
		} else {
			printf("RM\t%s\n", n->name);
		}
	}
}

static int
get_root(char *root, size_t len)
{
	char path[MAXPATHLEN];
	char *slash = NULL;
	int found = 1;

	getcwd(root, len);

	for (;;) {
		snprintf(path, sizeof(path), "%s/Yamfile", root);
		if (access(path, F_OK) != 0)
			break;
		else
			found = 0;

		if ((slash = strrchr(root, '/')) != NULL)
			*slash = '\0';
		else
			break;
	}

	if (slash != NULL)
		*slash = '/';

	return found;
}

int
main(int argc, char **argv)
{
	struct graph g;
	char root[MAXPATHLEN];
	int ch;

	bzero(&flags, sizeof(struct flags));

	while ((ch = getopt(argc, argv, "clgj:")) != -1) {
		switch(ch) {
			case 'c':
				flags.clean = 1;
				break;
			case 'g':
				flags.graphviz = 1;
				break;
			case 'j':
				flags.jobs = (int)strtol(optarg, (char **)NULL, 10);
				if (flags.jobs == 0)
					fprintf(stderr, "wrong -j arg `%s'", optarg);
				break;
			case 'l':
				flags.lint = 1;
				break;
			case 'v':
				flags.verbose++;
				break;
		}
	}
	argc -= optind;
	argv += optind;

	if (flags.jobs == 0)
		flags.jobs = 1;

	if( get_root(root, sizeof(root)) != 0)
		die("can't find root");

	graph_init(&g);
	yamfile(&g, root);

	if (flags.clean == 1)
		clean(&g);
	else if (flags.graphviz == 1)
		dump_graphviz(&g, stdout);
	else
		do_jobs(&g, root);

	graph_free(&g);

	return 0;
}
