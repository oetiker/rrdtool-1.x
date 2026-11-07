/*****************************************************************************
 * RRDtool 1.11.0 Copyright by Tobi Oetiker, 1997-2026
 *****************************************************************************
 * change header parameters of an rrd
 *****************************************************************************
 * $Id$
 * $Log$
 * Revision 1.6  2004/05/26 22:11:12  oetiker
 * reduce compiler warnings. Many small fixes. -- Mike Slifcak <slif@bellsouth.net>
 *
 * Revision 1.5  2002/02/01 20:34:49  oetiker
 * fixed version number and date/time
 *
 * Revision 1.4  2001/08/22 22:29:07  jake
 * Contents of this patch:
 * (1) Adds/revises documentation for rrd tune in rrd_tool.c and pod files.
 * (2) Moves some initialization code from rrd_create.c to rrd_hw.c.
 * (3) Adds another pass to smoothing for SEASONAL and DEVSEASONAL RRAs.
 * This pass computes the coefficients as deviations from an average; the
 * average is added the baseline coefficient of HWPREDICT. Statistical texts
 * suggest this to preserve algorithm stability. It will not invalidate
 * RRD files created and smoothed with the old code.
 * (4) Adds the aberrant-reset flag to rrd tune. This operation, which is
 * specified for a single data source, causes the holt-winters algorithm to
 * forget everything it has learned and start over.
 * (5) Fixes a few out-of-date code comments.
 *
 * Revision 1.3  2001/03/07 21:21:54  oetiker
 * complete rewrite of rrdgraph documentation. This also includes info
 * on upcoming/planned changes to the rrdgraph interface and functionality
 * -- Alex van den Bogaerdt <alex@slot.hollandcasino.nl>
 *
 * Revision 1.2  2001/03/04 13:01:55  oetiker
 * Aberrant Behavior Detection support. A brief overview added to rrdtool.pod.
 * Major updates to rrd_update.c, rrd_create.c. Minor update to other core files.
 * This is backwards compatible! But new files using the Aberrant stuff are not readable
 * by old rrdtool versions. See http://cricket.sourceforge.net/aberrant/rrd_hw.htm
 * -- Jake Brutlag <jakeb@corp.webtv.net>
 *
 *****************************************************************************/

#include <stdlib.h>
#include <locale.h>

#include "rrd_strtod.h"
#include "rrd_tool.h"
#include "rrd_rpncalc.h"
#include "rrd_hw.h"
#include "rrd_modify.h"
#include "rrd_create.h"
#include "rrd_client.h"

/* How tune checks the value of a Holt-Winters or FAILURES parameter */
enum tune_value_kind {
    TUNE_HW,                    /* 0 < value < 1 */
    TUNE_HW_SMOOTH,             /* 0 <= value <= 1, needs a version 4 header */
    TUNE_DELTA,                 /* value >= 0.1 */
    TUNE_WINDOW                 /* 1 .. MAX_FAILURES_WINDOW_LEN */
};

/* A parameter of a Holt-Winters or FAILURES RRA.  Entries with an option
   letter belong to the old options, which tune the one RRA of cf[0] or
   cf[1]; entries without one are the names RRA#<index>:<name>=<value>
   takes. */
typedef struct tune_param {
    int       opt;
    const char *optname;
    const char *name;
    enum rra_par_en par;
    enum tune_value_kind kind;
    enum cf_en cf[2];
    const char *cf_names;
    const char *missing;
} tune_param_t;

/* One parameter change for one RRA, checked and ready to apply */
typedef struct tune_change {
    const tune_param_t *param;
    unsigned long rra_idx;
    unival    value;
} tune_change_t;

static int tune_prepare(
    rrd_t *rrd,
    int argc,
    const char **argv,
    const struct optparse_long *longopts,
    tune_change_t **changes,
    unsigned long *change_cnt,
    int *smoothing_window);
static const tune_param_t *tune_option_param(
    int opt);
static int tune_option(
    rrd_t *rrd,
    const tune_param_t *param,
    const char *arg,
    int apply);
static int tune_apply(
    rrd_t *rrd,
    const tune_change_t *change);
static int is_rra_param_arg(
    const char *arg);

