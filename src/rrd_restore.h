/****************************************************************************
 * RRDtool 1.4.8  Copyright by Tobi Oetiker, 1997-2013
 ****************************************************************************
 * rrd_restore.h
 ****************************************************************************/
#ifdef  __cplusplus
extern    "C" {
#endif

#ifndef _RRD_RESTORE_H
#define _RRD_RESTORE_H

#include <stdio.h>
#include "rrd.h"

int write_file(
    const char *file_name,
    rrd_t *rrd);

int write_fh(
    FILE *fh,
    rrd_t *rrd);

#endif


#ifdef  __cplusplus
}
#endif
