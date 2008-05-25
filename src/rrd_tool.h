/*****************************************************************************
 * RRDtool 1.3rc5  Copyright by Tobi Oetiker, 1997-2008
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
#include "rrd_getopt.h"
#include "rrd_format.h"

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define DIM(x) (sizeof(x)/sizeof(x[0]))

    info_t   *rrd_info(
    int,
    char **);
    int       rrd_lastupdate(
    int argc,
    char **argv,
    time_t *last_update,
    unsigned long *ds_cnt,
    char ***ds_namv,
    char ***last_ds);
    info_t   *rrd_update_v(
    int,
    char **);
    char     *sprintf_alloc(
    char *,
    ...);
    info_t   *info_push(
    info_t *,
    char *,
    enum info_type,
    infoval);
    void      info_print(
    info_t *data);
    void      info_free(
    info_t *);

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

    void      rrd_free(
    rrd_t *rrd);
    void      rrd_freemem(
    void *mem);
    void      rrd_init(
    rrd_t *rrd);

    rrd_file_t *rrd_open(
    const char *const file_name,
    rrd_t *rrd,
    unsigned rdwr);
    void      rrd_dontneed(
    rrd_file_t *rrd_file,
    rrd_t *rrd);
    int       rrd_close(
    rrd_file_t *rrd_file);
    ssize_t   rrd_read(
    rrd_file_t *rrd_file,
    void *buf,
    size_t count);
    ssize_t   rrd_write(
    rrd_file_t *rrd_file,
    const void *buf,
    size_t count);
    void      rrd_flush(
    rrd_file_t *rrd_file);
    off_t     rrd_seek(
    rrd_file_t *rrd_file,
    off_t off,
    int whence);
    off_t     rrd_tell(
    rrd_file_t *rrd_file);
    int       readfile(
    const char *file,
    char **buffer,
    int skipfirst);

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

    /* rrd_strerror is thread safe, but still it uses a global buffer
       (but one per thread), thus subsequent calls within a single
       thread overwrite the same buffer */
    const char *rrd_strerror(
    int err);

#endif

#ifdef  __cplusplus
}
#endif