int rrd_tune(
    int argc,
    const char **argv)
{
    char      *opt_daemon = NULL;
    const char *in_filename = NULL;
    struct optparse_long longopts[] = {
        {"heartbeat", 'h', OPTPARSE_REQUIRED},
        {"minimum", 'i', OPTPARSE_REQUIRED},
        {"maximum", 'a', OPTPARSE_REQUIRED},
        {"data-source-type", 'd', OPTPARSE_REQUIRED},
        {"data-source-rename", 'r', OPTPARSE_REQUIRED},
        /* added parameter tuning options for aberrant behavior detection */
        {"deltapos", 'p', OPTPARSE_REQUIRED},
        {"deltaneg", 'n', OPTPARSE_REQUIRED},
        {"window-length", 'w', OPTPARSE_REQUIRED},
        {"failure-threshold", 'f', OPTPARSE_REQUIRED},
        {"alpha", 'x', OPTPARSE_REQUIRED},
        {"beta", 'y', OPTPARSE_REQUIRED},
        {"gamma", 'z', OPTPARSE_REQUIRED},
        {"gamma-deviation", 'v', OPTPARSE_REQUIRED},
        {"smoothing-window", 's', OPTPARSE_REQUIRED},
        {"smoothing-window-deviation", 'S', OPTPARSE_REQUIRED},
        {"aberrant-reset", 'b', OPTPARSE_REQUIRED},
	// integration of rrd_modify functionality.
        {"step", 't', OPTPARSE_REQUIRED},
	/* unfortunately, '-d' is already taken */
        {"daemon", 'D', OPTPARSE_REQUIRED},
        {0}
    };
    struct optparse options;
    int opt;
    int status = -1;

    rrd_thread_init();

    /* before we open the input RRD, we should flush it from any caching
    daemon, because we might totally rewrite it later on */

    /* for this, we FIRST have to find the daemon, this means we must parse options twice... */
    
    optparse_init(&options, argc, argv);
    while ((opt = optparse_long(&options, longopts, NULL)) != -1) {
	switch (opt) {
	case 'D':
            if (opt_daemon != NULL)
		free (opt_daemon);
            opt_daemon = strdup (options.optarg);
            if (opt_daemon == NULL)
            {
                rrd_set_error ("strdup failed.");
                return (-1);
            }
            break;
	default:
	    break;
	}
    }
    
    // connect to daemon (will take care of environment variable automatically)
    /* rrdc_connect() has set "Unable to connect to rrdcached: ..." */
    if (rrdc_connect(opt_daemon) != 0) {
        free(opt_daemon);
        return -1;
    }

    if (opt_daemon) {
	free(opt_daemon);
	opt_daemon = NULL;
    }

    if (!options.optind || options.optind >= options.argc) {
	// missing file name...
	rrd_set_error("missing file name");
	goto done;
    }
    
    /* NOTE: optparse_long reorders argv and places all NON option arguments to
    the back, starting with optind. This means the file name has travelled to
    options.argv[options.optind] */
    
    in_filename = options.argv[options.optind];
    
    if (rrdc_is_any_connected()) {
	// is it a good idea to just ignore the error ????
	rrdc_flush(in_filename);
	rrd_clear_error();
    }

    if (rrdc_is_any_connected ())
	    status = rrdc_tune (in_filename, argc, (const char **) argv);

    else
	    status = rrd_tune_r(in_filename, argc, (const char **) argv);
    
done:
    if (in_filename && rrdc_is_any_connected()) {
        // save any errors....
        char *e = strdup(rrd_get_error());
	// is it a good idea to just ignore the error ????
	rrdc_forget(in_filename);
	rrd_clear_error();
        
        if (e) {
            rrd_set_error(e);
            free(e);
        } else
            rrd_set_error("error message was lost (out of memory)");
    }
    return status;
}

