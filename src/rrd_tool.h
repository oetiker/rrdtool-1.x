#ifndef RRD_TOOL_H_3853987DDF7E4709A5B5849E5A6204F4
#define RRD_TOOL_H_3853987DDF7E4709A5B5849E5A6204F4

/*****************************************************************************
 * RRDtool 1.7.2 Copyright by Tobi Oetiker, 1997-2019
 *****************************************************************************
 * rrd_tool.h   Common Header File
 *****************************************************************************/
#ifdef  __cplusplus
extern    "C" {
#endif

#include "rrd_config.h"

#include "rrd.h"

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__) && !defined(__MINGW32__)

/* Win32 only includes */

#include <float.h>      /* for _isnan  */
#include <io.h>         /* for chdir   */

    struct tm *localtime_r(
    const time_t *timep,
    struct tm *result);
    char     *ctime_r(
    const time_t *timep,
    char *result);
    struct tm *gmtime_r(
    const time_t *timep,
    struct tm *result);
    char     *strtok_r(
    char *str,
    const char *sep,
    char **last);

#else

/* unix-only includes */
#if !defined isnan && !defined HAVE_ISNAN
    int       isnan(
    double value);
#endif

#endif

/* local include files -- need to be after the system ones */
#include "optparse.h"
#include "rrd_format.h"

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define DIM(x) (sizeof(x)/sizeof(x[0]))

    char     *sprintf_alloc(
    char *,
    ...);

/* HELPER FUNCTIONS */

    int       PngSize(
    FILE *,
    long *,
    long *);

    int       rrd_create_fn(
    const char *file_name,
    rrd_t *rrd,
    int no_overwrite);
    int rrd_fetch_fn (const char *filename,
            enum cf_en cf_idx,
            time_t *start,
            time_t *end,
            unsigned long *step,
            unsigned long *ds_cnt,
            char ***ds_namv,
            rrd_value_t **data);

    int rrd_fetch_empty(
        time_t *start,
        time_t *end,
        unsigned long *step,
        unsigned long *ds_cnt,
        char *ds_nam,
        char ***ds_namv,
        rrd_value_t **data);


#ifdef HAVE_LIBDBI
    int rrd_fetch_fn_libdbi(const char *filename, enum cf_en cf_idx,
 			time_t *start,time_t *end,
 			unsigned long *step,
 			unsigned long *ds_cnt,
 			char        ***ds_namv,
 			rrd_value_t **data);
#endif

typedef int (*rrd_fetch_cb_t)(
    const char     *filename,  /* name of the rrd */
    enum cf_en     cf_idx, /* consolidation function */
    time_t         *start,
    time_t         *end,       /* which time frame do you want ?
                                * will be changed to represent reality */
    unsigned long  *step,      /* which stepsize do you want? 
                                * will be changed to represent reality */
    unsigned long  *ds_cnt,    /* number of data sources in file */
    char           ***ds_namv, /* names of data_sources */
    rrd_value_t    **data      /* two dimensional array containing the data */
);
                                                                                                

    int rrd_fetch_fn_cb(const char *filename, enum cf_en cf_idx,
        time_t *start,time_t *end,
        unsigned long *step,
        unsigned long *ds_cnt,
        char        ***ds_namv,
        rrd_value_t **data);

    int rrd_fetch_cb_register(rrd_fetch_cb_t cb);


#define RRD_READONLY    (1<<0)
#define RRD_READWRITE   (1<<1)
#define RRD_CREAT       (1<<2)
#define RRD_READAHEAD   (1<<3)
#define RRD_COPY        (1<<4)
#define RRD_EXCL        (1<<5)
#define RRD_READVALUES  (1<<6)
#define RRD_LOCK        (1<<7)

    enum cf_en rrd_cf_conv(
    const char *string);
    enum dst_en dst_conv(
    const char *string);
    long      ds_match(
    rrd_t *rrd,
    char *ds_nam);
    off_t rrd_get_header_size(
    rrd_t *rrd);
    double    rrd_diff(
    char *a,
    char *b);

    const char *cf_to_string (enum cf_en cf);

#ifdef  __cplusplus
}
#endif

#endif /* RRD_TOOL_H */
