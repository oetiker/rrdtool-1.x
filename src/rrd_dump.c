/*****************************************************************************
 * RRDtool 1.2rc5  Copyright by Tobi Oetiker, 1997-2005
 *****************************************************************************
 * rrd_dump  Display a RRD
 *****************************************************************************
 * $Id$
 * $Log$
 * Revision 1.7  2004/05/25 20:53:21  oetiker
 * prevent small leak when resources are exhausted -- Mike Slifcak
 *
 * Revision 1.6  2004/05/25 20:51:49  oetiker
 * Update displayed copyright messages to be consistent. -- Mike Slifcak
 *
 * Revision 1.5  2003/02/13 07:05:27  oetiker
 * Find attached the patch I promised to send to you. Please note that there
 * are three new source files (src/rrd_is_thread_safe.h, src/rrd_thread_safe.c
 * and src/rrd_not_thread_safe.c) and the introduction of librrd_th. This
 * library is identical to librrd, but it contains support code for per-thread
 * global variables currently used for error information only. This is similar
 * to how errno per-thread variables are implemented.  librrd_th must be linked
 * alongside of libpthred
 *
 * There is also a new file "THREADS", holding some documentation.
 *
 * -- Peter Stamfest <peter@stamfest.at>
 *
 * Revision 1.4  2002/02/01 20:34:49  oetiker
 * fixed version number and date/time
 *
 * Revision 1.3  2001/03/10 23:54:39  oetiker
 * Support for COMPUTE data sources (CDEF data sources). Removes the RPN
 * parser and calculator from rrd_graph and puts then in a new file,
 * rrd_rpncalc.c. Changes to core files rrd_create and rrd_update. Some
 * clean-up of aberrant behavior stuff, including a bug fix.
 * Documentation update (rrdcreate.pod, rrdupdate.pod). Change xml format.
 * -- Jake Brutlag <jakeb@corp.webtv.net>
 *
 * Revision 1.2  2001/03/04 13:01:55  oetiker
 *
 * Revision 1.1.1.1  2001/02/25 22:25:05  oetiker
 * checkin
 *
 *****************************************************************************/

#include "rrd_tool.h"
#include "rrd_rpncalc.h"

extern char *tzname[2];

int
rrd_dump(int argc, char **argv) 
{
    int                 rc;

    if (argc < 2) {
	rrd_set_error("Not enough arguments");
	return -1;
    }

    rc = rrd_dump_r(argv[1]);
    
    return rc;
}

