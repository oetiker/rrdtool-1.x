/*****************************************************************************
 * RRDtool 1.2rc3  Copyright by Tobi Oetiker, 1997-2005
 *****************************************************************************
 * rrd_update.c  RRD Update Function
 *****************************************************************************
 * $Id$
 *****************************************************************************/

#include "rrd_tool.h"
#include <sys/types.h>
#include <fcntl.h>
#ifdef HAVE_MMAP
 #include <sys/mman.h>
#endif

#ifdef WIN32
 #include <sys/locking.h>
 #include <sys/stat.h>
 #include <io.h>
#endif

#include "rrd_hw.h"
#include "rrd_rpncalc.h"

#include "rrd_is_thread_safe.h"

#ifdef WIN32
/*
 * WIN32 does not have gettimeofday	and struct timeval. This is a quick and dirty
 * replacement.
 */
#include <sys/timeb.h>

struct timeval {
	time_t tv_sec; /* seconds */
	long tv_usec;  /* microseconds */
};

struct __timezone {
	int  tz_minuteswest; /* minutes W of Greenwich */
	int  tz_dsttime;     /* type of dst correction */
};

static gettimeofday(struct timeval *t, struct __timezone *tz) {
	
	struct timeb current_time;

	_ftime(&current_time);
	
	t->tv_sec  = current_time.time;
	t->tv_usec = current_time.millitm * 1000;
}

#endif
/*
 * normilize time as returned by gettimeofday. usec part must
 * be always >= 0
 */
static void normalize_time(struct timeval *t)
{
	if(t->tv_usec < 0) {
		t->tv_sec--;
		t->tv_usec += 1000000L;
	}
}

/* Local prototypes */
int LockRRD(FILE *rrd_file);
#ifdef HAVE_MMAP
info_t *write_RRA_row (rrd_t *rrd, unsigned long rra_idx, 
					unsigned long *rra_current,
					unsigned short CDP_scratch_idx, FILE *rrd_file,
					info_t *pcdp_summary, time_t *rra_time, void *rrd_mmaped_file);
#else
info_t *write_RRA_row (rrd_t *rrd, unsigned long rra_idx, 
					unsigned long *rra_current,
					unsigned short CDP_scratch_idx, FILE *rrd_file,
					info_t *pcdp_summary, time_t *rra_time);
#endif
int rrd_update_r(char *filename, char *template, int argc, char **argv);
int _rrd_update(char *filename, char *template, int argc, char **argv, 
					info_t*);

#define IFDNAN(X,Y) (isnan(X) ? (Y) : (X));


#ifdef STANDALONE
int 
main(int argc, char **argv){
        rrd_update(argc,argv);
        if (rrd_test_error()) {
                printf("RRDtool 1.2rc3  Copyright by Tobi Oetiker, 1997-2005\n\n"
                        "Usage: rrdupdate filename\n"
                        "\t\t\t[--template|-t ds-name:ds-name:...]\n"
                        "\t\t\ttime|N:value[:value...]\n\n"
                        "\t\t\tat-time@value[:value...]\n\n"
                        "\t\t\t[ time:value[:value...] ..]\n\n");
                                   
                printf("ERROR: %s\n",rrd_get_error());
                rrd_clear_error();                                                            
                return 1;
        }
        return 0;
}
#endif

info_t *rrd_update_v(int argc, char **argv)
{
    char             *template = NULL;          
	info_t *result = NULL;
	infoval rc;

    while (1) {
		static struct option long_options[] =
			{
				{"template",      required_argument, 0, 't'},
				{0,0,0,0}
			};
		int option_index = 0;
		int opt;
		opt = getopt_long(argc, argv, "t:", 
						  long_options, &option_index);
		
		if (opt == EOF)
			break;
		
		switch(opt) {
		case 't':
			template = optarg;
			break;
		
		case '?':
			rrd_set_error("unknown option '%s'",argv[optind-1]);
            rc.u_int = -1;
			goto end_tag;
		}
    }

    /* need at least 2 arguments: filename, data. */
    if (argc-optind < 2) {
		rrd_set_error("Not enough arguments");
        rc.u_int = -1;
		goto end_tag;
    }
    result = info_push(NULL,sprintf_alloc("return_value"),RD_I_INT,rc);
   	rc.u_int = _rrd_update(argv[optind], template,
		      argc - optind - 1, argv + optind + 1, result);
    result->value.u_int = rc.u_int;
end_tag:
    return result;
}

int
rrd_update(int argc, char **argv)
{
    char             *template = NULL;          
    int rc;

    while (1) {
		static struct option long_options[] =
			{
				{"template",      required_argument, 0, 't'},
				{0,0,0,0}
			};
		int option_index = 0;
		int opt;
		opt = getopt_long(argc, argv, "t:", 
						  long_options, &option_index);
		
		if (opt == EOF)
			break;
		
		switch(opt) {
		case 't':
			template = optarg;
			break;
		
		case '?':
			rrd_set_error("unknown option '%s'",argv[optind-1]);
			return(-1);
		}
    }

    /* need at least 2 arguments: filename, data. */
    if (argc-optind < 2) {
		rrd_set_error("Not enough arguments");

		return -1;
    }
 
   	rc = rrd_update_r(argv[optind], template,
		      argc - optind - 1, argv + optind + 1);
    return rc;
}

int
rrd_update_r(char *filename, char *template, int argc, char **argv)
{
   return _rrd_update(filename, template, argc, argv, NULL);
}

