/*****************************************************************************
 * RRDtool 1.3.8  Copyright by Tobi Oetiker, 1997-2009
 *****************************************************************************
 * rrd_last.c
 *****************************************************************************
 * Initial version by Russ Wright, @Home Network, 9/28/98
 *****************************************************************************/

#include "rrd_tool.h"

time_t rrd_last(
    int argc,
    char **argv)
{
    if (argc < 2) {
        rrd_set_error("please specify an rrd");
        return (-1);
    }

    return (rrd_last_r(argv[1]));
}


time_t rrd_last_r(
    const char *filename)
{
    time_t    lastup = -1;
    rrd_file_t *rrd_file;

    rrd_t     rrd;

    rrd_file = rrd_open(filename, &rrd, RRD_READONLY);
    if (rrd_file != NULL) {
        lastup = rrd.live_head->last_up;
        rrd_close(rrd_file);
    }
    rrd_free(&rrd);
    return (lastup);
}
