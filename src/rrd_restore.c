/*****************************************************************************
 * RRDtool 1.0.33  Copyright Tobias Oetiker, 1997 - 2000
 *****************************************************************************
 * rrd_restore.c  creates new rrd from data dumped by rrd_dump.c
 *****************************************************************************/

#include "rrd_tool.h"

/* Prototypes */

void xml_lc(char*);
int skip(char **);
int eat_tag(char **, char *);
int read_tag(char **, char *, char *, void *);
int xml2rrd(char*, rrd_t*, char);
int rrd_write(char *, rrd_t *);

/* convert all ocurances of <BlaBlaBla> to <blablabla> */

void xml_lc(char* buf){
  int intag=0;
  while((*buf)){
    if (intag ==0 && (*buf) == '<') {
      intag = 1;
    }
    else if (intag ==1 && (*buf) == '>') {
      intag = 0;
      continue;
    } else  if (intag ==1) {
      *buf = tolower(*buf);
    }
    buf++;    
  }
}

int skip(char **buf){
  char *ptr;  
  ptr=(*buf);
  do {
    (*buf)=ptr;
    while((*(ptr+1)) && ((*ptr)==' ' ||  (*ptr)=='\r' || (*ptr)=='\n' || (*ptr)=='\t')) ptr++;
    if (strncmp(ptr,"<!--",4) == 0) {
      ptr= strstr(ptr,"-->");
      if (ptr) ptr+=3; else {
	rrd_set_error("Dangling Comment");
	(*buf) = NULL;
	return -1;
      }
    }
  } while ((*buf)!=ptr);  
  return 1;
}

int eat_tag(char **buf, char *tag){ 
  if ((*buf)==NULL) return -1;   /* fall though clause */

  rrd_clear_error();
  skip(buf);
  if ((**buf)=='<' 
      && strncmp((*buf)+1,tag,strlen(tag)) == 0 
      && *((*buf)+strlen(tag)+1)=='>') {
    (*buf) += strlen(tag)+2;
  }
  else {
    rrd_set_error("No <%s> tag found",tag);
    (*buf) = NULL;
    return -1;
  }
  skip(buf);
  return 1;
}

int read_tag(char **buf, char *tag, char *format, void *value){
    char *end_tag;
    int matches;
    if ((*buf)==NULL) return -1;   /* fall though clause */
    rrd_clear_error();
    if (eat_tag(buf,tag)==1){
	char *temp;
	temp = (*buf);
	while(*((*buf)+1) && (*(*buf) != '<')) (*buf)++; /*find start of endtag*/
	*(*buf) = '\0';
	matches =sscanf(temp,format,value);
	*(*buf) = '<';
	end_tag = malloc((strlen(tag)+2)*sizeof(char));
	sprintf(end_tag,"/%s",tag);
	eat_tag(buf,end_tag);
	free(end_tag);
	if (matches == 0 && strcmp(format,"%lf") == 0)
	    (*((double* )(value))) = DNAN;
	if (matches != 1)	return 0;       
	return 1;
    }
    return -1;
}


