/****************************************************************************
 * RRDtool 1.4.3  Copyright by Tobi Oetiker, 1997-2010
 ****************************************************************************
 * rrd_xport.c  export RRD data 
 ****************************************************************************/

#include <sys/stat.h>

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

/* helper function for buffer handling */
typedef struct stringbuffer_t {
  size_t allocated;
  size_t len;
  unsigned char* data;
  FILE *file;
} stringbuffer_t;
int addToBuffer(stringbuffer_t *,char*,size_t);

int rrd_graph_xport(image_desc_t *);
int rrd_xport_format_xml(stringbuffer_t *,image_desc_t*,time_t, time_t, unsigned long, unsigned long, char**, rrd_value_t*);
int rrd_xport_format_json(stringbuffer_t *,image_desc_t*,time_t, time_t, unsigned long, unsigned long, char**, rrd_value_t*);
int rrd_xport_format_sv(char,stringbuffer_t *,image_desc_t*,time_t, time_t, unsigned long, unsigned long, char**, rrd_value_t*);

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
  /* if we write a file, then open it */
  if (strlen(im->graphfile)) {
    buffer.file=fopen(im->graphfile,"w");
  }

  /* do the data processing */
  if (rrd_xport_fn(im,&start,&end,&step,&col_cnt,&legend_v,&data,1)) { return -1;}

  /* fill in some data */
  rrd_infoval_t info;
  info.u_cnt = im->start;
  grinfo_push(im, sprintf_alloc("graph_start"), RD_I_CNT, info);
  info.u_cnt = im->end;
  grinfo_push(im, sprintf_alloc("graph_end"), RD_I_CNT, info);
  info.u_cnt = im->step;
  grinfo_push(im, sprintf_alloc("graph_step"), RD_I_CNT, info);


  /* format it for output */
  int r=0;
  switch(im->imgformat) {
  case IF_XML:
    r=rrd_xport_format_xml(&buffer,im, start, end, step, col_cnt, legend_v, data);
    break;
  case IF_JSON:
    r=rrd_xport_format_json(&buffer,im, start, end, step, col_cnt, legend_v, data);
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
  /* handle errors */
  if (r) {
    if (buffer.data) {free(buffer.data);}
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
  /* unfortunatley we have to do it this way, 
     as when no --x-graph argument is given,
     then the xlab_user is not in a clean state (e.g. zero-filled) */
  if (im->xlab_user.minsec!=-1) { timefmt=im->xlab_user.stst; }

  /* row count */
  unsigned long row_cnt=(end-start)/step;

  /* estimate buffer size (to avoid multiple allocations) */
  buffer->allocated=
    1024 /* bytes of overhead /header/footer */
    +(12+19*col_cnt) /* 12 bytes overhead/line plus 19 bytes per column*/
    *(1+row_cnt) /* number of columns + 1 (for header) */
    ;

  /* now start writing the header*/
  if (addToBuffer(buffer,"\"time\"",6)) { return 1; }
  char buf[256];
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
      snprintf(buf,254,"%ul",ti);
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

int rrd_xport_format_xml(stringbuffer_t *buffer,image_desc_t *im,time_t start, time_t end, unsigned long step, unsigned long col_cnt, char **legend_v, rrd_value_t* data) {
  addToBuffer(buffer,"xml not implemented",0);
  return 0;
}

int rrd_xport_format_json(stringbuffer_t *buffer,image_desc_t *im,time_t start, time_t end, unsigned long step, unsigned long col_cnt, char **legend_v, rrd_value_t* data) {
  addToBuffer(buffer,"JSON not implemented",0);
  return 0;
}

