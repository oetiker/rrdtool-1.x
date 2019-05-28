/*****************************************************************************
 * RRDtool 1.7.2 Copyright by Tobi Oetiker
 *****************************************************************************
 * rrd_restore.c  Contains logic to parse XML input and create an RRD file
 * This file:
 * Copyright (C) 2008  Florian octo Forster  (original libxml2 code)
 * Copyright (C) 2008,2009 Tobias Oetiker (rewrite using the pull parser)
 *****************************************************************************
 * $Id$
 *************************************************************************** */

#include "rrd_tool.h"
#include "rrd_rpncalc.h"
#include "rrd_restore.h"
#include "unused.h"
#include "rrd_strtod.h"
#include "rrd_create.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <locale.h>

#ifndef _WIN32
#	include <unistd.h>     /* for off_t */
#else
#ifndef __MINGW32__     /* MinGW-w64 has ssize_t and off_t */
	typedef size_t ssize_t;
	typedef long off_t;
#endif
#endif

#include <fcntl.h>
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)
# include <io.h>
# define open _open
# define close _close
#endif

#ifdef HAVE_LIBRADOS
#include "rrd_rados.h"
#endif


#define ARRAY_LENGTH(a) (sizeof (a) / sizeof ((a)[0]))

static int opt_range_check = 0;
static int opt_force_overwrite = 0;

/*
 * Helpers
 */

/* skip all but tags. complain if we do not get the right tag */
/* dept -1 causes depth to be ignored */
static xmlChar* get_xml_element (
    xmlTextReaderPtr reader
    )
{
    int rc;
    while((rc = xmlTextReaderRead(reader)) == 1){
        int type;
        xmlChar *name;
        type = xmlTextReaderNodeType(reader);
        if (type == XML_READER_TYPE_TEXT){
            xmlChar *value;
            value = xmlTextReaderValue(reader);
            rrd_set_error("line %d: expected element but found text '%s'",
                          xmlTextReaderGetParserLineNumber(reader),value);
            xmlFree(value);
            return NULL;
        }
        /* skip all other non-elements */
        if (type != XML_READER_TYPE_ELEMENT && type != XML_READER_TYPE_END_ELEMENT)
            continue;

        name = xmlTextReaderName(reader);
        if (type == XML_READER_TYPE_END_ELEMENT){
            xmlChar *temp;
            xmlChar *temp2;            
            temp = (xmlChar*)sprintf_alloc("/%s",name);
            temp2 = xmlStrdup(temp);
            free(temp);
            xmlFree(name);            
            return temp2;            
        }
        /* all seems well, return the happy news */
        return name;
    }
    if (rc == 0) {
	rrd_set_error("the xml ended while we were looking for an element");
    } else {
	xmlErrorPtr err = xmlGetLastError();
	/* argh: err->message often contains \n at the end. This is not 
	   what we want: Bite the bullet by copying the message, replacing any 
	   \n, constructing the rrd error message and freeing the temp. buffer.
	*/
	char *msgcpy = NULL, *c;
	if (err != NULL && err->message != NULL) {
	    msgcpy = strdup(err->message);
	    if (msgcpy != NULL) {
		for (c = msgcpy ; *c ; c++) {
		    if (*c == '\n') *c = ' ';
		}
		/* strip whitespace from end of message */
		for (c-- ; c != msgcpy ; c--) {
		    if (!isprint(*c)) {
			*c = 0;
		    }
		}
	    } else {
		/* out of memory during error handling, hmmmm */
	    }
	}

	rrd_set_error("error reading/parsing XML: %s", 
		      msgcpy != NULL ? msgcpy : "?");
	if (msgcpy) free(msgcpy);
    }
    return NULL;
} /* get_xml_element */

static void local_rrd_free (rrd_t *rrd)
{    
    free(rrd->live_head);
    free(rrd->stat_head);
    free(rrd->ds_def);
    free(rrd->rra_def); 
    free(rrd->rra_ptr);
    free(rrd->pdp_prep);
    free(rrd->cdp_prep);
    free(rrd->rrd_value);
    free(rrd);
}


static int expect_element (
    xmlTextReaderPtr reader,
    char *exp_name)
{
    xmlChar *name;
    name = get_xml_element(reader);
    if (!name)
        return -1;    
    if (xmlStrcasecmp(name,(xmlChar *)exp_name) != 0){
        rrd_set_error("line %d: expected <%s> element but found <%s>",
                      xmlTextReaderGetParserLineNumber(reader),name,exp_name);
        xmlFree(name);            
        return -1;            
    }
    xmlFree(name);    
    return 0;    
} /* expect_element */

static int expect_element_end (
    xmlTextReaderPtr reader,
    char *exp_name)
{
    xmlChar *name;
    /* maybe we are already on the end element ... lets see */
    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT){
         xmlChar *temp;
         xmlChar *temp2;            
         temp = xmlTextReaderName(reader);
         temp2 = (xmlChar*)sprintf_alloc("/%s", temp);
         name = xmlStrdup(temp2);
         xmlFree(temp);
         free(temp2);            
    } else {     
         name = get_xml_element(reader);
    }

    if (name == NULL)
        return -1;    
    if (xmlStrcasecmp(name+1,(xmlChar *)exp_name) != 0 || name[0] != '/'){
        rrd_set_error("line %d: expected </%s> end element but found <%s>",
                      xmlTextReaderGetParserLineNumber(reader),exp_name,name);
        xmlFree(name);            
        return -1;            
    }
    xmlFree(name);    
    return 0;    
} /* expect_element_end */


