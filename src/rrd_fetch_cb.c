#include "rrd_tool.h"
#include "unused.h"
#include <time.h>


static rrd_fetch_cb_t callback = NULL;

int rrd_fetch_cb_register(rrd_fetch_cb_t cb){
     callback = cb;
     /* if we find a way to figure out of cb provided is to our
        liking or not, we could return 0 if there is a problem */
     return 1;
}

int
rrd_fetch_fn_cb(
    const char     *filename,  /* name of the rrd */
    enum cf_en     cf_idx, /* consolidation function */
    time_t         *start,
    time_t         *end,       /* which time frame do you want ?
			        * will be changed to represent reality */
    unsigned long  *step,      /* which stepsize do you want?
				* will be changed to represent reality */
    unsigned long  *ds_cnt,    /* number of data sources in file */
    char           ***ds_namv, /* names of data_sources */
    rrd_value_t    **data)     /* two dimensional array containing the data */
{
    int ret;
    if (callback){
        ret = callback(filename,cf_idx,start,end,step,ds_cnt,ds_namv,data);
        if (*start > *end){
            rrd_set_error("Your callback returns a start after end. start: %lld end: %lld",(long long int)*start,(long long int)*end);
            return -1;
        }
        if (*step == 0){
            rrd_set_error("Your callback returns a step of 0");
            return -1;
        }
        return ret;
    }
    else {
        rrd_set_error("use rrd_fetch_cb_register to register your callback prior to calling rrd_fetch_fn_cb");
        return -1;
     }
}
