/*****************************************************************************
 * RRDtool 1.2rc7  Copyright by Tobi Oetiker, 1997-2005
 *****************************************************************************
 * rrd_create.c  creates new rrds
 *****************************************************************************/

#include "rrd_tool.h"
#include "rrd_rpncalc.h"
#include "rrd_hw.h"

#include "rrd_is_thread_safe.h"

unsigned long FnvHash(char *str);
int create_hw_contingent_rras(rrd_t *rrd, unsigned short period, unsigned long hashed_name);
void parseGENERIC_DS(char *def,rrd_t *rrd, int ds_idx);

int
rrd_create(int argc, char **argv) 
{
    time_t            last_up = time(NULL)-10;
    unsigned long     pdp_step = 300;
    struct rrd_time_value last_up_tv;
    char *parsetime_error = NULL;
    long              long_tmp;
    int               rc;

    while (1){
	static struct option long_options[] =
	{
	    {"start",      required_argument, 0, 'b'},
	    {"step",        required_argument,0,'s'},
	    {0,0,0,0}
	};
	int option_index = 0;
	int opt;
	opt = getopt_long(argc, argv, "b:s:", 
			  long_options, &option_index);
	
	if (opt == EOF)
	    break;
	
	switch(opt) {
	case 'b':
            if ((parsetime_error = parsetime(optarg, &last_up_tv))) {
                rrd_set_error("start time: %s", parsetime_error );
                return(-1);
	    }
	    if (last_up_tv.type == RELATIVE_TO_END_TIME ||
		last_up_tv.type == RELATIVE_TO_START_TIME) {
		rrd_set_error("specifying time relative to the 'start' "
                              "or 'end' makes no sense here");
		return(-1);
	    }

	    last_up = mktime(&last_up_tv.tm) + last_up_tv.offset;
	    
	    if (last_up < 3600*24*365*10){
		rrd_set_error("the first entry to the RRD should be after 1980");
		return(-1);
	    }	
	    break;

	case 's':
	    long_tmp = atol(optarg);
	    if (long_tmp < 1){
		rrd_set_error("step size should be no less than one second");
		return(-1);
	    }
	    pdp_step = long_tmp;
	    break;

	case '?':
            if (optopt != 0)
                rrd_set_error("unknown option '%c'", optopt);
            else
                rrd_set_error("unknown option '%s'",argv[optind-1]);
	    return(-1);
	}
    }

    rc = rrd_create_r(argv[optind],
		      pdp_step, last_up,
		      argc - optind - 1, argv + optind + 1);
    
    return rc;
}

