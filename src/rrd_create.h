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

#endif


#ifdef  __cplusplus
}
#endif
