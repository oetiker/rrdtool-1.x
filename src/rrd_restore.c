/*****************************************************************************
 * RRDtool 1.3.1  Copyright by Tobi Oetiker, 1997-2008
 * This file:     Copyright 2008 Florian octo Forster
 * Distributed under the GPL
 *****************************************************************************
 * rrd_restore.c   Contains logic to parse XML input and create an RRD file
 *****************************************************************************
 * $Id$
 *************************************************************************** */

/*
 * This program is free software; you can redistribute it and / or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (t your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110 - 1301 USA
 *
 * Authors:
 *   Florian octo Forster <octo at verplant.org>
 **/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)
# include <io.h>
# define open _open
# define close _close
#endif
#include <libxml/parser.h>
#include "rrd_tool.h"
#include "rrd_rpncalc.h"
#define ARRAY_LENGTH(a) (sizeof (a) / sizeof ((a)[0]))
static int opt_range_check = 0;
static int opt_force_overwrite = 0;

/*
 * Auxiliary functions
 */
static int get_string_from_node(
    xmlDoc * doc,
    xmlNode * node,
    char *buffer,
    size_t buffer_size)
{
    xmlChar  *temp0;
    char     *begin_ptr;
    char     *end_ptr;

    temp0 = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
    if (temp0 == NULL) {
        rrd_set_error("get_string_from_node: xmlNodeListGetString failed.");
        return (-1);
    }

    begin_ptr = (char *) temp0;
    while ((begin_ptr[0] != 0) && (isspace(begin_ptr[0])))
        begin_ptr++;

    if (begin_ptr[0] == 0) {
        xmlFree(temp0);
        buffer[0] = 0;
        return (0);
    }

    end_ptr = begin_ptr;
    while ((end_ptr[0] != 0) && (!isspace(end_ptr[0])))
        end_ptr++;
    end_ptr[0] = 0;

    strncpy(buffer, begin_ptr, buffer_size);
    buffer[buffer_size - 1] = 0;

    xmlFree(temp0);

    return (0);
}                       /* int get_string_from_node */

static int get_int_from_node(
    xmlDoc * doc,
    xmlNode * node,
    int *value)
{
    int       temp;
    char     *str_ptr;
    char     *end_ptr;

    str_ptr = (char *) xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
    if (str_ptr == NULL) {
        rrd_set_error("get_int_from_node: xmlNodeListGetString failed.");
        return (-1);
    }

    end_ptr = NULL;
    temp = strtol(str_ptr, &end_ptr, 0);
    xmlFree(str_ptr);

    if (str_ptr == end_ptr) {
        rrd_set_error("get_int_from_node: Cannot parse buffer as int: %s",
                      str_ptr);
        return (-1);
    }

    *value = temp;

    return (0);
}                       /* int get_int_from_node */

static int get_double_from_node(
    xmlDoc * doc,
    xmlNode * node,
    double *value)
{
    double    temp;
    char     *str_ptr;
    char     *end_ptr;

    str_ptr = (char *) xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
    if (str_ptr == NULL) {
        rrd_set_error("get_double_from_node: xmlNodeListGetString failed.");
        return (-1);
    }

    end_ptr = NULL;
    temp = strtod(str_ptr, &end_ptr);
    xmlFree(str_ptr);

    if (str_ptr == end_ptr) {
        rrd_set_error
            ("get_double_from_node: Cannot parse buffer as double: %s",
             str_ptr);
        return (-1);
    }

    *value = temp;

    return (0);
}                       /* int get_double_from_node */

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
}                       /* int value_check_range */

/*
 * Parse the <database> block within an RRA definition
 */
static int parse_tag_rra_database_row(
    xmlDoc * doc,
    xmlNode * node,
    rrd_t *rrd,
    rrd_value_t *rrd_value)
{
    unsigned int values_count = 0;
    xmlNode  *child;
    int       status;

    status = 0;
    for (child = node->xmlChildrenNode; child != NULL; child = child->next) {
        if ((xmlStrcmp(child->name, (const xmlChar *) "comment") == 0)
            || (xmlStrcmp(child->name, (const xmlChar *) "text") == 0))
            /* ignore */ ;
        else if (xmlStrcmp(child->name, (const xmlChar *) "v") == 0) {
            if (values_count < rrd->stat_head->ds_cnt) {
                status =
                    get_double_from_node(doc, child,
                                         rrd_value + values_count);
                if (status == 0)
                    value_check_range(rrd_value + values_count,
                                      rrd->ds_def + values_count);
            }

            values_count++;
        } else {
            rrd_set_error("parse_tag_rra_database_row: Unknown tag: %s",
                          child->name);
            status = -1;
        }

        if (status != 0)
            break;
    }                   /* for (child = node->xmlChildrenNode) */

    if (values_count != rrd->stat_head->ds_cnt) {
        rrd_set_error("parse_tag_rra_database_row: Row has %u values "
                      "and RRD has %lu data sources.",
                      values_count, rrd->stat_head->ds_cnt);
        status = -1;
    }

    return (status);
}                       /* int parse_tag_rra_database_row */