/* #define DEBUG */
int
rrd_create_r(char *filename,
	     unsigned long pdp_step, time_t last_up,
	     int argc, char **argv) 
{
    rrd_t             rrd;
    long              i;
    int               offset;
    char *token;
    unsigned short token_idx, error_flag, period=0;
    unsigned long hashed_name;

    /* init rrd clean */
    rrd_init(&rrd);
    /* static header */
    if((rrd.stat_head = calloc(1,sizeof(stat_head_t)))==NULL){
	rrd_set_error("allocating rrd.stat_head");
	rrd_free(&rrd);
	return(-1);
    }

    /* live header */
    if((rrd.live_head = calloc(1,sizeof(live_head_t)))==NULL){
	rrd_set_error("allocating rrd.live_head");
	rrd_free(&rrd);
	return(-1);
    }

    /* set some defaults */
    strcpy(rrd.stat_head->cookie,RRD_COOKIE);
    strcpy(rrd.stat_head->version,RRD_VERSION);
    rrd.stat_head->float_cookie = FLOAT_COOKIE;
    rrd.stat_head->ds_cnt = 0; /* this will be adjusted later */
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
    for(i=0;i<argc;i++){
	unsigned int ii;
	if (strncmp(argv[i],"DS:",3)==0){
	    size_t old_size = sizeof(ds_def_t)*(rrd.stat_head->ds_cnt);
	    if((rrd.ds_def = rrd_realloc(rrd.ds_def,
                                         old_size+sizeof(ds_def_t)))==NULL){
		rrd_set_error("allocating rrd.ds_def");
		rrd_free(&rrd);
		return(-1);	
	    }
	    memset(&rrd.ds_def[rrd.stat_head->ds_cnt], 0, sizeof(ds_def_t));
            /* extract the name and type */
	    if (sscanf(&argv[i][3],
		       DS_NAM_FMT ":" DST_FMT ":%n",
		       rrd.ds_def[rrd.stat_head->ds_cnt].ds_nam,
		       rrd.ds_def[rrd.stat_head->ds_cnt].dst,&offset) == 2)
            {
                /* check for duplicate datasource names */
                for(ii=0;ii<rrd.stat_head->ds_cnt;ii++)
                    if(strcmp(rrd.ds_def[rrd.stat_head->ds_cnt].ds_nam,
                              rrd.ds_def[ii].ds_nam) == 0){
                        rrd_set_error("Duplicate DS name: %s",rrd.ds_def[ii].ds_nam);
                    }				                                
            } else {
                rrd_set_error("invalid DS format");
            }
            
            /* parse the remainder of the arguments */
            switch(dst_conv(rrd.ds_def[rrd.stat_head->ds_cnt].dst))
	    {
            case DST_COUNTER:
            case DST_ABSOLUTE:
            case DST_GAUGE:
            case DST_DERIVE:
                parseGENERIC_DS(&argv[i][offset+3],&rrd, rrd.stat_head->ds_cnt);
                break;
            case DST_CDEF:
                parseCDEF_DS(&argv[i][offset+3],&rrd, rrd.stat_head->ds_cnt);
                break;
            default:
                rrd_set_error("invalid DS type specified");
                break;
            }
            
            if (rrd_test_error()) {
                rrd_free(&rrd);
                return -1;
            }
            rrd.stat_head -> ds_cnt++;
	} else if (strncmp(argv[i],"RRA:",3)==0){
	    char *tokptr;
	    size_t old_size = sizeof(rra_def_t)*(rrd.stat_head->rra_cnt);
	    if((rrd.rra_def = rrd_realloc(rrd.rra_def,
                                          old_size+sizeof(rra_def_t)))==NULL)
	    {
                rrd_set_error("allocating rrd.rra_def");
                rrd_free(&rrd);
                return(-1);	
	    }
	    memset(&rrd.rra_def[rrd.stat_head->rra_cnt], 0, sizeof(rra_def_t));
            
	    token = strtok_r(&argv[i][4],":", &tokptr);
	    token_idx = error_flag = 0;
	    while (token != NULL)
	    {
                switch(token_idx)
                {
                case 0:
                    if (sscanf(token,CF_NAM_FMT,
                               rrd.rra_def[rrd.stat_head->rra_cnt].cf_nam) != 1)
                        rrd_set_error("Failed to parse CF name");
                    switch(cf_conv(rrd.rra_def[rrd.stat_head->rra_cnt].cf_nam))
                    {
                    case CF_HWPREDICT:
                        /* initialize some parameters */
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_hw_alpha].u_val = 0.1;
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_hw_beta].u_val = 1.0/288;
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_dependent_rra_idx].u_cnt = 
                            rrd.stat_head -> rra_cnt;
                        break;
                    case CF_DEVSEASONAL:
                    case CF_SEASONAL:
                        /* initialize some parameters */
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_seasonal_gamma].u_val = 0.1;
                        /* fall through */
                    case CF_DEVPREDICT:
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_dependent_rra_idx].u_cnt = -1;
                        break;
                    case CF_FAILURES:
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_delta_pos].u_val = 2.0;
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_delta_neg].u_val = 2.0;
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_window_len].u_cnt = 3;
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_failure_threshold].u_cnt = 2;
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_dependent_rra_idx].u_cnt = -1;
                        break;
                        /* invalid consolidation function */
                    case -1:
                        rrd_set_error("Unrecognized consolidation function %s",
                                      rrd.rra_def[rrd.stat_head->rra_cnt].cf_nam);
                    default:
                        break;
                    }
                    /* default: 1 pdp per cdp */ 
                    rrd.rra_def[rrd.stat_head->rra_cnt].pdp_cnt = 1;
                    break;
                case 1:
                    switch(cf_conv(rrd.rra_def[rrd.stat_head->rra_cnt].cf_nam))
                    {
                    case CF_HWPREDICT:
                    case CF_DEVSEASONAL:
                    case CF_SEASONAL:
                    case CF_DEVPREDICT:
                    case CF_FAILURES:
                        rrd.rra_def[rrd.stat_head->rra_cnt].row_cnt = atoi(token);
                        break;
                    default:
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_cdp_xff_val].u_val = atof(token);
                        if (rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_cdp_xff_val].u_val<0.0 ||
                            rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_cdp_xff_val].u_val>=1.0)
                            rrd_set_error("Invalid xff: must be between 0 and 1");
                        break;
                    }
                    break;
                case 2:
                    switch(cf_conv(rrd.rra_def[rrd.stat_head->rra_cnt].cf_nam))
                    {
                    case CF_HWPREDICT:
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_hw_alpha].u_val = atof(token);
                        if (atof(token) <= 0.0 || atof(token) >= 1.0)
                            rrd_set_error("Invalid alpha: must be between 0 and 1");
                        break;
                    case CF_DEVSEASONAL:
                    case CF_SEASONAL:
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_seasonal_gamma].u_val = 
                            atof(token);
                        if (atof(token) <= 0.0 || atof(token) >= 1.0)
                            rrd_set_error("Invalid gamma: must be between 0 and 1");
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_seasonal_smooth_idx].u_cnt
                            = hashed_name % rrd.rra_def[rrd.stat_head->rra_cnt].row_cnt; 
                        break;
                    case CF_FAILURES:
                        /* specifies the # of violations that constitutes the failure threshold */
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_failure_threshold].u_cnt =
                            atoi(token);
                        if (atoi(token) < 1 || atoi(token) > MAX_FAILURES_WINDOW_LEN)
                            rrd_set_error("Failure threshold is out of range %d, %d",1,
                                          MAX_FAILURES_WINDOW_LEN);
                        break;
                    case CF_DEVPREDICT:
                        /* specifies the index (1-based) of CF_DEVSEASONAL array
                         * associated with this CF_DEVPREDICT array. */
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_dependent_rra_idx].u_cnt =
                            atoi(token) - 1;
                        break;
                    default:
                        rrd.rra_def[rrd.stat_head->rra_cnt].pdp_cnt = atoi(token);
                        break;
                    }
                    break;
                case 3:
                    switch(cf_conv(rrd.rra_def[rrd.stat_head->rra_cnt].cf_nam))
                    {
                    case CF_HWPREDICT:
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_hw_beta].u_val = atof(token);
                        if (atof(token) < 0.0 || atof(token) > 1.0)
                            rrd_set_error("Invalid beta: must be between 0 and 1");
                        break;
                    case CF_DEVSEASONAL:
                    case CF_SEASONAL:
                        /* specifies the index (1-based) of CF_HWPREDICT array
                         * associated with this CF_DEVSEASONAL or CF_SEASONAL array. 
                         * */
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_dependent_rra_idx].u_cnt =
                            atoi(token) - 1;
                        break;
                    case CF_FAILURES:
                        /* specifies the window length */
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_window_len].u_cnt =
                            atoi(token);
                        if (atoi(token) < 1 || atoi(token) > MAX_FAILURES_WINDOW_LEN)
                            rrd_set_error("Window length is out of range %d, %d",1,
                                          MAX_FAILURES_WINDOW_LEN);
                        /* verify that window length exceeds the failure threshold */
                        if (rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_window_len].u_cnt <
                            rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_failure_threshold].u_cnt)
                            rrd_set_error("Window length is shorter than the failure threshold");
                        break;
                    case CF_DEVPREDICT:
                        /* shouldn't be any more arguments */
                        rrd_set_error("Unexpected extra argument for consolidation function DEVPREDICT");
                        break;
                    default:
                        rrd.rra_def[rrd.stat_head->rra_cnt].row_cnt = atoi(token);
                        break;
                    }
                    break;
                case 4:
                    switch(cf_conv(rrd.rra_def[rrd.stat_head->rra_cnt].cf_nam))
                    {
                    case CF_FAILURES:
                        /* specifies the index (1-based) of CF_DEVSEASONAL array
                         * associated with this CF_DEVFAILURES array. */
                        rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_dependent_rra_idx].u_cnt =
                            atoi(token) - 1;
                        break;
                    case CF_HWPREDICT:
                        /* length of the associated CF_SEASONAL and CF_DEVSEASONAL arrays. */
                        period = atoi(token);
                        if (period > rrd.rra_def[rrd.stat_head->rra_cnt].row_cnt)
                            rrd_set_error("Length of seasonal cycle exceeds length of HW prediction array");
                        break;
                    default:
                        /* shouldn't be any more arguments */
                        rrd_set_error("Unexpected extra argument for consolidation function %s",
                                      rrd.rra_def[rrd.stat_head->rra_cnt].cf_nam);
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
                    rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_dependent_rra_idx].u_cnt =
                        atoi(token) - 1;
                    break;
                default:
                    /* should never get here */
                    rrd_set_error("Unknown error");
                    break;
                } /* end switch */
                if (rrd_test_error())
                {
                    /* all errors are unrecoverable */
                    rrd_free(&rrd);
                    return (-1);
                }
                token = strtok_r(NULL,":", &tokptr);
                token_idx++;
	    } /* end while */
