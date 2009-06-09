/*
** Compat-5.1
** Copyright Kepler Project 2004-2006 (http://www.keplerproject.org/compat)
** $Id$

Compat-5.1 is free software: it can be used for both academic and commercial
purposes at absolutely no cost. There are no royalties or GNU-like
"copyleft" restrictions. Compat-5.1 qualifies as Open Source software. Its
licenses are compatible with GPL. Compat-5.1 is not in the public domain and
the Kepler Project keep its copyright. The legal details are below.

The spirit of the license is that you are free to use Compat-5.1 for any
purpose at no cost without having to ask us. The only requirement is that if
you do use Compat-5.1, then you should give us credit by including the
appropriate copyright notice somewhere in your product or its documentation.

The Compat-5.1 library is designed and implemented by Roberto Ierusalimschy,
Diego Nehab, André Carregal and Tomás Guisasola. The implementation is not
derived from licensed software.

*/

#include <stdio.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "compat-5.1.h"

static void getfield(lua_State *L, int idx, const char *name) {
    const char *end = strchr(name, '.');
    lua_pushvalue(L, idx);
    while (end) {
        lua_pushlstring(L, name, end - name);
        lua_gettable(L, -2);
        lua_remove(L, -2);
        if (lua_isnil(L, -1)) return;
        name = end+1;
        end = strchr(name, '.');
    }
    lua_pushstring(L, name);
    lua_gettable(L, -2);
    lua_remove(L, -2);
}

static void setfield(lua_State *L, int idx, const char *name) {
    const char *end = strchr(name, '.');
    lua_pushvalue(L, idx);
    while (end) {
        lua_pushlstring(L, name, end - name);
        lua_gettable(L, -2);
        /* create table if not found */
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushlstring(L, name, end - name);
            lua_pushvalue(L, -2);
            lua_settable(L, -4);
        }
        lua_remove(L, -2);
        name = end+1;
        end = strchr(name, '.');
    }
    lua_pushstring(L, name);
    lua_pushvalue(L, -3);
    lua_settable(L, -3);
    lua_pop(L, 2);
}

LUALIB_API void luaL_module(lua_State *L, const char *libname,
                              const luaL_reg *l, int nup) {
  if (libname) {
    getfield(L, LUA_GLOBALSINDEX, libname);  /* check whether lib already exists */
    if (lua_isnil(L, -1)) { 
      int env, ns;
      lua_pop(L, 1); /* get rid of nil */
      lua_pushliteral(L, "require");
      lua_gettable(L, LUA_GLOBALSINDEX); /* look for require */
      lua_getfenv(L, -1); /* getfenv(require) */
      lua_remove(L, -2); /* remove function require */
      env = lua_gettop(L);

      lua_newtable(L); /* create namespace for lib */
      ns = lua_gettop(L);
      getfield(L, env, "package.loaded"); /* get package.loaded table */
      if (lua_isnil(L, -1)) { /* create package.loaded table */
          lua_pop(L, 1); /* remove previous result */
          lua_newtable(L);
          lua_pushvalue(L, -1);
          setfield(L, env, "package.loaded");
      }
      else if (!lua_istable(L, -1))
        luaL_error(L, "name conflict for library `%s'", libname);
      lua_pushstring(L, libname);
      lua_pushvalue(L, ns); 
      lua_settable(L, -3); /* package.loaded[libname] = ns */
      lua_pop(L, 1); /* get rid of package.loaded table */
      lua_pushvalue(L, ns); /* copy namespace */
      setfield(L, LUA_GLOBALSINDEX, libname);
      lua_remove (L, env); /* remove env */
    }
    lua_insert(L, -(nup+1));  /* move library table to below upvalues */
  }
  for (; l->name; l++) {
    int i;
    lua_pushstring(L, l->name);
    for (i=0; i<nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -(nup+1));
    lua_pushcclosure(L, l->func, nup);
    lua_settable(L, -(nup+3));
  }
  lua_pop(L, nup);  /* remove upvalues */
}

