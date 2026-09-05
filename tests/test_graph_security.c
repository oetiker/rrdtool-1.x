#include "rrd_graph.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect_overflow(
    const char *context)
{
    if (rrd_test_error() && strstr(rrd_get_error(), "impossibly large"))
        return 0;
    fprintf(stderr, "%s overflow was not diagnosed\n", context);
    return 1;
}

static int test_cdef_overflow(void)
{
    image_desc_t im;
    graph_desc_t gdes[2];
    rpnp_t    expression[2];
    double    value = 1.0;

    memset(&im, 0, sizeof(im));
    memset(gdes, 0, sizeof(gdes));
    memset(expression, 0, sizeof(expression));

    im.gdes = gdes;
    im.gdes_c = 2;
    gdes[0].ds_cnt = 1;
    gdes[0].step = 1;
    gdes[0].start = 0;
    gdes[0].end = LONG_MAX;
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
    double    value = 1.0;

    memset(&im, 0, sizeof(im));
    memset(gdes, 0, sizeof(gdes));

    im.gdes = gdes;
    im.gdes_c = 2;
    gdes[0].ds_cnt = 1;
    gdes[0].step = 1;
    gdes[0].start = 0;
    gdes[0].end = LONG_MAX;
    gdes[0].data = &value;
    gdes[1].vidx = 0;
    gdes[1].vf.op = VDEF_PERCENT;
    strcpy(gdes[1].vname, "overflow");

    rrd_clear_error();
    if (vdef_calc(&im, 1) == 0)
        return 1;
    return expect_overflow("VDEF");
}

int main(void)
{
    return test_cdef_overflow() || test_vdef_overflow();
}
