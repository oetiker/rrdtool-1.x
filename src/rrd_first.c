/*****************************************************************************
 * RRDtool 1.2.21  Copyright by Tobi Oetiker, 1997-2007
 *****************************************************************************
 * rrd_first Return
 *****************************************************************************
 * Initial version by Burton Strauss, ntopSupport.com - 3/2005
 *****************************************************************************/

#include "rrd_tool.h"


time_t
rrd_first(int argc, char **argv)
{
    int target_rraindex=0;
    char *endptr;
    optind = 0; opterr = 0;  /* initialize getopt */

    while (1){
        static struct option long_options[] =
        {
            {"rraindex", required_argument, 0, 129},
            {0,0,0,0}
        };
        int option_index = 0;
        int opt;
        opt = getopt_long(argc, argv, "", long_options, &option_index);

        if(opt == EOF)
            break;

        switch(opt) {
          case 129:
            target_rraindex=strtol(optarg,&endptr,0);
            if(target_rraindex < 0) {
                rrd_set_error("invalid rraindex number");
                return(-1);
            }
            break;
          default:
            rrd_set_error("usage rrdtool %s [--rraindex number] file.rrd", argv[0]);
            return(-1);
        }
    }

    if(optind >= argc){
        rrd_set_error("not enough arguments");
        return -1;       
    }     

    return(rrd_first_r(argv[optind], target_rraindex));
}


time_t
rrd_first_r(const char *filename, const int rraindex)
{
    long rra_start,
         timer;
    time_t then;
    FILE *in_file;
    rrd_t rrd;

    if(rrd_open(filename,&in_file,&rrd, RRD_READONLY)==-1){
        rrd_set_error("could not open RRD");
        return(-1);
    }

    if((rraindex < 0) || (rraindex >= (int)rrd.stat_head->rra_cnt)) {
        rrd_set_error("invalid rraindex number");
        rrd_free(&rrd);
        fclose(in_file);
        return(-1);
    }

    rra_start = ftell(in_file);    
    fseek(in_file,
          (rra_start +
           (rrd.rra_ptr[rraindex].cur_row+1) *
           rrd.stat_head->ds_cnt *
           sizeof(rrd_value_t)),
          SEEK_SET);
    timer = - (rrd.rra_def[rraindex].row_cnt-1);
    if (rrd.rra_ptr[rraindex].cur_row + 1 > rrd.rra_def[rraindex].row_cnt) {
      fseek(in_file,rra_start,SEEK_SET);
    }
    then = (rrd.live_head->last_up -
            rrd.live_head->last_up %
            (rrd.rra_def[rraindex].pdp_cnt*rrd.stat_head->pdp_step)) +
           (timer * 
            rrd.rra_def[rraindex].pdp_cnt*rrd.stat_head->pdp_step);

    rrd_free(&rrd);
    fclose(in_file);
    return(then);
}




