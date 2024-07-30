/*****************************************************************************
 * RRDtool 1.9.0 Copyright by Tobi Oetiker, 1997-2024
 *****************************************************************************
 * rrd_modify  Structurally modify an RRD file
 *      (c) 2014 by Peter Stamfest and Tobi Oetiker
 *****************************************************************************
 * Initially based on rrd_dump.c
 *****************************************************************************/
#include "rrd_tool.h"
#include "rrd_rpncalc.h"
#include "rrd_client.h"
#include "rrd_restore.h"   /* write_file */
#include "rrd_create.h"    /* parseDS */
#include "rrd_update.h"    /* update_cdp */
#include "rrd_modify.h"
#include "unused.h"

#include "fnv.h"

#include <locale.h>
#include "rrd_config.h"
#ifdef _WIN32
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif



// prototypes
static int add_rras(const rrd_t *in, rrd_t *out, const int *ds_map,
		    const rra_mod_op_t *rra_mod_ops, int rra_mod_ops_cnt, unsigned long hash);

/* a convenience realloc/memcpy combo  */
static void * copy_over_realloc(void *dest, int dest_index, 
				const void *src, int src_index,
				ssize_t size) {
    void *r = realloc(dest, size * (dest_index + 1));
    if (r == NULL) {
        rrd_set_error("copy_over_realloc: realloc failed.");
	return r;
    }

    memcpy(((char*)r) + size * dest_index, ((char*)src) + size * src_index, size);
    return r;
}


/* 
   Try to populate rows (presumably for added rows) in new_rra from
   available data in rrd. This only works for some CF types and
   generally is wildly inaccurate - eg. it does not take the xff factor
   into account. Do not think of it as producing correct data but
   rather as a way to produce nice pictures for subsequent rrdgraph
   invocations...

   NOTE: rrd and new_rra may point to entirely different RRAs.
*/


static int sort_candidates(const void *va, const void *vb) {
    const candidate_t *a = (candidate_t *) va;
    const candidate_t *b = (candidate_t *) vb;

    if (a == b) return 0;
    
    if (a->rrd == b->rrd && a->rra_index == b->rra_index) return 0;
    
    rra_def_t *a_def = a->rrd->rra_def + a->rra_index;
    rra_def_t *b_def = b->rrd->rra_def + b->rra_index;

    if (a_def->pdp_cnt == b_def->pdp_cnt) {
	return b_def->row_cnt - a_def->row_cnt;  // prefer the RRA with more rows
    }

    // ultimately, prefer the RRA with fewer PDPs per CDP
    return a_def->pdp_cnt - b_def->pdp_cnt;
}

static int select_for_modify(const rra_def_t *tofill, const rra_def_t *maybe) {
    enum cf_en cf = rrd_cf_conv(tofill->cf_nam);
    enum cf_en other_cf = rrd_cf_conv(maybe->cf_nam);
    return (other_cf == cf ||
	    (other_cf == CF_AVERAGE /*&& other_rra->pdp_cnt == 1*/));
}

candidate_t *find_candidate_rras(const rrd_t *rrd, const rra_def_t *rra, int *cnt, 
                                 candidate_extra_t extra, 
                                 int (*selectfunc)(const rra_def_t *tofill, const rra_def_t *maybe))
{
    int total_rows = 0;
    candidate_t *candidates = NULL;
    *cnt = 0;

    int i;

    /* find other RRAs with the same CF or an RRA with CF_AVERAGE and
       a stepping of 1 as possible candidates for filling */
    for (i = 0 ; i < (int) rrd->stat_head->rra_cnt ; i++) {
	rra_def_t *other_rra = rrd->rra_def + i;

	// can't use our own data
	if (other_rra == rra) {
	    continue;
	}

	if (selectfunc(rra, other_rra)) {
#ifdef _WINDOWS
            candidate_t c;
            c.rrd = rrd;
            c.rra_index = i;
            c.values = rrd->rrd_value + rrd->stat_head->ds_cnt * total_rows;
            c.rra = rrd->rra_def + i;
            c.ptr = rrd->rra_ptr + i;
            c.cdp = rrd->cdp_prep + rrd->stat_head->ds_cnt * i;
            memcpy(&c.extra, &extra, sizeof(extra));
#else
            const candidate_t c = { 
		.rrd = (rrd_t*) rrd,    /* cast effectively removes const-ness, but we won't
                                         * mess around with it. promised */
		.rra_index = i,
		.values = rrd->rrd_value + rrd->stat_head->ds_cnt * total_rows,
		.rra = rrd->rra_def + i,
		.rra_cf = rrd_cf_conv(rrd->rra_def[i].cf_nam),
		.ptr = rrd->rra_ptr + i,
		.cdp = rrd->cdp_prep + rrd->stat_head->ds_cnt * i,
		.extra = extra
	    };
#endif
            candidates = (candidate_t *) copy_over_realloc(candidates, *cnt,
					   &c, 0, sizeof(c));
	    if (candidates == NULL) {
		rrd_set_error("out of memory");
		*cnt = 0;
		return NULL;
	    }
	    (*cnt)++;
#ifdef MODIFY_DEBUG
	    fprintf(stderr, "candidate: index=%d pdp=%d\n", i, in_rrd->rra_def[i].pdp_cnt);
#endif
	}
	total_rows += other_rra->row_cnt;
    }

    if (*cnt == 0) {
	return NULL;
    }

    // now sort candidates by granularity
    qsort(candidates, *cnt, sizeof(candidate_t), sort_candidates);

    return candidates;
}


/* copy over existing DS definitions (and related data
   structures), check on the way (and skip) if they should be
   deleted
   */
static int copy_or_delete_DSs(const rrd_t *in, rrd_t *out, char *ops) {
    int rc = -1;
    
    for (unsigned int in_ds = 0 ; in_ds < in->stat_head->ds_cnt ; in_ds++) {
	switch (ops[in_ds]) {
	case 'c': {
	    out->ds_def = (ds_def_t *) copy_over_realloc(out->ds_def, out->stat_head->ds_cnt, 
					   in->ds_def, in_ds,
					   sizeof(ds_def_t));
	    if (out->ds_def == NULL) goto done;
	    
		out->pdp_prep = (pdp_prep_t *) copy_over_realloc(out->pdp_prep, out->stat_head->ds_cnt,
					     in->pdp_prep, in_ds,
					     sizeof(pdp_prep_t));
	    if (out->pdp_prep == NULL) goto done;

	    out->stat_head->ds_cnt++;
	    break;
	}
	case 'd':
	    break;
	case 'a':
	default:
	    rrd_set_error("internal error: invalid ops");
	    goto done;
	}
    }
    // only if we did all iterations without any problems will we arrive here
    rc = 0;
done:
    return rc;
}