static xmlChar* get_xml_text (
    xmlTextReaderPtr reader
    )
{
    while(xmlTextReaderRead(reader)){
        xmlChar  *ret;    
        xmlChar  *text;
        xmlChar  *begin_ptr;
        xmlChar  *end_ptr;
        int type;        
        type = xmlTextReaderNodeType(reader);
        if (type == XML_READER_TYPE_ELEMENT){
            xmlChar *name;
            name = xmlTextReaderName(reader);
            rrd_set_error("line %d: expected a value but found a <%s> element",
                          xmlTextReaderGetParserLineNumber(reader),
                          name);
            xmlFree(name);            
            return NULL;            
        }

        /* trying to read text from <a></a> we end up here
           lets return an empty string instead. This is a tad optimistic
           since we do not check if it is actually </a> and not </b>
           we got, but first we do not know if we expect </a> and second
           we the whole implementation is on the optimistic side. */
        if (type == XML_READER_TYPE_END_ELEMENT){
            return  xmlStrdup(BAD_CAST "");
        }        

        /* skip all other non-text */
        if (type != XML_READER_TYPE_TEXT)
            continue;
        
        text = xmlTextReaderValue(reader);

        begin_ptr = text;
        while ((begin_ptr[0] != 0) && (isspace(begin_ptr[0])))
            begin_ptr++;
        if (begin_ptr[0] == 0) {
            xmlFree(text);
            return xmlStrdup(BAD_CAST "");
        }        
        end_ptr = begin_ptr;
        while ((end_ptr[0] != 0) && (!isspace(end_ptr[0])))
            end_ptr++;
        end_ptr[0] = 0;
        
        ret = xmlStrdup(begin_ptr);
        xmlFree(text);
        return ret;
    }
    rrd_set_error("file ended while looking for text");
    return NULL;
}  /* get_xml_text */ 


static int get_xml_string(
    xmlTextReaderPtr reader,
    char *value,
    unsigned int max_len)
{
    xmlChar *str;
    str = get_xml_text(reader);
    if (str != NULL){
        if (strlen((char *)str) >= max_len){
            rrd_set_error("'%s' is longer than %i",str,max_len);
            return -1;
        }            
        strncpy(value,(char *)str,max_len);
        xmlFree(str);
        return 0;        
    }
    else
        return -1;    
}

 
static int get_xml_time_t(
    xmlTextReaderPtr reader,
    time_t *value)
{    
    xmlChar *text;
    time_t temp;    
    if ((text = get_xml_text(reader)) != NULL){
        errno = 0;        
#if SIZEOF_TIME_T == 4
        temp = strtol((char *)text,NULL, 0);
#elif SIZEOF_TIME_T == 8
        temp = strtoll((char *)text,NULL, 0);        
#else
#error "Don't know how to deal with TIME_T other than 4 or 8 bytes"
#endif    
        if (errno>0){
            rrd_set_error("ling %d: get_xml_time_t from '%s' %s",
                          xmlTextReaderGetParserLineNumber(reader),
                          text,rrd_strerror(errno));
            xmlFree(text);            
            return -1;
        }
        xmlFree(text);            
        *value = temp;
        return 0;
    }
    return -1;
} /* get_xml_time_t */

static int get_xml_ulong(
    xmlTextReaderPtr reader,
    unsigned long *value)
{
    
    xmlChar *text;
    unsigned long temp;    
    if ((text = get_xml_text(reader)) != NULL){
        errno = 0;        
        temp = strtoul((char *)text,NULL, 0);        
        if (errno>0){
            rrd_set_error("ling %d: get_xml_ulong from '%s' %s",
                          xmlTextReaderGetParserLineNumber(reader),
                          text,rrd_strerror(errno));
            xmlFree(text);            
            return -1;
        }
        xmlFree(text);
        *value = temp;        
        return 0;
    }
    return -1;
} /* get_xml_ulong */

static int get_xml_double(
    xmlTextReaderPtr reader,
    double *value)
{
    
    xmlChar *text;
    double temp;    
    if ((text = get_xml_text(reader))!= NULL){
        if (xmlStrcasestr(text,(xmlChar *)"nan")){
            *value = DNAN;
            xmlFree(text);
            return 0;            
        }
        else if (xmlStrcasestr(text,(xmlChar *)"-inf")){
            *value = -DINF;
            xmlFree(text);
            return 0;            
        }
        else if (xmlStrcasestr(text,(xmlChar *)"+inf")
                 || xmlStrcasestr(text,(xmlChar *)"inf")){
            *value = DINF;
            xmlFree(text);
            return 0;            
        }        
        if ( rrd_strtodbl((char *)text,NULL, &temp, NULL) != 2 ){
            rrd_set_error("ling %d: get_xml_double from '%s' %s",
                          xmlTextReaderGetParserLineNumber(reader),
                          text,rrd_strerror(errno));
            xmlFree(text);
            return -1;
        }
        xmlFree(text);        
        *value = temp;
        return 0;
    }
    return -1;
} /* get_xml_double */


static int value_check_range(
    rrd_value_t *rrd_value,
    const ds_def_t *ds_def)
{
    double    min;
    double    max;

    if (opt_range_check == 0)
        return (0);

    min = ds_def->par[DS_min_val].u_val;
    max = ds_def->par[DS_max_val].u_val;

    if (((!isnan(min)) && (*rrd_value < min))
        || ((!isnan(max)) && (*rrd_value > max)))
        *rrd_value = DNAN;

    return (0);
} /* int value_check_range */

/*
 * Parse the <database> block within an RRA definition
 */

