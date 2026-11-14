/*
 * Feed the rrdcached client canned daemon replies over a socketpair and
 * check that it parses valid replies, rejects malformed ones, and fails
 * cleanly when an allocation fails at any point while reading a reply.
 *
 * The client source is included so that its allocation calls go through
 * the shims below and the fake daemon can be attached to a client
 * directly.
 */
#include "rrd_tool.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

/* Fail the n-th allocation after alloc_countdown is set to n. */
static long alloc_countdown;
static int alloc_failed;

static int alloc_fails_now(
    void)
{
    if (alloc_countdown > 0 && --alloc_countdown == 0) {
        alloc_failed = 1;
        return 1;
    }
    return 0;
}

static void *test_malloc(
    size_t size)
{
    return alloc_fails_now() ? NULL : malloc(size);
}

static void *test_calloc(
    size_t nmemb,
    size_t size)
{
    return alloc_fails_now() ? NULL : calloc(nmemb, size);
}

static void *test_realloc(
    void *ptr,
    size_t size)
{
    return alloc_fails_now() ? NULL : realloc(ptr, size);
}

static char *test_strdup(
    const char *s)
{
    return alloc_fails_now() ? NULL : strdup(s);
}

/* A reply line can never come close to SIZE_MAX bytes; report that length
 * for one marker line to reach the size checks of rrd_client_list. */
static size_t test_strlen(
    const char *s)
{
    return strcmp(s, "HUGE-LINE") == 0 ? SIZE_MAX - 1 : strlen(s);
}

#undef malloc
#undef calloc
#undef realloc
#undef strdup
#undef strlen
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define strdup test_strdup
#define strlen test_strlen
#include "../src/rrd_client.c"
#undef malloc
#undef calloc
#undef realloc
#undef strdup
#undef strlen

static int failed;

#define CHECK(c, ...) do { if (!(c)) { fprintf(stderr, "FAILED: " __VA_ARGS__); fputc('\n', stderr); failed = 1; } } while (0)

enum op { OP_FETCH, OP_INFO, OP_LIST, OP_LASTUPDATE, OP_STATS };

struct outcome {
    int       status;
    int       connected;
    unsigned long step;
    unsigned long ds_num;
    rrd_value_t value;
    char      name[64];
};

/* Send one request to a fake daemon that has already written `reply' and
 * closed its sending side, and record what the client made of it. */
static void exchange(
    enum op op,
    const char *reply,
    size_t reply_len,
    long fail_alloc,
    struct outcome *out)
{
    rrd_client_t *client;
    int       pair[2];
    unsigned long i;

    memset(out, 0, sizeof(*out));
    out->status = -100;
    client = rrd_client_new(NULL);
    CHECK(client != NULL && socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0,
          "fake daemon setup");
    CHECK(write(pair[1], reply, reply_len) == (ssize_t) reply_len &&
          shutdown(pair[1], SHUT_WR) == 0, "fake daemon reply");
    client->sd = pair[0];
    client->sd_path = strdup("fake:42");

