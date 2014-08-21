/*****************************************************************************
 * RRDtool 1.4.3  Copyright by Tobi Oetiker, 1997-2010
 *****************************************************************************
 * rrd_create.c  creates new rrds
 *****************************************************************************/

#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <math.h>
#include <glib.h>   // will use glist
#include <sys/types.h>      // stat()
#include <sys/stat.h>       // stat()
#include <unistd.h>         // stat()

#include "rrd_strtod.h"
#include "rrd_tool.h"
#include "rrd_rpncalc.h"
#include "rrd_hw.h"
#include "rrd_client.h"
#include "rrd_config.h"
#include "rrd_create.h"

#include "rrd_is_thread_safe.h"
#include "rrd_modify.h"

#ifdef WIN32
# include <process.h>
#endif

static int rrd_init_data(rrd_t *rrd);
static int rrd_prefill_data(rrd_t *rrd, const GList *sources_rrd_files);
static int positive_mod(int a, int b);

unsigned long FnvHash(
    const char *str);
void      parseGENERIC_DS(
    const char *def,
    ds_def_t *ds_def);

int rrd_create(
    int argc,
    char **argv)
{
    struct option long_options[] = {
        {"start", required_argument, 0, 'b'},
        {"step", required_argument, 0, 's'},
        {"daemon", required_argument, 0, 'd'},
        {"source", required_argument, 0, 'r'},
        {"no-overwrite", no_argument, 0, 'O'},
        {0, 0, 0, 0}
    };
    int       option_index = 0;
    int       opt;
    time_t    last_up = time(NULL) - 10;
    unsigned long pdp_step = 300;
    rrd_time_value_t last_up_tv;
    const char *parsetime_error = NULL;
    int       rc = -1;
    char * opt_daemon = NULL;
    int       opt_no_overwrite = 0;
    GList * sources = NULL;
    const char **sources_array = NULL;
    
    optind = 0;
    opterr = 0;         /* initialize getopt */

    while (1) {
        opt = getopt_long(argc, argv, "Ob:s:d:", long_options, &option_index);

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
                rc = -1;
                goto done;
            }
            break;

        case 'b':
            if ((parsetime_error = rrd_parsetime(optarg, &last_up_tv))) {
                rrd_set_error("start time: %s", parsetime_error);
                rc = -1;
                goto done;
            }
            if (last_up_tv.type == RELATIVE_TO_END_TIME ||
                last_up_tv.type == RELATIVE_TO_START_TIME) {
                rrd_set_error("specifying time relative to the 'start' "
                              "or 'end' makes no sense here");
                rc = -1;
                goto done;
            }

            last_up = mktime(&last_up_tv.tm) +last_up_tv.offset;

            if (last_up < 3600 * 24 * 365 * 10) {
                rrd_set_error
                    ("the first entry to the RRD should be after 1980");
                rc = -1;
                goto done;
            }
            break;

        case 's':
            if ((parsetime_error = rrd_scaled_duration(optarg, 1, &pdp_step))) {
                rrd_set_error("step size: %s", parsetime_error);
                rc = -1;
                goto done;
            }
            break;

        case 'O':
            opt_no_overwrite = 1;
	    break;

        case 'r': {
            struct stat st;
            if (stat(optarg, &st) != 0) {
                char errmsg[100];
#ifdef GNU_SOURCE
#error using wrong version of strerror_r, because GNU_SOURCE is set
#endif
                strerror_r(errno, errmsg, sizeof(errmsg));
                rrd_set_error("error checking for source RRD %s: %s", optarg, errmsg);
                rc = -1;
                goto done;
            } 
            
            if (!S_ISREG(st.st_mode)) {
                rrd_set_error("Not a regular file: %s", optarg);
                rc = -1;
                goto done;
            }
            char * optcpy = strdup(optarg);
            if (optcpy == NULL) {
                rrd_set_error("Cannot allocate string");
                rc = -1;
                goto done;
            }
            sources = g_list_append(sources, optcpy);
            if (sources == NULL) {
                rrd_set_error("Cannot allocate required data structure");
                rc = -1;
                goto done;
            }

            break;
        }
        case '?':
            if (optopt != 0)
                rrd_set_error("unknown option '%c'", optopt);
            else
                rrd_set_error("unknown option '%s'", argv[optind - 1]);
                rc = -1;
                goto done;
        }
    }
    if (optind == argc) {
        rrd_set_error("need name of an rrd file to create");
        rc = -1;
        goto done;
    }

    if (sources != NULL) {
        sources_array = malloc((g_list_length(sources) + 1) * sizeof(char*));
        if (sources_array == NULL) {
            rrd_set_error("cannot allocate memory");
            goto done;
        }
        int n = 0;
        GList *p;
        for (p = sources ; p ; p = g_list_next(p), n++) {
            sources_array[n] = p->data;
        }
        sources_array[n] = NULL;
    }
    rrdc_connect (opt_daemon);
    if (rrdc_is_connected (opt_daemon)) {
        rc = rrdc_create_r2(argv[optind],
                      pdp_step, last_up, opt_no_overwrite, 
                      sources_array,
                      argc - optind - 1, (const char **) (argv + optind + 1));
	} else {
        rc = rrd_create_r2(argv[optind],
                      pdp_step, last_up, opt_no_overwrite,
                      sources_array,
                      argc - optind - 1, (const char **) (argv + optind + 1));
	}
done:
    if (sources_array != NULL) {
        free(sources_array);
        sources_array = NULL;
    }
    if (sources != NULL) {
        // this will free the list elements as well
        g_list_free_full(sources, free);
        sources = NULL;
    }
    return rc;
}


int parseDS(const char *def, 
	    ds_def_t *ds_def,
	    void *key_hash,
            long (*lookup)(void *, char *)
	    ) 
{
    char      dummychar1[2], dummychar2[2];
    int       offset;

    /* extract the name and type */
    switch (sscanf(def,
		   DS_NAM_FMT "%1[:]" DST_FMT "%1[:]%n",
		   ds_def->ds_nam,
		   dummychar1,
		   ds_def->dst,
		   dummychar2, &offset)) {
    case 0:
    case 1:
	rrd_set_error("Invalid DS name in [%s]", def);
	return -1;
    case 2:
    case 3:
	rrd_set_error("Invalid DS type in [%s]", def);
	return -1;
    case 4:    /* (%n may or may not be counted) */
    case 5:
	break;
    default:
	rrd_set_error("invalid DS format");
	return -1;
    }

    /* parse the remainder of the arguments */
    switch (dst_conv(ds_def->dst)) {
    case DST_COUNTER:
    case DST_ABSOLUTE:
    case DST_GAUGE:
    case DST_DERIVE:
	parseGENERIC_DS(def + offset, ds_def);
	break;
    case DST_CDEF:
	parseCDEF_DS(def + offset, ds_def, key_hash, lookup);
	break;
    default:
	rrd_set_error("invalid DS type specified");
	return -1;
    }
    return 0;
}


