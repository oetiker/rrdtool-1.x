/*****************************************************************************
 * RRDtool 1.7.2 Copyright by Tobi Oetiker
 *****************************************************************************
 * rrd_first Return
 *****************************************************************************
 * Initial version by Burton Strauss, ntopSupport.com - 3/2005
 *****************************************************************************/

#include <stdlib.h>
#include "rrd_tool.h"
#include "rrd_client.h"


time_t rrd_first(
    int argc,
    char **argv)
{
    struct optparse_long longopts[] = {
        {"rraindex", 129, OPTPARSE_REQUIRED},
        {"daemon", 'd', OPTPARSE_REQUIRED},
        {0},
    };
    struct    optparse options;
    int       opt;
    int       target_rraindex = 0;
    char     *endptr;
    char *opt_daemon = NULL;

    optparse_init(&options, argc, argv);
    while ((opt = optparse_long(&options, longopts, NULL)) != -1) {
        switch (opt) {
        case 129:
            target_rraindex = strtol(options.optarg, &endptr, 0);
            if (target_rraindex < 0) {
                rrd_set_error("invalid rraindex number");
                if (opt_daemon != NULL) {
                    free (opt_daemon);
                }
                return (-1);
            }
            break;
        case 'd':
            if (opt_daemon != NULL) {
                    free (opt_daemon);
            }
            opt_daemon = strdup(options.optarg);
            if (opt_daemon == NULL)
            {
                rrd_set_error ("strdup failed.");
                return -1;
            }
            break;
        case '?':
            rrd_set_error("%s", options.errmsg);
            if (opt_daemon != NULL) {
            	free (opt_daemon);
            }
            return -1;
        }
    }

    if (options.optind >= options.argc) {
        rrd_set_error("usage rrdtool %s [--rraindex number] [--daemon|-d <addr>] file.rrd",
                      options.argv[0]);
        if (opt_daemon != NULL) free (opt_daemon);
        return -1;
    }

    rrdc_connect (opt_daemon);
    if (rrdc_is_connected (opt_daemon)) {
      if (opt_daemon != NULL) {
        free (opt_daemon);
      }
      return rrdc_first(options.argv[options.optind], target_rraindex);
    } else {
      if (opt_daemon != NULL) {
      	free (opt_daemon);
      }
      return rrd_first_r(options.argv[options.optind], target_rraindex);
	}
}


time_t rrd_first_r(
    const char *filename,
    const int rraindex)
{
    off_t     rra_start, timer;
    time_t    then = -1;
    rrd_t     rrd;
    rrd_file_t *rrd_file;

    rrd_init(&rrd);
    rrd_file = rrd_open(filename, &rrd, RRD_READONLY | RRD_LOCK);
    if (rrd_file == NULL) {
        goto err_free;
    }

    if ((rraindex < 0) || (rraindex >= (int) rrd.stat_head->rra_cnt)) {
        rrd_set_error("invalid rraindex number");
        goto err_close;
    }

    rra_start = rrd_file->header_len;
    rrd_seek(rrd_file,
             (rra_start +
              (rrd.rra_ptr[rraindex].cur_row + 1) *
              rrd.stat_head->ds_cnt * sizeof(rrd_value_t)), SEEK_SET);
    timer = -(long)(rrd.rra_def[rraindex].row_cnt - 1);
    if (rrd.rra_ptr[rraindex].cur_row + 1 > rrd.rra_def[rraindex].row_cnt) {
        rrd_seek(rrd_file, rra_start, SEEK_SET);
    }
    then = (rrd.live_head->last_up -
            rrd.live_head->last_up %
            (rrd.rra_def[rraindex].pdp_cnt * rrd.stat_head->pdp_step)) +
        (timer * rrd.rra_def[rraindex].pdp_cnt * rrd.stat_head->pdp_step);
  err_close:
    rrd_close(rrd_file);
  err_free:
    rrd_free(&rrd);
    return (then);
}
