/*****************************************************************************
 * RRDtool 1.0.33  Copyright Tobias Oetiker, 1999
 *****************************************************************************
 * rrd_format.c  RRD Database Format helper functions
 *****************************************************************************
 * $Id$
 * $Log$
 * Revision 1.1  2001/02/25 22:25:05  oetiker
 * Initial revision
 *
 * Revision 1.3  1998/03/08 12:35:11  oetiker
 * checkpointing things because the current setup seems to work
 * according to the things said in the manpages
 *
 * Revision 1.2  1998/02/26 22:58:22  oetiker
 * fixed define
 *
 * Revision 1.1  1998/02/21 16:14:41  oetiker
 * Initial revision
 *
 *
 *****************************************************************************/
#include "rrd_tool.h"

#define converter(VV,VVV) \
   if (strcmp(#VV, string) == 0) return VVV;

/* conversion functions to allow symbolic entry of enumerations */
enum dst_en dst_conv(char *string)
{
    converter(COUNTER,DST_COUNTER)
    converter(ABSOLUTE,DST_ABSOLUTE)
    converter(GAUGE,DST_GAUGE)
    converter(DERIVE,DST_DERIVE)
    rrd_set_error("unknown date aquisition function '%s'",string);
    return(-1);
}


enum cf_en cf_conv(char *string)
{

    converter(AVERAGE,CF_AVERAGE)
    converter(MIN,CF_MINIMUM)
    converter(MAX,CF_MAXIMUM)
    converter(LAST,CF_LAST)
    rrd_set_error("unknown consolidation function '%s'",string);
    return(-1);
}

#undef converter	

long
ds_match(rrd_t *rrd,char *ds_nam){
    long i;
    for(i=0;i<rrd->stat_head->ds_cnt;i++)
	if ((strcmp(ds_nam,rrd->ds_def[i].ds_nam))==0)
	    return i;
    rrd_set_error("unknown data source name '%s'",ds_nam);
    return -1;
}







