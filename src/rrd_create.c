/*****************************************************************************
 * RRDtool 1.GIT, Copyright by Tobi Oetiker
 *****************************************************************************
 * rrd_create.c  creates new rrds
 *****************************************************************************/

#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <math.h>
#include <glib.h>   // will use glist and regex



#include <sys/types.h>      // stat()
#include <sys/stat.h>       // stat()
#include <unistd.h>         // stat()

#include "rrd_strtod.h"
#include "rrd_tool.h"

#ifndef HAVE_G_REGEX_NEW
#ifdef HAVE_PCRE_COMPILE
#include <pcre.h>
#else
#error "you must have either glib with regexp support or libpcre"
#endif
#endif

#include "rrd_rpncalc.h"
#include "rrd_hw.h"
#include "rrd_client.h"
#include "rrd_config.h"
#include "rrd_create.h"
#include "rrd_update.h"

#include "rrd_is_thread_safe.h"
#include "rrd_modify.h"
#include "quicksort.h"

#include "unused.h"

#ifdef WIN32
# include <process.h>
#endif

#ifdef HAVE_LIBRADOS
#include "rrd_rados.h"
#endif

static void reset_pdp_prep(rrd_t *rrd);
static int rrd_init_data(rrd_t *rrd);
static int rrd_prefill_data(rrd_t *rrd, const GList *sources_rrd_files,
                            mapping_t *mappings, int mappings_cnt);
static int positive_mod(int a, int b);

static void init_mapping(mapping_t *mapping);
static void free_mapping(mapping_t *mapping);

unsigned long FnvHash(
    const char *str);
void      parseGENERIC_DS(
    const char *def,
    ds_def_t *ds_def);