static int parse_tag_rra_database(
    xmlDoc * doc,
    xmlNode * node,
    rrd_t *rrd)
{
    rra_def_t *cur_rra_def;
    unsigned int total_row_cnt;
    xmlNode  *child;
    int       status;
    int       i;

    total_row_cnt = 0;
    for (i = 0; i < (((int) rrd->stat_head->rra_cnt) - 1); i++)
        total_row_cnt += rrd->rra_def[i].row_cnt;

    cur_rra_def = rrd->rra_def + i;

    status = 0;
    for (child = node->xmlChildrenNode; child != NULL; child = child->next) {
        if ((xmlStrcmp(child->name, (const xmlChar *) "comment") == 0)
            || (xmlStrcmp(child->name, (const xmlChar *) "text") == 0))
            /* ignore */ ;
        else if (xmlStrcmp(child->name, (const xmlChar *) "row") == 0) {
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
                parse_tag_rra_database_row(doc, child, rrd, cur_rrd_value);
        } /* if (xmlStrcmp (child->name, (const xmlChar *) "row") == 0) */
        else {
            rrd_set_error("parse_tag_rra_database: Unknown tag: %s",
                          child->name);
            status = -1;
        }

        if (status != 0)
            break;
    }                   /* for (child = node->xmlChildrenNode) */

    return (status);
}                       /* int parse_tag_rra_database */

/*
 * Parse the <cdp_prep> block within an RRA definition
 */
static int parse_tag_rra_cdp_prep_ds_history(
    xmlDoc * doc,
    xmlNode * node,
    cdp_prep_t *cdp_prep)
{
    /* Make `history_buffer' the same size as the scratch area, plus the
     * terminating NULL byte. */
    char      history_buffer[sizeof(((cdp_prep_t *)0)->scratch) + 1];
    char     *history_ptr;
    int       status;
    int       i;

    status = get_string_from_node(doc, node,
                                  history_buffer, sizeof(history_buffer));
    if (status != 0)
        return (-1);

    history_ptr = (char *) (&cdp_prep->scratch[0]);
    for (i = 0; history_buffer[i] != '\0'; i++)
        history_ptr[i] = (history_buffer[i] == '1') ? 1 : 0;

    return (0);
}                       /* int parse_tag_rra_cdp_prep_ds_history */

