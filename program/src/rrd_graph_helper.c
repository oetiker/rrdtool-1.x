/**********************************************************
 *** Several generic helper functions for rrd_graph.c   ***
 **********************************************************/

#include "rrd_graph.h"
#include "rrd_graph_helper.h"

/**********************************************************
 *** Helper functions for parsing command file          ***
 **********************************************************/

/* Parse a VNAME followed by an equal sign ( CDEF:VNAME= )
 *
 * Input:	pointer to argv
 * Input:	pointer to im structure
 * Input:	pointer to error string
 * Output:	number of chars eaten (0 means error)
 *
 * Usage:	n=parse_vname( &argv[i][argstart],&im, "VDEF");
 *		if (n==0) { error } else { argstart+=n };
 */
int
parse_vname1(cptr,im,err)
char *		cptr;
image_desc_t *	im;
char *		err;
{
    int		n=0,vidx;

    sscanf(cptr, DEF_NAM_FMT "=%n",im->gdes[im->gdes_c-1].vname,&n);

    /* Check if the sequence matches, including the
     * terminating equal sign */
    if (n==0) {
	im_free(im);
	rrd_set_error("Can't parse VNAME in %s: '%s'",err,cptr);
	return 0;
    }

    /* Check if this is an unused variable */
    vidx=find_var(im,im->gdes[im->gdes_c-1].vname);
    if (vidx!=-1) {
	switch(im->gdes[vidx].gf) {
	    case GF_DEF:
		rrd_set_error("Duplicate variable in %s: '%s' defined as DEF",
			err,im->gdes[im->gdes_c-1].vname);
		break;
	    case GF_CDEF:
		rrd_set_error("Duplicate variable in %s: '%s' defined as CDEF",
			err,im->gdes[im->gdes_c-1].vname);
		break;
	    case GF_VDEF:
		rrd_set_error("Duplicate variable in %s: '%s' defined as VDEF",
			err,im->gdes[im->gdes_c-1].vname);
		break;
	    default:
		rrd_set_error("Duplicate variable in %s: '%s' defined",
			err,im->gdes[im->gdes_c-1].vname);
		break;
	};
	im_free(im);
	return 0;
    }

    /* VNAME must start with a character other than numeric */
    if (isdigit(im->gdes[im->gdes_c-1].vname[0])) {
	rrd_set_error("Variable in %s starts with a digit: '%s'",
		err,im->gdes[im->gdes_c-1].vname);
	im_free(im);
	return 0;
    };

    /* Reserved words checking.  Not at the moment. */

    return n;
}

/**********************************************************
 ***                                                    ***
 **********************************************************/

