/*****************************************************************************
 * RRDtool 1.0.33  Copyright Tobias Oetiker, 1997 - 2000
 *****************************************************************************
 * rrd_create.c  creates new rrds
 *****************************************************************************/

#include "rrd_tool.h"


int
rrd_create(int argc, char **argv) 
{
    rrd_t          rrd;
    long                i,long_tmp;
    time_t             last_up;
    struct time_value last_up_tv;
    char *parsetime_error = NULL;

    /* init last_up */
    last_up = time(NULL)-10;
    /* init rrd clean */
    rrd_init(&rrd);
    /* static header */
    if((rrd.stat_head = calloc(1,sizeof(stat_head_t)))==NULL){
	rrd_set_error("allocating rrd.stat_head");
	return(-1);
    }

    /* live header */
    if((rrd.live_head = calloc(1,sizeof(live_head_t)))==NULL){
	rrd_set_error("allocating rrd.live_head");
	return(-1);
    }

    /* set some defaults */
    strcpy(rrd.stat_head->cookie,RRD_COOKIE);
    strcpy(rrd.stat_head->version,RRD_VERSION);
    rrd.stat_head->float_cookie = FLOAT_COOKIE;
    rrd.stat_head->ds_cnt = 0; /* this will be adjusted later */
    rrd.stat_head->rra_cnt = 0; /* ditto */
    rrd.stat_head->pdp_step = 300; /* 5 minute default */

    /* a default value */
    rrd.ds_def = NULL;
    rrd.rra_def = NULL;
    
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
		rrd_free(&rrd);
                return(-1);
	    }
	    if (last_up_tv.type == RELATIVE_TO_END_TIME ||
		last_up_tv.type == RELATIVE_TO_START_TIME) {
		rrd_set_error("specifying time relative to the 'start' "
                              "or 'end' makes no sense here");
		rrd_free(&rrd);
		return(-1);
	    }

	    last_up = mktime(&last_up_tv.tm) + last_up_tv.offset;
	    
	    if (last_up < 3600*24*365*10){
		rrd_set_error("the first entry to the RRD should be after 1980");
		rrd_free(&rrd);
		return(-1);
	    }	
	    break;

	case 's':
	    long_tmp = atol(optarg);
	    if (long_tmp < 1){
		rrd_set_error("step size should be no less than one second");
		rrd_free(&rrd);
		return(-1);
	    }
	    rrd.stat_head->pdp_step = long_tmp;
	    break;

	case '?':
            if (optopt != 0)
                rrd_set_error("unknown option '%c'", optopt);
            else
                rrd_set_error("unknown option '%s'",argv[optind-1]);
            rrd_free(&rrd);
	    return(-1);
	}
    }
    rrd.live_head->last_up = last_up;

    for(i=optind+1;i<argc;i++){
	char minstr[DS_NAM_SIZE], maxstr[DS_NAM_SIZE];	
	int ii;
	if (strncmp(argv[i],"DS:",3)==0){
	    size_t old_size = sizeof(ds_def_t)*(rrd.stat_head->ds_cnt);
	    if((rrd.ds_def = rrd_realloc(rrd.ds_def,
				     old_size+sizeof(ds_def_t)))==NULL){
		rrd_set_error("allocating rrd.ds_def");
		rrd_free(&rrd);
		return(-1);	
	    }
	    memset(&rrd.ds_def[rrd.stat_head->ds_cnt], 0, sizeof(ds_def_t));
	    if (sscanf(&argv[i][3],
		       DS_NAM_FMT ":" DST_FMT ":%lu:%18[^:]:%18[^:]",
		       rrd.ds_def[rrd.stat_head->ds_cnt].ds_nam,
		       rrd.ds_def[rrd.stat_head->ds_cnt].dst,
		       &rrd.ds_def[rrd.stat_head->ds_cnt].par[DS_mrhb_cnt].u_cnt,
		       minstr,maxstr) == 5){
		/* check for duplicate datasource names */
		for(ii=0;ii<rrd.stat_head->ds_cnt;ii++){
			if(strcmp(rrd.ds_def[rrd.stat_head->ds_cnt].ds_nam,
			  	  rrd.ds_def[ii].ds_nam) == 0){
				rrd_set_error("Duplicate DS name: %s",rrd.ds_def[ii].ds_nam);
		                rrd_free(&rrd);
                                return(-1);
			}				                                
		}
		if(dst_conv(rrd.ds_def[rrd.stat_head->ds_cnt].dst) == -1){
		    rrd_free(&rrd);
		    return (-1);
		}
		if (minstr[0] == 'U' && minstr[1] == 0)
		    rrd.ds_def[rrd.stat_head->ds_cnt].par[DS_min_val].u_val = DNAN;
		else
		    rrd.ds_def[rrd.stat_head->ds_cnt].par[DS_min_val].u_val = atof(minstr);
		
		if (maxstr[0] == 'U' && maxstr[1] == 0)
		    rrd.ds_def[rrd.stat_head->ds_cnt].par[DS_max_val].u_val = DNAN;
		else
		    rrd.ds_def[rrd.stat_head->ds_cnt].par[DS_max_val].u_val  = atof(maxstr);
		
		if (! isnan(rrd.ds_def[rrd.stat_head->ds_cnt].par[DS_min_val].u_val) &&
		    ! isnan(rrd.ds_def[rrd.stat_head->ds_cnt].par[DS_max_val].u_val) &&
		    rrd.ds_def[rrd.stat_head->ds_cnt].par[DS_min_val].u_val
		    >= rrd.ds_def[rrd.stat_head->ds_cnt].par[DS_max_val].u_val ) {
		    rrd_set_error("min must be less than max in DS definition");
		    rrd_free(&rrd);
		    return (-1);		
		}
		rrd.stat_head->ds_cnt++;	    
	    } else {
		rrd_set_error("can't parse argument '%s'",argv[i]);
		rrd_free(&rrd);
		return (-1);		
	    }
	} else if (strncmp(argv[i],"RRA:",3)==0){
	    size_t old_size = sizeof(rra_def_t)*(rrd.stat_head->rra_cnt);
	    if((rrd.rra_def = rrd_realloc(rrd.rra_def,
				      old_size+sizeof(rra_def_t)))==NULL){
		rrd_set_error("allocating rrd.rra_def");
		rrd_free(&rrd);
		return(-1);	
	    }
	    memset(&rrd.rra_def[rrd.stat_head->rra_cnt], 0, sizeof(rra_def_t));
	    if (sscanf(&argv[i][4],
		       CF_NAM_FMT ":%lf:%lu:%lu",
		       rrd.rra_def[rrd.stat_head->rra_cnt].cf_nam,
		       &rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_cdp_xff_val].u_val,
		       &rrd.rra_def[rrd.stat_head->rra_cnt].pdp_cnt,
		       &rrd.rra_def[rrd.stat_head->rra_cnt].row_cnt) == 4){
		if(cf_conv(rrd.rra_def[rrd.stat_head->rra_cnt].cf_nam) == -1){
		    rrd_free(&rrd);
		    return (-1);
		}
	        if (rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_cdp_xff_val].u_val<0.0 ||
		    rrd.rra_def[rrd.stat_head->rra_cnt].par[RRA_cdp_xff_val].u_val>=1.0) {
		    rrd_set_error("the xff must always be >= 0 and < 1");
		    rrd_free(&rrd);
		    return (-1);
		}
		rrd.stat_head->rra_cnt++;	    		
	    }
	    else {  
		rrd_set_error("can't parse argument '%s'",argv[i]);
		rrd_free(&rrd);
		return (-1);		
	    }

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
    return rrd_create_fn(argv[optind],&rrd);
}