int rrd_tune_r(
    const char* in_filename,
    int argc,
    const char **argv)
{
    rrd_t     rrd;
    int       matches;
    int       optcnt = 0;
    long      ds;
    char      ds_nam[DS_NAM_SIZE];
    char      ds_new[DS_NAM_SIZE];
    long      heartbeat;
    double    min = 0;
    double    max = 0;
    char      dst[DST_SIZE];
    int       rc = -1;
    int       opt_newstep = -1;
    rrd_file_t *rrd_file = NULL;
    char      double_str[ 41 ] = {0};
    struct optparse_long longopts[] = {
        {"heartbeat", 'h', OPTPARSE_REQUIRED},
        {"minimum", 'i', OPTPARSE_REQUIRED},
        {"maximum", 'a', OPTPARSE_REQUIRED},
        {"data-source-type", 'd', OPTPARSE_REQUIRED},
        {"data-source-rename", 'r', OPTPARSE_REQUIRED},
        /* added parameter tuning options for aberrant behavior detection */
        {"deltapos", 'p', OPTPARSE_REQUIRED},
        {"deltaneg", 'n', OPTPARSE_REQUIRED},
        {"window-length", 'w', OPTPARSE_REQUIRED},
        {"failure-threshold", 'f', OPTPARSE_REQUIRED},
        {"alpha", 'x', OPTPARSE_REQUIRED},
        {"beta", 'y', OPTPARSE_REQUIRED},
        {"gamma", 'z', OPTPARSE_REQUIRED},
        {"gamma-deviation", 'v', OPTPARSE_REQUIRED},
        {"smoothing-window", 's', OPTPARSE_REQUIRED},
        {"smoothing-window-deviation", 'S', OPTPARSE_REQUIRED},
        {"aberrant-reset", 'b', OPTPARSE_REQUIRED},
	// integration of rrd_modify functionality.
        {"step", 't', OPTPARSE_REQUIRED},
	/* unfortunately, '-d' is already taken */
        {"daemon", 'D', OPTPARSE_REQUIRED},
        {0}
    };
    struct optparse options;
    int opt;
    tune_change_t *changes = NULL;
    unsigned long change_cnt = 0;
    unsigned long change;
    int       smoothing_window = 0;
    const char **modify_argv = NULL;
    int       modify_argc = 0;
    int       arg_idx;

    /* Fix CWE-457 */
    memset(&rrd, 0, sizeof(rrd_t));
    rrd_init(&rrd);
    rrd_file = rrd_open(in_filename, &rrd, RRD_READWRITE | RRD_LOCK |
                                           RRD_READAHEAD | RRD_READVALUES);
    if (rrd_file == NULL) {
	goto done;
    }

    /* Check the Holt-Winters options and the RRA#<index>:<name>=<value>
       arguments before anything changes in the file */
    if (tune_prepare(&rrd, argc, argv, longopts, &changes, &change_cnt,
                     &smoothing_window) != 0)
        goto done;

    /* A smoothing window needs a version 4 header.  Files older than
       version 3 store the live header as a bare time_t, so relabelling
       them in place would make every later read misplace the rest of the
       file.  Rewrite such a file in the current layout first, as
       rrdtool restore does, then tune the rewritten file. */
    if (atoi(rrd.stat_head->version) < atoi(RRD_VERSION3)
        && smoothing_window) {
        char      old_version[sizeof(rrd.stat_head->version)];

        /* write_rrd() relabels the header in memory; with mmap that
           memory is the original file, so put the label back if the
           rewrite fails. */
        memcpy(old_version, rrd.stat_head->version, sizeof(old_version));
        if (write_rrd(in_filename, &rrd) != 0) {
            memcpy(rrd.stat_head->version, old_version, sizeof(old_version));
            if (!rrd_test_error())
                rrd_set_error("cannot convert '%s' to RRD version %s",
                              in_filename, RRD_VERSION3);
            goto done;
        }
        rrd_close(rrd_file);
        rrd_free(&rrd);
        rrd_init(&rrd);
        rrd_file = rrd_open(in_filename, &rrd, RRD_READWRITE | RRD_LOCK |
                                               RRD_READAHEAD | RRD_READVALUES);
        if (rrd_file == NULL) {
            goto done;
        }
    }

    optparse_init(&options, argc, argv); /* re-initialize optparse */
    while ((opt = optparse_long(&options, longopts, NULL)) != -1) {
        unsigned int strtod_ret_val;

        optcnt++;
        switch (opt) {
        case 'h':
            if ((matches =
		 sscanf(options.optarg, DS_NAM_FMT ":%ld", ds_nam,
			&heartbeat)) != 2) {
                rrd_set_error("invalid arguments for heartbeat");
		goto done;
            }
            if ((ds = ds_match(&rrd, ds_nam)) == -1) {
		goto done;
            }
            rrd.ds_def[ds].par[DS_mrhb_cnt].u_cnt = heartbeat;
            break;

        case 'i':
            matches = sscanf(options.optarg, DS_NAM_FMT ":%40[U0-9.e+-]", ds_nam, double_str);
            if ( matches == 2 ) {
                if (strcmp(double_str,"U") == 0){
                    min = DNAN;
                }
                else {
                    strtod_ret_val = rrd_strtodbl( double_str, NULL, &min, NULL );
                    if ((strtod_ret_val != 2)) {
                        rrd_set_error("invalid arguments for minimum ds value");
                        goto done;
                    }
                }
            }
            else {
                rrd_set_error("invalid arguments for minimum ds value");
                goto done;
            } 
            if ((ds = ds_match(&rrd, ds_nam)) == -1) {
		goto done;
            }
            rrd.ds_def[ds].par[DS_min_val].u_val = min;
            break;

        case 'a':
            matches = sscanf(options.optarg, DS_NAM_FMT ":%40[U0-9.e+-]", ds_nam, double_str);
            if ( matches == 2 ) {
                if (strcmp(double_str,"U") == 0){
                    max = DNAN;
                }
                else {
                    strtod_ret_val = rrd_strtodbl( double_str, NULL, &max, NULL );
                    if ((strtod_ret_val != 2)) {
                        rrd_set_error("invalid arguments for maximum ds value");
                        goto done;
                    }
                }
            }
            else {
                rrd_set_error("invalid arguments for maximum ds value");
                goto done;
            } 
            if ((ds = ds_match(&rrd, ds_nam)) == -1) {
		goto done;
            }
            rrd.ds_def[ds].par[DS_max_val].u_val = max;
            break;
            
        case 'd':
            if ((matches =
                 sscanf(options.optarg, DS_NAM_FMT ":" DST_FMT, ds_nam, dst)) != 2) {
                rrd_set_error("invalid arguments for data source type");
		goto done;
	    }
            if ((ds = ds_match(&rrd, ds_nam)) == -1) {
		goto done;
            }
            if ((int) dst_conv(dst) == -1) {
		goto done;
            }
            /* only reset when something is changed */
            if (strncmp(rrd.ds_def[ds].dst, dst, DST_SIZE - 1) != 0) {
                strncpy(rrd.ds_def[ds].dst, dst, DST_SIZE - 1);
                rrd.ds_def[ds].dst[DST_SIZE - 1] = '\0';

                rrd.pdp_prep[ds].last_ds[0] = 'U';
                rrd.pdp_prep[ds].last_ds[1] = 'N';
                rrd.pdp_prep[ds].last_ds[2] = 'K';
                rrd.pdp_prep[ds].last_ds[3] = 'N';
                rrd.pdp_prep[ds].last_ds[4] = '\0';
            }
            break;
        case 'r':
            if ((matches =
                 sscanf(options.optarg, DS_NAM_FMT ":" DS_NAM_FMT, ds_nam,
                        ds_new)) != 2) {
                rrd_set_error("invalid arguments for data source type");
		goto done;
            }
            if ((ds = ds_match(&rrd, ds_nam)) == -1) {
		goto done;
            }
            strncpy(rrd.ds_def[ds].ds_nam, ds_new, DS_NAM_SIZE);
            rrd.ds_def[ds].ds_nam[DS_NAM_SIZE - 1] = '\0';
            break;
        case 'p':
        case 'n':
        case 'f':
        case 'w':
        case 'x':
        case 'y':
        case 'z':
        case 'v':
            if (tune_option(&rrd, tune_option_param(opt), options.optarg, 1))
                goto done;
            break;
        case 'b':
            if (sscanf(options.optarg, DS_NAM_FMT, ds_nam) != 1) {
                rrd_set_error("invalid argument for aberrant-reset");
		goto done;
            }
            if ((ds = ds_match(&rrd, ds_nam)) == -1) {
                /* ds_match handles it own errors */
		goto done;
            }
            reset_aberrant_coefficients(&rrd, rrd_file, (unsigned long) ds);
            if (rrd_test_error()) {
		goto done;
            }
            break;
        case 's':
        case 'S':
            if (tune_option(&rrd, tune_option_param(opt), options.optarg, 1))
                goto done;
            break;
	case 't':
	    opt_newstep = atoi(options.optarg);
	    break;
	case 'D':
	    // ignore, handled in previous argv parsing round
	    break;
        case '?':
            rrd_set_error("%s", options.errmsg);
	    goto done;
        }
    }
    /* RRA#<index>:<name>=<value> arguments apply after the options, so
       they win over an option that tunes the same RRA */
    for (change = 0; change < change_cnt; change++) {
        if (tune_apply(&rrd, &changes[change]) != 0)
            goto done;
    }
    optcnt += change_cnt;

    if (optcnt > 0) {
        rrd_seek(rrd_file, 0, SEEK_SET);
        rrd_write(rrd_file, rrd.stat_head, sizeof(stat_head_t) * 1);
        rrd_write(rrd_file, rrd.ds_def,
                  sizeof(ds_def_t) * rrd.stat_head->ds_cnt);
        /* need to write rra_defs for RRA parameter changes */
        rrd_write(rrd_file, rrd.rra_def,
                  sizeof(rra_def_t) * rrd.stat_head->rra_cnt);
        /* --data-source-type resets last_ds in pdp_prep, which follows
           the live header.  With mmap the change already sits in the
           mapped file; without mmap it only exists in memory, so write
           the live header and pdp_prep back as well. */
        if (rrd.legacy_last_up != NULL)
            rrd_write(rrd_file, rrd.legacy_last_up, sizeof(time_t));
        else
            rrd_write(rrd_file, rrd.live_head, sizeof(live_head_t));
        rrd_write(rrd_file, rrd.pdp_prep,
                  sizeof(pdp_prep_t) * rrd.stat_head->ds_cnt);
    }
    
    if (options.optind >= options.argc) {
        int       i;

        for (i = 0; i < (int) rrd.stat_head->ds_cnt; i++)
            if (dst_conv(rrd.ds_def[i].dst) != DST_CDEF) {
                printf("DS[%s] typ: %s\thbt: %lu\tmin: %1.4f\tmax: %1.4f\n",
                       rrd.ds_def[i].ds_nam,
                       rrd.ds_def[i].dst,
                       rrd.ds_def[i].par[DS_mrhb_cnt].u_cnt,
                       rrd.ds_def[i].par[DS_min_val].u_val,
                       rrd.ds_def[i].par[DS_max_val].u_val);
            } else {
                char     *buffer = NULL;

                rpn_compact2str((rpn_cdefds_t *) &
                                (rrd.ds_def[i].par[DS_cdef]), rrd.ds_def,
                                &buffer);
                printf("DS[%s] typ: %s\tcdef: %s\n", rrd.ds_def[i].ds_nam,
                       rrd.ds_def[i].dst, buffer);
                if (buffer) free(buffer);
            }
    }

    /* the RRA#<index>:<name>=<value> arguments are done; hand the
       structural changes to rrd_modify, which rewrites the file */
    modify_argv = (const char **) malloc(sizeof(char *) * (options.argc + 1));
    if (modify_argv == NULL) {
        rrd_set_error("out of memory");
        goto done;
    }
    for (arg_idx = options.optind + 1; arg_idx < options.argc; arg_idx++) {
        if (!is_rra_param_arg(options.argv[arg_idx]))
            modify_argv[modify_argc++] = options.argv[arg_idx];
    }
    if (handle_modify(&rrd, in_filename, modify_argc, modify_argv, 0,
                      opt_newstep) < 0)
        goto done;
    rc = 0;
done:
    if (rrd_file) {
	rrd_close(rrd_file);
    }
    rrd_free(&rrd);
    free(changes);
    free((void *) modify_argv);
    return rc;
}

