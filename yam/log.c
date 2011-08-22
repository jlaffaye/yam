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
#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#define _WITH_GETLINE
#include <stdio.h>
#include <unistd.h>

#include "yam.h"

#define LOG_FILETEMP ".yam.log.temp"
#define LOG_FILE ".yam.log"
#define LOG_EOF "-- YAM LOG EOF --"

FILE *
log_open(const char *dir)
{
	char path[MAXPATHLEN];

	snprintf(path, sizeof(path), "%s/%s", dir, LOG_FILETEMP);

	return fopen(path, "w");
}

int
log_entry_start(FILE *log, const char *name, const char *cmd)
{
	fprintf(log, "%s\n%s\n", name, cmd);
	return 0;
}

int
log_entry_dep(FILE *log, const char *path)
{
	fprintf(log, "%s\n", path);
	return 0;
}

int
log_entry_finish(FILE *log)
{
	fprintf(log, "\n");
	return 0;
}

int
log_close(FILE *fp, const char *dir)
{
	char from[MAXPATHLEN];
	char to[MAXPATHLEN];

	snprintf(from, sizeof(from), "%s/%s", dir, LOG_FILETEMP);
	snprintf(to, sizeof(to), "%s/%s", dir, LOG_FILE);

	fprintf(fp, "%s\n", LOG_EOF);

	fflush(fp);
	fsync(fileno(fp));
	fclose(fp);
	rename(from, to);

	return 0;
}

#define STATE_ENTRY 0
#define STATE_CMD 1
#define STATE_DEP 2
#define STATE_EOF 3

int
log_load(const char *dir, struct graph *g)
{
	char path[MAXPATHLEN];
	struct stat st;
	FILE *fp;
	char *line = NULL;
	size_t cap = 0;
	ssize_t len;
	unsigned int state = STATE_ENTRY;
	struct node *n = NULL;

	snprintf(path, sizeof(path), "%s/%s", dir, LOG_FILE);

	if (stat(path, &st) != 0) {
		if (errno == ENOENT) {
			return 0;
		} else {
			perrorf("stat(%s)", path);
			return -1;
		}
	}
	g->log_mtime = st.st_mtime;

	fp = fopen(path, "r");
	if (fp == NULL) {
		perrorf("fopen(%s)", path);
		return -1;
	}

	while((len = getline(&line, &cap, fp)) > 0) {
		if (line[len - 1] == '\n')
			line[len - 1] = '\0';

		if (state == STATE_ENTRY) {
			if (strcmp(line, LOG_EOF) == 0) {
				state = STATE_EOF;
			} else {
				/*
				 * If the entry does not exist in the graph, the current entry
				 * is old. Do not create a new node for this entry.
				 * This means that `n' might be NULL.
				 */
				n = graph_get(g, line, false);
				state = STATE_CMD;
			}
		} else if (state == STATE_CMD) {
			state = STATE_DEP;
			if (n != NULL && strcmp(n->cmd, line) != 0)
				n->new_cmd = 1;
		} else {
			if (line[0] != '\0') {
				if (n != NULL)
					graph_add_dep(g, n, line, NODE_DEP_IMPLICIT);
			} else {
				state = STATE_ENTRY;
				n = NULL;
			}
		}
	}
	fclose(fp);
	free(line);

	if (state != STATE_EOF) {
		fprintf(stderr, "log corrupted %d\n", state);
		return -1;
	}

	return 0;
}
