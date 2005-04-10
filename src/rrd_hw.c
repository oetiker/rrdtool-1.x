/*****************************************************************************
 * RRDtool 1.2rc6  Copyright by Tobi Oetiker, 1997-2005
 *****************************************************************************
 * rrd_hw.c : Support for Holt-Winters Smoothing/ Aberrant Behavior Detection
 *****************************************************************************
 * Initial version by Jake Brutlag, WebTV Networks, 5/1/00
 *****************************************************************************/

#include "rrd_tool.h"
#include "rrd_hw.h"

/* #define DEBUG */

/* private functions */
unsigned long MyMod(signed long val, unsigned long mod);
int update_hwpredict(rrd_t *rrd, unsigned long cdp_idx, unsigned long rra_idx, 
                 unsigned long ds_idx, unsigned short CDP_scratch_idx);
int update_seasonal(rrd_t *rrd, unsigned long cdp_idx, unsigned long rra_idx, 
				 unsigned long ds_idx, unsigned short CDP_scratch_idx, 
				 rrd_value_t *seasonal_coef);
int update_devpredict(rrd_t *rrd, unsigned long cdp_idx, 
				  unsigned long rra_idx, unsigned long ds_idx, unsigned short CDP_scratch_idx);
int update_devseasonal(rrd_t *rrd, unsigned long cdp_idx, unsigned long rra_idx, 
	               unsigned long ds_idx, unsigned short CDP_scratch_idx, 
				   rrd_value_t *seasonal_dev);
int update_failures(rrd_t *rrd, unsigned long cdp_idx, unsigned long rra_idx, 
				unsigned long ds_idx, unsigned short CDP_scratch_idx);

int
update_hwpredict(rrd_t *rrd, unsigned long cdp_idx, unsigned long rra_idx, 
                 unsigned long ds_idx, unsigned short CDP_scratch_idx)
{
   rrd_value_t prediction, seasonal_coef;
   unsigned long dependent_rra_idx, seasonal_cdp_idx;
   unival *coefs = rrd -> cdp_prep[cdp_idx].scratch;
   rra_def_t *current_rra = &(rrd -> rra_def[rra_idx]);

   /* save coefficients from current prediction */
   coefs[CDP_hw_last_intercept].u_val = coefs[CDP_hw_intercept].u_val;
   coefs[CDP_hw_last_slope].u_val = coefs[CDP_hw_slope].u_val;
   coefs[CDP_last_null_count].u_cnt = coefs[CDP_null_count].u_cnt;

   /* retrieve the current seasonal coef */
   dependent_rra_idx = current_rra -> par[RRA_dependent_rra_idx].u_cnt;
   seasonal_cdp_idx = dependent_rra_idx*(rrd -> stat_head -> ds_cnt) + ds_idx;
   if (dependent_rra_idx < rra_idx)
	  seasonal_coef = rrd -> cdp_prep[seasonal_cdp_idx].scratch[CDP_hw_last_seasonal].u_val;
   else
	  seasonal_coef = rrd -> cdp_prep[seasonal_cdp_idx].scratch[CDP_hw_seasonal].u_val;
   
   /* compute the prediction */
   if (isnan(coefs[CDP_hw_intercept].u_val) || isnan(coefs[CDP_hw_slope].u_val)
      || isnan(seasonal_coef))
   {
     prediction = DNAN;
      
     /* bootstrap initialization of slope and intercept */
     if (isnan(coefs[CDP_hw_intercept].u_val) &&
	 !isnan(coefs[CDP_scratch_idx].u_val)) 
     {
#ifdef DEBUG
       fprintf(stderr,"Initialization of slope/intercept\n");
#endif
       coefs[CDP_hw_intercept].u_val = coefs[CDP_scratch_idx].u_val;
       coefs[CDP_hw_last_intercept].u_val = coefs[CDP_scratch_idx].u_val;
       /* initialize the slope to 0 */
       coefs[CDP_hw_slope].u_val = 0.0;
       coefs[CDP_hw_last_slope].u_val = 0.0;
       /* initialize null count to 1 */
       coefs[CDP_null_count].u_cnt = 1;
       coefs[CDP_last_null_count].u_cnt = 1;
     }
     /* if seasonal coefficient is NA, then don't update intercept, slope */
   } else {
     prediction = coefs[CDP_hw_intercept].u_val + 
       (coefs[CDP_hw_slope].u_val)*(coefs[CDP_null_count].u_cnt)
       + seasonal_coef;
#ifdef DEBUG
     fprintf(stderr,"computed prediction: %f\n",prediction);
#endif
     if (isnan(coefs[CDP_scratch_idx].u_val))
     {
       /* NA value, no updates of intercept, slope;
	    * increment the null count */
       (coefs[CDP_null_count].u_cnt)++;
     } else {
#ifdef DEBUG
       fprintf(stderr,"Updating intercept, slope\n");
#endif
       /* update the intercept */
       coefs[CDP_hw_intercept].u_val = (current_rra -> par[RRA_hw_alpha].u_val)*
	 	(coefs[CDP_scratch_idx].u_val - seasonal_coef) +
	 	(1 - current_rra -> par[RRA_hw_alpha].u_val)*(coefs[CDP_hw_intercept].u_val
	 	+ (coefs[CDP_hw_slope].u_val)*(coefs[CDP_null_count].u_cnt));
       /* update the slope */
       coefs[CDP_hw_slope].u_val = (current_rra -> par[RRA_hw_beta].u_val)*
	 	(coefs[CDP_hw_intercept].u_val - coefs[CDP_hw_last_intercept].u_val) +
	 	(1 - current_rra -> par[RRA_hw_beta].u_val)*(coefs[CDP_hw_slope].u_val);
       /* reset the null count */
       coefs[CDP_null_count].u_cnt = 1;
     }
   }

   /* store the prediction for writing */
   coefs[CDP_scratch_idx].u_val = prediction;
   return 0;
}