int parseRRA(const char *def,
	     rra_def_t *rra_def, 
	     rrd_t *rrd,
	     unsigned long hash) {
    char     *argvcopy;
    char     *tokptr = "";
    unsigned short token_idx, error_flag, period = 0;
    int       cf_id = -1;
    int       token_min = 4;
    const char *parsetime_error = NULL;
    char     *require_version = NULL;

    memset(rra_def, 0, sizeof(rra_def_t));

    argvcopy = strdup(def);
    char *token = strtok_r(&argvcopy[4], ":", &tokptr);
    token_idx = error_flag = 0;
            
    while (token != NULL) {
	switch (token_idx) {
	case 0:
	    if (sscanf(token, CF_NAM_FMT, rra_def->cf_nam) != 1)
		rrd_set_error("Failed to parse CF name");
	    cf_id = cf_conv(rra_def->cf_nam);
	    switch (cf_id) {
	    case CF_MHWPREDICT:
		require_version = RRD_VERSION;    /* MHWPREDICT causes Version 4 */
	    case CF_HWPREDICT:
		token_min = 5;
		/* initialize some parameters */
		rra_def->par[RRA_hw_alpha].
		    u_val = 0.1;
		rra_def->par[RRA_hw_beta].
		    u_val = 1.0 / 288;
		rra_def->par[RRA_dependent_rra_idx].u_cnt = INT_MAX;
		break;
	    case CF_DEVSEASONAL:
		token_min = 3;
	    case CF_SEASONAL:
		if (cf_id == CF_SEASONAL){
		    token_min = 4;
		}
		/* initialize some parameters */
		rra_def->par[RRA_seasonal_gamma].u_val = 0.1;
		rra_def->par[RRA_seasonal_smoothing_window].u_val = 0.05;
		/* fall through */
	    case CF_DEVPREDICT:
		if (cf_id == CF_DEVPREDICT){
		    token_min = 3;
		}
		rra_def->par[RRA_dependent_rra_idx].u_cnt = -1;
		break;
	    case CF_FAILURES:
		token_min = 5;
		rra_def->par[RRA_delta_pos].u_val = 2.0;
		rra_def->par[RRA_delta_neg].u_val = 2.0;
		rra_def->par[RRA_window_len].u_cnt = 3;
		rra_def->par[RRA_failure_threshold].u_cnt = 2;
		rra_def->par[RRA_dependent_rra_idx].u_cnt = -1;
		break;
		/* invalid consolidation function */
	    case -1:
		rrd_set_error
		    ("Unrecognized consolidation function %s",
		     rra_def->cf_nam);
	    default:
		break;
	    }
	    /* default: 1 pdp per cdp */
	    rra_def->pdp_cnt = 1;
	    break;
	case 1:
	    switch (cf_conv
		    (rra_def->cf_nam)) {
	    case CF_HWPREDICT:
	    case CF_MHWPREDICT:
	    case CF_DEVSEASONAL:
	    case CF_SEASONAL:
	    case CF_DEVPREDICT:
	    case CF_FAILURES:
		if ((parsetime_error = rrd_scaled_duration(token, rrd->stat_head->pdp_step, &rra_def->row_cnt)))
		    rrd_set_error("Invalid row count %s: %s", token, parsetime_error);
		break;
	    default:
		rra_def->par[RRA_cdp_xff_val].u_val = atof(token);
		if (rra_def->par[RRA_cdp_xff_val].u_val < 0.0
		    || rra_def->par[RRA_cdp_xff_val].u_val >= 1.0)
		    rrd_set_error
			("Invalid xff: must be between 0 and 1");
		break;
	    }
	    break;
	case 2:
	    switch (cf_conv
		    (rra_def->cf_nam)) {
	    case CF_HWPREDICT:
	    case CF_MHWPREDICT:
		rra_def->par[RRA_hw_alpha].
		    u_val = atof(token);
		if (atof(token) <= 0.0 || atof(token) >= 1.0)
		    rrd_set_error
			("Invalid alpha: must be between 0 and 1");
		break;
	    case CF_DEVSEASONAL:
	    case CF_SEASONAL:
		rra_def->par[RRA_seasonal_gamma].u_val = atof(token);
		if (atof(token) <= 0.0 || atof(token) >= 1.0)
		    rrd_set_error
			("Invalid gamma: must be between 0 and 1");
		rra_def->par[RRA_seasonal_smooth_idx].u_cnt =
		    hash % rra_def->row_cnt;
		break;
	    case CF_FAILURES:
		/* specifies the # of violations that constitutes the failure threshold */
		rra_def->par[RRA_failure_threshold].u_cnt = atoi(token);
		if (atoi(token) < 1
		    || atoi(token) > MAX_FAILURES_WINDOW_LEN)
		    rrd_set_error
			("Failure threshold is out of range %d, %d",
			 1, MAX_FAILURES_WINDOW_LEN);
		break;
	    case CF_DEVPREDICT:
		/* specifies the index (1-based) of CF_DEVSEASONAL array
		 * associated with this CF_DEVPREDICT array. */
		rra_def->par[RRA_dependent_rra_idx].u_cnt =
		    atoi(token) - 1;
		break;
	    default:
		if ((parsetime_error = rrd_scaled_duration(token, rrd->stat_head->pdp_step, &rra_def->pdp_cnt)))
		    rrd_set_error("Invalid step %s: %s", token, parsetime_error);
		break;
	    }
	    break;
	case 3:
	    switch (cf_conv(rra_def->cf_nam)) {
	    case CF_HWPREDICT:
	    case CF_MHWPREDICT:
		rra_def->par[RRA_hw_beta].u_val = atof(token);
		if (atof(token) < 0.0 || atof(token) > 1.0)
		    rrd_set_error
			("Invalid beta: must be between 0 and 1");
		break;
	    case CF_DEVSEASONAL:
	    case CF_SEASONAL:
		/* specifies the index (1-based) of CF_HWPREDICT array
		 * associated with this CF_DEVSEASONAL or CF_SEASONAL array. 
		 * */
		rra_def->par[RRA_dependent_rra_idx].u_cnt =
		    atoi(token) - 1;
		break;
	    case CF_FAILURES:
		/* specifies the window length */
		rra_def->par[RRA_window_len].u_cnt = atoi(token);
		if (atoi(token) < 1
		    || atoi(token) > MAX_FAILURES_WINDOW_LEN)
		    rrd_set_error
			("Window length is out of range %d, %d", 1,
			 MAX_FAILURES_WINDOW_LEN);
		/* verify that window length exceeds the failure threshold */
		if (rra_def->par[RRA_window_len].u_cnt <
		    rra_def->par[RRA_failure_threshold].u_cnt)
		    rrd_set_error
			("Window length is shorter than the failure threshold");
		break;
	    case CF_DEVPREDICT:
		/* shouldn't be any more arguments */
		rrd_set_error
		    ("Unexpected extra argument for consolidation function DEVPREDICT");
		break;
	    default:
                if ((parsetime_error = rrd_scaled_duration(token,
                                                           rrd->stat_head->pdp_step * rra_def->pdp_cnt,
                                                           &rra_def->row_cnt)))
		    rrd_set_error("Invalid row count %s: %s", token, parsetime_error);
#if SIZEOF_TIME_T == 4
		if (((long long) rrd->stat_head->pdp_step * rra_def->pdp_cnt) * rra_def->row_cnt > 4294967296LL){
		    /* database timespan > 2**32, would overflow time_t */
		    rrd_set_error("The time spanned by the database is too large: must be <= 4294967296 seconds");
		}
#endif
		break;
	    }
	    break;
	case 4:
	    switch (cf_conv(rra_def->cf_nam)) {
	    case CF_FAILURES:
		/* specifies the index (1-based) of CF_DEVSEASONAL array
		 * associated with this CF_DEVFAILURES array. */
		rra_def->par[RRA_dependent_rra_idx].u_cnt =
		    atoi(token) - 1;
		break;
	    case CF_DEVSEASONAL:
	    case CF_SEASONAL:
		/* optional smoothing window */
		if (sscanf(token, "smoothing-window=%lf",
			   &(rra_def->par[RRA_seasonal_smoothing_window].
			     u_val))) {
		    require_version = RRD_VERSION;    /* smoothing-window causes Version 4 */
		    if (rra_def->par[RRA_seasonal_smoothing_window].u_val < 0.0
			|| rra_def->par[RRA_seasonal_smoothing_window].u_val >
			1.0) {
			rrd_set_error
			    ("Invalid smoothing-window %f: must be between 0 and 1",
			     rra_def->par[RRA_seasonal_smoothing_window].
			     u_val);
		    }
		} else {
		    rrd_set_error("Invalid option %s", token);
		}
		break;
	    case CF_HWPREDICT:
	    case CF_MHWPREDICT:
		/* length of the associated CF_SEASONAL and CF_DEVSEASONAL arrays. */
		period = atoi(token);
		if (period >
		    rra_def->row_cnt)
		    rrd_set_error
			("Length of seasonal cycle exceeds length of HW prediction array");
		rra_def->par[RRA_period].u_val = period;

		break;
	    default:
		/* shouldn't be any more arguments */
		rrd_set_error
		    ("Unexpected extra argument for consolidation function %s",
		     rra_def->cf_nam);
		break;
	    }
	    break;
	case 5:
	    /* If we are here, this must be a CF_HWPREDICT RRA.
	     * Specifies the index (1-based) of CF_SEASONAL array
	     * associated with this CF_HWPREDICT array. If this argument 
	     * is missing, then the CF_SEASONAL, CF_DEVSEASONAL, CF_DEVPREDICT,
	     * CF_FAILURES.
	     * arrays are created automatically. */
	    rra_def->par[RRA_dependent_rra_idx].u_cnt = atoi(token) - 1;
	    break;
	default:
	    /* should never get here */
	    rrd_set_error("Unknown error");
	    break;
	}       /* end switch */
	if (rrd_test_error()) {
	    /* all errors are unrecoverable */
	    free(argvcopy);
	    return (-1);
	}
	token = strtok_r(NULL, ":", &tokptr);
	token_idx++;
    }           /* end while */
    free(argvcopy);
    if (token_idx < token_min){
	rrd_set_error("Expected at least %i arguments for RRA but got %i",token_min,token_idx);
	return(-1);
    }

    // parsing went well. ONLY THEN are we allowed to produce
    // additional side effects.
    if (require_version != NULL) {
        if (rrd) {
            strcpy(rrd->stat_head->version, RRD_VERSION);
        }
    }

#ifdef DEBUG
    fprintf(stderr,
	    "Creating RRA CF: %s, dep idx %lu\n",
	    rra_def->cf_nam,
	    rra_def->par[RRA_dependent_rra_idx].u_cnt);
#endif

    return 0;
}
/*
  this function checks THE LAST rra_def in the rra_def_array (that is,
  the one with index rra_cnt-1 to check if more RRAs have to be
  created
 */
