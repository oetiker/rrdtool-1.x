/*
 * Helper for tests/cached-lastupdate1: calls rrd_lastupdate_v() with its
 * own arguments and prints the result on one line as
 *   <last_update> <ds>=<value> ...
 * On failure it checks that the out-parameters were reset, prints the
 * librrd error to stderr and exits with status 1.
 */
#ifdef HAVE_CONFIG_H
#include "rrd_config.h"
#endif

#include "rrd.h"

#include <stdio.h>
#include <stdlib.h>

int main(
    int argc,
    char **argv)
{
    time_t    last_update = 1;
    unsigned long ds_count = 1, i;
    char    **ds_names = (char **) argv;
    char    **last_ds = (char **) argv;

    const char **av = (const char **) argv;

    av[0] = "lastupdate";
    if (rrd_lastupdate_v(argc, av, &last_update,
                         &ds_count, &ds_names, &last_ds) != 0) {
        fprintf(stderr, "ERROR: %s\n", rrd_get_error());
        if (ds_count != 0 || ds_names != NULL || last_ds != NULL) {
            fprintf(stderr, "out-parameters not reset on failure\n");
            return 2;
        }
        return 1;
    }
    printf("%lld", (long long) last_update);
    for (i = 0; i < ds_count; i++) {
        printf(" %s=%s", ds_names[i], last_ds[i]);
        free(ds_names[i]);
        free(last_ds[i]);
    }
    printf("\n");
    free(ds_names);
    free(last_ds);
    return 0;
}