int
lookup_seasonal(rrd_t *rrd, unsigned long rra_idx, unsigned long rra_start,
				FILE *rrd_file, unsigned long offset, rrd_value_t **seasonal_coef)
{
   unsigned long pos_tmp;
   /* rra_ptr[].cur_row points to the rra row to be written; this function
	* reads cur_row + offset */
   unsigned long row_idx = rrd -> rra_ptr[rra_idx].cur_row + offset;
   /* handle wrap around */
   if (row_idx >= rrd -> rra_def[rra_idx].row_cnt)
     row_idx = row_idx % (rrd -> rra_def[rra_idx].row_cnt);

   /* rra_start points to the appropriate rra block in the file */
   /* compute the pointer to the appropriate location in the file */
   pos_tmp = rra_start + (row_idx)*(rrd -> stat_head -> ds_cnt)*sizeof(rrd_value_t);

   /* allocate memory if need be */
   if (*seasonal_coef == NULL)
	  *seasonal_coef = 
	     (rrd_value_t *) malloc((rrd -> stat_head -> ds_cnt)*sizeof(rrd_value_t));
   if (*seasonal_coef == NULL) {
	  rrd_set_error("memory allocation failure: seasonal coef");
	  return -1;
   }

   if (!fseek(rrd_file,pos_tmp,SEEK_SET))
   {
      if (fread(*seasonal_coef,sizeof(rrd_value_t),rrd->stat_head->ds_cnt,rrd_file)
		  == rrd -> stat_head -> ds_cnt)
	  {
		 /* success! */
         /* we can safely ignore the rule requiring a seek operation between read
          * and write, because this read moves the file pointer to somewhere
          * in the file other than the next write location.
          * */
		 return 0;
	  } else {
	     rrd_set_error("read operation failed in lookup_seasonal(): %lu\n",pos_tmp);
	  }
   } else {
	  rrd_set_error("seek operation failed in lookup_seasonal(): %lu\n",pos_tmp);
   }
   
   return -1;
}