/*
  Handle all RRAs definitions (and associated CDP data structures), taking care
  of RRA removals, and RRA row additions and removals. NOTE: data copying is
  NOT done by this function, but it DOES calculate overall total_row
  information needed for sizing the data area.
  
  returns the total number out RRA rows for both the in and out RRDs in the
  variables pointed to by total_in_rra_rows and total_out_rra_rows respectively
  */
static int handle_rra_defs(const rrd_t *in, rrd_t *out, 
			   rra_mod_op_t *rra_mod_ops, unsigned int rra_mod_ops_cnt,
			   const char *ds_ops, unsigned int ds_ops_cnt,
			   int *total_in_rra_rows, int *total_out_rra_rows)
{
    int rc = -1;
    unsigned int j, r;
	rra_ptr_t rra_0_ptr; rra_0_ptr.cur_row = 0;
    cdp_prep_t empty_cdp_prep;
    memset(&empty_cdp_prep, 0, sizeof(empty_cdp_prep));

    for (j = 0 ; j < in->stat_head->rra_cnt ; j++) {
	if (total_in_rra_rows) 
	    *total_in_rra_rows +=  in->rra_def[j].row_cnt;

	rra_mod_op_t *rra_op = NULL;
	for (r = 0 ; r < rra_mod_ops_cnt ; r++) {
	    if (rra_mod_ops[r].index == (int) j) {
		rra_op = rra_mod_ops + r;
		break;
	    }
	}

	int final_row_count = in->rra_def[j].row_cnt;
	if (rra_op) {
	    switch (rra_op->op) {
	    case '=':
		final_row_count = rra_op->row_count;
		break;
	    case '-':
		final_row_count -= rra_op->row_count;
		break;
	    case '+':
		final_row_count += rra_op->row_count;
		break;
	    }
	    if (final_row_count < 0) final_row_count = 0;
	    /* record the final row_count. I don't like this, because
	       it changes the data passed to us via an argument: */

	    rra_op->final_row_count = final_row_count;
	}

	// do we have to keep the RRA at all??
	if (final_row_count == 0) {
	    // delete the RRA! - just skip processing this RRA....
	    continue;
	}

	out->cdp_prep = (cdp_prep_t *) realloc(out->cdp_prep,
			       sizeof(cdp_prep_t) * out->stat_head->ds_cnt 
			       * (out->stat_head->rra_cnt + 1));
	
	if (out->cdp_prep == NULL) {
	    rrd_set_error("Cannot allocate memory");
	    goto done;
	}

	/* for every RRA copy only those CDPs in the prep area where we keep 
	   the DS! */

	int start_index_in  = in->stat_head->ds_cnt * j;
	int start_index_out = out->stat_head->ds_cnt * out->stat_head->rra_cnt;
	
	out->rra_def = (rra_def_t *) copy_over_realloc(out->rra_def, out->stat_head->rra_cnt,
					 in->rra_def, j,
					 sizeof(rra_def_t));
	if (out->rra_def == NULL) goto done;

	// adapt row count:
	out->rra_def[out->stat_head->rra_cnt].row_cnt = final_row_count;

	out->rra_ptr = (rra_ptr_t *) copy_over_realloc(out->rra_ptr, out->stat_head->rra_cnt,
					&rra_0_ptr, 0,
					sizeof(rra_ptr_t));
	if (out->rra_ptr == NULL) goto done; 

	out->rra_ptr[out->stat_head->rra_cnt].cur_row = final_row_count - 1;

	unsigned int i, ii;
	for (i = ii = 0 ; i < ds_ops_cnt ; i++) {
	    switch (ds_ops[i]) {
	    case 'c': {
		memcpy(out->cdp_prep + start_index_out + ii,
		       in->cdp_prep + start_index_in + i, 
		       sizeof(cdp_prep_t));
		ii++;
		break;
	    } 
	    case 'a': {
		cdp_prep_t *cdp_prep = out->cdp_prep + start_index_out + ii;
		memcpy(cdp_prep,
		       &empty_cdp_prep, sizeof(cdp_prep_t));

		init_cdp(out, 
			 out->rra_def + out->stat_head->rra_cnt,
                         out->pdp_prep + ii,
			 cdp_prep);
		ii++;
		break;
	    }
	    case 'd':
		break;
	    default:
		rrd_set_error("internal error: invalid ops");
		goto done;
	    }
	}
	
	if (total_out_rra_rows)
	    *total_out_rra_rows += out->rra_def[out->stat_head->rra_cnt].row_cnt;

	out->stat_head->rra_cnt++;
    }
    rc = 0;
done:
    return rc;
}


/* add datasources as specified in the addDS array of DS specs as documented
    in rrdcreate(1) 

    Returns the number of DSs added or -1 on error
*/ 

static int add_dss(const rrd_t UNUSED(*in), rrd_t *out, 
		   const char **addDS)   
{
    if (addDS == NULL) return 0;
	  
    int added_count = 0;
    int rc = -1;
    int j;
    const char *c;
    const char *require_version = NULL;

    for (j = 0, c = addDS[j] ; c ; j++, c = addDS[j]) {
	ds_def_t added;

	// parse DS
	parseDS(c + 3,
		&added, // out.ds_def + out.stat_head->ds_cnt,
		out, lookup_DS, NULL, &require_version);

	// check if there is a name clash with an existing DS
	if (lookup_DS(out, added.ds_nam) >= 0) {
	    rrd_set_error("Duplicate DS name: %s", added.ds_nam);
	    goto done;
	}

	// copy parse result to output RRD
	out->ds_def = (ds_def_t *) copy_over_realloc(out->ds_def, out->stat_head->ds_cnt,
					&added, 0,
					sizeof(ds_def_t));
	if (out->ds_def ==  NULL) {
	    goto done;
	}

	// also add a pdp_prep_t
	pdp_prep_t added_pdp_prep;
	memset(&added_pdp_prep, 0, sizeof(added_pdp_prep));
	strcpy(added_pdp_prep.last_ds, "U");

	added_pdp_prep.scratch[PDP_val].u_val = 0.0;
	added_pdp_prep.scratch[PDP_unkn_sec_cnt].u_cnt =
	    out->live_head->last_up % out->stat_head->pdp_step;

	out->pdp_prep = (pdp_prep_t *) copy_over_realloc(out->pdp_prep,
					  out->stat_head->ds_cnt, 
					  &added_pdp_prep, 0,
					  sizeof(pdp_prep_t));
	if (out->pdp_prep == NULL) {
	    goto done;
	}
	out->stat_head->ds_cnt++;

	added_count++;
    }
    rc = added_count;
done:
    return rc;
}


