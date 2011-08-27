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
#include <stdlib.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "yam.h"

/* No `void *data' for Lua callback... */
static struct graph *_g = NULL;
static size_t _rootlen = 0;
static struct subdir *_subdir = NULL;
static struct subdir *_subdirs = NULL;

static char *
get_path(const char *src, char *buf)
{
	char *p;

	if ((p = realpath(src, buf)) == NULL)
		die("realpath(%s)", src);
	p += _rootlen + 1;

	return p;
}

static struct subdir *
new_subdir(const char *path)
{
	struct subdir *s;

	s = calloc(1, sizeof(struct subdir));
	if (s == NULL)
		die("calloc()");

	strncpy(s->path, path, sizeof(s->path));

	return s;
}

static int
l_add_target(lua_State *L)
{
	int i;
	int tlen;
	char buf[PATH_MAX];
	const char *path;

	struct node *n;

	if(lua_gettop(L) != 3)
		luaL_error(L, "add_target: incorrect number of arguments");

	luaL_checktype(L, 1, LUA_TSTRING);
	luaL_checktype(L, 2, LUA_TSTRING);
	luaL_checktype(L, 3, LUA_TTABLE);

	path = get_path(lua_tostring(L, 1), buf);
	n = graph_get(_g, path, true);

	n->cmd = strdup(lua_tostring(L, 2));
	n->type = NODE_JOB;
	n->cwd = _subdir->path;

	tlen = luaL_getn(L, 3);
	for (i = 1; i <= tlen; i++) {
		lua_rawgeti(L, 3, i);

		if (lua_type(L, 4) != LUA_TSTRING)
			luaL_error(L, "add_target: the table shall only"
				   " contain strings");

		path = get_path(lua_tostring(L, 4), buf);
		graph_add_dep(_g, n, path, NODE_DEP_EXPLICIT);

		lua_pop(L, 1);
	}

	return 0;
}

static int
l_subdir(lua_State *L)
{
	char buf[PATH_MAX];
	const char *path;
	struct subdir *s;

	if(lua_gettop(L) != 1)
		luaL_error(L, "subdir: incorrect number of arguments");

	luaL_checktype(L, 1, LUA_TSTRING);
	path = get_path(lua_tostring(L, 1), buf);

	s = new_subdir(path);
	/*
	 * Prepend, so the last subdir is at the beginning.
	 */
	DL_PREPEND(_subdirs, s);

	return 0;
}

static void
visit_subdir(struct subdir *s, const char *root)
{
	struct subdir *tmp;
	lua_State *L;

	_subdir = s;
	_subdirs = NULL;

	if (chdir(s->path) != 0)
		die("chdir(%s)", s->path);

	L = luaL_newstate();
	luaL_openlibs(L);
	lua_register(L, "add_target", l_add_target);
	lua_register(L, "subdir", l_subdir);

	if (luaL_dofile(L, "Yamfile") != 0)
		diex("luaL_dofile(): %s\n", lua_tostring(L, -1));

	lua_close(L);

	/*
	 * _subdirs being a LIFO, we prepend so we restore the correct order
	 * at the beginning of to_visit.
	 */
	while (_subdirs != NULL) {
		tmp = _subdirs;
		DL_DELETE(_subdirs, tmp);
		DL_PREPEND(_g->to_visit, tmp);
	}

	if (chdir(root) != 0)
		die("chdir(%s)", root);
}

void
yamfile(struct graph *g, const char *root)
{
	struct subdir *s;

	/* init globals */
	_g = g;
	_rootlen = strlen(root);

	s = new_subdir(".");
	DL_APPEND(_g->to_visit, s);

	while(_g->to_visit != NULL) {
		s = _g->to_visit;
		visit_subdir(s, root);
		DL_DELETE(_g->to_visit, s);
		DL_APPEND(_g->subdirs, s);
	}
}