#define HW_MISSING "Holt-Winters RRA does not exist in this RRD"
#define FAILURES_MISSING "Failures RRA does not exist in this RRD"

static const tune_param_t tune_params[] = {
    /* the old options */
    {'x', "--alpha", "alpha", RRA_hw_alpha, TUNE_HW,
     {CF_HWPREDICT, CF_MHWPREDICT}, NULL, HW_MISSING},
    {'y', "--beta", "beta", RRA_hw_beta, TUNE_HW,
     {CF_HWPREDICT, CF_MHWPREDICT}, NULL, HW_MISSING},
    {'z', "--gamma", "gamma", RRA_seasonal_gamma, TUNE_HW,
     {CF_SEASONAL, CF_SEASONAL}, NULL, HW_MISSING},
    {'v', "--gamma-deviation", "gamma", RRA_seasonal_gamma, TUNE_HW,
     {CF_DEVSEASONAL, CF_DEVSEASONAL}, NULL, HW_MISSING},
    {'s', "--smoothing-window", "smoothing-window",
     RRA_seasonal_smoothing_window, TUNE_HW_SMOOTH,
     {CF_SEASONAL, CF_SEASONAL}, NULL, HW_MISSING},
    {'S', "--smoothing-window-deviation", "smoothing-window",
     RRA_seasonal_smoothing_window, TUNE_HW_SMOOTH,
     {CF_DEVSEASONAL, CF_DEVSEASONAL}, NULL, HW_MISSING},
    {'p', "--deltapos", "deltapos", RRA_delta_pos, TUNE_DELTA,
     {CF_FAILURES, CF_FAILURES}, NULL, FAILURES_MISSING},
    {'n', "--deltaneg", "deltaneg", RRA_delta_neg, TUNE_DELTA,
     {CF_FAILURES, CF_FAILURES}, NULL, FAILURES_MISSING},
    {'f', "--failure-threshold", "failure-threshold", RRA_failure_threshold,
     TUNE_WINDOW, {CF_FAILURES, CF_FAILURES}, NULL, FAILURES_MISSING},
    {'w', "--window-length", "window-length", RRA_window_len, TUNE_WINDOW,
     {CF_FAILURES, CF_FAILURES}, NULL, FAILURES_MISSING},
    /* RRA#<index>:<name>=<value>; the CF of the RRA picks the meaning */
    {0, NULL, "alpha", RRA_hw_alpha, TUNE_HW,
     {CF_HWPREDICT, CF_MHWPREDICT}, "HWPREDICT and MHWPREDICT", NULL},
    {0, NULL, "beta", RRA_hw_beta, TUNE_HW,
     {CF_HWPREDICT, CF_MHWPREDICT}, "HWPREDICT and MHWPREDICT", NULL},
    {0, NULL, "gamma", RRA_seasonal_gamma, TUNE_HW,
     {CF_SEASONAL, CF_DEVSEASONAL}, "SEASONAL and DEVSEASONAL", NULL},
    {0, NULL, "smoothing-window", RRA_seasonal_smoothing_window,
     TUNE_HW_SMOOTH, {CF_SEASONAL, CF_DEVSEASONAL},
     "SEASONAL and DEVSEASONAL", NULL},
    {0, NULL, "deltapos", RRA_delta_pos, TUNE_DELTA,
     {CF_FAILURES, CF_FAILURES}, "FAILURES", NULL},
    {0, NULL, "deltaneg", RRA_delta_neg, TUNE_DELTA,
     {CF_FAILURES, CF_FAILURES}, "FAILURES", NULL},
    {0, NULL, "failure-threshold", RRA_failure_threshold, TUNE_WINDOW,
     {CF_FAILURES, CF_FAILURES}, "FAILURES", NULL},
    {0, NULL, "window-length", RRA_window_len, TUNE_WINDOW,
     {CF_FAILURES, CF_FAILURES}, "FAILURES", NULL},
};

