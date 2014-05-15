/*****************************************************************************
 * RRDtool 1.4.3  Copyright by Tobi Oetiker, 1997-2010
 *****************************************************************************
 * rrd_create.c  creates new rrds
 *****************************************************************************/

#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <math.h>

#include "rrd_tool.h"
#include "rrd_rpncalc.h"
#include "rrd_hw.h"
#include "rrd_client.h"
#include "rrd_config.h"
#include "rrd_create.h"

#include "rrd_is_thread_safe.h"

#ifdef WIN32
# include <process.h>
#endif

unsigned long FnvHash(
    const char *str);
void      parseGENERIC_DS(
    const char *def,
    ds_def_t *ds_def);

static int convert_to_count (const char * token,
			     unsigned long * valuep,
			     unsigned long divisor)
{
  char * ep = NULL;
  unsigned long int value = strtoul(token, &ep, 10);
  switch (*ep) {
    case 0: /* count, no conversion */
      break;
    case 's': /* seconds */
      value /= divisor;
      break;
    case 'm': /* minutes */
      value = (60 * value) / divisor;
      break;
    case 'h': /* hours */
      value = (60 * 60 * value) / divisor;
      break;
    case 'd': /* days */
      value = (24 * 60 * 60 * value) / divisor;
      break;
    case 'w': /* weeks */
      value = (7 * 24 * 60 * 60 * value) / divisor;
      break;
    case 'M': /* months */
      value = (31 * 24 * 60 * 60 * value) / divisor;
      break;
    case 'y': /* years */
      value = (366 * 24 * 60 * 60 * value) / divisor;
      break;
    default:
      return 0;
  }
  *valuep = value;
  return (0 != value);
}