/*
  Populate (presumably just added) rows of an RRA from available
  data. Currently only basic CF types are supported.

  in_rrd .. the RRD to use for the search of other RRAs to populate the new RRA
  out_rrd .. the RRD new_rra is part of
  ds_map .. maps the DS indices from the ones used in new_rra to the ones used in 
            rrd. If NULL, an identity mapping is used. This is needed to support
            DS addition/removal from the rrd to new_rra.
  new_rra .. the RRA to populate. It is assumed, that this RRA will
             become part of rrd. This means that all meta settings (step size, 
	     last update time, etc.) not part of the RRA definition can be taken
	     from rrd.
  cur_row .. The cur_row value for new_rra. This is not kept with the def, so it
             has to be passed separately
  values .. Pointer to the 0-index row of the RRA
  populate_start .. the first row to populate in new_rra
  populate_cnt .. the number of rows to populate in new_rra, starting at
                  populate_start
 */
static int populate_row(const rrd_t *in_rrd, 
			const rrd_t *out_rrd,
			const int *ds_map,
			rra_def_t *new_rra, 
			int cur_row,
			rrd_value_t *values,
			int populate_start,
			int populate_cnt) 
{
    int rc = -1;

    if (in_rrd->stat_head->rra_cnt < 1) return 0;

    enum cf_en cf = rrd_cf_conv(new_rra->cf_nam);
    switch (cf) {
    case CF_AVERAGE:
    case CF_MINIMUM:
    case CF_MAXIMUM:
    case CF_LAST:
	break;
    default: // unsupported CF for extension
	return 0;
    }

    int ds_cnt = in_rrd->stat_head->ds_cnt;

    candidate_t *candidates = NULL;
    int candidates_cnt = 0;

    int i, ri;
    candidate_extra_t junk;
    junk.l = 0; /* Initialize junk.l to avoid warning C4700 in MSVC */

    candidates = find_candidate_rras(in_rrd, new_rra, &candidates_cnt, junk, select_for_modify);
    if (candidates == NULL) {
	goto done;
    }

    /* some of the code below is based on
       https://github.com/ssinyagin/perl-rrd-tweak/blob/master/lib/RRD/Tweak.pm#L1455
    */

    for (ri = 0 ; ri < populate_cnt ; ri++) {
	int row = populate_start + ri;

	time_t new_timeslot = new_rra->pdp_cnt * out_rrd->stat_head->pdp_step;

	time_t row_end_time = end_time_for_row(out_rrd, new_rra, cur_row, row);
	time_t row_start_time   = row_end_time - new_timeslot + 1;

	/* now walk all candidates */

	for (i = 0 ; i < candidates_cnt ; i++) {
	    candidate_t *c = candidates + i;
	    rra_def_t *r = c->rrd->rra_def + c->rra_index;
	    int cand_cur_row = c->rrd->rra_ptr[c->rra_index].cur_row;

	    /* find a matching range of rows */
	    int cand_row_start = row_for_time(in_rrd, r, cand_cur_row, row_start_time);
	    int cand_row_end   = row_for_time(in_rrd, r, cand_cur_row, row_end_time);

	    if (cand_row_start == -1 && cand_row_end != -1) {
		// start time is beyond last_up */
		cand_row_start = cand_cur_row;
	    } else if (cand_row_start != -1 && cand_row_end == -1) {
		// maybe the candidate has fewer rows than the pdp_step ....
		cand_row_end = (cand_cur_row - 1) % r->row_cnt;
	    } else if (cand_row_start == -1 && cand_row_end == -1) {
		// neither start nor end in range. Can't use this candidate RRA...
		continue;
	    }

	    /* note: cand_row_end is usually after cand_row_start,
	       unless we have a wrap over.... so we turn the
	       iteration over the rows into one based on the number
	       of rows starting at cand_row_end. All this dance should
	       be in preparation for unusual cases where we have
	       candidates and new RRAs that have pdp counts that are
	       not directly divisible by each other (like populating a
	       2-pdp RRA from a 3-pdp RRA) */
	    
	    int cand_rows = (cand_row_end - cand_row_start + 1);
	    if (cand_rows < 0) cand_rows += r->row_cnt;

#ifdef MODIFY_DEBUG
	    fprintf(stderr, "cand: start=%d end=%d rows=%d\n",
		    cand_row_start, cand_row_end, cand_rows); 
#endif

	    int cand_timeslot = r->pdp_cnt * c->rrd->stat_head->pdp_step;

	    int out_ds_cnt = out_rrd->stat_head->ds_cnt;
	    for (int k = 0 ; k < out_ds_cnt ; k++) {
		/* check if we already have a value (maybe from a
		   prior (=better!) candidate....)  if we have: skip this DS */
		if (! isnan(values[row * out_ds_cnt + k])) {
		    continue;
		}

		int cand_row, ci ;
		rrd_value_t tmp = DNAN, final = DNAN;
		int covered = 0;

		int in_k = ds_map ? ds_map[k] : k;

		// if the DS was just added we have no pre-existing data anyway, so skip
		if (in_k < 0) continue;
		
		/* Go: Use the range of candidate rows to populate this DS in this row */
		for (cand_row = cand_row_start, ci = 0 ; 
		     ci < cand_rows ; 
		     ci++, cand_row = (cand_row + 1) % r->row_cnt)
		    {
		    rrd_value_t v = c->values[cand_row * ds_cnt + in_k];
		
		    /* skipping NAN values. Note that if all candidate
		       rows are filled with NAN values, a later
		       candidate RRA might be used instead. This works
		       in combination with the isnan check above */
		    if (isnan(v)) continue;

		    switch (cf) {
		    case CF_AVERAGE:
			tmp = isnan(tmp) ? v * cand_timeslot : (tmp + v * cand_timeslot);
			covered += cand_timeslot;
			final = tmp / covered;
			break;
		    case CF_MINIMUM:
			final = tmp = isnan(tmp) ? v : (tmp < v ? tmp : v);
			break;
		    case CF_MAXIMUM:
			final = tmp = isnan(tmp) ? v : (tmp > v ? tmp : v);
			break;
		    case CF_LAST:
			final = tmp = v;
			break;
		    default: // unsupported CF for extension
			return 0;
		    }
		}

		values[row * out_ds_cnt + k] = final;
	    }
	}
    }

    rc = 0;

 done:
    if (candidates) {
	free(candidates);
    }

    return rc;
}