#define TUNE_PARAM_CNT (sizeof(tune_params) / sizeof(tune_params[0]))

/* The parameter an old option tunes, NULL for any other option */
static const tune_param_t *tune_option_param(
    int opt)
{
    size_t    i;

    for (i = 0; i < TUNE_PARAM_CNT; i++) {
        if (tune_params[i].opt != 0 && tune_params[i].opt == opt)
            return &tune_params[i];
    }
    return NULL;
}

/* Read and check the value of a parameter.  The checks are those the
   options have always made. */
static int tune_parse_value(
    const tune_param_t *param,
    const char *arg,
    unival *value)
{
    double    dbl;
    long      cnt;
    char     *end;

    /* rrd_strtodbl returns 2 only when it converted the whole string;
       1 means a number followed by other characters.  It also returns 2
       for "nan" and "inf", which no range check below would catch, as
       every comparison with NaN is false. */
    switch (param->kind) {
    case TUNE_HW:
        if (rrd_strtodbl(arg, NULL, &dbl, NULL) != 2) {
            rrd_set_error("Unable to parse Holt-Winters parameter '%s'", arg);
            return -1;
        }
        if (!isfinite(dbl)) {
            rrd_set_error("%s must be a finite number, not '%s'",
                          param->name, arg);
            return -1;
        }
        if (dbl <= 0.0 || dbl >= 1.0) {
            rrd_set_error("Holt-Winters parameter must be between 0 and 1");
            return -1;
        }
        value->u_val = dbl;
        return 0;
    case TUNE_HW_SMOOTH:
        if (rrd_strtodbl(arg, NULL, &dbl, NULL) != 2) {
            rrd_set_error("Unable to parse Holt-Winters parameter '%s'", arg);
            return -1;
        }
        if (!isfinite(dbl)) {
            rrd_set_error("%s must be a finite number, not '%s'",
                          param->name, arg);
            return -1;
        }
        /* 0.0 turns the smoothing of SEASONAL or DEVSEASONAL off */
        if (dbl < 0.0 || dbl > 1.0) {
            rrd_set_error("Holt-Winters parameter must be between 0 and 1");
            return -1;
        }
        value->u_val = dbl;
        return 0;
    case TUNE_DELTA:
        if (rrd_strtodbl(arg, NULL, &dbl, NULL) != 2) {
            rrd_set_error("Unable to parse %s value '%s'", param->name, arg);
            return -1;
        }
        if (!isfinite(dbl)) {
            rrd_set_error("%s must be a finite number, not '%s'",
                          param->name, arg);
            return -1;
        }
        if (dbl < 0.1) {
            rrd_set_error("Parameter specified is too small");
            return -1;
        }
        value->u_val = dbl;
        return 0;
    case TUNE_WINDOW:
        errno = 0;
        cnt = strtol(arg, &end, 10);
        if (end == arg || *end != '\0' || errno != 0) {
            rrd_set_error("Unable to parse %s value '%s'", param->name, arg);
            return -1;
        }
        if (cnt < 1 || cnt > MAX_FAILURES_WINDOW_LEN) {
            rrd_set_error("Parameter must be between %d and %d",
                          1, MAX_FAILURES_WINDOW_LEN);
            return -1;
        }
        value->u_cnt = (unsigned long) cnt;
        return 0;
    }
    rrd_set_error("unknown parameter kind");
    return -1;
}