int
update_seasonal(rrd_t *rrd, unsigned long cdp_idx, unsigned long rra_idx, 
				 unsigned long ds_idx, unsigned short CDP_scratch_idx, rrd_value_t *seasonal_coef)
{
/* TODO: extract common if subblocks in the wake of I/O optimization */
   rrd_value_t intercept, seasonal;
   rra_def_t *current_rra = &(rrd -> rra_def[rra_idx]);
   rra_def_t *hw_rra = &(rrd -> rra_def[current_rra -> par[RRA_dependent_rra_idx].u_cnt]);
   /* obtain cdp_prep index for HWPREDICT */
   unsigned long hw_cdp_idx = (current_rra -> par[RRA_dependent_rra_idx].u_cnt)
      * (rrd -> stat_head -> ds_cnt) + ds_idx;
   unival *coefs = rrd -> cdp_prep[hw_cdp_idx].scratch;

   /* update seasonal coefficient in cdp prep areas */
   seasonal = rrd -> cdp_prep[cdp_idx].scratch[CDP_hw_seasonal].u_val;
   rrd -> cdp_prep[cdp_idx].scratch[CDP_hw_last_seasonal].u_val = seasonal;
   rrd -> cdp_prep[cdp_idx].scratch[CDP_hw_seasonal].u_val =
	  seasonal_coef[ds_idx];

   /* update seasonal value for disk */
   if (current_rra -> par[RRA_dependent_rra_idx].u_cnt < rra_idx)
	  /* associated HWPREDICT has already been updated */
	  /* check for possible NA values */
      if (isnan(rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val))
	  {
		 /* no update, store the old value unchanged,
		  * doesn't matter if it is NA */
	     rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val = seasonal;
	  } else if (isnan(coefs[CDP_hw_last_intercept].u_val) 
		     || isnan(coefs[CDP_hw_last_slope].u_val))
	  {
		 /* this should never happen, as HWPREDICT was already updated */
		 rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val= DNAN;
	  } else if (isnan(seasonal))
	  {
		 /* initialization: intercept is not currently being updated */
#ifdef DEBUG
		 fprintf(stderr,"Initialization of seasonal coef %lu\n",
			rrd -> rra_ptr[rra_idx].cur_row);
#endif
		 rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val 
			-= coefs[CDP_hw_last_intercept].u_val; 
	  } else {
	         intercept = coefs[CDP_hw_intercept].u_val;
#ifdef DEBUG
		 fprintf(stderr,
			"Updating seasonal, params: gamma %f, new intercept %f, old seasonal %f\n",
			current_rra -> par[RRA_seasonal_gamma].u_val,
			intercept, seasonal);
#endif
		 rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val =
			(current_rra -> par[RRA_seasonal_gamma].u_val)*
			(rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val - intercept) +
			(1 - current_rra -> par[RRA_seasonal_gamma].u_val)*seasonal;
	  }
   else {
	  /* SEASONAL array is updated first, which means the new intercept
	   * hasn't be computed; so we compute it here. */

	  /* check for possible NA values */
      if (isnan(rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val))
	  {
		 /* no update, simple store the old value unchanged,
		  * doesn't matter if it is NA */
		 rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val =  seasonal;
	  } else if (isnan(coefs[CDP_hw_intercept].u_val) 
		     || isnan(coefs[CDP_hw_slope].u_val))
	  {
		 /* Initialization of slope and intercept will occur.
		  * force seasonal coefficient to 0. */
		 rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val= 0.0;
	  } else if (isnan(seasonal))
	  {
		 /* initialization: intercept will not be updated
		  * CDP_hw_intercept = CDP_hw_last_intercept; just need to 
		  * subtract this baseline value. */
#ifdef DEBUG
		 fprintf(stderr,"Initialization of seasonal coef %lu\n",
			rrd -> rra_ptr[rra_idx].cur_row);
#endif
		 rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val -= coefs[CDP_hw_intercept].u_val; 
	  } else {
	         /* Note that we must get CDP_scratch_idx from SEASONAL array, as CDP_scratch_idx
	          * for HWPREDICT array will be DNAN. */
	     intercept = (hw_rra -> par[RRA_hw_alpha].u_val)*
		    (rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val - seasonal)
		    + (1 - hw_rra -> par[RRA_hw_alpha].u_val)*(coefs[CDP_hw_intercept].u_val
		    + (coefs[CDP_hw_slope].u_val)*(coefs[CDP_null_count].u_cnt));
		 rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val =
		   (current_rra -> par[RRA_seasonal_gamma].u_val)*
		   (rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val - intercept) +
		   (1 - current_rra -> par[RRA_seasonal_gamma].u_val)*seasonal;
	  }
   }
#ifdef DEBUG
   fprintf(stderr,"seasonal coefficient set= %f\n",
         rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val);
#endif
   return 0;
}

int
update_devpredict(rrd_t *rrd, unsigned long cdp_idx, 
				  unsigned long rra_idx, unsigned long ds_idx, unsigned short CDP_scratch_idx)
{
   /* there really isn't any "update" here; the only reason this information
    * is stored separately from DEVSEASONAL is to preserve deviation predictions
    * for a longer duration than one seasonal cycle. */
   unsigned long seasonal_cdp_idx = (rrd -> rra_def[rra_idx].par[RRA_dependent_rra_idx].u_cnt)
      * (rrd -> stat_head -> ds_cnt) + ds_idx;

   if (rrd -> rra_def[rra_idx].par[RRA_dependent_rra_idx].u_cnt < rra_idx)
   {
	  /* associated DEVSEASONAL array already updated */
	  rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val
		 = rrd -> cdp_prep[seasonal_cdp_idx].scratch[CDP_last_seasonal_deviation].u_val;
   } else {
	  /* associated DEVSEASONAL not yet updated */
	  rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val
		 = rrd -> cdp_prep[seasonal_cdp_idx].scratch[CDP_seasonal_deviation].u_val;
   }
   return 0;
}

