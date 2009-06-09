/*
** Compat-5.1
** Copyright Kepler Project 2004-2006 (http://www.keplerproject.org/compat/)
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

#ifndef COMPAT_H

LUALIB_API void luaL_module(lua_State *L, const char *libname,
                                       const luaL_reg *l, int nup);
#define luaL_openlib luaL_module

#endif
