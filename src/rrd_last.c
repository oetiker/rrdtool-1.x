/*****************************************************************************
 * RRDtool 1.4.3  Copyright by Tobi Oetiker, 1997-2010
 *****************************************************************************
 * rrd_last.c
 *****************************************************************************
 * Initial version by Russ Wright, @Home Network, 9/28/98
 *****************************************************************************/

#include "rrd_tool.h"
#include "rrd_client.h"

time_t rrd_last(
    int argc,
    char **argv)
{
    char *opt_daemon = NULL;
    int status;
    time_t lastupdate;
    int flushfirst = 1;

    optind = 0;
    opterr = 0;         /* initialize getopt */

    while (42) {
        int       opt;
        int       option_index = 0;
        static struct option long_options[] = {
            {"daemon", required_argument, 0, 'd'},
            {"noflush", no_argument, 0, 'F'},
            {0, 0, 0, 0}
        };

        opt = getopt_long(argc, argv, "d:F", long_options, &option_index);

        if (opt == EOF)
            break;

        switch (opt) {
        case 'd':
            if (opt_daemon != NULL)
                    free (opt_daemon);
            opt_daemon = strdup (optarg);
            if (opt_daemon == NULL)
            {
                rrd_set_error ("strdup failed.");
                return (-1);
            }
            break;

        case 'F':
            flushfirst = 0;
            break;

        default:
            rrd_set_error ("Usage: rrdtool %s [--daemon <addr> [--noflush]] <file>",
                    argv[0]);
            return (-1);
            break;
        }
    }                   /* while (42) */

    if ((argc - optind) != 1) {
        rrd_set_error ("Usage: rrdtool %s [--daemon <addr> [--noflush]] <file>",
                argv[0]);
        return (-1);
    }

    if(flushfirst) {
    status = rrdc_flush_if_daemon(opt_daemon, argv[optind]);
    if (status) return (-1);
    }

    rrdc_connect (opt_daemon);
    if (rrdc_is_connected (opt_daemon))
        lastupdate = rrdc_last (argv[optind]);

    else
        lastupdate = rrd_last_r(argv[optind]);

    return (lastupdate);
}

time_t rrd_last_r(
    const char *filename)
{
    time_t    lastup = -1;
    rrd_file_t *rrd_file;

    rrd_t     rrd;

    rrd_init(&rrd);
    rrd_file = rrd_open(filename, &rrd, RRD_READONLY);
    if (rrd_file != NULL) {
        lastup = rrd.live_head->last_up;
        rrd_close(rrd_file);
    }
    rrd_free(&rrd);
    return (lastup);
}