int
update_devseasonal(rrd_t *rrd, unsigned long cdp_idx, unsigned long rra_idx, 
	               unsigned long ds_idx, unsigned short CDP_scratch_idx, 
				   rrd_value_t *seasonal_dev)
{
   rrd_value_t prediction = 0, seasonal_coef = DNAN;
   rra_def_t *current_rra = &(rrd -> rra_def[rra_idx]);
   /* obtain cdp_prep index for HWPREDICT */
   unsigned long hw_rra_idx = current_rra -> par[RRA_dependent_rra_idx].u_cnt;
   unsigned long hw_cdp_idx = hw_rra_idx * (rrd -> stat_head -> ds_cnt) + ds_idx;
   unsigned long seasonal_cdp_idx;
   unival *coefs = rrd -> cdp_prep[hw_cdp_idx].scratch;
 
   rrd -> cdp_prep[cdp_idx].scratch[CDP_last_seasonal_deviation].u_val =
	  rrd -> cdp_prep[cdp_idx].scratch[CDP_seasonal_deviation].u_val;
   /* retrieve the next seasonal deviation value, could be NA */
   rrd -> cdp_prep[cdp_idx].scratch[CDP_seasonal_deviation].u_val =
	  seasonal_dev[ds_idx];

   /* retrieve the current seasonal_coef (not to be confused with the
	* current seasonal deviation). Could make this more readable by introducing
	* some wrapper functions. */
   seasonal_cdp_idx = (rrd -> rra_def[hw_rra_idx].par[RRA_dependent_rra_idx].u_cnt)
	  *(rrd -> stat_head -> ds_cnt) + ds_idx;
   if (rrd -> rra_def[hw_rra_idx].par[RRA_dependent_rra_idx].u_cnt < rra_idx)
	  /* SEASONAL array already updated */
	  seasonal_coef = rrd -> cdp_prep[seasonal_cdp_idx].scratch[CDP_hw_last_seasonal].u_val;
   else
	  /* SEASONAL array not yet updated */
	  seasonal_coef = rrd -> cdp_prep[seasonal_cdp_idx].scratch[CDP_hw_seasonal].u_val;
   
   /* compute the abs value of the difference between the prediction and
	* observed value */
   if (hw_rra_idx < rra_idx)
   {
	  /* associated HWPREDICT has already been updated */
	  if (isnan(coefs[CDP_hw_last_intercept].u_val) ||
	      isnan(coefs[CDP_hw_last_slope].u_val) ||
	      isnan(seasonal_coef))
	  {
		 /* one of the prediction values is uinitialized */
		 rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val = DNAN;
		 return 0;
	  } else {
	     prediction = coefs[CDP_hw_last_intercept].u_val + 
		   (coefs[CDP_hw_last_slope].u_val)*(coefs[CDP_last_null_count].u_cnt)
		   + seasonal_coef;
	  }
   } else {
	  /* associated HWPREDICT has NOT been updated */
	  if (isnan(coefs[CDP_hw_intercept].u_val) ||
	      isnan(coefs[CDP_hw_slope].u_val) ||
	      isnan(seasonal_coef))
	  {
		 /* one of the prediction values is uinitialized */
		 rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val = DNAN;
		 return 0;
	  } else {
	     prediction = coefs[CDP_hw_intercept].u_val + 
		   (coefs[CDP_hw_slope].u_val)*(coefs[CDP_null_count].u_cnt) 
		   + seasonal_coef;
	  }
   }

   if (isnan(rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val))
   {
      /* no update, store existing value unchanged, doesn't
	   * matter if it is NA */
	  rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val =
		 rrd -> cdp_prep[cdp_idx].scratch[CDP_last_seasonal_deviation].u_val;
   } else if (isnan(rrd -> cdp_prep[cdp_idx].scratch[CDP_last_seasonal_deviation].u_val))
   {
	  /* initialization */
#ifdef DEBUG
	  fprintf(stderr,"Initialization of seasonal deviation\n");
#endif
	  rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val =
	     fabs(prediction - rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val);
   } else {
	  /* exponential smoothing update */
	  rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val =
	    (rrd -> rra_def[rra_idx].par[RRA_seasonal_gamma].u_val)*
	    fabs(prediction - rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val)
	    + (1 -  rrd -> rra_def[rra_idx].par[RRA_seasonal_gamma].u_val)*
	    (rrd -> cdp_prep[cdp_idx].scratch[CDP_last_seasonal_deviation].u_val);
   }
   return 0;
}

/* Check for a failure based on a threshold # of violations within the specified
 * window. */