static int mod_rras(const rrd_t *in, rrd_t *out, const int *ds_map,
		    const rra_mod_op_t *rra_mod_ops, unsigned int rra_mod_ops_cnt,
		    const char *ds_ops, unsigned int ds_ops_cnt) 
{
    int rc = -1;
    unsigned int rra_index, j;
    int total_cnt = 0, total_cnt_out = 0;
    int out_rra = 0;	    // index of currently copied RRA
    
    for (rra_index = 0; rra_index < in->stat_head->rra_cnt; rra_index++) {
	const rra_mod_op_t *rra_op = NULL;
	for (unsigned int op_index = 0 ; op_index < rra_mod_ops_cnt ; op_index++) {
	    if (rra_mod_ops[op_index].index == (int)rra_index) {
		rra_op = rra_mod_ops + op_index;
		break;
	    }
	}

	if (rra_op && rra_op->final_row_count == 0) {
	    // RRA deleted - skip !
	    continue;
	}

	/* number and sizes of all the data in an RRA */
	int rra_values     = in->stat_head->ds_cnt  * in->rra_def[rra_index].row_cnt;
	int rra_values_out = out->stat_head->ds_cnt * out->rra_def[out_rra].row_cnt;

	/* we now have all the data for the current RRA available, now
	   start to transfer it to the output RRD: For every row copy 
	   the data corresponding to copied DSes, add NaN values for newly 
	   added DSes. */

	unsigned int ii = 0, jj, oi = 0;

	/* we have to decide beforehand about row addition and
	   deletion, because this takes place in the front of the
	   rrd_value array....
	 */

	if (rra_op) {
	    char op = rra_op->op;
	    unsigned int row_count = rra_op->row_count;
	    
	    // rewrite '=' ops into '-' or '+' for better code-reuse...
	    if (op == '=') {
		if (row_count < in->rra_def[rra_index].row_cnt) {
		    row_count = in->rra_def[rra_index].row_cnt - row_count;
		    op = '-';
		} else if (row_count > in->rra_def[rra_index].row_cnt) {
		    row_count = row_count - in->rra_def[rra_index].row_cnt;
		    op = '+';
		} else {
		    // equal - special case: nothing to do...
		}
	    }

	    switch (op) {
	    case '=':
		// no op
		break;
	    case '-':
		// remove rows: just skip the first couple of rows!
		ii = row_count;
		break;
	    case '+':
		// add rows: insert the requested number of rows!
		// currently, just add the all as NaN values...

		for ( ; oi < row_count ; oi++) {
		    for (j = 0 ; j < out->stat_head->ds_cnt ; j++) {
			out->rrd_value[total_cnt_out + 
				       oi * out->stat_head->ds_cnt +
				       j] = DNAN;
		    }		
		}

		// now try to populate the newly added rows 
		populate_row(in, out, ds_map,
			     out->rra_def + out_rra, 
			     out->rra_ptr[rra_index].cur_row,
			     out->rrd_value + total_cnt_out,
			     0, row_count);

		break;
	    default:
		rrd_set_error("RRA modification operation '%c' "
			      "not (yet) supported", rra_op->op);
		goto done;
	    }
	}

	/* now do the actual copying of data */

	for ( ; ii < in->rra_def[rra_index].row_cnt 
		  && oi < out->rra_def[out_rra].row_cnt ; ii++, oi++) {
	    int real_ii = (ii + in->rra_ptr[rra_index].cur_row + 1) % in->rra_def[rra_index].row_cnt;
	    for (j = jj = 0 ; j < ds_ops_cnt ; j++) {
		switch (ds_ops[j]) {
		case 'c': {
		    out->rrd_value[total_cnt_out + oi * out->stat_head->ds_cnt + jj] =
			in->rrd_value[total_cnt + real_ii * in->stat_head->ds_cnt + j];

		    /* it might be better to use memcpy, actually (to
		       treat them opaquely)... so keep the code for
		       the time being */
		    /*
		    memcpy((void*) (out.rrd_value + total_cnt_out + oi * out.stat_head->ds_cnt + jj),
			   (void*) (in.rrd_value + total_cnt + real_ii * in.stat_head->ds_cnt + j), sizeof(rrd_value_t));
		    */			   
		    jj++;
		    break;
		}
		case 'a': {
		    out->rrd_value[total_cnt_out + oi * out->stat_head->ds_cnt + jj] = DNAN;
		    jj++;
		    break;
		}
		case 'd':
		    break;
		default:
		    rrd_set_error("internal error: invalid ops");
		    goto done;
		}
	    }
	}

	total_cnt     += rra_values;
	total_cnt_out += rra_values_out;

	out_rra++;
    }
    rc = 0;
done:
    return rc;
}

static int stretch_rras(rrd_t *out, int stretch) {
    int rc = -1;
    if (stretch < 2) {
	rrd_set_error("invalid stretch count. Must be > 1");
	goto done;
    }
    
    unsigned int ds_cnt = out->stat_head->ds_cnt;
    unsigned int rra_index, ds_index;
    for (rra_index = 0 ; rra_index < out->stat_head->rra_cnt ; rra_index++) {
	rra_def_t *rra = out->rra_def + rra_index;
	enum cf_en cf = rrd_cf_conv(rra->cf_nam);
	
	cdp_prep_t *cdp_prep_row = out->cdp_prep + rra_index * ds_cnt;
	for (ds_index = 0 ; ds_index < ds_cnt ; ds_index++) {
	    switch (cf) {
	    case CF_AVERAGE:
	    case CF_MINIMUM:
	    case CF_MAXIMUM:
	    case CF_LAST:
		(cdp_prep_row + ds_index)->scratch[CDP_unkn_pdp_cnt].u_val *= stretch;
		break;
	    default:
		break;
	    }
	}
	
	rra->pdp_cnt *= stretch;
    }
    
    out->stat_head->pdp_step /= stretch;
    
    rc =0;
done:
    return rc;
}

static void rrd_memory_free(rrd_t *rrd)
{
    if (rrd == NULL) return;
    if (rrd->live_head) free(rrd->live_head);
    if (rrd->stat_head) free(rrd->stat_head);
    if (rrd->ds_def) free(rrd->ds_def);
    if (rrd->rra_def) free(rrd->rra_def);
    if (rrd->rra_ptr) free(rrd->rra_ptr);
    if (rrd->pdp_prep) free(rrd->pdp_prep);
    if (rrd->cdp_prep) free(rrd->cdp_prep);
    if (rrd->rrd_value) free(rrd->rrd_value);
}

