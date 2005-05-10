/*
 *  This file is part of RRDtool.
 *
 *  RRDtool is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  RRDtool is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Foobar; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*****************************************************************************
 * RRDtool 1.0.37  Copyright Tobias Oetiker, 1997, 1998, 1999
 *****************************************************************************
 * rrd_format.h  RRD Database Format header
 *****************************************************************************/

#ifndef _RRD_FORMAT_H
#define _RRD_FORMAT_H

#include "rrd.h"

/*****************************************************************************
 * put this in your /usr/lib/magic file (/etc/magic on HPUX)
 *
 *  # rrd database format
 *  0       string          RRD\0           rrd file
 *  >5      string          >\0             version '%s'
 *
 *****************************************************************************/

#define RRD_COOKIE    "RRD"
#define RRD_VERSION   "0001"
#define FLOAT_COOKIE  8.642135E130

#if defined(WIN32)
#define DNAN          ((double)fmod(0.0,0.0))    
#define DINF	      ((double)log(0.0))
#else

#define DNAN          ((double)(0.0/0.0))     /* we use a DNAN to
					       * represent the UNKNOWN
					       * */
#define DINF          ((double)(1.0/0.0))     /* we use a DINF to
					       * represent a value at the upper or
					       * lower border of the graph ...
					       * */
#endif

typedef union unival { 
    unsigned long u_cnt; 
    rrd_value_t   u_val;
} unival;


/****************************************************************************
 * The RRD Database Structure
 * ---------------------------
 * 
 * In oder to properly describe the database structure lets define a few
 * new words:
 *
 * ds - Data Source (ds) providing input to the database. A Data Source (ds)
 *       can be a traffic counter, a temperature, the number of users logged
 *       into a system. The rrd database format can handle the input of
 *       several Data Sources (ds) in a singe database.
 *  
 * dst - Data Source Type (dst). The Data Source Type (dst) defines the rules
 *       applied to Build Primary Data Points from the input provided by the
 *       data sources (ds).
 *
 * pdp - Primary Data Point (pdp). After the database has accepted the
 *       input from the data sources (ds). It starts building Primary
 *       Data Points (pdp) from the data. Primary Data Points (pdp)
 *       are evenly spaced along the time axis (pdp_step). The values
 *       of the Primary Data Points are calculated from the values of
 *       the data source (ds) and the exact time these values were
 *       provided by the data source (ds).
 *
 * pdp_st - PDP Start (pdp_st). The moments (pdp_st) in time where
 *       these steps occur are defined by the moments where the
 *       number of seconds since 1970-jan-1 modulo pdp_step equals
 *       zero (pdp_st). 
 *
 * cf -  Consolidation Function (cf). An arbitrary Consolidation Function (cf)
 *       (averaging, min, max) is applied to the primary data points (pdp) to
 *       calculate the consolidated data point.
 *
 * cdp - Consolidated Data Point (cdp) is the long term storage format for data
 *       in the rrd database. Consolidated Data Points represent one or
 *       several primary data points collected along the time axis. The
 *       Consolidated Data Points (cdp) are stored in Round Robin Archives
 *       (rra).
 *
 * rra - Round Robin Archive (rra). This is the place where the
 *       consolidated data points (cdp) get stored. The data is
 *       organized in rows (row) and columns (col). The Round Robin
 *       Archive got its name from the method data is stored in
 *       there. An RRD database can contain several Round Robin
 *       Archives. Each Round Robin Archive can have a different row
 *       spacing along the time axis (pdp_cnt) and a different
 *       consolidation function (cf) used to build its consolidated
 *       data points (cdp).  
 * 
 * rra_st - RRA Start (rra_st). The moments (rra_st) in time where
 *       Consolidated Data Points (cdp) are added to an rra are
 *       defined by the moments where the number of seconds since
 *       1970-jan-1 modulo pdp_cnt*pdp_step equals zero (rra_st).
 *
 * row - Row (row). A row represent all consolidated data points (cdp)
 *       in a round robin archive who are of the same age.
 *       
 * col - Column (col). A column (col) represent all consolidated
 *       data points (cdp) in a round robin archive (rra) who
 *       originated from the same data source (ds).
 *
 */

/****************************************************************************
 * POS 1: stat_head_t                           static header of the database
 ****************************************************************************/

typedef struct stat_head_t {

    /* Data Base Identification Section ***/
    char             cookie[4];          /* RRD */
    char             version[5];         /* version of the format */
    double           float_cookie;       /* is it the correct double
				  	  * representation ?  */

    /* Data Base Structure Definition *****/
    unsigned long    ds_cnt;             /* how many different ds provide
					  * input to the rrd */
    unsigned long    rra_cnt;            /* how many rras will be maintained
					  * in the rrd */
    unsigned long    pdp_step;           /* pdp interval in seconds */

    unival           par[10];            /* global parameters ... unused
					    at the moment */
} stat_head_t;


/****************************************************************************
 * POS 2: ds_def_t  (* ds_cnt)                        Data Source definitions
 ****************************************************************************/

enum dst_en          { DST_COUNTER=0,     /* data source types available */
                       DST_ABSOLUTE, 
                       DST_GAUGE,
                       DST_DERIVE};

enum ds_param_en {   DS_mrhb_cnt=0,       /* minimum required heartbeat. A
					   * data source must provide input at
					   * least every ds_mrhb seconds,
					   * otherwise it is regarded dead and
					   * will be set to UNKNOWN */             
		     DS_min_val,	  /* the processed input of a ds must */
                     DS_max_val };        /* be between max_val and min_val
					   * both can be set to UNKNOWN if you
					   * do not care. Data outside the limits
 					   * set to UNKNOWN */