static int tune_rra_fits(
    const rrd_t *rrd,
    unsigned long rra_idx,
    const tune_param_t *param)
{
    enum cf_en cf = rrd_cf_conv(rrd->rra_def[rra_idx].cf_nam);

    return cf == param->cf[0] || cf == param->cf[1];
}

/* Find the one RRA an old option tunes.  When several RRAs fit, the
   option cannot know which one is meant; it names them and points to
   RRA#<index>:<name>=<value>. */
static int tune_find_rra(
    const rrd_t *rrd,
    const tune_param_t *param,
    unsigned long *rra_idx)
{
    unsigned long i;
    unsigned long found = 0;
    char      list[256] = "";
    size_t    len = 0;
    int       n;

    for (i = 0; i < rrd->stat_head->rra_cnt; i++) {
        if (!tune_rra_fits(rrd, i, param))
            continue;
        if (found == 0)
            *rra_idx = i;
        if (len < sizeof(list) - 32) {
            n = snprintf(list + len, sizeof(list) - len, "%sRRA#%lu",
                         found > 0 ? ", " : "", i);
            if (n > 0)
                len += (size_t) n;
        } else if (len < sizeof(list)) {
            snprintf(list + len, sizeof(list) - len, ", ...");
            len = sizeof(list);
        }
        found++;
    }
    if (found == 0) {
        rrd_set_error("%s", param->missing);
        return -1;
    }
    if (found > 1) {
        rrd_set_error("%s fits %lu RRAs (%s); "
                      "use RRA#<index>:%s=<value> to tune one of them",
                      param->optname, found, list, param->name);
        return -1;
    }
    return 0;
}

