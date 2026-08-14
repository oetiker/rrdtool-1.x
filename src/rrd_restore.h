/****************************************************************************
 * RRDtool 1.11.0 Copyright by Tobi Oetiker, 1997-2026
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

#endif


#ifdef  __cplusplus
}
#endif