int
rrd_dump_r(char *filename)    
{   
    unsigned int i,ii,ix,iii=0;
    time_t       now;
    char         somestring[255];
    rrd_value_t  my_cdp;
    long         rra_base, rra_start, rra_next;
    FILE        *in_file;
    rrd_t        rrd;
    rrd_value_t  value;
    struct tm    tm;
    if(rrd_open(filename, &in_file,&rrd, RRD_READONLY)==-1){
	rrd_free(&rrd);
	return(-1);
    }

    puts("<!-- Round Robin Database Dump -->");
    puts("<rrd>");
    printf("\t<version> %s </version>\n",RRD_VERSION);
    printf("\t<step> %lu </step> <!-- Seconds -->\n",rrd.stat_head->pdp_step);
#if HAVE_STRFTIME
    localtime_r(&rrd.live_head->last_up, &tm);
    strftime(somestring,200,"%Y-%m-%d %H:%M:%S %Z",
	     &tm);
#else
# error "Need strftime"
#endif
    printf("\t<lastupdate> %ld </lastupdate> <!-- %s -->\n\n",
	   rrd.live_head->last_up,somestring);
    for(i=0;i<rrd.stat_head->ds_cnt;i++){
	printf("\t<ds>\n");
	printf("\t\t<name> %s </name>\n",rrd.ds_def[i].ds_nam);
	printf("\t\t<type> %s </type>\n",rrd.ds_def[i].dst);
    if (dst_conv(rrd.ds_def[i].dst) != DST_CDEF) {
	printf("\t\t<minimal_heartbeat> %lu </minimal_heartbeat>\n",rrd.ds_def[i].par[DS_mrhb_cnt].u_cnt);
	if (isnan(rrd.ds_def[i].par[DS_min_val].u_val)){
	  printf("\t\t<min> NaN </min>\n");
	} else {
	  printf("\t\t<min> %0.10e </min>\n",rrd.ds_def[i].par[DS_min_val].u_val);
	}
	if (isnan(rrd.ds_def[i].par[DS_max_val].u_val)){
	  printf("\t\t<max> NaN </max>\n");
	} else {
	  printf("\t\t<max> %0.10e </max>\n",rrd.ds_def[i].par[DS_max_val].u_val);
	}
    } else { /* DST_CDEF */
	  char *str;
	  rpn_compact2str((rpn_cdefds_t *) &(rrd.ds_def[i].par[DS_cdef]),rrd.ds_def,&str);
	  printf("\t\t<cdef> %s </cdef>\n", str);
	  free(str);
	}
	printf("\n\t\t<!-- PDP Status -->\n");
	printf("\t\t<last_ds> %s </last_ds>\n",rrd.pdp_prep[i].last_ds);
	if (isnan(rrd.pdp_prep[i].scratch[PDP_val].u_val)){
	  printf("\t\t<value> NaN </value>\n");
	} else {
	  printf("\t\t<value> %0.10e </value>\n",rrd.pdp_prep[i].scratch[PDP_val].u_val);
	}
	printf("\t\t<unknown_sec> %lu </unknown_sec>\n",
	       rrd.pdp_prep[i].scratch[PDP_unkn_sec_cnt].u_cnt);
	
	printf("\t</ds>\n\n");
    }

    puts("<!-- Round Robin Archives -->");

    rra_base=ftell(in_file);    
    rra_next = rra_base;

    for(i=0;i<rrd.stat_head->rra_cnt;i++){
	
	long timer=0;
	rra_start= rra_next;
	rra_next +=  ( rrd.stat_head->ds_cnt
                      * rrd.rra_def[i].row_cnt
                      * sizeof(rrd_value_t));
	printf("\t<rra>\n");
	printf("\t\t<cf> %s </cf>\n",rrd.rra_def[i].cf_nam);
	printf("\t\t<pdp_per_row> %lu </pdp_per_row> <!-- %lu seconds -->\n\n",
	       rrd.rra_def[i].pdp_cnt, rrd.rra_def[i].pdp_cnt
	       *rrd.stat_head->pdp_step);
	/* support for RRA parameters */
	printf("\t\t<params>\n");
	switch(cf_conv(rrd.rra_def[i].cf_nam)) {
	case CF_HWPREDICT:
	   printf("\t\t<hw_alpha> %0.10e </hw_alpha>\n", 
		  rrd.rra_def[i].par[RRA_hw_alpha].u_val);
	   printf("\t\t<hw_beta> %0.10e </hw_beta>\n", 
		  rrd.rra_def[i].par[RRA_hw_beta].u_val);
	   printf("\t\t<dependent_rra_idx> %lu </dependent_rra_idx>\n",
		  rrd.rra_def[i].par[RRA_dependent_rra_idx].u_cnt);
	   break;
	case CF_SEASONAL:
	case CF_DEVSEASONAL:
	   printf("\t\t<seasonal_gamma> %0.10e </seasonal_gamma>\n", 
		  rrd.rra_def[i].par[RRA_seasonal_gamma].u_val);
	   printf("\t\t<seasonal_smooth_idx> %lu </seasonal_smooth_idx>\n",
		  rrd.rra_def[i].par[RRA_seasonal_smooth_idx].u_cnt);
	   printf("\t\t<dependent_rra_idx> %lu </dependent_rra_idx>\n",
		  rrd.rra_def[i].par[RRA_dependent_rra_idx].u_cnt);
	   break;
	case CF_FAILURES:
	   printf("\t\t<delta_pos> %0.10e </delta_pos>\n", 
		  rrd.rra_def[i].par[RRA_delta_pos].u_val);
	   printf("\t\t<delta_neg> %0.10e </delta_neg>\n", 
		  rrd.rra_def[i].par[RRA_delta_neg].u_val);
	   printf("\t\t<window_len> %lu </window_len>\n",
		  rrd.rra_def[i].par[RRA_window_len].u_cnt);
	   printf("\t\t<failure_threshold> %lu </failure_threshold>\n",
		  rrd.rra_def[i].par[RRA_failure_threshold].u_cnt);
		  /* fall thru */
	case CF_DEVPREDICT:
	   printf("\t\t<dependent_rra_idx> %lu </dependent_rra_idx>\n",
		  rrd.rra_def[i].par[RRA_dependent_rra_idx].u_cnt);
	   break;
	case CF_AVERAGE:
	case CF_MAXIMUM:
	case CF_MINIMUM:
	case CF_LAST:
	default:
	   printf("\t\t<xff> %0.10e </xff>\n", rrd.rra_def[i].par[RRA_cdp_xff_val].u_val);
	   break;
	}
	printf("\t\t</params>\n");
	printf("\t\t<cdp_prep>\n");
	for(ii=0;ii<rrd.stat_head->ds_cnt;ii++){
		unsigned long ivalue;
		printf("\t\t\t<ds>\n");
		/* support for exporting all CDP parameters */
		/* parameters common to all CFs */
		    /* primary_val and secondary_val do not need to be saved between updates
			 * so strictly speaking they could be omitted.
			 * However, they can be useful for diagnostic purposes, so are included here. */
	        value = rrd.cdp_prep[i*rrd.stat_head->ds_cnt
			   +ii].scratch[CDP_primary_val].u_val;
			if (isnan(value)) {
			   printf("\t\t\t<primary_value> NaN </primary_value>\n");
			} else {
			   printf("\t\t\t<primary_value> %0.10e </primary_value>\n", value);
			}
	        value = rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_secondary_val].u_val;
			if (isnan(value)) {
			   printf("\t\t\t<secondary_value> NaN </secondary_value>\n");
			} else {
			   printf("\t\t\t<secondary_value> %0.10e </secondary_value>\n", value);
			}
		switch(cf_conv(rrd.rra_def[i].cf_nam)) {
		case CF_HWPREDICT:
	        value = rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_hw_intercept].u_val;
			if (isnan(value)) {
			   printf("\t\t\t<intercept> NaN </intercept>\n");
			} else {
			   printf("\t\t\t<intercept> %0.10e </intercept>\n", value);
			}
	        value = rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_hw_last_intercept].u_val;
			if (isnan(value)) {
			   printf("\t\t\t<last_intercept> NaN </last_intercept>\n");
			} else {
			   printf("\t\t\t<last_intercept> %0.10e </last_intercept>\n", value);
			}
	        value = rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_hw_slope].u_val;
			if (isnan(value)) {
			   printf("\t\t\t<slope> NaN </slope>\n");
			} else {
			   printf("\t\t\t<slope> %0.10e </slope>\n", value);
			}
	        value = rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_hw_last_slope].u_val;
			if (isnan(value)) {
			   printf("\t\t\t<last_slope> NaN </last_slope>\n");
			} else {
			   printf("\t\t\t<last_slope> %0.10e </last_slope>\n", value);
			}
			ivalue = rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_null_count].u_cnt;
			printf("\t\t\t<nan_count> %lu </nan_count>\n", ivalue);
			ivalue = rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_last_null_count].u_cnt;
			printf("\t\t\t<last_nan_count> %lu </last_nan_count>\n", ivalue);
			break;
		case CF_SEASONAL:
		case CF_DEVSEASONAL:
	        value = rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_hw_seasonal].u_val;
			if (isnan(value)) {
			   printf("\t\t\t<seasonal> NaN </seasonal>\n");
			} else {
			   printf("\t\t\t<seasonal> %0.10e </seasonal>\n", value);
			}
	        value = rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_hw_last_seasonal].u_val;
			if (isnan(value)) {
			   printf("\t\t\t<last_seasonal> NaN </last_seasonal>\n");
			} else {
			   printf("\t\t\t<last_seasonal> %0.10e </last_seasonal>\n", value);
			}
	        ivalue = rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_init_seasonal].u_cnt;
			printf("\t\t\t<init_flag> %lu </init_flag>\n", ivalue);
			break;
		case CF_DEVPREDICT:
			break;
		case CF_FAILURES:
		    {
            unsigned short vidx;
			char *violations_array = (char *) ((void*) 
			   rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch);
			printf("\t\t\t<history> ");
			for (vidx = 0; vidx < rrd.rra_def[i].par[RRA_window_len].u_cnt; ++vidx)
			{
				printf("%d",violations_array[vidx]);
			}
			printf(" </history>\n");
			}
			break;
		case CF_AVERAGE:
		case CF_MAXIMUM:
		case CF_MINIMUM:
		case CF_LAST:
		default:
	        value = rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_val].u_val;
			if (isnan(value)) {
			   printf("\t\t\t<value> NaN </value>\n");
			} else {
			   printf("\t\t\t<value> %0.10e </value>\n", value);
			}
		    printf("\t\t\t<unknown_datapoints> %lu </unknown_datapoints>\n",
		       rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_unkn_pdp_cnt].u_cnt);
			break;
		}
        printf("\t\t\t</ds>\n");	 
    }
	printf("\t\t</cdp_prep>\n");

	printf("\t\t<database>\n");
	fseek(in_file,(rra_start
		       +(rrd.rra_ptr[i].cur_row+1)
		       * rrd.stat_head->ds_cnt
		       * sizeof(rrd_value_t)),SEEK_SET);
	timer = - (rrd.rra_def[i].row_cnt-1);
	ii=rrd.rra_ptr[i].cur_row;
	for(ix=0;ix<rrd.rra_def[i].row_cnt;ix++){	    
	    ii++;
	    if (ii>=rrd.rra_def[i].row_cnt) {
		fseek(in_file,rra_start,SEEK_SET);
		ii=0; /* wrap if max row cnt is reached */
	    }
	    now = (rrd.live_head->last_up 
		   - rrd.live_head->last_up 
		   % (rrd.rra_def[i].pdp_cnt*rrd.stat_head->pdp_step)) 
		+ (timer*rrd.rra_def[i].pdp_cnt*rrd.stat_head->pdp_step);

	    timer++;
#if HAVE_STRFTIME
	    localtime_r(&now, &tm);
	    strftime(somestring,200,"%Y-%m-%d %H:%M:%S %Z", &tm);
#else
# error "Need strftime"
#endif
	    printf("\t\t\t<!-- %s / %d --> <row>",somestring,(int)now);
	    for(iii=0;iii<rrd.stat_head->ds_cnt;iii++){			 
		fread(&my_cdp,sizeof(rrd_value_t),1,in_file);		
		if (isnan(my_cdp)){
		  printf("<v> NaN </v>");
		} else {
		  printf("<v> %0.10e </v>",my_cdp);
		};
	    }
	    printf("</row>\n");
	}
	printf("\t\t</database>\n\t</rra>\n");
	
    }
    printf("</rrd>\n");
    rrd_free(&rrd);
    fclose(in_file);
    return(0);
}