/* Check an old option and, with apply set, make the change */
static int tune_option(
    rrd_t *rrd,
    const tune_param_t *param,
    const char *arg,
    int apply)
{
    tune_change_t change;

    change.param = param;
    if (tune_parse_value(param, arg, &change.value) != 0
        || tune_find_rra(rrd, param, &change.rra_idx) != 0)
        return -1;
    return apply ? tune_apply(rrd, &change) : 0;
}

/* Set a checked parameter in the header */
static int tune_apply(
    rrd_t *rrd,
    const tune_change_t *change)
{
    const tune_param_t *param = change->param;
    unsigned long rra_idx = change->rra_idx;
    unsigned long i;

    if (param->kind != TUNE_WINDOW) {
        if (param->kind == TUNE_HW_SMOOTH
            && atoi(rrd->stat_head->version) < atoi(RRD_VERSION4)) {
            /* smoothing_window causes Version 4 */
            strcpy(rrd->stat_head->version, RRD_VERSION4);
        }
        rrd->rra_def[rra_idx].par[param->par].u_val = change->value.u_val;
        return 0;
    }

    rrd->rra_def[rra_idx].par[param->par].u_cnt = change->value.u_cnt;
    /* erase existing violations */
    for (i = 0; i < rrd->stat_head->ds_cnt; i++) {
        if (erase_violations(rrd, rra_idx * rrd->stat_head->ds_cnt + i,
                             rra_idx) != 0)
            return -1;
    }
    return 0;
}

/* Is arg RRA#<index>:<name>=<value>?  RRA#<index>:[+-=]<rows> is not;
   rrd_modify handles that one. */
