/*****************************************************************************
 * RRDtool 1.GIT, Copyright by Tobi Oetiker
 *****************************************************************************
 * rrd_format.c  RRD Database Format helper functions
 *****************************************************************************/
#include "rrd_tool.h"
#ifdef WIN32
#include "stdlib.h"
#endif

#define converter(VV,VVV) \
   if (strcmp(#VV, string) == 0) return VVV;

/* conversion functions to allow symbolic entry of enumerations */
enum dst_en dst_conv(
    const char *string)
{
    converter(COUNTER, DST_COUNTER)
        converter(ABSOLUTE, DST_ABSOLUTE)
        converter(GAUGE, DST_GAUGE)
        converter(DERIVE, DST_DERIVE)
        converter(COMPUTE, DST_CDEF)
        converter(DCOUNTER, DST_DCOUNTER)
        converter(DDERIVE, DST_DDERIVE)
        rrd_set_error("unknown data acquisition function '%s'", string);
    return (enum dst_en)(-1);
}


enum cf_en cf_conv(
    const char *string)
{

    converter(AVERAGE, CF_AVERAGE)
        converter(MIN, CF_MINIMUM)
        converter(MAX, CF_MAXIMUM)
        converter(LAST, CF_LAST)
        converter(HWPREDICT, CF_HWPREDICT)
        converter(MHWPREDICT, CF_MHWPREDICT)
        converter(DEVPREDICT, CF_DEVPREDICT)
        converter(SEASONAL, CF_SEASONAL)
        converter(DEVSEASONAL, CF_DEVSEASONAL)
        converter(FAILURES, CF_FAILURES)
        rrd_set_error("unknown consolidation function '%s'", string);
    return (enum cf_en)(-1);
}

const char *cf_to_string (enum cf_en cf)
{
    switch (cf)
    {
        case CF_AVERAGE:     return "AVERAGE";
        case CF_MINIMUM:     return "MIN";
        case CF_MAXIMUM:     return "MAX";
        case CF_LAST:        return "LAST";
        case CF_HWPREDICT:   return "HWPREDICT";
        case CF_SEASONAL:    return "SEASONAL";
        case CF_DEVPREDICT:  return "DEVPREDICT";
        case CF_DEVSEASONAL: return "DEVSEASONAL";
        case CF_FAILURES:    return "FAILURES";
        case CF_MHWPREDICT:  return "MHWPREDICT";

        default:
            return NULL;
    }
} /* char *cf_to_string */

#undef converter

long ds_match(
    rrd_t *rrd,
    char *ds_nam)
{
    unsigned long i;

    for (i = 0; i < rrd->stat_head->ds_cnt; i++)
        if ((strcmp(ds_nam, rrd->ds_def[i].ds_nam)) == 0)
            return i;
    rrd_set_error("unknown data source name '%s'", ds_nam);
    return -1;
}

off_t rrd_get_header_size(
    rrd_t *rrd)
{
    return sizeof(stat_head_t) + \
        sizeof(ds_def_t) * rrd->stat_head->ds_cnt + \
        sizeof(rra_def_t) * rrd->stat_head->rra_cnt + \
        ( atoi(rrd->stat_head->version) < 3 ? sizeof(time_t) : sizeof(live_head_t) ) + \
        sizeof(pdp_prep_t) * rrd->stat_head->ds_cnt + \
        sizeof(cdp_prep_t) * rrd->stat_head->ds_cnt * rrd->stat_head->rra_cnt + \
        sizeof(rra_ptr_t) * rrd->stat_head->rra_cnt;
}
