/*****************************************************************************
 * RRDtool 1.1.x  Copyright Tobias Oetiker, 1997 - 2002
 *****************************************************************************
 * rrd_error.c   Common Header File
 *****************************************************************************
 * $Id$
 * $Log$
 * Revision 1.2  2002/02/01 20:34:49  oetiker
 * fixed version number and date/time
 *
 * Revision 1.1.1.1  2001/02/25 22:25:05  oetiker
 * checkin
 *
 *************************************************************************** */

#include "rrd_tool.h"
#define MAXLEN 4096
static char rrd_error[MAXLEN] = "\0";
#include <stdarg.h>



void
rrd_set_error(char *fmt, ...)
{
    va_list argp;
    rrd_clear_error();
    va_start(argp, fmt);
#ifdef HAVE_VSNPRINTF
    vsnprintf((char *)rrd_error, MAXLEN-1, fmt, argp);
#else
    vsprintf((char *)rrd_error, fmt, argp);
#endif
    va_end(argp);
}

int
rrd_test_error(void) {
    return rrd_error[0] != '\0';
}

void
rrd_clear_error(void){
    rrd_error[0] = '\0';
}

char *
rrd_get_error(void){
    return (char *)rrd_error;
}