int
_rrd_update(char *filename, char *template, int argc, char **argv, 
   info_t *pcdp_summary)
{

    int              arg_i = 2;
    short            j;
    unsigned long    i,ii,iii=1;

    unsigned long    rra_begin;          /* byte pointer to the rra
					  * area in the rrd file.  this
					  * pointer never changes value */
    unsigned long    rra_start;          /* byte pointer to the rra
					  * area in the rrd file.  this
					  * pointer changes as each rrd is
					  * processed. */
    unsigned long    rra_current;        /* byte pointer to the current write
					  * spot in the rrd file. */
    unsigned long    rra_pos_tmp;        /* temporary byte pointer. */
    double           interval,
	pre_int,post_int;                /* interval between this and
					  * the last run */
    unsigned long    proc_pdp_st;        /* which pdp_st was the last
					  * to be processed */
    unsigned long    occu_pdp_st;        /* when was the pdp_st
					  * before the last update
					  * time */
    unsigned long    proc_pdp_age;       /* how old was the data in
					  * the pdp prep area when it
					  * was last updated */
    unsigned long    occu_pdp_age;       /* how long ago was the last
					  * pdp_step time */
    rrd_value_t      *pdp_new;           /* prepare the incoming data
					  * to be added the the
					  * existing entry */
    rrd_value_t      *pdp_temp;          /* prepare the pdp values 
					  * to be added the the
					  * cdp values */

    long             *tmpl_idx;          /* index representing the settings
					    transported by the template index */
    unsigned long    tmpl_cnt = 2;       /* time and data */

    FILE             *rrd_file;
    rrd_t            rrd;
    time_t           current_time;
	time_t           rra_time; /* time of update for a RRA */
    unsigned long    current_time_usec;  /* microseconds part of current time */
    struct timeval   tmp_time;           /* used for time conversion */

    char             **updvals;
    int              schedule_smooth = 0;
	rrd_value_t      *seasonal_coef = NULL, *last_seasonal_coef = NULL;
					 /* a vector of future Holt-Winters seasonal coefs */
    unsigned long    elapsed_pdp_st;
					 /* number of elapsed PDP steps since last update */
    unsigned long    *rra_step_cnt = NULL;
					 /* number of rows to be updated in an RRA for a data
					  * value. */
    unsigned long    start_pdp_offset;
					 /* number of PDP steps since the last update that
					  * are assigned to the first CDP to be generated
					  * since the last update. */
    unsigned short   scratch_idx;
					 /* index into the CDP scratch array */
    enum cf_en       current_cf;
					 /* numeric id of the current consolidation function */
    rpnstack_t       rpnstack; /* used for COMPUTE DS */
    int		     version;  /* rrd version */
    char             *endptr; /* used in the conversion */
#ifdef HAVE_MMAP
    void	     *rrd_mmaped_file;
    unsigned long    rrd_filesize;
#endif

    rpnstack_init(&rpnstack);

    /* need at least 1 arguments: data. */
    if (argc < 1) {
	rrd_set_error("Not enough arguments");
	return -1;
    }
    
    

    if(rrd_open(filename,&rrd_file,&rrd, RRD_READWRITE)==-1){
	return -1;
    }
    /* initialize time */
    version = atoi(rrd.stat_head->version);
    gettimeofday(&tmp_time, 0);
    normalize_time(&tmp_time);
    current_time = tmp_time.tv_sec;
    if(version >= 3) {
        current_time_usec = tmp_time.tv_usec;
    }
    else {
	current_time_usec = 0;
    }

    rra_current = rra_start = rra_begin = ftell(rrd_file);
    /* This is defined in the ANSI C standard, section 7.9.5.3:

        When a file is opened with udpate mode ('+' as the second
        or third character in the ... list of mode argument
        variables), both input and ouptut may be performed on the
        associated stream.  However, ...  input may not be directly
        followed by output without an intervening call to a file
        positioning function, unless the input oepration encounters
        end-of-file. */
#ifdef HAVE_MMAP
    fseek(rrd_file, 0, SEEK_END);
    rrd_filesize = ftell(rrd_file);
    fseek(rrd_file, rra_current, SEEK_SET);
#else
    fseek(rrd_file, 0, SEEK_CUR);
#endif

    
    /* get exclusive lock to whole file.
     * lock gets removed when we close the file.
     */
    if (LockRRD(rrd_file) != 0) {
      rrd_set_error("could not lock RRD");
      rrd_free(&rrd);
      fclose(rrd_file);
      return(-1);   
    } 

    if((updvals = malloc( sizeof(char*) * (rrd.stat_head->ds_cnt+1)))==NULL){
	rrd_set_error("allocating updvals pointer array");
	rrd_free(&rrd);
        fclose(rrd_file);
	return(-1);
    }

    if ((pdp_temp = malloc(sizeof(rrd_value_t)
			   *rrd.stat_head->ds_cnt))==NULL){
	rrd_set_error("allocating pdp_temp ...");
	free(updvals);
	rrd_free(&rrd);
        fclose(rrd_file);
	return(-1);
    }

    if ((tmpl_idx = malloc(sizeof(unsigned long)
			   *(rrd.stat_head->ds_cnt+1)))==NULL){
	rrd_set_error("allocating tmpl_idx ...");
	free(pdp_temp);
	free(updvals);
	rrd_free(&rrd);
        fclose(rrd_file);
	return(-1);
    }
    /* initialize template redirector */
    /* default config example (assume DS 1 is a CDEF DS)
       tmpl_idx[0] -> 0; (time)
       tmpl_idx[1] -> 1; (DS 0)
       tmpl_idx[2] -> 3; (DS 2)
       tmpl_idx[3] -> 4; (DS 3) */
    tmpl_idx[0] = 0; /* time */
    for (i = 1, ii = 1 ; i <= rrd.stat_head->ds_cnt ; i++) 
	{
	   if (dst_conv(rrd.ds_def[i-1].dst) != DST_CDEF)
	      tmpl_idx[ii++]=i;
	}
    tmpl_cnt= ii;

    if (template) {
	char *dsname;
	unsigned int tmpl_len;
	dsname = template;
	tmpl_cnt = 1; /* the first entry is the time */
	tmpl_len = strlen(template);
	for(i=0;i<=tmpl_len ;i++) {
	    if (template[i] == ':' || template[i] == '\0') {
		template[i] = '\0';
		if (tmpl_cnt>rrd.stat_head->ds_cnt){
  		    rrd_set_error("Template contains more DS definitions than RRD");
		    free(updvals); free(pdp_temp);
		    free(tmpl_idx); rrd_free(&rrd);
		    fclose(rrd_file); return(-1);
		}
		if ((tmpl_idx[tmpl_cnt++] = ds_match(&rrd,dsname)) == -1){
  		    rrd_set_error("unknown DS name '%s'",dsname);
		    free(updvals); free(pdp_temp);
		    free(tmpl_idx); rrd_free(&rrd);
		    fclose(rrd_file); return(-1);
		} else {
		  /* the first element is always the time */
		  tmpl_idx[tmpl_cnt-1]++; 
		  /* go to the next entry on the template */
		  dsname = &template[i+1];
                  /* fix the damage we did before */
                  if (i<tmpl_len) {
                     template[i]=':';
                  } 

		}
	    }	    
	}
    }
    if ((pdp_new = malloc(sizeof(rrd_value_t)
			  *rrd.stat_head->ds_cnt))==NULL){
	rrd_set_error("allocating pdp_new ...");
	free(updvals);
	free(pdp_temp);
	free(tmpl_idx);
	rrd_free(&rrd);
        fclose(rrd_file);
	return(-1);
    }

#ifdef HAVE_MMAP
    rrd_mmaped_file = mmap(0, 
		    	rrd_filesize, 
			PROT_READ | PROT_WRITE, 
			MAP_SHARED, 
			fileno(rrd_file), 
			0);
    if (rrd_mmaped_file == MAP_FAILED) {
        rrd_set_error("error mmapping file %s", filename);
	free(updvals);
	free(pdp_temp);
	free(tmpl_idx);
	rrd_free(&rrd);
        fclose(rrd_file);
	return(-1);
    }
#endif
    /* loop through the arguments. */
    for(arg_i=0; arg_i<argc;arg_i++) {
	char *stepper = malloc((strlen(argv[arg_i])+1)*sizeof(char));
        char *step_start = stepper;
	char *p;
	char *parsetime_error = NULL;
	enum {atstyle, normal} timesyntax;
	struct rrd_time_value ds_tv;
        if (stepper == NULL){
                rrd_set_error("failed duplication argv entry");
                free(updvals);
                free(pdp_temp);  
                free(tmpl_idx);
                rrd_free(&rrd);
#ifdef HAVE_MMAP
    		munmap(rrd_mmaped_file, rrd_filesize);
#endif
                fclose(rrd_file);
                return(-1);
         }
	/* initialize all ds input to unknown except the first one
           which has always got to be set */
	for(ii=1;ii<=rrd.stat_head->ds_cnt;ii++) updvals[ii] = "U";
	strcpy(stepper,argv[arg_i]);
	updvals[0]=stepper;
	/* separate all ds elements; first must be examined separately
	   due to alternate time syntax */
	if ((p=strchr(stepper,'@'))!=NULL) {
	    timesyntax = atstyle;
	    *p = '\0';
	    stepper = p+1;
	} else if ((p=strchr(stepper,':'))!=NULL) {
	    timesyntax = normal;
	    *p = '\0';
	    stepper = p+1;
	} else {
	    rrd_set_error("expected timestamp not found in data source from %s:...",
			  argv[arg_i]);
	    free(step_start);
	    break;
	}
	ii=1;
	updvals[tmpl_idx[ii]] = stepper;
	while (*stepper) {
	    if (*stepper == ':') {
		*stepper = '\0';
		ii++;
		if (ii<tmpl_cnt){		    
		    updvals[tmpl_idx[ii]] = stepper+1;
		}
	    }
	    stepper++;
	}

	if (ii != tmpl_cnt-1) {
	    rrd_set_error("expected %lu data source readings (got %lu) from %s:...",
			  tmpl_cnt-1, ii, argv[arg_i]);
	    free(step_start);
	    break;
	}
	
        /* get the time from the reading ... handle N */
	if (timesyntax == atstyle) {
            if ((parsetime_error = parsetime(updvals[0], &ds_tv))) {
                rrd_set_error("ds time: %s: %s", updvals[0], parsetime_error );
		free(step_start);
		break;
	    }
	    if (ds_tv.type == RELATIVE_TO_END_TIME ||
		ds_tv.type == RELATIVE_TO_START_TIME) {
		rrd_set_error("specifying time relative to the 'start' "
			      "or 'end' makes no sense here: %s",
			      updvals[0]);
		free(step_start);
		break;
	    }

	    current_time = mktime(&ds_tv.tm) + ds_tv.offset;
	    current_time_usec = 0; /* FIXME: how to handle usecs here ? */
	    
	} else if (strcmp(updvals[0],"N")==0){
	    gettimeofday(&tmp_time, 0);
	    normalize_time(&tmp_time);
	    current_time = tmp_time.tv_sec;
	    current_time_usec = tmp_time.tv_usec;
	} else {
	    double tmp;
	    tmp = strtod(updvals[0], 0);
	    current_time = floor(tmp);
	    current_time_usec = (long)((tmp - current_time) * 1000000L);
	}
	/* dont do any correction for old version RRDs */
	if(version < 3) 
	    current_time_usec = 0;
	
	if(current_time <= rrd.live_head->last_up){
	    rrd_set_error("illegal attempt to update using time %ld when "
			  "last update time is %ld (minimum one second step)",
			  current_time, rrd.live_head->last_up);
	    free(step_start);
	    break;
	}
	
	
	/* seek to the beginning of the rra's */
	if (rra_current != rra_begin) {
#ifndef HAVE_MMAP
	    if(fseek(rrd_file, rra_begin, SEEK_SET) != 0) {
		rrd_set_error("seek error in rrd");
		free(step_start);
		break;
	    }
#endif
	    rra_current = rra_begin;
	}
	rra_start = rra_begin;

	/* when was the current pdp started */
	proc_pdp_age = rrd.live_head->last_up % rrd.stat_head->pdp_step;
	proc_pdp_st = rrd.live_head->last_up - proc_pdp_age;

	/* when did the last pdp_st occur */
	occu_pdp_age = current_time % rrd.stat_head->pdp_step;
	occu_pdp_st = current_time - occu_pdp_age;
	/* interval = current_time - rrd.live_head->last_up; */
	interval    = current_time + ((double)current_time_usec - (double)rrd.live_head->last_up_usec)/1000000.0 - rrd.live_head->last_up;
    
	if (occu_pdp_st > proc_pdp_st){
	    /* OK we passed the pdp_st moment*/
	    pre_int =  occu_pdp_st - rrd.live_head->last_up; /* how much of the input data
							      * occurred before the latest
							      * pdp_st moment*/
	    pre_int -= ((double)rrd.live_head->last_up_usec)/1000000.0; /* adjust usecs */
	    post_int = occu_pdp_age;			     /* how much after it */
	    post_int += ((double)current_time_usec)/1000000.0;  /* adjust usecs */
	} else {
	    pre_int = interval;
	    post_int = 0;
	}

#ifdef DEBUG
	printf(
	       "proc_pdp_age %lu\t"
	       "proc_pdp_st %lu\t" 
	       "occu_pfp_age %lu\t" 
	       "occu_pdp_st %lu\t"
	       "int %lf\t"
	       "pre_int %lf\t"
	       "post_int %lf\n", proc_pdp_age, proc_pdp_st, 
		occu_pdp_age, occu_pdp_st,
	       interval, pre_int, post_int);
#endif
    
	/* process the data sources and update the pdp_prep 
	 * area accordingly */
	for(i=0;i<rrd.stat_head->ds_cnt;i++){
	    enum dst_en dst_idx;
	    dst_idx= dst_conv(rrd.ds_def[i].dst);
		/* NOTE: DST_CDEF should never enter this if block, because
		 * updvals[i+1][0] is initialized to 'U'; unless the caller
		 * accidently specified a value for the DST_CDEF. To handle 
		 * this case, an extra check is required. */
	    if((updvals[i+1][0] != 'U') &&
		   (dst_idx != DST_CDEF) &&
	       rrd.ds_def[i].par[DS_mrhb_cnt].u_cnt >= interval) {
	       double rate = DNAN;
	       /* the data source type defines how to process the data */
		/* pdp_new contains rate * time ... eg the bytes
		 * transferred during the interval. Doing it this way saves
		 * a lot of math operations */
		

		switch(dst_idx){
		case DST_COUNTER:
		case DST_DERIVE:
		    if(rrd.pdp_prep[i].last_ds[0] != 'U'){
                      for(ii=0;updvals[i+1][ii] != '\0';ii++){
                            if(updvals[i+1][ii] < '0' || updvals[i+1][ii] > '9' || (ii==0 && updvals[i+1][ii] == '-')){
                                 rrd_set_error("not a simple integer: '%s'",updvals[i+1]);
                                 break;
                            }
                       }
                       if (rrd_test_error()){
                            break;
                       }
		       pdp_new[i]= rrd_diff(updvals[i+1],rrd.pdp_prep[i].last_ds);
		       if(dst_idx == DST_COUNTER) {
			  /* simple overflow catcher suggested by Andres Kroonmaa */
			  /* this will fail terribly for non 32 or 64 bit counters ... */
			  /* are there any others in SNMP land ? */
			  if (pdp_new[i] < (double)0.0 ) 
			    pdp_new[i] += (double)4294967296.0 ;  /* 2^32 */
			  if (pdp_new[i] < (double)0.0 ) 
			    pdp_new[i] += (double)18446744069414584320.0; /* 2^64-2^32 */;
		       }
		       rate = pdp_new[i] / interval;
		    }
		   else {
		     pdp_new[i]= DNAN;		
		   }
		   break;
		case DST_ABSOLUTE:
                    errno = 0;
                    pdp_new[i] = strtod(updvals[i+1],&endptr);
                    if (errno > 0){
                        rrd_set_error("converting  '%s' to float: %s",updvals[i+1],rrd_strerror(errno));
                        break;
                    };
                    if (endptr[0] != '\0'){
                        rrd_set_error("conversion of '%s' to float not complete: tail '%s'",updvals[i+1],endptr);
                        break;
                    }
		    rate = pdp_new[i] / interval;		  
		    break;
		case DST_GAUGE:
                    errno = 0;
                    pdp_new[i] = strtod(updvals[i+1],&endptr) * interval;
                    if (errno > 0){
                        rrd_set_error("converting  '%s' to float: %s",updvals[i+1],rrd_strerror(errno));
                        break;
                    };
                    if (endptr[0] != '\0'){
                        rrd_set_error("conversion of '%s' to float not complete: tail '%s'",updvals[i+1],endptr);
                        break;
                    }
		    rate = pdp_new[i] / interval;		   
		    break;
		default:
		    rrd_set_error("rrd contains unknown DS type : '%s'",
				  rrd.ds_def[i].dst);
		    break;
		}
		/* break out of this for loop if the error string is set */
		if (rrd_test_error()){
		    break;
		}
	       /* make sure pdp_temp is neither too large or too small
		* if any of these occur it becomes unknown ...
		* sorry folks ... */
	       if ( ! isnan(rate) && 
	            (( ! isnan(rrd.ds_def[i].par[DS_max_val].u_val) &&
	                 rate > rrd.ds_def[i].par[DS_max_val].u_val ) ||     
	            ( ! isnan(rrd.ds_def[i].par[DS_min_val].u_val) &&
	                rate < rrd.ds_def[i].par[DS_min_val].u_val ))){
		  pdp_new[i] = DNAN;
	       }	       
	    } else {
		/* no news is news all the same */
		pdp_new[i] = DNAN;
	    }
	    
	    /* make a copy of the command line argument for the next run */
#ifdef DEBUG
	    fprintf(stderr,
		    "prep ds[%lu]\t"
		    "last_arg '%s'\t"
		    "this_arg '%s'\t"
		    "pdp_new %10.2f\n",
		    i,
		    rrd.pdp_prep[i].last_ds,
		    updvals[i+1], pdp_new[i]);
#endif
	    if(dst_idx == DST_COUNTER || dst_idx == DST_DERIVE){
		strncpy(rrd.pdp_prep[i].last_ds,
			updvals[i+1],LAST_DS_LEN-1);
		rrd.pdp_prep[i].last_ds[LAST_DS_LEN-1]='\0';
	    }
	}
	/* break out of the argument parsing loop if the error_string is set */
	if (rrd_test_error()){
	    free(step_start);
	    break;
	}
	/* has a pdp_st moment occurred since the last run ? */

	if (proc_pdp_st == occu_pdp_st){
	    /* no we have not passed a pdp_st moment. therefore update is simple */

	    for(i=0;i<rrd.stat_head->ds_cnt;i++){
		if(isnan(pdp_new[i]))
		    rrd.pdp_prep[i].scratch[PDP_unkn_sec_cnt].u_cnt += interval;
		else
		    rrd.pdp_prep[i].scratch[PDP_val].u_val+= pdp_new[i];
#ifdef DEBUG
		fprintf(stderr,
			"NO PDP  ds[%lu]\t"
			"value %10.2f\t"
			"unkn_sec %5lu\n",
			i,
			rrd.pdp_prep[i].scratch[PDP_val].u_val,
			rrd.pdp_prep[i].scratch[PDP_unkn_sec_cnt].u_cnt);
#endif
	    }	
	} else {
	    /* an pdp_st has occurred. */

	    /* in pdp_prep[].scratch[PDP_val].u_val we have collected rate*seconds which 
	     * occurred up to the last run. 	   
	    pdp_new[] contains rate*seconds from the latest run.
	    pdp_temp[] will contain the rate for cdp */

	    for(i=0;i<rrd.stat_head->ds_cnt;i++){
		/* update pdp_prep to the current pdp_st */
		if(isnan(pdp_new[i]))
		    rrd.pdp_prep[i].scratch[PDP_unkn_sec_cnt].u_cnt += pre_int;
		else
		    rrd.pdp_prep[i].scratch[PDP_val].u_val += 
			pdp_new[i]/(double)interval*(double)pre_int;

		/* if too much of the pdp_prep is unknown we dump it */
		if ((rrd.pdp_prep[i].scratch[PDP_unkn_sec_cnt].u_cnt 
		     > rrd.ds_def[i].par[DS_mrhb_cnt].u_cnt) ||
		    (occu_pdp_st-proc_pdp_st <= 
		     rrd.pdp_prep[i].scratch[PDP_unkn_sec_cnt].u_cnt)) {
		    pdp_temp[i] = DNAN;
		} else {
		    pdp_temp[i] = rrd.pdp_prep[i].scratch[PDP_val].u_val
			/ (double)( occu_pdp_st
				   - proc_pdp_st
				   - rrd.pdp_prep[i].scratch[PDP_unkn_sec_cnt].u_cnt);
		}

		/* process CDEF data sources; remember each CDEF DS can
		 * only reference other DS with a lower index number */
	    if (dst_conv(rrd.ds_def[i].dst) == DST_CDEF) {
		   rpnp_t *rpnp;
		   rpnp = rpn_expand((rpn_cdefds_t *) &(rrd.ds_def[i].par[DS_cdef]));
		   /* substitue data values for OP_VARIABLE nodes */
		   for (ii = 0; rpnp[ii].op != OP_END; ii++)
		   {
			  if (rpnp[ii].op == OP_VARIABLE) {
				 rpnp[ii].op = OP_NUMBER;
				 rpnp[ii].val =  pdp_temp[rpnp[ii].ptr];
			  }
		   }
		   /* run the rpn calculator */
		   if (rpn_calc(rpnp,&rpnstack,0,pdp_temp,i) == -1) {
			  free(rpnp);
			  break; /* exits the data sources pdp_temp loop */
		   }
		}
        
		/* make pdp_prep ready for the next run */
		if(isnan(pdp_new[i])){
		    rrd.pdp_prep[i].scratch[PDP_unkn_sec_cnt].u_cnt = post_int;
		    rrd.pdp_prep[i].scratch[PDP_val].u_val = 0.0;
		} else {
		    rrd.pdp_prep[i].scratch[PDP_unkn_sec_cnt].u_cnt = 0;
		    rrd.pdp_prep[i].scratch[PDP_val].u_val = 
			pdp_new[i]/(double)interval*(double)post_int;
		}

#ifdef DEBUG
		fprintf(stderr,
			"PDP UPD ds[%lu]\t"
			"pdp_temp %10.2f\t"
			"new_prep %10.2f\t"
			"new_unkn_sec %5lu\n",
			i, pdp_temp[i],
			rrd.pdp_prep[i].scratch[PDP_val].u_val,
			rrd.pdp_prep[i].scratch[PDP_unkn_sec_cnt].u_cnt);
#endif
	    }

		/* if there were errors during the last loop, bail out here */
	    if (rrd_test_error()){
	       free(step_start);
	       break;
	    }

		/* compute the number of elapsed pdp_st moments */
		elapsed_pdp_st = (occu_pdp_st - proc_pdp_st) / rrd.stat_head -> pdp_step;
#ifdef DEBUG
		fprintf(stderr,"elapsed PDP steps: %lu\n", elapsed_pdp_st);
#endif
		if (rra_step_cnt == NULL)
		{
		   rra_step_cnt = (unsigned long *) 
			  malloc((rrd.stat_head->rra_cnt)* sizeof(unsigned long));
		}

	    for(i = 0, rra_start = rra_begin;
		i < rrd.stat_head->rra_cnt;
	    rra_start += rrd.rra_def[i].row_cnt * rrd.stat_head -> ds_cnt * sizeof(rrd_value_t),
		i++)
		{
		current_cf = cf_conv(rrd.rra_def[i].cf_nam);
		start_pdp_offset = rrd.rra_def[i].pdp_cnt -
		   (proc_pdp_st / rrd.stat_head -> pdp_step) % rrd.rra_def[i].pdp_cnt;
        if (start_pdp_offset <= elapsed_pdp_st) {
           rra_step_cnt[i] = (elapsed_pdp_st - start_pdp_offset) / 
		      rrd.rra_def[i].pdp_cnt + 1;
	    } else {
		   rra_step_cnt[i] = 0;
		}

		if (current_cf == CF_SEASONAL || current_cf == CF_DEVSEASONAL) 
		{
		   /* If this is a bulk update, we need to skip ahead in the seasonal
			* arrays so that they will be correct for the next observed value;
			* note that for the bulk update itself, no update will occur to
			* DEVSEASONAL or SEASONAL; futhermore, HWPREDICT and DEVPREDICT will
			* be set to DNAN. */
           if (rra_step_cnt[i] > 2) 
		   {
			  /* skip update by resetting rra_step_cnt[i],
			   * note that this is not data source specific; this is due
			   * to the bulk update, not a DNAN value for the specific data
			   * source. */
			  rra_step_cnt[i] = 0;
              lookup_seasonal(&rrd,i,rra_start,rrd_file,elapsed_pdp_st, 
			     &last_seasonal_coef);
		      lookup_seasonal(&rrd,i,rra_start,rrd_file,elapsed_pdp_st + 1,
			     &seasonal_coef);
		   }
		
		  /* periodically run a smoother for seasonal effects */
		  /* Need to use first cdp parameter buffer to track
		   * burnin (burnin requires a specific smoothing schedule).
		   * The CDP_init_seasonal parameter is really an RRA level,
		   * not a data source within RRA level parameter, but the rra_def
		   * is read only for rrd_update (not flushed to disk). */
		  iii = i*(rrd.stat_head -> ds_cnt);
		  if (rrd.cdp_prep[iii].scratch[CDP_init_seasonal].u_cnt 
			  <= BURNIN_CYCLES)
		  {
		     if (rrd.rra_ptr[i].cur_row + elapsed_pdp_st 
				 > rrd.rra_def[i].row_cnt - 1) {
			   /* mark off one of the burnin cycles */
			   ++(rrd.cdp_prep[iii].scratch[CDP_init_seasonal].u_cnt);
		       schedule_smooth = 1;
			 }  
		  } else {
			 /* someone has no doubt invented a trick to deal with this
			  * wrap around, but at least this code is clear. */
			 if (rrd.rra_def[i].par[RRA_seasonal_smooth_idx].u_cnt >
			     rrd.rra_ptr[i].cur_row)
			 {
				 /* here elapsed_pdp_st = rra_step_cnt[i] because of 1-1
				  * mapping between PDP and CDP */
				 if (rrd.rra_ptr[i].cur_row + elapsed_pdp_st
					>= rrd.rra_def[i].par[RRA_seasonal_smooth_idx].u_cnt)
				 {
#ifdef DEBUG
					fprintf(stderr,
					"schedule_smooth 1: cur_row %lu, elapsed_pdp_st %lu, smooth idx %lu\n",
                    rrd.rra_ptr[i].cur_row, elapsed_pdp_st, 
					rrd.rra_def[i].par[RRA_seasonal_smooth_idx].u_cnt);
#endif
					schedule_smooth = 1;
				 }
             } else {
				 /* can't rely on negative numbers because we are working with
				  * unsigned values */
				 /* Don't need modulus here. If we've wrapped more than once, only
				  * one smooth is executed at the end. */
				 if (rrd.rra_ptr[i].cur_row + elapsed_pdp_st >= rrd.rra_def[i].row_cnt
					&& rrd.rra_ptr[i].cur_row + elapsed_pdp_st - rrd.rra_def[i].row_cnt
					>= rrd.rra_def[i].par[RRA_seasonal_smooth_idx].u_cnt)
				 {
#ifdef DEBUG
					fprintf(stderr,
					"schedule_smooth 2: cur_row %lu, elapsed_pdp_st %lu, smooth idx %lu\n",
                    rrd.rra_ptr[i].cur_row, elapsed_pdp_st, 
					rrd.rra_def[i].par[RRA_seasonal_smooth_idx].u_cnt);
#endif
					schedule_smooth = 1;
				 }
			 }
		  }

	      rra_current = ftell(rrd_file); 
		} /* if cf is DEVSEASONAL or SEASONAL */

        if (rrd_test_error()) break;

		    /* update CDP_PREP areas */
		    /* loop over data soures within each RRA */
		    for(ii = 0;
			ii < rrd.stat_head->ds_cnt;
			ii++)
			{
			
			/* iii indexes the CDP prep area for this data source within the RRA */
			iii=i*rrd.stat_head->ds_cnt+ii;

			if (rrd.rra_def[i].pdp_cnt > 1) {
			  
			   if (rra_step_cnt[i] > 0) {
			   /* If we are in this block, as least 1 CDP value will be written to
				* disk, this is the CDP_primary_val entry. If more than 1 value needs
				* to be written, then the "fill in" value is the CDP_secondary_val
				* entry. */
				  if (isnan(pdp_temp[ii]))
                  {
					 rrd.cdp_prep[iii].scratch[CDP_unkn_pdp_cnt].u_cnt += start_pdp_offset;
					 rrd.cdp_prep[iii].scratch[CDP_secondary_val].u_val = DNAN;
				  } else {
					 /* CDP_secondary value is the RRA "fill in" value for intermediary
					  * CDP data entries. No matter the CF, the value is the same because
					  * the average, max, min, and last of a list of identical values is
					  * the same, namely, the value itself. */
					 rrd.cdp_prep[iii].scratch[CDP_secondary_val].u_val = pdp_temp[ii];
				  }
                     
				  if (rrd.cdp_prep[iii].scratch[CDP_unkn_pdp_cnt].u_cnt
				      > rrd.rra_def[i].pdp_cnt*
				      rrd.rra_def[i].par[RRA_cdp_xff_val].u_val)
				  {
					 rrd.cdp_prep[iii].scratch[CDP_primary_val].u_val = DNAN;
					 /* initialize carry over */
					 if (current_cf == CF_AVERAGE) {
						   if (isnan(pdp_temp[ii])) { 
							  rrd.cdp_prep[iii].scratch[CDP_val].u_val = DNAN;
						   } else {
							  rrd.cdp_prep[iii].scratch[CDP_val].u_val = pdp_temp[ii] *
								 ((elapsed_pdp_st - start_pdp_offset) % rrd.rra_def[i].pdp_cnt);
						   }
					 } else {
						rrd.cdp_prep[iii].scratch[CDP_val].u_val = pdp_temp[ii];
					 }
				  } else {
					 rrd_value_t cum_val, cur_val; 
				     switch (current_cf) {
						case CF_AVERAGE:
						  cum_val = IFDNAN(rrd.cdp_prep[iii].scratch[CDP_val].u_val, 0.0);
						  cur_val = IFDNAN(pdp_temp[ii],0.0);
                          rrd.cdp_prep[iii].scratch[CDP_primary_val].u_val =
					       (cum_val + cur_val * start_pdp_offset) /
				           (rrd.rra_def[i].pdp_cnt
					       -rrd.cdp_prep[iii].scratch[CDP_unkn_pdp_cnt].u_cnt);
						   /* initialize carry over value */
						   if (isnan(pdp_temp[ii])) { 
							  rrd.cdp_prep[iii].scratch[CDP_val].u_val = DNAN;
						   } else {
							  rrd.cdp_prep[iii].scratch[CDP_val].u_val = pdp_temp[ii] *
								 ((elapsed_pdp_st - start_pdp_offset) % rrd.rra_def[i].pdp_cnt);
						   }
						   break;
						case CF_MAXIMUM:
						  cum_val = IFDNAN(rrd.cdp_prep[iii].scratch[CDP_val].u_val, -DINF);
						  cur_val = IFDNAN(pdp_temp[ii],-DINF);
#ifdef DEBUG
						  if (isnan(rrd.cdp_prep[iii].scratch[CDP_val].u_val) &&
							  isnan(pdp_temp[ii])) {
						     fprintf(stderr,
								"RRA %lu, DS %lu, both CDP_val and pdp_temp are DNAN!",
								i,ii);
							 exit(-1);
						  }
#endif
						  if (cur_val > cum_val)
							 rrd.cdp_prep[iii].scratch[CDP_primary_val].u_val = cur_val;
						  else
							 rrd.cdp_prep[iii].scratch[CDP_primary_val].u_val = cum_val;
						  /* initialize carry over value */
						  rrd.cdp_prep[iii].scratch[CDP_val].u_val = pdp_temp[ii];
						  break;
						case CF_MINIMUM:
						  cum_val = IFDNAN(rrd.cdp_prep[iii].scratch[CDP_val].u_val, DINF);
						  cur_val = IFDNAN(pdp_temp[ii],DINF);
#ifdef DEBUG
						  if (isnan(rrd.cdp_prep[iii].scratch[CDP_val].u_val) &&
							  isnan(pdp_temp[ii])) {
						     fprintf(stderr,
								"RRA %lu, DS %lu, both CDP_val and pdp_temp are DNAN!",
								i,ii);
							 exit(-1);
						  }
#endif
						  if (cur_val < cum_val)
							 rrd.cdp_prep[iii].scratch[CDP_primary_val].u_val = cur_val;
						  else
							 rrd.cdp_prep[iii].scratch[CDP_primary_val].u_val = cum_val;
						  /* initialize carry over value */
						  rrd.cdp_prep[iii].scratch[CDP_val].u_val = pdp_temp[ii];
						  break;
						case CF_LAST:
						default:
						   rrd.cdp_prep[iii].scratch[CDP_primary_val].u_val = pdp_temp[ii];
						   /* initialize carry over value */
						   rrd.cdp_prep[iii].scratch[CDP_val].u_val = pdp_temp[ii];
						break;
					 }
				  } /* endif meets xff value requirement for a valid value */
				  /* initialize carry over CDP_unkn_pdp_cnt, this must after CDP_primary_val
				   * is set because CDP_unkn_pdp_cnt is required to compute that value. */
				  if (isnan(pdp_temp[ii]))
					 rrd.cdp_prep[iii].scratch[CDP_unkn_pdp_cnt].u_cnt = 
						(elapsed_pdp_st - start_pdp_offset) % rrd.rra_def[i].pdp_cnt;
				  else
					 rrd.cdp_prep[iii].scratch[CDP_unkn_pdp_cnt].u_cnt = 0;
               } else  /* rra_step_cnt[i]  == 0 */
			   {
#ifdef DEBUG
				  if (isnan(rrd.cdp_prep[iii].scratch[CDP_val].u_val)) {
				  fprintf(stderr,"schedule CDP_val update, RRA %lu DS %lu, DNAN\n",
					 i,ii);
				  } else {
				  fprintf(stderr,"schedule CDP_val update, RRA %lu DS %lu, %10.2f\n",
					 i,ii,rrd.cdp_prep[iii].scratch[CDP_val].u_val);
				  }
#endif
				  if (isnan(pdp_temp[ii])) {
			     	 rrd.cdp_prep[iii].scratch[CDP_unkn_pdp_cnt].u_cnt += elapsed_pdp_st;
				  } else if (isnan(rrd.cdp_prep[iii].scratch[CDP_val].u_val))
				  {
					 if (current_cf == CF_AVERAGE) {
					    rrd.cdp_prep[iii].scratch[CDP_val].u_val = pdp_temp[ii] *
						   elapsed_pdp_st;
					 } else {
					    rrd.cdp_prep[iii].scratch[CDP_val].u_val = pdp_temp[ii];
					 }
#ifdef DEBUG
					 fprintf(stderr,"Initialize CDP_val for RRA %lu DS %lu: %10.2f\n",
					    i,ii,rrd.cdp_prep[iii].scratch[CDP_val].u_val);
#endif
				  } else {
					 switch (current_cf) {
					 case CF_AVERAGE:
					    rrd.cdp_prep[iii].scratch[CDP_val].u_val += pdp_temp[ii] *
						   elapsed_pdp_st;
						break;
					 case CF_MINIMUM:
						if (pdp_temp[ii] < rrd.cdp_prep[iii].scratch[CDP_val].u_val)
						   rrd.cdp_prep[iii].scratch[CDP_val].u_val = pdp_temp[ii];
						break; 
					 case CF_MAXIMUM:
						if (pdp_temp[ii] > rrd.cdp_prep[iii].scratch[CDP_val].u_val)
						   rrd.cdp_prep[iii].scratch[CDP_val].u_val = pdp_temp[ii];
						break; 
					 case CF_LAST:
					 default:
						rrd.cdp_prep[iii].scratch[CDP_val].u_val = pdp_temp[ii];
						break;
					 }
				  }
			   }
			} else { /* rrd.rra_def[i].pdp_cnt == 1 */
			   if (elapsed_pdp_st > 2)
			   {
				   switch (current_cf) {
				   case CF_AVERAGE:
				   default:
			          rrd.cdp_prep[iii].scratch[CDP_primary_val].u_val=pdp_temp[ii];
			          rrd.cdp_prep[iii].scratch[CDP_secondary_val].u_val=pdp_temp[ii];
					  break;
                   case CF_SEASONAL:
				   case CF_DEVSEASONAL:
					  /* need to update cached seasonal values, so they are consistent
					   * with the bulk update */
                      /* WARNING: code relies on the fact that CDP_hw_last_seasonal and
					   * CDP_last_deviation are the same. */
                      rrd.cdp_prep[iii].scratch[CDP_hw_last_seasonal].u_val =
						 last_seasonal_coef[ii];
					  rrd.cdp_prep[iii].scratch[CDP_hw_seasonal].u_val =
						 seasonal_coef[ii];
					  break;
                   case CF_HWPREDICT:
					  /* need to update the null_count and last_null_count.
					   * even do this for non-DNAN pdp_temp because the
					   * algorithm is not learning from batch updates. */
					  rrd.cdp_prep[iii].scratch[CDP_null_count].u_cnt += 
						 elapsed_pdp_st;
					  rrd.cdp_prep[iii].scratch[CDP_last_null_count].u_cnt += 
						 elapsed_pdp_st - 1;
					  /* fall through */
				   case CF_DEVPREDICT:
			          rrd.cdp_prep[iii].scratch[CDP_primary_val].u_val = DNAN;
			          rrd.cdp_prep[iii].scratch[CDP_secondary_val].u_val = DNAN;
					  break;
                   case CF_FAILURES:
					  /* do not count missed bulk values as failures */
			          rrd.cdp_prep[iii].scratch[CDP_primary_val].u_val = 0;
			          rrd.cdp_prep[iii].scratch[CDP_secondary_val].u_val = 0;
					  /* need to reset violations buffer.
					   * could do this more carefully, but for now, just
					   * assume a bulk update wipes away all violations. */
                      erase_violations(&rrd, iii, i);
					  break;
				   }
			   } 
			} /* endif rrd.rra_def[i].pdp_cnt == 1 */

			if (rrd_test_error()) break;

			} /* endif data sources loop */
        } /* end RRA Loop */

		/* this loop is only entered if elapsed_pdp_st < 3 */
		for (j = elapsed_pdp_st, scratch_idx = CDP_primary_val; 
			 j > 0 && j < 3; j--, scratch_idx = CDP_secondary_val)
		{
	       for(i = 0, rra_start = rra_begin;
		   i < rrd.stat_head->rra_cnt;
	       rra_start += rrd.rra_def[i].row_cnt * rrd.stat_head -> ds_cnt * sizeof(rrd_value_t),
		   i++)
		   {
			  if (rrd.rra_def[i].pdp_cnt > 1) continue;

	          current_cf = cf_conv(rrd.rra_def[i].cf_nam);
			  if (current_cf == CF_SEASONAL || current_cf == CF_DEVSEASONAL)
			  {
		         lookup_seasonal(&rrd,i,rra_start,rrd_file,
				    elapsed_pdp_st + (scratch_idx == CDP_primary_val ? 1 : 2),
			        &seasonal_coef);
                 rra_current = ftell(rrd_file);
			  }
			  if (rrd_test_error()) break;
		      /* loop over data soures within each RRA */
		      for(ii = 0;
			  ii < rrd.stat_head->ds_cnt;
			  ii++)
			  {
			     update_aberrant_CF(&rrd,pdp_temp[ii],current_cf,
					i*(rrd.stat_head->ds_cnt) + ii,i,ii,
				    scratch_idx, seasonal_coef);
			  }
           } /* end RRA Loop */
		   if (rrd_test_error()) break;
	    } /* end elapsed_pdp_st loop */

		if (rrd_test_error()) break;

		/* Ready to write to disk */
		/* Move sequentially through the file, writing one RRA at a time.
		 * Note this architecture divorces the computation of CDP with
		 * flushing updated RRA entries to disk. */
	    for(i = 0, rra_start = rra_begin;
		i < rrd.stat_head->rra_cnt;
	    rra_start += rrd.rra_def[i].row_cnt * rrd.stat_head -> ds_cnt * sizeof(rrd_value_t),
		i++) {
		/* is there anything to write for this RRA? If not, continue. */
        if (rra_step_cnt[i] == 0) continue;

		/* write the first row */
#ifdef DEBUG
        fprintf(stderr,"  -- RRA Preseek %ld\n",ftell(rrd_file));
#endif
	    rrd.rra_ptr[i].cur_row++;
	    if (rrd.rra_ptr[i].cur_row >= rrd.rra_def[i].row_cnt)
		   rrd.rra_ptr[i].cur_row = 0; /* wrap around */
		/* positition on the first row */
		rra_pos_tmp = rra_start +
		   (rrd.stat_head->ds_cnt)*(rrd.rra_ptr[i].cur_row)*sizeof(rrd_value_t);
		if(rra_pos_tmp != rra_current) {
#ifndef HAVE_MMAP
		   if(fseek(rrd_file, rra_pos_tmp, SEEK_SET) != 0){
		      rrd_set_error("seek error in rrd");
		      break;
		   }
#endif
		   rra_current = rra_pos_tmp;
		}

#ifdef DEBUG
	    fprintf(stderr,"  -- RRA Postseek %ld\n",ftell(rrd_file));
#endif
		scratch_idx = CDP_primary_val;
		if (pcdp_summary != NULL)
		{
		   rra_time = (current_time - current_time 
		   % (rrd.rra_def[i].pdp_cnt*rrd.stat_head->pdp_step))
		   - ((rra_step_cnt[i]-1)*rrd.rra_def[i].pdp_cnt*rrd.stat_head->pdp_step);
		}
#ifdef HAVE_MMAP
		pcdp_summary = write_RRA_row(&rrd, i, &rra_current, scratch_idx, rrd_file, 
		   pcdp_summary, &rra_time, rrd_mmaped_file);
#else
		pcdp_summary = write_RRA_row(&rrd, i, &rra_current, scratch_idx, rrd_file, 
		   pcdp_summary, &rra_time);
#endif
		if (rrd_test_error()) break;

		/* write other rows of the bulk update, if any */
		scratch_idx = CDP_secondary_val;
               for ( ; rra_step_cnt[i] > 1; rra_step_cnt[i]--)
		{
                  if (++rrd.rra_ptr[i].cur_row == rrd.rra_def[i].row_cnt)
		   {
#ifdef DEBUG
              fprintf(stderr,"Wraparound for RRA %s, %lu updates left\n",
			  rrd.rra_def[i].cf_nam, rra_step_cnt[i] - 1);
#endif
			  /* wrap */
			  rrd.rra_ptr[i].cur_row = 0;
			  /* seek back to beginning of current rra */
		      if (fseek(rrd_file, rra_start, SEEK_SET) != 0)
			  {
		         rrd_set_error("seek error in rrd");
		         break;
			  }
#ifdef DEBUG
	          fprintf(stderr,"  -- Wraparound Postseek %ld\n",ftell(rrd_file));
#endif
			  rra_current = rra_start;
		   }
		   if (pcdp_summary != NULL)
		   {
		      rra_time = (current_time - current_time 
		      % (rrd.rra_def[i].pdp_cnt*rrd.stat_head->pdp_step))
		      - ((rra_step_cnt[i]-2)*rrd.rra_def[i].pdp_cnt*rrd.stat_head->pdp_step);
		   }
#ifdef HAVE_MMAP
		   pcdp_summary = write_RRA_row(&rrd, i, &rra_current, scratch_idx, rrd_file,
		      pcdp_summary, &rra_time, rrd_mmaped_file);
#else
		   pcdp_summary = write_RRA_row(&rrd, i, &rra_current, scratch_idx, rrd_file,
		      pcdp_summary, &rra_time);
#endif
		}
		
		if (rrd_test_error())
		  break;
		} /* RRA LOOP */

	    /* break out of the argument parsing loop if error_string is set */
	    if (rrd_test_error()){
		   free(step_start);
		   break;
	    } 
	    
	} /* endif a pdp_st has occurred */ 
	rrd.live_head->last_up = current_time;
	rrd.live_head->last_up_usec = current_time_usec; 
	free(step_start);
    } /* function argument loop */

    if (seasonal_coef != NULL) free(seasonal_coef);
    if (last_seasonal_coef != NULL) free(last_seasonal_coef);
	if (rra_step_cnt != NULL) free(rra_step_cnt);
    rpnstack_free(&rpnstack);

#ifdef HAVE_MMAP
    if (munmap(rrd_mmaped_file, rrd_filesize) == -1) {
            rrd_set_error("error writing(unmapping) file: %s", filename);
    }
#endif    
    /* if we got here and if there is an error and if the file has not been
     * written to, then close things up and return. */
    if (rrd_test_error()) {
	free(updvals);
	free(tmpl_idx);
	rrd_free(&rrd);
	free(pdp_temp);
	free(pdp_new);
	fclose(rrd_file);
	return(-1);
    }

    /* aargh ... that was tough ... so many loops ... anyway, its done.
     * we just need to write back the live header portion now*/

    if (fseek(rrd_file, (sizeof(stat_head_t)
			 + sizeof(ds_def_t)*rrd.stat_head->ds_cnt 
			 + sizeof(rra_def_t)*rrd.stat_head->rra_cnt),
	      SEEK_SET) != 0) {
	rrd_set_error("seek rrd for live header writeback");
	free(updvals);
	free(tmpl_idx);
	rrd_free(&rrd);
	free(pdp_temp);
	free(pdp_new);
	fclose(rrd_file);
	return(-1);
    }

    if(version >= 3) {
	    if(fwrite( rrd.live_head,
		       sizeof(live_head_t), 1, rrd_file) != 1){
		rrd_set_error("fwrite live_head to rrd");
		free(updvals);
		rrd_free(&rrd);
		free(tmpl_idx);
		free(pdp_temp);
		free(pdp_new);
		fclose(rrd_file);
		return(-1);
	    }
    }
    else {
	    if(fwrite( &rrd.live_head->last_up,
		       sizeof(time_t), 1, rrd_file) != 1){
		rrd_set_error("fwrite live_head to rrd");
		free(updvals);
		rrd_free(&rrd);
		free(tmpl_idx);
		free(pdp_temp);
		free(pdp_new);
		fclose(rrd_file);
		return(-1);
	    }
    }
	    

    if(fwrite( rrd.pdp_prep,
	       sizeof(pdp_prep_t),
	       rrd.stat_head->ds_cnt, rrd_file) != rrd.stat_head->ds_cnt){
	rrd_set_error("ftwrite pdp_prep to rrd");
	free(updvals);
	rrd_free(&rrd);
	free(tmpl_idx);
	free(pdp_temp);
	free(pdp_new);
	fclose(rrd_file);
	return(-1);
    }

    if(fwrite( rrd.cdp_prep,
	       sizeof(cdp_prep_t),
	       rrd.stat_head->rra_cnt *rrd.stat_head->ds_cnt, rrd_file) 
       != rrd.stat_head->rra_cnt *rrd.stat_head->ds_cnt){

	rrd_set_error("ftwrite cdp_prep to rrd");
	free(updvals);
	free(tmpl_idx);
	rrd_free(&rrd);
	free(pdp_temp);
	free(pdp_new);
	fclose(rrd_file);
	return(-1);
    }

    if(fwrite( rrd.rra_ptr,
	       sizeof(rra_ptr_t), 
	       rrd.stat_head->rra_cnt,rrd_file) != rrd.stat_head->rra_cnt){
	rrd_set_error("fwrite rra_ptr to rrd");
	free(updvals);
	free(tmpl_idx);
	rrd_free(&rrd);
	free(pdp_temp);
	free(pdp_new);
	fclose(rrd_file);
	return(-1);
    }

    /* OK now close the files and free the memory */
    if(fclose(rrd_file) != 0){
	rrd_set_error("closing rrd");
	free(updvals);
	free(tmpl_idx);
	rrd_free(&rrd);
	free(pdp_temp);
	free(pdp_new);
	return(-1);
    }

    /* calling the smoothing code here guarantees at most
 	 * one smoothing operation per rrd_update call. Unfortunately,
	 * it is possible with bulk updates, or a long-delayed update
	 * for smoothing to occur off-schedule. This really isn't
	 * critical except during the burning cycles. */
	if (schedule_smooth)
	{
#ifndef WIN32
	  rrd_file = fopen(filename,"r+");
#else
	  rrd_file = fopen(filename,"rb+");
#endif
	  rra_start = rra_begin;
	  for (i = 0; i < rrd.stat_head -> rra_cnt; ++i)
	  {
	    if (cf_conv(rrd.rra_def[i].cf_nam) == CF_DEVSEASONAL ||
		cf_conv(rrd.rra_def[i].cf_nam) == CF_SEASONAL)
	    {
#ifdef DEBUG
	      fprintf(stderr,"Running smoother for rra %ld\n",i);
#endif
	      apply_smoother(&rrd,i,rra_start,rrd_file);
	      if (rrd_test_error())
		break;
	    }
	    rra_start += rrd.rra_def[i].row_cnt
	      *rrd.stat_head->ds_cnt*sizeof(rrd_value_t);
	  }
	  fclose(rrd_file);
	}
    rrd_free(&rrd);
    free(updvals);
    free(tmpl_idx);
    free(pdp_new);
    free(pdp_temp);
    return(0);
}

