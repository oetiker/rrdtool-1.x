/*****************************************************************************
 * RRDtool 1.4.8  Copyright by Tobi Oetiker, 1997-2013
 *****************************************************************************
 * rrd_modify  Structurally modify an RRD file
 *****************************************************************************
 * Initially based on rrd_dump.c
 *****************************************************************************/
#include "rrd_tool.h"
#include "rrd_rpncalc.h"
#include "rrd_client.h"
#include "rrd_restore.h"   /* write_file */
#include "rrd_create.h"    /* parseDS */

#include <locale.h>

// prototype
static int write_rrd(const char *outfilename, rrd_t *out);

/* a convenience realloc/memcpy combo  */
static void * copy_over_realloc(void *dest, int dest_index, 
				const void *src, int index,
				ssize_t size) {
    void *r = realloc(dest, size * (dest_index + 1));
    if (r == NULL) {
        rrd_set_error("copy_over_realloc: realloc failed.");
	return r;
    }

    memcpy(((char*)r) + size * dest_index, ((char*)src) + size * index, size);
    return r;
}

/* copies the RRD named by infilename to a new RRD called outfilename. 

   In that process, data sources may be removed or added.

   removeDS points to an array of strings, each naming a DS to be
   removed. The list itself is NULL terminated. addDS points to a
   similar list holding rrdcreate-style data source definitions to be
   added.
*/

typedef struct {
    int index;
    char op;  // '+', '-', '='. 
    unsigned int row_count;
    unsigned int final_row_count;
    
} rra_mod_op_t;


