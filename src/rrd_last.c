/*****************************************************************************
 * RRDtool 1.9.0 Copyright by Tobi Oetiker, 1997-2024
 *****************************************************************************
 * rrd_last.c
 *****************************************************************************
 * Initial version by Russ Wright, @Home Network, 9/28/98
 *****************************************************************************/

#include "rrd_tool.h"
#include "rrd_client.h"

time_t rrd_last(
    int argc,
    const char **argv)
{
    char *opt_daemon = NULL;
    time_t lastupdate;
    struct optparse_long longopts[] = {
        {"daemon", 'd', OPTPARSE_REQUIRED},
        {0},
    };
    struct optparse options;
    int opt;

    optparse_init(&options, argc, argv);
    while ((opt = optparse_long(&options, longopts, NULL)) != -1) {
        switch (opt) {
        case 'd':
            if (opt_daemon != NULL) {
                    free (opt_daemon);
            }
            opt_daemon = strdup(options.optarg);
            if (opt_daemon == NULL)
            {
                rrd_set_error ("strdup failed.");
                return (-1);
            }
            break;

        case '?':
            rrd_set_error("%s", options.errmsg);
            if (opt_daemon != NULL) {
            	free (opt_daemon);
            }
            return -1;
        }
    }                   /* while (opt) */

    if ((options.argc - options.optind) != 1) {
        rrd_set_error ("Usage: rrdtool %s [--daemon|-d <addr>] <file>",
                options.argv[0]);
        if (opt_daemon != NULL) {
            free (opt_daemon);
        }
        return -1;
    }

    rrdc_connect (opt_daemon);
    if (rrdc_is_connected (opt_daemon))
        lastupdate = rrdc_last(options.argv[options.optind]);

    else
        lastupdate = rrd_last_r(options.argv[options.optind]);

    if (opt_daemon != NULL) {
    	free(opt_daemon);
    }
    return (lastupdate);
}

time_t rrd_last_r(
    const char *filename)
{
    time_t    lastup = -1;
    rrd_file_t *rrd_file;

    rrd_t     rrd;

    rrd_init(&rrd);
    rrd_file = rrd_open(filename, &rrd, RRD_READONLY | RRD_LOCK);
    if (rrd_file != NULL) {
        lastup = rrd.live_head->last_up;
        rrd_close(rrd_file);
    }
    rrd_free(&rrd);
    return (lastup);
}