#ifdef DEBUG
	    fprintf(stderr,"Creating RRA CF: %s, dep idx %lu, current idx %lu\n",
		    rrd.rra_def[rrd.stat_head->rra_cnt].cf_nam,
		    rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_dependent_rra_idx].u_cnt, 
		    rrd.stat_head -> rra_cnt);
#endif
	    /* should we create CF_SEASONAL, CF_DEVSEASONAL, and CF_DEVPREDICT? */
	    if (cf_conv(rrd.rra_def[rrd.stat_head->rra_cnt].cf_nam) == CF_HWPREDICT
		&& rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_dependent_rra_idx].u_cnt 
		== rrd.stat_head -> rra_cnt)
	    {
#ifdef DEBUG
                fprintf(stderr,"Creating HW contingent RRAs\n");
#endif
                if (create_hw_contingent_rras(&rrd,period,hashed_name) == -1) {
                    rrd_set_error("creating contingent RRA");
                    rrd_free(&rrd);
                    return -1;
                }
	    }
	    rrd.stat_head->rra_cnt++;	    		
	} else {
	    rrd_set_error("can't parse argument '%s'",argv[i]);
	    rrd_free(&rrd);
            return -1;
	}
    }
    
    
    if (rrd.stat_head->rra_cnt < 1){
	rrd_set_error("you must define at least one Round Robin Archive");
	rrd_free(&rrd);
	return(-1);
    }
    
    if (rrd.stat_head->ds_cnt < 1){
	rrd_set_error("you must define at least one Data Source");
	rrd_free(&rrd);
	return(-1);
    }
    return rrd_create_fn(filename, &rrd);
}

