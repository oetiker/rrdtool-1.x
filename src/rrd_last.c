/*****************************************************************************
 * RRDtool 1.3.2  Copyright by Tobi Oetiker, 1997-2008
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

    optind = 0;
    opterr = 0;         /* initialize getopt */

    while (42) {
        int       opt;
        int       option_index = 0;
        static struct option long_options[] = {
            {"daemon", required_argument, 0, 'd'},
            {0, 0, 0, 0}
        };

        opt = getopt_long(argc, argv, "d:", long_options, &option_index);

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

        default:
            rrd_set_error ("Usage: rrdtool %s [--daemon <addr>] <file>",
                    argv[0]);
            return (-1);
            break;
        }
    }                   /* while (42) */

    if ((argc - optind) != 1) {
        rrd_set_error ("Usage: rrdtool %s [--daemon <addr>] <file>",
                argv[0]);
        return (-1);
    }

    if (opt_daemon == NULL)
    {
        char *temp;

        temp = getenv (ENV_RRDCACHED_ADDRESS);
        if (temp != NULL)
        {
            opt_daemon = strdup (temp);
            if (opt_daemon == NULL)
            {
                rrd_set_error("strdup failed.");
                return (-1);
            }
        }
    }

    if (opt_daemon != NULL)
    {
        int status;

        status = rrdc_connect (opt_daemon);
        if (status != 0)
        {
            rrd_set_error ("rrdc_connect failed with status %i.", status);
            return (-1);
        }

        status = rrdc_flush (argv[optind]);
        if (status != 0)
        {
            rrd_set_error ("rrdc_flush (%s) failed with status %i.",
                    argv[optind], status);
            return (-1);
        }

        rrdc_disconnect ();
    } /* if (opt_daemon) */

    return (rrd_last_r (argv[optind]));
}

time_t rrd_last_r(
    const char *filename)
{
    time_t    lastup = -1;
    rrd_file_t *rrd_file;

    rrd_t     rrd;

    rrd_file = rrd_open(filename, &rrd, RRD_READONLY);
    if (rrd_file != NULL) {
        lastup = rrd.live_head->last_up;
        rrd_close(rrd_file);
    }
    rrd_free(&rrd);
    return (lastup);
}
