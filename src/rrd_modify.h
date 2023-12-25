/****************************************************************************
 * RRDtool 1.8.0 Copyright by Tobi Oetiker, 1997-2022
 ****************************************************************************
 * rrd_create.h
 ****************************************************************************/
#ifdef  __cplusplus
extern    "C" {
#endif

#ifndef _RRD_MODIFY_H
#define _RRD_MODIFY_H

#include "rrd.h"

typedef struct {
    /* the index of the RRA to be changed or -1 if there is no current
 *        RRA */
    int index;
    /* what operation */
    char op;  // '+', '-', '=', 'a'
    /* the number originally specified with the operation (eg. rows to
 *        be added) */
    unsigned int row_count;
    /* the resulting final row count for the RRA */
    unsigned int final_row_count;
    /* An RRA definition in case of an addition */
    char *def;
} rra_mod_op_t;

int handle_modify(const rrd_t *in, const char *outfilename,
		  int argc, const char **argv, int optind,
		  int newstep);

typedef union {
	long l;
	unsigned long ul;
	void *vp;
} candidate_extra_t;

typedef struct {
    rrd_t *rrd;
    int rra_index;
    rrd_value_t *values;
    rra_def_t *rra;
    enum cf_en rra_cf;
    rra_ptr_t *ptr;
    cdp_prep_t *cdp;
    candidate_extra_t extra;
} candidate_t;

/* 
   Try to find a set of RRAs from rrd that might be used to populate
   added rows in RRA rra. Generally, candidates are RRAs that have a
   pdp step of 1 (regardless of CF type) and those that have the same
   CF (or a CF of AVERAGE) and any pdp step count.

   The function returns a pointer to a newly allocated array of
   candidate_t structs. The number of elements is returned in *cnt.

   The returned memory must be free()'d by the calling code. NULL is
   returned in case of error or if there are no candidates. In case of
   an error, the RRD error gets set.

   Arguments:
   rrd .. the RRD to pick RRAs from
   rra .. the RRA we want to populate
   cnt .. a pointer to an int receiving the number of returned candidates
*/
typedef int candidate_selectfunc_t(const rra_def_t *tofill, const rra_def_t *maybe);

candidate_t *find_candidate_rras(const rrd_t *rrd, const rra_def_t *rra, int *cnt,
				 candidate_extra_t extra,
                                 candidate_selectfunc_t *select_func);

#endif


#ifdef  __cplusplus
}
#endif