void parseGENERIC_DS(char *def,rrd_t *rrd, int ds_idx)
{
    char minstr[DS_NAM_SIZE], maxstr[DS_NAM_SIZE];	
    /*
      int temp;
      
      temp = sscanf(def,"%lu:%18[^:]:%18[^:]",	
      &(rrd -> ds_def[ds_idx].par[DS_mrhb_cnt].u_cnt),
      minstr,maxstr);
    */
    if (sscanf(def,"%lu:%18[^:]:%18[^:]",	
               &(rrd -> ds_def[ds_idx].par[DS_mrhb_cnt].u_cnt),
               minstr,maxstr) == 3)
    {
        if (minstr[0] == 'U' && minstr[1] == 0)
            rrd -> ds_def[ds_idx].par[DS_min_val].u_val = DNAN;
        else
            rrd -> ds_def[ds_idx].par[DS_min_val].u_val = atof(minstr);
        
        if (maxstr[0] == 'U' && maxstr[1] == 0)
            rrd -> ds_def[ds_idx].par[DS_max_val].u_val = DNAN;
        else
            rrd -> ds_def[ds_idx].par[DS_max_val].u_val  = atof(maxstr);
        
        if (! isnan(rrd -> ds_def[ds_idx].par[DS_min_val].u_val) &&
            ! isnan(rrd -> ds_def[ds_idx].par[DS_max_val].u_val) &&
            rrd -> ds_def[ds_idx].par[DS_min_val].u_val
            >= rrd -> ds_def[ds_idx].par[DS_max_val].u_val ) {
            rrd_set_error("min must be less than max in DS definition");
            return;		
        }
    } else {
        rrd_set_error("failed to parse data source %s", def);
    }
}

/* Create the CF_DEVPREDICT, CF_DEVSEASONAL, CF_SEASONAL, and CF_FAILURES RRAs
 * associated with a CF_HWPREDICT RRA. */
