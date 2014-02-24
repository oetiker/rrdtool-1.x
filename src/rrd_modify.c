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

static int rrd_modify_r(const char *infilename,
			const char *outfilename,
			const char **removeDS,
			const char **addDS) {
    rrd_t in, out;
    int rc = -1;
    unsigned int i, j;
    rrd_file_t *rrd_file;
    char       *old_locale = "";
    char       *ops = NULL;
    unsigned int ops_cnt = 0;

    rrd_init(&in);
    rrd_init(&out);

    rrd_file = rrd_open(infilename, &in, RRD_READONLY | RRD_READAHEAD);
    if (rrd_file == NULL) {
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

    out.live_head = (live_head_t *) malloc(sizeof(live_head_t));
    if (out.live_head == NULL) {
        rrd_set_error("parse_tag_rrd: malloc failed.");
	goto done;
    }

    out.live_head->last_up = in.live_head->last_up;
    out.live_head->last_up_usec = in.live_head->last_up_usec;

    ops_cnt = in.stat_head->ds_cnt;
    ops = malloc(ops_cnt);

    if (ops == NULL) {
        rrd_set_error("parse_tag_rrd: malloc failed.");
	goto done;
    } 

    memset(ops, 'c', in.stat_head->ds_cnt);
    
    // check all DSs for deletion
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

	if (ops[i] == 'c') {
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

	}
    }

    if (addDS) {
	const char *c;
	for (j = 0, c = addDS[j] ; c ; j++, c = addDS[j]) {
	    // should test for name clash
	    ds_def_t empty;
	    out.ds_def = copy_over_realloc(out.ds_def, out.stat_head->ds_cnt, 
					   &empty, 0,
					   sizeof(ds_def_t));
	    if (out.ds_def == NULL) goto done;


	    // parse DS
	    parseDS(c + 3,
		    out.ds_def + out.stat_head->ds_cnt,
		    &out, lookup_DS);


	    if (lookup_DS(&out, out.ds_def[out.stat_head->ds_cnt].ds_nam) 
		>= 0) {
		rrd_set_error("Duplicate DS name: %s",
			      out.ds_def[out.stat_head->ds_cnt].ds_nam);

		goto done;
	    }

	    out.stat_head->ds_cnt++;

	    ops = realloc(ops, ops_cnt + 1);
	    ops[ops_cnt] = 'a';
	    ops_cnt++;
	}
    }




    rra_ptr_t rra_0_ptr = { .cur_row = 0 };
    for (j = 0 ; j < in.stat_head->rra_cnt ; j++) {
	/* for every RRA copy only those CDPs in the prep area where we keep 
	   the DS! */

	out.cdp_prep = realloc(out.cdp_prep, 
			       sizeof(cdp_prep_t) * out.stat_head->ds_cnt 
			       * (j+1));
	int start_index_in  = in.stat_head->ds_cnt * j;
	int start_index_out = out.stat_head->ds_cnt * j;
	
	int ii;
	for (i = ii = 0 ; i < ops_cnt ; i++) {
	    if (ops[i] == 'c') {
		memcpy(out.cdp_prep + start_index_out + ii,
		       in.cdp_prep + start_index_in + i, sizeof(cdp_prep_t));
		ii++;
	    }
	    if (ops[i] == 'a') {
		cdp_prep_t empty;
		memset(&empty, 0, sizeof(empty));

		memcpy(out.cdp_prep + start_index_out + ii,
		       &empty, sizeof(cdp_prep_t));
		ii++;
	    }
	}

	out.rra_def = copy_over_realloc(out.rra_def, j,
					in.rra_def, j,
					sizeof(rra_def_t));
	if (out.rra_def == NULL) goto done;

	out.rra_ptr = copy_over_realloc(out.rra_ptr, j,
					&rra_0_ptr, 0,
					sizeof(rra_ptr_t));
	if (out.rra_ptr == NULL) goto done; 

	// 	out.rra_ptr[j].cur_row = out.rra_def[j].row_cnt - 1;

	out.stat_head->rra_cnt++;
    }

    old_locale = setlocale(LC_NUMERIC, NULL);
    setlocale(LC_NUMERIC, "C");

    /* read all data ... */

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
    ssize_t total_size = 0, total_size_out = 0;
    ssize_t rra_start = 0, rra_start_out = 0;

    int total_cnt = 0, total_cnt_out = 0;

    for (i = 0; i < in.stat_head->rra_cnt; i++) {
	int rra_values     = in.stat_head->ds_cnt  * in.rra_def[i].row_cnt;
	int rra_values_out = out.stat_head->ds_cnt * out.rra_def[i].row_cnt;

	ssize_t rra_size     = sizeof(rrd_value_t) * rra_values;
	ssize_t rra_size_out = sizeof(rrd_value_t) * rra_values_out;
	/*
	fprintf(stderr, "A %08x\n", &(in.rra_ptr[i]));
	fprintf(stderr, "A %08x\n", in.rra_ptr[i].cur_row);
	*/
	total_size += rra_size;
	all_data = realloc(all_data, total_size);
	in.rrd_value = (void *) all_data;

	if (all_data == NULL) {
	    rrd_set_error("out of memory");
	    goto done;
	}

	total_size_out += rra_size_out;
	out.rrd_value = realloc(out.rrd_value, total_size_out);

	if (out.rrd_value == NULL) {
	    rrd_set_error("out of memory");
	    goto done;
	}



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

	unsigned int ii, jj;
	for (ii = 0 ; ii < in.rra_def[i].row_cnt ; ii++) {
	    for (j = jj = 0 ; j < ops_cnt ; j++) {
		if (ops[j] == 'c') {
		    out.rrd_value[total_cnt_out +ii*out.stat_head->ds_cnt +jj] =
			in.rrd_value[total_cnt + ii * in.stat_head->ds_cnt + j];

		    /*
		    memcpy((void*) (out.rrd_value + total_cnt_out + ii * out.stat_head->ds_cnt + jj),
			   (void*) (in.rrd_value + total_cnt + ii * in.stat_head->ds_cnt + j), sizeof(rrd_value_t));
		    */			   
		    /*
		    fprintf(stderr, "%d->%d\n",
			    total_cnt + ii * in.stat_head->ds_cnt + j,
			    total_cnt_out + ii * out.stat_head->ds_cnt + jj);
		    */
		    jj++;
		}
		if (ops[j] == 'a') {
		    out.rrd_value[total_cnt_out +ii*out.stat_head->ds_cnt +jj] = DNAN;
		    jj++;
		}
	    }
	}

	rra_start     += rra_size;
	rra_start_out += rra_size_out;

	total_cnt     += rra_values;
	total_cnt_out += rra_values_out;
    }
/*
    for (int z  = 0 ; z < 160 ; z++) {
	fprintf(stderr, "%02x ", (unsigned) ((char*)all_data)[z] & 0x0ff);
	if (z % 16 == 15) {
	    fprintf(stderr, "\n");
	}
    }


    fprintf(stderr, "\n");
    fprintf(stderr, "cnt %ld\n", (long) total_size);
*/


    write_file(outfilename, &out);

    rc = 0;
done:
    if (rrd_file != NULL) {
	rrd_close(rrd_file);
    }
    rrd_free(&in);
    rrd_free(&out);

    if (ops != NULL) free(ops);

    return rc;
}