static int parse_tag_rra_cdp_prep_ds(
    xmlDoc * doc,
    xmlNode * node,
    rrd_t *rrd,
    cdp_prep_t *cdp_prep)
{
    xmlNode  *child;
    int       status;

    memset(cdp_prep, '\0', sizeof(cdp_prep_t));

    status = 0;
    for (child = node->xmlChildrenNode; child != NULL; child = child->next) {
        if (atoi(rrd->stat_head->version) == 1){
                cdp_prep->scratch[CDP_primary_val].u_val = 0.0;
                cdp_prep->scratch[CDP_secondary_val].u_val = 0.0;
        }
        if ((xmlStrcmp(child->name, (const xmlChar *) "comment") == 0)
            || (xmlStrcmp(child->name, (const xmlChar *) "text") == 0))
            /* ignore */ ;
        else if (xmlStrcmp(child->name, (const xmlChar *) "primary_value") ==
                 0)
            status =
                get_double_from_node(doc, child,
                                     &cdp_prep->scratch[CDP_primary_val].
                                     u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "secondary_value")
                 == 0)
            status =
                get_double_from_node(doc, child,
                                     &cdp_prep->scratch[CDP_secondary_val].
                                     u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "intercept") == 0)
            status = get_double_from_node(doc, child,
                                          &cdp_prep->
                                          scratch[CDP_hw_intercept].u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "last_intercept") ==
                 0)
            status =
                get_double_from_node(doc, child,
                                     &cdp_prep->
                                     scratch[CDP_hw_last_intercept].u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "slope") == 0)
            status = get_double_from_node(doc, child,
                                          &cdp_prep->scratch[CDP_hw_slope].
                                          u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "last_slope") == 0)
            status = get_double_from_node(doc, child,
                                          &cdp_prep->
                                          scratch[CDP_hw_last_slope].u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "nan_count") == 0)
            status = get_int_from_node(doc, child,
                                       (int *) &cdp_prep->
                                       scratch[CDP_null_count].u_cnt);
        else if (xmlStrcmp(child->name, (const xmlChar *) "last_nan_count") ==
                 0)
            status =
                get_int_from_node(doc, child,
                                  (int *) &cdp_prep->
                                  scratch[CDP_last_null_count].u_cnt);
        else if (xmlStrcmp(child->name, (const xmlChar *) "seasonal") == 0)
            status = get_double_from_node(doc, child,
                                          &cdp_prep->scratch[CDP_hw_seasonal].
                                          u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "last_seasonal") ==
                 0)
            status =
                get_double_from_node(doc, child,
                                     &cdp_prep->scratch[CDP_hw_last_seasonal].
                                     u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "init_flag") == 0)
            status = get_int_from_node(doc, child,
                                       (int *) &cdp_prep->
                                       scratch[CDP_init_seasonal].u_cnt);
        else if (xmlStrcmp(child->name, (const xmlChar *) "history") == 0)
            status = parse_tag_rra_cdp_prep_ds_history(doc, child, cdp_prep);
        else if (xmlStrcmp(child->name, (const xmlChar *) "value") == 0)
            status = get_double_from_node(doc, child,
                                          &cdp_prep->scratch[CDP_val].u_val);
        else if (xmlStrcmp(child->name,
                           (const xmlChar *) "unknown_datapoints") == 0)
            status = get_int_from_node(doc, child,
                                       (int *) &cdp_prep->
                                       scratch[CDP_unkn_pdp_cnt].u_cnt);
        else {
            rrd_set_error("parse_tag_rra_cdp_prep: Unknown tag: %s",
                          child->name);
            status = -1;
        }

        if (status != 0)
            break;
    }

    return (status);
}                       /* int parse_tag_rra_cdp_prep_ds */

