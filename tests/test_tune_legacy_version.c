/* rrdtool tune --smoothing-window on a version 0002 RRD.
 *
 * Files before version 3 store the live header as a bare time_t.  tune
 * must not relabel such a file as version 0004 in place, or every later
 * read finds the rest of the file at the wrong offset.  The legacy file
 * is written here from a freshly created RRD, so the layout matches the
 * platform the test runs on. */

#ifdef HAVE_CONFIG_H
#include "rrd_config.h"
#endif

#include "rrd_tool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fail(
    const char *message)
{
    fprintf(stderr, "%s", message);
    if (rrd_test_error())
        fprintf(stderr, ": %s", rrd_get_error());
    fputc('\n', stderr);
    return 1;
}

static int temp_path(
    char *path,
    size_t size,
    const char *builddir,
    const char *tag)
{
    int       status;
    int       fd;

    status = snprintf(path, size, "%s/tune-legacy-%s-XXXXXX", builddir, tag);
    if (status < 0 || (size_t) status >= size)
        return -1;
    fd = mkstemp(path);
    if (fd < 0)
        return -1;
    close(fd);
    return 0;
}

/* Write rrd in the version 0002 layout: the live header is a bare time_t. */
static int write_v2(
    const char *path,
    rrd_t *rrd)
{
    stat_head_t head = *rrd->stat_head;
    unsigned long ds_cnt = rrd->stat_head->ds_cnt;
    unsigned long rra_cnt = rrd->stat_head->rra_cnt;
    unsigned long rows = 0;
    unsigned long i;
    time_t    last_up = rrd->live_head->last_up;
    FILE     *fh;
    int       ok = 1;

    for (i = 0; i < rra_cnt; i++)
        rows += rrd->rra_def[i].row_cnt;
    strcpy(head.version, "0002");

    fh = fopen(path, "wb");
    if (fh == NULL)
        return -1;
    ok &= fwrite(&head, sizeof(head), 1, fh) == 1;
    ok &= fwrite(rrd->ds_def, sizeof(ds_def_t), ds_cnt, fh) == ds_cnt;
    ok &= fwrite(rrd->rra_def, sizeof(rra_def_t), rra_cnt, fh) == rra_cnt;
    ok &= fwrite(&last_up, sizeof(last_up), 1, fh) == 1;
    ok &= fwrite(rrd->pdp_prep, sizeof(pdp_prep_t), ds_cnt, fh) == ds_cnt;
    ok &= fwrite(rrd->cdp_prep, sizeof(cdp_prep_t), ds_cnt * rra_cnt, fh)
        == ds_cnt * rra_cnt;
    ok &= fwrite(rrd->rra_ptr, sizeof(rra_ptr_t), rra_cnt, fh) == rra_cnt;
    ok &= fwrite(rrd->rrd_value, sizeof(rrd_value_t), rows * ds_cnt, fh)
        == rows * ds_cnt;
    ok &= fclose(fh) == 0;
    return ok ? 0 : -1;
}

