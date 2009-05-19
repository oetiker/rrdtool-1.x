/*****************************************************************************
 * RRDtool 1.3.8  Copyright by Tobi Oetiker, 1997-2009
 *****************************************************************************
 * rrd_tool.h   Common Header File
 *****************************************************************************/
#ifdef  __cplusplus
extern    "C" {
#endif

#ifndef _RRD_TOOL_H
#define _RRD_TOOL_H

#ifdef HAVE_CONFIG_H
#include "../rrd_config.h"
#elif defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)
#include "../win32/config.h"
#endif

#include "rrd.h"

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)

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
#ifdef HAVE_GETOPT_LONG
#define _GNU_SOURCE
#include <getopt.h>
#else
#include "rrd_getopt.h"
#endif

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
    rrd_t *rrd);
    int       rrd_fetch_fn(
    const char *filename,
    enum cf_en cf_idx,
    time_t *start,
    time_t *end,
    unsigned long *step,
    unsigned long *ds_cnt,
    char ***ds_namv,
    rrd_value_t **data);

#define RRD_READONLY    (1<<0)
#define RRD_READWRITE   (1<<1)
#define RRD_CREAT       (1<<2)
#define RRD_READAHEAD   (1<<3)
#define RRD_COPY        (1<<4)

    enum cf_en cf_conv(
    const char *string);
    enum dst_en dst_conv(
    char *string);
    long      ds_match(
    rrd_t *rrd,
    char *ds_nam);
    double    rrd_diff(
    char *a,
    char *b);

#endif

#ifdef  __cplusplus
}
#endif
