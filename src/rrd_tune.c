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

static int set_hwarg(
    rrd_t *rrd,
    enum cf_en cf,
    enum rra_par_en rra_par,
    const char *arg);
static int set_deltaarg(
    rrd_t *rrd,
    enum rra_par_en rra_par,
    const char *arg);
static int set_windowarg(
    rrd_t *rrd,
    enum rra_par_en,
    const char *arg);

static int set_hwsmootharg(
    rrd_t *rrd,
    enum cf_en cf,
    enum rra_par_en rra_par,
    const char *arg);

static int smoothing_window_requested(
    int argc,
    const char **argv,
    const struct optparse_long *longopts);

/* The parts of the header the tune options change.  They are tuned in
   private copies and written to the file only when every option has been
   accepted, so a bad option leaves the file untouched.  With mmap the
   header rrd_open() hands out is the file itself. */
typedef struct tune_header {
    stat_head_t *stat_head;
    ds_def_t *ds_def;
    rra_def_t *rra_def;
    pdp_prep_t *pdp_prep;
    cdp_prep_t *cdp_prep;
} tune_header_t;

static int tune_header_copy(
    rrd_t *rrd,
    tune_header_t *file_header);
static void tune_header_release(
    rrd_t *rrd,
    tune_header_t *file_header);