int 
update_failures(rrd_t *rrd, unsigned long cdp_idx, unsigned long rra_idx, 
				unsigned long ds_idx, unsigned short CDP_scratch_idx)
{
   /* detection of a violation depends on 3 RRAs:
	* HWPREDICT, SEASONAL, and DEVSEASONAL */
   rra_def_t *current_rra = &(rrd -> rra_def[rra_idx]);
   unsigned long dev_rra_idx = current_rra -> par[RRA_dependent_rra_idx].u_cnt;
   rra_def_t *dev_rra = &(rrd -> rra_def[dev_rra_idx]);
   unsigned long hw_rra_idx = dev_rra -> par[RRA_dependent_rra_idx].u_cnt;
   rra_def_t *hw_rra =  &(rrd -> rra_def[hw_rra_idx]);
   unsigned long seasonal_rra_idx = hw_rra -> par[RRA_dependent_rra_idx].u_cnt;
   unsigned long temp_cdp_idx;
   rrd_value_t deviation = DNAN;
   rrd_value_t seasonal_coef = DNAN;
   rrd_value_t prediction = DNAN;
   char violation = 0; 
   unsigned short violation_cnt = 0, i;
   char *violations_array;

   /* usual checks to determine the order of the RRAs */
   temp_cdp_idx = dev_rra_idx * (rrd -> stat_head -> ds_cnt) + ds_idx;
   if (rra_idx < seasonal_rra_idx)
   {
	  /* DEVSEASONAL not yet updated */
	  deviation = rrd -> cdp_prep[temp_cdp_idx].scratch[CDP_seasonal_deviation].u_val;
   } else {
	  /* DEVSEASONAL already updated */
	  deviation = rrd -> cdp_prep[temp_cdp_idx].scratch[CDP_last_seasonal_deviation].u_val;
   }
   if (!isnan(deviation)) {

   temp_cdp_idx = seasonal_rra_idx * (rrd -> stat_head -> ds_cnt) + ds_idx;
   if (rra_idx < seasonal_rra_idx)
   {
	  /* SEASONAL not yet updated */
	  seasonal_coef = rrd -> cdp_prep[temp_cdp_idx].scratch[CDP_hw_seasonal].u_val;
   } else {
	  /* SEASONAL already updated */
	  seasonal_coef = rrd -> cdp_prep[temp_cdp_idx].scratch[CDP_hw_last_seasonal].u_val;
   }
   /* in this code block, we know seasonal coef is not DNAN, because deviation is not
	* null */

   temp_cdp_idx = hw_rra_idx * (rrd -> stat_head -> ds_cnt) + ds_idx;
   if (rra_idx < hw_rra_idx)
   {
	  /* HWPREDICT not yet updated */
	  prediction = rrd -> cdp_prep[temp_cdp_idx].scratch[CDP_hw_intercept].u_val + 
	     (rrd -> cdp_prep[temp_cdp_idx].scratch[CDP_hw_slope].u_val)
		 *(rrd -> cdp_prep[temp_cdp_idx].scratch[CDP_null_count].u_cnt)
		 + seasonal_coef;
   } else {
	  /* HWPREDICT already updated */
	  prediction = rrd -> cdp_prep[temp_cdp_idx].scratch[CDP_hw_last_intercept].u_val + 
	     (rrd -> cdp_prep[temp_cdp_idx].scratch[CDP_hw_last_slope].u_val)
		 *(rrd -> cdp_prep[temp_cdp_idx].scratch[CDP_last_null_count].u_cnt)
		 + seasonal_coef;
   }

   /* determine if the observed value is a violation */
   if (!isnan(rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val))
   {
	  if (rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val > prediction + 
		 (current_rra -> par[RRA_delta_pos].u_val)*deviation
	     || rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val < prediction - 
		 (current_rra -> par[RRA_delta_neg].u_val)*deviation)
		 violation = 1;
   } else {
	  violation = 1; /* count DNAN values as violations */
   }

   }

   /* determine if a failure has occurred and update the failure array */
   violation_cnt = violation;
   violations_array = (char *) ((void *) rrd -> cdp_prep[cdp_idx].scratch);
   for (i = current_rra -> par[RRA_window_len].u_cnt; i > 1; i--)
   {
	  /* shift */
	  violations_array[i-1] = violations_array[i-2]; 
	  violation_cnt += violations_array[i-1];
   }
   violations_array[0] = violation;

   if (violation_cnt < current_rra -> par[RRA_failure_threshold].u_cnt)
	  /* not a failure */
	  rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val = 0.0;
   else
	  rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val = 1.0;

   return (rrd-> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val);
}

/* For the specified CDP prep area and the FAILURES RRA,
 * erase all history of past violations.
 */
