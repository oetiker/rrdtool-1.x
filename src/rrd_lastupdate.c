/*****************************************************************************
 * RRDtool 1.3.8  Copyright by Tobi Oetiker, 1997-2009
 *****************************************************************************
 * rrd_lastupdate  Get the last datum entered for each DS
 *****************************************************************************/

#include "rrd_tool.h"
#include "rrd_rpncalc.h"
#include <stdarg.h>

#ifdef WIN32
#include <stdlib.h>
#endif

int rrd_lastupdate(
    int argc,
    char **argv,
    time_t *last_update,
    unsigned long *ds_cnt,
    char ***ds_namv,
    char ***last_ds)
{
    unsigned long i = 0;
    char     *filename;
    rrd_t     rrd;
    rrd_file_t *rrd_file;

    if (argc < 2) {
        rrd_set_error("please specify an rrd");
        goto err_out;
    }
    filename = argv[1];

    rrd_file = rrd_open(filename, &rrd, RRD_READONLY);
    if (rrd_file == NULL)
        goto err_free;

    *last_update = rrd.live_head->last_up;
    *ds_cnt = rrd.stat_head->ds_cnt;
    if (((*ds_namv) =
         (char **) malloc(rrd.stat_head->ds_cnt * sizeof(char *))) == NULL) {
        rrd_set_error("malloc fetch ds_namv array");
        goto err_close;
    }

    if (((*last_ds) =
         (char **) malloc(rrd.stat_head->ds_cnt * sizeof(char *))) == NULL) {
        rrd_set_error("malloc fetch last_ds array");
        goto err_free_ds_namv;
    }

    for (i = 0; i < rrd.stat_head->ds_cnt; i++) {
        (*ds_namv)[i] = sprintf_alloc("%s", rrd.ds_def[i].ds_nam);
        (*last_ds)[i] = sprintf_alloc("%s", rrd.pdp_prep[i].last_ds);
    }

    rrd_free(&rrd);
    rrd_close(rrd_file);
    return (0);

  err_free_ds_namv:
    free(*ds_namv);
  err_close:
    rrd_close(rrd_file);
  err_free:
    rrd_free(&rrd);
  err_out:
    return (-1);
}
