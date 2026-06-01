#include <check.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

/*
 * We test that rrd_xport rejects inputs that would cause integer overflow
 * in malloc size calculations. Since rrd_xport is driven by argc/argv
 * (like rrd_graph), we call it with crafted DEF/XPORT arguments that
 * attempt to create an enormous col_cnt.
 */

#include "rrd.h"

START_TEST(test_xport_overflow_col_cnt)
{
    /* Invariant: rrd_xport must not succeed (or must not crash) when
     * given inputs that would cause col_cnt to overflow malloc size
     * calculations. A safe implementation returns an error code. */

    time_t start, end;
    unsigned long step, col_cnt;
    char **legend_v = NULL;
    rrd_value_t *data = NULL;
    int ret;

    /* Case 1: Valid minimal call with no DEFs - should fail gracefully */
    {
        char *argv1[] = { "xport", "--start", "0", "--end", "100" };
        int argc1 = 5;
        optind = 0;
        opterr = 0;
        ret = rrd_xport(argc1, argv1, &col_cnt, &start, &end, &step, &legend_v, &data);
        /* Should either fail or return 0 columns; must not crash */
        if (ret == 0) {
            ck_assert_uint_le(col_cnt, 1024*1024); /* sanity bound */
        }
        if (data) { free(data); data = NULL; }
        if (legend_v) { free(legend_v); legend_v = NULL; }
    }

    /* Case 2: Attempt with a non-existent RRD file - should fail gracefully */
    {
        char *argv2[] = { "xport", "--start", "0", "--end", "100",
                          "DEF:a=/nonexistent.rrd:ds:AVERAGE",
                          "XPORT:a:legend" };
        int argc2 = 6;
        optind = 0;
        opterr = 0;
        ret = rrd_xport(argc2, argv2, &col_cnt, &start, &end, &step, &legend_v, &data);
        /* Must not crash; expected to return error */
        ck_assert_int_ne(ret, 0);
        if (data) { free(data); data = NULL; }
        if (legend_v) { free(legend_v); legend_v = NULL; }
    }

    /* Case 3: Verify SIZE_MAX / sizeof(int) boundary is safe -
     * If col_cnt were SIZE_MAX/sizeof(int)+1, malloc would overflow.
     * We can't easily inject that via argv, but we verify the function
     * doesn't crash with many XPORT lines referencing bad DEFs. */
    {
        /* 256 XPORT lines all referencing non-existent DEF */
        char *argv3[520];
        int argc3 = 0;
        argv3[argc3++] = "xport";
        argv3[argc3++] = "--start";
        argv3[argc3++] = "0";
        argv3[argc3++] = "--end";
        argv3[argc3++] = "100";
        char defs[256][80];
        char xports[256][80];
        for (int i = 0; i < 256 && argc3 < 518; i++) {
            snprintf(defs[i], sizeof(defs[i]),
                     "DEF:v%d=/nonexistent.rrd:ds%d:AVERAGE", i, i);
            argv3[argc3++] = defs[i];
            snprintf(xports[i], sizeof(xports[i]), "XPORT:v%d:leg%d", i, i);
            argv3[argc3++] = xports[i];
        }
        optind = 0;
        opterr = 0;
        ret = rrd_xport(argc3, argv3, &col_cnt, &start, &end, &step, &legend_v, &data);
        /* Must not crash; will likely fail due to missing files */
        if (ret == 0) {
            ck_assert_uint_le(col_cnt, 256);
        }
        if (data) { free(data); data = NULL; }
        if (legend_v) { free(legend_v); legend_v = NULL; }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create