void
erase_violations(rrd_t *rrd, unsigned long cdp_idx, unsigned long rra_idx)
{
   unsigned short i;
   char *violations_array;
   /* check that rra_idx is a CF_FAILURES array */
   if (cf_conv(rrd -> rra_def[rra_idx].cf_nam) != CF_FAILURES)
   {
#ifdef DEBUG
	  fprintf(stderr,"erase_violations called for non-FAILURES RRA: %s\n",
	     rrd -> rra_def[rra_idx].cf);
#endif
	  return;
   }

#ifdef DEBUG
   fprintf(stderr,"scratch buffer before erase:\n");
   for (i = 0; i < MAX_CDP_PAR_EN; i++)
   {
	  fprintf(stderr,"%lu ", rrd -> cdp_prep[cdp_idx].scratch[i].u_cnt);
   }
   fprintf(stderr,"\n");
#endif

   /* WARNING: an array of longs on disk is treated as an array of chars
    * in memory. */
   violations_array = (char *) ((void *) rrd -> cdp_prep[cdp_idx].scratch);
   /* erase everything in the part of the CDP scratch array that will be
    * used to store violations for the current window */
   for (i = rrd -> rra_def[rra_idx].par[RRA_window_len].u_cnt; i > 0; i--)
   {
	  violations_array[i-1] = 0;
   }
#ifdef DEBUG
   fprintf(stderr,"scratch buffer after erase:\n");
   for (i = 0; i < MAX_CDP_PAR_EN; i++)
   {
	  fprintf(stderr,"%lu ", rrd -> cdp_prep[cdp_idx].scratch[i].u_cnt);
   }
   fprintf(stderr,"\n");
#endif
}

/* Smooth a periodic array with a moving average: equal weights and
 * length = 5% of the period. */
int
apply_smoother(rrd_t *rrd, unsigned long rra_idx, unsigned long rra_start,
               FILE *rrd_file)
{
   unsigned long i, j, k;
   unsigned long totalbytes;
   rrd_value_t *rrd_values;
   unsigned long row_length = rrd -> stat_head -> ds_cnt;
   unsigned long row_count = rrd -> rra_def[rra_idx].row_cnt;
   unsigned long offset;
   FIFOqueue **buffers;
   rrd_value_t *working_average;
   rrd_value_t *baseline;

   offset = floor(0.025*row_count);
   if (offset == 0) return 0; /* no smoothing */

   /* allocate memory */
   totalbytes = sizeof(rrd_value_t)*row_length*row_count;
   rrd_values = (rrd_value_t *) malloc(totalbytes);
   if (rrd_values == NULL)
   {
	  rrd_set_error("apply smoother: memory allocation failure");
	  return -1;
   }

   /* rra_start is at the beginning of this rra */
   if (fseek(rrd_file,rra_start,SEEK_SET))
   {
	  rrd_set_error("seek to rra %d failed", rra_start);
	  free(rrd_values);
	  return -1;
   }
   fflush(rrd_file);
   /* could read all data in a single block, but we need to
    * check for NA values */
   for (i = 0; i < row_count; ++i)
   {
	  for (j = 0; j < row_length; ++j)
	  {
		 fread(&(rrd_values[i*row_length + j]),sizeof(rrd_value_t),1,rrd_file);
		 /* should check fread for errors... */
		 if (isnan(rrd_values[i*row_length + j])) {
			/* can't apply smoothing, still uninitialized values */
#ifdef DEBUG
			fprintf(stderr,"apply_smoother: NA detected in seasonal array: %ld %ld\n",i,j);
#endif
			free(rrd_values);
			return 0;
		 }
	  }
   }

   /* allocate queues, one for each data source */
   buffers = (FIFOqueue **) malloc(sizeof(FIFOqueue *)*row_length);
   for (i = 0; i < row_length; ++i)
   {
      queue_alloc(&(buffers[i]),2*offset + 1);
   }
   /* need working average initialized to 0 */
   working_average = (rrd_value_t *) calloc(row_length,sizeof(rrd_value_t));
   baseline = (rrd_value_t *) calloc(row_length,sizeof(rrd_value_t));

   /* compute sums of the first 2*offset terms */ 
   for (i = 0; i < 2*offset; ++i)
   {
	  k = MyMod(i - offset,row_count);
	  for (j = 0; j < row_length; ++j)
	  {
		 queue_push(buffers[j],rrd_values[k*row_length + j]);
		 working_average[j] += rrd_values[k*row_length + j];
	  }
   }

   /* compute moving averages */
   for (i = offset; i < row_count + offset; ++i)
   {
	  for (j = 0; j < row_length; ++j)
	  {
	     k = MyMod(i,row_count);
	     /* add a term to the sum */
	     working_average[j] += rrd_values[k*row_length + j];
	     queue_push(buffers[j],rrd_values[k*row_length + j]);

	     /* reset k to be the center of the window */
	     k = MyMod(i - offset,row_count);
	     /* overwrite rdd_values entry, the old value is already
	      * saved in buffers */
	     rrd_values[k*row_length + j] = working_average[j]/(2*offset + 1);
	     baseline[j] += rrd_values[k*row_length + j];

	     /* remove a term from the sum */
	     working_average[j] -= queue_pop(buffers[j]);
	  }	
   } 
 
   for (i = 0; i < row_length; ++i)
   {
	  queue_dealloc(buffers[i]);
	  baseline[i] /= row_count; 
   }
   free(buffers);
   free(working_average);

   if (cf_conv(rrd->rra_def[rra_idx].cf_nam) == CF_SEASONAL) {
   for (j = 0; j < row_length; ++j)
   {
   for (i = 0; i < row_count; ++i)
   {
	 rrd_values[i*row_length + j] -= baseline[j];
   }
	 /* update the baseline coefficient,
	  * first, compute the cdp_index. */
	 offset = (rrd->rra_def[rra_idx].par[RRA_dependent_rra_idx].u_cnt)
	  * row_length + j;
	 (rrd->cdp_prep[offset]).scratch[CDP_hw_intercept].u_val += baseline[j];
   }
   /* flush cdp to disk */
   fflush(rrd_file);
   if (fseek(rrd_file,sizeof(stat_head_t) + 
	  rrd->stat_head->ds_cnt * sizeof(ds_def_t) +
	  rrd->stat_head->rra_cnt * sizeof(rra_def_t) + 
	  sizeof(live_head_t) +
	  rrd->stat_head->ds_cnt * sizeof(pdp_prep_t),SEEK_SET))
   {
	  rrd_set_error("apply_smoother: seek to cdp_prep failed");
	  free(rrd_values);
	  return -1;
   }
   if (fwrite( rrd -> cdp_prep,
	  sizeof(cdp_prep_t),
	  (rrd->stat_head->rra_cnt) * rrd->stat_head->ds_cnt, rrd_file) 
	  != (rrd->stat_head->rra_cnt) * (rrd->stat_head->ds_cnt) )
   { 
	  rrd_set_error("apply_smoother: cdp_prep write failed");
	  free(rrd_values);
	  return -1;
   }
   } /* endif CF_SEASONAL */ 

   /* flush updated values to disk */
   fflush(rrd_file);
   if (fseek(rrd_file,rra_start,SEEK_SET))
   {
	  rrd_set_error("apply_smoother: seek to pos %d failed", rra_start);
	  free(rrd_values);
	  return -1;
   }
   /* write as a single block */
   if (fwrite(rrd_values,sizeof(rrd_value_t),row_length*row_count,rrd_file)
	  != row_length*row_count)
   {
	  rrd_set_error("apply_smoother: write failed to %lu",rra_start);
	  free(rrd_values);
	  return -1;
   }

   fflush(rrd_file);
   free(rrd_values);
   free(baseline);
   return 0;
}

