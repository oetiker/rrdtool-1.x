/****************************************************************************
 * RRDtool 1.4.3  Copyright by Tobi Oetiker, 1997-2010
 ****************************************************************************
 * rrd_xport.c  export RRD data 
 ****************************************************************************/

#include <sys/stat.h>
#include <locale.h>

#include "rrd_tool.h"
#include "rrd_graph.h"
#include "rrd_xport.h"
#include "unused.h"
#include "rrd_client.h"

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)
#include <io.h>
#include <fcntl.h>
#endif

int       rrd_xport(
    int,
    char **,
    int *,
    time_t *,
    time_t *,
    unsigned long *,
    unsigned long *,
    char ***,
    rrd_value_t **);

int       rrd_xport_fn(
    image_desc_t *,
    time_t *,
    time_t *,
    unsigned long *,
    unsigned long *,
    char ***,
    rrd_value_t **,
    int);

/* helper function for buffer handling */
typedef struct stringbuffer_t {
  size_t allocated;
  size_t len;
  unsigned char* data;
  FILE *file;
} stringbuffer_t;
int addToBuffer(stringbuffer_t *,char*,size_t);
void escapeJSON(char*,size_t);

int rrd_graph_xport(image_desc_t *);
int rrd_xport_format_xmljson(int,stringbuffer_t *,image_desc_t*,time_t, time_t, unsigned long, unsigned long, char**, rrd_value_t*);
int rrd_xport_format_sv(char,stringbuffer_t *,image_desc_t*,time_t, time_t, unsigned long, unsigned long, char**, rrd_value_t*);
int rrd_xport_format_addprints(int,stringbuffer_t *,image_desc_t *);

