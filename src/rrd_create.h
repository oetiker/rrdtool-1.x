/****************************************************************************
 * RRDtool 1.7.2 Copyright by Tobi Oetiker
 ****************************************************************************
 * rrd_create.h
 ****************************************************************************/
#ifdef  __cplusplus
extern    "C" {
#endif

#ifndef _RRD_CREATE_H
#define _RRD_CREATE_H

#include "rrd.h"

typedef struct {
    char *ds_nam;
    char *def;
    char *mapped_name;
    int index;
} mapping_t;

int parseDS(const char *def, 
	    ds_def_t *ds_def,
	    void *key_hash,
	    long (*lookup)(void *, char *),
	    mapping_t *mapping,
            const char **require_version);

/* Parse a textual RRA definition into rra_def. The rra_def might be
   disconnected from any RRD. However, because some definitions cause
   modifications to other parts of an RRD (like the version) it is
   possible to pass the RRD that is about to hold the RRA. If the
   definition really is to stay disconnected, it is permitted to pass
   NULL to the rrd parameter.
   The hash is a parameter to introduce some randomness to avoid 
   potential performance problems when doing bulk updates to many RRDs
   at once.
*/
int parseRRA(const char *def,
	     rra_def_t *rra_def, 
	     rrd_t *rrd,
	     unsigned long hash,
             const char **require_version);

rra_def_t *handle_dependent_rras(rra_def_t *rra_def_array, 
				 long unsigned int *rra_cnt, 
				 unsigned long hash);

/** Initialize a cdp_prep structure. The rra_def, pdp_prep and cdp_prep should
 correspond to each other. Global values are taken from rrd. */
void init_cdp(const rrd_t *rrd, 
	      const rra_def_t *rra_def,
	      const pdp_prep_t *pdp_prep,
	      cdp_prep_t *cdp_prep);

int write_rrd(const char *outfilename, rrd_t *out);
int write_fh(FILE *fh, rrd_t *rrd);

/* would these make more sense elsewhere? */

/* find last second a row corresponds with */
time_t end_time_for_row_simple(const rrd_t *rrd, 
				int rra_index,
				int row);
time_t end_time_for_row(const rrd_t *rrd, 
			const rra_def_t *rra,
			int cur_row,
			int row);

/* find the row covering a given time */
int row_for_time(const rrd_t *rrd, 
		 const rra_def_t *rra, 
		 int cur_row, time_t req_time);


#endif


#ifdef  __cplusplus
}
#endif
