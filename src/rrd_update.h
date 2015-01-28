/****************************************************************************
 * RRDtool 1.GIT, Copyright by Tobi Oetiker
 ****************************************************************************
 * rrd_restore.h
 ****************************************************************************/
#ifdef  __cplusplus
extern    "C" {
#endif

#ifndef _RRD_UPDATE_H
#define _RRD_UPDATE_H

#include <stdio.h>
#include "rrd.h"

void update_cdp(
    unival *scratch,
    int current_cf,
    rrd_value_t pdp_temp_val,
    unsigned long rra_step_cnt,
    unsigned long elapsed_pdp_st,
    unsigned long start_pdp_offset,
    unsigned long pdp_cnt,
    rrd_value_t xff,
    int i,
    int ii);

#endif


#ifdef  __cplusplus
}
#endif