static int parse_tag_rra_database_row(
    xmlTextReaderPtr reader,
    rrd_t *rrd,
    rrd_value_t *rrd_value)
{
    unsigned int values_count = 0;
    int       status;
    
    status = 0;
    for (values_count = 0;values_count <  rrd->stat_head->ds_cnt;values_count++){
        if (expect_element(reader,"v") == 0){
            status = get_xml_double(reader,rrd_value + values_count);
            if (status == 0)
                value_check_range(rrd_value + values_count,
                                  rrd->ds_def + values_count);
            else
                break;            
        } else
            return -1;
        if (expect_element(reader,"/v") == -1){
            return -1;
        }
    }
    return status;
}                       /* int parse_tag_rra_database_row */

static int parse_tag_rra_database(
    xmlTextReaderPtr reader,
    rrd_t *rrd )
{
    rra_def_t *cur_rra_def;
    rra_ptr_t *cur_rra_ptr;
    unsigned int total_row_cnt;
    int       status;
    int       i;
    xmlChar *element;
    unsigned int start_row_cnt;
    int       ds_cnt;
    
    ds_cnt = rrd->stat_head->ds_cnt;
    
    total_row_cnt = 0;
    for (i = 0; i < (((int) rrd->stat_head->rra_cnt) - 1); i++)
        total_row_cnt += rrd->rra_def[i].row_cnt;

    cur_rra_def = rrd->rra_def + i;
    cur_rra_ptr = rrd->rra_ptr + i;
    start_row_cnt = total_row_cnt;
    
    status = 0;
    while ((element = get_xml_element(reader)) != NULL){        
        if (xmlStrcasecmp(element,(const xmlChar *)"row") == 0){
           rrd_value_t *temp;
           rrd_value_t *cur_rrd_value;
           unsigned int total_values_count = rrd->stat_head->ds_cnt
               * (total_row_cnt + 1);

            /* Allocate space for the new values.. */
            temp = (rrd_value_t *) realloc(rrd->rrd_value,
                                           sizeof(rrd_value_t) *
                                           total_values_count);
            if (temp == NULL) {
                rrd_set_error("parse_tag_rra_database: realloc failed.");
                status = -1;
               break;
            }
            rrd->rrd_value = temp;
            cur_rrd_value = rrd->rrd_value
                + (rrd->stat_head->ds_cnt * total_row_cnt);
            memset(cur_rrd_value, '\0',
                   sizeof(rrd_value_t) * rrd->stat_head->ds_cnt);
            total_row_cnt++;
            cur_rra_def->row_cnt++;

            status =
                parse_tag_rra_database_row(reader, rrd, cur_rrd_value);
            if (status == 0)
                status =  expect_element(reader,"/row");
        } /* if (xmlStrcasecmp(element,"row")) */
        else {
            if ( xmlStrcasecmp(element,(const xmlChar *)"/database") == 0){
                xmlFree(element);                
                break;
            }
            else {
                rrd_set_error("line %d: found unexpected tag: %s",
                              xmlTextReaderGetParserLineNumber(reader),element);
                status = -1;
            }
        }
        xmlFree(element);        
        if (status != 0)
            break;        
    }
    
    /* Set the RRA pointer to a random location */
    cur_rra_ptr->cur_row = rrd_random() % cur_rra_def->row_cnt;
    
    /*
     * rotate rows to match cur_row...
     * 
     * this will require some extra temp. memory. We can do this rather 
     * brainlessly, because we have done all kinds of realloc before, 
     * so we messed around with memory a lot already.
     */
    
    /*
        
     What we want:
     
     +-start_row_cnt
     |           +-cur_rra_def->row_cnt
     |           |
     |a---------n012-------------------------|
    
   (cur_rra_def->row_cnt slots of ds_cnt width)
   
     What we have 
      
     |   
     |012-------------------------a---------n|
     
     Do this by:
     copy away 0..(a-1) to a temp buffer
     move a..n to start of buffer
     copy temp buffer to position after where we moved n to
     */
    
    int a = cur_rra_def->row_cnt - cur_rra_ptr->cur_row - 1;
    
    rrd_value_t *temp = malloc(ds_cnt * sizeof(rrd_value_t) * a);
    if (temp == NULL) {
        rrd_set_error("parse_tag_rra: malloc failed.");
        return -1;
    }

    rrd_value_t *start = rrd->rrd_value + start_row_cnt * ds_cnt;
    /* */            
    memcpy(temp, start,
            a * ds_cnt * sizeof(rrd_value_t));
    
    memmove(start,
            start + a * ds_cnt,
            (cur_rra_ptr->cur_row + 1) * ds_cnt * sizeof(rrd_value_t));
            
    memcpy(start + (cur_rra_ptr->cur_row + 1) * ds_cnt,
           temp,
           a * ds_cnt * sizeof(rrd_value_t));
            
    free(temp);

    return (status);
}                       /* int parse_tag_rra_database */

/*
 * Parse the <cdp_prep> block within an RRA definition
 */
static int parse_tag_rra_cdp_prep_ds_history(
    xmlTextReaderPtr reader,
    cdp_prep_t *cdp_prep)
{
    /* Make `history_buffer' the same size as the scratch area, plus the
     * terminating NULL byte. */
    xmlChar  *history;    
    char     *history_ptr;
    int       i;
    if ((history = get_xml_text(reader)) != NULL){
        history_ptr = (char *) (&cdp_prep->scratch[0]);
        for (i = 0; history[i] != '\0' && i < MAX_CDP_PAR_EN; i++)
            history_ptr[i] = (history[i] == '1') ? 1 : 0;
        xmlFree(history);        
        return 0;        
    }    
    return -1;    
}  /* int parse_tag_rra_cdp_prep_ds_history */