static int tune_header_write(
    rrd_t *rrd,
    rrd_file_t *rrd_file,
    const char *filename);

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
    tune_header_t file_header = { NULL, NULL, NULL, NULL, NULL };
    char     *aberrant_reset = NULL;
    unsigned long reset_ds;
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

    /* Fix CWE-457 */
    memset(&rrd, 0, sizeof(rrd_t));
    rrd_init(&rrd);
    rrd_file = rrd_open(in_filename, &rrd, RRD_READWRITE | RRD_LOCK |
                                           RRD_READAHEAD | RRD_READVALUES);
    if (rrd_file == NULL) {
	goto done;
    }

    /* A smoothing window needs a version 4 header.  Files older than
       version 3 store the live header as a bare time_t, so relabelling
       them in place would make every later read misplace the rest of the
       file.  Rewrite such a file in the current layout first, as
       rrdtool restore does, then tune the rewritten file. */
    if (atoi(rrd.stat_head->version) < atoi(RRD_VERSION3)
        && smoothing_window_requested(argc, argv, longopts)) {
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

    if (tune_header_copy(&rrd, &file_header) != 0)
        goto done;
    /* --aberrant-reset also clears data rows, so it runs once all
       options are accepted; remember which data sources it names */
    aberrant_reset = (char *) calloc(rrd.stat_head->ds_cnt + 1, 1);
    if (aberrant_reset == NULL) {
        rrd_set_error("out of memory");
        goto done;
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
            if (set_deltaarg(&rrd, RRA_delta_pos, options.optarg)) {
		goto done;
            }
            break;
        case 'n':
            if (set_deltaarg(&rrd, RRA_delta_neg, options.optarg)) {
		goto done;
            }
            break;
        case 'f':
            if (set_windowarg(&rrd, RRA_failure_threshold, options.optarg)) {
		goto done;
            }
            break;
        case 'w':
            if (set_windowarg(&rrd, RRA_window_len, options.optarg)) {
		goto done;
            }
            break;
        case 'x':
            if (set_hwarg(&rrd, CF_HWPREDICT, RRA_hw_alpha, options.optarg)) {
                if (set_hwarg(&rrd, CF_MHWPREDICT, RRA_hw_alpha, options.optarg)) {
		    goto done;
                }
                rrd_clear_error();
            }
            break;
        case 'y':
            if (set_hwarg(&rrd, CF_HWPREDICT, RRA_hw_beta, options.optarg)) {
                if (set_hwarg(&rrd, CF_MHWPREDICT, RRA_hw_beta, options.optarg)) {
		    goto done;
                }
                rrd_clear_error();
            }
            break;
        case 'z':
            if (set_hwarg(&rrd, CF_SEASONAL, RRA_seasonal_gamma, options.optarg)) {
		goto done;
            }
            break;
        case 'v':
            if (set_hwarg(&rrd, CF_DEVSEASONAL, RRA_seasonal_gamma, options.optarg)) {
		goto done;
	    }
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
            aberrant_reset[ds] = 1;
            break;
        case 's':
            if (atoi(rrd.stat_head->version) < atoi(RRD_VERSION4)) {
                strcpy(rrd.stat_head->version, RRD_VERSION4);    /* smoothing_window causes Version 4 */
            }
            if (set_hwsmootharg
                (&rrd, CF_SEASONAL, RRA_seasonal_smoothing_window, options.optarg)) {
		goto done;
            }
            break;
        case 'S':
            if (atoi(rrd.stat_head->version) < atoi(RRD_VERSION4)) {
                strcpy(rrd.stat_head->version, RRD_VERSION4);    /* smoothing_window causes Version 4 */
            }
            if (set_hwsmootharg
                (&rrd, CF_DEVSEASONAL, RRA_seasonal_smoothing_window,
                 options.optarg)) {
		goto done;
            }
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
    if (optcnt > 0) {
        if (tune_header_write(&rrd, rrd_file, in_filename) != 0)
            goto done;
        for (reset_ds = 0; reset_ds < rrd.stat_head->ds_cnt; reset_ds++) {
            if (!aberrant_reset[reset_ds])
                continue;
            reset_aberrant_coefficients(&rrd, rrd_file, reset_ds);
            if (rrd_test_error())
                goto done;
        }
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

    if (handle_modify(&rrd, in_filename, options.argc, options.argv,
                      options.optind + 1, opt_newstep) < 0)
        goto done;
    rc = 0;
done:
    free(aberrant_reset);
    tune_header_release(&rrd, &file_header);
    if (rrd_file) {
	rrd_close(rrd_file);
    }
    rrd_free(&rrd);
    return rc;
}

static void *tune_dup(
    const void *src,
    size_t size)
{
    void     *copy = malloc(size > 0 ? size : 1);

    if (copy != NULL && size > 0)
        memcpy(copy, src, size);
    return copy;
}

/* Point rrd at private copies of the tunable header parts and keep the
   originals in file_header. */
static int tune_header_copy(
    rrd_t *rrd,
    tune_header_t *file_header)
{
    unsigned long ds_cnt = rrd->stat_head->ds_cnt;
    unsigned long rra_cnt = rrd->stat_head->rra_cnt;
    tune_header_t copy;

    copy.stat_head = (stat_head_t *) tune_dup(rrd->stat_head,
                                              sizeof(stat_head_t));
    copy.ds_def = (ds_def_t *) tune_dup(rrd->ds_def,
                                        sizeof(ds_def_t) * ds_cnt);
    copy.rra_def = (rra_def_t *) tune_dup(rrd->rra_def,
                                          sizeof(rra_def_t) * rra_cnt);
    copy.pdp_prep = (pdp_prep_t *) tune_dup(rrd->pdp_prep,
                                            sizeof(pdp_prep_t) * ds_cnt);
    copy.cdp_prep = (cdp_prep_t *) tune_dup(rrd->cdp_prep,
                                            sizeof(cdp_prep_t) * ds_cnt
                                            * rra_cnt);
    if (copy.stat_head == NULL || copy.ds_def == NULL
        || copy.rra_def == NULL || copy.pdp_prep == NULL
        || copy.cdp_prep == NULL) {
        free(copy.stat_head);
        free(copy.ds_def);
        free(copy.rra_def);
        free(copy.pdp_prep);
        free(copy.cdp_prep);
        rrd_set_error("out of memory");
        return -1;
    }

    file_header->stat_head = rrd->stat_head;
    file_header->ds_def = rrd->ds_def;
    file_header->rra_def = rrd->rra_def;
    file_header->pdp_prep = rrd->pdp_prep;
    file_header->cdp_prep = rrd->cdp_prep;
    rrd->stat_head = copy.stat_head;
    rrd->ds_def = copy.ds_def;
    rrd->rra_def = copy.rra_def;
    rrd->pdp_prep = copy.pdp_prep;
    rrd->cdp_prep = copy.cdp_prep;
    return 0;
}

/* Drop the private copies and give rrd its own header back, so rrd_free()
   releases what rrd_open() allocated. */
static void tune_header_release(
    rrd_t *rrd,
    tune_header_t *file_header)
{
    if (file_header->stat_head == NULL)
        return;
    free(rrd->stat_head);
    free(rrd->ds_def);
    free(rrd->rra_def);
    free(rrd->pdp_prep);
    free(rrd->cdp_prep);
    rrd->stat_head = file_header->stat_head;
    rrd->ds_def = file_header->ds_def;
    rrd->rra_def = file_header->rra_def;
    rrd->pdp_prep = file_header->pdp_prep;
    rrd->cdp_prep = file_header->cdp_prep;
    file_header->stat_head = NULL;
}

static int tune_write_part(
    rrd_file_t *rrd_file,
    const char *filename,
    const void *buf,
    size_t size)
{
    if (rrd_write(rrd_file, buf, size) != (ssize_t) size) {
        if (!rrd_test_error())
            rrd_set_error("writing header of '%s' failed", filename);
        return -1;
    }
    return 0;
}

/* Write the tuned header parts to the file.  pdp_prep and cdp_prep sit
   right before rra_ptr at the end of the header; taking their offset
   from header_len keeps this right for files before version 3, whose
   live header is shorter. */
static int tune_header_write(
    rrd_t *rrd,
    rrd_file_t *rrd_file,
    const char *filename)
{
    unsigned long ds_cnt = rrd->stat_head->ds_cnt;
    unsigned long rra_cnt = rrd->stat_head->rra_cnt;
    size_t    pdp_size = sizeof(pdp_prep_t) * ds_cnt;
    size_t    cdp_size = sizeof(cdp_prep_t) * ds_cnt * rra_cnt;
    off_t     pdp_start = rrd_file->header_len
        - sizeof(rra_ptr_t) * rra_cnt - cdp_size - pdp_size;

    if (rrd_seek(rrd_file, 0, SEEK_SET) != 0) {
        rrd_set_error("seek to start of '%s' failed", filename);
        return -1;
    }
    if (tune_write_part(rrd_file, filename, rrd->stat_head,
                        sizeof(stat_head_t)) != 0
        || tune_write_part(rrd_file, filename, rrd->ds_def,
                           sizeof(ds_def_t) * ds_cnt) != 0
        || tune_write_part(rrd_file, filename, rrd->rra_def,
                           sizeof(rra_def_t) * rra_cnt) != 0)
        return -1;
    if (rrd_seek(rrd_file, pdp_start, SEEK_SET) != 0) {
        rrd_set_error("seek in '%s' failed", filename);
        return -1;
    }
    if (tune_write_part(rrd_file, filename, rrd->pdp_prep, pdp_size) != 0
        || tune_write_part(rrd_file, filename, rrd->cdp_prep,
                           cdp_size) != 0)
        return -1;
    return 0;
}

static int set_hwarg(
    rrd_t *rrd,
    enum cf_en cf,
    enum rra_par_en rra_par,
    const char *arg)
{
    double    param;
    unsigned long i;
    signed short rra_idx = -1;
    unsigned int strtod_ret_val;

    strtod_ret_val = rrd_strtodbl(arg, NULL, &param, NULL);
    /* read the value */
    if ((strtod_ret_val == 1 || strtod_ret_val == 2 ) &&
         (param <= 0.0 || param >= 1.0) ) {
        rrd_set_error("Holt-Winters parameter must be between 0 and 1");
        return -1;
    } else if( strtod_ret_val == 0 || strtod_ret_val > 2 ) {
        rrd_set_error("Unable to parse Holt-Winters parameter");
        return -1;
    }
    /* does the appropriate RRA exist?  */
    for (i = 0; i < rrd->stat_head->rra_cnt; ++i) {
        if (rrd_cf_conv(rrd->rra_def[i].cf_nam) == cf) {
            rra_idx = i;
            break;
        }
    }
    if (rra_idx == -1) {
        rrd_set_error("Holt-Winters RRA does not exist in this RRD");
        return -1;
    }

    /* set the value */
    rrd->rra_def[rra_idx].par[rra_par].u_val = param;
    return 0;
}

static int set_hwsmootharg(
    rrd_t *rrd,
    enum cf_en cf,
    enum rra_par_en rra_par,
    const char *arg)
{
    double    param;
    unsigned long i;
    signed short rra_idx = -1;
    unsigned int strtod_ret_val;

    /* read the value */
    strtod_ret_val = rrd_strtodbl(arg, NULL, &param, NULL);
    /* in order to avoid smoothing of SEASONAL or DEVSEASONAL, we need to 
     * the 0.0 value*/
    if ( (strtod_ret_val == 1 || strtod_ret_val == 2 ) &&
         (param < 0.0 || param > 1.0) ) {
        rrd_set_error("Holt-Winters parameter must be between 0 and 1");
        return -1;
    } else if( strtod_ret_val == 0 || strtod_ret_val > 2 ) {
        rrd_set_error("Unable to parse Holt-Winters parameter");
        return -1;
    }
    /* does the appropriate RRA exist?  */
    for (i = 0; i < rrd->stat_head->rra_cnt; ++i) {
        if (rrd_cf_conv(rrd->rra_def[i].cf_nam) == cf) {
            rra_idx = i;
            break;
        }
    }
    if (rra_idx == -1) {
        rrd_set_error("Holt-Winters RRA does not exist in this RRD");
        return -1;
    }

    /* set the value */
    rrd->rra_def[rra_idx].par[rra_par].u_val = param;
    return 0;
}

static int smoothing_window_requested(
    int argc,
    const char **argv,
    const struct optparse_long *longopts)
{
    struct optparse options;
    int       opt;

    optparse_init(&options, argc, argv);
    while ((opt = optparse_long(&options, longopts, NULL)) != -1) {
        if (opt == 's' || opt == 'S')
            return 1;
    }
    return 0;
}

static int set_deltaarg(
    rrd_t *rrd,
    enum rra_par_en rra_par,
    const char *arg)
{
    rrd_value_t param;
    unsigned long i;
    signed short rra_idx = -1;
    unsigned int strtod_ret_val;

    strtod_ret_val = rrd_strtodbl(arg, NULL, &param, NULL);
    if ((strtod_ret_val == 1 || strtod_ret_val == 2) &&
         param < 0.1) {
        rrd_set_error("Parameter specified is too small");
        return -1;
    } else if( strtod_ret_val == 1 || strtod_ret_val > 2 ) {
        rrd_set_error("Unable to parse parameter in set_deltaarg");
        return -1;
    }

    /* does the appropriate RRA exist?  */
    for (i = 0; i < rrd->stat_head->rra_cnt; ++i) {
        if (rrd_cf_conv(rrd->rra_def[i].cf_nam) == CF_FAILURES) {
            rra_idx = i;
            break;
        }
    }
    if (rra_idx == -1) {
        rrd_set_error("Failures RRA does not exist in this RRD");
        return -1;
    }

    /* set the value */
    rrd->rra_def[rra_idx].par[rra_par].u_val = param;
    return 0;
}

static int set_windowarg(
    rrd_t *rrd,
    enum rra_par_en rra_par,
    const char *arg)
{
    unsigned long param;
    unsigned long i, cdp_idx;
    signed short rra_idx = -1;

    /* read the value */
    param = atoi(arg);
    if (param < 1 || param > MAX_FAILURES_WINDOW_LEN) {
        rrd_set_error("Parameter must be between %d and %d",
                      1, MAX_FAILURES_WINDOW_LEN);
        return -1;
    }
    /* does the appropriate RRA exist?  */
    for (i = 0; i < rrd->stat_head->rra_cnt; ++i) {
        if (rrd_cf_conv(rrd->rra_def[i].cf_nam) == CF_FAILURES) {
            rra_idx = i;
            break;
        }
    }
    if (rra_idx == -1) {
        rrd_set_error("Failures RRA does not exist in this RRD");
        return -1;
    }

    /* set the value */
    rrd->rra_def[rra_idx].par[rra_par].u_cnt = param;

    /* erase existing violations */
    for (i = 0; i < rrd->stat_head->ds_cnt; i++) {
        cdp_idx = rra_idx * (rrd->stat_head->ds_cnt) + i;
        if (erase_violations(rrd, cdp_idx, rra_idx) != 0) {
            return -1;
        }
    }
    return 0;
}
