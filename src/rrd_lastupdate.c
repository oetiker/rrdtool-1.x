/*****************************************************************************
 * RRDtool 1.GIT, Copyright by Tobi Oetiker
 *                Copyright by Florian Forster, 2008
 *****************************************************************************
 * rrd_lastupdate  Get the last datum entered for each DS
 *****************************************************************************/

#ifdef __MINGW64__
#define __USE_MINGW_ANSI_STDIO 1    /* for %10llu */
#endif

#include "rrd_tool.h"
#include "rrd_rpncalc.h"
#include "rrd_client.h"
#include <stdarg.h>

int rrd_lastupdate (int argc, char **argv)
{
    struct optparse_long longopts[] = {
        {"daemon", 'd', OPTPARSE_REQUIRED},
        {0},
    };
    struct    optparse options;
    int       opt;
    time_t    last_update;
    char    **ds_names;
    char    **last_ds;
    unsigned long ds_count, i;
    int status;
    char *opt_daemon = NULL;

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
            	free(opt_daemon);
            }
            return -1;
        }
    }                   /* while (opt!=-1) */

    if ((options.argc - options.optind) != 1) {
        rrd_set_error ("Usage: rrdtool %s [--daemon|-d <addr>] <file>",
                options.argv[0]);
        if (opt_daemon != NULL) {
            free(opt_daemon);
        }
        return (-1);
    }

    status = rrdc_flush_if_daemon(opt_daemon, options.argv[options.optind]);
    if (opt_daemon != NULL) {
    	free (opt_daemon);
    }
    if (status) return (-1);

    status = rrd_lastupdate_r(options.argv[options.optind],
            &last_update, &ds_count, &ds_names, &last_ds);
    if (status != 0)
        return (status);

    for (i = 0; i < ds_count; i++)
        printf(" %s", ds_names[i]);
    printf ("\n\n");

#ifdef __MINGW64__
    printf ("%10llu:", last_update);    /* argument 2 has type 'time_t {aka long long int} */
#else
    printf ("%10lu:", last_update);
#endif
    for (i = 0; i < ds_count; i++) {
        printf(" %s", last_ds[i]);
        free(last_ds[i]);
        free(ds_names[i]);
    }
    printf("\n");

    free(last_ds);
    free(ds_names);

    return (0);
} /* int rrd_lastupdate */

int rrd_lastupdate_r(const char *filename,
        time_t *ret_last_update,
        unsigned long *ret_ds_count,
        char ***ret_ds_names,
        char ***ret_last_ds)
{
    unsigned long i = 0;
    rrd_t     rrd;
    rrd_file_t *rrd_file;

    rrd_init(&rrd);
    rrd_file = rrd_open(filename, &rrd, RRD_READONLY | RRD_LOCK);
    if (rrd_file == NULL) {
        rrd_free(&rrd);
        return (-1);
    }

    *ret_last_update = rrd.live_head->last_up;
    *ret_ds_count = rrd.stat_head->ds_cnt;
    *ret_ds_names = (char **) malloc (rrd.stat_head->ds_cnt * sizeof(char *));
    if (*ret_ds_names == NULL) {
        rrd_set_error ("malloc fetch ret_ds_names array");
        rrd_close (rrd_file);
        rrd_free (&rrd);
        return (-1);
    }
    memset (*ret_ds_names, 0, rrd.stat_head->ds_cnt * sizeof(char *));

    *ret_last_ds = (char **) malloc (rrd.stat_head->ds_cnt * sizeof(char *));
    if (*ret_last_ds == NULL) {
        rrd_set_error ("malloc fetch ret_last_ds array");
        free (*ret_ds_names);
        *ret_ds_names = NULL;
        rrd_close (rrd_file);
        rrd_free (&rrd);
        return (-1);
    }
    memset (*ret_last_ds, 0, rrd.stat_head->ds_cnt * sizeof(char *));

    for (i = 0; i < rrd.stat_head->ds_cnt; i++) {
        (*ret_ds_names)[i] = sprintf_alloc("%s", rrd.ds_def[i].ds_nam);
        (*ret_last_ds)[i] = sprintf_alloc("%s", rrd.pdp_prep[i].last_ds);

        if (((*ret_ds_names)[i] == NULL) || ((*ret_last_ds)[i] == NULL))
            break;
    }

    /* Check if all names and values could be copied and free everything if
     * not. */
    if (i < rrd.stat_head->ds_cnt) {
        rrd_set_error ("sprintf_alloc failed");
        for (i = 0; i < rrd.stat_head->ds_cnt; i++) {
            if ((*ret_ds_names)[i] != NULL)
            {
                free ((*ret_ds_names)[i]);
                (*ret_ds_names)[i] = NULL;
            }
            if ((*ret_last_ds)[i] != NULL)
            {
                free ((*ret_last_ds)[i]);
                (*ret_last_ds)[i] = NULL;
            }
        }
        free (*ret_ds_names);
        *ret_ds_names = NULL;
        free (*ret_last_ds);
        *ret_last_ds = NULL;
        rrd_close (rrd_file);
        rrd_free (&rrd);
        return (-1);
    }

    rrd_free(&rrd);
    rrd_close(rrd_file);
    return (0);
} /* int rrd_lastupdate_r */