rra_def_t *handle_dependent_rras(rra_def_t *rra_def_array, 
				 long unsigned int *rra_cnt, 
				 unsigned long hash) {
    rra_def_t *rra_def = rra_def_array + (*rra_cnt-1);

    /* should we create CF_SEASONAL, CF_DEVSEASONAL, and CF_DEVPREDICT? */
    if ((cf_conv(rra_def->cf_nam) == CF_HWPREDICT
	 || cf_conv(rra_def->cf_nam) == CF_MHWPREDICT)
	&& rra_def->par[RRA_dependent_rra_idx].u_cnt == INT_MAX) {
	rra_def->par[RRA_dependent_rra_idx].u_cnt = *rra_cnt-1;
#ifdef DEBUG
	fprintf(stderr, "Creating HW contingent RRAs\n");
#endif

	rra_def_array = create_hw_contingent_rras(rra_def_array,
						  rra_cnt,
						  rra_def->par[RRA_period].u_val,
						  hash);
	if (rra_def_array == NULL) {
	    rrd_set_error("creating contingent RRA");
	    return NULL;
	}
    }

    return rra_def_array;
}


/* #define DEBUG */
/* For backwards compatibility with previous API.  Use rrd_create_r2 if you
   need to have the no_overwrite parameter.                                */
