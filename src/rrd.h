/*****************************************************************************
 * RRDtool 1.3rc9  Copyright by Tobi Oetiker, 1997-2008
 *****************************************************************************
 * rrdlib.h   Public header file for librrd
 *****************************************************************************
 * $Id$
 * $Log$
 * Revision 1.9  2005/02/13 16:13:33  oetiker
 * let rrd_graph return the actual value range it picked ...
 * -- Henrik Stoerner <henrik@hswn.dk>
 *
 * Revision 1.8  2004/05/26 22:11:12  oetiker
 * reduce compiler warnings. Many small fixes. -- Mike Slifcak <slif@bellsouth.net>
 *
 * Revision 1.7  2003/11/12 22:14:26  oetiker
 * allow to pass an open filehandle into rrd_graph as an extra argument
 *
 * Revision 1.6  2003/11/11 19:46:21  oetiker
 * replaced time_value with rrd_time_value as MacOS X introduced a struct of that name in their standard headers
 *
 * Revision 1.5  2003/04/25 18:35:08  jake
 * Alternate update interface, updatev. Returns info about CDPs written to disk as result of update. Output format is similar to rrd_info, a hash of key-values.
 *
 * Revision 1.4  2003/04/01 22:52:23  jake
 * Fix Win32 build. VC++ 6.0 and 7.0 now use the thread-safe code.
 *
 * Revision 1.3  2003/02/13 07:05:27  oetiker
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
 * Revision 1.2  2002/05/07 21:58:32  oetiker
 * new command rrdtool xport integrated
 * --  Wolfgang Schrimm <Wolfgang.Schrimm@urz.uni-heidelberg.de>
 *
 * Revision 1.1.1.1  2001/02/25 22:25:05  oetiker
 * checkin
 *
 *****************************************************************************/
