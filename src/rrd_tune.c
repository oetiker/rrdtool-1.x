/*****************************************************************************
 * RRDtool 1.0.33  Copyright Tobias Oetiker, 1997 - 2000
 *****************************************************************************
 * change header parameters of an rrd
 *****************************************************************************
 * $Id$
 * $Log$
 * Revision 1.1  2001/02/25 22:25:06  oetiker
 * Initial revision
 *
 *****************************************************************************/

#include "rrd_tool.h"

int
rrd_tune(int argc, char **argv)    
{   
    rrd_t               rrd;
    FILE               *rrd_file;
    int                 matches;
    int                 optcnt = 0;
    long                ds;
    char                ds_nam[DS_NAM_SIZE];
    char                ds_new[DS_NAM_SIZE];
    long                heartbeat;
    double              min;
    double              max;
    char                dst[DST_SIZE];


    if(rrd_open(argv[1],&rrd_file,&rrd, RRD_READWRITE)==-1){
        return -1;
    }

    
    while (1){
	static struct option long_options[] =
	{
	    {"heartbeat",        required_argument, 0, 'h'},
	    {"minimum",          required_argument, 0, 'i'},
	    {"maximum",          required_argument, 0, 'a'},
	    {"data-source-type", required_argument, 0, 'd'},
	    {"data-source-rename", required_argument, 0, 'r'},
	    {0,0,0,0}
	};
	int option_index = 0;
	int opt;
	opt = getopt_long(argc, argv, "h:i:a:d:r:", 
			  long_options, &option_index);
	if (opt == EOF)
	    break;
	
	optcnt++;
	switch(opt) {	    
	case 'h':
	    if ((matches = sscanf(optarg, DS_NAM_FMT ":%ld",ds_nam,&heartbeat)) != 2){
		rrd_set_error("invalid arguments for heartbeat");
		rrd_free(&rrd);
	        fclose(rrd_file);
		return -1;
	    }
	    if ((ds=ds_match(&rrd,ds_nam))==-1){
		rrd_free(&rrd);
                fclose(rrd_file);
		return -1;
	    }
	    rrd.ds_def[ds].par[DS_mrhb_cnt].u_cnt = heartbeat;
	    break;

	case 'i':
	    if ((matches = sscanf(optarg,DS_NAM_FMT ":%lf",ds_nam,&min)) <1){
		rrd_set_error("invalid arguments for minimum ds value");
		rrd_free(&rrd);
                fclose(rrd_file);
		return -1;
	    }
	    if ((ds=ds_match(&rrd,ds_nam))==-1){
		rrd_free(&rrd);
                fclose(rrd_file);
		return -1;
	    }

	    if(matches == 1)
		min= DNAN;
	    rrd.ds_def[ds].par[DS_min_val].u_val = min;
	    break;

	case 'a':
	    if ((matches = sscanf(optarg, DS_NAM_FMT ":%lf",ds_nam,&max)) <1){
		rrd_set_error("invalid arguments for maximum ds value");
		rrd_free(&rrd);
	        fclose(rrd_file);
		return -1;
	    }
	    if ((ds=ds_match(&rrd,ds_nam))==-1){
		rrd_free(&rrd);
	        fclose(rrd_file);
		return -1;
	    }
	    if(matches == 1) 
		max= DNAN; 
	    rrd.ds_def[ds].par[DS_max_val].u_val = max;
	    break;

	case 'd':
	    if ((matches = sscanf(optarg, DS_NAM_FMT ":" DST_FMT ,ds_nam,dst)) != 2){
		rrd_set_error("invalid arguments for data source type");
		rrd_free(&rrd);
	        fclose(rrd_file);
		return -1;
	    }
	    if ((ds=ds_match(&rrd,ds_nam))==-1){
		rrd_free(&rrd);
	        fclose(rrd_file);
		return -1;
	    }
	    if (dst_conv(dst) == -1){
		rrd_free(&rrd);
	        fclose(rrd_file);
		return -1;
	    }
	    strncpy(rrd.ds_def[ds].dst,dst,DST_SIZE-1);
	    rrd.ds_def[ds].dst[DST_SIZE-1]='\0';

	    rrd.pdp_prep[ds].last_ds[0] = 'U';
	    rrd.pdp_prep[ds].last_ds[1] = 'N';
	    rrd.pdp_prep[ds].last_ds[2] = 'K';
	    rrd.pdp_prep[ds].last_ds[3] = 'N';
	    rrd.pdp_prep[ds].last_ds[4] = '\0';
	    
	    break;
	case 'r':
	    if ((matches = 
		 sscanf(optarg,DS_NAM_FMT ":" DS_NAM_FMT , ds_nam,ds_new)) != 2){
		rrd_set_error("invalid arguments for data source type");
		rrd_free(&rrd);
	        fclose(rrd_file);
		return -1;
	    }
	    if ((ds=ds_match(&rrd,ds_nam))==-1){
		rrd_free(&rrd);
	        fclose(rrd_file);
		return -1;
	    }
	    strncpy(rrd.ds_def[ds].ds_nam,ds_new,DS_NAM_SIZE-1);
	    rrd.ds_def[ds].ds_nam[DS_NAM_SIZE-1]='\0';
	    break;
	case '?':
            if (optopt != 0)
                rrd_set_error("unknown option '%c'", optopt);
            else
                rrd_set_error("unknown option '%s'",argv[optind-1]);
	    rrd_free(&rrd);	    
            fclose(rrd_file);
            return -1;
        }
    }
    if(optcnt>0){
	
	fseek(rrd_file,0,SEEK_SET);
	fwrite(rrd.stat_head,
	       sizeof(stat_head_t),1, rrd_file);
	fwrite(rrd.ds_def,
	       sizeof(ds_def_t), rrd.stat_head->ds_cnt, rrd_file);
    } else {
	int i;
	for(i=0;i< rrd.stat_head->ds_cnt;i++)
	    printf("DS[%s] typ: %s\thbt: %ld\tmin: %1.4f\tmax: %1.4f\n",
		   rrd.ds_def[i].ds_nam,
		   rrd.ds_def[i].dst,
		   rrd.ds_def[i].par[DS_mrhb_cnt].u_cnt,
		   rrd.ds_def[i].par[DS_min_val].u_val,
		   rrd.ds_def[i].par[DS_max_val].u_val);
    }
    fclose(rrd_file);
    rrd_free(&rrd);
    return 0;
}