int rrd_create(
    int argc,
    char **argv)
{
    struct option long_options[] = {
        {"start", required_argument, 0, 'b'},
        {"step", required_argument, 0, 's'},
        {"daemon", required_argument, 0, 'd'},
        {"no-overwrite", no_argument, 0, 'O'},
        {0, 0, 0, 0}
    };
    int       option_index = 0;
    int       opt;
    time_t    last_up = time(NULL) - 10;
    unsigned long pdp_step = 300;
    rrd_time_value_t last_up_tv;
    char     *parsetime_error = NULL;
    int       rc;
    char * opt_daemon = NULL;
    int       opt_no_overwrite = 0;

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
                return (-1);
            }
            break;

        case 'b':
            if ((parsetime_error = rrd_parsetime(optarg, &last_up_tv))) {
                rrd_set_error("start time: %s", parsetime_error);
                return (-1);
            }
            if (last_up_tv.type == RELATIVE_TO_END_TIME ||
                last_up_tv.type == RELATIVE_TO_START_TIME) {
                rrd_set_error("specifying time relative to the 'start' "
                              "or 'end' makes no sense here");
                return (-1);
            }

            last_up = mktime(&last_up_tv.tm) +last_up_tv.offset;

            if (last_up < 3600 * 24 * 365 * 10) {
                rrd_set_error
                    ("the first entry to the RRD should be after 1980");
                return (-1);
            }
            break;

        case 's':
            if (! convert_to_count(optarg, &pdp_step, 1)) {
                rrd_set_error("step size should be no less than one second");
                return (-1);
            }
            break;

        case 'O':
            opt_no_overwrite = 1;
	    break;

        case '?':
            if (optopt != 0)
                rrd_set_error("unknown option '%c'", optopt);
            else
                rrd_set_error("unknown option '%s'", argv[optind - 1]);
            return (-1);
        }
    }
    if (optind == argc) {
        rrd_set_error("need name of an rrd file to create");
        return -1;
    }

    rrdc_connect (opt_daemon);
    if (rrdc_is_connected (opt_daemon)) {
        rc = rrdc_create (argv[optind],
                      pdp_step, last_up, opt_no_overwrite,
                      argc - optind - 1, (const char **) (argv + optind + 1));
	} else {
        rc = rrd_create_r2(argv[optind],
                      pdp_step, last_up, opt_no_overwrite,
                      argc - optind - 1, (const char **) (argv + optind + 1));
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
		if (! convert_to_count(token, &rra_def->row_cnt, 1))
		    rrd_set_error("Invalid row count: %s", token);
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
		if (! convert_to_count(token, &rra_def->pdp_cnt, rrd->stat_head->pdp_step))
		    rrd_set_error("Invalid step: must be >= 1");
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
                if (! convert_to_count(token, &rra_def->row_cnt, rra_def->pdp_cnt))
		    rrd_set_error("Invalid row count: %s", token);
#if SIZEOF_TIME_T == 4
		if ((long long) pdp_step * rra_def->pdp_cnt * row_cnt > 4294967296LL){
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
	return rrd_create_r2(filename,pdp_step,last_up,0,argc,argv);
}
int rrd_create_r2(
    const char *filename,
    unsigned long pdp_step,
    time_t last_up,
    int no_overwrite,
    int argc,
    const char **argv)
{
    rrd_t     rrd;
    long      i;
    unsigned long hashed_name;
    int rc = -1;
    
    /* clear any previous errors */
    rrd_clear_error();

    /* init rrd clean */
    rrd_init(&rrd);
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
    rc = rrd_create_fn(filename, &rrd, no_overwrite);
    
done:
    rrd_free(&rrd);
    return rc;
}

void parseGENERIC_DS(
    const char *def,
    ds_def_t *ds_def)
{
    char      minstr[DS_NAM_SIZE], maxstr[DS_NAM_SIZE];
    char     *old_locale;
    int       emit_failure = 1;

    /*
       int temp;

       temp = sscanf(def,"%lu:%18[^:]:%18[^:]", 
       &(rrd -> ds_def[ds_idx].par[DS_mrhb_cnt].u_cnt),
       minstr,maxstr);
     */
    old_locale = setlocale(LC_NUMERIC, "C");
    do {
        char      numbuf[32];
        size_t    heartbeat_len;
        char     *colonp;

        /* convert heartbeat as count or duration */
        colonp = strchr(def, ':');
        if (! colonp)
            break;
        heartbeat_len = colonp - def;
        if (heartbeat_len >= sizeof(numbuf))
            break;
        strncpy (numbuf, def, heartbeat_len);
        numbuf[heartbeat_len] = 0;

        if (! convert_to_count(numbuf, &(ds_def->par[DS_mrhb_cnt].u_cnt), 1))
            break;

        if (sscanf(1+colonp, "%18[^:]:%18[^:]",
                   minstr, maxstr) == 2) {
            emit_failure = 0;
            if (minstr[0] == 'U' && minstr[1] == 0)
                ds_def->par[DS_min_val].u_val = DNAN;
            else
                ds_def->par[DS_min_val].u_val = atof(minstr);

            if (maxstr[0] == 'U' && maxstr[1] == 0)
                ds_def->par[DS_max_val].u_val = DNAN;
            else
                ds_def->par[DS_max_val].u_val = atof(maxstr);

            if (!isnan(ds_def->par[DS_min_val].u_val) &&
                !isnan(ds_def->par[DS_max_val].u_val) &&
                ds_def->par[DS_min_val].u_val
                >= ds_def->par[DS_max_val].u_val) {
                rrd_set_error("min must be less than max in DS definition");
                break;
            }
        }
    } while (0);
    if (emit_failure)
        rrd_set_error("failed to parse data source %s", def);
    setlocale(LC_NUMERIC, old_locale);
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

void init_cdp(const rrd_t *rrd, const rra_def_t *rra_def, cdp_prep_t *cdp_prep)
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
                  rrd->pdp_prep->scratch[PDP_unkn_sec_cnt].u_cnt)
                 % (rrd->stat_head->pdp_step
                    * rra_def->pdp_cnt)) / rrd->stat_head->pdp_step;
            break;
    
    }
}

/* create and empty rrd file according to the specs given */

