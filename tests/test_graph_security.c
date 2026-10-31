#include "rrd_graph.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int expect_overflow(
    const char *context)
{
    if (rrd_test_error() && strstr(rrd_get_error(), "impossibly large"))
        return 0;
    fprintf(stderr, "%s overflow was not diagnosed: %s\n", context,
            rrd_test_error() ? rrd_get_error() : "no librrd error");
    return 1;
}

static int test_cdef_overflow(time_t start, time_t end)
{
    image_desc_t im;
    graph_desc_t gdes[2];
    rpnp_t    expression[2];
    rrd_value_t value = 1.0;

    memset(&im, 0, sizeof(im));
    memset(gdes, 0, sizeof(gdes));
    memset(expression, 0, sizeof(expression));

    im.gdes = gdes;
    im.gdes_c = 2;
    gdes[0].ds_cnt = 1;
    gdes[0].step = 1;
    gdes[0].start = start;
    gdes[0].end = end;
    gdes[0].data = &value;
    gdes[1].gf = GF_CDEF;
    strcpy(gdes[1].vname, "overflow");
    gdes[1].rpnp = expression;
    expression[0].op = OP_VARIABLE;
    expression[0].ptr = 0;
    expression[1].op = OP_END;

    rrd_clear_error();
    if (data_calc(&im) == 0)
        return 1;
    return expect_overflow("CDEF");
}

static int test_vdef_overflow(void)
{
    image_desc_t im;
    graph_desc_t gdes[2];
    rrd_value_t value = 1.0;

    memset(&im, 0, sizeof(im));
    memset(gdes, 0, sizeof(gdes));

    im.gdes = gdes;
    im.gdes_c = 2;
    gdes[0].ds_cnt = 1;
    gdes[0].step = 1;
    gdes[0].start = 0;
    gdes[0].end = (time_t) ((size_t) -1 / sizeof(rrd_value_t) + 1);
    gdes[0].data = &value;
    gdes[1].vidx = 0;
    gdes[1].gf = GF_VDEF;
    gdes[1].vf.op = VDEF_PERCENT;
    strcpy(gdes[1].vname, "overflow");

    rrd_clear_error();
    if (data_calc(&im) == 0)
        return 1;
    return expect_overflow("VDEF");
}

static int test_percentiles(time_t start)
{
    image_desc_t im;
    graph_desc_t gdes[2];
    rrd_value_t values[] = { NAN, 1.0, 3.0, 2.0 };
    memset(&im, 0, sizeof(im));
    memset(gdes, 0, sizeof(gdes));
    im.gdes = gdes;
    im.gdes_c = 2;
    gdes[0].ds_cnt = 1;
    gdes[0].step = 1;
    gdes[0].start = start;
    gdes[0].end = start + 4;
    gdes[0].data = values;
    gdes[1].vidx = 0;
    for (int variant = 0; variant < 2; ++variant) {
        gdes[1].vf.op = variant ? VDEF_PERCENTNAN : VDEF_PERCENT;
        for (int percentile = 0; percentile <= 100; percentile += 50) {
            double expected = percentile == 100 ? 3 : percentile == 50 ? 2 : 1;
            gdes[1].vf.param = percentile;
            rrd_clear_error();
            if (vdef_calc(&im, 1) != 0)
                return 1;
            if (!variant && percentile == 0) {
                if (!isnan(gdes[1].vf.val))
                    return 1;
            } else if (gdes[1].vf.val != expected) {
                return 1;
            }
        }
    }
    return 0;
}

static int test_invalid_ranges(void)
{
    image_desc_t im = {0};
    graph_desc_t gdes[2] = {{0}};
    rrd_value_t value = 1;
    rpnp_t expression[2] = {{0}};
    im.gdes = gdes;
    im.gdes_c = 2;
    gdes[0].ds_cnt = 1;
    gdes[0].data = &value;
    gdes[1].vidx = 0;
    gdes[1].vf.op = VDEF_PERCENT;
    gdes[1].gf = GF_VDEF;
    for (int mode = 0; mode < 2; mode++) {
        gdes[0].step = mode ? 1 : 0;
        gdes[0].start = mode ? 2 : 0;
        gdes[0].end = 1;
        rrd_clear_error();
        if (data_calc(&im) == 0 ||
            !strstr(rrd_get_error(), "invalid data range"))
            return 1;
    }
    gdes[1].gf = GF_CDEF;
    gdes[1].rpnp = expression;
    expression[0].op = OP_VARIABLE;
    expression[1].op = OP_END;
    rrd_clear_error();
    if (data_calc(&im) == 0 ||
        !strstr(rrd_get_error(), "invalid data range"))
        return 1;
    return 0;
}

static int test_empty_percentiles(void)
{
    image_desc_t im = {0};
    graph_desc_t gdes[2] = {{0}};
    rrd_value_t value = NAN;
    im.gdes = gdes;
    im.gdes_c = 2;
    gdes[0].step = 1;
    gdes[0].ds_cnt = 1;
    gdes[0].data = &value;
    for (int mode = 0; mode < 2; mode++) {
        gdes[0].end = mode;
        gdes[1].vf.op = mode ? VDEF_PERCENTNAN : VDEF_PERCENT;
        rrd_clear_error();
        if (vdef_calc(&im, 1) != 0 || !isnan(gdes[1].vf.val))
            return 1;
    }
    return 0;
}