static rrd_t *rrd_modify_structure(const rrd_t *in,
				   const char **removeDS,
				   const char **addDS,
				   rra_mod_op_t *rra_mod_ops, int rra_mod_ops_cnt,
				   unsigned long hash)
{
    rrd_t *out;
    int rc = -1;
    unsigned int i, j;
    char       *ds_ops = NULL;
    unsigned int ds_ops_cnt = 0;
    int *ds_map = NULL;
    
	out = (rrd_t *) malloc(sizeof(rrd_t));
    if (out == NULL) {
	rrd_set_error("Out of memory");
	goto done;
    }
    rrd_init(out);
    
    /* currently we only allow to modify version 3 RRDs. If other
       files should be modified, a dump/restore cycle should be
       done.... */
    
    if (atoi(in->stat_head->version) < atoi(RRD_VERSION3) || atoi(in->stat_head->version) > atoi(RRD_VERSION5)) {
	rrd_set_error("direct modification is only supported for version 3, 4 or 5 of RRD files. Consider to dump/restore before retrying a modification");
	goto done;
    }
    
    
    /* copy over structure to out RRD */
    
    out->stat_head = (stat_head_t *) malloc(sizeof(stat_head_t));
    if (out->stat_head == NULL) {
	rrd_set_error("rrd_modify_r: malloc failed.");
	goto done;
    }
    
    memset(out->stat_head, 0, (sizeof(stat_head_t)));
    
    strncpy(out->stat_head->cookie, "RRD", sizeof(out->stat_head->cookie));
    strcpy(out->stat_head->version, in->stat_head->version);
    out->stat_head->float_cookie = FLOAT_COOKIE;
    out->stat_head->pdp_step = in->stat_head->pdp_step;
    
    out->stat_head->ds_cnt = 0;
    out->stat_head->rra_cnt = 0;
    
	out->live_head = (live_head_t *) copy_over_realloc(out->live_head, 0, in->live_head, 0,
				       sizeof(live_head_t));
    
    if (out->live_head == NULL) goto done;
    
    /* use the ops array as a scratchpad to remember what we are about
    to do to each DS. There is one entry for every DS in the
    original RRD and one additional entry for every added DS. 

    Entries marked as 
    - 'c' will be copied to the out RRD, 
    - 'd' will not be copied (= will effectively be deleted)
    - 'a' will be added.
    */
    ds_ops_cnt = in->stat_head->ds_cnt;
    ds_ops = (char *) malloc(ds_ops_cnt);
    
    if (ds_ops == NULL) {
	rrd_set_error("parse_tag_rrd: malloc failed.");
	goto done;
    } 
    
    memset(ds_ops, 'c', in->stat_head->ds_cnt);
    
    // record DSs to be deleted in ds_ops
    if (removeDS != NULL) {
	for (unsigned int in_ds = 0 ; in_ds < in->stat_head->ds_cnt ; in_ds++) {
	    const char *c;
	    for (j = 0, c = removeDS[j] ; c ; j++, c = removeDS[j]) {
		if (strcmp(in->ds_def[in_ds].ds_nam, c) == 0) {
		    ds_ops[in_ds] = 'd';
		    break;
		}
	    }
	}
    }
    
    if (copy_or_delete_DSs(in, out, ds_ops) != 0) {
	// error
	goto done;
    }
    
    /* now add any DS definitions to be added */
    int added_cnt = add_dss(in, out, addDS);
    if (added_cnt < 0) {
	// error
	goto done;
    }
    if (added_cnt > 0) {
	// and extend the ds_ops array as well
		ds_ops = (char *) realloc(ds_ops, ds_ops_cnt + added_cnt);
	for(; added_cnt > 0 ; added_cnt--) {
	    ds_ops[ds_ops_cnt++] = 'a';
	}
    }
    
    /* prepare explicit data source index to map from output index to
       input index */
    
    ds_map = (int *) malloc(sizeof(int) * out->stat_head->ds_cnt);
    
    j = 0;
    for (i = 0 ; i < ds_ops_cnt ; i++) {
	switch (ds_ops[i]) {
	case 'c': 
	    ds_map[j++] = i;
	    break;
	case 'd': 
	    break;
	case 'a':
	    ds_map[j++] = -1;
	    break;
	}
    }
    
    /* now take care to copy all RRAs, removing and adding columns for
       every row as needed for the requested DS changes */
    
    /* we also reorder all rows, adding/removing rows as needed */
    
    /* later on, we'll need to know the total number of rows for both RRDs in
       order to allocate memory. Luckily, handle_rra_defs will give that to us. */
    int total_out_rra_rows = 0, total_in_rra_rows = 0;
    
    rc = handle_rra_defs(in, out, rra_mod_ops, rra_mod_ops_cnt, ds_ops, ds_ops_cnt, &total_in_rra_rows, &total_out_rra_rows);
    if (rc != 0) goto done;
    
    /* read and process all data ... */
    
    /* there seem to be two function in the current rrdtool codebase dealing
       with writing a new rrd file to disk: write_file and rrd_create_fn. The
       latter has the major problem, that it tries to free data passed to it
       (WTF?), but write_file assumes chronologically ordered data in RRAs (that
       is, in the data space starting at rrd.rrd_value....

       This is the reason why: 
        - we use write_file and 
        - why we reset cur_row in RRAs and reorder data to be chronological
    */
    
    /* prepare space for output data */
	out->rrd_value = (rrd_value_t *) realloc(out->rrd_value,
			     total_out_rra_rows * out->stat_head->ds_cnt
			     * sizeof(rrd_value_t));
    
    if (out->rrd_value == NULL) {
	rrd_set_error("out of memory");
	goto done;
    }
    
    rc = mod_rras(in, out, ds_map, rra_mod_ops, rra_mod_ops_cnt, ds_ops, ds_ops_cnt);
    if (rc != 0) goto done;
    
    rc = add_rras(in, out, ds_map, rra_mod_ops, rra_mod_ops_cnt, hash);
    if (rc != 0) goto done;
    

done:
    if (ds_ops != NULL) free(ds_ops);
    if (ds_map != NULL) free(ds_map);

    if (rc != 0 && out != NULL) {
	rrd_memory_free(out);
	free(out);
	out = NULL;
    }
    return out;
}

