/*****************************************************************************
 * RRDtool 1.2.23  Copyright by Tobi Oetiker, 1997-2007
 *****************************************************************************
 * rrd_open.c  Open an RRD File
 *****************************************************************************
 * $Id$
 * $Log$
 * Revision 1.10  2004/05/26 22:11:12  oetiker
 * reduce compiler warnings. Many small fixes. -- Mike Slifcak <slif@bellsouth.net>
 *
 * Revision 1.9  2003/04/29 21:56:49  oetiker
 * readline in rrd_open.c reads the file in 8 KB blocks, and calls realloc for
 * each block. realloc is very slow in Mac OS X for huge blocks, e.g. when
 * restoring databases from huge xml files. This patch finds the size of the
 * file, and starts out with malloc'ing the full size.
 * -- Peter Speck <speck@ruc.dk>
 *
 * Revision 1.8  2003/04/11 19:43:44  oetiker
 * New special value COUNT which allows calculations based on the position of a
 * value within a data set. Bug fix in rrd_rpncalc.c. PREV returned erroneus
 * value for the second value. Bug fix in rrd_restore.c. Bug causing seek error
 * when accesing an RRD restored from an xml that holds an RRD version <3.
 * --  Ruben Justo <ruben@ainek.com>
 *
 * Revision 1.7  2003/03/31 21:22:12  oetiker
 * enables RRDtool updates with microsecond or in case of windows millisecond
 * precision. This is needed to reduce time measurement error when archive step
 * is small. (<30s) --  Sasha Mikheev <sasha@avalon-net.co.il>
 *
 * Revision 1.6  2003/02/13 07:05:27  oetiker
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
rrd_open(const char *file_name, FILE **in_file, rrd_t *rrd, int rdwr)    
{

    
    char *mode = NULL;
    int version;
    
    rrd_init(rrd);
    if (rdwr == RRD_READONLY) {
        mode = "rb";
    } else {
        mode = "rb+";
    }
    
    if (((*in_file) = fopen(file_name,mode)) == NULL ){
        rrd_set_error("opening '%s': %s",file_name, rrd_strerror(errno));
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
    /* lets see if the first read worked */
    if (ferror( *in_file ) || feof(*in_file)) {
        rrd_set_error("reading the cookie off %s faild",file_name);
        fclose(*in_file);
        return(-1);
    }        

        /* lets do some test if we are on track ... */
        if (strncmp(rrd->stat_head->cookie,RRD_COOKIE,4) != 0){
            rrd_set_error("'%s' is not an RRD file",file_name);
            free(rrd->stat_head);
            rrd->stat_head = NULL; 
            fclose(*in_file);
            return(-1);}

        if (rrd->stat_head->float_cookie != FLOAT_COOKIE){
            rrd_set_error("This RRD was created on other architecture");
            free(rrd->stat_head);
            rrd->stat_head = NULL; 
            fclose(*in_file);
            return(-1);}

    version = atoi(rrd->stat_head->version);

        if (version > atoi(RRD_VERSION)){
            rrd_set_error("can't handle RRD file version %s",
                        rrd->stat_head->version);
            free(rrd->stat_head);
            rrd->stat_head = NULL; 
            fclose(*in_file);
            return(-1);}


    MYFREAD(rrd->ds_def,    ds_def_t,     rrd->stat_head->ds_cnt)
    MYFREAD(rrd->rra_def,   rra_def_t,    rrd->stat_head->rra_cnt)
    /* handle different format for the live_head */
    if(version < 3) {
            rrd->live_head = (live_head_t *)malloc(sizeof(live_head_t));
            if(rrd->live_head == NULL) {
                rrd_set_error("live_head_t malloc");
                fclose(*in_file); 
                return (-1);
            }
                fread(&rrd->live_head->last_up, sizeof(long), 1, *in_file); 
                rrd->live_head->last_up_usec = 0;
    }
    else {
            MYFREAD(rrd->live_head, live_head_t, 1)
    }
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
    if (rrd->stat_head) free(rrd->stat_head);
    if (rrd->ds_def) free(rrd->ds_def);
    if (rrd->rra_def) free(rrd->rra_def);
    if (rrd->live_head) free(rrd->live_head);
    if (rrd->rra_ptr) free(rrd->rra_ptr);
    if (rrd->pdp_prep) free(rrd->pdp_prep);
    if (rrd->cdp_prep) free(rrd->cdp_prep);
    if (rrd->rrd_value) free(rrd->rrd_value);
}

/* routine used by external libraries to free memory allocated by
 * rrd library */
void rrd_freemem(void *mem)
{

    if (mem) free(mem);
}

int readfile(const char *file_name, char **buffer, int skipfirst){
    long writecnt=0,totalcnt = MEMBLK;
     long offset = 0;
    FILE *input=NULL;
    char c ;
    if ((strcmp("-",file_name) == 0)) { input = stdin; }
    else {
      if ((input = fopen(file_name,"rb")) == NULL ){
        rrd_set_error("opening '%s': %s",file_name,rrd_strerror(errno));
        return (-1);
      }
    }
    if (skipfirst){
      do { c = getc(input); offset++; } while (c != '\n' && ! feof(input));
    }
    if (strcmp("-",file_name)) {
      fseek(input, 0, SEEK_END);
      /* have extra space for detecting EOF without realloc */
      totalcnt = (ftell(input) + 1) / sizeof(char) - offset;
      if (totalcnt < MEMBLK)
        totalcnt = MEMBLK; /* sanitize */
      fseek(input, offset * sizeof(char), SEEK_SET);
    }
    if (((*buffer) = (char *) malloc((totalcnt+4) * sizeof(char))) == NULL) {
        perror("Allocate Buffer:");
        exit(1);
    };
    do{
      writecnt += fread((*buffer)+writecnt, 1, (totalcnt - writecnt) * sizeof(char),input);
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