static int parse_tag_rra_cdp_prep(
    xmlDoc * doc,
    xmlNode * node,
    rrd_t *rrd,
    cdp_prep_t *cdp_prep)
{
    xmlNode  *child;
    int       status;

    unsigned int ds_count = 0;

    status = 0;
    for (child = node->xmlChildrenNode; child != NULL; child = child->next) {
        if ((xmlStrcmp(child->name, (const xmlChar *) "comment") == 0)
            || (xmlStrcmp(child->name, (const xmlChar *) "text") == 0))
            /* ignore */ ;
        else if (xmlStrcmp(child->name, (const xmlChar *) "ds") == 0) {
            if (ds_count >= rrd->stat_head->ds_cnt)
                status = -1;
            else {
                status = parse_tag_rra_cdp_prep_ds(doc, child, rrd,
                                                   cdp_prep + ds_count);
                ds_count++;
            }
        } else {
            rrd_set_error("parse_tag_rra_cdp_prep: Unknown tag: %s",
                          child->name);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (ds_count != rrd->stat_head->ds_cnt) {
        rrd_set_error("parse_tag_rra_cdp_prep: There are %i data sources in "
                      "the RRD file, but %i in this cdp_prep block!",
                      (int) rrd->stat_head->ds_cnt, ds_count);
        status = -1;
    }

    return (status);
}                       /* int parse_tag_rra_cdp_prep */

/*
 * Parse the <params> block within an RRA definition
 */
static int parse_tag_rra_params(
    xmlDoc * doc,
    xmlNode * node,
    rra_def_t *rra_def)
{
    xmlNode  *child;
    int       status;

    status = 0;
    for (child = node->xmlChildrenNode; child != NULL; child = child->next) {
        if ((xmlStrcmp(child->name, (const xmlChar *) "comment") == 0)
            || (xmlStrcmp(child->name, (const xmlChar *) "text") == 0))
            /* ignore */ ;
        /*
         * Parameters for CF_HWPREDICT
         */
        else if (xmlStrcmp(child->name, (const xmlChar *) "hw_alpha") == 0)
            status = get_double_from_node(doc, child,
                                          &rra_def->par[RRA_hw_alpha].u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "hw_beta") == 0)
            status = get_double_from_node(doc, child,
                                          &rra_def->par[RRA_hw_beta].u_val);
        else if (xmlStrcmp(child->name,
                           (const xmlChar *) "dependent_rra_idx") == 0)
            status = get_int_from_node(doc, child,
                                       (int *) &rra_def->
                                       par[RRA_dependent_rra_idx].u_cnt);
        /*
         * Parameters for CF_SEASONAL and CF_DEVSEASONAL
         */
        else if (xmlStrcmp(child->name, (const xmlChar *) "seasonal_gamma") ==
                 0)
            status =
                get_double_from_node(doc, child,
                                     &rra_def->par[RRA_seasonal_gamma].u_val);
        else if (xmlStrcmp
                 (child->name, (const xmlChar *) "seasonal_smooth_idx") == 0)
            status =
                get_int_from_node(doc, child,
                                  (int *) &rra_def->
                                  par[RRA_seasonal_smooth_idx].u_cnt);
        else if (xmlStrcmp(child->name, (const xmlChar *) "smoothing_window")
                 == 0)
            status =
                get_double_from_node(doc, child,
                                     &rra_def->
                                     par[RRA_seasonal_smoothing_window].
                                     u_val);
        /* else if (dependent_rra_idx) ...; */
        /*
         * Parameters for CF_FAILURES
         */
        else if (xmlStrcmp(child->name, (const xmlChar *) "delta_pos") == 0)
            status = get_double_from_node(doc, child,
                                          &rra_def->par[RRA_delta_pos].u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "delta_neg") == 0)
            status = get_double_from_node(doc, child,
                                          &rra_def->par[RRA_delta_neg].u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "window_len") == 0)
            status = get_int_from_node(doc, child,
                                       (int *) &rra_def->par[RRA_window_len].
                                       u_cnt);
        else if (xmlStrcmp(child->name, (const xmlChar *) "failure_threshold")
                 == 0)
            status =
                get_int_from_node(doc, child,
                                  (int *) &rra_def->
                                  par[RRA_failure_threshold].u_cnt);
        /*
         * Parameters for CF_AVERAGE, CF_MAXIMUM, CF_MINIMUM, and CF_LAST
         */
        else if (xmlStrcmp(child->name, (const xmlChar *) "xff") == 0)
            status = get_double_from_node(doc, child,
                                          &rra_def->par[RRA_cdp_xff_val].
                                          u_val);
        /*
         * Compatibility code for 1.0.49
         */
        else if (xmlStrcmp(child->name, (const xmlChar *) "value") == 0) {  /* {{{ */
            unsigned int i = 0;

            while (42) {
                if (i >= ARRAY_LENGTH(rra_def->par)) {
                    status = -1;
                    break;
                }

                if ((i == RRA_dependent_rra_idx)
                    || (i == RRA_seasonal_smooth_idx)
                    || (i == RRA_failure_threshold))
                    status = get_int_from_node(doc, child,
                                               (int *) &rra_def->par[i].
                                               u_cnt);
                else
                    status = get_double_from_node(doc, child,
                                                  &rra_def->par[i].u_val);

                if (status != 0)
                    break;

                /* When this loops exits (sucessfully) `child' points to the last
                 * `value' tag in the list. */
                if ((child->next == NULL)
                    || (xmlStrcmp(child->name, (const xmlChar *) "value") !=
                        0))
                    break;

                child = child->next;
                i++;
            }
        } /* }}} */
        else {
            rrd_set_error("parse_tag_rra_params: Unknown tag: %s",
                          child->name);
            status = -1;
        }

        if (status != 0)
            break;
    }

    return (status);
}                       /* int parse_tag_rra_params */

/*
 * Parse an RRA definition
 */
static int parse_tag_rra_cf(
    xmlDoc * doc,
    xmlNode * node,
    rra_def_t *rra_def)
{
    int       status;

    status = get_string_from_node(doc, node,
                                  rra_def->cf_nam, sizeof(rra_def->cf_nam));
    if (status != 0)
        return (-1);

    status = cf_conv(rra_def->cf_nam);
    if (status == -1) {
        rrd_set_error("parse_tag_rra_cf: Unknown consolidation function: %s",
                      rra_def->cf_nam);
        return (-1);
    }

    return (0);
}                       /* int parse_tag_rra_cf */

