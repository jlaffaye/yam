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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "graph.h"
#include "yamfile.h"

static int
add_target(lua_State *L)
{
	int i;
	int tlen;

	struct node *n;

	if(lua_gettop(L) != 3)
		luaL_error(L, "add_target: incorrect number of arguments");

	luaL_checktype(L, 1, LUA_TSTRING);
	luaL_checktype(L, 2, LUA_TSTRING);
	luaL_checktype(L, 3, LUA_TTABLE);

	n = graph_get(lua_tostring(L, 1));

	n->cmd = strdup(lua_tostring(L, 2));
	n->type = NODE_JOB;

	tlen = luaL_getn(L, 3);
	for (i = 1; i <= tlen; i++) {
		lua_rawgeti(L, 3, i);

		if (lua_type(L, 4) != LUA_TSTRING)
			luaL_error(L, "add_target: the table shall only"
				   " contain strings");

		graph_add_dep(n, lua_tostring(L, 4));

		lua_pop(L, 1);
	}

	return 0;
}

void
yamfile(void)
{
	lua_State *L;

	L = luaL_newstate();
	luaL_openlibs(L);
	lua_register(L, "add_target", add_target);

	if (luaL_dofile(L, "Yamfile") != 0)
		printf("luaL_dofile(): %s\n", lua_tostring(L, -1));

	lua_close(L);
}
