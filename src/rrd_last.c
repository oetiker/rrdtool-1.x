/*****************************************************************************
 * RRDtool 1.2.2  Copyright by Tobi Oetiker, 1997-2005
 *****************************************************************************
 * rrd_last.c
 *****************************************************************************
 * Initial version by Russ Wright, @Home Network, 9/28/98
 *****************************************************************************/

#include "rrd_tool.h"

time_t
rrd_last(int argc, char **argv)
{
    if(argc < 2){
        rrd_set_error("please specify an rrd");
        return(-1);
    }

    return( rrd_last_r(argv[1]) );
}
 

time_t
rrd_last_r(const char *filename)
{
    FILE	*in_file;
    time_t       lastup;

    rrd_t	 rrd;

    if(rrd_open(filename, &in_file, &rrd, RRD_READONLY)==-1){
        return(-1);
    }
    lastup = rrd.live_head->last_up;
    rrd_free(&rrd);
    fclose(in_file);
    return(lastup);
}