static int parse_tag_rra_cdp_prep_ds(
    xmlTextReaderPtr reader,
    rrd_t *rrd,
    cdp_prep_t *cdp_prep)
{
    int       status;
    xmlChar *element;
    memset(cdp_prep, '\0', sizeof(cdp_prep_t));

    status = -1;
    
    if (atoi(rrd->stat_head->version) == 1) {
        cdp_prep->scratch[CDP_primary_val].u_val = 0.0;
        cdp_prep->scratch[CDP_secondary_val].u_val = 0.0;
    }

    while ((element = get_xml_element(reader)) != NULL){
        if (xmlStrcasecmp(element, (const xmlChar *) "primary_value") == 0)
            status =
                get_xml_double(reader,&cdp_prep->scratch[CDP_primary_val].u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "secondary_value") == 0)
            status =
                get_xml_double(reader,&cdp_prep->scratch[CDP_secondary_val].u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "intercept") == 0)
            status = get_xml_double(reader,
                                          &cdp_prep->
                                          scratch[CDP_hw_intercept].u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "last_intercept") ==
                 0)
            status =
                get_xml_double(reader,
                                     &cdp_prep->
                                     scratch[CDP_hw_last_intercept].u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "slope") == 0)
            status = get_xml_double(reader,
                                    &cdp_prep->scratch[CDP_hw_slope].
                                    u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "last_slope") == 0)
            status = get_xml_double(reader,
                                    &cdp_prep->
                                    scratch[CDP_hw_last_slope].u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "nan_count") == 0)
            status = get_xml_ulong(reader,
                                   &cdp_prep->
                                   scratch[CDP_null_count].u_cnt);
        else if (xmlStrcasecmp(element, (const xmlChar *) "last_nan_count") ==
                 0)
            status =
                get_xml_ulong(reader,
                              &cdp_prep->
                              scratch[CDP_last_null_count].u_cnt);
        else if (xmlStrcasecmp(element, (const xmlChar *) "seasonal") == 0)
            status = get_xml_double(reader,
                                    &cdp_prep->scratch[CDP_hw_seasonal].
                                    u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "last_seasonal") ==
                 0)
            status =
                get_xml_double(reader,
                                     &cdp_prep->scratch[CDP_hw_last_seasonal].
                                     u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "init_flag") == 0)
            status = get_xml_ulong(reader,
                                        &cdp_prep->
                                       scratch[CDP_init_seasonal].u_cnt);
        else if (xmlStrcasecmp(element, (const xmlChar *) "history") == 0)
            status = parse_tag_rra_cdp_prep_ds_history(reader, cdp_prep);
        else if (xmlStrcasecmp(element, (const xmlChar *) "value") == 0)
            status = get_xml_double(reader,
                                    &cdp_prep->scratch[CDP_val].u_val);
        else if (xmlStrcasecmp(element,
                           (const xmlChar *) "unknown_datapoints") == 0)
            status = get_xml_ulong(reader,
                                        &cdp_prep->
                                       scratch[CDP_unkn_pdp_cnt].u_cnt);
        else if (xmlStrcasecmp(element,
                               (const xmlChar *) "/ds") == 0){
            xmlFree(element);            
            break;
        }        
        else {
            rrd_set_error("parse_tag_rra_cdp_prep: Unknown tag: %s",
                          element);
            status = -1;
            xmlFree(element);            
            break;            
        }
        if (status != 0){
            xmlFree(element);
            break;
        }
        status = expect_element_end(reader,(char *)element);
        xmlFree(element);        
        if (status != 0)
            break;
    }    
    return (status);
}                       /* int parse_tag_rra_cdp_prep_ds */

static int parse_tag_rra_cdp_prep(
    xmlTextReaderPtr reader,
    rrd_t *rrd,
    cdp_prep_t *cdp_prep)
{
    int       status;

    unsigned int ds_count;

    status = 0;
    for ( ds_count = 0; ds_count < rrd->stat_head->ds_cnt;ds_count++){
        if (expect_element(reader,"ds") == 0) {
            status = parse_tag_rra_cdp_prep_ds(reader, rrd,
                                               cdp_prep + ds_count);
            if (status != 0)
                break;
        } else {
            status = -1;            
            break;
        }        
    }
    if (status == 0)
        status =  expect_element(reader,"/cdp_prep");
    return (status);
}                       /* int parse_tag_rra_cdp_prep */

/*
 * Parse the <params> block within an RRA definition
 */