/* parse the data stored in buf and return a filled rrd structure */
int xml2rrd(char* buf, rrd_t* rrd, char rc){
  /* pass 1 identify number of RRAs  */
  char *ptr,*ptr2,*ptr3; /* walks thought the buffer */
  long rows=0,mempool=0,i=0;
  xml_lc(buf); /* lets lowercase all active parts of the xml */
  ptr=buf;
  ptr2=buf;
  ptr3=buf;
  /* start with an RRD tag */

  eat_tag(&ptr,"rrd");
  /* allocate static header */
  if((rrd->stat_head = calloc(1,sizeof(stat_head_t)))==NULL){
    rrd_set_error("allocating rrd.stat_head");
    return -1;    
  };

  strcpy(rrd->stat_head->cookie,RRD_COOKIE);
  read_tag(&ptr,"version","%4[0-9]",rrd->stat_head->version);
  rrd->stat_head->float_cookie = FLOAT_COOKIE;
  rrd->stat_head->ds_cnt = 0;
  rrd->stat_head->rra_cnt = 0;
  read_tag(&ptr,"step","%lu",&(rrd->stat_head->pdp_step));

  /* allocate live head */
  if((rrd->live_head = calloc(1,sizeof(live_head_t)))==NULL){
    rrd_set_error("allocating rrd.live_head");
    return -1;    
  }
  read_tag(&ptr,"lastupdate","%lu",&(rrd->live_head->last_up));

  /* Data Source Definition Part */
  ptr2 = ptr;
  while (eat_tag(&ptr2,"ds") == 1){
      rrd->stat_head->ds_cnt++;
      if((rrd->ds_def = rrd_realloc(rrd->ds_def,rrd->stat_head->ds_cnt*sizeof(ds_def_t)))==NULL){
	  rrd_set_error("allocating rrd.ds_def");
	  return -1;
      };
      /* clean out memory to make sure no data gets stored from previous tasks */
      memset(&(rrd->ds_def[rrd->stat_head->ds_cnt-1]), 0, sizeof(ds_def_t));
      if((rrd->pdp_prep = rrd_realloc(rrd->pdp_prep,rrd->stat_head->ds_cnt
				  *sizeof(pdp_prep_t)))==NULL){
	rrd_set_error("allocating pdp_prep");
	return(-1);
      }
      /* clean out memory to make sure no data gets stored from previous tasks */
      memset(&(rrd->pdp_prep[rrd->stat_head->ds_cnt-1]), 0, sizeof(pdp_prep_t));

      read_tag(&ptr2,"name",DS_NAM_FMT,rrd->ds_def[rrd->stat_head->ds_cnt-1].ds_nam);

      read_tag(&ptr2,"type",DST_FMT,rrd->ds_def[rrd->stat_head->ds_cnt-1].dst);
      /* test for valid type */
      if(dst_conv(rrd->ds_def[rrd->stat_head->ds_cnt-1].dst) == -1) return -1;      

      read_tag(&ptr2,"minimal_heartbeat","%lu",
	       &(rrd->ds_def[rrd->stat_head->ds_cnt-1].par[DS_mrhb_cnt].u_cnt));
      read_tag(&ptr2,"min","%lf",&(rrd->ds_def[rrd->stat_head->ds_cnt-1].par[DS_min_val].u_val));
      read_tag(&ptr2,"max","%lf",&(rrd->ds_def[rrd->stat_head->ds_cnt-1].par[DS_max_val].u_val));

      read_tag(&ptr2,"last_ds","%30s",rrd->pdp_prep[rrd->stat_head->ds_cnt-1].last_ds);
      read_tag(&ptr2,"value","%lf",&(rrd->pdp_prep[rrd->stat_head->ds_cnt-1].scratch[PDP_val].u_val));
      read_tag(&ptr2,"unknown_sec","%lu",&(rrd->pdp_prep[i].scratch[PDP_unkn_sec_cnt].u_cnt));      
      eat_tag(&ptr2,"/ds");
      ptr=ptr2;
  }
  
  ptr2 = ptr;
  while (eat_tag(&ptr2,"rra") == 1){
      rrd->stat_head->rra_cnt++;

      /* alocate and reset rra definition areas */
      if((rrd->rra_def = rrd_realloc(rrd->rra_def,rrd->stat_head->rra_cnt*sizeof(rra_def_t)))==NULL){
	  rrd_set_error("allocating rra_def"); return -1; }      
      memset(&(rrd->rra_def[rrd->stat_head->rra_cnt-1]), 0, sizeof(rra_def_t));

      /* alocate and reset consolidation point areas */
      if((rrd->cdp_prep = rrd_realloc(rrd->cdp_prep,
				  rrd->stat_head->rra_cnt
				  *rrd->stat_head->ds_cnt*sizeof(cdp_prep_t)))==NULL){
	  rrd_set_error("allocating cdp_prep"); return -1; }

      memset(&(rrd->cdp_prep[rrd->stat_head->ds_cnt*(rrd->stat_head->rra_cnt-1)]), 
	     0, rrd->stat_head->ds_cnt*sizeof(cdp_prep_t));

      
      read_tag(&ptr2,"cf",CF_NAM_FMT,rrd->rra_def[rrd->stat_head->rra_cnt-1].cf_nam);
      /* test for valid type */
      if(cf_conv(rrd->rra_def[rrd->stat_head->rra_cnt-1].cf_nam) == -1) return -1;

      read_tag(&ptr2,"pdp_per_row","%lu",&(rrd->rra_def[rrd->stat_head->rra_cnt-1].pdp_cnt));
      read_tag(&ptr2,"xff","%lf",&(rrd->rra_def[rrd->stat_head->rra_cnt-1].par[RRA_cdp_xff_val].u_val));
      if(rrd->rra_def[rrd->stat_head->rra_cnt-1].par[RRA_cdp_xff_val].u_val > 1 ||
         rrd->rra_def[rrd->stat_head->rra_cnt-1].par[RRA_cdp_xff_val].u_val < 0)
          return -1;
      
      eat_tag(&ptr2,"cdp_prep");
      for(i=0;i<rrd->stat_head->ds_cnt;i++){
	   eat_tag(&ptr2,"ds");
	   read_tag(&ptr2,"value","%lf",&(rrd->cdp_prep[rrd->stat_head->ds_cnt*
						       (rrd->stat_head->rra_cnt-1)
						       +i].scratch[CDP_val].u_val));
	   read_tag(&ptr2,"unknown_datapoints","%lu",&(rrd->cdp_prep[rrd->stat_head->ds_cnt
						      *(rrd->stat_head->rra_cnt-1)
						      +i].scratch[CDP_unkn_pdp_cnt].u_cnt));
	   eat_tag(&ptr2,"/ds");
      }
      eat_tag(&ptr2,"/cdp_prep");
      rrd->rra_def[rrd->stat_head->rra_cnt-1].row_cnt=0;
      eat_tag(&ptr2,"database");
      ptr3 = ptr2;      
      while (eat_tag(&ptr3,"row") == 1){
	
	  if(mempool==0){
	    mempool = 1000;
	    if((rrd->rrd_value = rrd_realloc(rrd->rrd_value,
					 (rows+mempool)*(rrd->stat_head->ds_cnt)
					 *sizeof(rrd_value_t)))==NULL) {
	      rrd_set_error("allocating rrd_values"); return -1; }
	  }
	  rows++;
	  mempool--;
	  rrd->rra_def[rrd->stat_head->rra_cnt-1].row_cnt++;
	  for(i=0;i<rrd->stat_head->ds_cnt;i++){

		  rrd_value_t  * value = &(rrd->rrd_value[(rows-1)*rrd->stat_head->ds_cnt+i]);

		  read_tag(&ptr3,"v","%lf", value);
		  
		  if (
			  (rc == 1)			/* do we have to check for the ranges */
			  &&
		      (!isnan(*value))	/* not a NAN value */
		      &&
		      (					/* min defined and in the range ? */
			  (!isnan(rrd->ds_def[i].par[DS_min_val].u_val) 
			  	&& (*value < rrd->ds_def[i].par[DS_min_val].u_val)) 
			  ||				/* max defined and in the range ? */
			  (!isnan(rrd->ds_def[i].par[DS_max_val].u_val) 
			  	&& (*value > rrd->ds_def[i].par[DS_max_val].u_val))
		      )
		  ) {
		      fprintf (stderr, "out of range found [ds: %lu], [value : %0.10e]\n", i, *value);
		      *value = DNAN;
		  }
	  }
      	  eat_tag(&ptr3,"/row");                  
	  ptr2=ptr3;
      }
      eat_tag(&ptr2,"/database");
      eat_tag(&ptr2,"/rra");                  
      ptr=ptr2;
  }  
  eat_tag(&ptr,"/rrd");

  if((rrd->rra_ptr = calloc(1,sizeof(rra_ptr_t)*rrd->stat_head->rra_cnt)) == NULL) {
      rrd_set_error("allocating rra_ptr");
      return(-1);
  }

  for(i=0; i <rrd->stat_head->rra_cnt; i++) {
      rrd->rra_ptr[i].cur_row = rrd->rra_def[i].row_cnt-1;
  }
  if (ptr==NULL)
      return -1;
  return 1;
}
  
    