int rrd_create_r(
    const char *filename,
    unsigned long pdp_step,
    time_t last_up,
    int argc,
    const char **argv)
{
	return rrd_create_r2(filename,pdp_step,last_up,0, NULL, argc,argv);
}


static void cleanup_source_file(rrd_file_t *file) {
    if (file == NULL) return;
    
    if (file->rrd != NULL) {
        rrd_free(file->rrd);
        free(file->rrd);
        file->rrd = NULL;
    }

    rrd_close(file);
}

int rrd_create_r2(
    const char *filename,
    unsigned long pdp_step,
    time_t last_up,
    int no_overwrite,
    const char **sources,
    int argc,
    const char **argv)
{
    rrd_t     rrd;
    long      i;
    unsigned long hashed_name;
    int rc = -1;
    struct stat stat_buf;
    GList *sources_rrd_files = NULL;
    
    /* clear any previous errors */
    rrd_clear_error();

    /* init rrd clean */
    rrd_init(&rrd);
    
    if (no_overwrite && (stat(filename, &stat_buf) == 0)) {
        rrd_set_error("creating '%s': File exists", filename);
        goto done;
    }
    
    /* static header */
    if ((rrd.stat_head = (stat_head_t*)calloc(1, sizeof(stat_head_t))) == NULL) {
        rrd_set_error("allocating rrd.stat_head");
	goto done;
    }

    /* live header */
    if ((rrd.live_head = (live_head_t*)calloc(1, sizeof(live_head_t))) == NULL) {
        rrd_set_error("allocating rrd.live_head");
	goto done;
    }

    /* set some defaults */
    strcpy(rrd.stat_head->cookie, RRD_COOKIE);
    strcpy(rrd.stat_head->version, RRD_VERSION3);   /* by default we are still version 3 */
    rrd.stat_head->float_cookie = FLOAT_COOKIE;
    rrd.stat_head->ds_cnt = 0;  /* this will be adjusted later */
    rrd.stat_head->rra_cnt = 0; /* ditto */
    rrd.stat_head->pdp_step = pdp_step; /* 5 minute default */

    /* a default value */
    rrd.ds_def = NULL;
    rrd.rra_def = NULL;

    rrd.live_head->last_up = last_up;

    /* optind points to the first non-option command line arg,
     * in this case, the file name. */
    /* Compute the FNV hash value (used by SEASONAL and DEVSEASONAL
     * arrays. */
    hashed_name = FnvHash(filename);
    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "DS:", 3) == 0) {
            size_t    old_size = sizeof(ds_def_t) * (rrd.stat_head->ds_cnt);

            if ((rrd.ds_def = (ds_def_t*)rrd_realloc(rrd.ds_def,
                                          old_size + sizeof(ds_def_t))) ==
                NULL) {
                rrd_set_error("allocating rrd.ds_def");
		goto done;
            }
            memset(&rrd.ds_def[rrd.stat_head->ds_cnt], 0, sizeof(ds_def_t));

	    parseDS(argv[i] + 3, rrd.ds_def + rrd.stat_head->ds_cnt,
		    &rrd, lookup_DS);

	    /* check for duplicate DS name */

	    if (lookup_DS(&rrd, rrd.ds_def[rrd.stat_head->ds_cnt].ds_nam) 
		>= 0) {
		rrd_set_error("Duplicate DS name: %s",
			      rrd.ds_def[rrd.stat_head->ds_cnt].ds_nam);
	    }
	    
	    rrd.stat_head->ds_cnt++;

            if (rrd_test_error()) {
		goto done;
            }
	    
        } else if (strncmp(argv[i], "RRA:", 4) == 0) {
            size_t    old_size = sizeof(rra_def_t) * (rrd.stat_head->rra_cnt);
            if ((rrd.rra_def = (rra_def_t*)rrd_realloc(rrd.rra_def,
                                           old_size + sizeof(rra_def_t))) ==
                NULL) {
                rrd_set_error("allocating rrd.rra_def");
		goto done;
            }

	    parseRRA(argv[i], rrd.rra_def + rrd.stat_head->rra_cnt, &rrd,
		     hashed_name);

	    if (rrd_test_error()) {
		goto done;
            }

	    rrd.stat_head->rra_cnt++;

	    rrd.rra_def = handle_dependent_rras(rrd.rra_def, &(rrd.stat_head->rra_cnt), 
						hashed_name);
	    if (rrd.rra_def == NULL) {
		goto done;
	    }
        } else {
            rrd_set_error("can't parse argument '%s'", argv[i]);
	    goto done;
        }
    }


    if (rrd.stat_head->rra_cnt < 1) {
        rrd_set_error("you must define at least one Round Robin Archive");
	goto done;
    }

    if (rrd.stat_head->ds_cnt < 1) {
        rrd_set_error("you must define at least one Data Source");
	goto done;
    }
    
    rc = rrd_init_data(&rrd);
    if (rc != 0) goto done;

    if (sources != NULL) {
        for (const char **s = sources ; *s ; s++) {
            rrd_t *srrd = malloc(sizeof(rrd_t));
            if (srrd == NULL) {
                rrd_set_error("cannot allocate memory");
                goto done;
            }
        
            rrd_init(srrd);
            rrd_file_t *sf = rrd_open(*s, srrd, RRD_READONLY | RRD_READAHEAD | RRD_READVALUES);

            if (sf == NULL) {
                goto done;
            }
            sources_rrd_files = g_list_append(sources_rrd_files, sf);
            if (sources_rrd_files == NULL) {
                rrd_set_error("Cannot keep information about just opened source RRD - likely leaking resources!");
                goto done;
            }
        }
        
        rrd_prefill_data(&rrd, sources_rrd_files);
    }
    
    rc = write_rrd(filename, &rrd);
    
done:
    g_list_free_full(sources_rrd_files, (GDestroyNotify) cleanup_source_file);
            
    rrd_free(&rrd);
    return rc;
}

