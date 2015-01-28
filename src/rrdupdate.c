/*****************************************************************************
 * RRDtool 1.GIT, Copyright by Tobi Oetiker
 *****************************************************************************
 * rrdupdate.c  Main program for the (standalone) rrdupdate utility
 *****************************************************************************
 * $Id$
 *****************************************************************************/

#include "rrd_config.h"

#include "rrd.h"
/* for basename */
#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#else
#include "plbasename.h"
#endif

int main(
    int argc,
    char **argv)
{
    char *name=basename(argv[0]);
    rrd_info_t *info;

    if (!strcmp(name, "rrdcreate")) {
        rrd_create(argc, argv);
    }
    else if (!strcmp(name, "rrdinfo")) {
         info=rrd_info(argc, argv);
         rrd_info_print(info);
         rrd_info_free(info);
    }
    else {
        rrd_update(argc, argv);
    }

    if (rrd_test_error()) {
         printf("RRDtool " PACKAGE_VERSION
               "  Copyright by Tobi Oetiker\n\n");
        if (!strcmp(name, "rrdcreate")) {
            printf("Usage: rrdcreate <filename>\n"
                   "\t\t\t[--start|-b start time]\n"
                   "\t\t\t[--step|-s step]\n"
                   "\t\t\t[--no-overwrite]\n"
                   "\t\t\t[DS:ds-name:DST:dst arguments]\n"
                   "\t\t\t[RRA:CF:cf arguments]\n\n");
       }
       else if (!strcmp(name, "rrdinfo")) {
           printf("Usage: rrdinfo <filename>\n");
       }
       else {
            printf("Usage: rrdupdate <filename>\n"
                   "\t\t\t[--template|-t ds-name[:ds-name]...]\n"
                   "\t\t\t[--skip-past-updates]\n"
                   "\t\t\ttime|N:value[:value...]\n\n"
                   "\t\t\tat-time@value[:value...]\n\n"
                   "\t\t\t[ time:value[:value...] ..]\n\n");
       }

       printf("ERROR: %s\n", rrd_get_error());
       rrd_clear_error();
       return 1;
   }
    return 0;
}
