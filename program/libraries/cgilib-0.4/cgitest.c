/*
    cgitest.c - Testprogram for cgi.o
    Copyright (c) 1998  Martin Schulze <joey@infodrom.north.de>

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
 * Compile with: cc -o cgitest cgitest.c -lcgi
 */

#include <stdio.h>
#include <stdlib.h>
#include <cgi.h>

s_cgi **cgi;

void print_form()
{
    printf ("<h1>Test-Form</h1>\n");
    printf ("<form action=\"/cgi-bin/cgitest/insertdata\" method=post>\n");
    printf ("Input: <input name=string size=50>\n<br>");
    printf ("<select name=select multiple>\n<option>Nr. 1\n<option>Nr. 2\n<option>Nr. 3\n<option>Nr. 4\n</select>\n");
    printf ("Text: <textarea name=text cols=50>\n</textarea>\n");
    printf ("<center><input type=submit value=Submit> ");
    printf ("<input type=reset value=Reset></center>\n");
    printf ("</form>\n");
}

void eval_cgi()
{
    printf ("<h1>Results</h1>\n\n");
    printf ("<b>string</b>: %s<p>\n", cgiGetValue(cgi, "string"));
    printf ("<b>text</b>: %s<p>\n", cgiGetValue(cgi, "text"));
    printf ("<b>select</b>: %s<p>\n", cgiGetValue(cgi, "select"));
}


void main ()
{
    char *path_info = NULL;

    cgiDebug(0, 0);
    cgi = cgiInit();

    path_info = getenv("PATH_INFO");
    if (path_info) {
	if (!strcmp(path_info, "/redirect")) {
	    cgiRedirect("http://www.infodrom.north.de/");
	    exit (0);
	} else {
	    cgiHeader();
	    printf ("<html>\n<head><title>cgilib</title></title>\n\n<body>\n");
	    printf ("<h1>cgilib</h1>\n");
	    printf ("path_info: %s<br>\n", path_info);
	    if (!strcmp(path_info, "/insertdata")) {
		eval_cgi();
	    } else
		print_form();
	}
    } else {
	cgiHeader();
	printf ("<html>\n<head><title>cgilib</title></title>\n\n<body>\n");
	printf ("<h1>cgilib</h1>\n");
	print_form();
    }

    printf ("\n<hr>\n</body>\n</html>\n");
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
