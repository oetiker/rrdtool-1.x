/*
 * rrd_client.h is an installed header and must compile on its own: a
 * program that includes nothing else before it has to build. This file
 * includes only that header; if it fails to compile, `make check` fails.
 * Running the program checks nothing more.
 */

#include <rrd_client.h>

int main(
    void)
{
    rrd_client_t *client = 0;
    rrd_info_t *info = 0;
    rrd_value_t value = 0;
    rrd_output_callback_t cb = 0;
    time_t    last = 0;

    (void) client;
    (void) info;
    (void) value;
    (void) cb;
    (void) last;
    return sizeof(rrdc_stats_t) > 0 ? 0 : 1;
}
