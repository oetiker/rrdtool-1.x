/*****************************************************************************
 * RRDtool 1.2.20  Copyright by Tobi Oetiker, 1997-2007
 *****************************************************************************
 * rrd_tool.h   Common Header File
 *****************************************************************************/
#ifdef  __cplusplus
extern "C" {
#endif


#ifndef _RRD_TOOL_H
#define _RRD_TOOL_H

#ifdef HAVE_CONFIG_H
#include "../rrd_config.h"
#elif defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)
#include "../win32/config.h"
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
/* Sorry: don't know autoconf as well how to check the exist of
   dirent.h ans sys/stat.h
*/

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
# include <sys/stat.h>
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

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)

/* Win32 only includes */

#include <float.h>        /* for _isnan  */
#include <io.h>           /* for chdir   */

struct tm* localtime_r(const time_t *timep, struct tm* result);
char* ctime_r(const time_t *timep, char* result);
struct tm* gmtime_r(const time_t *timep, struct tm* result);
char *strtok_r(char *str, const char *sep, char **last);

#else

/* unix-only includes */
#ifndef isnan
int isnan(double value);
#endif

#endif

/* local include files -- need to be after the system ones */
#include "rrd_getopt.h"
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
	       RD_I_STR, 
		   RD_I_INT };

typedef union infoval { 
    unsigned long u_cnt; 
    rrd_value_t   u_val;
    char         *u_str;
    int		  u_int;
} infoval;

typedef struct info_t {
    char            *key;
    enum info_type  type;
    union infoval   value;
    struct info_t   *next;
} info_t;

info_t *rrd_info(int, char **);
int rrd_lastupdate(int argc, char **argv, time_t *last_update,
                unsigned long *ds_cnt, char ***ds_namv, char ***last_ds);
info_t *rrd_update_v(int, char **);
char * sprintf_alloc(char *, ...);
info_t *info_push(info_t *, char *, enum info_type, infoval);

/* HELPER FUNCTIONS */

int PngSize(FILE *, long *, long *);

int rrd_create_fn(const char *file_name, rrd_t *rrd);
int rrd_fetch_fn(const char *filename, enum cf_en cf_idx,
		 time_t *start,time_t *end,
		 unsigned long *step,
		 unsigned long *ds_cnt,
		 char        ***ds_namv,
		 rrd_value_t **data);

void rrd_free(rrd_t *rrd);
void rrd_freemem(void *mem);
void rrd_init(rrd_t *rrd);

int rrd_open(const char *file_name, FILE **in_file, rrd_t *rrd, int rdwr);
int readfile(const char *file, char **buffer, int skipfirst);

#define RRD_READONLY    0
#define RRD_READWRITE   1

enum cf_en cf_conv(const char *string);
enum dst_en dst_conv(char *string);
long ds_match(rrd_t *rrd,char *ds_nam);
double rrd_diff(char *a, char *b);

    /* rrd_strerror is thread safe, but still it uses a global buffer
       (but one per thread), thus subsequent calls within a single
       thread overwrite the same buffer */
const char *rrd_strerror(int err);

#endif

#ifdef  __cplusplus
}
#endif
