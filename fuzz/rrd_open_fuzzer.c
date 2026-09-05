/*****************************************************************************
 * RRDtool 1.10.3 Copyright by Tobi Oetiker, 1997-2026
 *****************************************************************************
 * rrd_open_fuzzer.c  libFuzzer harness for rrd_open()
 *****************************************************************************
 * rrd_open() parses untrusted .rrd files from disk and is the entry point
 * for the whole header/DS/RRA layout. It reads through mmap or seek+read
 * (see __rrd_read_mmap/__rrd_read_seq in rrd_open.c), so unlike most
 * fuzzable parsers it cannot take the fuzzer's buffer directly -- each
 * iteration has to spill it to a real file first.
 *****************************************************************************/

#include "rrd_tool.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int LLVMFuzzerTestOneInput(
    const uint8_t *data,
    size_t size)
{
    const char *tmpdir = getenv("TMPDIR");
    char      path[PATH_MAX];
    int       fd;
    size_t    written;
    rrd_t     rrd;
    rrd_file_t *rrd_file;

    if (tmpdir == NULL || *tmpdir == '\0')
        tmpdir = "/tmp";
    if (snprintf(path, sizeof(path), "%s/rrd_open_fuzz.XXXXXX",
                 tmpdir) >= (int) sizeof(path))
        return 0;

    fd = mkstemp(path);
    if (fd < 0)
        return 0;

    /* short writes can't happen for a regular file barring ENOSPC/EINTR,
     * but check anyway rather than feed rrd_open() a truncated fixture */
    written = 0;
    while (written < size) {
        ssize_t   n = write(fd, data + written, size - written);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        written += (size_t) n;
    }
    close(fd);

    rrd_clear_error();
    rrd_init(&rrd);
    rrd_file = rrd_open(path, &rrd, RRD_READONLY | RRD_LOCK_NONE);
    if (rrd_file != NULL)
        rrd_close(rrd_file);
    rrd_free(&rrd);

    unlink(path);

    return 0;
}