/*
 * get exclusive lock to whole file.
 * lock gets removed when we close the file
 *
 * returns 0 on success
 */
int
LockRRD(FILE *rrdfile)
{
    int	rrd_fd;		/* File descriptor for RRD */
    int	rcstat;

    rrd_fd = fileno(rrdfile);

	{
#ifndef WIN32    
    struct flock	lock;
    lock.l_type = F_WRLCK;    /* exclusive write lock */
    lock.l_len = 0;	      /* whole file */
    lock.l_start = 0;	      /* start of file */
    lock.l_whence = SEEK_SET;   /* end of file */

    rcstat = fcntl(rrd_fd, F_SETLK, &lock);
#else
    struct _stat st;

    if ( _fstat( rrd_fd, &st ) == 0 ) {
	    rcstat = _locking ( rrd_fd, _LK_NBLCK, st.st_size );
    } else {
	    rcstat = -1;
    }
#endif
	}

    return(rcstat);
}


#ifdef HAVE_MMAP
info_t
*write_RRA_row (rrd_t *rrd, unsigned long rra_idx, unsigned long *rra_current,
	       unsigned short CDP_scratch_idx, FILE *rrd_file,
		   info_t *pcdp_summary, time_t *rra_time, void *rrd_mmaped_file)
#else
info_t
*write_RRA_row (rrd_t *rrd, unsigned long rra_idx, unsigned long *rra_current,
	       unsigned short CDP_scratch_idx, FILE *rrd_file,
		   info_t *pcdp_summary, time_t *rra_time)