    alloc_failed = 0;
    alloc_countdown = fail_alloc;
    switch (op) {
    case OP_FETCH: {
        time_t    start = 1000000000, end = 1000000120;
        char    **names = NULL;
        rrd_value_t *data = NULL;

        out->status = rrd_client_fetch(client, "x.rrd", "AVERAGE", &start,
                                       &end, &out->step, &out->ds_num,
                                       &names, &data);
        alloc_countdown = 0;
        if (out->status == 0) {
            out->value = data[out->ds_num * 2 - 1];
            snprintf(out->name, sizeof(out->name), "%s", names[out->ds_num - 1]);
        }
        CHECK(out->status == 0 || (names == NULL && data == NULL),
              "fetch failure left outputs set");
        for (i = 0; names != NULL && i < out->ds_num; i++)
            free(names[i]);
        free(names);
        free(data);
        break;
    }
    case OP_INFO: {
        rrd_info_t *info = rrd_client_info(client, "x.rrd");
        rrd_info_t *p;

        alloc_countdown = 0;
        out->status = info == NULL ? -1 : 0;
        for (p = info; p != NULL; p = p->next) {
            out->ds_num++;
            if (p->type == RD_I_INT)
                out->value = p->value.u_int;
            if (p->type == RD_I_STR)
                snprintf(out->name, sizeof(out->name), "%s", p->value.u_str);
        }
        rrd_info_free(info);
        break;
    }
    case OP_LIST: {
        char     *list = rrd_client_list(client, 0, "/");

        alloc_countdown = 0;
        out->status = list == NULL ? -1 : 0;
        snprintf(out->name, sizeof(out->name), "%s", list ? list : "");
        free(list);
        break;
    }
    case OP_LASTUPDATE: {
        time_t    last = 0;
        char    **names = NULL, **values = NULL;

        out->status = rrd_client_lastupdate(client, "x.rrd", &last,
                                            &out->ds_num, &names, &values);
        alloc_countdown = 0;
        out->step = (unsigned long) last;
        if (out->status == 0)
            snprintf(out->name, sizeof(out->name), "%s=%s",
                     names[out->ds_num - 1], values[out->ds_num - 1]);
        CHECK(out->status == 0 || (out->ds_num == 0 && names == NULL &&
                                   values == NULL),
              "lastupdate failure left outputs set");
        for (i = 0; names != NULL && i < out->ds_num; i++) {
            free(names[i]);
            free(values[i]);
        }
        free(names);
        free(values);
        break;
    }
    case OP_STATS: {
        rrdc_stats_t *stats = NULL;
        rrdc_stats_t *p;

        out->status = rrd_client_stats_get(client, &stats);
        alloc_countdown = 0;
        CHECK(out->status == 0 || stats == NULL,
              "stats failure left outputs set");
        for (p = stats; p != NULL; p = p->next) {
            out->ds_num++;
            if (p->type == RRDC_STATS_TYPE_COUNTER)
                out->value = (rrd_value_t) p->value.counter;
        }
        rrdc_stats_free(stats);
        break;
    }
    }
    out->connected = rrd_client_is_connected(client);
    rrd_client_destroy(client);
    close(pair[1]);
}

/* The reply must be rejected and the connection dropped. */
static void reject(
    enum op op,
    const char *reply,
    size_t reply_len,
    const char *what)
{
    struct outcome out;

    exchange(op, reply, reply_len, 0, &out);
    CHECK(out.status != 0 && !out.connected, "%s was accepted (status %d)",
          what, out.status);
}

/* Fail each allocation the exchange makes in turn; every one must make
 * the call fail. Returns the number of allocations injected. */
static long fail_each_allocation(
    enum op op,
    const char *reply,
    const char *what)
{
    struct outcome out;
    long      n;

    for (n = 1; n < 1000; n++) {
        exchange(op, reply, strlen(reply), n, &out);
        if (!alloc_failed)
            break;
        CHECK(out.status != 0, "%s succeeded with allocation %ld failing",
              what, n);
    }
    CHECK(n > 1 && n < 1000 && out.status == 0,
          "%s: %ld allocations, status %d", what, n - 1, out.status);
    return n - 1;
}

#define REJECT(op, reply, what) reject(op, reply, sizeof(reply) - 1, what)
#define EXCHANGE(op, reply, out) exchange(op, reply, sizeof(reply) - 1, 0, out)

#define FETCH_HEAD "8 fetch\nFlushVersion: 1\n"
#define FETCH_TIMES "Start: 1000000000\nEnd: 1000000120\n"
#define FETCH_DS "Step: 60\nDSCount: 2\nDSName: a b\n"
#define FETCH_ROWS "1000000060: 1 2\n1000000120: 3 4\n"

