#ifndef RRD_XPORT_H_C5A7EBAF331140D38C241326B81F357D
#define RRD_XPORT_H_C5A7EBAF331140D38C241326B81F357D

/****************************************************************************
 * RRDtool 1.4.3  Copyright by Tobi Oetiker, 1997-2010
 ****************************************************************************
 * rrd_xport.h  contains XML related constants
 ****************************************************************************/
#ifdef  __cplusplus
extern    "C" {
#endif

#define XML_ENCODING     "ISO-8859-1"
#define ROOT_TAG         "xport"
#define META_TAG         "meta"
#define META_START_TAG   "start"
#define META_STEP_TAG    "step"
#define META_END_TAG     "end"
#define META_ROWS_TAG    "rows"
#define META_COLS_TAG    "columns"
#define LEGEND_TAG       "legend"
#define LEGEND_ENTRY_TAG "entry"
#define DATA_TAG         "data"
#define DATA_ROW_TAG     "row"
#define COL_TIME_TAG     "t"
#define COL_DATA_TAG     "v"

#ifdef  __cplusplus
}
#endif

#endif
