/****************************************************************************
 * RRDtool 1.2.19  Copyright by Tobi Oetiker, 1997-2007
 ****************************************************************************
 * rrd_xport.c  export RRD data 
 ****************************************************************************/

#include <sys/stat.h>

#include "rrd_tool.h"
#include "rrd_graph.h"
#include "rrd_xport.h"
#include "unused.h"

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)
#include <io.h>
#include <fcntl.h>
#endif


int rrd_xport(int, char **, int *,
	      time_t *, time_t *,
	      unsigned long *, unsigned long *,
	      char ***, rrd_value_t **);

int rrd_xport_fn(image_desc_t *,
		 time_t *, time_t *,
		 unsigned long *, unsigned long *,
		 char ***, rrd_value_t **);




int 
rrd_xport(int argc, char **argv, int UNUSED(*xsize),
	  time_t         *start,
	  time_t         *end,        /* which time frame do you want ?
				       * will be changed to represent reality */
	  unsigned long  *step,       /* which stepsize do you want? 
				       * will be changed to represent reality */
	  unsigned long  *col_cnt,    /* number of data columns in the result */
	  char           ***legend_v, /* legend entries */
	  rrd_value_t    **data)      /* two dimensional array containing the data */

{

    image_desc_t   im;
    time_t	   start_tmp=0,end_tmp=0;
    struct rrd_time_value start_tv, end_tv;
    char           *parsetime_error = NULL;
    optind = 0; opterr = 0;  /* initialize getopt */

    rrd_graph_init(&im);

    parsetime("end-24h", &start_tv);
    parsetime("now", &end_tv);

    while (1){
	static struct option long_options[] =
	{
	    {"start",      required_argument, 0,  's'},
	    {"end",        required_argument, 0,  'e'},
	    {"maxrows",    required_argument, 0,  'm'},
	    {"step",       required_argument, 0,   261},
	    {"enumds",     no_argument,       0,   262}, /* these are handled in the frontend ... */
	    {0,0,0,0}
	};
	int option_index = 0;
	int opt;
	
	opt = getopt_long(argc, argv, "s:e:m:",
			  long_options, &option_index);

	if (opt == EOF)
	    break;
	
	switch(opt) {
	case 261:
	    im.step =  atoi(optarg);
	    break;
	case 262:
	    break;
	case 's':
	    if ((parsetime_error = parsetime(optarg, &start_tv))) {
	        rrd_set_error( "start time: %s", parsetime_error );
		return -1;
	    }
	    break;
	case 'e':
	    if ((parsetime_error = parsetime(optarg, &end_tv))) {
	        rrd_set_error( "end time: %s", parsetime_error );
		return -1;
	    }
	    break;
	case 'm':
	    im.xsize = atol(optarg);
	    if (im.xsize < 10) {
		rrd_set_error("maxrows below 10 rows");
		return -1;
	    }
	    break;
	case '?':
            rrd_set_error("unknown option '%s'",argv[optind-1]);
            return -1;
	}
    }

    if (proc_start_end(&start_tv,&end_tv,&start_tmp,&end_tmp) == -1){
	return -1;
    }  
    
    if (start_tmp < 3600*24*365*10){
	rrd_set_error("the first entry to fetch should be after 1980 (%ld)",start_tmp);
	return -1;
    }
    
    if (end_tmp < start_tmp) {
	rrd_set_error("start (%ld) should be less than end (%ld)", 
	       start_tmp, end_tmp);
	return -1;
    }
    
    im.start = start_tmp;
    im.end = end_tmp;
    im.step = max((long)im.step, (im.end-im.start)/im.xsize);
    
    rrd_graph_script(argc,argv,&im,0);
    if (rrd_test_error()) {
	im_free(&im);
	return -1;
    }

    if (im.gdes_c == 0){
	rrd_set_error("can't make a graph without contents");
	im_free(&im);
	return(-1); 
    }
    
    if (rrd_xport_fn(&im, start, end, step, col_cnt, legend_v, data) == -1){
	im_free(&im);
	return -1;
    }

    im_free(&im);
    return 0;
}