static int parse_tag_rra_params(
    xmlTextReaderPtr reader,
    rra_def_t *rra_def)
{
    xmlChar *element;
    int       status;

    status = -1;
    while ((element = get_xml_element(reader)) != NULL){
        /*
         * Parameters for CF_HWPREDICT
         */
        if (xmlStrcasecmp(element, (const xmlChar *) "hw_alpha") == 0)
            status = get_xml_double(reader,
                                          &rra_def->par[RRA_hw_alpha].u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "hw_beta") == 0)
            status = get_xml_double(reader,
                                          &rra_def->par[RRA_hw_beta].u_val);
        else if (xmlStrcasecmp(element,
                           (const xmlChar *) "dependent_rra_idx") == 0)
            status = get_xml_ulong(reader,
                                        &rra_def->
                                       par[RRA_dependent_rra_idx].u_cnt);
        /*
         * Parameters for CF_SEASONAL and CF_DEVSEASONAL
         */
        else if (xmlStrcasecmp(element, (const xmlChar *) "seasonal_gamma") ==
                 0)
            status =
                get_xml_double(reader,
                                     &rra_def->par[RRA_seasonal_gamma].u_val);
        else if (xmlStrcasecmp
                 (element, (const xmlChar *) "seasonal_smooth_idx") == 0)
            status =
                get_xml_ulong(reader,
                                   &rra_def->
                                  par[RRA_seasonal_smooth_idx].u_cnt);
        else if (xmlStrcasecmp(element, (const xmlChar *) "smoothing_window")
                 == 0)
            status =
                get_xml_double(reader,
                                     &rra_def->
                                     par[RRA_seasonal_smoothing_window].
                                     u_val);
        /* else if (dependent_rra_idx) ...; */
        /*
         * Parameters for CF_FAILURES
         */
        else if (xmlStrcasecmp(element, (const xmlChar *) "delta_pos") == 0)
            status = get_xml_double(reader,
                                          &rra_def->par[RRA_delta_pos].u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "delta_neg") == 0)
            status = get_xml_double(reader,
                                          &rra_def->par[RRA_delta_neg].u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "window_len") == 0)
            status = get_xml_ulong(reader,
                                        &rra_def->par[RRA_window_len].
                                       u_cnt);
        else if (xmlStrcasecmp(element, (const xmlChar *) "failure_threshold")
                 == 0)
            status =
                get_xml_ulong(reader,
                                   &rra_def->
                                  par[RRA_failure_threshold].u_cnt);
        /*
         * Parameters for CF_AVERAGE, CF_MAXIMUM, CF_MINIMUM, and CF_LAST
         */
        else if (xmlStrcasecmp(element, (const xmlChar *) "xff") == 0)
            status = get_xml_double(reader,
                                          &rra_def->par[RRA_cdp_xff_val].
                                          u_val);
        /*
         * Compatibility code for 1.0.49
         */
        else if (xmlStrcasecmp(element, (const xmlChar *) "value") == 0) {  /* {{{ */
            unsigned int i = 0;

            for (i=0;i<ARRAY_LENGTH(rra_def->par);i++){
                if ((i == RRA_dependent_rra_idx)
                    || (i == RRA_seasonal_smooth_idx)
                    || (i == RRA_failure_threshold))
                    status = get_xml_ulong(reader,
                                                &rra_def->par[i].
                                               u_cnt);
                else
                    status = get_xml_double(reader,
                                                  &rra_def->par[i].u_val);

                if (status != 0)
                    break;
                if ( i-1 < ARRAY_LENGTH(rra_def->par)){
                    status = expect_element(reader,"/value");
                    if (status == 0){
                        status  = expect_element(reader,"value");
                    }
                }
                if (status != 0){
                    break;                    
                }
            }
        }  /* }}} */        
        else if (xmlStrcasecmp(element,(const xmlChar *) "/params") == 0){
            xmlFree(element);            
            return status;
        }  /* }}} */        
        else {
            rrd_set_error("line %d: parse_tag_rra_params: Unknown tag: %s",
                          xmlTextReaderGetParserLineNumber(reader),element);
            status = -1;
        }
        status = expect_element_end(reader,(char *)element);
        xmlFree(element);        
        if (status != 0)
            break;
    }
    return (status);
}                       /* int parse_tag_rra_params */

/*
 * Parse an RRA definition
 */
static int parse_tag_rra_cf(
    xmlTextReaderPtr reader,
    rra_def_t *rra_def)
{
    int       status;

    status = get_xml_string(reader,
                                  rra_def->cf_nam, sizeof(rra_def->cf_nam));
    if (status != 0)
        return status;

    status = rrd_cf_conv(rra_def->cf_nam);
    if (status == -1) {
        rrd_set_error("parse_tag_rra_cf: Unknown consolidation function: %s",
                      rra_def->cf_nam);
        return -1;
    }

    return 0;
}                       /* int parse_tag_rra_cf */

static int parse_tag_rra(
    xmlTextReaderPtr reader,
    rrd_t *rrd)
{
    int       status;
    xmlChar *element;
    
    rra_def_t *cur_rra_def;
    cdp_prep_t *cur_cdp_prep;
    rra_ptr_t *cur_rra_ptr;

    /* Allocate more rra_def space for this RRA */
    {                   /* {{{ */
        rra_def_t *temp;

        temp = (rra_def_t *) realloc(rrd->rra_def,
                                     sizeof(rra_def_t) *
                                     (rrd->stat_head->rra_cnt + 1));
        if (temp == NULL) {
            rrd_set_error("parse_tag_rra: realloc failed.");
            return (-1);
        }
        rrd->rra_def = temp;
        cur_rra_def = rrd->rra_def + rrd->stat_head->rra_cnt;
        memset(cur_rra_def, '\0', sizeof(rra_def_t));
    }                   /* }}} */

    /* allocate cdp_prep_t */
    {                   /* {{{ */
        cdp_prep_t *temp;

        temp = (cdp_prep_t *) realloc(rrd->cdp_prep, sizeof(cdp_prep_t)
                                      * rrd->stat_head->ds_cnt
                                      * (rrd->stat_head->rra_cnt + 1));
        if (temp == NULL) {
            rrd_set_error("parse_tag_rra: realloc failed.");
            return (-1);
        }
        rrd->cdp_prep = temp;
        cur_cdp_prep = rrd->cdp_prep
            + (rrd->stat_head->ds_cnt * rrd->stat_head->rra_cnt);
        memset(cur_cdp_prep, '\0',
               sizeof(cdp_prep_t) * rrd->stat_head->ds_cnt);
    }                   /* }}} */

    /* allocate rra_ptr_t */
    {                   /* {{{ */
        rra_ptr_t *temp;

        temp = (rra_ptr_t *) realloc(rrd->rra_ptr,
                                     sizeof(rra_ptr_t) *
                                     (rrd->stat_head->rra_cnt + 1));
        if (temp == NULL) {
            rrd_set_error("parse_tag_rra: realloc failed.");
            return (-1);
        }
        rrd->rra_ptr = temp;
        cur_rra_ptr = rrd->rra_ptr + rrd->stat_head->rra_cnt;
        memset(cur_rra_ptr, '\0', sizeof(rra_ptr_t));
    }                   /* }}} */

    /* All space successfully allocated, increment number of RRAs. */
    rrd->stat_head->rra_cnt++;
    
    status = 0;
    while ((element = get_xml_element(reader)) != NULL){
        if (xmlStrcasecmp(element, (const xmlChar *) "cf") == 0)
            status = parse_tag_rra_cf(reader, cur_rra_def);
        else if (xmlStrcasecmp(element, (const xmlChar *) "pdp_per_row") == 0)
            status = get_xml_ulong(reader,
                                        &cur_rra_def->pdp_cnt);
        else if (atoi(rrd->stat_head->version) == 1
                 && xmlStrcasecmp(element, (const xmlChar *) "xff") == 0)
            status = get_xml_double(reader,
                                          (double *) &cur_rra_def->
                                          par[RRA_cdp_xff_val].u_val);
        else if (atoi(rrd->stat_head->version) >= 2
                 && xmlStrcasecmp(element, (const xmlChar *) "params") == 0){            
            xmlFree(element);
            status = parse_tag_rra_params(reader, cur_rra_def);
            if (status == 0)
                continue;
            else
                return status;
        }
        else if (xmlStrcasecmp(element, (const xmlChar *) "cdp_prep") == 0){
            xmlFree(element);
            status = parse_tag_rra_cdp_prep(reader, rrd, cur_cdp_prep);
            if (status == 0)
                continue;
            else
                return status;
        }        
        else if (xmlStrcasecmp(element, (const xmlChar *) "database") == 0){            
            xmlFree(element);
            status = parse_tag_rra_database(reader, rrd);
            if (status == 0)
                continue;
            else
                return status;
        }
        else if (xmlStrcasecmp(element,(const xmlChar *) "/rra") == 0){
            xmlFree(element);
            return status;
        }  /* }}} */        
       else {
            rrd_set_error("line %d: parse_tag_rra: Unknown tag: %s",
                          xmlTextReaderGetParserLineNumber(reader), element);
            status = -1;            
        }
        if (status != 0) {
            xmlFree(element);
            return status;
        }        
        status = expect_element_end(reader,(char *)element);
        xmlFree(element);
        if (status != 0) {
            return status;
        }        
    }    
    return (status);
}                       /* int parse_tag_rra */