int rrd_create(
    int argc,
    char **argv)
{
    struct optparse_long longopts[] = {
        {"start", 'b', OPTPARSE_REQUIRED},
        {"step", 's', OPTPARSE_REQUIRED},
        {"daemon", 'd', OPTPARSE_REQUIRED},
        {"source", 'r', OPTPARSE_REQUIRED},
        {"template", 't', OPTPARSE_REQUIRED},
        {"no-overwrite", 'O', OPTPARSE_NONE},
        {0},
    };
    struct optparse options;
    int opt;
    time_t    last_up = -1;
    unsigned long pdp_step = 0;
    rrd_time_value_t last_up_tv;
    const char *parsetime_error = NULL;
    int       rc = -1;
    char * opt_daemon = NULL;
    int       opt_no_overwrite = 0;
    GList * sources = NULL;
    const char **sources_array = NULL;
    char *template = NULL;
    
    optparse_init(&options, argc, argv);
    while ((opt = optparse_long(&options, longopts, NULL)) != -1) {
        switch (opt) {
        case 'd':
            if (opt_daemon != NULL)
                free (opt_daemon);
            opt_daemon = strdup(options.optarg);
            if (opt_daemon == NULL)
            {
                rrd_set_error ("strdup failed.");
                rc = -1;
                goto done;
            }
            break;

        case 'b':
            if ((parsetime_error = rrd_parsetime(options.optarg, &last_up_tv))) {
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
            if ((parsetime_error = rrd_scaled_duration(options.optarg, 1, &pdp_step))) {
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
            if (stat(options.optarg, &st) != 0) {
                rrd_set_error("error checking for source RRD %s: %s", options.optarg, rrd_strerror(errno));
                rc = -1;
                goto done;
            } 
            
            if (!S_ISREG(st.st_mode)) {
                rrd_set_error("Not a regular file: %s", options.optarg);
                rc = -1;
                goto done;
            }
            char * optcpy = strdup(options.optarg);
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
        case 't': {
            if (template != NULL) {
                rrd_set_error("template already set");
                rc = -1;
                goto done;
            }
            char * optcpy = strdup(options.optarg);
            if (optcpy == NULL) {
                rrd_set_error("Cannot allocate string");
                rc = -1;
                goto done;
            }

            template = optcpy;
            break;
        }
        case '?':
            rrd_set_error("%s", options.errmsg);
            rc = -1;
            goto done;
        }
    }
    if (options.optind == options.argc) {
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
        rc = rrdc_create_r2(options.argv[options.optind],
                      pdp_step, last_up, opt_no_overwrite, 
                      sources_array, template,
                      options.argc - options.optind - 1,
                      (const char **) (options.argv + options.optind + 1));
    } else {
        rc = rrd_create_r2(options.argv[options.optind],
                      pdp_step, last_up, opt_no_overwrite,
                      sources_array, template,
                      options.argc - options.optind - 1,
                      (const char **) (options.argv + options.optind + 1));
    }
done:
    if (sources_array != NULL) {
        free(sources_array);
        sources_array = NULL;
    }
    if (sources != NULL) {
        // this will free the list elements as well
        g_list_foreach( sources, (GFunc)free, NULL );
        g_list_free( sources );
        sources = NULL;
    }
    if (template != NULL) {
        free(template);
        template = NULL;
    }
    return rc;
}

#ifndef HAVE_STRNDUP
/* Implement the strndup function.
   Copyright (C) 2005 Free Software Foundation, Inc.
   Written by Kaveh R. Ghazi <ghazi@caip.rutgers.edu>. 

This function is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,
Boston, MA 02110-1301, USA.  */

static char* strndup (const char *s, size_t n) {
  char *result;
  size_t len = strlen (s);

  if (n < len)
    len = n;

  result = (char *) malloc (len + 1);
  if (!result)
    return 0;

  result[len] = '\0';
  return (char *) memcpy (result, s, len);
}

#endif

//                           1                  2                    3               4                5
static const char *DS_RE = "^(" DS_NAM_RE ")(?:=(" DS_NAM_RE ")(?:\\[([0-9]+)\\])?)?:(" DST_FMT_RE "):(.+)$";
/*
 * relevant RE subgroups:
 * 0 .. the entire input 
 * 1 .. the DS name
 * 2 .. the mapped DS name
 * 3 .. the optional integer mapped DS index
 * 4 .. the DS format (AVERAGE....)
 * 5 .. the DS format specific part
 */
#define DS_NAME_SUBGROUP            1
#define MAPPED_DS_NAME_SUBGROUP     2
#define OPT_MAPPED_INDEX_SUBGROUP   3
#define DST_SUBGROUP                4
#define DST_ARGS_SUBGROUP           5

int parseDS(const char *def, 
	    ds_def_t *ds_def,
	    void *key_hash,
            long (*lookup)(void *, char *),
            mapping_t *mapping,
	    const char **require_version)
{
    int rc = -1;
    char *dst_tmp = NULL;
    char *dst_args = NULL;
#ifdef HAVE_G_REGEX_NEW    
    GError *gerr = NULL;
    GRegex *re = g_regex_new(DS_RE, G_REGEX_EXTENDED, 0, &gerr);
    GMatchInfo *mi = NULL;

    if (gerr != NULL) {
        rrd_set_error("cannot compile RE: %s", gerr->message);
        goto done;
    }
    int m = g_regex_match(re, def, 0, &mi);
#else
#define OVECCOUNT 30    /* should be a multiple of 3 */
    pcre *re;
    const char *error;
    int erroffset;
    int ovector[OVECCOUNT];
    re = pcre_compile(DS_RE,PCRE_EXTENDED,&error,&erroffset,NULL);
    if (re == NULL){
        rrd_set_error("cannot compile regular expression: %s (%s)", error,DS_RE);
        goto done;
    }
    int m = pcre_exec(re,NULL,def,(int)strlen(def),0,0,ovector,OVECCOUNT);
#endif
    if (!m) {
        rrd_set_error("invalid DS format");
        goto done;
    }
    
    /*
    int scnt = g_regex_get_capture_count(re);
    int i;
    for (i = 0 ; i < scnt+1 ; i++) {
            gint s, e;
            if (! g_match_info_fetch_pos (mi, i, &s, &e)) continue;
            if (e != s) fprintf(stderr, "%d %.*s %d\n", i, e - s, def + s, e - s);
    }
    */
    int s, e, s2, e2;

    // NAME
    memset(ds_def->ds_nam, 0, sizeof(ds_def->ds_nam));
#ifdef HAVE_G_REGEX_NEW     
    g_match_info_fetch_pos(mi, DS_NAME_SUBGROUP, &s, &e);
#else
    s=ovector[DS_NAME_SUBGROUP*2];
    e=ovector[DS_NAME_SUBGROUP*2+1];
#endif
    strncpy(ds_def->ds_nam, def + s, e - s);

    // DST + DST args
#ifdef HAVE_G_REGEX_NEW     
    g_match_info_fetch_pos(mi, DST_SUBGROUP, &s, &e);
    g_match_info_fetch_pos(mi, DST_ARGS_SUBGROUP, &s2, &e2);
#else
    s=ovector[DST_SUBGROUP*2];
    e=ovector[DST_SUBGROUP*2+1];
    s2=ovector[DST_ARGS_SUBGROUP*2];
    e2=ovector[DST_ARGS_SUBGROUP*2+1];
#endif        

    dst_tmp  = strndup(def + s, e - s);
    dst_args = strndup(def + s2, e2 - s2);
        
    if ((dst_conv(dst_tmp) == DST_DCOUNTER || dst_conv(dst_tmp) == DST_DDERIVE) &&
      (*require_version == NULL || atoi(*require_version) < atoi(RRD_VERSION5))) {
        *require_version = RRD_VERSION5;
    }

    switch (dst_conv(dst_tmp)) {
    case DST_COUNTER:
    case DST_ABSOLUTE:
    case DST_GAUGE:
    case DST_DERIVE:
    case DST_DCOUNTER:
    case DST_DDERIVE:
        strncpy(ds_def->dst, dst_tmp, DST_SIZE - 1);
	parseGENERIC_DS(dst_args, ds_def);
	break;
    case DST_CDEF:
	strncpy(ds_def->dst, dst_tmp, DST_SIZE - 1);
        parseCDEF_DS(dst_args, ds_def, key_hash, lookup);
	break;
    default:
	rrd_set_error("invalid DS type specified (%s)", dst_tmp);
	goto done;
    }
   
    // mapping, but only if we are interested in it...
    if (mapping) {
        char *endptr;
        mapping->ds_nam = strdup(ds_def->ds_nam);
#ifdef HAVE_G_REGEX_NEW     
        g_match_info_fetch_pos(mi, MAPPED_DS_NAME_SUBGROUP, &s, &e);
#else
        s=ovector[MAPPED_DS_NAME_SUBGROUP*2];
        e=ovector[MAPPED_DS_NAME_SUBGROUP*2+1];
#endif
        mapping->mapped_name = strndup(def + s, e - s);
        if (mapping->ds_nam == NULL || mapping->mapped_name == NULL) {
            rrd_set_error("Cannot allocate memory");
            goto done;
        }
#ifdef HAVE_G_REGEX_NEW        
        g_match_info_fetch_pos(mi, OPT_MAPPED_INDEX_SUBGROUP, &s, &e);
#else
        s=ovector[OPT_MAPPED_INDEX_SUBGROUP*2];
        e=ovector[OPT_MAPPED_INDEX_SUBGROUP*2+1];
#endif
        /* we do not have to check for errors: invalid indices will be checked later, 
         * and syntactically, the RE has done the job for us already*/
        mapping->index = s != e ? strtol(def + s, &endptr, 10) : -1;

    }
    rc = 0;
 
done:
    if (re) {
#ifdef HAVE_G_REGEX_NEW 
        g_match_info_free(mi);
        g_regex_unref(re);
#else
        pcre_free(re);
#endif
    }

    if (dst_tmp) free(dst_tmp);
    if (dst_args) free(dst_args);

    return rc;
}

int parseRRA(const char *def,
	     rra_def_t *rra_def, 
	     rrd_t *rrd,
	     unsigned long hash,
             const char **require_version) {
    char     *argvcopy;
    char     *tokptr = "";
    unsigned short token_idx, error_flag, period = 0;
    int       cf_id = -1;
    int       token_min = 4;
    const char *parsetime_error = NULL;
    double    tmpdbl;
    
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
                if (*require_version == NULL || atoi(*require_version) < atoi(RRD_VERSION4)) {
		    *require_version = RRD_VERSION4;    /* MHWPREDICT causes Version 4 */
                }
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
	        if (rrd_strtodbl(token, NULL, &tmpdbl, NULL) != 2
	            || tmpdbl < 0.0 || tmpdbl >= 1.0 ){
	            rrd_set_error
			("Invalid xff: must be between 0 and 1");
                }
		rra_def->par[RRA_cdp_xff_val].u_val = tmpdbl;
		break;
	    }
	    break;
	case 2:
	    switch (cf_conv
		    (rra_def->cf_nam)) {
	    case CF_HWPREDICT:
	    case CF_MHWPREDICT:
	        if (rrd_strtodbl(token, NULL, &tmpdbl, NULL) != 2
	            || tmpdbl <= 0.0 || tmpdbl >= 1.0){
		    rrd_set_error
			("Invalid alpha: must be between 0 and 1");
                }	        
		rra_def->par[RRA_hw_alpha].u_val = tmpdbl;
		break;
	    case CF_DEVSEASONAL:
	    case CF_SEASONAL:
	        if (rrd_strtodbl(token, NULL, &tmpdbl, NULL) != 2
	            || tmpdbl <= 0.0 || tmpdbl >= 1.0){
		    rrd_set_error
			("Invalid gamma: must be between 0 and 1");
                }	        
      		rra_def->par[RRA_seasonal_gamma].u_val = tmpdbl;
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
	        if (rrd_strtodbl(token, NULL, &tmpdbl, NULL) != 2
	            || tmpdbl < 0.0 || tmpdbl > 1.0){
		    rrd_set_error
			("Invalid beta: must be between 0 and 1");
                }	        
		rra_def->par[RRA_hw_beta].u_val = tmpdbl;
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
                    if (*require_version == NULL || atoi(*require_version) < atoi(RRD_VERSION4)) {
		        *require_version = RRD_VERSION4;    /* smoothing-window causes Version 4 */
                    }
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
	return rrd_create_r2(filename,pdp_step,last_up,0, NULL, NULL, argc,argv);
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
    const char *template,
    int argc,
    const char **argv)
{
    rrd_t     rrd;
    long      i;
    unsigned long hashed_name;
    int rc = -1;
    struct stat stat_buf;
    GList *sources_rrd_files = NULL;
    mapping_t *mappings = NULL;
    int mappings_cnt = 0;
    const char *require_version = NULL;
    
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

    rrd.live_head->last_up = last_up > 0 ? last_up : time(NULL) - 10;
    int last_up_set = last_up > 0;
    
    time_t    template_latest_last_up = 0;

    if (template) {
        rrd_t trrd;

        rrd_init(&trrd);
        rrd_file_t *tf = rrd_open(template, &trrd, RRD_READONLY | RRD_READAHEAD | RRD_READVALUES);
        if (tf == NULL) {
            rrd_set_error("Cannot open template RRD %s", template);
            goto done;
        }
        
        /* copy step time if not yet set */
        
        if (rrd.stat_head->pdp_step == 0) {
            rrd.stat_head->pdp_step = trrd.stat_head->pdp_step;
        }

        /* copy DS and RRA definitions from template... */

        rrd.stat_head->ds_cnt = trrd.stat_head->ds_cnt;
        rrd.ds_def = malloc(sizeof(ds_def_t) * trrd.stat_head->ds_cnt);
        rrd.stat_head->rra_cnt = trrd.stat_head->rra_cnt;
        rrd.rra_def = malloc(sizeof(rra_def_t) * trrd.stat_head->rra_cnt);
        
        if (rrd.ds_def == NULL || rrd.rra_def == NULL) {
            rrd_set_error("cannot allocate memory");
	    rrd_close(tf);
            goto done;
        }

        template_latest_last_up = trrd.live_head->last_up;

        memcpy(rrd.ds_def,  trrd.ds_def,  sizeof(ds_def_t)  * trrd.stat_head->ds_cnt);
        memcpy(rrd.rra_def, trrd.rra_def, sizeof(rra_def_t) * trrd.stat_head->rra_cnt);

        rrd_close(tf);
    }
    
    if (rrd.stat_head->pdp_step <= 0) {
        rrd.stat_head->pdp_step = 300;
    }

    /* options.optind points to the first non-option command line arg,
     * in this case, the file name. */
    /* Compute the FNV hash value (used by SEASONAL and DEVSEASONAL
     * arrays. */
    hashed_name = FnvHash(filename);
    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "DS:", 3) == 0) {
            size_t    old_size = sizeof(ds_def_t) * (rrd.stat_head->ds_cnt);
            mapping_t m;
            init_mapping(&m);
            
            if ((rrd.ds_def = (ds_def_t*)rrd_realloc(rrd.ds_def,
                                          old_size + sizeof(ds_def_t))) ==
                NULL) {
                rrd_set_error("allocating rrd.ds_def");
		goto done;
            }
            memset(&rrd.ds_def[rrd.stat_head->ds_cnt], 0, sizeof(ds_def_t));

	    parseDS(argv[i] + 3, rrd.ds_def + rrd.stat_head->ds_cnt,
		    &rrd, lookup_DS, &m, &require_version);

            mappings = realloc(mappings, sizeof(mapping_t) * (mappings_cnt + 1));
            if (! mappings) {
                rrd_set_error("allocating mappings");
		goto done;
            }
            memcpy(mappings + mappings_cnt, &m, sizeof(mapping_t));
            mappings_cnt++;
            
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
		     hashed_name, &require_version);

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
    // parsing went well. ONLY THEN are we allowed to produce
    // additional side effects.
    if (require_version != NULL) {
        strncpy(rrd.stat_head->version, require_version, 5);
    }

    if (rrd.stat_head->rra_cnt < 1) {
        rrd_set_error("you must define at least one Round Robin Archive");
	goto done;
    }

    if (rrd.stat_head->ds_cnt < 1) {
        rrd_set_error("you must define at least one Data Source");
	goto done;
    }

    /* set last update time from template if not set explicitly and there 
     * are no sources given */
    if (!last_up_set && template_latest_last_up > 0 && sources == NULL) {
        rrd.live_head->last_up = template_latest_last_up;
    }

    rc = rrd_init_data(&rrd);
    if (rc != 0) goto done;

    rc = -1;    // reset rc to default error state
    
    if (sources != NULL) {
        time_t    sources_latest_last_up = 0;

        for (const char **s = sources ; *s ; s++) {
            rrd_t *srrd = malloc(sizeof(rrd_t));
            if (srrd == NULL) {
                rrd_set_error("cannot allocate memory");
                goto done;
            }
        
            rrd_init(srrd);
            rrd_file_t *sf = rrd_open(*s, srrd, RRD_READONLY | RRD_READAHEAD | RRD_READVALUES);

            if (sf == NULL) {
                rrd_set_error("Cannot open source RRD %s", *s);
                goto done;
            }
            sources_rrd_files = g_list_append(sources_rrd_files, sf);
            if (sources_rrd_files == NULL) {
                rrd_set_error("Cannot keep information about just opened source RRD - likely leaking resources!");
                goto done;
            }
            
            sources_latest_last_up = max(sources_latest_last_up, sf->rrd->live_head->last_up);
        }
    
        if (last_up == -1) {
            rrd.live_head->last_up = sources_latest_last_up;
            last_up_set = 1;
            reset_pdp_prep(&rrd);
        }
        rrd_prefill_data(&rrd, sources_rrd_files, mappings, mappings_cnt);
    }

    rc = write_rrd(filename, &rrd);
    
done:
        g_list_foreach( sources_rrd_files, (GFunc) cleanup_source_file, NULL );
        g_list_free( sources_rrd_files );
            
    if (mappings) {
        int ii;
        for (ii = 0 ; ii < mappings_cnt ; ii++) {
            free_mapping(mappings + ii);
        }
        free(mappings);
    }
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


static void reset_pdp_prep(rrd_t *rrd) {
    unsigned long ds_index;
    
    for (ds_index = 0 ; ds_index < rrd->stat_head->ds_cnt ; ds_index++) {
        strcpy(rrd->pdp_prep[ds_index].last_ds, "U");
        /* interestingly, "U" was associated sometimes with a value of 0.0 
         * and sometimes with a value of DNAN traditionally during create.
         * I do not know why. Ask Tobi. :-)
         * I choose DNAN here, because it makes more sense, IMHO.
         */
        rrd->pdp_prep[ds_index].scratch[PDP_val].u_val = DNAN;
        rrd->pdp_prep[ds_index].scratch[PDP_unkn_sec_cnt].u_cnt =
            rrd->live_head->last_up % rrd->stat_head->pdp_step;
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
        reset_pdp_prep(rrd);
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
    mode_t saved_umask;

    /* write out the new file */
#ifdef HAVE_LIBRADOS
    if (strncmp("ceph//", outfilename, 6) == 0) {
      rc = rrd_rados_create(outfilename + 6, out);
      goto done;
    }
#endif

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
	
	/* fix CWE-377 */
	saved_umask = umask(S_IRUSR|S_IWUSR);
	int tmpfd = mkstemp(tmpfilename);
	umask(saved_umask);
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
	    
#ifdef WIN32
/* in windows, renaming to an existing file is verboten */
            unlink(outfilename);
#endif 
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

#define FWRITE_CHECK(ptr, size, nitems, fp)                    \
    do {                                                       \
        if (fwrite((ptr), (size), (nitems), (fp)) != (nitems)) \
            return (-1);                                       \
    } while (0)

    FWRITE_CHECK(rrd->stat_head, sizeof(stat_head_t), 1, fh);
    FWRITE_CHECK(rrd->ds_def, sizeof(ds_def_t), rrd->stat_head->ds_cnt, fh);
    FWRITE_CHECK(rrd->rra_def, sizeof(rra_def_t), rrd->stat_head->rra_cnt, fh);
    FWRITE_CHECK(rrd->live_head, sizeof(live_head_t), 1, fh);
    FWRITE_CHECK(rrd->pdp_prep, sizeof(pdp_prep_t), rrd->stat_head->ds_cnt, fh);
    FWRITE_CHECK(rrd->cdp_prep, sizeof(cdp_prep_t),
           rrd->stat_head->rra_cnt * rrd->stat_head->ds_cnt, fh);
    FWRITE_CHECK(rrd->rra_ptr, sizeof(rra_ptr_t), rrd->stat_head->rra_cnt, fh);

    /* calculate the number of rrd_values to dump */
    rra_offset = 0;
    for (i = 0; i < rrd->stat_head->rra_cnt; i++) {
        unsigned long num_rows = rrd->rra_def[i].row_cnt;
        unsigned long ds_cnt = rrd->stat_head->ds_cnt;
        if (num_rows > 0){
            FWRITE_CHECK(rrd->rrd_value + rra_offset * ds_cnt,
                    sizeof(rrd_value_t), 
                    num_rows * ds_cnt, fh);
                    
            rra_offset += num_rows;
        }
    }

    if (fflush(fh) != 0)
        return (-1);

    return (0);

#undef FWRITE_CHECK
}                       /* int write_file */

static long overlap(time_t start1, time_t end1, time_t start2, time_t end2) {
    if (start1 >= end1) return 0;
    if (start2 >= end2) return 0;

    if (start1 > end2) return 0;
    if (start2 > end1) return 0;

    return min(end1, end2) - max(start1, start2);
}

static long is_interval_within_interval(time_t start1, time_t end1,
        time_t start2, time_t end2) {
    if (end1 - start1 > end2 - start2) return 0;
    
    long o = overlap(start1, end1, start2, end2);
    return o == end1-start1;
}

static int is_time_within_interval(time_t t, time_t start, time_t end) {
    return (t >= start && t <= end);
}

typedef struct {
    int covered;        /* != 0 -> covered, == 0 -> not covered */
    /* start and end are inclusive times */
    time_t start;
    time_t end;
} coverage_t;

static inline void set_interval(coverage_t *c, int covered, time_t start, time_t end) {
    c->covered = covered;
    c->start = start;
    c->end = end;
}

#ifdef DEBUG_PREFILL
static void dump_coverage_array(const coverage_t *current_coverage, const int *coverage_array_size) {
    for (int i =  0 ; i < *coverage_array_size ; i++) {
        fprintf(stderr, "%d covered=%d start=%ld end=%ld\n", i, current_coverage[i].covered,
                current_coverage[i].start, current_coverage[i].end);
    }
}
#endif

static coverage_t *add_coverage(coverage_t *current_coverage, int *coverage_array_size,
        time_t start, time_t end,
        int *newly_covered_interval) 
{
    int i;
    
    if (coverage_array_size == NULL) return NULL;
    if (current_coverage    == NULL) return NULL;

    /*
     * Never extend beyond the ends of current coverage information. We do 
     * this by forcibly trimming the newly added interval to the start of 
     * the first and the end of the last interval.
     */
    if (start < current_coverage->start) {
        start = current_coverage->start;
    }
    if (end > (current_coverage + (*coverage_array_size - 1))->end) {
        end = (current_coverage + (*coverage_array_size - 1))->end;
    }
    
    *newly_covered_interval = 0;

    /* out of boundaries check */
    if (end < current_coverage->start || 
        start > (current_coverage + (*coverage_array_size - 1))->end) {
        return current_coverage;
    }
    
    for (i = 0 ; i < *coverage_array_size ; i++) {
        coverage_t *cc = current_coverage + i;
        coverage_t *next;
        
        time_t org_start = cc->start;
        time_t org_end = cc->end;

        if (is_interval_within_interval(start, end, org_start, org_end)) {
            /*
             * Case (A): newly added interval is fully contained within the current one.
             */

            if (cc->covered) {
                // no new data .. already fully covered, just return
                break;
            } 
            /* NOT covered by the interval, but new interval is fully contained within the current one */
            
            /* special case: is the newly covered interval EXACTLY the same as the current? 
             * If yes: just turn the current interval into a covered one.
             * Also make sure to only report a newly covered interval if it wasn't covered before 
             * (NOTE: this is actually redundant, as we reach this point only for "uncovered" intervals).
             * Any required collapsing of intervals will be done during the cleanup pass.
             */
            
            if (start == org_start && end == org_end) {
                *newly_covered_interval += cc->covered ? 0 : end - start + 1;
                cc->covered = 1;
                break;
            }

            // split interval...
            if (org_start == start) {
                // insert before current interval

                current_coverage = realloc(current_coverage, sizeof(coverage_t) * (*coverage_array_size + 1));
                cc = current_coverage + i;

                memmove(cc + 1, 
                        cc, (*coverage_array_size - i) * sizeof(coverage_t));
        
                set_interval(cc    , 1, org_start, end    );
                set_interval(cc + 1, 0, end + 1  , org_end);

                (*coverage_array_size)++;
                *newly_covered_interval += end - start + 1;
                break;
            }
            if (org_end == end) {
                // insert after current interval

                current_coverage = realloc(current_coverage, sizeof(coverage_t) * (*coverage_array_size + 1));
                cc = current_coverage + i;

                memmove(cc + 1, 
                        cc, (*coverage_array_size - i) * sizeof(coverage_t));
                
                set_interval(cc    , 0, org_start, start - 1);
                set_interval(cc + 1, 1, start    , org_end  );
                
                (*coverage_array_size)++;
                *newly_covered_interval += end - start + 1;
                break;
            }

            // split into three intervals: uncovered/covered/uncovered .. add TWO new array elements
            
            current_coverage = realloc(current_coverage, sizeof(coverage_t) * (*coverage_array_size + 2));
            cc = current_coverage + i;

            memmove(cc + 2, 
                    cc, (*coverage_array_size - i) * sizeof(coverage_t));

            set_interval(cc    , 0, org_start, start - 1);
            set_interval(cc + 1, 1, start    , end      );
            set_interval(cc + 2, 0, end + 1  , org_end  );

            (*coverage_array_size) += 2;
            *newly_covered_interval += end - start + 1;
            break;
        }

        /* 
         * Case (B);
         * 
         * does the new interval fully cover the current interval?
         * This might happen more than once!
         * 
         * Note that if this case happens, case (A) above will NEVER happen...
         */ 
        if (is_interval_within_interval(org_start, org_end, start, end)) {
            if (! cc->covered) {
                /* just turn the current interval into a covered one. Report 
                 * the range as newly covered */
                cc->covered = 1;
                *newly_covered_interval += cc->end - cc->start + 1;
            }
        }
        
        /*
         * Case (C): The newly added interval starts within the current one, but 
         * it does not end within.
         * 
         * We handle this by handling the implications for the current interval and then
         * adjusting the new interval start period for the next iteration. That way, we will 
         * finally hit cases (A) or (B) and we will never see a situation where the 
         * new interval ends within the current on but does not start within 
         * (which would have become case (D)).
         */
        
        if (is_time_within_interval(start, org_start, org_end)) {
           /* If the current interval is a covered one, we do nothing but 
            * to adjust the start interval for the next iteration.
            */
            if (cc->covered) {
                start = org_end + 1;
                continue;
            }
            
            /* if the current interval is not covered... */
            
            if (cc->start == start) {
                /* ... and the new interval starts with the current one, we just turn it into a 
                 * covered one and adjust the start... */
                cc->covered = 1;
                start = org_end + 1;
            } else {
                /* ... and the new interval does NOT start with the current one, we have to split the interval .. */
                
                current_coverage = realloc(current_coverage, sizeof(coverage_t) * (*coverage_array_size + 1));
                cc = current_coverage + i;
                memmove(cc + 1, cc, sizeof(coverage_t) * ((*coverage_array_size) - i));
                (*coverage_array_size)++;
                
                next = cc + 1;
                set_interval(cc    , 0, org_start, start - 1);
                set_interval(cc + 1, 1, start    , org_end  );
                
                *newly_covered_interval += next->end - next->start + 1;
                start = org_end + 1;
            }
        }
    }
    
    /* cleanup pass: collapse same type-intervals bordering each other.... */
    i = 0;
    while (i < *coverage_array_size - 1) {
        coverage_t *cc = current_coverage + i;
        coverage_t *next = cc + 1;
        
        if (cc->covered == next->covered) {
            cc->end = next->end;
            
            memmove(next, next + 1, sizeof(coverage_t) * (*coverage_array_size - i - 2));
            (*coverage_array_size)--;

            // re-iterate with i unchanged !!
            continue;
        }
        
        i++;
    }
    
    return current_coverage;
}

#if 0
static long total_coverage(const coverage_t *coverage, const int *array_size) {
    long total = 0;
    for (int i = 0 ; i < *array_size ; i++) {
        if (coverage[i].covered) total += coverage[i].end - coverage[i].start + 1;
    }
    return total;
}
#endif

static rrd_value_t prefill_consolidate(rra_def_t UNUSED(*rra_def), enum cf_en current_cf, 
        rrd_value_t current_estimate, 
        rrd_value_t added_value, 
        int target_bin_size, int newly_covered_size) {
    switch (current_cf) {
    case CF_MINIMUM:
        if (isnan(current_estimate)) return added_value;
        return current_estimate < added_value ? current_estimate : added_value;
    case CF_MAXIMUM:
        if (isnan(current_estimate)) return added_value;
        return current_estimate > added_value ? current_estimate : added_value;
    case CF_LAST:
        if (isnan(current_estimate)) return added_value;
       /* FIXME: There are better ways to do this, but it requires more information 
         * from the caller.
         */
        return added_value;
    case CF_AVERAGE:
    default:
        if (isnan(current_estimate)) current_estimate = 0.0;

        return current_estimate + added_value / target_bin_size * newly_covered_size;
    }
}

static rrd_value_t prefill_finish(rra_def_t UNUSED(*rra_def), enum cf_en current_cf,
                                  rrd_value_t current_estimate, 
                                  int target_bin_size, int total_covered_size) {
    if (total_covered_size == 0) return DNAN;
    
    switch (current_cf) {
    case CF_AVERAGE:
        return current_estimate / total_covered_size * target_bin_size;
    case CF_MINIMUM:
    case CF_MAXIMUM:
    case CF_LAST:
    default:
        return current_estimate;
    }
}

static int order_candidates(candidate_t *a, candidate_t *b, const candidate_t UNUSED(*target)) {
    enum cf_en acf = cf_conv(a->rra->cf_nam);
    enum cf_en bcf = cf_conv(b->rra->cf_nam);

    enum cf_en tcf = cf_conv(target->rra->cf_nam);
    
    int astep = a->rrd->stat_head->pdp_step;
    int bstep = b->rrd->stat_head->pdp_step;
    int tstep = target->rrd->stat_head->pdp_step;
    
    /* an exact match ALWAYS goes first*/
    if (acf == tcf && a->rra->pdp_cnt * astep == target->rra->pdp_cnt * tstep) {
        return -1;
    }
    
    if (bcf == tcf && b->rra->pdp_cnt * bstep == target->rra->pdp_cnt * tstep) {
        return 1;
    }
    
    
    if (acf != bcf) {
        /* different RRA CF functions: AVERAGE CF takes precedence, this is 
         * more correct mathematically.
         */
        if (acf == /*targetcf*/CF_AVERAGE) return -1;
        if (bcf == /*targetcf*/CF_AVERAGE) return 1;
        // problem: this should not really be possible 
        return 0;
    }
    
    int d = a->rra->pdp_cnt * astep - b->rra->pdp_cnt * bstep;
    if (d != 0) return d;
    
    d = a->rra->row_cnt - b->rra->row_cnt;
    return -d;          // higher row counts sort earlier
}

/* select AVERAGE and same CF RRAs. */
static int select_create_candidates(const rra_def_t *tofill, const rra_def_t *maybe) {
    enum cf_en cf = cf_conv(maybe->cf_nam);
    if (cf == CF_AVERAGE) return 1;
    if (cf == cf_conv(tofill->cf_nam)) return 1;
    return 0;
}

static void prefill_bin(candidate_t *target, int cnt,
                        const candidate_t *candidates, int candidate_cnt) {
    unsigned long k;
    
    long bin_size = target->rra->pdp_cnt * target->rrd->stat_head->pdp_step;
    long min_required_coverage = target->rra->par[RRA_cdp_xff_val].u_val * bin_size;

    time_t bin_end_time = end_time_for_row_simple(target->rrd, target->rra_index, cnt);
    time_t bin_start_time = bin_end_time - bin_size + 1;

    /* find corresponding range of bins in all candidates... */

    coverage_t *coverage = malloc(sizeof(coverage_t));
    int coverage_size = 1;
    if (coverage == NULL) {
        rrd_set_error("Cannot allocate memory");
        goto done;
    }
    set_interval(coverage, 0, bin_start_time, bin_end_time);

    long total_covered = 0, covering_bins = 0;
    rrd_value_t value = DNAN; // init_prefill_consolidate(rra_def, current_cf);

    for (k = 0 ; k < (unsigned long) candidate_cnt && total_covered < bin_size ; k++) {
        const candidate_t *candidate = candidates + k;

        rra_def_t * candidate_rra_def = candidate->rra;

        //candidates[k].
        unsigned long end_bin = row_for_time(candidate->rrd, candidate->rra, 
                candidate->rrd->rra_ptr[candidate->rra_index].cur_row,
                bin_end_time);
        unsigned long start_bin = row_for_time(candidate->rrd, candidate->rra, 
                candidate->rrd->rra_ptr[candidate->rra_index].cur_row,
                bin_start_time);

        if (start_bin < candidate_rra_def->row_cnt 
                && end_bin < candidate_rra_def->row_cnt) {
            int bin_count = positive_mod(end_bin - start_bin + 1, candidate_rra_def->row_cnt);

            for (unsigned int ci = start_bin ; bin_count > 0 && total_covered < bin_size ; ci++, bin_count-- ) {
                if (ci == candidate->rra->row_cnt) ci = 0;

                // find overlap range....
                long cand_bin_size = candidate_rra_def->pdp_cnt * candidate->rrd->stat_head->pdp_step;
                time_t cand_bin_end_time = end_time_for_row_simple(candidate->rrd, candidate->rra_index, ci);
                time_t cand_bin_start_time = cand_bin_end_time - cand_bin_size + 1;

                long covered = overlap(bin_start_time, bin_end_time,
                                       cand_bin_start_time, cand_bin_end_time) +1;
                rrd_value_t v = candidate->values[ci * candidate->rrd->stat_head->ds_cnt + candidate->extra.l];
                if (covered > 0 && ! isnan(v)) {
                    int newly_covered = 0;
                    coverage = add_coverage(coverage, &coverage_size, cand_bin_start_time, cand_bin_end_time, &newly_covered);
                    if (coverage == NULL) {
                        rrd_set_error("Memory allocation failed");
                        goto done;
                    }

                    if (newly_covered > 0) {
                        total_covered += newly_covered;
                        covering_bins++;

                        value = prefill_consolidate(target->rra, target->rra_cf,
                                                    value, v, 
                                                    bin_size, newly_covered);


                    }
                }
            }
        }

    }
    //row_for_time();

    if (total_covered > min_required_coverage) {
        value = prefill_finish(target->rra, target->rra_cf, value, bin_size, total_covered);
        *(target->values + target->rrd->stat_head->ds_cnt * cnt + target->extra.l) = value;
    }
done:
    if (coverage) free(coverage);
}


/* 
 * prefill last value for the target RRA if we haven't done so yet (That is, 
 * if the last value is still unknown). We can only *really* do this, if the 
 * last update of the target RRD is compatible with one of the source RRDs,
 * because there is generally no way to deduce the input data from an RRA bin.
 */
static void prefill_pdp_prep(candidate_t *target, const candidate_t *candidates, int candidate_cnt) {
    rrd_t *rrd = target->rrd;
    int ds_index = target->extra.l;
    
    if (strncmp(rrd->pdp_prep[ds_index].last_ds, "U", LAST_DS_LEN) != 0) {
        /* PDP prep for this DS already set. */
        return;
    }
    
    time_t current_pdp_begin_time = 
            (rrd->live_head->last_up / rrd->stat_head->pdp_step) *
             rrd->stat_head->pdp_step + 1;
    time_t current_pdp_end_time = current_pdp_begin_time + rrd->stat_head->pdp_step - 1;

    /* walk all source RRAs to find a matching last update value. 
     * We use the candidate RRA list for this, to make sure that we
     * won't erroneously use an RRD we have no candidate for... */

    const rrd_t *srrd = NULL;
    for (int c = 0 ; c < candidate_cnt ; c++) {
        const candidate_t *candidate = candidates + c;
        /* only look at each source RRD once. We use the fact that all 
         * candidates are block-sorted by the source RRDs */
        if (srrd == candidate->rrd) continue;
        srrd = candidate->rrd;

        if (strncmp(srrd->pdp_prep[candidate->extra.l].last_ds, "U", LAST_DS_LEN) == 0) {
            /* this source RRD does not have a usable last DS value - skip */
            continue;
        }

        time_t source_pdp_begin_time = 
            (srrd->live_head->last_up / srrd->stat_head->pdp_step) *
            srrd->stat_head->pdp_step + 1;
        time_t source_pdp_end_time = source_pdp_begin_time + srrd->stat_head->pdp_step - 1;

        if (source_pdp_end_time <= current_pdp_end_time && source_pdp_end_time >= current_pdp_begin_time) {
            /* only really copy the data if the DS Type matches... */
            if (strcmp(rrd->ds_def[ds_index].dst, srrd->ds_def[candidate->extra.l].dst) == 0) {
                memcpy(rrd->pdp_prep + ds_index, srrd->pdp_prep + candidate->extra.l,
                        sizeof(pdp_prep_t));

                /* in case the step sizes of target and source do not match, 
                 * we take an additional look at the unknown seconds, because that
                 * value should not be larger than the step size.
                 * 
                 * We also assume, that it is unusual to explicitly add unknown data to an
                 * RRD, so we use the current value in any case, and assume the unknown data to
                 * be at the beginning of the current pdp_prep time range.
                 * 
                 * I really hope that I got the semantics right here - PSt.
                 */

                if (rrd->pdp_prep[ds_index].scratch[PDP_unkn_sec_cnt].u_val > rrd->stat_head->pdp_step) {
                    long source_known = srrd->stat_head->pdp_step - rrd->pdp_prep[ds_index].scratch[PDP_unkn_sec_cnt].u_val;
                    long max_target_known = rrd->live_head->last_up - current_pdp_begin_time;

                    rrd->pdp_prep[ds_index].scratch[PDP_unkn_sec_cnt].u_cnt = max(0, max_target_known - source_known);
                }

                /* we now have set a known last DS value in the pdp_prep, break 
                 * out of the loop, because we are done */
                break;
            }
        }
    }
}

static void get_cdp_start_end(const rrd_t *rrd, const rra_def_t *rra, time_t *begin, time_t *end) {
    int cdp_step = rra->pdp_cnt * rrd->stat_head->pdp_step;
    *begin = (rrd->live_head->last_up / cdp_step) * cdp_step + 1;
    *end = *begin + cdp_step - 1; 
}

static void prefill_cdp_prep(candidate_t *target, 
                             candidate_t *candidates, int candidate_cnt, 
                             long cdp_rra_index)
{
    rrd_t *rrd = target->rrd;
    
    time_t current_cdp_begin_time, current_cdp_end_time;
    get_cdp_start_end(rrd, target->rra, &current_cdp_begin_time, &current_cdp_end_time);
    
    const rrd_t *srrd = NULL;
    
    /* the list of candidates only contains RRAs with the same PDP count, we 
     * still have to check if the step times are the same and if we 
     * have the time covered by the CDPs...*/
    for (int c = 0 ; c < candidate_cnt ; c++) {
        const candidate_t *candidate = candidates + c;
        srrd = candidate->rrd;

        if (rrd->stat_head->pdp_step != srrd->stat_head->pdp_step) continue;

        time_t source_cdp_begin_time, source_cdp_end_time;
        get_cdp_start_end(srrd, candidate->rra, &source_cdp_begin_time, &source_cdp_end_time);

        if (source_cdp_begin_time == current_cdp_begin_time 
                && source_cdp_end_time == current_cdp_end_time) {
            /* CDPs are fully compatible, but still: do the last update time match the very same bin?
             * Only then are we able to just copy over data */
            
            if (rrd->live_head->last_up / rrd->stat_head->pdp_step == srrd->live_head->last_up / srrd->stat_head->pdp_step) {
                // OK, fully compatible...
                memcpy(target->cdp + target->extra.l, 
                        candidate->cdp + candidate->extra.l, sizeof(cdp_prep_t));
                return;
            }
        }
    }
    
    /* we are still here: we have not found a compatible CDP, use the "CDP RRA"... */
    /* void init_cdp(const rrd_t *rrd, const rra_def_t *rra_def, 
     *               const pdp_prep_t *pdp_prep, cdp_prep_t *cdp_prep) */

    // must find first value index first....
    
    int ii, first_cdp_rra_value = 0;
    for (ii = 0 ; ii < cdp_rra_index ; ii++) {
        first_cdp_rra_value += rrd->rra_def[ii].row_cnt;
    }

    init_cdp(rrd, target->rra, rrd->pdp_prep + target->extra.l, 
             target->cdp + target->extra.l);
    // explicitly set unknown count to 0...
    
    (target->cdp + target->extra.l)->scratch[CDP_unkn_pdp_cnt].u_cnt = 0;

    enum cf_en tcf = cf_conv(target->rra->cf_nam);
    time_t t;

    rra_def_t *cdp_rra = rrd->rra_def + cdp_rra_index;
    
    if (target->rra->pdp_cnt == 1) {
        /* special case: for pdp_cnt == 1 RRAs, we may just use the last average 
         * value for the CDP primary value */
        target->cdp[target->extra.l].scratch[CDP_unkn_pdp_cnt].u_cnt =  0;
        target->cdp[target->extra.l].scratch[CDP_primary_val].u_val =  0;
        target->cdp[target->extra.l].scratch[CDP_secondary_val].u_val =  0;

        int r = row_for_time(rrd, cdp_rra, 
                rrd->rra_ptr[cdp_rra_index].cur_row,
                current_cdp_begin_time - target->rrd->stat_head->pdp_step);
        
        if (r >= 0) {  
            rrd_value_t v =  rrd->rrd_value[(first_cdp_rra_value + r) * rrd->stat_head->ds_cnt 
                    + target->extra.l];
            target->cdp[target->extra.l].scratch[CDP_primary_val].u_val =  v;
        }
        
        return;
    }
    
    int cdp_step = rrd->stat_head->pdp_step * target->rra->pdp_cnt;

    for (t = current_cdp_begin_time - target->rra->pdp_cnt * rrd->stat_head->pdp_step ; 
            t < current_cdp_end_time && t < rrd->live_head->last_up ; 
            t += rrd->stat_head->pdp_step) {
        
        int r = row_for_time(rrd, cdp_rra, 
                rrd->rra_ptr[cdp_rra_index].cur_row,
                t);

        if (r < 0) continue;
        rrd_value_t v =  rrd->rrd_value[(first_cdp_rra_value + r) * rrd->stat_head->ds_cnt 
                + target->extra.l];
        int n = target->rra->pdp_cnt - (t % cdp_step) / rrd->stat_head->pdp_step;

        update_cdp(target->cdp[target->extra.l].scratch,  
                   tcf, 
                   v,       // pdp_temp_val
                   n == 1,       // rra_step_cnt
                   1,       // elapsed_pdp_st
                   n,       // start_pdp_offset
                   target->rra->pdp_cnt,       // pdp_cnt
                   target->rra->par[RRA_cdp_xff_val].u_val,     // xff
                   0, 0);
    }
}

static unsigned long find_ds_match(const ds_def_t *ds_def, const rrd_t *src_rrd,
                                    mapping_t *mapping) {
    unsigned long source_ds_index;
    const char *looked_for_ds_name = ds_def->ds_nam;
    if (mapping && mapping->mapped_name && strlen(mapping->mapped_name) > 0) {
        looked_for_ds_name = mapping->mapped_name;
    }
    
    for (source_ds_index = 0 ; source_ds_index < src_rrd->stat_head->ds_cnt ; source_ds_index++) {
        if (strcmp(looked_for_ds_name, src_rrd->ds_def[source_ds_index].ds_nam) == 0) {
            return source_ds_index;
        }
    }
    return src_rrd->stat_head->ds_cnt;
}

static int find_mapping(const char *ds_nam, const mapping_t *mappings, int mappings_cnt) {
    int i;
    for (i = 0 ; i < mappings_cnt ; i++) {
        if (strcmp(ds_nam, mappings[i].ds_nam) == 0) return i;
    }
    return -1;
}

/* Find a set of RRAs among all RRA in the sources list, as matched by the select_func and 
 * ordered (by source) according to order_func.
 */

static candidate_t *find_matching_candidates(const candidate_t *target, 
        const GList *sources, int *candidate_cnt,
        mapping_t *mappings, int mappings_cnt,
        candidate_selectfunc_t *select_func, 
        compar_ex_t *order_func) {
    if (select_func == NULL) return NULL;
    
    ds_def_t *ds_def = target->rrd->ds_def + target->extra.l;

    const GList *src;
    candidate_t *candidates = NULL;

    int mindex = find_mapping(ds_def->ds_nam, mappings, mappings_cnt);
    mapping_t *mapping = mindex < 0 ? NULL : mappings + mindex;

    int cnt = 0, srcindex;
    for (src = sources, srcindex = 1 ; src ;  src = g_list_next(src), srcindex++) {
        // first: check source index if we have a mapping containing an index...
        // NOTE: the index is 1-based....
        if (mapping && mapping->index >= 0 && srcindex != mapping->index) {
            continue;
        }
        // then: find matching DS

        const rrd_file_t *rrd_file = src->data;
        if (rrd_file == NULL) continue;

        const rrd_t *src_rrd = rrd_file->rrd;
        if (src_rrd == NULL) continue; 

        unsigned long source_ds_index = find_ds_match(ds_def, src_rrd, mapping);
        if (source_ds_index < src_rrd->stat_head->ds_cnt) {
            // match found

            candidate_extra_t extra = { .l = source_ds_index };
            // candidates = g_list_append(candidates, (gpointer) src);
            int candidate_cnt_for_source = 0;
            candidate_t *candidates_for_source = 
                    find_candidate_rras(src_rrd, target->rra, &candidate_cnt_for_source, 
                                        extra,
                                        select_func);

            if (candidates_for_source && candidate_cnt_for_source > 0) {
                if (order_func != NULL) {
                    quick_sort(candidates_for_source, sizeof(candidate_t),
                            candidate_cnt_for_source, order_func, (void*) target);
                }

                candidates = realloc(candidates, 
                                     sizeof(candidate_t) * (cnt + candidate_cnt_for_source));
                if (candidates == NULL) {
                    rrd_set_error("Cannot realloc memory");
                    free(candidates_for_source);
                    goto done;
                }
                memcpy(candidates + cnt, 
                       candidates_for_source,
                       sizeof(candidate_t) * candidate_cnt_for_source);

                cnt += candidate_cnt_for_source;
            }
            if (candidates_for_source) {
                free(candidates_for_source);
            }
        }
    }
done:
    *candidate_cnt = cnt;
    return candidates;
}

/* For CDP pre-filling we want to have an AVERAGE RRA with a resolution of 1 
 * PDP with a minimum length of the largest pdp count of all other RRAs...
 * 
 * If we do not already have something compatible, we add a temporary RRA that 
 * can/should be removed again after CDP prefilling
 * 
 * Returns the index of a suitable RRA in the target RRD. If that RRA was 
 * added just for this purpose, the *added flag will be true and false otherwise.
 */

static long rra_for_cdp_prefilling(rrd_t *rrd, int *added) 
{
    // find largest pdp count
    unsigned long largest_pdp = 0;
    
    rra_def_t *average_1_pdp_rra = NULL;
    long found_rra_index = -1;
    
    unsigned long original_total_rows = 0;
    unsigned long rra_index;
    
    for (rra_index = 0 ; rra_index < rrd->stat_head->rra_cnt ; rra_index++) {
        rra_def_t *rra = rrd->rra_def + rra_index;
        original_total_rows += rra->row_cnt;
        
        largest_pdp = max(largest_pdp, rra->pdp_cnt);
        if (rra->pdp_cnt == 1 && cf_conv(rra->cf_nam) == CF_AVERAGE) {
            if (average_1_pdp_rra == NULL) {
                average_1_pdp_rra = rra;
                found_rra_index = rra_index;
            } else if (average_1_pdp_rra->row_cnt < rra->row_cnt) { 
                average_1_pdp_rra = rra;
                found_rra_index = rra_index;
            }
        }
    }
    largest_pdp *= 2;
    *added = 0;
    if (average_1_pdp_rra == NULL || average_1_pdp_rra->row_cnt < largest_pdp) {
        // add our own temporary average RRA with a PDP count == 1

        /* temporarily extend */
        rrd->stat_head->rra_cnt++;

        found_rra_index = rrd->stat_head->rra_cnt - 1;
        *added = 1;
        
        rrd->rra_def = realloc(rrd->rra_def, sizeof(rra_def_t) * rrd->stat_head->rra_cnt);
        rrd->rra_ptr = realloc(rrd->rra_ptr, sizeof(rra_ptr_t) * rrd->stat_head->rra_cnt);
        rrd->cdp_prep = realloc(rrd->cdp_prep, sizeof(cdp_prep_t) * rrd->stat_head->rra_cnt * rrd->stat_head->ds_cnt);
        rrd->rrd_value = realloc(rrd->rrd_value, sizeof(rrd_value_t) * (original_total_rows + largest_pdp) * rrd->stat_head->ds_cnt);
        
        if (rrd->rra_def == NULL 
                || rrd->rra_ptr == NULL 
                || rrd->cdp_prep == NULL 
                || rrd->rrd_value == NULL) {
            rrd_set_error("Memory allocation failed");
            return -1;
        }
        
        strcpy(rrd->rra_def[found_rra_index].cf_nam, "AVERAGE");
        rrd->rra_def[found_rra_index].pdp_cnt = 1;
        rrd->rra_def[found_rra_index].row_cnt = largest_pdp;
        rrd->rra_def[found_rra_index].par[RRA_cdp_xff_val].u_val = 0.5;

        rrd->rra_ptr[found_rra_index].cur_row = 0;        // doesn't matter
        
        unsigned long i, ii;
        for (ii = 0 ; ii < rrd->stat_head->ds_cnt ; ii++) {
            init_cdp(rrd, &(rrd->rra_def[found_rra_index]), 
                    rrd->pdp_prep + found_rra_index,
                    rrd->cdp_prep + rrd->stat_head->ds_cnt * found_rra_index + ii);
        }
        
        for (ii = 0 ; ii < largest_pdp ; ii++) {
            for (i = 0 ; i < rrd->stat_head->ds_cnt ; i++) {
                rrd->rrd_value[rrd->stat_head->ds_cnt * (original_total_rows + ii) + i] = DNAN;
            }
        }
    }
    return found_rra_index;
}

static void remove_temporary_rra_for_cdp_prefilling(rrd_t *rrd, long added_index) {
    if (added_index < 0 || rrd == NULL) return;

    /* if we have added a temporary RRA for CDP preparation, we now have to 
     * shorten the various data elements again... */
    
    unsigned long rra_index = 0;
    long total_rows = 0;
    for (rra_index = 0 ; rra_index < rrd->stat_head->rra_cnt ; rra_index++) {
        if ((long)rra_index != added_index) {
            total_rows += rrd->rra_def[rra_index].row_cnt;
        }
    }

    rrd->stat_head->rra_cnt--;

    rrd->rra_def = realloc(rrd->rra_def, sizeof(rra_def_t) * rrd->stat_head->rra_cnt);
    rrd->rra_ptr = realloc(rrd->rra_ptr, sizeof(rra_ptr_t) * rrd->stat_head->rra_cnt);
    rrd->cdp_prep = realloc(rrd->cdp_prep, sizeof(cdp_prep_t) * rrd->stat_head->rra_cnt * rrd->stat_head->ds_cnt);
    rrd->rrd_value = realloc(rrd->rrd_value, 
                             sizeof(rrd_value_t) * (total_rows * rrd->stat_head->ds_cnt));
}

static int cdp_match(const rra_def_t *tofill, const rra_def_t *maybe) {
    enum cf_en mcf = cf_conv(maybe->cf_nam);
    if (cf_conv(tofill->cf_nam) == mcf &&
        tofill->pdp_cnt == maybe->pdp_cnt) {
        return 1;
    }
    return 0;
}

static int rrd_prefill_data(rrd_t *rrd, const GList *sources, mapping_t *mappings, int mappings_cnt) {
    int rc = -1;
    long cdp_rra_index = -1;
    int rra_added_temporarily = 0;
     
    if (sources == NULL) {
        // we are done if there is nothing to copy data from
        rc = 0;
        goto done;
    }

    unsigned long rra_index, ds_index;
    unsigned long total_rows = 0;
    
    cdp_rra_index = rra_for_cdp_prefilling(rrd, &rra_added_temporarily);

    if (rrd_test_error()) goto done;

    // process one RRA after the other
    for (rra_index = 0 ; rra_index < rrd->stat_head->rra_cnt ; rra_index++) {
        rra_def_t *rra_def = rrd->rra_def + rra_index;
        
        /*
         * Re-use candidate_t as a container for all information, because the data structure contains
         * everything we need further on.
         * 
         */
        
        candidate_t target = {
            .rrd = rrd,
            .rra = rra_def,
            .rra_cf = cf_conv(rra_def->cf_nam),
            .rra_index = rra_index,
            .values = rrd->rrd_value + rrd->stat_head->ds_cnt * total_rows,
            .cdp = rrd->cdp_prep + rrd->stat_head->ds_cnt * rra_index,
        };
        

        for (ds_index = 0 ; ds_index < rrd->stat_head->ds_cnt ; ds_index++) {
            target.extra.l = ds_index;
            /* for each DS in each RRA within rrd find a list of candidate DS/RRAs from 
             * the sources list that match by name... */
    
            
            int candidate_cnt = 0;
            candidate_t *candidates = find_matching_candidates(&target, sources, &candidate_cnt, 
                    mappings, mappings_cnt,
                    (candidate_selectfunc_t*) select_create_candidates, 
                    (compar_ex_t*) order_candidates);

            
#ifdef DEBUG_PREFILL
            // report candidates
            fprintf(stderr, "ds=%s candidates for %s %d rows=%d\n", 
                    rrd->ds_def[ds_index].ds_nam,
                    rra_def->cf_nam, rra_def->pdp_cnt, 
                            rra_def->row_cnt);
            for (int rr = 0 ; rr < candidate_cnt ; rr++) {
                candidate_t *cd = candidates + rr;
                fprintf(stderr, " candidate %s %d rows=%d\n", cd->rra->cf_nam, cd->rra->pdp_cnt, cd->rra->row_cnt);
                
            }
#endif            
            /* if we have no candidates for an RRA we just skip to the next... */
            if (candidates == NULL) {
                if (rrd_test_error()) goto done;
                continue;
            }
            
            /* walk all RRA bins and fill the current DS with data from 
             * the list of candidates */

            unsigned long cnt = 0;
            
            for (cnt = 0 ; cnt < rra_def->row_cnt ; cnt++) {
                prefill_bin(&target, cnt, candidates, candidate_cnt);
            }

            prefill_pdp_prep(&target, candidates, candidate_cnt);
            
            free(candidates);
        }
        total_rows += rra_def->row_cnt;
    }
    
    /* now we walk all RRAs and DSs again to handle CDP prefilling. Do this AFTER 
     * taking care of RRA data, because we might have added the average-1-pdp RRA
     * to obtain input data for CDP consolidation. And THAT RRA will only be available
     * AFTER all bin prefilling done in the previous pass */
    
    total_rows = 0;
    for (rra_index = 0 ; rra_index < rrd->stat_head->rra_cnt ; rra_index++) {
        candidate_t target = {
            .rrd = rrd,
            .rra = rrd->rra_def + rra_index,
            .rra_cf = cf_conv(rrd->rra_def[rra_index].cf_nam),
            .rra_index = rra_index,
            .values = rrd->rrd_value + rrd->stat_head->ds_cnt * total_rows,
            .cdp = rrd->cdp_prep + rrd->stat_head->ds_cnt * rra_index,
        };

        for (ds_index = 0 ; ds_index < rrd->stat_head->ds_cnt ; ds_index++) {
            target.extra.l = ds_index;

            int candidate_cnt = 0;
            candidate_t *candidates = find_matching_candidates(&target, sources, 
                    &candidate_cnt, 
                    mappings, mappings_cnt,
                    cdp_match, NULL);

            if (candidates == NULL && rrd_test_error()) {
                goto done;
            }
            prefill_cdp_prep(&target, candidates, candidate_cnt, cdp_rra_index);
            free(candidates);
        }
        total_rows += rrd->rra_def[rra_index].row_cnt;
    }
    
    rc = 0;
    /* within each source file, order the RRAs by resolution - if we have an 
     * exact resolution match, use that one as the first in the (sub)list. */
    
    /* for each bin in each RRA select the best bin from among the candidate 
     * RRA data sets */
done:
    if (rra_added_temporarily) {
        remove_temporary_rra_for_cdp_prefilling(rrd, cdp_rra_index);
    }

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

static void init_mapping(mapping_t *mapping) {
    if (! mapping) return;
    mapping->ds_nam = NULL;
    mapping->def = NULL;
    mapping->mapped_name = NULL;
    mapping->index = -1;
}

static void free_mapping(mapping_t *mapping) {
    if (! mapping) return;
    if (mapping->ds_nam) free(mapping->ds_nam);
    if (mapping->def) free(mapping->def);
#ifdef HAVE_G_REGEX_NEW 
    if (mapping->mapped_name) free(mapping->mapped_name);
#else
    if (mapping->mapped_name) pcre_free_substring(mapping->mapped_name);
#endif
}