/* create and empty rrd file according to the specs given */

int
rrd_create_fn(char *file_name, rrd_t *rrd)
{
    unsigned long    i,ii;
    FILE             *rrd_file;
    rrd_value_t       unknown = DNAN ;

    if ((rrd_file = fopen(file_name,"wb")) == NULL ) {
	rrd_set_error("creating '%s': %s",file_name,strerror(errno));
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

    /* can not be zero because we don't know nothing ... */
    rrd->cdp_prep->scratch[CDP_val].u_val = DNAN;
    for(i=0; i < rrd->stat_head->rra_cnt; i++) {

	/* startup missing pdp count */
	rrd->cdp_prep->scratch[CDP_unkn_pdp_cnt].u_cnt = 
	    ((rrd->live_head->last_up -
	     rrd->pdp_prep->scratch[PDP_unkn_sec_cnt].u_cnt)
	    % (rrd->stat_head->pdp_step 
	       * rrd->rra_def[i].pdp_cnt)) / rrd->stat_head->pdp_step;	


	for(ii=0; ii < rrd->stat_head->ds_cnt; ii++) {
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

    rrd->rra_ptr->cur_row = 0;
    for(i=0; i <rrd->stat_head->rra_cnt; i++)
	fwrite( rrd->rra_ptr,
		sizeof(rra_ptr_t), 1,rrd_file);



    /* write the empty data area */
    for(i=0; 
	i <  rrd->stat_head->rra_cnt;
	i++)
	{
	    for(ii=0; 
		ii <  rrd->rra_def[i].row_cnt 
		    * rrd->stat_head->ds_cnt;
		ii++){
		fwrite(&unknown,sizeof(rrd_value_t),1,rrd_file);
	    }
	}

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