int
create_hw_contingent_rras(rrd_t *rrd, unsigned short period, unsigned long hashed_name)
{
    size_t old_size;
    rra_def_t* current_rra;
    
    /* save index to CF_HWPREDICT */
    unsigned long hw_index = rrd -> stat_head -> rra_cnt;
    /* advance the pointer */
    (rrd -> stat_head -> rra_cnt)++;	    		
    /* allocate the memory for the 4 contingent RRAs */
    old_size = sizeof(rra_def_t)*(rrd -> stat_head->rra_cnt);
    if ((rrd -> rra_def = rrd_realloc(rrd -> rra_def,
                                      old_size+4*sizeof(rra_def_t)))==NULL)
    {
        rrd_set_error("allocating rrd.rra_def");
        return(-1);	
    }
    /* clear memory */
    memset(&(rrd -> rra_def[rrd -> stat_head->rra_cnt]), 0, 4*sizeof(rra_def_t));
    
    /* create the CF_SEASONAL RRA */
    current_rra = &(rrd -> rra_def[rrd -> stat_head -> rra_cnt]);
    strcpy(current_rra -> cf_nam,"SEASONAL");
    current_rra -> row_cnt = period;
    current_rra -> par[RRA_seasonal_smooth_idx].u_cnt = hashed_name % period;
    current_rra -> pdp_cnt = 1;
    current_rra -> par[RRA_seasonal_gamma].u_val = 
        rrd -> rra_def[hw_index].par[RRA_hw_alpha].u_val;
    current_rra -> par[RRA_dependent_rra_idx].u_cnt = hw_index; 
    rrd -> rra_def[hw_index].par[RRA_dependent_rra_idx].u_cnt = rrd -> stat_head -> rra_cnt;
    
    /* create the CF_DEVSEASONAL RRA */
    (rrd -> stat_head -> rra_cnt)++; 
    current_rra = &(rrd -> rra_def[rrd -> stat_head -> rra_cnt]);
    strcpy(current_rra -> cf_nam,"DEVSEASONAL");
    current_rra -> row_cnt = period;
    current_rra -> par[RRA_seasonal_smooth_idx].u_cnt = hashed_name % period;
    current_rra -> pdp_cnt = 1;
    current_rra -> par[RRA_seasonal_gamma].u_val = 
        rrd -> rra_def[hw_index].par[RRA_hw_alpha].u_val;
    current_rra -> par[RRA_dependent_rra_idx].u_cnt = hw_index; 
    
    /* create the CF_DEVPREDICT RRA */
    (rrd -> stat_head -> rra_cnt)++; 
    current_rra = &(rrd -> rra_def[rrd -> stat_head -> rra_cnt]);
    strcpy(current_rra -> cf_nam,"DEVPREDICT");
    current_rra -> row_cnt = (rrd -> rra_def[hw_index]).row_cnt;
    current_rra -> pdp_cnt = 1;
    current_rra -> par[RRA_dependent_rra_idx].u_cnt 
        = hw_index + 2; /* DEVSEASONAL */
    
    /* create the CF_FAILURES RRA */
    (rrd -> stat_head -> rra_cnt)++; 
    current_rra = &(rrd -> rra_def[rrd -> stat_head -> rra_cnt]);
    strcpy(current_rra -> cf_nam,"FAILURES");
    current_rra -> row_cnt = period; 
    current_rra -> pdp_cnt = 1;
    current_rra -> par[RRA_delta_pos].u_val = 2.0;
    current_rra -> par[RRA_delta_neg].u_val = 2.0;
    current_rra -> par[RRA_failure_threshold].u_cnt = 7;
    current_rra -> par[RRA_window_len].u_cnt = 9;
    current_rra -> par[RRA_dependent_rra_idx].u_cnt = 
        hw_index + 2; /* DEVSEASONAL */
    return 0;
}

/* create and empty rrd file according to the specs given */