/*
 * Parse a DS definition
 */
static int parse_tag_ds_cdef(
    xmlTextReaderPtr reader,
    rrd_t *rrd)
{
    xmlChar *cdef;

    cdef = get_xml_text(reader);
    if (cdef != NULL){
        /* We're always working on the last DS that has been added to the structure
         * when we get here */
        parseCDEF_DS((char *)cdef, rrd->ds_def + rrd->stat_head->ds_cnt - 1,
		     rrd, lookup_DS);
        xmlFree(cdef);
        if (rrd_test_error())
            return -1;
        else            
            return 0;        
    }
    return -1;
}                       /* int parse_tag_ds_cdef */

static int parse_tag_ds_type(
    xmlTextReaderPtr reader,
    ds_def_t *ds_def)
{
    char *dst;
    dst = (char *)get_xml_text(reader);
    if (dst != NULL){
        int status;
        status = dst_conv(dst);
        if (status == -1) {
            rrd_set_error("parse_tag_ds_type: Unknown data source type: %s",
                          dst);
            return -1;
        }
        strncpy(ds_def->dst,dst,sizeof(ds_def->dst)-1);
        ds_def->dst[sizeof(ds_def->dst)-1] = '\0';
        xmlFree(dst);
        return 0;        
    }
    return -1;
}                       /* int parse_tag_ds_type */

static int parse_tag_ds(
    xmlTextReaderPtr reader,
    rrd_t *rrd)
{
    int       status;
    xmlChar  *element;
    
    ds_def_t *cur_ds_def;
    pdp_prep_t *cur_pdp_prep;

    /*
     * If there are DS definitions after RRA definitions the number of values,
     * cdp_prep areas and so on will be calculated wrong. Thus, enforce a
     * specific order in this case.
     */
    if (rrd->stat_head->rra_cnt > 0) {
        rrd_set_error("parse_tag_ds: All data source definitions MUST "
                      "precede the RRA definitions!");
        return (-1);
    }

    /* Allocate space for the new DS definition */
    {                   /* {{{ */
        ds_def_t *temp;

        temp = (ds_def_t *) realloc(rrd->ds_def,
                                    sizeof(ds_def_t) *
                                    (rrd->stat_head->ds_cnt + 1));
        if (temp == NULL) {
            rrd_set_error("parse_tag_ds: malloc failed.");
            return (-1);
        }
        rrd->ds_def = temp;
        cur_ds_def = rrd->ds_def + rrd->stat_head->ds_cnt;
        memset(cur_ds_def, '\0', sizeof(ds_def_t));
    }                   /* }}} */

    /* Allocate pdp_prep space for the new DS definition */
    {                   /* {{{ */
        pdp_prep_t *temp;

        temp = (pdp_prep_t *) realloc(rrd->pdp_prep,
                                      sizeof(pdp_prep_t) *
                                      (rrd->stat_head->ds_cnt + 1));
        if (temp == NULL) {
            rrd_set_error("parse_tag_ds: malloc failed.");
            return (-1);
        }
        rrd->pdp_prep = temp;
        cur_pdp_prep = rrd->pdp_prep + rrd->stat_head->ds_cnt;
        memset(cur_pdp_prep, '\0', sizeof(pdp_prep_t));
    }                   /* }}} */

    /* All allocations successful, let's increment the number of DSes. */
    rrd->stat_head->ds_cnt++;

    status = 0;
    while ((element = get_xml_element(reader)) != NULL){
        if (xmlStrcasecmp(element, (const xmlChar *) "name") == 0){
            status = get_xml_string(reader,cur_ds_def->ds_nam,sizeof(cur_ds_def->ds_nam));
        }
        else if (xmlStrcasecmp(element, (const xmlChar *) "type") == 0)
            status = parse_tag_ds_type(reader, cur_ds_def);
        else if (xmlStrcasecmp(element,
                           (const xmlChar *) "minimal_heartbeat") == 0)
            status = get_xml_ulong(reader,
                                        &cur_ds_def->par[DS_mrhb_cnt].
                                       u_cnt);
        else if (xmlStrcasecmp(element, (const xmlChar *) "min") == 0)
            status = get_xml_double(reader,
                                          &cur_ds_def->par[DS_min_val].u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "max") == 0)
            status = get_xml_double(reader,
                                          &cur_ds_def->par[DS_max_val].u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "cdef") == 0)
            status = parse_tag_ds_cdef(reader, rrd);
        else if (xmlStrcasecmp(element, (const xmlChar *) "last_ds") == 0)
            status = get_xml_string(reader,
                                          cur_pdp_prep->last_ds,
                                          sizeof(cur_pdp_prep->last_ds));
        else if (xmlStrcasecmp(element, (const xmlChar *) "value") == 0)
            status = get_xml_double(reader,
                                          &cur_pdp_prep->scratch[PDP_val].
                                          u_val);
        else if (xmlStrcasecmp(element, (const xmlChar *) "unknown_sec") == 0)
            status = get_xml_ulong(reader,
                                        &cur_pdp_prep->
                                       scratch[PDP_unkn_sec_cnt].u_cnt);
        else if (xmlStrcasecmp(element, (const xmlChar *) "/ds") == 0) {
            xmlFree(element);            
            break;
        }        
        else {
            rrd_set_error("parse_tag_ds: Unknown tag: %s", element);
            status = -1;
        }        
        if (status != 0) {            
            xmlFree(element);        
            break;
        }
        status = expect_element_end(reader,(char *)element);
        xmlFree(element);        
        if (status != 0)
            break;        
    }
    
    return (status);
}                       /* int parse_tag_ds */

