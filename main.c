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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "do.h"
#include "err.h"
#include "graph.h"
#include "subprocess.h"
#include "yamfile.h"

static void
clean(struct graph *g)
{
	struct node *n;

	for (n = g->index; n != NULL; n = n->hh.next) {
		/* Skip non jobs */
		if (n->childs.len == 0)
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

int
main(int argc, char **argv)
{
	struct graph g;
	int num_proc = 1;
	int cflag = 0;
	int gflag = 0;
	int ch;

	while ((ch = getopt(argc, argv, "cgj:")) != -1) {
		switch(ch) {
			case 'c':
				cflag = 1;
				break;
			case 'g':
				gflag = 1;
				break;
			case 'j':
				num_proc = (int)strtol(optarg, (char **)NULL, 10);
				if (num_proc == 0)
					fprintf(stderr, "wrong -j arg `%s'", optarg);
				break;
		}
	}
	argc -= optind;
	argv += optind;

	graph_init(&g);
	yamfile(&g);

	if (cflag == 1)
		clean(&g);
	else if (gflag == 1)
		dump_graphviz(&g, stdout);
	else
		do_jobs(&g, num_proc);

	graph_free(&g);

	return 0;
}