int rrd_modify (
    int argc,
    char **argv)
{
    int       rc;
    int       i;
    /** 
     * 0 = no header
     * 1 = dtd header
     * 2 = xsd header
     */
    int       opt_header = 1;
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

    rc = rrdc_flush_if_daemon(opt_daemon, argv[optind]);
    if (opt_daemon) free(opt_daemon);
    if (rc) return (rc);

    // parse add/remove options
    const char **remove = NULL, **add = NULL;
    int rcnt = 0, acnt = 0;

    for (i = optind + 2 ; i < argc ; i++) {
	if (strncmp("DEL:", argv[i], 4) == 0 && strlen(argv[i]) > 4) {
	    remove = realloc(remove, (rcnt + 2) * sizeof(char*));
	    if (remove == NULL) {
		rrd_set_error("out of memory");
		return -1;
	    }

	    remove[rcnt] = strdup(argv[i] + 4);
	    if (remove[rcnt] == NULL) {
		rrd_set_error("out of memory");
		return -1;
	    }

	    rcnt++;
	    remove[rcnt] = NULL;
	}
	if (strncmp("DS:", argv[i], 3) == 0 && strlen(argv[i]) > 3) {
	    add = realloc(add, (acnt + 2) * sizeof(char*));
	    if (add == NULL) {
		rrd_set_error("out of memory");
		return -1;
	    }

	    add[acnt] = strdup(argv[i]);
	    if (add[acnt] == NULL) {
		rrd_set_error("out of memory");
		return -1;
	    }

	    acnt++;
	    add[acnt] = NULL;
	}
    }

    if ((argc - optind) >= 2) {
        rc = rrd_modify_r(argv[optind], argv[optind + 1], 
			  remove, add);
    }

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
    return rc;
}