#endif
{
   unsigned long ds_idx, cdp_idx;
   infoval iv;
  
   for (ds_idx = 0; ds_idx < rrd -> stat_head -> ds_cnt; ds_idx++)
   {
      /* compute the cdp index */
      cdp_idx =rra_idx * (rrd -> stat_head->ds_cnt) + ds_idx;
#ifdef DEBUG
	  fprintf(stderr,"  -- RRA WRITE VALUE %e, at %ld CF:%s\n",
	     rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val,ftell(rrd_file),
	     rrd -> rra_def[rra_idx].cf_nam);
#endif 
      if (pcdp_summary != NULL)
	  {
	     iv.u_val = rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val;
	     /* append info to the return hash */
	  	 pcdp_summary = info_push(pcdp_summary,
		 sprintf_alloc("[%d]RRA[%s][%lu]DS[%s]",
		 *rra_time, rrd->rra_def[rra_idx].cf_nam, 
		 rrd->rra_def[rra_idx].pdp_cnt, rrd->ds_def[ds_idx].ds_nam),
         RD_I_VAL, iv);
	  }
#ifdef HAVE_MMAP
	  memcpy((char *)rrd_mmaped_file + *rra_current,
			  &(rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val),
			  sizeof(rrd_value_t));
#else
	  if(fwrite(&(rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val),
		 sizeof(rrd_value_t),1,rrd_file) != 1)
	  { 
	     rrd_set_error("writing rrd");
	     return 0;
	  }
#endif
	  *rra_current += sizeof(rrd_value_t);
	}
	return (pcdp_summary);
}