/* copies the RRD in to a new RRD and return it

   In that process, data sources may be removed or added.

   removeDS points to an array of strings, each naming a DS to be
   removed. The list itself is NULL terminated. addDS points to a
   similar list holding rrdcreate-style data source definitions to be
   added.
*/

static rrd_t *rrd_modify_r2(const rrd_t *in,
			    const char **removeDS,
			    const char **addDS,
			    rra_mod_op_t *rra_mod_ops, int rra_mod_ops_cnt,
			    int newstep,
			    unsigned long hash) 
{
    int rc = -1;
    /* basic check: do we have a new step size: if we do: is it a smaller than
       the original and is the old one a whole-number multiple of the new one? */
    
    int stretch = 0; 
    rrd_t *out = NULL;
    rrd_t *finalout = NULL;
    
    if (newstep > 0) {
	if (in->stat_head->pdp_step % newstep == 0
		&& in->stat_head->pdp_step / newstep > 1) {
	    /* we will "stretch" the RRD: existing rows will correspond to the same
	       time period, but the CF will consolidate 'stretch' times as many PDPs.
	     */
	    
	    stretch = in->stat_head->pdp_step / newstep;
	} else {
	    rrd_set_error("invalid 'newstep' parameter. The newsize must "
			  "divide the old step parameter without a remainder.");
	    goto done;
	}
    
	// create temporary RRD structure for in-place resizing...
	
	out = rrd_modify_structure(in, NULL, NULL, NULL, 0, hash);
	if (out == NULL) {
	    goto done;
	}
	    
	if (stretch > 1) {
	    rc = stretch_rras(out, stretch);
	    if (rc != 0) goto done;
/*	} else if (shrink > 1) {
	    rc = shrink_rras(out, shrink);
	    if (rc != 0) goto done;*/
	}
	
	
	finalout = rrd_modify_structure(out, removeDS, addDS, rra_mod_ops, rra_mod_ops_cnt, hash);
	if (finalout == NULL) {
	    goto done;
	}
    } else {
	// shortcut: do changes in one step
	finalout = rrd_modify_structure(in, removeDS, addDS, rra_mod_ops, rra_mod_ops_cnt, hash);
	if (finalout == NULL) {
	    goto done;
	}
    }
    rc = 0;
done:
    if (out) {
	rrd_memory_free(out);
	free(out);
	out = NULL;
    }
    if (rc != 0) {
	out = NULL;
	finalout = NULL;
    }
    
    return finalout;
}




// prepare CDPs + values for new RRA

static void prepare_CDPs(const rrd_t *in, rrd_t *out, 
			 int curr_rra,
			 int start_values_index_out,
			 const int *ds_map)
{
    cdp_prep_t empty_cdp_prep;
    memset(&empty_cdp_prep, 0, sizeof(cdp_prep_t));

    rra_def_t *rra_def = out->rra_def + curr_rra;

    enum cf_en cf = rrd_cf_conv(rra_def->cf_nam);
    int candidates_cnt = 0;
    candidate_t *candidates = NULL;
    candidate_t *chosen_candidate = NULL;
    candidate_extra_t junk;
    junk.l = 0;

    candidates = find_candidate_rras(in, rra_def, &candidates_cnt, junk, select_for_modify);

    if (candidates != NULL) {
	int ci;
	for (ci = 0 ; ci < candidates_cnt ; ci++) {
	    candidate_t *c = candidates + ci;
	    rra_def_t *cand_rra = c->rrd->rra_def + c->rra_index;
		
	    // we only accept AVERAGE RRAs or RRAs with pdp_cnt == 1
	    if (cand_rra->pdp_cnt == 1 || rrd_cf_conv(cand_rra->cf_nam) == CF_AVERAGE) {
		chosen_candidate = c;
		break;
	    }
	}
    }
#ifdef MODIFY_DEBUG
    fprintf(stderr, "chosen candidate index=%d row_cnt=%ld\n", chosen_candidate->rra_index, chosen_candidate->rra->row_cnt);
#endif
    int start_cdp_index_out = out->stat_head->ds_cnt * curr_rra;

    for (int i = 0 ; i < (int) out->stat_head->ds_cnt ; i++) {
	int mapped_i = ds_map[i];

	cdp_prep_t *cdp_prep = out->cdp_prep + start_cdp_index_out + i;
	memcpy(cdp_prep, &empty_cdp_prep, sizeof(cdp_prep_t));

	init_cdp(out, rra_def, out->pdp_prep + i, cdp_prep);

	if (chosen_candidate && mapped_i != -1) {
	    int ds_cnt = chosen_candidate->rrd->stat_head->ds_cnt;

	    /* we have a chosen candidate. Find out what the */

	    time_t last_up = in->live_head->last_up;

	    int timeslot = rra_def->pdp_cnt * in->stat_head->pdp_step;

	    int delta = last_up % timeslot;
	    time_t end_time = last_up, start_time;
	    if (delta != 0) {
		end_time = last_up - delta + timeslot;
	    }
	    start_time = end_time - timeslot + 1;

	    int start_row = row_for_time(chosen_candidate->rrd, 
					 chosen_candidate->rra, 
					 chosen_candidate->ptr->cur_row,
					 start_time); 
	    int end_row = row_for_time(chosen_candidate->rrd, 
				       chosen_candidate->rra, 
				       chosen_candidate->ptr->cur_row,
				       end_time); 

#ifdef MODIFY_DEBUG
	    fprintf(stderr, "need PDP data for %ld to %ld\n", start_time, end_time);
	    fprintf(stderr, "last_up %ld\n", chosen_candidate->rrd->live_head->last_up);
	    fprintf(stderr, "RAW fill CDP using rows %d to %d\n", start_row, end_row);
#endif
	    if (end_time == last_up) {
		end_row = chosen_candidate->ptr->cur_row;
	    }
	    if (end_time > last_up) {
		end_row = chosen_candidate->ptr->cur_row;
	    }

	    int cnt = end_row - start_row + 1;
	    if (end_row < start_row) {
		cnt += chosen_candidate->rra->row_cnt;
	    }
		
	    int row_cnt = chosen_candidate->rra->row_cnt;
		
	    // the chosen candidate CDP for the DS...
	    cdp_prep_t *ccdp = chosen_candidate->cdp + mapped_i;

#ifdef MODIFY_DEBUG
	    fprintf(stderr, "fill CDP using rows %d to %d (=%d)\n", start_row, end_row, cnt);
#endif
	    /*
	      if (start_row == -1) we are just at the start of a
	      new CDP interval and we can just reconstruct the CDP
	      from various information:

	      if (start_row != -1, we assume that we are a couple
	      of steps behind (namely at a time that would CAUSE
	      start_row to be -1, fill out the CDP and then we
	      update the CDP with data points from the chosen
	      candidate RRA.
	    */

	    // null out the CDP....
		
	    for (int z = 0 ; z < MAX_CDP_PAR_EN ; z++) {
		cdp_prep->scratch[z].u_val = 0;
	    }

	    rrd_value_t curr = out->rrd_value[start_values_index_out + 
					      out->stat_head->ds_cnt  * (out->rra_ptr[curr_rra].cur_row) +
					      i];

	    cdp_prep->scratch[CDP_primary_val].u_val = curr;
	    cdp_prep->scratch[CDP_val].u_val = 0;

	    if (start_row == -1) {
		// the primary value of the chosen_candidate cdp becomes the secondary value for the new CDP
		    
		cdp_prep->scratch[CDP_secondary_val].u_val = 
		    ccdp->scratch[CDP_primary_val].u_val;
	    } else {
		int pre_start = start_row - 1;
		if (pre_start < 0) pre_start = chosen_candidate->rra->row_cnt - 1;

		cdp_prep->scratch[CDP_secondary_val].u_val = 
		    chosen_candidate->values[ds_cnt * pre_start + mapped_i];

		int start_pdp_offset = rra_def->pdp_cnt;

		for (int j =  0 ; j < cnt ; j++) {
		    int row = (start_row + j) % row_cnt;
		    rrd_value_t v = chosen_candidate->values[ds_cnt * row + mapped_i];
			
		    update_cdp(
			       cdp_prep->scratch,    //    unival *scratch,
			       cf,   //    int current_cf,
			       v,    //    rrd_value_t pdp_temp_val,
			       0,    //    unsigned long rra_step_cnt,
			       1,    //    unsigned long elapsed_pdp_st,
			       start_pdp_offset--,    //    unsigned long start_pdp_offset,
			       rra_def->pdp_cnt,    //    unsigned long pdp_cnt,
			       chosen_candidate->rra->par[RRA_cdp_xff_val].u_val,    //    rrd_value_t xff,
			       0,    //    int i,
			       0     //    int ii)
			       );
		}
	    }
	}
    }

    if (candidates) free(candidates);
}


