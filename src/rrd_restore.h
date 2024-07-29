/****************************************************************************
 * RRDtool 1.9.0 Copyright by Tobi Oetiker, 1997-2024
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
