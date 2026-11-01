/*****************************************************************************
 * RRDtool 1.11.0 Copyright by Tobi Oetiker, 1997-2026
 *****************************************************************************
 * rrd_validate.h  Structural invariants of an RRD header
 *****************************************************************************
 * Internal: not installed and not exported from librrd.
 *
 * rrd_open() enforces these on every file it reads, and rrd_restore()
 * enforces the same ones before it writes, so restore cannot produce a
 * file that open rejects.  `rrdtool verify` reaches them through rrd_open
 * and shares rrd_data_len(); it links only the exported librrd symbols,
 * which is why the pieces it needs are static inline.
 *****************************************************************************/
#ifndef RRD_VALIDATE_H
#define RRD_VALIDATE_H

#include "rrd_tool.h"

/* ds_cnt / rra_cnt and the record counts derived from them are read
 * straight from an untrusted file.  Every "sizeof(record) * count" used as
 * an offset or allocation size must be range-checked first: a crafted
 * count can otherwise wrap size_t, pass a "> file_len" bounds check, and
 * alias or iterate past the mapped file. */
#if defined(__has_builtin)
#define RRD_HAVE_OVERFLOW_BUILTINS (__has_builtin(__builtin_mul_overflow) && __has_builtin(__builtin_add_overflow))
#elif defined(__GNUC__) && __GNUC__ >= 5
#define RRD_HAVE_OVERFLOW_BUILTINS 1
#else
#define RRD_HAVE_OVERFLOW_BUILTINS 0
#endif

static inline int rrd_mul_overflow(
    size_t a,
    size_t b,
    size_t *out)
{
#if RRD_HAVE_OVERFLOW_BUILTINS
    return __builtin_mul_overflow(a, b, out);
#else
    *out = a * b;
    return b != 0 && a > (size_t) -1 / b;
#endif
}

static inline int rrd_add_overflow(
    size_t a,
    size_t b,
    size_t *out)
{
#if RRD_HAVE_OVERFLOW_BUILTINS
    return __builtin_add_overflow(a, b, out);
#else
    *out = a + b;
    return a > (size_t) -1 - b;
#endif
}

/* Bytes of archive values that follow the header.  Returns nonzero when
 * the row counts make that length overflow size_t. */
static inline int rrd_data_len(
    const rrd_t *rrd,
    size_t *out)
{
    size_t    rows = 0;
    unsigned long i;

    for (i = 0; i < rrd->stat_head->rra_cnt; i++)
        if (rrd_add_overflow(rows, rrd->rra_def[i].row_cnt, &rows))
            return 1;
    return rrd_mul_overflow(rows, rrd->stat_head->ds_cnt, out) ||
        rrd_mul_overflow(*out, sizeof(rrd_value_t), out);
}

/* Each check below sets the rrd error and returns -1 on failure, else 0.
 * They are called in rrd_open() as soon as the section they check has
 * been read; rrd_validate_header() runs them all on a complete rrd_t. */
int       rrd_validate_stat_head(
    const rrd_t *rrd);
int       rrd_validate_failures_windows(
    const rrd_t *rrd);
int       rrd_validate_live_head(
    const rrd_t *rrd,
    const char *file_name);
int       rrd_validate_pdp_prep(
    const rrd_t *rrd,
    const char *file_name);
/* requires a nonzero pdp_step */
int       rrd_validate_rra_sizes(
    const rrd_t *rrd,
    const char *file_name);
int       rrd_validate_header(
    const rrd_t *rrd,
    const char *file_name);

#endif
