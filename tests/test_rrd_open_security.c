#ifdef HAVE_CONFIG_H
#include "rrd_config.h"
#endif

#include "rrd_tool.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(
    const char *message)
{
    fprintf(stderr, "%s", message);
    if (rrd_test_error())
        fprintf(stderr, ": %s", rrd_get_error());
    fputc('\n', stderr);
    return 1;
}

int main(void)
{
#ifndef HAVE_MMAP
    return 77;
#else
    const char *args[] = {
        "DS:value:GAUGE:120:U:U",
        "RRA:AVERAGE:0.5:1:2"
    };
    const char *builddir;
    char      path[4096];
    rrd_t     rrd;
    rrd_file_t *rrd_file;
    int       status;

    builddir = getenv("BUILDDIR");
    if (builddir == NULL)
        builddir = ".";
    status = snprintf(path, sizeof(path), "%s/rrd-open-security.rrd",
                      builddir);
    if (status < 0 || (size_t) status >= sizeof(path))
        return fail("temporary RRD path is too long");

    remove(path);
    rrd_clear_error();
    if (rrd_create_r(path, 60, 1, 2, args) != 0)
        return fail("could not create the test RRD");

    rrd_init(&rrd);
    rrd_file = rrd_open(path, &rrd, RRD_READWRITE | RRD_LOCK_NONE);
    if (rrd_file == NULL) {
        rrd_free(&rrd);
        remove(path);
        return fail("could not open the test RRD for mutation");
    }

    rrd.rra_def[0].row_cnt = ULONG_MAX;
    rrd_close(rrd_file);
    rrd_free(&rrd);

    rrd_clear_error();
    rrd_init(&rrd);
    rrd_file = rrd_open(path, &rrd, RRD_READONLY | RRD_LOCK_NONE);
    if (rrd_file != NULL) {
        rrd_close(rrd_file);
        rrd_free(&rrd);
        remove(path);
        return fail("RRD with an overflowing row count was accepted");
    }
    rrd_free(&rrd);

    if (!rrd_test_error() ||
        strstr(rrd_get_error(), "impossibly large database") == NULL) {
        remove(path);
        return fail("row-count overflow was not diagnosed");
    }

    remove(path);
    return 0;
#endif
}