/*
 * Parse root nodes
 */
static int parse_tag_rrd(
    xmlTextReaderPtr reader,
    rrd_t *rrd)
{
    int       status;
    xmlChar *element;
    
    status = 0;
    while ((element = get_xml_element(reader)) != NULL ){
        if (xmlStrcasecmp(element, (const xmlChar *) "version") == 0)
            status = get_xml_string(reader,
                                          rrd->stat_head->version,
                                          sizeof(rrd->stat_head->version));
        else if (xmlStrcasecmp(element, (const xmlChar *) "step") == 0)
            status = get_xml_ulong(reader,
                                        &rrd->stat_head->pdp_step);
        else if (xmlStrcasecmp(element, (const xmlChar *) "lastupdate") == 0) {
                status = get_xml_time_t(reader, &rrd->live_head->last_up);
        }
        else if (xmlStrcasecmp(element, (const xmlChar *) "ds") == 0){            
            xmlFree(element);
            status = parse_tag_ds(reader, rrd);
            /* as we come back the </ds> tag is already gone */
            if (status == 0)
                continue;
            else
                return status;
        }        
        else if (xmlStrcasecmp(element, (const xmlChar *) "rra") == 0){            
            xmlFree(element);
            status = parse_tag_rra(reader, rrd);
            if (status == 0)
                continue;
            else
                return status;
        }
        else if (xmlStrcasecmp(element, (const xmlChar *) "/rrd") == 0) {
            xmlFree(element);
            return status;
        }
        else {
            rrd_set_error("parse_tag_rrd: Unknown tag: %s", element);
            status = -1;
        }

        if (status != 0){
            xmlFree(element);
            break;
        }        
        status = expect_element_end(reader,(char *)element);
        xmlFree(element);        
        if (status != 0)
            break;        
    }
    return (status);
}                       /* int parse_tag_rrd */

/* helper type for stdioXmlInputReaderForPipeInterface */

typedef struct stdioXmlReaderContext_t {
    FILE *stream;
    int freeOnClose;
    int closed;
    char eofchar;
} stdioXmlReaderContext;

/* 
 * this is a xmlInputReadCallback that is used for the pipe interface
 * in case the passed filename is "-" (meaning standard input) to take
 * care it will never actually close the stdio stream stdin. It will
 * report eof once it reads the eof character (currently set to ctrl-Z
 * (character code 26, hex 0x1A) in the calling code) anywhere on a line.
 *
 * Note that ctrl-Z is not an allowed character in XML 1.0 (which rrdtool
 * uses). 
 *
 */
static int stdioXmlInputReadCallback(
    void *context, 
    char *buffer, 
    int len)
{
    stdioXmlReaderContext *sctx = (stdioXmlReaderContext*) context;

    if (sctx == NULL) return -1;
    if (sctx->stream == NULL) return -1;
    if (sctx->closed) return 0;

    char *r = fgets(buffer, len, sctx->stream);
    if (r == NULL) {
	sctx->closed = 1;
	return 0;
    }

    char *where = strchr(r, sctx->eofchar);
    if (where != NULL) {
	sctx->closed = 1;
	*where = 0;
    }

    return strlen(r);
}

static int stdioXmlInputCloseCallback(void *context)
{
    stdioXmlReaderContext *sctx = (stdioXmlReaderContext*) context;

    if (sctx == NULL) return 0;

    if (sctx->freeOnClose) {
	sctx->freeOnClose = 0;
	free(sctx);
    }
    return 0; /* everything is OK */
}

/* an XML error reporting function that just suppresses all error messages.
   This is used when parsing an XML file from stdin. This should help to 
   not break the pipe interface protocol by suppressing the sending out of
   XML error messages. */
static void ignoringErrorFunc(
    void UNUSED(*ctx), 
    const char UNUSED(*msg), 
    ...)
{
}