static int parse_tag_rra(
    xmlDoc * doc,
    xmlNode * node,
    rrd_t *rrd)
{
    xmlNode  *child;
    int       status;

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
    for (child = node->xmlChildrenNode; child != NULL; child = child->next) {
        if ((xmlStrcmp(child->name, (const xmlChar *) "comment") == 0)
            || (xmlStrcmp(child->name, (const xmlChar *) "text") == 0))
            /* ignore */ ;
        else if (xmlStrcmp(child->name, (const xmlChar *) "cf") == 0)
            status = parse_tag_rra_cf(doc, child, cur_rra_def);
        else if (xmlStrcmp(child->name, (const xmlChar *) "pdp_per_row") == 0)
            status = get_int_from_node(doc, child,
                                       (int *) &cur_rra_def->pdp_cnt);
        else if (atoi(rrd->stat_head->version) == 1
                 && xmlStrcmp(child->name, (const xmlChar *) "xff") == 0)
            status = get_double_from_node(doc, child,
                                       (double *) &cur_rra_def->par[RRA_cdp_xff_val].u_val);
        else if (atoi(rrd->stat_head->version) >= 2
                 && xmlStrcmp(child->name, (const xmlChar *) "params") == 0)
            status = parse_tag_rra_params(doc, child, cur_rra_def);
        else if (xmlStrcmp(child->name, (const xmlChar *) "cdp_prep") == 0)
            status = parse_tag_rra_cdp_prep(doc, child, rrd, cur_cdp_prep);
        else if (xmlStrcmp(child->name, (const xmlChar *) "database") == 0)
            status = parse_tag_rra_database(doc, child, rrd);
        else {
            rrd_set_error("parse_tag_rra: Unknown tag: %s", child->name);
            status = -1;
        }

        if (status != 0)
            break;
    }

    /* Set the RRA pointer to a random location */
    cur_rra_ptr->cur_row = random() % cur_rra_def->row_cnt;

    return (status);
}                       /* int parse_tag_rra */

/*
 * Parse a DS definition
 */
static int parse_tag_ds_cdef(
    xmlDoc * doc,
    xmlNode * node,
    rrd_t *rrd)
{
    char      buffer[1024];
    int       status;

    status = get_string_from_node(doc, node, buffer, sizeof(buffer));
    if (status != 0)
        return (-1);

    /* We're always working on the last DS that has been added to the structure
     * when we get here */
    parseCDEF_DS(buffer, rrd, rrd->stat_head->ds_cnt - 1);

    return (0);
}                       /* int parse_tag_ds_cdef */

static int parse_tag_ds_type(
    xmlDoc * doc,
    xmlNode * node,
    ds_def_t *ds_def)
{
    int       status;

    status = get_string_from_node(doc, node,
                                  ds_def->dst, sizeof(ds_def->dst));
    if (status != 0)
        return (-1);

    status = dst_conv(ds_def->dst);
    if (status == -1) {
        rrd_set_error("parse_tag_ds_type: Unknown data source type: %s",
                      ds_def->dst);
        return (-1);
    }

    return (0);
}                       /* int parse_tag_ds_type */

static int parse_tag_ds(
    xmlDoc * doc,
    xmlNode * node,
    rrd_t *rrd)
{
    xmlNode  *child;
    int       status;

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
    for (child = node->xmlChildrenNode; child != NULL; child = child->next) {
        if ((xmlStrcmp(child->name, (const xmlChar *) "comment") == 0)
            || (xmlStrcmp(child->name, (const xmlChar *) "text") == 0))
            /* ignore */ ;
        else if (xmlStrcmp(child->name, (const xmlChar *) "name") == 0)
            status = get_string_from_node(doc, child,
                                          cur_ds_def->ds_nam,
                                          sizeof(cur_ds_def->ds_nam));
        else if (xmlStrcmp(child->name, (const xmlChar *) "type") == 0)
            status = parse_tag_ds_type(doc, child, cur_ds_def);
        else if (xmlStrcmp(child->name,
                           (const xmlChar *) "minimal_heartbeat") == 0)
            status = get_int_from_node(doc, child,
                                       (int *) &cur_ds_def->par[DS_mrhb_cnt].
                                       u_cnt);
        else if (xmlStrcmp(child->name, (const xmlChar *) "min") == 0)
            status = get_double_from_node(doc, child,
                                          &cur_ds_def->par[DS_min_val].u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "max") == 0)
            status = get_double_from_node(doc, child,
                                          &cur_ds_def->par[DS_max_val].u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "cdef") == 0)
            status = parse_tag_ds_cdef(doc, child, rrd);
        else if (xmlStrcmp(child->name, (const xmlChar *) "last_ds") == 0)
            status = get_string_from_node(doc, child,
                                          cur_pdp_prep->last_ds,
                                          sizeof(cur_pdp_prep->last_ds));
        else if (xmlStrcmp(child->name, (const xmlChar *) "value") == 0)
            status = get_double_from_node(doc, child,
                                          &cur_pdp_prep->scratch[PDP_val].
                                          u_val);
        else if (xmlStrcmp(child->name, (const xmlChar *) "unknown_sec") == 0)
            status = get_int_from_node(doc, child,
                                       (int *) &cur_pdp_prep->
                                       scratch[PDP_unkn_sec_cnt].u_cnt);
        else {
            rrd_set_error("parse_tag_ds: Unknown tag: %s", child->name);
            status = -1;
        }

        if (status != 0)
            break;
    }

    return (status);
}                       /* int parse_tag_ds */