void parseGENERIC_DS(
    const char *def,
    ds_def_t *ds_def)
{
    char      minstr[DS_NAM_SIZE], maxstr[DS_NAM_SIZE];
    const char *parsetime_error = NULL;

    /*
       int temp;

       temp = sscanf(def,"%lu:%18[^:]:%18[^:]", 
       &(rrd -> ds_def[ds_idx].par[DS_mrhb_cnt].u_cnt),
       minstr,maxstr);
     */
    do {
        char      numbuf[32];
        size_t    heartbeat_len;
        char     *colonp;

        /* convert heartbeat as count or duration */
        colonp = (char *) strchr(def, ':');
        if (! colonp) {
            parsetime_error = "missing separator";
            break;
        }
        heartbeat_len = colonp - def;
        if (heartbeat_len >= sizeof(numbuf)) {
            parsetime_error = "heartbeat too long";
            break;
        }
        strncpy (numbuf, def, heartbeat_len);
        numbuf[heartbeat_len] = 0;

        if ((parsetime_error = rrd_scaled_duration(numbuf, 1, &(ds_def->par[DS_mrhb_cnt].u_cnt))))
            break;

        if (sscanf(1+colonp, "%18[^:]:%18[^:]",
                   minstr, maxstr) == 2) {
            if (minstr[0] == 'U' && minstr[1] == 0)
                ds_def->par[DS_min_val].u_val = DNAN;
            else
                if( rrd_strtodbl(minstr, 0, &(ds_def->par[DS_min_val].u_val), "parsing min val") != 2 ) return;

            if (maxstr[0] == 'U' && maxstr[1] == 0)
                ds_def->par[DS_max_val].u_val = DNAN;
            else
                if( rrd_strtodbl(maxstr, 0, &(ds_def->par[DS_max_val].u_val), "parsing max val") != 2 ) return;

            if ( ds_def->par[DS_min_val].u_val >= ds_def->par[DS_max_val].u_val ) {
                parsetime_error = "min must be less than max in DS definition";
                break;
            }
        } else {
            parsetime_error = "failed to extract min:max";
        }
    } while (0);
    if (parsetime_error)
        rrd_set_error("failed to parse data source %s: %s", def, parsetime_error);
}

/* Create the CF_DEVPREDICT, CF_DEVSEASONAL, CF_SEASONAL, and CF_FAILURES RRAs
 * associated with a CF_HWPREDICT RRA. */
rra_def_t * create_hw_contingent_rras(rra_def_t *rra_defs,
				      long unsigned int *rra_cnt, 
				      unsigned short period,
				      unsigned long hash)
{
    size_t    old_size;
    rra_def_t *current_rra;

    /* save index to CF_HWPREDICT */
    unsigned long hw_index = *rra_cnt - 1;

    /* allocate the memory for the 4 contingent RRAs */
    old_size = sizeof(rra_def_t) * (*rra_cnt);
    if ((rra_defs = (rra_def_t*)rrd_realloc(rra_defs,
					   old_size + 4 * sizeof(rra_def_t))) ==
        NULL) {
        rrd_set_error("allocating rra_def");
        return NULL;
    }


    /* clear memory */
    memset(&(rra_defs[*rra_cnt]), 0,
           4 * sizeof(rra_def_t));

    rra_def_t *hw_rra = rra_defs + hw_index;

    /* create the CF_SEASONAL RRA */
    current_rra = rra_defs + *rra_cnt;

    strcpy(current_rra->cf_nam, "SEASONAL");
    current_rra->row_cnt = period;
    current_rra->par[RRA_seasonal_smooth_idx].u_cnt = hash % period;
    current_rra->pdp_cnt = 1;
    current_rra->par[RRA_seasonal_gamma].u_val = 
	hw_rra->par[RRA_hw_alpha].u_val;
    current_rra->par[RRA_dependent_rra_idx].u_cnt = hw_index;

    hw_rra->par[RRA_dependent_rra_idx].u_cnt = *rra_cnt;

    (*rra_cnt)++;

    /* create the CF_DEVSEASONAL RRA */
    current_rra = rra_defs + *rra_cnt;

    strcpy(current_rra->cf_nam, "DEVSEASONAL");
    current_rra->row_cnt = period;
    current_rra->par[RRA_seasonal_smooth_idx].u_cnt = hash % period;
    current_rra->pdp_cnt = 1;
    current_rra->par[RRA_seasonal_gamma].u_val =
        hw_rra->par[RRA_hw_alpha].u_val;
    current_rra->par[RRA_dependent_rra_idx].u_cnt = hw_index;

    (*rra_cnt)++;

    /* create the CF_DEVPREDICT RRA */
    current_rra = rra_defs + *rra_cnt;

    strcpy(current_rra->cf_nam, "DEVPREDICT");
    current_rra->row_cnt = hw_rra->row_cnt;
    current_rra->pdp_cnt = 1;
    current_rra->par[RRA_dependent_rra_idx].u_cnt = hw_index + 2;   /* DEVSEASONAL */

    (*rra_cnt)++;

    /* create the CF_FAILURES RRA */
    current_rra = rra_defs + *rra_cnt;

    strcpy(current_rra->cf_nam, "FAILURES");
    current_rra->row_cnt = period;
    current_rra->pdp_cnt = 1;
    current_rra->par[RRA_delta_pos].u_val = 2.0;
    current_rra->par[RRA_delta_neg].u_val = 2.0;
    current_rra->par[RRA_failure_threshold].u_cnt = 7;
    current_rra->par[RRA_window_len].u_cnt = 9;
    current_rra->par[RRA_dependent_rra_idx].u_cnt = hw_index + 2;   /* DEVSEASONAL */
     (*rra_cnt)++;

     return rra_defs;
}

void init_cdp(const rrd_t *rrd, const rra_def_t *rra_def, const pdp_prep_t *pdp_prep, cdp_prep_t *cdp_prep)
{

    switch (cf_conv(rra_def->cf_nam)) {
        case CF_HWPREDICT:
        case CF_MHWPREDICT:
            init_hwpredict_cdp(cdp_prep);
            break;
        case CF_SEASONAL:
        case CF_DEVSEASONAL:
            init_seasonal_cdp(cdp_prep);
            break;
        case CF_FAILURES:
            /* initialize violation history to 0 */
            for (int ii = 0; ii < MAX_CDP_PAR_EN; ii++) {
                /* We can zero everything out, by setting u_val to the
                 * NULL address. Each array entry in scratch is 8 bytes
                 * (a double), but u_cnt only accessed 4 bytes (long) */
                cdp_prep->scratch[ii].u_val = 0.0;
            }
            break;
        default:
            /* can not be zero because we don't know anything ... */
            cdp_prep->scratch[CDP_val].u_val = DNAN;
            /* startup missing pdp count */
            cdp_prep->scratch[CDP_unkn_pdp_cnt].u_cnt =
                ((rrd->live_head->last_up -
                  pdp_prep->scratch[PDP_unkn_sec_cnt].u_cnt)
                 % (rrd->stat_head->pdp_step
                    * rra_def->pdp_cnt)) / rrd->stat_head->pdp_step;
            break;
    
    }
}