/* create and empty rrd file according to the specs given */

int
rrd_write(char *file_name, rrd_t *rrd)
{
    unsigned long    i,ii,val_cnt;
    FILE             *rrd_file=NULL;

    if (strcmp("-",file_name)==0){
      *rrd_file= *stdout;
    } else {
      if ((rrd_file = fopen(file_name,"wb")) == NULL ) {
	rrd_set_error("creating '%s': %s",file_name,strerror(errno));
	rrd_free(rrd);
	return(-1);
      }
    }
    fwrite(rrd->stat_head,
	   sizeof(stat_head_t), 1, rrd_file);

    fwrite(rrd->ds_def,
	   sizeof(ds_def_t), rrd->stat_head->ds_cnt, rrd_file);

    fwrite(rrd->rra_def,
	   sizeof(rra_def_t), rrd->stat_head->rra_cnt, rrd_file);
    
    fwrite(rrd->live_head, sizeof(live_head_t),1, rrd_file);

    fwrite( rrd->pdp_prep, sizeof(pdp_prep_t),rrd->stat_head->ds_cnt,rrd_file);
    
    fwrite( rrd->cdp_prep, sizeof(cdp_prep_t),rrd->stat_head->rra_cnt*
	    rrd->stat_head->ds_cnt,rrd_file);
    fwrite( rrd->rra_ptr, sizeof(rra_ptr_t), rrd->stat_head->rra_cnt,rrd_file);



    /* calculate the number of rrd_values to dump */
    val_cnt=0;
    for(i=0; i <  rrd->stat_head->rra_cnt; i++)
	for(ii=0; ii <  rrd->rra_def[i].row_cnt * rrd->stat_head->ds_cnt;ii++)
	    val_cnt++;
    fwrite( rrd->rrd_value, sizeof(rrd_value_t),val_cnt,rrd_file);

    /* lets see if we had an error */
    if(ferror(rrd_file)){
	rrd_set_error("a file error occurred while creating '%s'",file_name);
	fclose(rrd_file);	
	return(-1);
    }
    
    fclose(rrd_file);    
    return 0;
}


