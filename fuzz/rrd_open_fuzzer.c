/*****************************************************************************
 * RRDtool 1.11.0 Copyright by Tobi Oetiker, 1997-2026
 *****************************************************************************
 * rrd_open_fuzzer.c  libFuzzer harness for rrd_open()
 *****************************************************************************
 * rrd_open() parses untrusted .rrd files from disk and is the entry point
 * for the whole header/DS/RRA layout. It reads through mmap or seek+read
 * (see __rrd_read_mmap/__rrd_read_seq in rrd_open.c), so unlike most
 * fuzzable parsers it cannot take the fuzzer's buffer directly -- each
 * iteration has to spill it to a real file first.
 *
 * rrd_open() accepting a file is only half of it: info, dump, first/last,
 * lastupdate and update then interpret the header fields (Holt-Winters
 * links, COMPUTE programs, row pointers), so a file that opens is run
 * through each of them too.
 *****************************************************************************/

#include "rrd_tool.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

/* bounds the update argument; files with more DS still go through the rest */
#define MAX_UPDATE_DS 64

static void run_consumers(
    const char *path,
    unsigned long ds_cnt,
    time_t last_up)
{
    rrd_info_t *info;
    time_t    last;
    unsigned long cnt, i;
    char    **names, **values;

    info = rrd_info_r(path);
    rrd_info_free(info);
    rrd_clear_error();

    rrd_dump_r(path, "/dev/null");
    rrd_clear_error();

    rrd_first_r(path, 0);
    rrd_last_r(path);
    rrd_clear_error();

    if (rrd_lastupdate_r(path, &last, &cnt, &names, &values) == 0) {
        for (i = 0; i < cnt; i++) {
            free(names[i]);
            free(values[i]);
        }
        free(names);
        free(values);
    }
    rrd_clear_error();

    /* last_up comes from the file; keep the next second representable */
    if (ds_cnt <= MAX_UPDATE_DS && last_up >= 0
        && (long long) last_up < LLONG_MAX) {
        char      arg[32 + 2 * MAX_UPDATE_DS];
        const char *argv[1] = { arg };
        int       off = snprintf(arg, sizeof(arg), "%lld",
                                 (long long) last_up + 1);

        for (i = 0; i < ds_cnt && off > 0
             && (size_t) off < sizeof(arg) - 2; i++)
            off += snprintf(arg + off, sizeof(arg) - off, ":1");
        rrd_update_r(path, NULL, 1, argv);
        rrd_clear_error();
    }
}

int LLVMFuzzerTestOneInput(
    const uint8_t *data,
    size_t size)
{
    const char *tmpdir = getenv("TMPDIR");
    char     *path;
    int       fd;
    size_t    written;
    rrd_t     rrd;
    rrd_file_t *rrd_file;

    if (tmpdir == NULL || *tmpdir == '\0')
        tmpdir = "/tmp";
    size_t path_size = strlen(tmpdir) + sizeof("/rrd_open_fuzz.XXXXXX");
    path = malloc(path_size);
    if (path == NULL) {
        return 0;
    }
    snprintf(path, path_size, "%s/rrd_open_fuzz.XXXXXX", tmpdir);

    fd = mkstemp(path);
    if (fd < 0) {
        free(path);
        return 0;
    }

    /* short writes can't happen for a regular file barring ENOSPC/EINTR,
     * but check anyway rather than feed rrd_open() a truncated fixture */
    written = 0;
    while (written < size) {
        ssize_t   n = write(fd, data + written, size - written);

        if (n <= 0) {
            if (n < 0 && errno == EINTR)
                continue;
            break;
        }
        written += (size_t) n;
    }
    {
        int status = close(fd);
        if (written != size || status != 0)
            goto cleanup;
    }

    rrd_clear_error();
    rrd_init(&rrd);
    rrd_file = rrd_open(path, &rrd, RRD_READONLY | RRD_LOCK_NONE);
    if (rrd_file != NULL) {
        unsigned long ds_cnt = rrd.stat_head->ds_cnt;
        time_t    last_up = rrd.live_head->last_up;

        rrd_free(&rrd);
        rrd_close(rrd_file);
        run_consumers(path, ds_cnt, last_up);
    } else
        rrd_free(&rrd);

  cleanup:
    unlink(path);
    free(path);

    return 0;
}