/* create and empty rrd file according to the specs given */

static int rrd_init_data(rrd_t *rrd)
{
    int rc = -1;
    unsigned long i, ii;
    
    if (rrd->stat_head == NULL) {
        rrd_set_error("stat_head missing");
        goto done;
    }
    
    if (rrd->live_head == NULL) {
        rrd_set_error("live_head missing\n");
        goto done;
    }

    if (rrd->ds_def == NULL) {
        rrd_set_error("ds_def missing");
        goto done;
    }
    
    if (rrd->rra_def == NULL) {
        rrd_set_error("rra_def missing");
        goto done;

    }

    if (rrd->pdp_prep == NULL) {
        rrd->pdp_prep = calloc(rrd->stat_head->ds_cnt, sizeof(pdp_prep_t));
        if (rrd->pdp_prep == NULL) {
            rrd_set_error("cannot allocate memory");
            goto done;
        }
        for (i = 0 ; i < rrd->stat_head->ds_cnt ; i++) {
            strcpy(rrd->pdp_prep[i].last_ds, "U");
            rrd->pdp_prep[i].scratch[PDP_val].u_val = 0.0;
            rrd->pdp_prep[i].scratch[PDP_unkn_sec_cnt].u_cnt =
                rrd->live_head->last_up % rrd->stat_head->pdp_step;
        }
    }

    if (rrd->cdp_prep == NULL) {
        rrd->cdp_prep = calloc(rrd->stat_head->rra_cnt * rrd->stat_head->ds_cnt, sizeof(cdp_prep_t));
        if (rrd->cdp_prep == NULL) {
            rrd_set_error("cannot allocate memory");
            goto done;
        }
        
        for (i = 0; i < rrd->stat_head->rra_cnt; i++) {
            for (ii = 0 ; ii < rrd->stat_head->ds_cnt ; ii++) {
                init_cdp(rrd, &(rrd->rra_def[i]), 
                        rrd->pdp_prep + ii,
                        rrd->cdp_prep + rrd->stat_head->ds_cnt * i + ii);
            }
        }
    }
    
    if (rrd->rra_ptr == NULL) {
        rrd->rra_ptr = calloc(rrd->stat_head->rra_cnt, sizeof(rra_ptr_t));
        if (rrd->rra_ptr == NULL) {
            rrd_set_error("cannot allocate memory");
            goto done;
        }
        
        for (i = 0; i < rrd->stat_head->rra_cnt; i++) {
            rrd->rra_ptr[i].cur_row = rrd_select_initial_row(NULL, i, &rrd->rra_def[i]);
        }
    }
    
    if (rrd->rrd_value == NULL) {
        unsigned long total_rows = 0, total_values;
        for (i = 0 ; i < rrd->stat_head->rra_cnt ; i++) {
            total_rows += rrd->rra_def[i].row_cnt;
        }
        total_values = total_rows * rrd->stat_head->ds_cnt;
        
        rrd->rrd_value = calloc(total_values, sizeof(rrd_value_t));
        if (rrd->rrd_value == NULL) {
            rrd_set_error("cannot allocate memory");
            goto done;
        }
   
        for (i = 0 ; i < total_values ; i++) {
            rrd->rrd_value[i] = DNAN;
        }
    }
    
    rc = 0;
    /*
    if (rrd->stat_head->version < 3) {
        *rrd->legacy_last_up = rrd->live_head->last_up;
    }*/
done:
    return rc;
}

int write_rrd(const char *outfilename, rrd_t *out) {
    int rc = -1;
    char *tmpfilename = NULL;

    /* write out the new file */
    FILE *fh = NULL;
    if (strcmp(outfilename, "-") == 0) {
	fh = stdout;
	// to stdout
    } else {
	/* create RRD with a temporary name, rename atomically afterwards. */
	tmpfilename = (char *) malloc(strlen(outfilename) + 7);
	if (tmpfilename == NULL) {
	    rrd_set_error("out of memory");
	    goto done;
	}

	strcpy(tmpfilename, outfilename);
	strcat(tmpfilename, "XXXXXX");
	
	int tmpfd = mkstemp(tmpfilename);
	if (tmpfd < 0) {
	    rrd_set_error("Cannot create temporary file");
	    goto done;
	}

	fh = fdopen(tmpfd, "wb");
	if (fh == NULL) {
	    // some error 
	    rrd_set_error("Cannot open output file");
	    goto done;
	}
    }

    rc = write_fh(fh, out);

    if (fh != NULL && tmpfilename != NULL) {
	/* tmpfilename != NULL indicates that we did NOT write to stdout,
	   so we have to close the stream and do the rename dance */

	fclose(fh);
	if (rc == 0)  {
	    // renaming is only done if write_fh was successful
	    struct stat stat_buf;

	    /* in case we have an existing file, copy its mode... This
	       WILL NOT take care of any ACLs that may be set. Go
	       figure. */
	    if (stat(outfilename, &stat_buf) != 0) {
#ifdef WIN32
                stat_buf.st_mode = _S_IREAD | _S_IWRITE;  // have to test it is 
#else
		/* an error occurred (file not found, maybe?). Anyway:
		   set the mode to 0666 using current umask */
		stat_buf.st_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
		
		mode_t mask = umask(0);
		umask(mask);

		stat_buf.st_mode &= ~mask;
#endif                
	    }
	    if (chmod(tmpfilename, stat_buf.st_mode) != 0) {
		rrd_set_error("Cannot chmod temporary file!");
		goto done;
	    }

	    // before we rename the file to the target file: forget all cached changes....
	    if (rrdc_is_any_connected()) {
		// is it a good idea to just ignore the error ????
		rrdc_forget(outfilename);
		rrd_clear_error();
	    }

	    if (rename(tmpfilename, outfilename) != 0) {
		rrd_set_error("Cannot rename temporary file to final file!");
		goto done;
	    }

	    // after the rename: forget the file again, just to be sure...
	    if (rrdc_is_any_connected()) {
		// is it a good idea to just ignore the error ????
		rrdc_forget(outfilename);
		rrd_clear_error();
	    }
	} else {
	    /* in case of any problems during write: just remove the
	       temporary file! */
	    unlink(tmpfilename);
	}
    }
done:
    if (tmpfilename != NULL) {
        /* remove temp. file by name - and ignore errors, because it might have 
         * been successfully renamed. And if somebody else used the same temp.
         * file name - well that is bad luck, I guess.. */
        unlink(tmpfilename);
	free(tmpfilename);
    }

    return rc;
}

