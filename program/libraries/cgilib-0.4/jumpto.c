/*
   jumpto.c - Jump to a given URL
   Copyright (c) 1998  Martin Schulze <joey@orgatech.de>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111, USA.
 */

/*
 * Compile with: cc -o jumpto jumpto.c -lcgi
 *
 * To include this in your web pages you'll need something
 * like this:
 *
 * <form action="/cgi-bin/jumpto">
 * </form>
 * <select>
 * <option value="/this/is/my/url.html">My URL
 * <option value="http://www.debian.org/">Debian GNU/Linux
 * <option value="http://www.debian.org/OpenHardware/">Open Hardware
 * <option value="http://www.opensource.org/">Open Source
 * </select>
 * <input type=submit value="Jump">
 * </form>
 */

#include <stdlib.h>
#include <stdio.h>
#include <cgi.h>

s_cgi **cgiArg;

void main()
{
    char *url;
    char *server_url = NULL;

    cgiDebug(0,0);
    cgiArg = cgiInit ();

    server_url = getenv("SERVER_URL");
    if ((url = cgiGetValue(cgiArg, "url")) == NULL) {
	if (server_url)
	    cgiRedirect(server_url);
	else
	    cgiRedirect("/");
    } else
	cgiRedirect(url);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