/*
 * Parse root nodes
 */
static int parse_tag_rrd(
    xmlDoc * doc,
    xmlNode * node,
    rrd_t *rrd)
{
    xmlNode  *child;
    int       status;

    status = 0;
    for (child = node->xmlChildrenNode; child != NULL; child = child->next) {
        if ((xmlStrcmp(child->name, (const xmlChar *) "comment") == 0)
            || (xmlStrcmp(child->name, (const xmlChar *) "text") == 0))
            /* ignore */ ;
        else if (xmlStrcmp(child->name, (const xmlChar *) "version") == 0)
            status = get_string_from_node(doc, child,
                                          rrd->stat_head->version,
                                          sizeof(rrd->stat_head->version));
        else if (xmlStrcmp(child->name, (const xmlChar *) "step") == 0)
            status = get_int_from_node(doc, child,
                                       (int *) &rrd->stat_head->pdp_step);
        else if (xmlStrcmp(child->name, (const xmlChar *) "lastupdate") == 0)
            status = get_int_from_node(doc, child,
                                       (int *) &rrd->live_head->last_up);
        else if (xmlStrcmp(child->name, (const xmlChar *) "ds") == 0)
            status = parse_tag_ds(doc, child, rrd);
        else if (xmlStrcmp(child->name, (const xmlChar *) "rra") == 0)
            status = parse_tag_rra(doc, child, rrd);
        else {
            rrd_set_error("parse_tag_rrd: Unknown tag: %s", child->name);
            status = -1;
        }

        if (status != 0)
            break;
    }

    return (status);
}                       /* int parse_tag_rrd */

static rrd_t *parse_file(
    const char *filename)
{
    xmlDoc   *doc;
    xmlNode  *cur;
    int       status;

    rrd_t    *rrd;

    doc = xmlParseFile(filename);
    if (doc == NULL) {
        rrd_set_error("Document not parsed successfully.");
        return (NULL);
    }

    cur = xmlDocGetRootElement(doc);
    if (cur == NULL) {
        rrd_set_error("Document is empty.");
        xmlFreeDoc(doc);
        return (NULL);
    }

    if (xmlStrcmp(cur->name, (const xmlChar *) "rrd") != 0) {
        rrd_set_error
            ("Document of the wrong type, root node is not \"rrd\".");
        xmlFreeDoc(doc);
        return (NULL);
    }

    rrd = (rrd_t *) malloc(sizeof(rrd_t));
    if (rrd == NULL) {
        rrd_set_error("parse_file: malloc failed.");
        xmlFreeDoc(doc);
        return (NULL);
    }
    memset(rrd, '\0', sizeof(rrd_t));

    rrd->stat_head = (stat_head_t *) malloc(sizeof(stat_head_t));
    if (rrd->stat_head == NULL) {
        rrd_set_error("parse_tag_rrd: malloc failed.");
        xmlFreeDoc(doc);
        free(rrd);
        return (NULL);
    }
    memset(rrd->stat_head, '\0', sizeof(stat_head_t));

    strncpy(rrd->stat_head->cookie, "RRD", sizeof(rrd->stat_head->cookie));
    rrd->stat_head->float_cookie = FLOAT_COOKIE;

    rrd->live_head = (live_head_t *) malloc(sizeof(live_head_t));
    if (rrd->live_head == NULL) {
        rrd_set_error("parse_tag_rrd: malloc failed.");
        xmlFreeDoc(doc);
        free(rrd->stat_head);
        free(rrd);
        return (NULL);
    }
    memset(rrd->live_head, '\0', sizeof(live_head_t));

    status = parse_tag_rrd(doc, cur, rrd);

    xmlFreeDoc(doc);
    if (status != 0) {
        rrd_free(rrd);
        rrd = NULL;
    }

    return (rrd);
}                       /* rrd_t *parse_file */

