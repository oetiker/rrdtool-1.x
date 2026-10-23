/* rrdtool tune --aberrant-reset on a version 0002 RRD.
 *
 * Files before version 3 store the live header as a bare time_t, which
 * is shorter than the live header of later versions.  The reset must find
 * the cdp_prep area and the SEASONAL rows at their real offsets, so
 * resetting a version 0002 file has to change exactly what resetting the
 * same data in a version 0003 file changes.  The legacy file is written
 * here from a freshly created RRD, so the layout matches the platform the
 * test runs on. */

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

    status = snprintf(path, size, "%s/tune-aberrant-%s-XXXXXX", builddir,
                      tag);
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

/* NaN != NaN, so compare values bit by bit */
static int same_bytes(
    const void *a,
    const void *b,
    size_t size)
{
    return memcmp(a, b, size) == 0;
}

int main(void)
{
    const char *create_args[] = {
        "DS:a:GAUGE:120:0:U",
        "DS:b:GAUGE:120:0:U",
        "RRA:AVERAGE:0.5:1:10",
        "RRA:HWPREDICT:30:0.1:0.0035:5"
    };
    const char *builddir;
    char      path3[4096];
    char      path2[4096];
    char      update_buf[40][64];
    const char *update_args[40];
    const char *tune_args[4];
    rrd_t     ref;
    rrd_t     tuned;
    rrd_file_t *ref_file = NULL;
    rrd_file_t *tuned_file = NULL;
    unsigned long rows = 0;
    unsigned long i;
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

    rrd_init(&ref);
    rrd_init(&tuned);

    rrd_clear_error();
    if (rrd_create_r(path3, 60, 1300000000, 4, create_args) != 0) {
        fail("could not create the test RRD");
        goto out;
    }
    for (i = 0; i < 40; i++) {
        snprintf(update_buf[i], sizeof(update_buf[i]), "%lu:%lu:%lu",
                 1300000060UL + 60 * i, (i * 37) % 97, (i * 53) % 89);
        update_args[i] = update_buf[i];
    }
    if (rrd_update_r(path3, NULL, 40, update_args) != 0) {
        fail("could not update the test RRD");
        goto out;
    }

    /* the version 0002 copy holds the same data as the version 0003 file */
    ref_file = rrd_open(path3, &ref, RRD_READONLY | RRD_READVALUES);
    if (ref_file == NULL) {
        fail("could not read the test RRD");
        goto out;
    }
    if (write_v2(path2, &ref) != 0) {
        fail("could not write the version 0002 RRD");
        goto out;
    }
    rrd_close(ref_file);
    ref_file = NULL;
    rrd_free(&ref);
    rrd_init(&ref);

    tune_args[0] = "tune";
    tune_args[1] = "--aberrant-reset";
    tune_args[2] = "a";
    tune_args[3] = path3;
    rrd_clear_error();
    if (rrd_tune_r(path3, 4, tune_args) != 0) {
        fail("tune --aberrant-reset failed on a version 0003 RRD");
        goto out;
    }
    tune_args[3] = path2;
    rrd_clear_error();
    if (rrd_tune_r(path2, 4, tune_args) != 0) {
        fail("tune --aberrant-reset failed on a version 0002 RRD");
        goto out;
    }

    rrd_clear_error();
    ref_file = rrd_open(path3, &ref, RRD_READONLY | RRD_READVALUES);
    if (ref_file == NULL) {
        fail("reset version 0003 RRD cannot be opened");
        goto out;
    }
    tuned_file = rrd_open(path2, &tuned, RRD_READONLY | RRD_READVALUES);
    if (tuned_file == NULL) {
        fail("reset version 0002 RRD cannot be opened");
        goto out;
    }
    if (strcmp(tuned.stat_head->version, "0002") != 0) {
        fprintf(stderr, "reset RRD has version %s, expected 0002\n",
                tuned.stat_head->version);
        goto out;
    }
    if (tuned.live_head->last_up != ref.live_head->last_up) {
        fprintf(stderr, "last update changed from %lld to %lld\n",
                (long long) ref.live_head->last_up,
                (long long) tuned.live_head->last_up);
        goto out;
    }
    for (i = 0; i < ref.stat_head->rra_cnt; i++)
        rows += ref.rra_def[i].row_cnt;
    if (!same_bytes(tuned.pdp_prep, ref.pdp_prep,
                    sizeof(pdp_prep_t) * ref.stat_head->ds_cnt)) {
        fail("pdp_prep differs from the version 0003 reset");
        goto out;
    }
    if (!same_bytes(tuned.cdp_prep, ref.cdp_prep,
                    sizeof(cdp_prep_t) * ref.stat_head->ds_cnt
                    * ref.stat_head->rra_cnt)) {
        fail("cdp_prep differs from the version 0003 reset");
        goto out;
    }
    if (!same_bytes(tuned.rra_ptr, ref.rra_ptr,
                    sizeof(rra_ptr_t) * ref.stat_head->rra_cnt)) {
        fail("rra_ptr differs from the version 0003 reset");
        goto out;
    }
    if (!same_bytes(tuned.rrd_value, ref.rrd_value,
                    sizeof(rrd_value_t) * rows * ref.stat_head->ds_cnt)) {
        fail("data rows differ from the version 0003 reset");
        goto out;
    }
    rc = 0;

  out:
    if (tuned_file != NULL)
        rrd_close(tuned_file);
    rrd_free(&tuned);
    if (ref_file != NULL)
        rrd_close(ref_file);
    rrd_free(&ref);
    remove(path3);
    remove(path2);
    return rc;
}
