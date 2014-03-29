/****************************************************************************
 * RRDtool 1.4.8  Copyright by Tobi Oetiker, 1997-2013
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
		  int argc, char **argv, int optind,
		  int newstep);

#endif


#ifdef  __cplusplus
}
#endif