static int write_file(
    const char *file_name,
    rrd_t *rrd)
{
    FILE     *fh;
    unsigned int i;
    unsigned int rra_offset;

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
    if (atoi(rrd->stat_head->version) < 3){
        /* we output 3 or higher */
        strcpy(rrd->stat_head->version,"0003");        
    }
    fwrite(rrd->stat_head, sizeof(stat_head_t), 1, fh);
    fwrite(rrd->ds_def, sizeof(ds_def_t), rrd->stat_head->ds_cnt, fh);
    fwrite(rrd->rra_def, sizeof(rra_def_t), rrd->stat_head->rra_cnt, fh);
    fwrite(rrd->live_head, sizeof(live_head_t), 1, fh);
    fwrite(rrd->pdp_prep, sizeof(pdp_prep_t), rrd->stat_head->ds_cnt, fh);
    fwrite(rrd->cdp_prep, sizeof(cdp_prep_t),
           rrd->stat_head->rra_cnt * rrd->stat_head->ds_cnt, fh);
    fwrite(rrd->rra_ptr, sizeof(rra_ptr_t), rrd->stat_head->rra_cnt, fh);

    /* calculate the number of rrd_values to dump */
    rra_offset=0;
    for(i=0; i <  rrd->stat_head->rra_cnt; i++)
    {
        unsigned long num_rows = rrd->rra_def[i].row_cnt;
        unsigned long cur_row = rrd->rra_ptr[i].cur_row;
        unsigned long ds_cnt = rrd->stat_head->ds_cnt;

        fwrite(rrd->rrd_value + (rra_offset + num_rows-1 - cur_row) * ds_cnt,
               sizeof(rrd_value_t), (cur_row+1)*ds_cnt, fh);

        fwrite(rrd->rrd_value + rra_offset * ds_cnt,
               sizeof(rrd_value_t), (num_rows-1 - cur_row)*ds_cnt, fh);

        rra_offset += num_rows;
    }

    /* lets see if we had an error */
    if (ferror(fh)) {
        rrd_set_error("a file error occurred while creating '%s'", file_name);
        fclose(fh);
        return (-1);
    }

    fclose(fh);
    return (0);
}                       /* int write_file */

int rrd_restore(
    int argc,
    char **argv)
{
    rrd_t    *rrd;

    srandom((unsigned int)time(NULL) + (unsigned int)getpid());
    /* init rrd clean */
    optind = 0;
    opterr = 0;         /* initialize getopt */
    while (42) {
        int       opt;
        int       option_index = 0;
        static struct option long_options[] = {
            {"range-check", no_argument, 0, 'r'},
            {"force-overwrite", no_argument, 0, 'f'},
            {0, 0, 0, 0}
        };

        opt = getopt_long(argc, argv, "rf", long_options, &option_index);

        if (opt == EOF)
            break;

        switch (opt) {
        case 'r':
            opt_range_check = 1;
            break;

        case 'f':
            opt_force_overwrite = 1;
            break;

        default:
            rrd_set_error("usage rrdtool %s [--range-check|-r] "
                          "[--force-overwrite/-f]  file.xml file.rrd",
                          argv[0]);
            return (-1);
            break;
        }
    }                   /* while (42) */

    if ((argc - optind) != 2) {
        rrd_set_error("usage rrdtool %s [--range-check/-r] "
                      "[--force-overwrite/-f] file.xml file.rrd", argv[0]);
        return (-1);
    }

    rrd = parse_file(argv[optind]);
    if (rrd == NULL)
        return (-1);

    if (write_file(argv[optind + 1], rrd) != 0) {
        rrd_free(rrd);
        return (-1);
    }

    rrd_free(rrd);
    return (0);
}                       /* int rrd_restore */

/* vim: set sw=2 sts=2 ts=8 et fdm=marker : */
