/*****************************************************************************
 * RRDtool 1.2.23  Copyright by Tobi Oetiker, 1997-2007
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
    time_t    lastup;
    rrd_file_t *rrd_file;

    rrd_t     rrd;

    rrd_file = rrd_open(filename, &rrd, RRD_READONLY);
    if (rrd_file == NULL)
        return (-1);

    lastup = rrd.live_head->last_up;
    rrd_free(&rrd);
    close(rrd_file->fd);
    rrd_close(rrd_file);
    return (lastup);
}
