/*
    cgi.h - Some simple routines for cgi programming
    Copyright (c) 1996-8  Martin Schulze <joey@infodrom.north.de>

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

#ifndef _CGI_H_
#define _CGI_H_

typedef struct cgi_s {
	char	*name,
		*value;
} s_cgi;

/* cgiHeader
 * 
 *  returns a valid CGI Header (Content-type...)
 */
void cgiHeader ();

/* cgiDebug
 * 
 *  Set/unsets debugging
 */
void cgiDebug (int level, int where);

/* cgiInit
 *
 *  Reads in variables set via POST or stdin
 */
s_cgi **cgiInit ();

/* cgiGetValue
 *
 *  Returns the value of the specified variable or NULL if it's empty
 *  or doesn't exist.
 */
char *cgiGetValue(s_cgi **parms, const char *var);

/* cgiRedirect
 *
 *  Provides a valid redirect for web pages.
 */
void cgiRedirect (const char *url);

#endif /* _CGI_H_ */
