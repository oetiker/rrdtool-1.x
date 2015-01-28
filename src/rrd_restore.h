/****************************************************************************
 * RRDtool 1.GIT, Copyright by Tobi Oetiker
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