int
rrd_restore(int argc, char **argv) 
{
    rrd_t          rrd;
    char          *buf;
	char			rc = 0;

    /* init rrd clean */
    rrd_init(&rrd);
    if (argc<3) {
		rrd_set_error("usage rrdtool %s [--range-check/-r] file.xml file.rrd",argv[0]);
		return -1;
    }
	
	while (1) {
		static struct option long_options[] =
		{
			{"range-check",      required_argument, 0,  'r'},
			{0,0,0,0}
		};
		int option_index = 0;
		int opt;
		
		
		opt = getopt_long(argc, argv, "r", long_options, &option_index);
		
		if (opt == EOF)
			break;
		
		switch(opt) {
		case 'r':
			rc=1;
			break;
		default:
			rrd_set_error("usage rrdtool %s [--range-check|-r] file.xml file.rrd",argv[0]);
	return -1;
			break;
		}
    }
	
    if (readfile(argv[optind],&buf,0)==-1){
      return -1;
    }
    if (xml2rrd(buf,&rrd,rc)==-1) {
	rrd_free(&rrd);
	free(buf);
	return -1;
    }
    free(buf);
    if(rrd_write(argv[optind+1],&rrd)==-1){
	rrd_free(&rrd);	
	return -1;	
    };
    rrd_free(&rrd);    
    return 0;
}
