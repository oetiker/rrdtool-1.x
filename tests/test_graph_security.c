#include "rrd_graph.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(void)
{
    int result = test_percentiles(0) | test_percentiles(-4) | test_percentiles(-2) |
        test_invalid_ranges() | test_empty_percentiles();
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