int write_fh(
    FILE *fh,
    rrd_t *rrd)
{
    unsigned int i;
    unsigned int rra_offset;

    if (atoi(rrd->stat_head->version) < 3) {
        /* we output 3 or higher */
        strcpy(rrd->stat_head->version, "0003");
    }
    fwrite(rrd->stat_head, sizeof(stat_head_t), 1, fh);
    fwrite(rrd->ds_def, sizeof(ds_def_t), rrd->stat_head->ds_cnt, fh);
    fwrite(rrd->rra_def, sizeof(rra_def_t), rrd->stat_head->rra_cnt, fh);
    fwrite(rrd->live_head, sizeof(live_head_t), 1, fh);
    fwrite(rrd->pdp_prep, sizeof(pdp_prep_t), rrd->stat_head->ds_cnt, fh);
    fwrite(rrd->cdp_prep, sizeof(cdp_prep_t),
           rrd->stat_head->rra_cnt * rrd->stat_head->ds_cnt, fh);
    fwrite(rrd->rra_ptr, sizeof(rra_ptr_t), rrd->stat_head->rra_cnt, fh);

    /* calculate the number of rrd_values to dump */
    rra_offset = 0;
    for (i = 0; i < rrd->stat_head->rra_cnt; i++) {
        unsigned long num_rows = rrd->rra_def[i].row_cnt;
        unsigned long ds_cnt = rrd->stat_head->ds_cnt;
        if (num_rows > 0){
            fwrite(rrd->rrd_value + rra_offset * ds_cnt,
                    sizeof(rrd_value_t), 
                    num_rows * ds_cnt, fh);
                    
            rra_offset += num_rows;
        }
    }

    return (0);
}                       /* int write_file */

static long overlap(long start1, long end1, long start2, long end2) {
    if (start1 >= end1) return 0;
    if (start2 >= end2) return 0;

    if (start1 > end2) return 0;
    if (start2 > end1) return 0;

    return min(end1, end2) - max(start1, start2);
}

static void debug_dump_rra(const rrd_t *rrd, int rra_index, int ds_index) {
    long total_cnt = 0;
    for (int zz = 0 ; zz < rra_index ; zz++) {
        total_cnt += rrd->rra_def[rra_index].row_cnt;
    }
    fprintf(stderr, "Dump rra_index=%d ds_index=%d\n", rra_index, ds_index);
    
    for (unsigned int zz = 0 ; zz < rrd->rra_def[rra_index].row_cnt ; zz++) {
        time_t zt = end_time_for_row_simple(rrd, rra_index, zz);

        rrd_value_t v = rrd->rrd_value[rrd->stat_head->ds_cnt * (total_cnt + zz) + ds_index];

        int zz_calc = row_for_time(rrd, rrd->rra_def + rra_index, 
                                   rrd->rra_ptr[rra_index].cur_row, zt);
        fprintf(stderr, "%d %ld %g %d\n", zz, zt, v, zz_calc);
    }
}