int main(
    void)
{
    struct outcome out;
    time_t    t;
    rrdc_stats_t *stats;
    char      line[64];
    char     *key;
    char     *names[1];
    rrd_value_t values[1];

    unsetenv("RRDCACHED_STRIPPATH");

    /* valid replies */
    EXCHANGE(OP_FETCH, FETCH_HEAD FETCH_TIMES FETCH_DS FETCH_ROWS, &out);
    CHECK(out.status == 0 && out.step == 60 && out.ds_num == 2 &&
          out.value == 4 && strcmp(out.name, "b") == 0 && out.connected,
          "valid fetch (status %d)", out.status);

    EXCHANGE(OP_INFO, "4 info\nfilename 2 x.rrd\nstep 1 300\nrate 0 1.5\n"
             "offset 3 -5\n", &out);
    CHECK(out.status == 0 && out.ds_num == 4 && out.value == -5 &&
          strcmp(out.name, "x.rrd") == 0 && out.connected, "valid info");

    EXCHANGE(OP_LIST, "2 list\na.rrd\nb.rrd\n", &out);
    CHECK(out.status == 0 && strcmp(out.name, "a.rrd\nb.rrd\n") == 0,
          "valid list");

    EXCHANGE(OP_LASTUPDATE, "2 1000000000 LastUpdate values follow\n"
             "a 1\nb 42\n", &out);
    CHECK(out.status == 0 && out.ds_num == 2 && out.step == 1000000000 &&
          strcmp(out.name, "b=42") == 0, "valid lastupdate");

    EXCHANGE(OP_STATS, "2 Statistics follow\nQueueLength: 3\n"
             "UpdatesWritten: 7\n", &out);
    CHECK(out.status == 0 && out.ds_num == 2 && out.value == 7,
          "valid stats");

    /* malformed replies */
    REJECT(OP_FETCH, "12 abc\0def", "a status line with a NUL byte");
    REJECT(OP_FETCH, FETCH_HEAD "Start 1000000000\nEnd: 1000000120\n"
           FETCH_DS FETCH_ROWS, "a Start header without a colon");
    REJECT(OP_FETCH, FETCH_HEAD "Start: 99999999999999999999\n"
           "End: 1000000120\n" FETCH_DS FETCH_ROWS, "an overflowing Start");
    REJECT(OP_FETCH, FETCH_HEAD FETCH_TIMES FETCH_DS
           "-1000000060: 1 2\n1000000120: 3 4\n", "a negative timestamp");
    REJECT(OP_INFO, "1 info\nstep 9 1\n", "an unknown info type");
    REJECT(OP_INFO, "1 info\nrate 0 abc\n", "a malformed info value");
    REJECT(OP_STATS, "1 Statistics follow\nGarbage\n", "a line without key");
    REJECT(OP_STATS, "1 Statistics follow\nUpdatesWritten: x\n",
           "a non-numeric counter");
    REJECT(OP_STATS, "1 Statistics follow\nQueueLength: abc\n",
           "a non-numeric gauge");

    EXCHANGE(OP_LIST, "2 list\na.rrd\nHUGE-LINE\n", &out);
    CHECK(out.status != 0, "an oversized list was accepted");

    /* allocation failures while handling a reply */
    fail_each_allocation(OP_FETCH, FETCH_HEAD FETCH_TIMES FETCH_DS FETCH_ROWS,
                         "fetch");
    fail_each_allocation(OP_INFO, "1 info\nfilename 2 x.rrd\n", "info");
    fail_each_allocation(OP_LASTUPDATE, "2 1000000000 LastUpdate values "
                         "follow\na 1\nb 42\n", "lastupdate");
    fail_each_allocation(OP_STATS, "2 Statistics follow\nQueueLength: 3\n"
                         "UpdatesWritten: 7\n", "stats");

    /* argument checks */
    stats = NULL;
    CHECK(rrd_client_stats_get(NULL, &stats) == EINVAL && stats == NULL,
          "stats without a client");
    CHECK(parse_time_value_suffix(NULL, "", &t) == -1,
          "time value without a string");
    /* the fetch parser never asks for zero or too many fields */
    snprintf(line, sizeof(line), "DSName: a");
    CHECK(parse_char_array_header(line, &key, names, 0, 0) == -1 &&
          parse_char_array_header(line, &key, names, SIZE_MAX, 0) == -1,
          "name array of an invalid size");
    snprintf(line, sizeof(line), "1000000060: 1");
    CHECK(parse_value_array_header(line, &t, values, 0) == -1 &&
          parse_value_array_header(line, &t, values, SIZE_MAX) == -1,
          "value array of an invalid size");

    return failed;
}
