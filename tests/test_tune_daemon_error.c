/* rrd_tune() must return -1 when it cannot reach rrdcached.
 *
 * The bindings test for -1 (Python) or a negative value (.NET); a
 * positive return made a failed tune look like success there. */

#ifdef HAVE_CONFIG_H
#include "rrd_config.h"
#endif

#include "rrd.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *args[] = {
        "tune",
        "--daemon",
        "unix:/nonexistent-rrdtool-test-dir/rrdcached.sock",
        "--heartbeat",
        "a:120",
        "nonexistent.rrd"
    };
    int       status;

    rrd_clear_error();
    status = rrd_tune(6, args);
    if (status != -1) {
        fprintf(stderr, "rrd_tune returned %d, expected -1\n", status);
        return 1;
    }
    if (!rrd_test_error()
        || strstr(rrd_get_error(), "rrdcached") == NULL) {
        fprintf(stderr, "unexpected error message: '%s'\n",
                rrd_test_error() ? rrd_get_error() : "");
        return 1;
    }
    return 0;
}