static int is_rra_param_arg(
    const char *arg)
{
    const char *s;

    if (strncmp(arg, "RRA#", 4) != 0)
        return 0;
    s = arg + 4;
    if (!isdigit((unsigned char) *s))
        return 0;
    while (isdigit((unsigned char) *s))
        s++;
    return s[0] == ':' && isalpha((unsigned char) s[1]);
}

/* Parse and check RRA#<index>:<name>=<value> */
static int tune_parse_rra_param(
    const rrd_t *rrd,
    const char *arg,
    tune_change_t *change)
{
    const char *digits = arg + 4;
    const char *name;
    const char *eq;
    char     *end;
    unsigned long rra_idx;
    size_t    i, len;
    const tune_param_t *param = NULL;
    char     *msg;

    errno = 0;
    rra_idx = strtoul(digits, &end, 10);
    if (errno == ERANGE)
        rra_idx = ULONG_MAX;
    name = end + 1;
    eq = strchr(name, '=');
    if (eq == NULL) {
        rrd_set_error("%s: expected RRA#<index>:<parameter>=<value>", arg);
        return -1;
    }
    len = (size_t) (eq - name);
    for (i = 0; i < TUNE_PARAM_CNT; i++) {
        if (tune_params[i].opt == 0
            && strlen(tune_params[i].name) == len
            && strncmp(tune_params[i].name, name, len) == 0) {
            param = &tune_params[i];
            break;
        }
    }
    if (param == NULL) {
        rrd_set_error("%s: unknown parameter '%.*s'; RRA#<index>: takes "
                      "alpha, beta, gamma, smoothing-window, deltapos, "
                      "deltaneg, failure-threshold or window-length",
                      arg, (int) len, name);
        return -1;
    }
    if (rra_idx >= rrd->stat_head->rra_cnt) {
        rrd_set_error("%s: there is no RRA#%.*s, the RRAs of this RRD are "
                      "RRA#0 to RRA#%lu", arg, (int) (end - digits), digits,
                      rrd->stat_head->rra_cnt - 1);
        return -1;
    }
    if (!tune_rra_fits(rrd, rra_idx, param)) {
        rrd_set_error("%s: %s belongs to %s RRAs, but RRA#%lu is %s",
                      arg, param->name, param->cf_names, rra_idx,
                      rrd->rra_def[rra_idx].cf_nam);
        return -1;
    }
    if (tune_parse_value(param, eq + 1, &change->value) != 0) {
        msg = strdup(rrd_get_error());
        rrd_set_error("%s: %s", arg, msg ? msg : "invalid value");
        free(msg);
        return -1;
    }
    change->param = param;
    change->rra_idx = rra_idx;
    return 0;
}

/* Check the old Holt-Winters options and parse the
   RRA#<index>:<name>=<value> arguments, all before tune changes anything:
   with mmap the header is the file itself, so an error half way through
   the options would leave the changes of the earlier ones behind.
   smoothing_window tells whether a smoothing window gets set. */
static int tune_prepare(
    rrd_t *rrd,
    int argc,
    const char **argv,
    const struct optparse_long *longopts,
    tune_change_t **changes,
    unsigned long *change_cnt,
    int *smoothing_window)
{
    struct optparse options;
    const tune_param_t *param;
    int       opt;
    int       i;
    unsigned long cnt = 0;

    optparse_init(&options, argc, argv);
    while ((opt = optparse_long(&options, longopts, NULL)) != -1) {
        param = tune_option_param(opt);
        if (param == NULL)
            continue;
        if (tune_option(rrd, param, options.optarg, 0) != 0)
            return -1;
        if (param->kind == TUNE_HW_SMOOTH)
            *smoothing_window = 1;
    }

    /* the file name sits at optind, the other arguments follow it */
    for (i = options.optind + 1; i < options.argc; i++) {
        if (is_rra_param_arg(options.argv[i]))
            cnt++;
    }
    if (cnt == 0)
        return 0;
    *changes = (tune_change_t *) malloc(sizeof(tune_change_t) * cnt);
    if (*changes == NULL) {
        rrd_set_error("out of memory");
        return -1;
    }
    for (i = options.optind + 1; i < options.argc; i++) {
        if (!is_rra_param_arg(options.argv[i]))
            continue;
        if (tune_parse_rra_param(rrd, options.argv[i],
                                 &(*changes)[*change_cnt]) != 0)
            return -1;
        if ((*changes)[*change_cnt].param->kind == TUNE_HW_SMOOTH)
            *smoothing_window = 1;
        (*change_cnt)++;
    }
    return 0;
}
