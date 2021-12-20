/**
 * RRDtool - src/rrd_client.h
 * Copyright (C) 2008-2010  Florian octo Forster
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *   Florian octo Forster <octo at verplant.org>
 **/

#ifndef RRD_CLIENT_H_887A36401735431B937F44DFCAE0B7C9
#define RRD_CLIENT_H_887A36401735431B937F44DFCAE0B7C9 1

#include <stdint.h>

/* max length of socket command or response */
#define RRD_CMD_MAX 4096

#ifndef RRDCACHED_DEFAULT_ADDRESS
# define RRDCACHED_DEFAULT_ADDRESS "unix:/tmp/rrdcached.sock"
#endif

#define RRDCACHED_DEFAULT_PORT "42217"
#define ENV_RRDCACHED_ADDRESS "RRDCACHED_ADDRESS"
#define ENV_RRDCACHED_STRIPPATH "RRDCACHED_STRIPPATH"

struct rrd_client;
typedef struct rrd_client rrd_client_t;

struct rrdc_stats_s
{
  const char *name;
  uint16_t type;
#define RRDC_STATS_TYPE_GAUGE   0x0001
#define RRDC_STATS_TYPE_COUNTER 0x0002
  uint16_t flags;
  union
  {
    uint64_t counter;
    double   gauge;
  } value;
  struct rrdc_stats_s *next;
};
typedef struct rrdc_stats_s rrdc_stats_t;

rrd_client_t *rrd_client_new(const char *addr);
void rrd_client_destroy(rrd_client_t *client);

int rrd_client_connect(rrd_client_t *client, const char *addr);
int rrd_client_is_connected(rrd_client_t *client);
int rrd_client_ping(rrd_client_t *client);
const char *rrd_client_address(rrd_client_t *client);
int rrd_client_disconnect(rrd_client_t *client);

int rrd_client_update(rrd_client_t *client, const char *filename, int values_num,
    const char * const *values);

rrd_info_t * rrd_client_info(rrd_client_t *client, const char *filename);
char *rrd_client_list(rrd_client_t *client, int recursive, const char *dirname);
time_t rrd_client_last(rrd_client_t *client, const char *filename);
time_t rrd_client_first(rrd_client_t *client, const char *filename, int rraindex);
int rrd_client_create(rrd_client_t *client, const char *filename,
    unsigned long pdp_step,
    time_t last_up,
    int no_overwrite,
    int argc,
    const char **argv);

int rrd_client_create_r2(rrd_client_t *client, const char *filename,
    unsigned long pdp_step,
    time_t last_up,
    int no_overwrite,
    const char **sources,
    const char *_template,
    int argc,
    const char **argv);

int rrd_client_flush(rrd_client_t *client, const char *filename);
int rrd_client_forget(rrd_client_t *client, const char *filename);
int rrd_client_flushall(rrd_client_t *client);

int rrd_client_fetch(rrd_client_t *client, const char *filename,
    const char *cf,
    time_t *ret_start, time_t *ret_end,
    unsigned long *ret_step,
    unsigned long *ret_ds_num,
    char ***ret_ds_names,
    rrd_value_t **ret_data);

int rrd_client_tune(rrd_client_t *client, const char *filename,
    int argc,
    const char **argv);

int rrd_client_stats_get(rrd_client_t *client, rrdc_stats_t **ret_stats);

/*
 * Simple interface:
 */

int rrdc_connect(const char *addr);
int rrdc_is_connected(const char *daemon_addr);
int rrdc_is_any_connected(void);
int rrdc_ping(void);
int rrdc_disconnect(void);

int rrdc_update (const char *filename, int values_num,
    const char * const *values);

rrd_info_t * rrdc_info (const char *filename);
char *rrdc_list(int recursive, const char *dirname);
time_t rrdc_last (const char *filename);
time_t rrdc_first (const char *filename, int rraindex);
int rrdc_create (const char *filename,
    unsigned long pdp_step,
    time_t last_up,
    int no_overwrite,
    int argc,
    const char **argv);

int rrdc_create_r2 (const char *filename,
    unsigned long pdp_step,
    time_t last_up,
    int no_overwrite,
    const char **sources,
    const char *_template,
    int argc,
    const char **argv);

int rrdc_flush (const char *filename);
int rrdc_forget (const char *filename);
int rrdc_flush_if_daemon (const char *opt_daemon, const char *filename);
int rrdc_flushall (void);
int rrdc_flushall_if_daemon (const char *opt_daemon);

int rrdc_tune (const char *filename,
    int argc,
    const char **argv);

int rrdc_fetch (const char *filename,
    const char *cf,
    time_t *ret_start, time_t *ret_end,
    unsigned long *ret_step,
    unsigned long *ret_ds_num,
    char ***ret_ds_names,
    rrd_value_t **ret_data);

int rrdc_stats_get (rrdc_stats_t **ret_stats);
void rrdc_stats_free (rrdc_stats_t *ret_stats);

#endif /* __RRD_CLIENT_H */
/*
 * vim: set sw=2 sts=2 ts=8 et fdm=marker :
 */
