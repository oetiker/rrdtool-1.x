/*****************************************************************************
 * RRDtool 1.2rc5  Copyright by Tobi Oetiker, 1997-2005
 *****************************************************************************
 * rrd_stat Retreive the header part of an RRD
 *****************************************************************************/

#include "rrd_tool.h"

extern char *tzname[2];

stat_node
rrd_stat(int argc, char **argv)    
{   
    int          i,ii,ix,iii=0;
    time_t       now;
    char         somestring[255];
    rrd_value_t  my_cdp;
    long         rra_base, rra_start, rra_next;
    FILE                  *in_file;
    rrd_t             rrd;


    if(rrd_open(argv[1],&in_file,&rrd, RRD_READONLY)==-1){
	return(-1);
    }
    puts("<!-- Round Robin Database Dump -->");
    puts("<rrd>");
    printf("\t<version> %s </version>\n",rrd.stat_head->version);
    printf("\t<step> %lu </step> <!-- Seconds -->\n",rrd.stat_head->pdp_step);
#if HAVE_STRFTIME
    strftime(somestring,200,"%Y-%m-%d %H:%M:%S %Z",
	     localtime_r(&rrd.live_head->last_up, &tm));
#else
# error "Need strftime"
#endif
    printf("\t<lastupdate> %ld </lastupdate> <!-- %s -->\n\n",
	   rrd.live_head->last_up,somestring);
    for(i=0;i<rrd.stat_head->ds_cnt;i++){
	printf("\t<ds>\n");
	printf("\t\t<name> %s </name>\n",rrd.ds_def[i].ds_nam);
	printf("\t\t<type> %s </type>\n",rrd.ds_def[i].dst);
	printf("\t\t<minimal_heartbeat> %lu </minimal_heartbeat>\n",
	       rrd.ds_def[i].par[DS_mrhb_cnt].u_cnt);
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
	printf("\t\t<cdp_prep>\n");
	for(ii=0;ii<rrd.stat_head->ds_cnt;ii++){
	    double value = rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_val].u_val;
	    printf("\t\t\t<ds>");	
	    if (isnan(value)){
	      printf("<value> NaN </value>");
	    } else {
	      printf("<value> %0.10e </value>", value);
	    }
	    printf("  <unknown_datapoints> %lu </unknown_datapoints>",
                   rrd.cdp_prep[i*rrd.stat_head->ds_cnt+ii].scratch[CDP_unkn_pdp_cnt].u_cnt);
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
	    strftime(somestring,200,"%Y-%m-%d %H:%M:%S %Z", localtime_r(&now, &tm));
#else
# error "Need strftime"
#endif
	    printf("\t\t\t<!-- %s --> <row>",somestring);
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