static int add_rras(const rrd_t *in, rrd_t *out, const int *ds_map,
		    const rra_mod_op_t *rra_mod_ops, int rra_mod_ops_cnt, unsigned long hash) 
{
    int rc = -1;

    /* now add any new RRAs: */
    cdp_prep_t empty_cdp_prep;
    int i, r;
    unsigned int last_rra_cnt = out->stat_head->rra_cnt;
    int total_out_rra_rows = 0;
    int total_cnt_out = 0;
    const char *require_version = NULL;

    memset(&empty_cdp_prep, 0, sizeof(cdp_prep_t));

    // first, calculate total number of rows already in rrd_value...

    for (i = 0 ; i < (int) last_rra_cnt ; i++) {
	total_out_rra_rows += out->rra_def[i].row_cnt;
    }
    total_cnt_out = out->stat_head->ds_cnt * total_out_rra_rows;

    for (r = 0 ; r < rra_mod_ops_cnt ; r++) {
	if (rra_mod_ops[r].op == 'a') {
	    rra_def_t rra_def;

	    // the hash doesn't really matter...
	    parseRRA(rra_mod_ops[r].def, &rra_def, out, hash, &require_version);

	    if (rrd_test_error()) {
		// failed!!!
		goto done;
            }

		out->rra_def = (rra_def_t *) copy_over_realloc(out->rra_def, out->stat_head->rra_cnt,
					    &rra_def, 0,
					    sizeof(rra_def_t));
	    if (out->rra_def == NULL) goto done;
	    out->stat_head->rra_cnt++;
	    
	    out->rra_def = handle_dependent_rras(out->rra_def, &(out->stat_head->rra_cnt), 
						hash);
	    if (out->rra_def == NULL) {
		goto done;
	    }
	}
    }

    if (require_version != NULL && atoi(require_version) < atoi(out->stat_head->version)) {
        strncpy(out->stat_head->version, require_version, 4);
        out->stat_head->version[4] = '\0';
    }

    if (last_rra_cnt < out->stat_head->rra_cnt) {
	// extend cdp_prep and rra_ptr arrays
		out->cdp_prep = (cdp_prep_t *) realloc(out->cdp_prep,
				sizeof(cdp_prep_t) * out->stat_head->ds_cnt 
				* (out->stat_head->rra_cnt));

	if (out->cdp_prep == NULL) {
	    rrd_set_error("out of memory");
	    goto done;
	}
	
	out->rra_ptr = (rra_ptr_t *) realloc(out->rra_ptr,
			       sizeof(rra_ptr_t) * out->stat_head->rra_cnt);
	
	if (out->rra_ptr == NULL) {
	    rrd_set_error("out of memory");
	    goto done;
	}
    }

    int curr_rra;
    for (curr_rra = last_rra_cnt ; curr_rra < (int) out->stat_head->rra_cnt ; curr_rra++ ) {
	// RRA added!!!
	rra_def_t *rra_def = out->rra_def + curr_rra;

	// null out CDPs
	int start_cdp_index_out = out->stat_head->ds_cnt * curr_rra;
	for (i = 0 ; i < (int) out->stat_head->ds_cnt ; i++) {
	    cdp_prep_t *cdp_prep = out->cdp_prep + start_cdp_index_out + i;
	    memcpy(cdp_prep,
		   &empty_cdp_prep, sizeof(cdp_prep_t));
	}

	out->rra_ptr[curr_rra].cur_row = rra_def->row_cnt - 1;

	// extend and fill rrd_value array...
	int start_values_index_out = total_out_rra_rows;

	total_out_rra_rows += rra_def->row_cnt;

	/* prepare space for output data */
	out->rrd_value = (rrd_value_t *) realloc(out->rrd_value,
				(total_out_rra_rows) * out->stat_head->ds_cnt
				* sizeof(rrd_value_t));
    
	if (out->rrd_value == NULL) {
	    rrd_set_error("out of memory");
	    goto done;
	}

	unsigned int oi, jj;
	for (oi = 0 ; oi < rra_def->row_cnt ; oi++) {
	    for (jj = 0 ; jj < out->stat_head->ds_cnt ; jj++) {
		out->rrd_value[total_cnt_out + oi * out->stat_head->ds_cnt + jj] = DNAN;
	    }
	}

	int rra_values_out = out->stat_head->ds_cnt * rra_def->row_cnt;

	// now try to populate the newly added rows 
	populate_row(in, out, ds_map,
		     rra_def, 
		     out->rra_ptr[curr_rra].cur_row,
		     out->rrd_value + total_cnt_out,
		     0, rra_def->row_cnt);

	prepare_CDPs(in, out, curr_rra, start_values_index_out, ds_map);

	total_cnt_out += rra_values_out;
    }
    rc = 0;
done:
    return rc;
}