int rrd_xport(
    int argc,
    char **argv,
    int UNUSED(*xsize),
    time_t *start,
    time_t *end,        /* which time frame do you want ?
                         * will be changed to represent reality */
    unsigned long *step,    /* which stepsize do you want? 
                             * will be changed to represent reality */
    unsigned long *col_cnt, /* number of data columns in the result */
    char ***legend_v,   /* legend entries */
    rrd_value_t **data)
{                       /* two dimensional array containing the data */
    image_desc_t im;
    time_t    start_tmp = 0, end_tmp = 0;
    rrd_time_value_t start_tv, end_tv;
    char     *parsetime_error = NULL;

    struct option long_options[] = {
        {"start", required_argument, 0, 's'},
        {"end", required_argument, 0, 'e'},
        {"maxrows", required_argument, 0, 'm'},
        {"step", required_argument, 0, 261},
        {"enumds", no_argument, 0, 262},    /* these are handled in the frontend ... */
        {"json", no_argument, 0, 263},    /* these are handled in the frontend ... */
        {"daemon", required_argument, 0, 'd'},
        {0, 0, 0, 0}
    };

    optind = 0;
    opterr = 0;         /* initialize getopt */

    rrd_graph_init(&im);

    rrd_parsetime("end-24h", &start_tv);
    rrd_parsetime("now", &end_tv);

    int enumds=0;
    int json=0;

    while (1) {
        int       option_index = 0;
        int       opt;

        opt = getopt_long(argc, argv, "s:e:m:d:", long_options, &option_index);

        if (opt == EOF)
            break;

        switch (opt) {
        case 261:
            im.step = atoi(optarg);
            break;
        case 262:
  	    enumds=1;
            break;
        case 263:
  	    json=1;
            break;
        case 's':
            if ((parsetime_error = rrd_parsetime(optarg, &start_tv))) {
                rrd_set_error("start time: %s", parsetime_error);
                return -1;
            }
            break;
        case 'e':
            if ((parsetime_error = rrd_parsetime(optarg, &end_tv))) {
                rrd_set_error("end time: %s", parsetime_error);
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
        case 'd':
        {
            if (im.daemon_addr != NULL)
            {
                rrd_set_error ("You cannot specify --daemon "
                        "more than once.");
                return (-1);
            }

            im.daemon_addr = strdup(optarg);
            if (im.daemon_addr == NULL)
            {
                rrd_set_error("strdup error");
                return -1;
            }
            break;
        }

        case '?':
            rrd_set_error("unknown option '%s'", argv[optind - 1]);
            return -1;
        }
    }

    if (rrd_proc_start_end(&start_tv, &end_tv, &start_tmp, &end_tmp) == -1) {
        return -1;
    }

    if (start_tmp < 3600 * 24 * 365 * 10) {
        rrd_set_error("the first entry to fetch should be after 1980 (%ld)",
                      start_tmp);
        return -1;
    }

    if (end_tmp < start_tmp) {
        rrd_set_error("start (%ld) should be less than end (%ld)",
                      start_tmp, end_tmp);
        return -1;
    }

    im.start = start_tmp;
    im.end = end_tmp;
    im.step = max((long) im.step, (im.end - im.start) / im.xsize);

    rrd_graph_script(argc, argv, &im, 0);
    if (rrd_test_error()) {
        im_free(&im);
        return -1;
    }

    if (im.gdes_c == 0) {
        rrd_set_error("can't make an xport without contents");
        im_free(&im);
        return (-1);
    }

    {   /* try to connect to rrdcached */
        int status = rrdc_connect(im.daemon_addr);
        if (status != 0) return status;
    }

    if (rrd_xport_fn(&im, start, end, step, col_cnt, legend_v, data,0) == -1) {
        im_free(&im);
        return -1;
    }

    /* and create the export */
    if (!xsize) {
      int flags=0;
      if (json) { flags|=1; }
      if (enumds) { flags|=4; }
      stringbuffer_t buffer={0,0,NULL,stdout};
      rrd_xport_format_xmljson(flags,&buffer,&im, 
			       *start, *end, *step,
			       *col_cnt, *legend_v,
			       *data);
    }

    im_free(&im);
    return 0;
}



int rrd_xport_fn(
    image_desc_t *im,
    time_t *start,
    time_t *end,        /* which time frame do you want ?
                         * will be changed to represent reality */
    unsigned long *step,    /* which stepsize do you want? 
                             * will be changed to represent reality */
    unsigned long *col_cnt, /* number of data columns in the result */
    char ***legend_v,   /* legend entries */
    rrd_value_t **data,
    int dolines)
{                       /* two dimensional array containing the data */

    int       i = 0, j = 0;
    unsigned long dst_row, row_cnt;
    rrd_value_t  *dstptr;

    unsigned long xport_counter = 0;
    int      *ref_list;
    long     *step_list;
    long     *step_list_ptr;    
    char    **legend_list;


    /* pull the data from the rrd files ... */
    if (data_fetch(im) == -1)
        return -1;

    /* evaluate CDEF  operations ... */
    if (data_calc(im) == -1)
        return -1;

    /* how many xports or lines/AREA/STACK ? */
    *col_cnt = 0;
    for (i = 0; i < im->gdes_c; i++) {
        switch (im->gdes[i].gf) {
        case GF_LINE:
        case GF_AREA:
        case GF_STACK:
	  (*col_cnt)+=dolines;
	  break;
        case GF_XPORT:
	  (*col_cnt)++;
            break;
        default:
            break;
        }
    }
    if ((*col_cnt) == 0) {
        rrd_set_error("no XPORT found, nothing to do");
        return -1;
    }

    /* a list of referenced gdes */
    ref_list = (int*)malloc(sizeof(int) * (*col_cnt));
    if (ref_list == NULL)
        return -1;

    /* a list to save pointers to the column's legend entry */
    /* this is a return value! */
    legend_list = (char**)malloc(sizeof(char *) * (*col_cnt));
    if (legend_list == NULL) {
        free(ref_list);
        return -1;
    }

    /* lets find the step size we have to use for xport */
    step_list = (long*)malloc(sizeof(long)*((*col_cnt)+1));
    step_list_ptr = step_list;
    j = 0;
    for (i = 0; i < im->gdes_c; i++) {
      /* decide if we need to handle the output */
        int handle=0;
        switch (im->gdes[i].gf) {
        case GF_LINE:
        case GF_AREA:
        case GF_STACK:
	  handle=dolines;
	  break;
        case GF_XPORT:
	  handle=1;
	  break;
	default:
	  handle=0;
	  break;
	}
	/* and now do the real work */
	if (handle) {
            ref_list[xport_counter++] = i;
            *step_list_ptr = im->gdes[im->gdes[i].vidx].step;
            /* printf("%s:%lu\n",im->gdes[i].legend,*step_list_ptr); */
            step_list_ptr++;
            /* reserve room for one legend entry */
            /* is FMT_LEG_LEN + 5 the correct size? */
            if ((legend_list[j] =
                (char*)malloc(sizeof(char) * (FMT_LEG_LEN + 5))) == NULL) {
                free(ref_list);
                *data = NULL;
                while (--j > -1)
                    free(legend_list[j]);
                free(legend_list);
                free(step_list);
                rrd_set_error("malloc xport legend entry");
                return (-1);
            }

            if (im->gdes[i].legend)
                /* omit bounds check, should have the same size */
                strcpy(legend_list[j++], im->gdes[i].legend);
            else
                legend_list[j++][0] = '\0';
	}
    }
    *step_list_ptr=0;    
    /* find a common step */
    *step = lcd(step_list);
    /* printf("step: %lu\n",*step); */
    free(step_list);
    
    *start =  im->start - im->start % (*step);
    *end = im->end - im->end % (*step) + (*step);
    

    /* room for rearranged data */
    /* this is a return value! */
    row_cnt = ((*end) - (*start)) / (*step);
    if (((*data) =
        (rrd_value_t*)malloc((*col_cnt) * row_cnt * sizeof(rrd_value_t))) == NULL) {
        free(ref_list);
        free(legend_list);
        rrd_set_error("malloc xport data area");
        return (-1);
    }
    dstptr = (*data);

    /* fill data structure */
    for (dst_row = 0; (int) dst_row < (int) row_cnt; dst_row++) {
        for (i = 0; i < (int) (*col_cnt); i++) {
            long vidx = im->gdes[ref_list[i]].vidx;
            time_t now = *start + dst_row * *step;
            (*dstptr++) = im->gdes[vidx].data[(unsigned long)
                                              floor((double)
                                                    (now - im->gdes[vidx].start)
                                                    /im->gdes[vidx].step)
                                              * im->gdes[vidx].ds_cnt +
                                              im->gdes[vidx].ds];

        }
    }

    *legend_v = legend_list;
    free(ref_list);
    return 0;

}

int rrd_graph_xport(image_desc_t *im) {
  /* prepare the data for processing */
  unsigned long col_cnt=0;
  time_t start=im->start;
  time_t end=im->end;
  unsigned long step=im->step;
  char **legend_v=NULL;
  rrd_value_t *data=NULL;
  /* initialize buffer */
  stringbuffer_t buffer={0,0,NULL,NULL}; 

  /* check if we have a supported ggraph format */
  switch (im->graph_type) {
    /* allow the following to pass */
  case GTYPE_TIME:
  case GTYPE_XY:
    break;
  default:
    rrd_set_error("Not supported graph type");
    return -1;
  }

  /* if we write a file, then open it */
  if (strlen(im->graphfile)) {
    buffer.file=fopen(im->graphfile,"w");
  }

  /* do the data processing */
  if (rrd_xport_fn(im,&start,&end,&step,&col_cnt,&legend_v,&data,1)) { return -1;}

  /* fill in some data */
  rrd_infoval_t info;
  info.u_cnt = start;
  grinfo_push(im, sprintf_alloc("graph_start"), RD_I_CNT, info);
  info.u_cnt = end;
  grinfo_push(im, sprintf_alloc("graph_end"), RD_I_CNT, info);
  info.u_cnt = step;
  grinfo_push(im, sprintf_alloc("graph_step"), RD_I_CNT, info);

  /* set locale */
  char *old_locale = setlocale(LC_NUMERIC,NULL);
  setlocale(LC_NUMERIC, "C");

  /* format it for output */
  int r=0;
  switch(im->imgformat) {
  case IF_XML:
    r=rrd_xport_format_xmljson(2,&buffer,im, start, end, step, col_cnt, legend_v, data);
    break;
  case IF_XMLENUM:
    r=rrd_xport_format_xmljson(6,&buffer,im, start, end, step, col_cnt, legend_v, data);
    break;
  case IF_JSON:
    r=rrd_xport_format_xmljson(1,&buffer,im, start, end, step, col_cnt, legend_v, data);
    break;
  case IF_JSONTIME:
    r=rrd_xport_format_xmljson(3,&buffer,im, start, end, step, col_cnt, legend_v, data);
    break;
  case IF_CSV:
    r=rrd_xport_format_sv(',',&buffer,im, start, end, step, col_cnt, legend_v, data);
    break;
  case IF_TSV:
    r=rrd_xport_format_sv('\t',&buffer,im, start, end, step, col_cnt, legend_v, data);
    break;
  case IF_SSV:
    r=rrd_xport_format_sv(';',&buffer,im, start, end, step, col_cnt, legend_v, data);
    break;
  default:
    break;
  }
  /* restore locale */
  setlocale(LC_NUMERIC, old_locale);
  /* handle errors */
  if (r) {
    /* free legend */
    for (unsigned long j = 0; j < col_cnt; j++) {
      free(legend_v[j]);
    }
    free(legend_v);
    /* free data */
    free(data);
    /* free the bufer */
    if (buffer.data) {free(buffer.data);}
    /* close the file */
    if (buffer.file) {fclose(buffer.file);}
    /* and return with error */
    return r;
  }

  /* now do the cleanup */
  if (buffer.file) {
    fclose(buffer.file); buffer.file=NULL; 
    im->rendered_image_size=0;
    im->rendered_image=NULL;
  } else {
    im->rendered_image_size=buffer.len;
    im->rendered_image=buffer.data;    
  }

  /* and print stuff */
  return print_calc(im);
}

int addToBuffer(stringbuffer_t * sb,char* data,size_t len) {
  /* if len <= 0  we assume a string and calculate the length ourself */
  if (len<=0) { len=strlen(data); }
  /* if we have got a file, then take the shortcut */
  if (sb->file) { 
    sb->len+=len;
    fwrite(data,len,1,sb->file); 
    return 0; 
  }
  /* if buffer is 0, then initialize */
  if (! sb->data) { 
    /* make buffer a multiple of 8192 */
    sb->allocated+=8192;
    sb->allocated-=(sb->allocated%8192);    
    /* and allocate it */
    sb->data=malloc(sb->allocated); 
    if (! sb->data) { 
      rrd_set_error("malloc issue");
      return 1;
    }
    /* and initialize the buffer */
    sb->len=0;
    sb->data[0]=0;
  }
  /* and figure out if we need to extend the buffer */
  if (sb->len+len+1>=sb->allocated) {
    /* add so many pages until we have a buffer big enough */
    while(sb->len+len+1>=sb->allocated) {
      sb->allocated+=8192;
    }
    /* try to resize it */
    unsigned char* resized=(unsigned char*)realloc(sb->data,sb->allocated);
    if (resized) {
      sb->data=resized;
    } else {
      free(sb->data);
      sb->data=NULL;
      sb->allocated=0;
      rrd_set_error("realloc issue");
      return -1;
    }
  }
  /* and finally add to the buffer */
  memcpy(sb->data+sb->len,data,len);
  sb->len+=len;
  /* and 0 terminate it */
  sb->data[sb->len]=0;
  /* and return */
  return 0;
}

int rrd_xport_format_sv(char sep, stringbuffer_t *buffer,image_desc_t *im,time_t start, time_t end, unsigned long step, unsigned long col_cnt, char **legend_v, rrd_value_t* data) {
  /* define the time format */
  char* timefmt=NULL;
  if (im->xlab_user.minsec!=-1) { timefmt=im->xlab_user.stst; }

  /* row count */
  unsigned long row_cnt=(end-start)/step;

  /* estimate buffer size (to avoid multiple allocations) */
  buffer->allocated=
    1024 /* bytes of overhead /header/footer */
    +(12+19*col_cnt) /* 12 bytes overhead/line plus 19 bytes per column*/
    *(1+row_cnt) /* number of columns + 1 (for header) */
    ;

  char buf[256];

  /* now start writing the header*/
  if (addToBuffer(buffer,"\"time\"",6)) { return 1; }
  for(unsigned long i=0;i<col_cnt;i++) {
    /* strip leading spaces */
    char *t=legend_v[i]; while (isspace(*t)) { t++;}
    /* and print it */
    snprintf(buf,255,"%c\"%s\"",sep,t);
    if (addToBuffer(buffer,buf,0)) { return 1;}
  }
  if (addToBuffer(buffer,"\r\n",2)) { return 1; }
  /* and now write the data */
  rrd_value_t *ptr=data;
  for(time_t ti=start+step;ti<end;ti+=step) {
    /* write time */
    if (timefmt) {
      struct tm loc;
      localtime_r(&ti,&loc);
      strftime(buf,254,timefmt,&loc);
    } else {
      snprintf(buf,254,"%lld",(long long int)ti);
    }
    if (addToBuffer(buffer,buf,0)) { return 1; }
    /* write the columns */
    for(unsigned long i=0;i<col_cnt;i++) {
      /* get the value */
      rrd_value_t v=*ptr;ptr++;
      /* and print it */
      if (isnan(v)) {
	snprintf(buf,255,"%c\"NaN\"",sep);
      } else {
	snprintf(buf,255,"%c\"%0.10e\"",sep,v);
      }
      if (addToBuffer(buffer,buf,0)) { return 1;}
    }
    /* and add a newline */
    if (addToBuffer(buffer,"\r\n",2)) { return 1; }
  }

  /* and return OK */
  return 0;
}

int rrd_xport_format_xmljson(int flags,stringbuffer_t *buffer,image_desc_t *im,time_t start, time_t end, unsigned long step, unsigned long col_cnt, char **legend_v, rrd_value_t* data) {

  /* define some other stuff based on flags */
  int json=0;
  if (flags &1) { json=1; }
  int showtime=0;
  if (flags &2) { showtime=1;}
  int enumds=0;
  if (flags &4) { enumds=1;}

  /* define the time format */
  char* timefmt=NULL;
  /* unfortunatley we have to do it this way, 
     as when no --x-graph argument is given,
     then the xlab_user is not in a clean state (e.g. zero-filled) */
  if (im->xlab_user.minsec!=-1) { timefmt=im->xlab_user.stst; }

  /* row count */
  unsigned long row_cnt=(end-start)/step;

  /* estimate buffer size (to avoid multiple allocations) */
  /* better estimates are needed here */
  buffer->allocated=
    1024 /* bytes of overhead /header/footer */
    +(12+19*col_cnt) /* 12 bytes overhead/line plus 19 bytes per column*/
    *(1+row_cnt) /* number of columns + 1 (for header) */
    ;
  char buf[256];
  char dbuf[1024];

  rrd_value_t *ptr = data;
  if (json == 0){
    snprintf(buf,sizeof(buf),
	     "<?xml version=\"1.0\" encoding=\"%s\"?>\n\n<%s>\n  <%s>\n",
	     XML_ENCODING,ROOT_TAG,META_TAG);
    addToBuffer(buffer,buf,0);
  }
  else {
    addToBuffer(buffer,"{ \"about\": \"RRDtool graph JSON output\",\n  \"meta\": {\n",0);
  }
  
  /* calculate start time */
  if (timefmt) {
    struct tm loc;
    time_t ti=start+step;
    localtime_r(&ti,&loc);
    strftime(dbuf,sizeof(dbuf),timefmt,&loc);
    if (json) {
      snprintf(buf,sizeof(buf),"    \"%s\": \"%s\",\n",META_START_TAG,dbuf);
    } else {
      snprintf(buf,sizeof(buf),"    <%s>%s</%s>\n",META_START_TAG,dbuf,META_START_TAG);
    }
  } else {
    if (json) {
      snprintf(buf,sizeof(buf),"    \"%s\": %lld,\n",META_START_TAG,(long long int)start+step);
    } else {
      snprintf(buf,sizeof(buf),"    <%s>%lld</%s>\n",META_START_TAG,(long long int)start+step,META_START_TAG);
    }
  }
  addToBuffer(buffer,buf,0);

  /* calculate end time */
  if (timefmt) {
    struct tm loc;
    time_t ti=end;
    localtime_r(&ti,&loc);
    strftime(dbuf,sizeof(dbuf),timefmt,&loc);
    if (json) {
      snprintf(buf,sizeof(buf),"    \"%s\": \"%s\",\n",META_END_TAG,dbuf);
    } else {
      snprintf(buf,sizeof(buf),"    <%s>%s</%s>\n",META_END_TAG,dbuf,META_END_TAG);
    }
  } else {
    if (json) {
      snprintf(buf,sizeof(buf),"    \"%s\": %lld,\n",META_END_TAG,(long long int)end);
    } else {
      snprintf(buf,sizeof(buf),"    <%s>%lld</%s>\n",META_END_TAG,(long long int)end,META_END_TAG);
    }
  }
  addToBuffer(buffer,buf,0);
  /* print other info */
  if (json) {
    snprintf(buf,sizeof(buf),"    \"%s\": %lld,\n",META_STEP_TAG,(long long int)step);
    addToBuffer(buffer,buf,0);
  } else {
    snprintf(buf,sizeof(buf),"    <%s>%lld</%s>\n",META_STEP_TAG,(long long int)step,META_STEP_TAG); 
    addToBuffer(buffer,buf,0);
    snprintf(buf,sizeof(buf),"    <%s>%lu</%s>\n",META_ROWS_TAG,row_cnt,META_ROWS_TAG); 
    addToBuffer(buffer,buf,0);
    snprintf(buf,sizeof(buf),"    <%s>%lu</%s>\n",META_COLS_TAG,col_cnt,META_COLS_TAG); 
    addToBuffer(buffer,buf,0);
  }
  
  /* start legend */
  if (json){
    snprintf(buf,sizeof(buf),"    \"%s\": [\n", LEGEND_TAG);
  }
  else {
    snprintf(buf,sizeof(buf),"    <%s>\n", LEGEND_TAG);
  }
  addToBuffer(buffer,buf,0);
  /* add legend entries */
  for (unsigned long j = 0; j < col_cnt; j++) {
    char *entry = legend_v[j];
    /* I do not know why the legend is "spaced", but let us skip it */
    while(isspace(*entry)){entry++;}
    /* now output it */
    if (json){
      snprintf(buf,sizeof(buf),"      \"%s\"", entry);
      addToBuffer(buffer,buf,0);
      if (j < col_cnt -1){
	addToBuffer(buffer,",",1);
      }
      addToBuffer(buffer,"\n",1);
    }
    else {
      snprintf(buf,sizeof(buf),"      <%s>%s</%s>\n", LEGEND_ENTRY_TAG, entry,LEGEND_ENTRY_TAG);
      addToBuffer(buffer,buf,0);
    }
  }
  /* end legend */
  if (json){
    snprintf(buf,sizeof(buf),"          ],\n");
  }
  else {
    snprintf(buf,sizeof(buf),"    </%s>\n", LEGEND_TAG);
  }
  addToBuffer(buffer,buf,0);
  
  /* add graphs and prints */
  if (rrd_xport_format_addprints(json,buffer,im)) {return -1;}

  /* if we have got a trailing , then kill it */
  if (buffer->data) {
    if (buffer->data[buffer->len-2]==',') { 
      buffer->data[buffer->len-2]=buffer->data[buffer->len-1];
      buffer->len--;
    }
  }

  /* end meta */
  if (json){
    snprintf(buf,sizeof(buf),"     },\n");
  } else {
    snprintf(buf,sizeof(buf),"  </%s>\n", META_TAG);
  }
  addToBuffer(buffer,buf,0);

  
  /* start data */
  if (json){
    snprintf(buf,sizeof(buf),"  \"%s\": [\n",DATA_TAG);
  } else {
    snprintf(buf,sizeof(buf),"  <%s>\n", DATA_TAG);
  }
  addToBuffer(buffer,buf,0);
  /* iterate over data */
  for (time_t ti = start + step; ti <= end; ti += step) {
    if (timefmt) {
      struct tm loc;
      localtime_r(&ti,&loc);
      strftime(dbuf,sizeof(dbuf),timefmt,&loc);
    } else {
      snprintf(dbuf,sizeof(dbuf),"%lld",(long long int)ti);
    }
    if (json){
      addToBuffer(buffer,"    [ ",0);
      if(showtime){
	addToBuffer(buffer,"\"",1);
	addToBuffer(buffer,dbuf,0);
	addToBuffer(buffer,"\",",2);
      }
    }
    else {
      if (showtime) {
	snprintf(buf,sizeof(buf),
		 "    <%s><%s>%s</%s>", DATA_ROW_TAG,COL_TIME_TAG, dbuf, COL_TIME_TAG);
      } else {
	snprintf(buf,sizeof(buf),
		 "    <%s>", DATA_ROW_TAG);
      }
      addToBuffer(buffer,buf,0);
    }
    for (unsigned long j = 0; j < col_cnt; j++) {
      rrd_value_t newval = DNAN;
      newval = *ptr;
      if (json){
	if (isnan(newval)){
	  addToBuffer(buffer,"null",0);                        
	} else {
	  snprintf(buf,sizeof(buf),"%0.10e",newval);
	  addToBuffer(buffer,buf,0);
	}
	if (j < col_cnt -1){
	  addToBuffer(buffer,", ",0);
	}
      }
      else {
	if (isnan(newval)) {
	  if (enumds) {
	    snprintf(buf,sizeof(buf),"<%s%lu>NaN</%s%lu>", COL_DATA_TAG,j,COL_DATA_TAG,j);
	  } else {
	    snprintf(buf,sizeof(buf),"<%s>NaN</%s>", COL_DATA_TAG,COL_DATA_TAG);
	  }
	} else {
	  if (enumds) {
	    snprintf(buf,sizeof(buf),"<%s%lu>%0.10e</%s%lu>", COL_DATA_TAG,j,newval,COL_DATA_TAG,j);
	  } else {
	    snprintf(buf,sizeof(buf),"<%s>%0.10e</%s>", COL_DATA_TAG,newval,COL_DATA_TAG);
	  }
	}
	addToBuffer(buffer,buf,0);
      }
      ptr++;
    }                
    if (json){
      addToBuffer(buffer,(ti < end ? " ],\n" : " ]\n"),0);
    }
    else {                
      snprintf(buf,sizeof(buf),"</%s>\n", DATA_ROW_TAG);
      addToBuffer(buffer,buf,0);
    }
  }
  /* end data */
  if (json){
    addToBuffer(buffer,"  ]\n",0);
  }
  else {
    snprintf(buf,sizeof(buf),"  </%s>\n",DATA_TAG);
    addToBuffer(buffer,buf,0);
  }

  /* end all */
  if (json){
    addToBuffer(buffer,"}\n",0);
  } else {
    snprintf(buf,sizeof(buf),"</%s>\n", ROOT_TAG);
    addToBuffer(buffer,buf,0);
  }
  return 0;
}

void escapeJSON(char* txt,size_t len) {
  char *tmp=(char*)malloc(len+2);
  size_t l=strlen(txt);
  size_t pos=0;
  /* now iterate over the chars */
  for(size_t i=0;(i<l)&&(pos<len);i++,pos++) {
    switch (txt[i]) {
      case '"':
      case '\\':
	tmp[pos]='\\';pos++;
	tmp[pos]=txt[i];
	break;
    default:
      tmp[pos]=txt[i];
      break;
    }
  }
  /* 0 terminate it */
  tmp[pos]=0;
  /* and copy back over txt */
  strncpy(txt,tmp,len);
  /* and release tmp */
  free(tmp);
}

int rrd_xport_format_addprints(int flags,stringbuffer_t *buffer,image_desc_t *im) {
  /* initialize buffer */
  stringbuffer_t prints={1024,0,NULL,NULL}; 
  stringbuffer_t rules={1024,0,NULL,NULL}; 
  stringbuffer_t gprints={4096,0,NULL,NULL}; 
  char buf[256];
  char dbuf[1024];
  char* val;
  char* timefmt=NULL;
  if (im->xlab_user.minsec!=-1) { timefmt=im->xlab_user.stst; }

  /* define some other stuff based on flags */
  int json=0;
  if (flags &1) { json=1; }
  int showtime=0;
  if (flags &2) { showtime=1;}
  int enumds=0;
  if (flags &4) { enumds=1;}

  /* define some values */
  time_t    now = time(NULL);
  struct tm tmvdef;
  localtime_r(&now, &tmvdef);
  double printval=DNAN;

  /* iterate over all fields and start the work writing to the correct buffers */
  for (long i = 0; i < im->gdes_c; i++) {
    long vidx = im->gdes[i].vidx;
    char* entry;
    switch (im->gdes[i].gf) {
    case GF_PRINT:
    case GF_GPRINT:
      /* PRINT and GPRINT can now print VDEF generated values.
       * There's no need to do any calculations on them as these
       * calculations were already made.
       * we do not support the depreciated version here
       */
      printval=DNAN;
      /* decide to which buffer we print to*/
      stringbuffer_t *usebuffer=&gprints;
      char* usetag="gprint";
      if (im->gdes[i].gf==GF_PRINT) { 
	usebuffer=&prints;
	usetag="print";
      }
      /* get the value */
      if (im->gdes[vidx].gf == GF_VDEF) { /* simply use vals */
	printval = im->gdes[vidx].vf.val;
	localtime_r(&im->gdes[vidx].vf.when, &tmvdef);
      } else {
	int max_ii = ((im->gdes[vidx].end - im->gdes[vidx].start)
		      / im->gdes[vidx].step * im->gdes[vidx].ds_cnt);
	printval = DNAN;
	long validsteps = 0;
	for (long ii = im->gdes[vidx].ds;
	     ii < max_ii; ii += im->gdes[vidx].ds_cnt) {
	  if (!finite(im->gdes[vidx].data[ii]))
	    continue;
	  if (isnan(printval)) {
	    printval = im->gdes[vidx].data[ii];
	    validsteps++;
	    continue;
	  }
	  
	  switch (im->gdes[i].cf) {
	  case CF_HWPREDICT:
	  case CF_MHWPREDICT:
	  case CF_DEVPREDICT:
	  case CF_DEVSEASONAL:
	  case CF_SEASONAL:
	  case CF_AVERAGE:
	    validsteps++;
	    printval += im->gdes[vidx].data[ii];
	    break;
	  case CF_MINIMUM:
	    printval = min(printval, im->gdes[vidx].data[ii]);
	    break;
	  case CF_FAILURES:
	  case CF_MAXIMUM:
	    printval = max(printval, im->gdes[vidx].data[ii]);
	    break;
	  case CF_LAST:
	    printval = im->gdes[vidx].data[ii];
	  }
	}
	if (im->gdes[i].cf == CF_AVERAGE || im->gdes[i].cf > CF_LAST) {
	  if (validsteps > 1) {
	    printval = (printval / validsteps);
	  }
	}
      }
      /* we handle PRINT and GPRINT the same - format now*/
      if (im->gdes[i].strftm) {
	if (im->gdes[vidx].vf.never == 1) {
	  time_clean(buf, im->gdes[i].format);
	} else {
	  strftime(dbuf,sizeof(dbuf), im->gdes[i].format, &tmvdef);
	}
      } else if (bad_format(im->gdes[i].format)) {
	rrd_set_error
	  ("bad format for PRINT in \"%s'", im->gdes[i].format);
	return -1;
      } else {
	snprintf(dbuf,sizeof(dbuf), im->gdes[i].format, printval,"");
      }
      /* print */
      if (json) {
	escapeJSON(dbuf,sizeof(dbuf));
	snprintf(buf,sizeof(buf),",\n        { \"%s\": \"%s\" }",usetag,dbuf);
      } else {
	snprintf(buf,sizeof(buf),"        <%s>%s</%s>\n",usetag,dbuf,usetag);
      }
      addToBuffer(usebuffer,buf,0);
      break;
    case GF_COMMENT:
      if (json) {
	strncpy(dbuf,im->gdes[i].legend,sizeof(dbuf));
	escapeJSON(dbuf,sizeof(dbuf));
	snprintf(buf,sizeof(buf),",\n        { \"comment\": \"%s\" }",dbuf);
      } else {
	snprintf(buf,sizeof(buf),"        <comment>%s</comment>\n",im->gdes[i].legend);
      }
      addToBuffer(&gprints,buf,0);
      break;
    case GF_LINE:
      entry = im->gdes[i].legend;
      /* I do not know why the legend is "spaced", but let us skip it */
      while(isspace(*entry)){entry++;}
      if (json) {
	snprintf(buf,sizeof(buf),",\n        { \"line\": \"%s\" }",entry);
      } else {
	snprintf(buf,sizeof(buf),"        <line>%s</line>\n",entry);
      }
      addToBuffer(&gprints,buf,0);
      break;
    case GF_AREA:
      if (json) {
	snprintf(buf,sizeof(buf),",\n        { \"area\": \"%s\" }",im->gdes[i].legend);
      } else {
	snprintf(buf,sizeof(buf),"        <area>%s</area>\n",im->gdes[i].legend);
      }
      addToBuffer(&gprints,buf,0);
      break;
    case GF_STACK:
      if (json) {
	snprintf(buf,sizeof(buf),",\n        { \"stack\": \"%s\" }",im->gdes[i].legend);
      } else {
	snprintf(buf,sizeof(buf),"        <stack>%s</stack>\n",im->gdes[i].legend);
      }
      addToBuffer(&gprints,buf,0);
      break;
    case GF_TEXTALIGN:
      val="";
      switch (im->gdes[i].txtalign) {
      case TXA_LEFT: val="left"; break;
      case TXA_RIGHT: val="right"; break;
      case TXA_CENTER: val="center"; break;
      case TXA_JUSTIFIED: val="justified"; break;
      }
      if (json) {
	snprintf(buf,sizeof(buf),",\n        { \"align\": \"%s\" }",val);
      } else {
	snprintf(buf,sizeof(buf),"        <align>%s</align>\n",val);
      }
      addToBuffer(&gprints,buf,0);
      break;
    case GF_HRULE:
      /* This does not work as expected - Tobi please help!!! */
      snprintf(dbuf,sizeof(dbuf),"%0.10e",im->gdes[i].vf.val);
      /* and output it */
      if (json) {
        snprintf(buf,sizeof(buf),",\n        { \"hrule\": \"%s\" }",dbuf);
      } else {
        snprintf(buf,sizeof(buf),"        <hrule>%s</hrule>\n",dbuf);
      }
      addToBuffer(&rules,buf,0);
      break;
    case GF_VRULE:
      if (timefmt) {
	struct tm loc;
	localtime_r(&im->gdes[i].xrule,&loc);
	strftime(dbuf,254,timefmt,&loc);
      } else {
	snprintf(dbuf,254,"%lld",(long long int)im->gdes[i].vf.when);
      }
      /* and output it */
      if (json) {
        snprintf(buf,sizeof(buf),",\n        { \"vrule\": \"%s\" }",dbuf);
      } else {
        snprintf(buf,sizeof(buf),"        <vrule>%s</vrule>\n",dbuf);
      }
      addToBuffer(&rules,buf,0);
      break;
    default: 
      break;
    }
  }
  /* now add prints */
  if (prints.len) {
    if (json){
      snprintf(buf,sizeof(buf),"    \"%s\": [\n","prints");
      addToBuffer(buffer,buf,0);
      addToBuffer(buffer,(char*)prints.data+2,prints.len-2);
      addToBuffer(buffer,"\n        ],\n",0);
    } else {
      snprintf(buf,sizeof(buf),"    <%s>\n", "prints");
      addToBuffer(buffer,buf,0);
      addToBuffer(buffer,(char*)prints.data,prints.len);
      snprintf(buf,sizeof(buf),"    </%s>\n", "prints");
      addToBuffer(buffer,buf,0);
    }
    free(prints.data);
  }
  /* now add gprints */
  if (gprints.len) {
    if (json){
      snprintf(buf,sizeof(buf),"    \"%s\": [\n","gprints");
      addToBuffer(buffer,buf,0);
      addToBuffer(buffer,(char*)gprints.data+2,gprints.len-2);
      addToBuffer(buffer,"\n        ],\n",0);
    } else {
      snprintf(buf,sizeof(buf),"    <%s>\n", "gprints");
      addToBuffer(buffer,buf,0);
      addToBuffer(buffer,(char*)gprints.data,gprints.len);
      snprintf(buf,sizeof(buf),"    </%s>\n", "gprints");
      addToBuffer(buffer,buf,0);
    }
    free(gprints.data);
  }
  /* now add rules */
  if (rules.len) {
    if (json){
      snprintf(buf,sizeof(buf),"    \"%s\": [\n","rules");
      addToBuffer(buffer,buf,0);
      addToBuffer(buffer,(char*)rules.data+2,rules.len-2);
      addToBuffer(buffer,"\n        ],\n",0);
    } else {
      snprintf(buf,sizeof(buf),"    <%s>\n", "rules");
      addToBuffer(buffer,buf,0);
      addToBuffer(buffer,(char*)rules.data,rules.len);
      snprintf(buf,sizeof(buf),"    </%s>\n", "rules");
      addToBuffer(buffer,buf,0);
    }
    free(rules.data);
  }
  /* and return */
  return 0;
}