/* The magic number here is one less than DS_NAM_SIZE */
#define DS_NAM_FMT    "%19[a-zA-Z0-9_-]"
#define DS_NAM_SIZE   20

#define DST_FMT    "%19[A-Z]"
#define DST_SIZE   20

typedef struct ds_def_t {
    char             ds_nam[DS_NAM_SIZE]; /* Name of the data source (null terminated)*/
    char             dst[DST_SIZE];       /* Type of data source (null terminated)*/
    unival           par[10];             /* index of this array see ds_param_en */
} ds_def_t;

/****************************************************************************
 * POS 3: rra_def_t ( *  rra_cnt)         one for each store to be maintained
 ****************************************************************************/
enum cf_en           { CF_AVERAGE=0,     /* data consolidation functions */ 
                       CF_MINIMUM, 
                       CF_MAXIMUM,
                       CF_LAST};

enum rra_par_en {   RRA_cdp_xff_val=0};   /* what part of the consolidated 
					    datapoint may be unknown, while 
					    still a valid entry in goes into the rra */
		   	
#define CF_NAM_FMT    "%19[A-Z]"
#define CF_NAM_SIZE   20

typedef struct rra_def_t {
    char             cf_nam[CF_NAM_SIZE];/* consolidation function (null term) */
    unsigned long    row_cnt;            /* number of entries in the store */
    unsigned long    pdp_cnt;            /* how many primary data points are
					  * required for a consolidated data
					  * point?*/
    unival           par[10];            /* index see rra_param_en */

} rra_def_t;


/****************************************************************************
 ****************************************************************************
 ****************************************************************************
 * LIVE PART OF THE HEADER. THIS WILL BE WRITTEN ON EVERY UPDATE         *
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************/
/****************************************************************************
 * POS 4: live_head_t                    
 ****************************************************************************/

typedef struct live_head_t {
    time_t           last_up;            /* when was rrd last updated */
} live_head_t;


/****************************************************************************
 * POS 5: pdp_prep_t  (* ds_cnt)                     here we prepare the pdps 
 ****************************************************************************/
#define LAST_DS_LEN 30 /* DO NOT CHANGE THIS ... */

enum pdp_par_en {   PDP_unkn_sec_cnt=0,  /* how many seconds of the current
					  * pdp value is unknown data? */

		    PDP_val};	         /* current value of the pdp.
					    this depends on dst */

typedef struct pdp_prep_t{    
    char last_ds[LAST_DS_LEN];           /* the last reading from the data
					  * source.  this is stored in ASCII
					  * to cater for very large counters
					  * we might encounter in connection
					  * with SNMP. */
    unival          scratch[10];         /* contents according to pdp_par_en */
} pdp_prep_t;

/* data is passed from pdp to cdp when seconds since epoch modulo pdp_step == 0
   obviously the updates do not occur at these times only. Especially does the
   format allow for updates to occur at different times for each data source.
   The rules which makes this work is as follows:

   * DS updates may only occur at ever increasing points in time
   * When any DS update arrives after a cdp update time, the *previous*
     update cycle gets executed. All pdps are transfered to cdps and the
     cdps feed the rras where necessary. Only then the new DS value
     is loaded into the PDP.                                                   */


/****************************************************************************
 * POS 6: cdp_prep_t (* rra_cnt * ds_cnt )      data prep area for cdp values
 ****************************************************************************/
enum cdp_par_en {  CDP_val=0,          /* the base_interval is always an
					  * average */
		   CDP_unkn_pdp_cnt };       /* how many unknown pdp were
               				  * integrated. This and the cdp_xff
					    will decide if this is going to
					    be a UNKNOWN or a valid value */

typedef struct cdp_prep_t{
    unival         scratch[10];          /* contents according to cdp_par_en *
                                          * init state should be NAN */

} cdp_prep_t;

/****************************************************************************
 * POS 7: rra_ptr_t (* rra_cnt)       pointers to the current row in each rra
 ****************************************************************************/

typedef struct rra_ptr_t {
    unsigned long    cur_row;            /* current row in the rra*/
} rra_ptr_t;


/****************************************************************************
 ****************************************************************************
 * One single struct to hold all the others. For convenience.
 ****************************************************************************
 ****************************************************************************/
typedef struct rrd_t {
    stat_head_t      *stat_head;          /* the static header */
    ds_def_t         *ds_def;             /* list of data source definitions */
    rra_def_t        *rra_def;            /* list of round robin archive def */
    live_head_t      *live_head;
    pdp_prep_t       *pdp_prep;           /* pdp data prep area */  
    cdp_prep_t       *cdp_prep;           /* cdp prep area */
    rra_ptr_t        *rra_ptr;            /* list of rra pointers */
    rrd_value_t      *rrd_value;          /* list of rrd values */
} rrd_t;

/****************************************************************************
 ****************************************************************************
 * AFTER the header section we have the DATA STORAGE AREA it is made up from
 * Consolidated Data Points organized in Round Robin Archives.
 ****************************************************************************
 ****************************************************************************
 
 *RRA 0
 (0,0) .................... ( ds_cnt -1 , 0)
 .
 . 
 .
 (0, row_cnt -1) ... (ds_cnt -1, row_cnt -1)

 *RRA 1
 *RRA 2

 *RRA rra_cnt -1
 
 ****************************************************************************/


#endif