int rrd_create_fn(
    const char *file_name,
    rrd_t *rrd,
    int no_overwrite )
{
    unsigned long i, ii;
    rrd_value_t *unknown;
    int       unkn_cnt;
    rrd_file_t *rrd_file_dn;
    rrd_t     rrd_dn;
    unsigned  rrd_flags = RRD_READWRITE | RRD_CREAT;

    if (no_overwrite) {
      rrd_flags |= RRD_EXCL ;
    }

    unkn_cnt = 0;
    for (i = 0; i < rrd->stat_head->rra_cnt; i++)
        unkn_cnt += rrd->stat_head->ds_cnt * rrd->rra_def[i].row_cnt;

    if ((rrd_file_dn = rrd_open(file_name, rrd, rrd_flags)) == NULL) {
        rrd_set_error("creating '%s': %s", file_name, rrd_strerror(errno));
        return (-1);
    }

    rrd_write(rrd_file_dn, rrd->stat_head, sizeof(stat_head_t));

    rrd_write(rrd_file_dn, rrd->ds_def, sizeof(ds_def_t) * rrd->stat_head->ds_cnt);

    rrd_write(rrd_file_dn, rrd->rra_def,
          sizeof(rra_def_t) * rrd->stat_head->rra_cnt);

    rrd_write(rrd_file_dn, rrd->live_head, sizeof(live_head_t));

    if ((rrd->pdp_prep = (pdp_prep_t*)calloc(1, sizeof(pdp_prep_t))) == NULL) {
        rrd_set_error("allocating pdp_prep");
        rrd_close(rrd_file_dn);
        return (-1);
    }

    strcpy(rrd->pdp_prep->last_ds, "U");

    rrd->pdp_prep->scratch[PDP_val].u_val = 0.0;
    rrd->pdp_prep->scratch[PDP_unkn_sec_cnt].u_cnt =
        rrd->live_head->last_up % rrd->stat_head->pdp_step;

    for (i = 0; i < rrd->stat_head->ds_cnt; i++)
        rrd_write(rrd_file_dn, rrd->pdp_prep, sizeof(pdp_prep_t));

    if ((rrd->cdp_prep = (cdp_prep_t*)calloc(1, sizeof(cdp_prep_t))) == NULL) {
        rrd_set_error("allocating cdp_prep");
        rrd_close(rrd_file_dn);
        return (-1);
    }


    for (i = 0; i < rrd->stat_head->rra_cnt; i++) {
	init_cdp(rrd, &(rrd->rra_def[i]), rrd->cdp_prep);

        for (ii = 0; ii < rrd->stat_head->ds_cnt; ii++) {
            rrd_write(rrd_file_dn, rrd->cdp_prep, sizeof(cdp_prep_t));
        }
    }

    /* now, we must make sure that the rest of the rrd
       struct is properly initialized */

    if ((rrd->rra_ptr = (rra_ptr_t*)calloc(1, sizeof(rra_ptr_t))) == NULL) {
        rrd_set_error("allocating rra_ptr");
        rrd_close(rrd_file_dn);
        return (-1);
    }

    /* changed this initialization to be consistent with
     * rrd_restore. With the old value (0), the first update
     * would occur for cur_row = 1 because rrd_update increments
     * the pointer a priori. */
    for (i = 0; i < rrd->stat_head->rra_cnt; i++) {
        rrd->rra_ptr->cur_row = rrd_select_initial_row(rrd_file_dn, i, &rrd->rra_def[i]);
        rrd_write(rrd_file_dn, rrd->rra_ptr, sizeof(rra_ptr_t));
    }

    /* write the empty data area */
    if ((unknown = (rrd_value_t *) malloc(512 * sizeof(rrd_value_t))) == NULL) {
        rrd_set_error("allocating unknown");
        rrd_close(rrd_file_dn);
        return (-1);
    }
    for (i = 0; i < 512; ++i)
        unknown[i] = DNAN;

    while (unkn_cnt > 0) {
        if(rrd_write(rrd_file_dn, unknown, sizeof(rrd_value_t) * min(unkn_cnt, 512)) < 0)
        {
            rrd_set_error("creating rrd: %s", rrd_strerror(errno));
            return -1;
        }

        unkn_cnt -= 512;
    }
    free(unknown);
    if (rrd_close(rrd_file_dn) == -1) {
        rrd_set_error("creating rrd: %s", rrd_strerror(errno));
        return -1;
    }
    /* flush all we don't need out of the cache */
    rrd_init(&rrd_dn);
    if((rrd_file_dn = rrd_open(file_name, &rrd_dn, RRD_READONLY)) != NULL)
    {
        rrd_dontneed(rrd_file_dn, &rrd_dn);
        /* rrd_free(&rrd_dn); */  /* FIXME: Why is this commented out? 
                                     This would only make sense if
                                     we are sure about mmap */
        rrd_close(rrd_file_dn);
    }
    return (0);
}


