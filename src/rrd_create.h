/****************************************************************************
 * RRDtool 1.4.8  Copyright by Tobi Oetiker, 1997-2013
 ****************************************************************************
 * rrd_create.h
 ****************************************************************************/
#ifdef  __cplusplus
extern    "C" {
#endif

#ifndef _RRD_CREATE_H
#define _RRD_CREATE_H

#include "rrd.h"

int parseDS(const char *def, 
	    ds_def_t *ds_def,
	    void *key_hash,
	    long (*lookup)(void *, char *));

int parseRRA(const char *def,
	     rra_def_t *rra_def, 
	     unsigned long hash);

rra_def_t *handle_dependent_rras(rra_def_t *rra_def_array, 
				 long unsigned int *rra_cnt, 
				 unsigned long hash);

void init_cdp(const rrd_t *rrd, 
	      const rra_def_t *rra_def, 
	      cdp_prep_t *cdp_prep);

#endif


#ifdef  __cplusplus
}
#endif