/* Reset aberrant behavior model coefficients, including intercept, slope,
 * seasonal, and seasonal deviation for the specified data source. */
void
reset_aberrant_coefficients(rrd_t *rrd, FILE *rrd_file, unsigned long ds_idx)
{
   unsigned long cdp_idx, rra_idx, i;
   unsigned long cdp_start, rra_start;
   rrd_value_t nan_buffer = DNAN;

   /* compute the offset for the cdp area */
   cdp_start = sizeof(stat_head_t) + 
	  rrd->stat_head->ds_cnt * sizeof(ds_def_t) +
	  rrd->stat_head->rra_cnt * sizeof(rra_def_t) + 
	  sizeof(live_head_t) +
	  rrd->stat_head->ds_cnt * sizeof(pdp_prep_t);
   /* compute the offset for the first rra */
   rra_start = cdp_start + 
	  (rrd->stat_head->ds_cnt) * (rrd->stat_head->rra_cnt) * sizeof(cdp_prep_t) +
	  rrd->stat_head->rra_cnt * sizeof(rra_ptr_t);

   /* loop over the RRAs */
   for (rra_idx = 0; rra_idx < rrd -> stat_head -> rra_cnt; rra_idx++)
   {
	  cdp_idx = rra_idx * (rrd-> stat_head-> ds_cnt) + ds_idx;
	  switch (cf_conv(rrd -> rra_def[rra_idx].cf_nam))
	  {
		 case CF_HWPREDICT:
	        init_hwpredict_cdp(&(rrd -> cdp_prep[cdp_idx]));
			break;
		 case CF_SEASONAL:
		 case CF_DEVSEASONAL:
			/* don't use init_seasonal because it will reset burn-in, which
			 * means different data sources will be calling for the smoother
			 * at different times. */
	        rrd->cdp_prep[cdp_idx].scratch[CDP_hw_seasonal].u_val = DNAN;
	        rrd->cdp_prep[cdp_idx].scratch[CDP_hw_last_seasonal].u_val = DNAN;
			/* move to first entry of data source for this rra */
			fseek(rrd_file,rra_start + ds_idx * sizeof(rrd_value_t),SEEK_SET);
			/* entries for the same data source are not contiguous, 
			 * temporal entries are contiguous */
	        for (i = 0; i < rrd->rra_def[rra_idx].row_cnt; ++i)
			{
			   if (fwrite(&nan_buffer,sizeof(rrd_value_t),1,rrd_file) != 1)
			   {
                  rrd_set_error(
				  "reset_aberrant_coefficients: write failed data source %lu rra %s",
				  ds_idx,rrd->rra_def[rra_idx].cf_nam);
				  return;
			   } 
			   fseek(rrd_file,(rrd->stat_head->ds_cnt - 1) * 
				  sizeof(rrd_value_t),SEEK_CUR);
			}
			break;
		 case CF_FAILURES:
			erase_violations(rrd,cdp_idx,rra_idx);
			break;
		 default:
			break;
	  }
	  /* move offset to the next rra */
	  rra_start += rrd->rra_def[rra_idx].row_cnt * rrd->stat_head->ds_cnt * 
		 sizeof(rrd_value_t);
   }
   fseek(rrd_file,cdp_start,SEEK_SET);
   if (fwrite( rrd -> cdp_prep,
	  sizeof(cdp_prep_t),
	  (rrd->stat_head->rra_cnt) * rrd->stat_head->ds_cnt, rrd_file) 
	  != (rrd->stat_head->rra_cnt) * (rrd->stat_head->ds_cnt) )
   {
	  rrd_set_error("reset_aberrant_coefficients: cdp_prep write failed");
	  return;
   }
}