static int rrd_modify_r(const char *infilename,
			const char *outfilename,
			const char **removeDS,
			const char **addDS,
			rra_mod_op_t *rra_mod_ops, int rra_mod_ops_cnt) {
    rrd_t in, out;
    int rc = -1;
    unsigned int i, j;
    rrd_file_t *rrd_file;
    char       *old_locale = NULL;
    char       *ops = NULL;
    unsigned int ops_cnt = 0;

    old_locale = setlocale(LC_NUMERIC, NULL);
    setlocale(LC_NUMERIC, "C");

    rrd_clear_error();

    if (rrdc_is_any_connected()) {
	// is it a good idea to just ignore the error ????
	rrdc_flush(infilename);
	rrd_clear_error();
    }

    rrd_init(&in);
    rrd_init(&out);

    rrd_file = rrd_open(infilename, &in, RRD_READONLY | RRD_READAHEAD);
    if (rrd_file == NULL) {
	goto done;
    }

    /* currently we only allow to modify version 3 RRDs. If other
       files should be modified, a dump/restore cycle should be
       done.... */

    if (strcmp(in.stat_head->version, "0003") != 0) {
	rrd_set_error("direct modification is only supported for version 3 RRD files. Consider to dump/restore before retrying a modification");
	goto done;
    }

    /* copy over structure to out RRD */

    out.stat_head = (stat_head_t *) malloc(sizeof(stat_head_t));
    if (out.stat_head == NULL) {
        rrd_set_error("rrd_modify_r: malloc failed.");
	goto done;
    }
    
    memset(out.stat_head, 0, (sizeof(stat_head_t)));

    strncpy(out.stat_head->cookie, "RRD", sizeof(out.stat_head->cookie));
    strcpy(out.stat_head->version, "0003");
    out.stat_head->float_cookie = FLOAT_COOKIE;
    out.stat_head->pdp_step = in.stat_head->pdp_step;

    out.stat_head->ds_cnt = 0;
    out.stat_head->rra_cnt = 0;

    out.live_head = copy_over_realloc(out.live_head, 0, in.live_head, 0,
				      sizeof(live_head_t));

    if (out.live_head == NULL) goto done;

    /* use the ops array as a scratchpad to remember what we are about
       to do to each DS. There is one entry for every DS in the
       original RRD and one additional entry for every added DS. 

       Entries marked as 
       - 'c' will be copied to the out RRD, 
       - 'd' will not be copied (= will effectively be deleted)
       - 'a' will be added.
    */
    ops_cnt = in.stat_head->ds_cnt;
    ops = malloc(ops_cnt);

    if (ops == NULL) {
        rrd_set_error("parse_tag_rrd: malloc failed.");
	goto done;
    } 

    memset(ops, 'c', in.stat_head->ds_cnt);
    
    /* copy over existing DS definitions (and related data
       structures), check on the way (and skip) if they should be
       deleted
       */
    for (i = 0 ; i < in.stat_head->ds_cnt ; i++) {
	const char *c;
	if (removeDS != NULL) {
	    for (j = 0, c = removeDS[j] ; c ; j++, c = removeDS[j]) {
		if (strcmp(in.ds_def[i].ds_nam, c) == 0) {
		    ops[i] = 'd';
		    break;
		}
	    }
	}

	switch (ops[i]) {
	case 'c': {
	    j = out.stat_head->ds_cnt;
	    out.stat_head->ds_cnt++;

	    out.ds_def = copy_over_realloc(out.ds_def, j, 
					   in.ds_def, i,
					   sizeof(ds_def_t));
	    if (out.ds_def == NULL) goto done;

	    out.pdp_prep = copy_over_realloc(out.pdp_prep, j, 
					     in.pdp_prep, i,
					     sizeof(pdp_prep_t));
	    if (out.pdp_prep == NULL) goto done;
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

    /* now add any definitions to be added */
    if (addDS) {
	const char *c;
	for (j = 0, c = addDS[j] ; c ; j++, c = addDS[j]) {
	    ds_def_t added;

	    // parse DS
	    parseDS(c + 3,
		    &added, // out.ds_def + out.stat_head->ds_cnt,
		    &out, lookup_DS);

	    // check if there is a name clash with an existing DS
	    if (lookup_DS(&out, added.ds_nam) >= 0) {
		rrd_set_error("Duplicate DS name: %s", added.ds_nam);
		goto done;
	    }

	    // copy parse result to output RRD
	    out.ds_def = copy_over_realloc(out.ds_def, out.stat_head->ds_cnt, 
					   &added, 0,
					   sizeof(ds_def_t));
	    if (out.ds_def == NULL) goto done;

	    // also add a pdp_prep_t
	    pdp_prep_t added_pdp_prep;
	    memset(&added_pdp_prep, 0, sizeof(added_pdp_prep));
	    strcpy(added_pdp_prep.last_ds, "U");

	    added_pdp_prep.scratch[PDP_val].u_val = 0.0;
	    added_pdp_prep.scratch[PDP_unkn_sec_cnt].u_cnt =
		out.live_head->last_up % out.stat_head->pdp_step;

	    out.pdp_prep = copy_over_realloc(out.pdp_prep, 
					     out.stat_head->ds_cnt, 
					     &added_pdp_prep, 0,
					     sizeof(pdp_prep_t));
	    if (out.pdp_prep == NULL) goto done;

	    out.stat_head->ds_cnt++;

	    // and extend the ops array as well
	    ops = realloc(ops, ops_cnt + 1);
	    ops[ops_cnt] = 'a';
	    ops_cnt++;
	}
    }

    /* now take care to copy all RRAs, removing and adding columns for
       every row as needed for the requested DS changes */

    /* we also reorder all rows, adding/removing rows as needed */

    rra_ptr_t rra_0_ptr = { .cur_row = 0 };

    cdp_prep_t empty_cdp_prep;
    memset(&empty_cdp_prep, 0, sizeof(empty_cdp_prep));

    int total_out_rra_rows = 0, total_in_rra_rows = 0;

    rra_mod_op_t *rra_op = NULL;
    int r;
    for (j = 0 ; j < in.stat_head->rra_cnt ; j++) {
	total_in_rra_rows +=  in.rra_def[j].row_cnt;

	rra_op = NULL;
	for (r = 0 ; r < rra_mod_ops_cnt ; r++) {
	    if (rra_mod_ops[r].index == j) {
		rra_op = rra_mod_ops + r;
		break;
	    }
	}

	int final_row_count = in.rra_def[j].row_cnt;
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

	out.cdp_prep = realloc(out.cdp_prep, 
			       sizeof(cdp_prep_t) * out.stat_head->ds_cnt 
			       * (out.stat_head->rra_cnt + 1));
	
	if (out.cdp_prep == NULL) {
	    rrd_set_error("Cannot allocate memory");
	    goto done;
	}

	/* for every RRA copy only those CDPs in the prep area where we keep 
	   the DS! */

	int start_index_in  = in.stat_head->ds_cnt * j;
	int start_index_out = out.stat_head->ds_cnt * out.stat_head->rra_cnt;
	
	out.rra_def = copy_over_realloc(out.rra_def, out.stat_head->rra_cnt,
					in.rra_def, j,
					sizeof(rra_def_t));
	if (out.rra_def == NULL) goto done;

	// adapt row count:
	out.rra_def[out.stat_head->rra_cnt].row_cnt = final_row_count;

	out.rra_ptr = copy_over_realloc(out.rra_ptr, out.stat_head->rra_cnt,
					&rra_0_ptr, 0,
					sizeof(rra_ptr_t));
	if (out.rra_ptr == NULL) goto done; 

	int ii;
	for (i = ii = 0 ; i < ops_cnt ; i++) {
	    switch (ops[i]) {
	    case 'c': {
		memcpy(out.cdp_prep + start_index_out + ii,
		       in.cdp_prep + start_index_in + i, 
		       sizeof(cdp_prep_t));
		ii++;
		break;
	    } 
	    case 'a': {
		cdp_prep_t *cdp_prep = out.cdp_prep + start_index_out + ii;
		memcpy(cdp_prep,
		       &empty_cdp_prep, sizeof(cdp_prep_t));

		init_cdp(&out, 
			 out.rra_def + out.stat_head->rra_cnt,
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

	total_out_rra_rows +=  out.rra_def[out.stat_head->rra_cnt].row_cnt;

	out.stat_head->rra_cnt++;
    }

    /* read and process all data ... */

    /* there seem to be two function in the current rrdtool codebase
       dealing with writing a new rrd file to disk: write_file and
       rrd_create_fn.  The latter has the major problem, that it tries
       to free data passed to it (WTF?), but write_file assumes
       chronologically ordered data in RRAs (that is, in the data
       space starting at rrd.rrd_value....

       This is the reason why: 
       - we use write_file and 
       - why we reset cur_row in RRAs and reorder data to be cronological
    */

    char *all_data = NULL;

    /* prepare space to read data in */
    all_data = realloc(all_data, 
		       total_in_rra_rows * in.stat_head->ds_cnt
		       * sizeof(rrd_value_t));
    in.rrd_value = (void *) all_data;
    
    if (all_data == NULL) {
	rrd_set_error("out of memory");
	goto done;
    }

    /* prepare space for output data */
    out.rrd_value = realloc(out.rrd_value,
			    total_out_rra_rows * out.stat_head->ds_cnt
			    * sizeof(rrd_value_t));
    
    if (out.rrd_value == NULL) {
	rrd_set_error("out of memory");
	goto done;
    }

    ssize_t rra_start = 0, rra_start_out = 0;

    int total_cnt = 0, total_cnt_out = 0;

    int out_rra = 0;
    for (i = 0; i < in.stat_head->rra_cnt; i++) {
	rra_op = NULL;
	for (r = 0 ; r < rra_mod_ops_cnt ; r++) {
	    if (rra_mod_ops[r].index == i) {
		rra_op = rra_mod_ops + r;
		break;
	    }
	}

	if (rra_op) {
	    if (rra_op->final_row_count == 0) {
		// RRA deleted - skip !
		continue;
	    }
	}

	/* number and sizes of all the data in an RRA */
	int rra_values     = in.stat_head->ds_cnt  * in.rra_def[i].row_cnt;
	int rra_values_out = out.stat_head->ds_cnt * out.rra_def[out_rra].row_cnt;

	ssize_t rra_size     = sizeof(rrd_value_t) * rra_values;
	ssize_t rra_size_out = sizeof(rrd_value_t) * rra_values_out;

	/* reorder data by cleaverly reading it into the right place */

	/*
	  Data in the RRD file:
	                             
	  <-------------RRA-------------->
	  BBBBBBBBBBBBBBBBBBBBBAAAAAAAAAAA
          ^                    ^          ^
          0                 cur_row    row_cnt

	  Data in memory should become
	                             
	  <-------------RRA-------------->
          AAAAAAAAAAABBBBBBBBBBBBBBBBBBBBB
	  ^          ^                    ^
       cur_row=0     n                 row_cnt

	  using:   n = row_cnt - cur_row

	  we first read cur_row values to position n and then n values to 
	  position 0.

	 */

	/* instead of the actual cur_row, we have to add 1, because
	   cur_row does not point to the next "free", but the
	   currently in use position */
	int last = (in.rra_ptr[i].cur_row + 1) % in.rra_def[i].row_cnt; 
	int n = in.rra_def[i].row_cnt - last;

	size_t to_read = last * sizeof(rrd_value_t) * in.stat_head->ds_cnt;
	size_t read_bytes;

	if (to_read > 0) {
	    read_bytes = 
		rrd_read(rrd_file, 
			 all_data + rra_start + 
			 n * sizeof(rrd_value_t) * in.stat_head->ds_cnt, 
			 to_read);	
	    
	    if (read_bytes != to_read) {
		rrd_set_error("short read 1");
		goto done;
	    }
	}

	to_read = n * sizeof(rrd_value_t) * in.stat_head->ds_cnt ;
	if (to_read > 0) {
	    read_bytes = 
		rrd_read(rrd_file, 
			 all_data + rra_start,
			 to_read);
	    
	    if (read_bytes != to_read) {
		rrd_set_error("short read 2");
		goto done;
	    }
	}

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
		if (row_count < in.rra_def[i].row_cnt) {
		    row_count = in.rra_def[i].row_cnt - row_count;
		    op = '-';
		} else if (row_count > in.rra_def[i].row_cnt) {
		    row_count = row_count - in.rra_def[i].row_cnt;
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
		    for (j = 0 ; j < out.stat_head->ds_cnt ; j++) {
			out.rrd_value[total_cnt_out + 
				      oi * out.stat_head->ds_cnt +
				      j] = DNAN;
		    }		
		}
		break;
	    default:
		rrd_set_error("RRA modification operation '%c' "
			      "not (yet) supported", rra_op->op);
		goto done;
	    }
	}

	for ( ; ii < in.rra_def[i].row_cnt 
		  && oi < out.rra_def[out_rra].row_cnt ; ii++, oi++) {
	    for (j = jj = 0 ; j < ops_cnt ; j++) {
		switch (ops[j]) {
		case 'c': {
		    out.rrd_value[total_cnt_out + oi * out.stat_head->ds_cnt + jj] =
			in.rrd_value[total_cnt + ii * in.stat_head->ds_cnt + j];

		    /* it might be better to use memcpy, actually (to
		       treat them opaquely)... so keep the code for
		       the time being */
		    /*
		    memcpy((void*) (out.rrd_value + total_cnt_out + oi * out.stat_head->ds_cnt + jj),
			   (void*) (in.rrd_value + total_cnt + ii * in.stat_head->ds_cnt + j), sizeof(rrd_value_t));
		    */			   
		    jj++;
		    break;
		}
		case 'a': {
		    out.rrd_value[total_cnt_out + oi * out.stat_head->ds_cnt + jj] = DNAN;
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

	rra_start     += rra_size;
	rra_start_out += rra_size_out;

	total_cnt     += rra_values;
	total_cnt_out += rra_values_out;

	out_rra++;
    }

    rc = write_rrd(outfilename, &out);

done:
    /* clean up */
    if (old_locale) 
	setlocale(LC_NUMERIC, old_locale);

    if (rrd_file != NULL) {
	rrd_close(rrd_file);
    }
    rrd_free(&in);
    rrd_free(&out);

    if (ops != NULL) free(ops);

    return rc;
}


static int write_rrd(const char *outfilename, rrd_t *out) {
    int rc = -1;
    char *tmpfile = NULL;

    /* write out the new file */
    FILE *fh = NULL;
    if (strcmp(outfilename, "-") == 0) {
	fh = stdout;
	// to stdout
    } else {
	/* create RRD with a temporary name, rename atomically afterwards. */
	tmpfile = malloc(strlen(outfilename) + 7);
	if (tmpfile == NULL) {
	    rrd_set_error("out of memory");
	    goto done;
	}

	strcpy(tmpfile, outfilename);
	strcat(tmpfile, "XXXXXX");
	
	int tmpfd = mkstemp(tmpfile);
	if (tmpfd < 0) {
	    rrd_set_error("Cannot create temporary file");
	    goto done;
	}

	fh = fdopen(tmpfd, "wb");
	if (fh == NULL) {
	    // some error 
	    rrd_set_error("Cannot open output file");
	    goto done;
	}
    }

    rc = write_fh(fh, out);

    if (fh != NULL && tmpfile != NULL) {
	/* tmpfile != NULL indicates that we did NOT write to stdout,
	   so we have to close the stream and do the rename dance */

	fclose(fh);
	if (rc == 0)  {
	    // renaming is only done if write_fh was successful
	    struct stat stat_buf;

	    /* in case we have an existing file, copy its mode... This
	       WILL NOT take care of any ACLs that may be set. Go
	       figure. */
	    if (stat(outfilename, &stat_buf) != 0) {
		/* an error occurred (file not found, maybe?). Anyway:
		   set the mode to 0666 using current umask */
		stat_buf.st_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
		
		mode_t mask = umask(0);
		umask(mask);

		stat_buf.st_mode &= ~mask;
	    }
	    if (chmod(tmpfile, stat_buf.st_mode) != 0) {
		rrd_set_error("Cannot chmod temporary file!");
		goto done;
	    }

	    // before we rename the file to the target file: forget all cached changes....
	    if (rrdc_is_any_connected()) {
		// is it a good idea to just ignore the error ????
		rrdc_forget(outfilename);
		rrd_clear_error();
	    }

	    if (rename(tmpfile, outfilename) != 0) {
		rrd_set_error("Cannot rename temporary file to final file!");
		unlink(tmpfile);
		goto done;
	    }

	    // after the rename: forget the file again, just to be sure...
	    if (rrdc_is_any_connected()) {
		// is it a good idea to just ignore the error ????
		rrdc_forget(outfilename);
		rrd_clear_error();
	    }
	} else {
	    /* in case of any problems during write: just remove the
	       temporary file! */
	    unlink(tmpfile);
	}
    }
done:
    if (tmpfile != NULL)
	free(tmpfile);

    return rc;
}

int rrd_modify (
    int argc,
    char **argv)
{
    int       rc = 9;
    int       i;
    char     *opt_daemon = NULL;

    /* init rrd clean */

    optind = 0;
    opterr = 0;         /* initialize getopt */

    while (42) {/* ha ha */
        int       opt;
        int       option_index = 0;
        static struct option long_options[] = {
            {"daemon", required_argument, 0, 'd'},
            {0, 0, 0, 0}
        };

        opt = getopt_long(argc, argv, "d:", long_options, &option_index);

        if (opt == EOF)
            break;

        switch (opt) {
        case 'd':
            if (opt_daemon != NULL)
                    free (opt_daemon);
            opt_daemon = strdup (optarg);
            if (opt_daemon == NULL)
            {
                rrd_set_error ("strdup failed.");
                return (-1);
            }
            break;

        default:
            rrd_set_error("usage rrdtool %s"
                          "in.rrd out.rrd", argv[0]);
            return (-1);
            break;
        }
    }                   /* while (42) */

    if ((argc - optind) < 2) {
        rrd_set_error("usage rrdtool %s"
                      "in.rrd out.rrd", argv[0]);
        return (-1);
    }

    // connect to daemon (will take care of environment variable automatically)
    if (rrdc_connect(opt_daemon) != 0) {
	rrd_set_error("Cannot connect to daemon");
	return 1;
    }

    if (opt_daemon) {
	free(opt_daemon);
	opt_daemon = NULL;
    }

    // parse add/remove options
    const char **remove = NULL, **add = NULL;
    rra_mod_op_t *rra_ops = NULL;
    int rcnt = 0, acnt = 0, rraopcnt = 0;

    for (i = optind + 2 ; i < argc ; i++) {
	if (strncmp("DEL:", argv[i], 4) == 0 && strlen(argv[i]) > 4) {
	    remove = realloc(remove, (rcnt + 2) * sizeof(char*));
	    if (remove == NULL) {
		rrd_set_error("out of memory");
		rc = -1;
		goto done;
	    }

	    remove[rcnt] = strdup(argv[i] + 4);
	    if (remove[rcnt] == NULL) {
		rrd_set_error("out of memory");
		rc = -1;
		goto done;
	    }

	    rcnt++;
	    remove[rcnt] = NULL;
	}
	if (strncmp("DS:", argv[i], 3) == 0 && strlen(argv[i]) > 3) {
	    add = realloc(add, (acnt + 2) * sizeof(char*));
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
	}
  	if (strncmp("RRA#", argv[i], 4) == 0 && strlen(argv[i]) > 4) {
	    rra_mod_op_t rra_mod;
	    char sign;
	    unsigned int number;
	    unsigned int index;
	    
	    if (sscanf(argv[i] + 4, "%u:%c%u", &index, &sign, &number) != 3) {
		rrd_set_error("Failed to parse RRA# command");
		rc = -1;
		goto done;
	    }

	    rra_mod.index = index;
	    switch (sign) {
	    case '=':
	    case '-':
	    case '+':
		rra_mod.index = index;
		rra_mod.op = sign;
		rra_mod.row_count = number;
		rra_mod.final_row_count = 0;
		break;
	    default:
		rrd_set_error("Failed to parse RRA# command: invalid operation: %c", sign);
		rc = -1;
		goto done;
	    }

	    rra_ops = copy_over_realloc(rra_ops, rraopcnt,
					&rra_mod, 0, sizeof(rra_mod));
	    if (rra_ops == NULL) {
		rrd_set_error("out of memory");
		rc = -1;
		goto done;
	    }
	    rraopcnt++;
	}
  }

    if ((argc - optind) >= 2) {
        rc = rrd_modify_r(argv[optind], argv[optind + 1], 
			  remove, add, rra_ops, rraopcnt);
    } else {
	rrd_set_error("missing arguments");
	rc = 2;
    }

done:
    if (remove) {
	for (const char **c = remove ; *c ; c++) {
	    free((void*) *c);
	}
	free(remove);
    } 
    if (add) {
	for (const char **c = add ; *c ; c++) {
	    free((void*) *c);
	}
	free(add);
    }
    if (rra_ops) {
	free(rra_ops);
    }
    return rc;
}