#ifdef  __cplusplus
extern    "C" {
#endif

#ifndef _RRDLIB_H
#define _RRDLIB_H

#include <sys/types.h>  /* for off_t */
#include <unistd.h>     /* for off_t */
#include <time.h>
#include <stdio.h>      /* for FILE */


/* Formerly rrd_nan_inf.h */
#ifndef DNAN
# define DNAN rrd_set_to_DNAN()
#endif

#ifndef DINF
# define DINF rrd_set_to_DINF()
#endif
    double    rrd_set_to_DNAN(
    void);
    double    rrd_set_to_DINF(
    void);
/* end of rrd_nan_inf.h */

/* Transplanted from rrd_format.h */
    typedef double rrd_value_t; /* the data storage type is
                                 * double */
/* END rrd_format.h */

/* information about an rrd file */
    typedef struct rrd_file_t {
        int       fd;   /* file descriptor if this rrd file */
        char     *file_start;   /* start address of an open rrd file */
        off_t     header_len;   /* length of the header of this rrd file */
        off_t     file_len; /* total size of the rrd file */
        off_t     pos;  /* current pos in file */
    } rrd_file_t;

/* rrd info interface */
    typedef struct rrd_blob_t {
        unsigned long size; /* size of the blob */
        unsigned char *ptr; /* pointer */
    } rrd_blob_t;

    typedef enum rrd_info_type { RD_I_VAL = 0,
        RD_I_CNT,
        RD_I_STR,
        RD_I_INT,
        RD_I_BLO
    } rrd_info_type_t;

    typedef union rrd_infoval {
        unsigned long u_cnt;
        rrd_value_t u_val;
        char     *u_str;
        int       u_int;
        rrd_blob_t u_blo;
    } rrd_infoval_t;

    typedef struct rrd_info_t {
        char     *key;
        rrd_info_type_t type;
        rrd_infoval_t value;
        struct rrd_info_t *next;
    } rrd_info_t;


/* main function blocks */
    int       rrd_create(
    int,
    char **);
    rrd_info_t *rrd_info(
    int,
    char **);
    rrd_info_t *rrd_info_push(
    rrd_info_t *,
    char *,
    rrd_info_type_t,
    rrd_infoval_t);
    void      rrd_info_print(
    rrd_info_t * data);
    void      rrd_info_free(
    rrd_info_t *);
    int       rrd_update(
    int,
    char **);
    rrd_info_t *rrd_update_v(
    int,
    char **);
    int       rrd_graph(
    int,
    char **,
    char ***,
    int *,
    int *,
    FILE *,
    double *,
    double *);
    rrd_info_t *rrd_graph_v(
    int,
    char **);

    int       rrd_fetch(
    int,
    char **,
    time_t *,
    time_t *,
    unsigned long *,
    unsigned long *,
    char ***,
    rrd_value_t **);
    int       rrd_restore(
    int,
    char **);
    int       rrd_dump(
    int,
    char **);
    int       rrd_tune(
    int,
    char **);
    time_t    rrd_last(
    int,
    char **);
    int       rrd_lastupdate(
    int argc,
    char **argv,
    time_t *last_update,
    unsigned long *ds_cnt,
    char ***ds_namv,
    char ***last_ds);
    time_t    rrd_first(
    int,
    char **);
    int       rrd_resize(
    int,
    char **);
    char     *rrd_strversion(
    void);
    double    rrd_version(
    void);
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

/* thread-safe (hopefully) */
    int       rrd_create_r(
    const char *filename,
    unsigned long pdp_step,
    time_t last_up,
    int argc,
    const char **argv);
/* NOTE: rrd_update_r are only thread-safe if no at-style time
   specifications get used!!! */

    int       rrd_update_r(
    const char *filename,
    const char *_template,
    int argc,
    const char **argv);
    int       rrd_fetch_r(
    const char *filename,
    const char *cf,
    time_t *start,
    time_t *end,
    unsigned long *step,
    unsigned long *ds_cnt,
    char ***ds_namv,
    rrd_value_t **data);
    int       rrd_dump_r(
    const char *filename,
    char *outname);
    time_t    rrd_last_r(
    const char *filename);
    time_t    rrd_first_r(
    const char *filename,
    int rraindex);

/* Transplanted from rrd_parsetime.h */
    typedef enum {
        ABSOLUTE_TIME,
        RELATIVE_TO_START_TIME,
        RELATIVE_TO_END_TIME
    } rrd_timetype_t;

#define TIME_OK NULL

    typedef struct rrd_time_value {
        rrd_timetype_t type;
        long      offset;
        struct tm tm;
    } rrd_time_value_t;

    char     *rrd_parsetime(
    const char *spec,
    rrd_time_value_t * ptv);
/* END rrd_parsetime.h */

    typedef struct rrd_context {
        char      lib_errstr[256];
        char      rrd_error[4096];
    } rrd_context_t;

/* returns the current per-thread rrd_context */
    rrd_context_t *rrd_get_context(
    void);


    int       rrd_proc_start_end(
    rrd_time_value_t *,
    rrd_time_value_t *,
    time_t *,
    time_t *);

/* HELPER FUNCTIONS */
    void      rrd_set_error(
    char *,
    ...);
    void      rrd_clear_error(
    void);
    int       rrd_test_error(
    void);
    char     *rrd_get_error(
    void);

    /* rrd_strerror is thread safe, but still it uses a global buffer
       (but one per thread), thus subsequent calls within a single
       thread overwrite the same buffer */
    const char *rrd_strerror(
    int err);

/** MULTITHREADED HELPER FUNCTIONS */
    rrd_context_t *rrd_new_context(
    void);
    void      rrd_free_context(
    rrd_context_t * buf);

/* void   rrd_set_error_r  (rrd_context_t *, char *, ...); */
/* void   rrd_clear_error_r(rrd_context_t *); */
/* int    rrd_test_error_r (rrd_context_t *); */
/* char  *rrd_get_error_r  (rrd_context_t *); */

#endif                  /* _RRDLIB_H */

#ifdef  __cplusplus
}
#endif