void init_hwpredict_cdp(cdp_prep_t *cdp)
{
   cdp->scratch[CDP_hw_intercept].u_val = DNAN;
   cdp->scratch[CDP_hw_last_intercept].u_val = DNAN;
   cdp->scratch[CDP_hw_slope].u_val = DNAN;
   cdp->scratch[CDP_hw_last_slope].u_val = DNAN;
   cdp->scratch[CDP_null_count].u_cnt = 1;
   cdp->scratch[CDP_last_null_count].u_cnt = 1;
}

void init_seasonal_cdp(cdp_prep_t *cdp)
{
   cdp->scratch[CDP_hw_seasonal].u_val = DNAN;
   cdp->scratch[CDP_hw_last_seasonal].u_val = DNAN;
   cdp->scratch[CDP_init_seasonal].u_cnt = 1;
}

int
update_aberrant_CF(rrd_t *rrd, rrd_value_t pdp_val, enum cf_en current_cf, 
		  unsigned long cdp_idx, unsigned long rra_idx, unsigned long ds_idx, 
		  unsigned short CDP_scratch_idx, rrd_value_t *seasonal_coef)
{
   rrd -> cdp_prep[cdp_idx].scratch[CDP_scratch_idx].u_val = pdp_val;
   switch (current_cf) {
   case CF_AVERAGE:
   default:
	 return 0;
   case CF_HWPREDICT:
     return update_hwpredict(rrd,cdp_idx,rra_idx,ds_idx,CDP_scratch_idx);
   case CF_DEVPREDICT:
	 return update_devpredict(rrd,cdp_idx,rra_idx,ds_idx,CDP_scratch_idx);
   case CF_SEASONAL:
     return update_seasonal(rrd,cdp_idx,rra_idx,ds_idx,CDP_scratch_idx,seasonal_coef);
   case CF_DEVSEASONAL:
     return update_devseasonal(rrd,cdp_idx,rra_idx,ds_idx,CDP_scratch_idx,seasonal_coef);
   case CF_FAILURES:
     return update_failures(rrd,cdp_idx,rra_idx,ds_idx,CDP_scratch_idx);
   }
   return -1;
}

unsigned long MyMod(signed long val, unsigned long mod)
{
   unsigned long new_val;
   if (val < 0)
     new_val = ((unsigned long) abs(val)) % mod;
   else
     new_val = (val % mod);
   
   if (val < 0) 
     return (mod - new_val);
   else
     return (new_val);
}

/* a standard fixed-capacity FIF0 queue implementation
 * No overflow checking is performed. */
int queue_alloc(FIFOqueue **q,int capacity)
{
   *q = (FIFOqueue *) malloc(sizeof(FIFOqueue));
   if (*q == NULL) return -1;
   (*q) -> queue = (rrd_value_t *) malloc(sizeof(rrd_value_t)*capacity);
   if ((*q) -> queue == NULL)
   {
	  free(*q);
	  return -1;
   }
   (*q) -> capacity = capacity;
   (*q) -> head = capacity;
   (*q) -> tail = 0;
   return 0;
}

int queue_isempty(FIFOqueue *q)
{
   return (q -> head % q -> capacity == q -> tail);
}

void queue_push(FIFOqueue *q, rrd_value_t value)
{
   q -> queue[(q -> tail)++] = value;
   q -> tail = q -> tail % q -> capacity;
}

rrd_value_t queue_pop(FIFOqueue *q)
{
   q -> head = q -> head % q -> capacity;
   return q -> queue[(q -> head)++];
}

void queue_dealloc(FIFOqueue *q)
{
   free(q -> queue);
   free(q);
}
