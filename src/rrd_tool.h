/*****************************************************************************
 * RRDtool 1.0.33  Copyright Tobias Oetiker, 1997 - 2000
 *****************************************************************************
 * rrd_tool.h   Common Header File
 *****************************************************************************/
#ifdef  __cplusplus
extern "C" {
#endif


#ifndef _RRD_TOOL_H
#define _RRD_TOOL_H

#ifdef WIN32
#include "../confignt/config.h"
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

#endif

#ifdef  __cplusplus
}
#endif
