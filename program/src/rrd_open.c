/*****************************************************************************
 * RRDtool 1.1.x  Copyright Tobias Oetiker, 1997 - 2002
 *****************************************************************************
 * rrd_open.c  Open an RRD File
 *****************************************************************************
 * $Id$
 * $Log$
 * Revision 1.5  2002/06/20 00:21:03  jake
 * More Win32 build changes; thanks to Kerry Calvert.
 *
 * Revision 1.4  2002/02/01 20:34:49  oetiker
 * fixed version number and date/time
 *
 * Revision 1.3  2001/03/04 13:01:55  oetiker
 * Aberrant Behavior Detection support. A brief overview added to rrdtool.pod.
 * Major updates to rrd_update.c, rrd_create.c. Minor update to other core files.
 * This is backwards compatible! But new files using the Aberrant stuff are not readable
 * by old rrdtool versions. See http://cricket.sourceforge.net/aberrant/rrd_hw.htm
 * -- Jake Brutlag <jakeb@corp.webtv.net>
 *
 * Revision 1.2  2001/03/04 10:29:20  oetiker
 * fixed filedescriptor leak
 * -- Mike Franusich <mike@franusich.com>
 *
 * Revision 1.1.1.1  2001/02/25 22:25:05  oetiker
 * checkin
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
/*
	if (rdwr == RRD_READWRITE)
	{
	   if (setvbuf((*in_file),NULL,_IONBF,2)) {
		  rrd_set_error("failed to disable the stream buffer\n");
		  return (-1);
	   }
	}
*/
    
#define MYFREAD(MYVAR,MYVART,MYCNT) \
    if ((MYVAR = malloc(sizeof(MYVART) * MYCNT)) == NULL) {\
	rrd_set_error("" #MYVAR " malloc"); \
        fclose(*in_file); \
    return (-1); } \
    fread(MYVAR,sizeof(MYVART),MYCNT, *in_file); 


    MYFREAD(rrd->stat_head, stat_head_t,  1)
    
	/* lets do some test if we are on track ... */
	if (strncmp(rrd->stat_head->cookie,RRD_COOKIE,4) != 0){
	    rrd_set_error("'%s' is not an RRD file",file_name);
	    free(rrd->stat_head);
	    fclose(*in_file);
	    return(-1);}

        if (atoi(rrd->stat_head->version) > atoi(RRD_VERSION)){
	    rrd_set_error("can't handle RRD file version %s",
			rrd->stat_head->version);
	    free(rrd->stat_head);
	    fclose(*in_file);
	    return(-1);}

	if (rrd->stat_head->float_cookie != FLOAT_COOKIE){
	    rrd_set_error("This RRD was created on other architecture");
	    free(rrd->stat_head);
	    fclose(*in_file);
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

/* routine used by external libraries to free memory allocated by
 * rrd library */
void rrd_freemem(void *mem)
{

    free(mem);
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


