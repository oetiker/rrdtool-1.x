/*****************************************************************************
 * RRDtool 1.0.33  Copyright Tobias Oetiker, 1997 - 2000
 *****************************************************************************
 * rrd_tool.h   Common Header File
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
 * Revision 1.1.1.1  2001/02/25 22:25:06  oetiker
 * checkin
 *
 *****************************************************************************/
#ifdef  __cplusplus
extern "C" {
#endif


#ifndef _RRD_TOOL_H
#define _RRD_TOOL_H

#ifdef WIN32
# include "ntconfig.h"
#else
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#endif

#ifdef MUST_DISABLE_SIGFPE
#include <signal.h>
#endif

#ifdef MUST_DISABLE_FPMASK
#include <floatingpoint.h>
#endif
    
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#if HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#ifndef MAXPATH
#  define MAXPATH 1024
#endif

#if HAVE_MATH_H
# include <math.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#if HAVE_SYS_TIMES_H
# include <sys/times.h>
#endif
#if HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#if (defined(__svr4__) && defined(__sun__))
/* Solaris headers (pre 2.6) don't have a getrusage prototype.
   Use this instead. */
extern int getrusage(int, struct rusage *);
#endif /* __svr4__ && __sun__ */
#endif

#include "rrd.h"

#ifndef WIN32

/* unix-only includes */
#ifndef isnan
int isnan(double value);
#endif

#else

/* Win32 only includes */

#include <float.h>        /* for _isnan  */
#define isnan _isnan
#define finite _finite
#define isinf(a) (_fpclass(a) == _FPCLASS_NINF || _fpclass(a) == _FPCLASS_PINF)
#endif

/* local include files -- need to be after the system ones */
#include "getopt.h"
#include "rrd_format.h"

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif                                                   

#define DIM(x) (sizeof(x)/sizeof(x[0]))

/* rrd info interface */
enum info_type   { RD_I_VAL=0,
	       RD_I_CNT,
	       RD_I_STR  };

typedef union infoval { 
    unsigned long u_cnt; 
    rrd_value_t   u_val;
    char         *u_str;
} infoval;

typedef struct info_t {
    char            *key;
    enum info_type  type;
    union infoval   value;
    struct info_t   *next;
} info_t;


info_t *rrd_info(int, char **);

/* HELPER FUNCTIONS */
int GifSize(FILE *, long *, long *);
int PngSize(FILE *, long *, long *);
int PngSize(FILE *, long *, long *);

#include <gd.h>
void gdImagePng(gdImagePtr im, FILE *out);

int rrd_create_fn(char *file_name, rrd_t *rrd);
int rrd_fetch_fn(char *filename, enum cf_en cf_idx,
		 time_t *start,time_t *end,
		 unsigned long *step,
		 unsigned long *ds_cnt,
		 char        ***ds_namv,
		 rrd_value_t **data);

void rrd_free(rrd_t *rrd);
void rrd_init(rrd_t *rrd);

int rrd_open(char *file_name, FILE **in_file, rrd_t *rrd, int rdwr);
int readfile(char *file, char **buffer, int skipfirst);

#define RRD_READONLY    0
#define RRD_READWRITE   1

enum cf_en cf_conv(char *string);
enum dst_en dst_conv(char *string);
long ds_match(rrd_t *rrd,char *ds_nam);
double rrd_diff(char *a, char *b);

/* functions added for aberrant behavior detection.
 * implemented for the most part in rrd_hw.c */
int update_aberrant_CF(rrd_t *rrd, rrd_value_t pdp_val, enum cf_en current_cf,
   unsigned long cdp_idx, unsigned long rra_idx, unsigned long ds_idx,
   unsigned short CDP_scratch_idx, rrd_value_t *seasonal_coef);
int create_hw_contingent_rras(rrd_t *rrd, unsigned short period, 
   unsigned long hashed_name);
int lookup_seasonal(rrd_t *rrd, unsigned long rra_idx, unsigned long rra_start,
   FILE *rrd_file, unsigned long offset, rrd_value_t **seasonal_coef);
void erase_violations(rrd_t *rrd, unsigned long cdp_idx, unsigned long rra_idx);
int apply_smoother(rrd_t *rrd, unsigned long rra_idx, unsigned long rra_start,
   FILE *rrd_file);

/* a standard fixed-capacity FIFO queue implementation */
typedef struct FIFOqueue {
   rrd_value_t *queue;
   int capacity, head, tail;
} FIFOqueue;

int queue_alloc(FIFOqueue **q,int capacity);
void queue_dealloc(FIFOqueue *q);
void queue_push(FIFOqueue *q, rrd_value_t value);
int queue_isempty(FIFOqueue *q);
rrd_value_t queue_pop(FIFOqueue *q);

#define BURNIN_CYCLES 3

#endif


#ifdef  __cplusplus
}
#endif
