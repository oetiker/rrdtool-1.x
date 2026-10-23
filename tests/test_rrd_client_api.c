/*
 * Call the client API declared in the installed header rrd_client.h
 * through the shared librrd, the way a program built against an
 * installed rrdtool does. Every rrd_client_* function, and the rrdc_*
 * functions that were missing from the export list, is referenced here,
 * so this program fails to link if any of them is not exported.
 *
 * usage: rrd-client-api DAEMON-ADDRESS RRD-FILE
 * RRD-FILE must not exist yet; the daemon must accept its path.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "rrd.h"
#include "rrd_client.h"

static int failures = 0;

static void check(
    int ok,
    const char *what)
{
    if (ok) {
        printf("OK: %s\n", what);
    } else {
        printf("FAILED: %s%s%s\n", what,
               rrd_test_error()? ": " : "",
               rrd_test_error()? rrd_get_error() : "");
        failures++;
    }
    rrd_clear_error();
}

static size_t count_bytes(
    const void *data,
    size_t len,
    void *user)
{
    (void) data;
    *(size_t *) user += len;
    return len;
}

int main(
    int argc,
    char **argv)
{
    const char *addr;
    const char *file;
    const char *create_argv[] = {
        "DS:a:GAUGE:600:U:U",
        "RRA:AVERAGE:0.5:1:10"
    };
    const char *tune_argv[] = { "--heartbeat", "a:900" };
    char      value[64];
    const char *values[1];
    rrd_client_t *client;
    rrd_info_t *info;
    rrdc_stats_t *stats;
    char     *list;
    /* on a step boundary: rrdcached reports "last" rounded down to one */
    time_t    start = 1300000200;
    time_t    last_update;
    unsigned long ds_count;
    char    **ds_names;
    char    **last_ds;
    time_t    fstart, fend;
    unsigned long step, ds_num, i;
    rrd_value_t *data;
    size_t    dumped;

    if (argc != 3) {
        fprintf(stderr, "usage: %s DAEMON-ADDRESS RRD-FILE\n", argv[0]);
        return 2;
    }
    addr = argv[1];
    file = argv[2];

    client = rrd_client_new(NULL);
    check(client != NULL, "rrd_client_new without address");
    if (client == NULL)
        return 1;
    check(!rrd_client_is_connected(client), "new client is not connected");

    check(rrd_client_connect(client, addr) == 0, "rrd_client_connect");
    check(rrd_client_is_connected(client), "rrd_client_is_connected");
    check(rrd_client_address(client) != NULL
          && strcmp(rrd_client_address(client), addr) == 0,
          "rrd_client_address");
    check(rrd_client_ping(client), "rrd_client_ping");

    check(rrd_client_create(client, file, 300, start, 1, 2, create_argv) == 0,
          "rrd_client_create");
    check(rrd_client_create_r2(client, file, 300, start, 1, NULL, NULL,
                               2, create_argv) != 0,
          "rrd_client_create_r2 refuses to overwrite");

    snprintf(value, sizeof(value), "%lld:42", (long long) (start + 300));
    values[0] = value;
    check(rrd_client_update(client, file, 1, values) == 0,
          "rrd_client_update");
    check(rrd_client_flush(client, file) == 0, "rrd_client_flush");
    check(rrd_client_flushall(client) == 0, "rrd_client_flushall");

    check(rrd_client_last(client, file) == start + 300, "rrd_client_last");
    check(rrd_client_first(client, file, 0) > 0, "rrd_client_first");

    info = rrd_client_info(client, file);
    check(info != NULL, "rrd_client_info");
    rrd_info_free(info);

    ds_names = last_ds = NULL;
    check(rrd_client_lastupdate(client, file, &last_update, &ds_count,
                                &ds_names, &last_ds) == 0
          && ds_count == 1 && last_update == start + 300
          && strcmp(last_ds[0], "42") == 0, "rrd_client_lastupdate");
    for (i = 0; ds_names != NULL && i < ds_count; i++) {
        free(ds_names[i]);
        free(last_ds[i]);
    }
    free(ds_names);
    free(last_ds);

    fstart = start;
    fend = start + 600;
    ds_names = NULL;
    data = NULL;
    check(rrd_client_fetch(client, file, "AVERAGE", &fstart, &fend, &step,
                           &ds_num, &ds_names, &data) == 0
          && step == 300 && ds_num == 1, "rrd_client_fetch");
    for (i = 0; ds_names != NULL && i < ds_num; i++)
        free(ds_names[i]);
    free(ds_names);
    free(data);

    list = rrd_client_list(client, 0, "/");
    check(list != NULL, "rrd_client_list");
    free(list);

    check(rrd_client_tune(client, file, 2, tune_argv) == 0,
          "rrd_client_tune");

    stats = NULL;
    check(rrd_client_stats_get(client, &stats) == 0 && stats != NULL,
          "rrd_client_stats_get");
    rrdc_stats_free(stats);

    check(rrd_client_forget(client, file) == 0, "rrd_client_forget");

    /* the daemon ends a dump by closing the connection */
    dumped = 0;
    check(rrd_client_dump(client, file, "none", count_bytes, &dumped) == 0
          && dumped > 0, "rrd_client_dump");
    check(rrd_client_connect(client, addr) == 0, "reconnect after dump");

    check(rrd_client_disconnect(client) == 0
          && !rrd_client_is_connected(client), "rrd_client_disconnect");
    rrd_client_destroy(client);

    /* the simple interface: functions that were declared but not exported */
    check(rrdc_connect(addr) == 0, "rrdc_connect");
    check(rrdc_tune(file, 2, tune_argv) == 0, "rrdc_tune");
    dumped = 0;
    check(rrdc_dump(file, "none", count_bytes, &dumped) == 0
          && dumped > 0, "rrdc_dump");
    rrdc_disconnect();

    return failures ? 1 : 0;
}
