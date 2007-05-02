/*****************************************************************************
 * RRDtool 1.2.23  Copyright by Tobi Oetiker, 1997-2007
 *****************************************************************************
 * rrd_lastupdate  Get the last datum entered for each DS
 *****************************************************************************/

#include "rrd_tool.h"
#include "rrd_rpncalc.h"
#include <stdarg.h>

int
rrd_lastupdate(int argc, char **argv, time_t *last_update,
                 unsigned long *ds_cnt, char ***ds_namv, char ***last_ds) {
    unsigned long i=0;
    char	 *filename;
    FILE         *in_file;
    rrd_t        rrd;

    if(argc < 2){
        rrd_set_error("please specify an rrd");
        return -1;
    }
    filename = argv[1];

    if(rrd_open(filename,&in_file,&rrd, RRD_READONLY)==-1){
	return(-1);
    }
    fclose(in_file);

    *last_update=rrd.live_head->last_up;
    *ds_cnt = rrd.stat_head->ds_cnt;
    if (((*ds_namv) =
    		(char **) malloc(rrd.stat_head->ds_cnt * sizeof(char*)))==NULL){
        rrd_set_error("malloc fetch ds_namv array");
	rrd_free(&rrd);
	return(-1);
    } 

    if (((*last_ds) =
    		(char **) malloc(rrd.stat_head->ds_cnt * sizeof(char*)))==NULL){
        rrd_set_error("malloc fetch last_ds array");
	rrd_free(&rrd);
	free(*ds_namv);
	return(-1);
    } 

    for(i=0;i<rrd.stat_head->ds_cnt;i++){
	(*ds_namv)[i] = sprintf_alloc("%s", rrd.ds_def[i].ds_nam);
	(*last_ds)[i] = sprintf_alloc("%s", rrd.pdp_prep[i].last_ds);
    }

    rrd_free(&rrd);
    return(0); 
}