static int rrd_prefill_data(rrd_t *rrd, const GList *sources) {
    int rc = -1;
    
    if (sources == NULL) {
        // we are done if there is nothing to copy data from
        rc = 0;
        goto done;
    }

    unsigned long i, j, si, sj;
    unsigned long total_rows = 0;
    
    debug_dump_rra(((rrd_file_t *) sources->data)->rrd, 0, 0);

    // process one RRA after the other
    for (i = 0 ; i < rrd->stat_head->rra_cnt ; i++) {
        fprintf(stderr, "PREFILL RRA %ld\n", i);

        rra_def_t *rra_def = rrd->rra_def + i;
        unsigned long cur_row = rrd->rra_ptr[i].cur_row;

        for (j = 0 ; j < rrd->stat_head->ds_cnt ; j++) {
            /* for each DS in each RRA within rrd find a list of candidate DS/RRAs from 
             * the sources list that match by name... */
    
            ds_def_t  *ds_def  = rrd->ds_def + j;
            
            const GList *src;
            candidate_t *candidates = NULL;
            int candidate_cnt = 0;
            
            for (src = sources ; src ;  src = g_list_next(src)) {
                // first: find matching DS
                
                const rrd_file_t *rrd_file = src->data;
                if (rrd_file == NULL) continue;
                
                const rrd_t *src_rrd = rrd_file->rrd;
                if (src_rrd == NULL) continue; 

                fprintf(stderr, "cur_rows: %ld %ld\n", 
                        rrd->rra_ptr[0].cur_row, src_rrd->rra_ptr[0].cur_row);
                
                fprintf(stderr, "src rrd last_up %ld\n", src_rrd->live_head->last_up);
                fprintf(stderr, "dst rrd last_up %ld\n", rrd->live_head->last_up);
                
                int found_ds_index = -1;
                for (sj = 0 ; sj < src_rrd->stat_head->ds_cnt ; sj++) {
                    if (strcmp(ds_def->ds_nam, src_rrd->ds_def[sj].ds_nam) == 0) {
                        // name match!!!
                        found_ds_index = sj;
                        
                        // candidates = g_list_append(candidates, (gpointer) src);
                        candidates = find_candidate_rras(src_rrd, rra_def, &candidate_cnt);
                        
for (unsigned int tt = 0 ; tt < src_rrd->stat_head->rra_cnt ; tt++) {
    fprintf(stderr, "SRC RRA %d row_cnt=%ld\n", tt, src_rrd->rra_def[tt].row_cnt);
}                        
for (int tt = 0 ; tt < candidate_cnt ; tt++) {
    fprintf(stderr, "CAND SRC RRA %d row_cnt=%ld\n", candidates[tt].rra_index, candidates[tt].rra->row_cnt);
}                 
                    }
                }
            }

            /* walk all RRA bins and fill the current DS with data from 
             * the list of candidates */

            unsigned long cnt = 0, k;
            
            for (cnt = 0 ; cnt < rra_def->row_cnt ; cnt++) {
                long bin_size = rra_def->pdp_cnt * rrd->stat_head->pdp_step;
                time_t bin_end_time = end_time_for_row_simple(rrd, i, cnt);
                time_t bin_start_time = bin_end_time - bin_size + 1;
                
                fprintf(stderr, "Bin %ld from %ld to %ld\n", cnt, bin_start_time, bin_end_time);
                /* find corresponding range of bins in all candidates... */
                
                rrd_value_t best_value = 0, best_covered = 0, best_covering_bins = 0;
                for (k = 0 ; k < (unsigned long) candidate_cnt ; k++) {
                    candidate_t *candidate = candidates + k;
                    
                    rra_def_t * candidate_rra_def = candidate->rra;
                    
                    //candidates[k].
                    unsigned long end_bin = row_for_time(candidate->rrd, candidate->rra, 
                            candidate->rrd->rra_ptr[candidate->rra_index].cur_row,
                            bin_end_time);
                    unsigned long start_bin = row_for_time(candidate->rrd, candidate->rra, 
                            candidate->rrd->rra_ptr[candidate->rra_index].cur_row,
                            bin_start_time);
                    fprintf(stderr, " candidate #%ld (index=%d) from %ld to %ld (row_cnt=%ld)\n", k, 
                            candidate->rra_index, 
                            start_bin, end_bin, 
                            candidate_rra_def->row_cnt);
                    
                    if (start_bin < candidate_rra_def->row_cnt && end_bin < candidate_rra_def->row_cnt) {
                        int bin_count = positive_mod(end_bin - start_bin + 1, candidate_rra_def->row_cnt);
                        fprintf(stderr, "  bin_count %d\n", bin_count);
                        
                        long total_covered = 0, covering_bins = 0;
                        rrd_value_t value = 0;
                        
                        for (int ci = start_bin ; bin_count > 0 ; ci++, bin_count-- ) {
                            // find overlap range....
                            long cand_bin_size = candidate_rra_def->pdp_cnt * candidate->rrd->stat_head->pdp_step;
                            time_t cand_bin_end_time = end_time_for_row_simple(candidate->rrd, candidate->rra_index, ci);
                            time_t cand_bin_start_time = cand_bin_end_time - cand_bin_size + 1;
                            
                            long covered = overlap(bin_start_time, bin_end_time,
                                                   cand_bin_start_time, cand_bin_end_time) +1;
                            rrd_value_t v = candidate->values[ci * candidate->rrd->stat_head->ds_cnt + j];
                            if (covered > 0 && v != NAN) {
                                total_covered += covered;
                                covering_bins++;
                                
                                value += v / cand_bin_size * covered;
                            }
                            fprintf(stderr, "  covers from %ld to %ld overlap is %g value=%g\n",
                                        cand_bin_start_time, cand_bin_end_time,
                                        (float) covered / bin_size, v);
                        }
                        fprintf(stderr, "total coverage=%ld/%ld from %ld bins\n", total_covered, bin_size, covering_bins);
                        
                        if (total_covered > best_covered || (total_covered == best_covered && covering_bins < best_covering_bins)) {
                            fprintf(stderr, "Choosing as current best value %g\n", value);
                            best_value = value;
                            best_covered = total_covered;
                            best_covering_bins = covering_bins;
                        }
                    }
                    
                }
                //row_for_time();

                if (best_covered > 0) {
                    
                    *(rrd->rrd_value + rrd->stat_head->ds_cnt * (total_rows + cnt) + j) = best_value;
                }
            }

            if (candidates) {
                free(candidates);
                candidates = NULL;
            }
        }
        total_rows += rra_def->row_cnt;
    }
    
    rc = 0;
    /* within each source file, order the RRAs by resolution - if we have an 
     * exact resolution match, use that one as the first in the (sub)list. */
    
    /* for each bin in each RRA select the best bin from among the candidate 
     * RRA data sets */
debug_dump_rra(rrd, 0, 0);
done:
    return rc;
}

// calculate a % b, guaranteeing a positive result...
static int positive_mod(int a, int b) {
    int x = a % b;
    if (x < 0) x += b;
    return x;
}

time_t end_time_for_row_simple(const rrd_t *rrd, 
			int rra_index, int row) {
    rra_def_t *rra = rrd->rra_def + rra_index;
    unsigned int cur_row = rrd->rra_ptr[rra_index].cur_row;
    
    return end_time_for_row(rrd, rra, cur_row, row);
}

time_t end_time_for_row(const rrd_t *rrd, 
                           const rra_def_t *rra,
			   int cur_row, int row) {

    // one entry in the candidate covers timeslot seconds
    int timeslot = rra->pdp_cnt * rrd->stat_head->pdp_step;
	    
    /* Just to re-iterate how data is stored in RRAs, in order to
       understand the following code: the current slot was filled at
       last_up time, but slots always correspond with time periods of
       length timeslot, ending at exact multiples of timeslot
       wrt. the unix epoch. So the current timeslot ends at:
       
       int(last_up / timeslot) * timeslot 
       
       or (equivalently):
     t
       last_up - last_up % timeslot
    */

    int past_cnt = positive_mod((cur_row - row), rra->row_cnt);
    
    time_t last_up = rrd->live_head->last_up;
    time_t now = (last_up - last_up % timeslot) - past_cnt * timeslot;

    
        fprintf(stderr, "ETFR %012lx, %012lx, cr=%d, r=%d, ts=%d, pc=%d, lu=%ld = %ld\n", 
                rrd, rra, cur_row, row,
                timeslot, past_cnt, last_up, now);
    

    return now;
}

int row_for_time(const rrd_t *rrd, 
		 const rra_def_t *rra, 
		 int cur_row, time_t req_time) 
{
    time_t last_up = rrd->live_head->last_up;
    int    timeslot = rra->pdp_cnt * rrd->stat_head->pdp_step;

    // align to slot boundary end times
    time_t delta = req_time % timeslot;
    if (delta > 0) req_time += timeslot - delta;
    
    delta = req_time % timeslot;
    if (delta > 0) last_up += timeslot - delta;

    if (req_time > last_up) return -1;  // out of range
    if (req_time <= (int) last_up - (int) rra->row_cnt * timeslot) return -1; // out of range
     
    int past_cnt = (last_up - req_time) / timeslot;
    if (past_cnt >= (int) rra->row_cnt) return -1;

    // NOTE: rra->row_cnt is unsigned!!
    int row = positive_mod(cur_row - past_cnt, rra->row_cnt);

    return row < 0 ? (row + (int) rra->row_cnt) : row ;
}

