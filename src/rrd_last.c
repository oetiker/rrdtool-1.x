/*****************************************************************************
 * RRDtool 1.1.x  Copyright Tobias Oetiker, 1997 - 2002
 *****************************************************************************
 * rrd_last.c
 *****************************************************************************
 * Initial version by Russ Wright, @Home Network, 9/28/98
 *****************************************************************************/

#include "rrd_tool.h"

time_t
rrd_last(int argc, char **argv)
{
    FILE	*in_file;
    time_t       lastup;

    rrd_t	 rrd;

    if(argc < 2){
        rrd_set_error("please specify an rrd");
        return(-1);
    }
    if(rrd_open(argv[1], &in_file, &rrd, RRD_READONLY)==-1){
        return(-1);
    }
    lastup = rrd.live_head->last_up;
    rrd_free(&rrd);
    fclose(in_file);
    return(lastup);
}
 



