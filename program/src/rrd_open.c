/*****************************************************************************
 * RRDtool 1.0.33  Copyright Tobias Oetiker, 1997 - 2000
 *****************************************************************************
 * rrd_open.c  Open an RRD File
 *****************************************************************************
 * $Id$
 * $Log$
 * Revision 1.1  2001/02/25 22:25:05  oetiker
 * Initial revision
 *
 *****************************************************************************/

#include "rrd_tool.h"
#define MEMBLK 8192

/* open a database file, return its header and a open filehandle */
/* positioned to the first cdp in the first rra */

int
rrd_open(char *file_name, FILE **in_file, rrd_t *rrd, int rdwr)    
{

    
    char *mode = NULL;
    rrd_init(rrd);
    if (rdwr == RRD_READONLY) {
#ifndef WIN32
	mode = "r";
#else
	mode = "rb";
#endif
    } else {
#ifndef WIN32
	mode = "r+";
#else
	mode = "rb+";
#endif
    }
    
    if (((*in_file) = fopen(file_name,mode)) == NULL ){
	rrd_set_error("opening '%s': %s",file_name, strerror(errno));
	return (-1);
    }
    
#define MYFREAD(MYVAR,MYVART,MYCNT) \
    if ((MYVAR = malloc(sizeof(MYVART) * MYCNT)) == NULL) {\
	rrd_set_error("" #MYVAR " malloc"); \
    return (-1); } \
    fread(MYVAR,sizeof(MYVART),MYCNT, *in_file); 


    MYFREAD(rrd->stat_head, stat_head_t,  1)
    
	/* lets do some test if we are on track ... */
	if (strncmp(rrd->stat_head->cookie,RRD_COOKIE,4) != 0){
	    rrd_set_error("'%s' is not an RRD file",file_name);
	    free(rrd->stat_head);
	    return(-1);}

	if (strncmp(rrd->stat_head->version,RRD_VERSION,5) != 0){
	    rrd_set_error("cant handle RRD file version %s",
			rrd->stat_head->version);
	    free(rrd->stat_head);
	    return(-1);}

	if (rrd->stat_head->float_cookie != FLOAT_COOKIE){
	    rrd_set_error("This RRD was created on other architecture");
	    free(rrd->stat_head);
	    return(-1);}

    MYFREAD(rrd->ds_def,    ds_def_t,     rrd->stat_head->ds_cnt)
    MYFREAD(rrd->rra_def,   rra_def_t,    rrd->stat_head->rra_cnt)
    MYFREAD(rrd->live_head,   live_head_t,  1)
    MYFREAD(rrd->pdp_prep,  pdp_prep_t,   rrd->stat_head->ds_cnt)
    MYFREAD(rrd->cdp_prep,  cdp_prep_t,   (rrd->stat_head->rra_cnt
	                                     * rrd->stat_head->ds_cnt))
    MYFREAD(rrd->rra_ptr,   rra_ptr_t,    rrd->stat_head->rra_cnt)
#undef MYFREAD

    return(0);
}

void rrd_init(rrd_t *rrd)
{
    rrd->stat_head = NULL;
    rrd->ds_def = NULL;
    rrd->rra_def = NULL;
    rrd->live_head = NULL;
    rrd->rra_ptr = NULL;
    rrd->pdp_prep = NULL;
    rrd->cdp_prep = NULL;
    rrd->rrd_value = NULL;
}

void rrd_free(rrd_t *rrd)
{
    free(rrd->stat_head);
    free(rrd->ds_def);
    free(rrd->rra_def);
    free(rrd->live_head);
    free(rrd->rra_ptr);
    free(rrd->pdp_prep);
    free(rrd->cdp_prep);
    free(rrd->rrd_value);
}

int readfile(char *file_name, char **buffer, int skipfirst){
    long writecnt=0,totalcnt = MEMBLK;
    FILE *input=NULL;
    char c ;
    if ((strcmp("-",file_name) == 0)) { input = stdin; }
    else {
      if ((input = fopen(file_name,"rb")) == NULL ){
	rrd_set_error("opening '%s': %s",file_name,strerror(errno));
	return (-1);
      }
    }
    if (skipfirst){
      do { c = getc(input); } while (c != '\n' && ! feof(input)); 
    }
    if (((*buffer) = (char *) malloc((MEMBLK+4)*sizeof(char))) == NULL) {
	perror("Allocate Buffer:");
	exit(1);
    };
    do{
      writecnt += fread((*buffer)+writecnt, 1, MEMBLK * sizeof(char) ,input);
      if (writecnt >= totalcnt){
	totalcnt += MEMBLK;
	if (((*buffer)=rrd_realloc((*buffer), (totalcnt+4) * sizeof(char)))==NULL){
	    perror("Realloc Buffer:");
	    exit(1);
	};
      }
    } while (! feof(input));
    (*buffer)[writecnt] = '\0';
    if (strcmp("-",file_name) != 0) {fclose(input);};
    return writecnt;
}