int handle_modify(const rrd_t *in, const char *outfilename,
		  int argc, const char **argv, int optidx,
		  int newstep) {
    // parse add/remove options
    int rc = -1;
    int i;

    const char **del = NULL, **add = NULL;
    rra_mod_op_t *rra_ops = NULL;
    int rcnt = 0, acnt = 0, rraopcnt = 0;
    
    for (i = optidx ; i < argc ; i++) {
	if (strncmp("DEL:", argv[i], 4) == 0 && strlen(argv[i]) > 4) {
		del = (const char **) realloc((char **) del, (rcnt + 2) * sizeof(char*));   /* Cast 'del' from 'const char **' to 'char **' to avoid MSVC warning C4090 */
	    if (del == NULL) {
		rrd_set_error("out of memory");
		rc = -1;
		goto done;
	    }

	    del[rcnt] = strdup(argv[i] + 4);
	    if (del[rcnt] == NULL) {
		rrd_set_error("out of memory");
		rc = -1;
		goto done;
	    }

	    rcnt++;
	    del[rcnt] = NULL;
	} else if (strncmp("DS:", argv[i], 3) == 0 && strlen(argv[i]) > 3) {
		add = (const char **) realloc((char **) add, (acnt + 2) * sizeof(char*));   /* Cast 'add' from 'const char **' to 'char **' to avoid MSVC warning C4090 */
	    if (add == NULL) {
		rrd_set_error("out of memory");
		rc = -1;
		goto done;
	    }

	    add[acnt] = strdup(argv[i]);
	    if (add[acnt] == NULL) {
		rrd_set_error("out of memory");
		rc = -1;
		goto done;
	    }

	    acnt++;
	    add[acnt] = NULL;
	} else if (strncmp("RRA#", argv[i], 4) == 0 && strlen(argv[i]) > 4) {
		rra_mod_op_t rra_mod; // = { .def = NULL };  // not in VS2013
		rra_mod.def = NULL;
	    char sign;
	    unsigned int number;
	    unsigned int idx;
	    
	    if (sscanf(argv[i] + 4, "%u:%c%u", &idx, &sign, &number) != 3) {
		rrd_set_error("Failed to parse RRA# command");
		rc = -1;
		goto done;
	    }

	    rra_mod.index = idx;
	    switch (sign) {
	    case '=':
	    case '-':
	    case '+':
		rra_mod.index = idx;
		rra_mod.op = sign;
		rra_mod.row_count = number;
		rra_mod.final_row_count = 0;
		break;
	    default:
		rrd_set_error("Failed to parse RRA# command: invalid operation: %c", sign);
		rc = -1;
		goto done;
	    }

	    rra_ops = (rra_mod_op_t *) copy_over_realloc(rra_ops, rraopcnt,
					&rra_mod, 0, sizeof(rra_mod));
	    if (rra_ops == NULL) {
		rrd_set_error("out of memory");
		rc = -1;
		goto done;
	    }
	    rraopcnt++;
	} else if (strncmp("RRA:", argv[i], 4) == 0 && strlen(argv[i]) > 4) {
	    rra_mod_op_t rra_mod;
	    rra_mod.op = 'a';
	    rra_mod.index = -1;
	    rra_mod.def = strdup(argv[i]);

	    if (rra_mod.def == NULL) {
		rrd_set_error("out of memory");
		rc = -1;
		goto done;
	    }

		rra_ops = (rra_mod_op_t *) copy_over_realloc(rra_ops, rraopcnt,
					&rra_mod, 0, sizeof(rra_mod));
	    if (rra_ops == NULL) {
		rrd_set_error("out of memory");
		rc = -1;
		goto done;
	    }
	    rraopcnt++;
	} else if (strncmp("DELRRA:", argv[i], 7) == 0 && strlen(argv[i]) > 7) {
		rra_mod_op_t rra_mod;
		/* NOT in VS2013 C++ allowed
		= { .def = NULL,
				     .op = '=', 
				     .row_count = 0 // eg. deletion
	    };*/
		rra_mod.def = NULL;
		rra_mod.op = '='; 
		rra_mod.row_count = 0; // eg. deletion

	    rra_mod.index = atoi(argv[i] + 7);
	    if (rra_mod.index < 0 ) {
		rrd_set_error("DELRRA requires a non-negative, integer argument");
		rc = -1;
		goto done;
	    }

		rra_ops = (rra_mod_op_t *) copy_over_realloc(rra_ops, rraopcnt,
					&rra_mod, 0, sizeof(rra_mod));
	    if (rra_ops == NULL) {
		rrd_set_error("out of memory");
		rc = -1;
		goto done;
	    }
	    rraopcnt++;
	} else {
	    rrd_set_error("unparsable argument: %s", argv[i]);
	    rc = -1;
	    goto done;
	}
    }
    
    if (rcnt > 0 || acnt > 0 || rraopcnt > 0) {
	unsigned long hashed_name = FnvHash(outfilename);
	rrd_t *out = rrd_modify_r2(in, del, add, rra_ops, rraopcnt, newstep, hashed_name);
    
	if (out == NULL) {
	    goto done;
	}
    
	rc = write_rrd(outfilename, out);
	rrd_free(out);
	free(out);

	if (rc < 0) goto done;
    }
    
    rc = argc;

done:
    if (del) {
	for (const char **c = del ; *c ; c++) {
	    free((void*) *c);
	}
	free((char **) del);    /* Cast 'del' from 'const char **' to 'char **' to avoid MSVC warning C4090 */
    } 
    if (add) {
	for (const char **c = add ; *c ; c++) {
	    free((void*) *c);
	}
	free((char **) add);    /* Cast 'add' from 'const char **' to 'char **' to avoid MSVC warning C4090 */
    }
    if (rra_ops) {
	for (i = 0 ; i < rraopcnt ; i++) {
	    if (rra_ops[i].def) free(rra_ops[i].def);
	}
	free(rra_ops);
    }

    return rc;
}