int
rrd_xport_fn(image_desc_t *im,
	     time_t         *start,
	     time_t         *end,        /* which time frame do you want ?
					  * will be changed to represent reality */
	     unsigned long  *step,       /* which stepsize do you want? 
					  * will be changed to represent reality */
	     unsigned long  *col_cnt,    /* number of data columns in the result */
	     char           ***legend_v, /* legend entries */
	     rrd_value_t    **data)      /* two dimensional array containing the data */
{

    int            i = 0, j = 0;
    unsigned long  *ds_cnt;    /* number of data sources in file */
    unsigned long  col, dst_row, row_cnt;
    rrd_value_t    *srcptr, *dstptr;

    unsigned long nof_xports = 0;
    unsigned long xport_counter = 0;
    unsigned long *ref_list;
    rrd_value_t **srcptr_list;
    char **legend_list;
    int ii = 0;

    time_t start_tmp = 0;
    time_t end_tmp = 0;
    unsigned long step_tmp = 1;

    /* pull the data from the rrd files ... */
    if(data_fetch(im)==-1)
	return -1;

    /* evaluate CDEF  operations ... */
    if(data_calc(im)==-1)
	return -1;

    /* how many xports? */
    for(i = 0; i < im->gdes_c; i++) {	
	switch(im->gdes[i].gf) {
	case GF_XPORT:
	  nof_xports++;
	  break;
	default:
	  break;
	}
    }

    if(nof_xports == 0) {
      rrd_set_error("no XPORT found, nothing to do");
      return -1;
    }

    /* a list of referenced gdes */
    ref_list = malloc(sizeof(int) * nof_xports);
    if(ref_list == NULL)
      return -1;

    /* a list to save pointers into each gdes data */
    srcptr_list = malloc(sizeof(srcptr) * nof_xports);
    if(srcptr_list == NULL) {
      free(ref_list);
      return -1;
    }

    /* a list to save pointers to the column's legend entry */
    /* this is a return value! */
    legend_list = malloc(sizeof(char *) * nof_xports);
    if(legend_list == NULL) {
      free(srcptr_list);
      free(ref_list);
      return -1;
    }

    /* find referenced gdes and save their index and */
    /* a pointer into their data */
    for(i = 0; i < im->gdes_c; i++) {	
	switch(im->gdes[i].gf) {
	case GF_XPORT:
	  ii = im->gdes[i].vidx;
	  if(xport_counter > nof_xports) {
	    rrd_set_error( "too many xports: should not happen. Hmmm");
	    free(srcptr_list);
	    free(ref_list);
	    free(legend_list);
	    return -1;
	  } 
	  srcptr_list[xport_counter] = im->gdes[ii].data;
	  ref_list[xport_counter++] = i;
	  break;
	default:
	  break;
	}
    }

    start_tmp = im->gdes[0].start;
    end_tmp = im->gdes[0].end;
    step_tmp = im->gdes[0].step;

    /* fill some return values */
    *col_cnt = nof_xports;
    *start = start_tmp;
    *end = end_tmp;
    *step = step_tmp;

    row_cnt = ((*end)-(*start))/(*step);

    /* room for rearranged data */
    /* this is a return value! */
    if (((*data) = malloc((*col_cnt) * row_cnt * sizeof(rrd_value_t)))==NULL){
        free(srcptr_list);
        free(ref_list);
	free(legend_list);
	rrd_set_error("malloc xport data area");
	return(-1);
    }
    dstptr = (*data);

    j = 0;
    for(i = 0; i < im->gdes_c; i++) {	
	switch(im->gdes[i].gf) {
	case GF_XPORT:
	  /* reserve room for one legend entry */
	  /* is FMT_LEG_LEN + 5 the correct size? */
	  if ((legend_list[j] = malloc(sizeof(char) * (FMT_LEG_LEN+5)))==NULL) {
	    free(srcptr_list);
	    free(ref_list);
	    free(*data);  *data = NULL;
	    while (--j > -1) free(legend_list[j]);
	    free(legend_list);
	    rrd_set_error("malloc xport legend entry");
	    return(-1);
	  }

	  if (im->gdes[i].legend)
	    /* omit bounds check, should have the same size */
	    strcpy (legend_list[j++], im->gdes[i].legend);
	  else
	    legend_list[j++][0] = '\0';

	  break;
	default:
	  break;
	}
    }

    /* fill data structure */
    for(dst_row = 0; (int)dst_row < (int)row_cnt; dst_row++) {
      for(i = 0; i < (int)nof_xports; i++) {
        j = ref_list[i];
	ii = im->gdes[j].vidx;
	ds_cnt = &im->gdes[ii].ds_cnt;

	srcptr = srcptr_list[i];
	for(col = 0; col < (*ds_cnt); col++) {
	  rrd_value_t newval = DNAN;
	  newval = srcptr[col];

	  if (im->gdes[ii].ds_namv && im->gdes[ii].ds_nam) {
	    if(strcmp(im->gdes[ii].ds_namv[col],im->gdes[ii].ds_nam) == 0)
	      (*dstptr++) = newval;
	  } else {
	    (*dstptr++) = newval;
	  }

	}
	srcptr_list[i] += (*ds_cnt);
      }
    }

    *legend_v = legend_list;
    free(srcptr_list);
    free(ref_list);
    return 0;

}