int main(void)
{
    const char *create_args[] = {
        "DS:a:GAUGE:120:0:U",
        "RRA:AVERAGE:0.5:1:10",
        "RRA:HWPREDICT:20:0.1:0.0035:5"
    };
    const char *builddir;
    char      path3[4096];
    char      path2[4096];
    char      update_buf[20][64];
    const char *update_args[20];
    const char *tune_args[4];
    rrd_t     orig;
    rrd_t     tuned;
    rrd_file_t *orig_file = NULL;
    rrd_file_t *tuned_file = NULL;
    unsigned long rows = 0;
    unsigned long i;
    int       seasonal = -1;
    int       rc = 1;

    builddir = getenv("BUILDDIR");
    if (builddir == NULL)
        builddir = ".";
    if (temp_path(path3, sizeof(path3), builddir, "v3") != 0)
        return fail("could not reserve a temporary RRD path");
    if (temp_path(path2, sizeof(path2), builddir, "v2") != 0) {
        remove(path3);
        return fail("could not reserve a temporary RRD path");
    }

    rrd_init(&orig);
    rrd_init(&tuned);

    rrd_clear_error();
    if (rrd_create_r(path3, 60, 1300000000, 3, create_args) != 0) {
        fail("could not create the test RRD");
        goto out;
    }
    for (i = 0; i < 20; i++) {
        snprintf(update_buf[i], sizeof(update_buf[i]), "%lu:%lu",
                 1300000060UL + 60 * i, (i * 37) % 97);
        update_args[i] = update_buf[i];
    }
    if (rrd_update_r(path3, NULL, 20, update_args) != 0) {
        fail("could not update the test RRD");
        goto out;
    }

    /* keep the files open: with mmap the values live in the mapping */
    orig_file = rrd_open(path3, &orig, RRD_READONLY | RRD_READVALUES);
    if (orig_file == NULL) {
        fail("could not read the test RRD");
        goto out;
    }
    if (write_v2(path2, &orig) != 0) {
        fail("could not write the version 0002 RRD");
        goto out;
    }
    for (i = 0; i < orig.stat_head->rra_cnt; i++) {
        rows += orig.rra_def[i].row_cnt;
        if (strcmp(orig.rra_def[i].cf_nam, "SEASONAL") == 0)
            seasonal = (int) i;
    }
    if (seasonal < 0) {
        fail("test RRD has no SEASONAL RRA");
        goto out;
    }

    tune_args[0] = "tune";
    tune_args[1] = "--smoothing-window";
    tune_args[2] = "0.1";
    tune_args[3] = path2;
    rrd_clear_error();
    if (rrd_tune_r(path2, 4, tune_args) != 0) {
        fail("tune --smoothing-window failed on a version 0002 RRD");
        goto out;
    }

    rrd_clear_error();
    tuned_file = rrd_open(path2, &tuned, RRD_READONLY | RRD_READVALUES);
    if (tuned_file == NULL) {
        fail("tuned version 0002 RRD cannot be opened");
        goto out;
    }

    if (atoi(tuned.stat_head->version) < 4) {
        fprintf(stderr, "tuned RRD has version %s, expected 0004 or later\n",
                tuned.stat_head->version);
        goto out;
    }
    if (tuned.live_head->last_up != orig.live_head->last_up) {
        fprintf(stderr, "last update changed from %lld to %lld\n",
                (long long) orig.live_head->last_up,
                (long long) tuned.live_head->last_up);
        goto out;
    }
    if (tuned.stat_head->ds_cnt != orig.stat_head->ds_cnt
        || tuned.stat_head->rra_cnt != orig.stat_head->rra_cnt) {
        fail("tuned RRD has a different shape");
        goto out;
    }
    if (tuned.rra_def[seasonal].par[RRA_seasonal_smoothing_window].u_val
        != 0.1) {
        fail("smoothing window was not stored");
        goto out;
    }
    if (memcmp(tuned.pdp_prep, orig.pdp_prep,
               sizeof(pdp_prep_t) * orig.stat_head->ds_cnt) != 0
        || memcmp(tuned.cdp_prep, orig.cdp_prep,
                  sizeof(cdp_prep_t) * orig.stat_head->ds_cnt
                  * orig.stat_head->rra_cnt) != 0
        || memcmp(tuned.rra_ptr, orig.rra_ptr,
                  sizeof(rra_ptr_t) * orig.stat_head->rra_cnt) != 0
        || memcmp(tuned.rrd_value, orig.rrd_value,
                  sizeof(rrd_value_t) * rows * orig.stat_head->ds_cnt) != 0) {
        fail("tuned RRD does not hold the original data");
        goto out;
    }
    rc = 0;

  out:
    if (tuned_file != NULL)
        rrd_close(tuned_file);
    rrd_free(&tuned);
    if (orig_file != NULL)
        rrd_close(orig_file);
    rrd_free(&orig);
    remove(path3);
    remove(path2);
    return rc;
}