int
rrd_create_fn(char *file_name, rrd_t *rrd)
{
    unsigned long    i,ii;
    FILE             *rrd_file;
    rrd_value_t      *unknown;
    int	unkn_cnt;
    
    if ((rrd_file = fopen(file_name,"wb")) == NULL ) {
	rrd_set_error("creating '%s': %s",file_name, rrd_strerror(errno));
	free(rrd->stat_head);
	free(rrd->ds_def);
	free(rrd->rra_def);
	return(-1);
    }
    
    fwrite(rrd->stat_head,
	   sizeof(stat_head_t), 1, rrd_file);
    
    fwrite(rrd->ds_def,
	   sizeof(ds_def_t), rrd->stat_head->ds_cnt, rrd_file);
    
    fwrite(rrd->rra_def,
	   sizeof(rra_def_t), rrd->stat_head->rra_cnt, rrd_file);
    
    fwrite(rrd->live_head,
	   sizeof(live_head_t),1, rrd_file);

    if((rrd->pdp_prep = calloc(1,sizeof(pdp_prep_t))) == NULL){
	rrd_set_error("allocating pdp_prep");
	rrd_free(rrd);
	fclose(rrd_file);
	return(-1);
    }

    strcpy(rrd->pdp_prep->last_ds,"UNKN");

    rrd->pdp_prep->scratch[PDP_val].u_val = 0.0;
    rrd->pdp_prep->scratch[PDP_unkn_sec_cnt].u_cnt = 
	rrd->live_head->last_up % rrd->stat_head->pdp_step;

    for(i=0; i < rrd->stat_head->ds_cnt; i++)
	fwrite( rrd->pdp_prep,sizeof(pdp_prep_t),1,rrd_file);
    
    if((rrd->cdp_prep = calloc(1,sizeof(cdp_prep_t))) == NULL){
	rrd_set_error("allocating cdp_prep");
	rrd_free(rrd);
	fclose(rrd_file);
	return(-1);
    }


    for(i=0; i < rrd->stat_head->rra_cnt; i++) {
       switch (cf_conv(rrd->rra_def[i].cf_nam))
	   {
           case CF_HWPREDICT:
               init_hwpredict_cdp(rrd->cdp_prep);
               break;
           case CF_SEASONAL:
           case CF_DEVSEASONAL:
               init_seasonal_cdp(rrd->cdp_prep);
               break;
           case CF_FAILURES:
               /* initialize violation history to 0 */
               for (ii = 0; ii < MAX_CDP_PAR_EN; ii++)
               {
				/* We can zero everything out, by setting u_val to the
				 * NULL address. Each array entry in scratch is 8 bytes
				 * (a double), but u_cnt only accessed 4 bytes (long) */
                   rrd->cdp_prep->scratch[ii].u_val = 0.0;
               }
               break;
           default:
               /* can not be zero because we don't know anything ... */
               rrd->cdp_prep->scratch[CDP_val].u_val = DNAN;
               /* startup missing pdp count */
               rrd->cdp_prep->scratch[CDP_unkn_pdp_cnt].u_cnt = 
                   ((rrd->live_head->last_up -
	         rrd->pdp_prep->scratch[PDP_unkn_sec_cnt].u_cnt)
                    % (rrd->stat_head->pdp_step 
                       * rrd->rra_def[i].pdp_cnt)) / rrd->stat_head->pdp_step;	
               break;
	   }
       
       for(ii=0; ii < rrd->stat_head->ds_cnt; ii++) 
       {
           fwrite( rrd->cdp_prep,sizeof(cdp_prep_t),1,rrd_file);
       }
    }
    
    /* now, we must make sure that the rest of the rrd
       struct is properly initialized */
    
    if((rrd->rra_ptr = calloc(1,sizeof(rra_ptr_t))) == NULL) {
	rrd_set_error("allocating rra_ptr");
	rrd_free(rrd);
	fclose(rrd_file);
	return(-1);
    }
    
    /* changed this initialization to be consistent with
     * rrd_restore. With the old value (0), the first update
     * would occur for cur_row = 1 because rrd_update increments
     * the pointer a priori. */
    for (i=0; i < rrd->stat_head->rra_cnt; i++)
    {
        rrd->rra_ptr->cur_row = rrd->rra_def[i].row_cnt - 1;
        fwrite( rrd->rra_ptr, sizeof(rra_ptr_t),1,rrd_file);
    }
    
    /* write the empty data area */
    if ((unknown = (rrd_value_t *)malloc(512 * sizeof(rrd_value_t))) == NULL) {
	rrd_set_error("allocating unknown");
	rrd_free(rrd);
	fclose(rrd_file);
	return(-1);
    }
    for (i = 0; i < 512; ++i)
	unknown[i] = DNAN;
    
    unkn_cnt = 0;
    for (i = 0; i < rrd->stat_head->rra_cnt; i++)
        unkn_cnt += rrd->stat_head->ds_cnt * rrd->rra_def[i].row_cnt;
		      
    while (unkn_cnt > 0) {
	fwrite(unknown, sizeof(rrd_value_t), min(unkn_cnt, 512), rrd_file);
	unkn_cnt -= 512;
     }
    free(unknown);
    
    /* lets see if we had an error */
    if(ferror(rrd_file)){
	rrd_set_error("a file error occurred while creating '%s'",file_name);
	fclose(rrd_file);	
	rrd_free(rrd);
	return(-1);
    }
    
    fclose(rrd_file);    
    rrd_free(rrd);
    return (0);
}
