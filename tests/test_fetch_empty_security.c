#include "rrd_tool.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int run_case(time_t end, unsigned long step, const char *error)
{
    time_t start = 900000000;
    unsigned long ds_cnt = 0;
    char **names = NULL;
    rrd_value_t *values = NULL;
    int status;
    rrd_clear_error();
    status = rrd_fetch_empty(&start, &end, &step, &ds_cnt, "value", &names, &values);
    if (error != NULL) {
        if (status == 0 || !strstr(rrd_get_error(), error)) {
            fprintf(stderr, "expected %s, got %s\n", error, rrd_get_error());
            return 1;
        }
        return 0;
    }
    if (status != 0 || ds_cnt != 1)
        return 1;
    for (size_t i = 0; i < (size_t)((end - start) / step + 1); ++i) {
        if (!isnan(values[i]))
            return 1;
    }
    free(names[0]);
    free(names);
    free(values);
    return 0;
}

int main(void)
{
    if (run_case(900000010, 0, "fetch step is zero") ||
        run_case(899999999, 1, "fetch end precedes start") ||
        run_case(900000010, 1, NULL))
        return 1;
    /* On narrower time_t ABIs this allocation boundary is unreachable. */
    if (sizeof(time_t) >= sizeof(size_t) && sizeof(time_t) >= 8) {
        time_t end = (time_t)(SIZE_MAX / sizeof(rrd_value_t) + 900000001);
        if (run_case(end, 1, "fetch data size overflow"))
            return 1;
    }
    return 0;
}