static int test_repeated_fetch(unsigned long step)
{
    const char *create_args[] = {
        "DS:a:GAUGE:120:U:U", "DS:b:GAUGE:120:U:U",
        "RRA:AVERAGE:0.5:1:20"
    };
    const char *update_args[] = { "1000000060:1:2", "1000000120:3:4" };
    const char *builddir = getenv("BUILDDIR");
    char path[1024];
    image_desc_t im;
    rrd_value_t *data[2];
    char **names[2];
    int owners[2];
    int fd, length, result = 1;
    unsigned int i;

    length = snprintf(path, sizeof(path), "%s/graph-fetch-XXXXXX",
                      builddir ? builddir : ".");
    if (length < 0 || (size_t) length >= sizeof(path))
        return 1;
    fd = mkstemp(path);
    if (fd < 0)
        return 1;
    close(fd);
    rrd_clear_error();
    if (rrd_create_r(path, 60, 1000000000, 3, create_args) != 0 ||
        rrd_update_r(path, NULL, 2, update_args) != 0) {
        fprintf(stderr, "could not create fetch fixture: %s\n", rrd_get_error());
        remove(path);
        return 1;
    }

    rrd_graph_init(&im, IMAGE_INIT_NO_CAIRO);
    im.step = step;
    im.gdes = calloc(2, sizeof(*im.gdes));
    if (im.gdes == NULL)
        goto done;
    im.gdes_c = 2;
    for (i = 0; i < 2; i++) {
        graph_desc_t *def = &im.gdes[i];

        def->gf = GF_DEF;
        snprintf(def->rrd, sizeof(def->rrd), "%s", path);
        snprintf(def->ds_nam, sizeof(def->ds_nam), "%c", 'a' + i);
        def->ds = -1;
        def->cf = CF_AVERAGE;
        def->start = def->start_orig = 1000000000;
        def->end = def->end_orig = 1000000120;
        def->step = def->step_orig = step;
    }
    if (data_fetch(&im) != 0)
        goto done;
    for (i = 0; i < 2; i++) {
        data[i] = im.gdes[i].data;
        names[i] = im.gdes[i].ds_namv;
        owners[i] = im.gdes[i].data_first;
        if (im.gdes[i].ds != (int) i || im.gdes[i].ds_cnt != 2)
            goto done;
    }
    /* Exercise both independently fetched and shared buffers, followed
     * by the second pass used by graphv --add-jsontime. */
    if (data_fetch(&im) != 0)
        goto done;
    for (i = 0; i < 2; i++) {
        if (im.gdes[i].data != data[i] || im.gdes[i].ds_namv != names[i] ||
            im.gdes[i].data_first != owners[i]) {
            fprintf(stderr, "repeated fetch changed DEF %u ownership\n", i);
            goto done;
        }
    }
    result = 0;
done:
    im_free(&im);
    remove(path);
    return result;
}

static int test_def_retry_error(void)
{
    const char *args[] = {
        "DEF:debug=bad%literal:vname=x:rrd=missing.rrd:ds=a:cf=INVALID"
    };
    image_desc_t im;
    int failed;

    rrd_graph_init(&im, IMAGE_INIT_NO_CAIRO);
    rrd_clear_error();
    rrd_graph_script(1, args, &im, 0);
    failed = !rrd_test_error() ||
        strcmp(rrd_get_error(), "Bad debug value: bad%literal") != 0;
    im_free(&im);
    return failed;
}

static int test_title_cleanup(void)
{
    const char *titles[] = {"", "\nleading\n\nsecond", "one<br>two\\nthree\nfour"};
    const int counts[] = {1, 2, 3};
    const char *first[] = {"", "leading", "one"};
    size_t i;

    for (i = 0; i < sizeof(titles) / sizeof(titles[0]); i++) {
        image_title_t title = graph_title_split(titles[i]);

        if (title.lines == NULL || title.buf == NULL ||
            title.count != counts[i] || strcmp(title.lines[0], first[i]) != 0)
            return 1;
        graph_title_free(&title);
        if (title.lines != NULL || title.buf != NULL)
            return 1;
        graph_title_free(&title);
    }
    return 0;
}

int main(void)
{
    int result = test_percentiles(0) | test_percentiles(-4) | test_percentiles(-2) |
        test_invalid_ranges() | test_empty_percentiles() |
        test_repeated_fetch(1) | test_repeated_fetch(60) | test_def_retry_error() |
        test_title_cleanup();
    if (sizeof(time_t) > sizeof(int))
        result |= test_cdef_overflow(0, (time_t) ((uintmax_t) INT_MAX + 1));
    /* Allocation-bound cases require a representable end timestamp. */
    if (sizeof(time_t) >= sizeof(size_t)) {
        result |= test_cdef_overflow(0, (time_t) ((size_t) -1 / sizeof(rrd_value_t) + 1));
        result |= test_vdef_overflow();
    }
    else if (result == 0)
        return 77;
    return result;
}
