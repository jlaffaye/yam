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

#include <stdio.h>
#include <unistd.h>

#include "log.h"

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