static rrd_t *parse_file(
    const char *filename)
{
    xmlTextReaderPtr reader;
    int       status;

    rrd_t    *rrd;
    stdioXmlReaderContext *sctx = NULL;

    /* special handling for XML on stdin (like it is the case when using
       the pipe interface) */
    if (strcmp(filename, "-") == 0) {
		sctx = (stdioXmlReaderContext *) malloc(sizeof(*sctx));
	if (sctx == NULL) {
	    rrd_set_error("parse_file: malloc failed.");
	    return (NULL);
	}
	sctx->stream = stdin;
	sctx->freeOnClose = 1;
	sctx->closed = 0;
	sctx->eofchar = 0x1A; /* ctrl-Z */

	xmlSetGenericErrorFunc(NULL, ignoringErrorFunc);

        reader = xmlReaderForIO(stdioXmlInputReadCallback,
                                stdioXmlInputCloseCallback,
                                sctx, 
                                filename,
                                NULL,
                                0);
    } else {
        reader = xmlNewTextReaderFilename(filename);
    } 
    if (reader == NULL) {
	if (sctx != NULL) free(sctx);

        rrd_set_error("Could not create xml reader for: %s",filename);
        return (NULL);
    }

    /* NOTE: from now on, sctx will get freed implicitly through
     * xmlFreeTextReader and its call to
     * stdioXmlInputCloseCallback. */

    if (expect_element(reader,"rrd") != 0) {
        xmlFreeTextReader(reader);
        return (NULL);
    }

    rrd = (rrd_t *) malloc(sizeof(rrd_t));
    if (rrd == NULL) {
        rrd_set_error("parse_file: malloc failed.");
        xmlFreeTextReader(reader);
        return (NULL);
    }
    memset(rrd, '\0', sizeof(rrd_t));

    rrd->stat_head = (stat_head_t *) malloc(sizeof(stat_head_t));
    if (rrd->stat_head == NULL) {
        rrd_set_error("parse_tag_rrd: malloc failed.");
        xmlFreeTextReader(reader);
        free(rrd);
        return (NULL);
    }
    memset(rrd->stat_head, '\0', sizeof(stat_head_t));

    strncpy(rrd->stat_head->cookie, "RRD", sizeof(rrd->stat_head->cookie));
    rrd->stat_head->float_cookie = FLOAT_COOKIE;

    rrd->live_head = (live_head_t *) malloc(sizeof(live_head_t));
    if (rrd->live_head == NULL) {
        rrd_set_error("parse_tag_rrd: malloc failed.");
        xmlFreeTextReader(reader);
        free(rrd->stat_head);
        free(rrd);
        return (NULL);
    }
    memset(rrd->live_head, '\0', sizeof(live_head_t));

    status = parse_tag_rrd(reader, rrd);

    xmlFreeTextReader(reader);

    if (status != 0) {
        local_rrd_free(rrd);
        rrd = NULL;
    }

    return (rrd);
}                       /* rrd_t *parse_file */

int write_file(
    const char *file_name,
    rrd_t *rrd)
{
    FILE     *fh;

#ifdef HAVE_LIBRADOS
    if (strncmp("ceph//", file_name, 6) == 0) {
      return rrd_rados_create(file_name + 6, rrd);
    }
#endif

    if (strcmp("-", file_name) == 0)
        fh = stdout;
    else {
        int       fd_flags = O_WRONLY | O_CREAT;
        int       fd;

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)
        fd_flags |= O_BINARY;
#endif

        if (opt_force_overwrite == 0)
            fd_flags |= O_EXCL;

        fd = open(file_name, fd_flags, 0666);
        if (fd == -1) {
            rrd_set_error("creating '%s': %s", file_name,
                          rrd_strerror(errno));
            return (-1);
        }

        fh = fdopen(fd, "wb");
        if (fh == NULL) {
            rrd_set_error("fdopen failed: %s", rrd_strerror(errno));
            close(fd);
            return (-1);
        }
    }

    int rc = write_fh(fh, rrd);

    /* lets see if we had an error */
    if (ferror(fh)) {
        rrd_set_error("a file error occurred while creating '%s': %s", file_name,
            rrd_strerror(errno));
        fclose(fh);
        if (strcmp("-", file_name) != 0)
            unlink(file_name);
        return (-1);
    }

    fclose(fh);
    
    return rc;
}

int rrd_restore(
    int argc,
    char **argv)
{
    struct optparse_long longopts[] = {
        {"range-check", 'r', OPTPARSE_NONE},
        {"force-overwrite", 'f', OPTPARSE_NONE},
        {0},
    };
    struct    optparse options;
    int       opt;
    rrd_t    *rrd;

    optparse_init(&options, argc, argv);
    while ((opt = optparse_long(&options, longopts, NULL)) != -1) {
        switch (opt) {
        case 'r':
            opt_range_check = 1;
            break;

        case 'f':
            opt_force_overwrite = 1;
            break;

        case '?':
            rrd_set_error("%s", options.errmsg);
            return -1;
        }
    } /* while (opt != -1) */

    if (options.argc - options.optind != 2) {
        rrd_set_error("usage rrdtool %s [--range-check|-r] "
                      "[--force-overwrite|-f] file.xml file.rrd",
                      options.argv[0]);
        return -1;
    }

    rrd = parse_file(options.argv[options.optind]);

    if (rrd == NULL)
        return (-1);
    
    if (write_file(options.argv[options.optind + 1], rrd) != 0) {
        local_rrd_free(rrd);
        return (-1);
    }
    local_rrd_free(rrd);


    return (0);
}                       /* int rrd_restore */

/* vim: set sw=2 sts=2 ts=8 et fdm=marker : */
