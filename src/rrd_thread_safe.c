/*****************************************************************************
 * RRDtool 1.3.2  Copyright by Tobi Oetiker, 1997-2008
 * This file:     Copyright 2003 Peter Stamfest <peter@stamfest.at> 
 *                             & Tobias Oetiker
 * Distributed under the GPL
 *****************************************************************************
 * rrd_thread_safe.c   Contains routines used when thread safety is required
 *****************************************************************************
 * $Id$
 *************************************************************************** */

#include <pthread.h>
#include <string.h>
/* #include <error.h> */
#include "rrd.h"
#include "rrd_tool.h"

/* Key for the thread-specific rrd_context */
static pthread_key_t context_key;

/* Once-only initialisation of the key */
static pthread_once_t context_key_once = PTHREAD_ONCE_INIT;

/* Free the thread-specific rrd_context - we might actually use
   rrd_free_context instead...
 */
static void context_destroy_context(
    void *ctx_)
{
    rrd_context_t *ctx = ctx_;

    if (ctx)
        rrd_free_context(ctx);
}

/* Allocate the key */
static void context_get_key(
    void)
{
    pthread_key_create(&context_key, context_destroy_context);
}

rrd_context_t *rrd_get_context(
    void)
{
    rrd_context_t *ctx;

    pthread_once(&context_key_once, context_get_key);
    ctx = pthread_getspecific(context_key);
    if (!ctx) {
        ctx = rrd_new_context();
        pthread_setspecific(context_key, ctx);
    }
    return ctx;
}

#ifdef HAVE_STRERROR_R
const char *rrd_strerror(
    int err)
{
    rrd_context_t *ctx = rrd_get_context();

    if (strerror_r(err, ctx->lib_errstr, sizeof(ctx->lib_errstr)))
        return "strerror_r failed. sorry!";
    else
        return ctx->lib_errstr;
}
#else
#undef strerror
const char *rrd_strerror(
    int err)
{
    static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    rrd_context_t *ctx;

    ctx = rrd_get_context();
    pthread_mutex_lock(&mtx);
    strncpy(ctx->lib_errstr, strerror(err), sizeof(ctx->lib_errstr));
    ctx->lib_errstr[sizeof(ctx->lib_errstr) - 1] = '\0';
    pthread_mutex_unlock(&mtx);
    return ctx->lib_errstr;
}
#endif
