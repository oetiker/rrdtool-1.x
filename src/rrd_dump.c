/*****************************************************************************
 * RRDtool 1.0.33  Copyright Tobias Oetiker, 1997 - 2000
 *****************************************************************************
 * rrd_dump  Display a RRD
 *****************************************************************************
 * $Id$
 * $Log$
 * Revision 1.2  2001/03/04 13:01:55  oetiker
 * Aberrant Behavior Detection support. A brief overview added to rrdtool.pod.
 * Major updates to rrd_update.c, rrd_create.c. Minor update to other core files.
 * This is backwards compatible! But new files using the Aberrant stuff are not readable
 * by old rrdtool versions. See http://cricket.sourceforge.net/aberrant/rrd_hw.htm
 * -- Jake Brutlag <jakeb@corp.webtv.net>
 *
 * Revision 1.1.1.1  2001/02/25 22:25:05  oetiker
 * checkin
 *
 *****************************************************************************/

#include "rrd_tool.h"

extern char *tzname[2];

int
rrd_dump(int argc, char **argv)    
{   
    int          i,ii,ix,iii=0;
    time_t       now;
    char         somestring[255];
    rrd_value_t  my_cdp;
    long         rra_base, rra_start, rra_next;
    FILE                  *in_file;
    rrd_t             rrd;
    unival value;

    if(rrd_open(argv[1],&in_file,&rrd, RRD_READONLY)==-1){
	return(-1);
    }

    puts("<!-- Round Robin Database Dump -->");
    puts("<rrd>");
    printf("\t<version> %s </version>\n",RRD_VERSION);
    printf("\t<step> %lu </step> <!-- Seconds -->\n",rrd.stat_head->pdp_step);
#if HAVE_STRFTIME
    strftime(somestring,200,"%Y-%m-%d %H:%M:%S %Z",
	     localtime(&rrd.live_head->last_up));
#else
# error "Need strftime"
#endif
    printf("\t<lastupdate> %ld </lastupdate> <!-- %s -->\n\n",
	   rrd.live_head->last_up,somestring);
    for(i=0;i<rrd.stat_head->ds_cnt;i++){
	printf("\t<ds>\n");
	printf("\t\t<name> %s </name>\n",rrd.ds_def[i].ds_nam);
	printf("\t\t<type> %s </type>\n",rrd.ds_def[i].dst);
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
	/* added support for RRA parameters */
	printf("\t\t<params>");
	for (ii = 0; ii < MAX_RRA_PAR_EN; ii++)
	{
	   value = rrd.rra_def[i].par[ii];
	   if (ii == RRA_dependent_rra_idx ||
		   ii == RRA_seasonal_smooth_idx ||
		   ii == RRA_failure_threshold)
	          printf("<value> %lu </value>", value.u_cnt);
	   else {
	          if (isnan(value.u_val)) {
	              printf("<value> NaN </value>");
	          } else {
	              printf("<value> %0.10e </value>", value.u_val);
	          }
	   }
	}
	printf("\t\t</params>\n");
	printf("\t\t<cdp_prep>\n");
	for(ii=0;ii<rrd.stat_head->ds_cnt;ii++){
#if 0
	        double value = rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_val].u_val;
#endif 
		printf("\t\t\t<ds>");
		/* added support for exporting all CDP parameters */
		for (iii=0; iii < MAX_CDP_PAR_EN ; iii++)
		{
		   value = rrd.cdp_prep[i*rrd.stat_head->ds_cnt + ii].scratch[iii];
           /* handle integer values as a special case */
		   if (cf_conv(rrd.rra_def[i].cf_nam) == CF_FAILURES ||
			   iii == CDP_unkn_pdp_cnt || 
		       iii == CDP_null_count ||
		       iii == CDP_last_null_count)
		     printf("<value> %lu </value>", value.u_cnt);
		   else {	
		     if (isnan(value.u_val)) {
		       printf("<value> NaN </value>");
		     } else {
		       printf("<value> %0.10e </value>", value.u_val);
		     }
		   }
		}
#if 0 
		printf("  <unknown_datapoints> %lu </unknown_datapoints>",
		       rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_unkn_pdp_cnt].u_cnt);
#endif
        printf("</ds>\n");	 
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
	    strftime(somestring,200,"%Y-%m-%d %H:%M:%S %Z", localtime(&now));
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




