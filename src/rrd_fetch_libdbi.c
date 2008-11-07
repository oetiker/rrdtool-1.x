#include "rrd_tool.h"
#include <dbi/dbi.h>
#include <time.h>

/* the structures */
struct sql_table_helper {
  dbi_conn conn;
  int connected;
  dbi_result result;
  char const* filename;
  char const* dbdriver;
  char* table_start;
  char* table_next;
  char const* where;
  char * timestamp;
  char * value;
};

/* the prototypes */
int _sql_setparam(struct sql_table_helper* th,char* key, char* value);
int _sql_fetchrow(struct sql_table_helper* th,time_t *timestamp, rrd_value_t *value);
char* _find_next_separator(char* start,char separator);
char* _find_next_separator_twice(char*start,char separator);
char _hexcharhelper(char c);
int _inline_unescape (char* string);
double rrd_fetch_dbi_double(dbi_result *result,int idx);
long rrd_fetch_dbi_long(dbi_result *result,int idx);

/* the real code */

/* helpers to get correctly converted values from DB*/
long rrd_fetch_dbi_long(dbi_result *result,int idx) {
  char *ptmp="";
  switch (dbi_result_get_field_type_idx(result,idx)) {
  case DBI_TYPE_STRING:
    ptmp=(char*)dbi_result_get_string_idx(result,idx);
    return atoi(ptmp);
  default:
    return dbi_result_get_longlong_idx(result,idx);
  }
}

double rrd_fetch_dbi_double(dbi_result *result,int idx) {
  char *ptmp="";
  /* return NAN if NULL */
  if(dbi_result_field_is_null_idx(result,idx)) { return DNAN; }
  /* do some conversions */
  switch (dbi_result_get_field_type_idx(result,idx)) {
  case DBI_TYPE_STRING:
    ptmp=(char*)dbi_result_get_string_idx(result,idx);
    return strtod(ptmp,NULL);
  default:
    return dbi_result_get_double_idx(result,idx);
  }
}

int _sql_close(struct sql_table_helper* th) {
  /* close only if connected */
  if (th->conn) {
    /* shutdown dbi */
    dbi_conn_close(th->conn);
    dbi_shutdown();
    /* and assign empty */
    th->conn=NULL;
  }
}

int _sql_setparam(struct sql_table_helper* th,char* key, char* value) {
  char* dbi_errstr=NULL;
  /* if not connected */
  if (! th->conn) {
    /* initialize some stuff */
    th->table_next=th->table_start;
    th->result=NULL;
    th->connected=0;
    /* initialize db */
    dbi_initialize(NULL);
    th->conn=dbi_conn_new(th->dbdriver);
    /* and handle errors */
    if (! th->conn) { 
      dbi_conn_error(th->conn,(const char**)&dbi_errstr);
      rrd_set_error( "libdbi - no such driver: %s (possibly a dynamic link problem of the driver being linked without -ldbi)",dbi_errstr); 
      dbi_shutdown();
      return -1; 
    }
  }
  if (th->connected) {
    rrd_set_error( "we are already connected - can not set parameter %s=%s",key,value);
    _sql_close(th);
    return -1; 
  }
  if (dbi_conn_set_option(th->conn,key,value)) {
    dbi_conn_error(th->conn,(const char**)&dbi_errstr);
    rrd_set_error( "libdbi: problems setting %s to %s - %s",key,value,dbi_errstr);
    _sql_close(th);
    return -1;
  }
  return 0;
}

int _sql_fetchrow(struct sql_table_helper* th,time_t *timestamp, rrd_value_t *value) {
  char* dbi_errstr=NULL;
  char sql[10240];
  time_t startt=0,endt=0;
  /*connect to the database if needed */
  if (! th->conn) {
      rrd_set_error( "libdbi no parameters set for libdbi",th->filename,dbi_errstr);
      return -1;
  }
  if (! th->connected) {
    /* and now connect */
    if (dbi_conn_connect(th->conn) <0) {
      dbi_conn_error(th->conn,(const char**)&dbi_errstr);
      rrd_set_error( "libdbi: problems connecting to db with connect string %s - error: %s",th->filename,dbi_errstr);
      _sql_close(th);
      return -1;
    }
    th->connected=1;
  }
  /* now find out regarding an existing result-set */
  if (! th->result) {
    /* return if table_next is NULL */
    if (th->table_next==NULL) { 
      /* but first close connection */
      _sql_close(th);
      /* and return with end of data */
      return 0;
    }
    /* calculate the table to use next */
    th->table_start=th->table_next;
    th->table_next=_find_next_separator(th->table_start,'+');
    _inline_unescape(th->table_start);
    /* and prepare FULL SQL Statement */
    snprintf(sql,sizeof(sql)-1,"SELECT %s as rrd_time, %s as rrd_value FROM %s WHERE %s GROUP BY rrd_time",
	     th->timestamp,th->value,th->table_start,th->where);
    /* and execute sql */
    if (getenv("RRDDEBUGSQL")) { startt=time(NULL); fprintf(stderr,"RRDDEBUGSQL: %li: executing %s\n",startt,sql); }
    th->result=dbi_conn_query(th->conn,sql);
    if (startt) { endt=time(NULL);fprintf(stderr,"RRDDEBUGSQL: %li: timing %li\n",endt,endt-startt); }
    /* handle error case */
    if (! th->result) {
      dbi_conn_error(th->conn,(const char**)&dbi_errstr);      
      if (startt) { fprintf(stderr,"RRDDEBUGSQL: %li: error %s\n",endt,dbi_errstr); }
      rrd_set_error("libdbi: problems with query: %s - errormessage: %s",sql,dbi_errstr);
      _sql_close(th);
      return -1;
    }
  }
  /* and now fetch key and value */
  if (! dbi_result_next_row(th->result)) {
    /* free result */
    dbi_result_free(th->result);
    th->result=NULL;
    /* and call recursively - this will open the next table or close connection as a whole*/
    return _sql_fetchrow(th,timestamp,value);
  } 
  /* and return with flag for one value */
  *timestamp=rrd_fetch_dbi_long(th->result,1);
  *value=rrd_fetch_dbi_double(th->result,2);
  return 1;
}

char* _find_next_separator(char* start,char separator) {
  char* found=strchr(start,separator);
  /* have we found it */
  if (found) {
    /* then 0 terminate current string */
    *found=0; 
    /* and return the pointer past the separator */
    return (found+1);
  }
  /* not found, so return NULL */
  return NULL;
}

char* _find_next_separator_twice(char*start,char separator) {
  char *found=start;
  /* find next separator in string*/
  while (found) {
    /* if found and the next one is also a separator */
    if (found[1] == separator) {
      /* then 0 terminate current string */
      *found=0;
      /* and return the pointer past the current one*/
      return (found+2);
    }
    /* find next occurance */
    found=strchr(found+1,separator);
  }
  /* not found, so return NULL */
  return NULL;
}

char _hexcharhelper(char c) {
  switch (c) {
  case '0': return 0 ; break;
  case '1': return 1 ; break;
  case '2': return 2 ; break;
  case '3': return 3 ; break;
  case '4': return 4 ; break;
  case '5': return 5 ; break;
  case '6': return 6 ; break;
  case '7': return 7 ; break;
  case '8': return 8 ; break;
  case '9': return 9 ; break;
  case 'a': return 10 ; break;
  case 'b': return 11 ; break;
  case 'c': return 12 ; break;
  case 'd': return 13 ; break;
  case 'e': return 14 ; break;
  case 'f': return 15 ; break;
  case 'A': return 10 ; break;
  case 'B': return 11 ; break;
  case 'C': return 12 ; break;
  case 'D': return 13 ; break;
  case 'E': return 14 ; break;
  case 'F': return 15 ; break;
  }
  return -1;
}

int _inline_unescape (char* string) {
  char *src=string;
  char *dst=string;
  char c,h1,h2;
  while((c= *src)) {
    src++;
    if (c == '%') {
      if (*src == '%') { 
	/* increase src pointer by 1 skiping second % */
	src+=1;
      } else {
	/* try to calculate hex value from the next 2 values*/
	h1=_hexcharhelper(*src);
	if (h1<0) { rrd_set_error( "string escape error at: %s\n",string);return(1); }
	h2=_hexcharhelper(*(src+1));
	if (h2<0) { rrd_set_error( "string escape error at: %s\n",string);return(1); }
	c=h2+(h1<<4);
	/* increase src pointer by 2 skiping 2 chars */
	src+=2;
      } 
    }
    *dst=c;
    dst++;
  }
  *dst=0;
  return 0;
}

int
rrd_fetch_fn_libdbi(
    char           *filename,  /* name of the rrd */
    enum cf_en     cf_idx,     /* which consolidation function ?*/
    time_t         *start,
    time_t         *end,       /* which time frame do you want ?
			        * will be changed to represent reality */
    unsigned long  *step,      /* which stepsize do you want? 
				* will be changed to represent reality */
    unsigned long  *ds_cnt,    /* number of data sources in file */
    char           ***ds_namv, /* names of data_sources */
    rrd_value_t    **data)     /* two dimensional array containing the data */
{
  /* the separator used */
  char separator='/';
  /* a local copy of the filename - used for copying plus some pointer variables */
  char filenameworkcopy[10240];
  char *tmpptr=filenameworkcopy;
  char *nextptr=NULL;
  char *libdbiargs=NULL;
  char *sqlargs=NULL;
  /* the settings for the "works" of rrd */
  int fillmissing=0;
  unsigned long minstepsize=300;
  /* the result-set */
  long r_timestamp,l_timestamp,d_timestamp;
  double r_value,l_value,d_value;
  int r_status;
  int rows;
  long idx;
  int derive=0;
  /* the libdbi connection data and the table_help structure */
  struct sql_table_helper table_help;
  char where[10240];
  table_help.conn=NULL;
  table_help.where=where;

  /* some loop variables */
  int i=0;

  /* check header */
  if (strncmp("sql",filename,3)!=0) { 
    rrd_set_error( "formatstring wrong - %s",filename );return -1; 
  }
  if (filename[3]!=filename[4]) { 
    rrd_set_error( "formatstring wrong - %s",filename );return -1; 
  }

  /* now make this the separator */
  separator=filename[3];

  /* copy filename for local modifications during parsing */
  strncpy(filenameworkcopy,filename+5,sizeof(filenameworkcopy));

  /* get the driver */
  table_help.dbdriver=tmpptr;
  libdbiargs=_find_next_separator(tmpptr,separator);
  if (! libdbiargs) { 
    /* error in argument */
    rrd_set_error( "formatstring wrong as we did not find \"%c\"- %s",separator,table_help.dbdriver);
    return -1; 
  }

  /* now find the next double separator - this defines the args to the database */
  sqlargs=_find_next_separator_twice(libdbiargs,separator);
  if (!sqlargs) {
    rrd_set_error( "formatstring wrong for db arguments as we did not find \"%c%c\" in \"%s\"",separator,separator,libdbiargs);
    return 1;
  }

  /* now we can start with the SQL Statement - best to start with this first, 
     as then the error-handling is easier, as we do not have to handle libdbi shutdown as well */

  /* parse the table(s) */
  table_help.table_start=sqlargs;
  nextptr=_find_next_separator(table_help.table_start,separator);
  if (! nextptr) { 
    /* error in argument */
    rrd_set_error( "formatstring wrong - %s",tmpptr);
    return -1; 
  }
  /* hex-unescape the value */
  if(_inline_unescape(table_help.table_start)) { return -1; }

  /* parse the unix timestamp column */
  table_help.timestamp=nextptr;
  nextptr=_find_next_separator(nextptr,separator);
  if (! nextptr) { 
    /* error in argument */
    rrd_set_error( "formatstring wrong - %s",tmpptr);
    return -1; 
  }
  /* hex-unescape the value */
  if(_inline_unescape(table_help.timestamp)) { return -1; }

  /* parse the value column */
  table_help.value=nextptr;
  nextptr=_find_next_separator(nextptr,separator);
  if (! nextptr) { 
    /* error in argument */
    rrd_set_error( "formatstring wrong - %s",tmpptr);
    return -1; 
  }
  /* hex-unescape the value */
  if(_inline_unescape(table_help.value)) { return -1; }
  
  /* now prepare WHERE clause as empty string*/
  where[0]=0;

  /* and the where clause */
  sqlargs=nextptr;
  while(sqlargs) {
    /* find next separator */
    nextptr=_find_next_separator(sqlargs,separator);
    /* now handle fields */
    if (strcmp(sqlargs,"derive")==0) { /* the derive option with the default allowed max delta */
      derive=600;
    } else if (strcmp(sqlargs,"prediction")==0) {
      rrd_set_error("argument prediction is no longer supported in a VDEF - use new generic CDEF-functions instead");
      return -1;
    } else if (strcmp(sqlargs,"sigma")==0) {
      rrd_set_error("argument sigma is no longer supported in a VDEF - use new generic CDEF-functions instead");
      return -1;
    } else if (*sqlargs==0) { /* ignore empty */
    } else { /* else add to where string */
      if (where[0]) {strcat(where," AND ");}
      strcat(where,sqlargs);
    }
    /* and continue loop with next pointer */
    sqlargs=nextptr;
  }
  /* and unescape */
  if(_inline_unescape(where)) { return -1; }

  /* now parse LIBDBI options - this start initializing libdbi and beyond this point we need to reset the db as well in case of errors*/
  while (libdbiargs) {
    /* find separator */
    nextptr=_find_next_separator(libdbiargs,separator);
    /* now find =, separating key from value*/
    tmpptr=_find_next_separator(libdbiargs,'=');
    if (! tmpptr) { 
      rrd_set_error( "formatstring wrong for db arguments as we did not find \"=\" in \"%s\"",libdbiargs);
      _sql_close(&table_help);
      return 1;
    }
    /* hex-unescape the value */
    if(_inline_unescape(tmpptr)) { return -1; }
    /* now handle the key/value pair */
    if (strcmp(libdbiargs,"rrdminstepsize")==0) { /* allow override for minstepsize */
      i=atoi(tmpptr);if (i>0) { minstepsize=i; }
    } else if (strcmp(libdbiargs,"rrdfillmissing")==0) { /* allow override for minstepsize */
      i=atoi(tmpptr);if (i>0) { fillmissing=i; }
    } else if (strcmp(libdbiargs,"rrdderivemaxstep")==0) { /* allow override for minstepsize */
      i=atoi(tmpptr);if (i>0) { derive=i; }
    } else { /* store in libdbi, as these are parameters */
      if (_sql_setparam(&table_help,libdbiargs,tmpptr)) { 
	_sql_close(&table_help);
	return -1; 
      }
    }
    /* and continue loop with next pointer */
    libdbiargs=nextptr;
  }
  
  /* and modify step if given */
  if (*step<minstepsize) {*step=minstepsize;}
  *start-=(*start)%(*step);
  *end-=(*end)%(*step);

  /* and append the SQL WHERE Clause for the timeframe calculated above (adding AND if required) */
  if (where[0]) {strcat(where," AND ");}
  i=strlen(where);
  snprintf(where+i,sizeof(where)-1-i,"%li < %s AND %s < %li",*start,table_help.timestamp,table_help.timestamp,*end);

  /* and now calculate the number of rows in the resultset... */
  rows=((*end)-(*start))/(*step)+2;
  
  /* define the result set variables/columns returned */
  *ds_cnt=5;
  *ds_namv=(char**)malloc((*ds_cnt)*sizeof(char*));
  for (i=0;i<(int)(*ds_cnt);i++) {
    tmpptr=(char*)malloc(sizeof(char) * DS_NAM_SIZE);
    (*ds_namv)[i]=tmpptr;
    /* now copy what is required */
    switch (i) {
    case 0: strncpy(tmpptr,"min",DS_NAM_SIZE-1); break;
    case 1: strncpy(tmpptr,"avg",DS_NAM_SIZE-1); break;
    case 2: strncpy(tmpptr,"max",DS_NAM_SIZE-1); break;
    case 3: strncpy(tmpptr,"count",DS_NAM_SIZE-1); break;
    case 4: strncpy(tmpptr,"sigma",DS_NAM_SIZE-1); break;
    }
  }

  /* allocate memory for resultset (with the following columns: min,avg,max,count,sigma) */
  i=rows * sizeof(rrd_value_t)*(*ds_cnt);
  if (((*data) = malloc(i))==NULL){
    /* and return error */
    rrd_set_error("malloc failed for %i bytes",i);
    return(-1);
  }
  /* and fill with NAN */
  for(i=0;i<rows;i++) {
    (*data)[i*(*ds_cnt)+0]=DNAN; /* MIN */
    (*data)[i*(*ds_cnt)+1]=DNAN; /* AVG */
    (*data)[i*(*ds_cnt)+2]=DNAN; /* MAX */
    (*data)[i*(*ds_cnt)+3]=0;    /* COUNT */
    (*data)[i*(*ds_cnt)+4]=DNAN; /* SIGMA */
  }
  /* and assign undefined values for last - in case of derived calculation */
  l_value=DNAN;l_timestamp=0;
  /* here goes the real work processing all data */
  while((r_status=_sql_fetchrow(&table_help,&r_timestamp,&r_value))>0) {
    /* processing of value */
    /* calculate index for the timestamp */
    idx=(r_timestamp-(*start))/(*step);
    /* some out of bounds checks on idx */
    if (idx<0) { idx=0;}
    if (idx>rows) { idx=rows;}
    /* and calculate derivative if necessary */
    if (derive) {
      /* calc deltas */
      d_timestamp=r_timestamp-l_timestamp;
      d_value=r_value-l_value;
      /* assign current as last values */
      l_timestamp=r_timestamp;
      l_value=r_value;
      /* assign DNAN by default for value */
      r_value=DNAN;
      /* check for timestamp delta to be within an acceptable range */
      if ((d_timestamp>0)&&(d_timestamp<2*derive)) {
	/* only handle positive delta - avoid wrap-arrounds/counter resets showing up as spikes */
	if (d_value>0) {
	  /* and normalize to per second */
	  r_value=d_value/d_timestamp;
	}
      }
    }
    /* only add value if we have a value that is not NAN */
    if (! isnan(r_value)) {
      if ((*data)[idx*(*ds_cnt)+3]==0) { /* count is 0 so assign to overwrite DNAN */
	(*data)[idx*(*ds_cnt)+0]=r_value; /* MIN */
	(*data)[idx*(*ds_cnt)+1]=r_value; /* AVG */
	(*data)[idx*(*ds_cnt)+2]=r_value; /* MAX */
	(*data)[idx*(*ds_cnt)+3]=1;       /* COUNT */
	(*data)[idx*(*ds_cnt)+4]=r_value; /* SIGMA */
      } else {
	/* MIN */
	if ((*data)[idx*(*ds_cnt)+0]>r_value) { (*data)[idx*(*ds_cnt)+0]=r_value; }
        /* AVG - at this moment still sum - corrected in post processing */
	(*data)[idx*(*ds_cnt)+1]+=r_value;
        /* MAX */
	if ((*data)[idx*(*ds_cnt)+2]<r_value) { (*data)[idx*(*ds_cnt)+2]=r_value; }
        /* COUNT */
	(*data)[idx*(*ds_cnt)+3]++;
        /* SIGMA - at this moment still sum of squares - corrected in post processing */
	(*data)[idx*(*ds_cnt)+4]+=r_value*r_value;
      }
    }
  }
  /* and check for negativ status, pass back immediately */
  if (r_status==-1) { return -1; }

  /* post processing */
  for(idx=0;idx<rows;idx++) {
    long count=(*data)[idx*(*ds_cnt)+3];
    if (count>0) {
      /* calc deviation first */
      if (count>2) {
	r_value=count*(*data)[idx*(*ds_cnt)+4]-(*data)[idx*(*ds_cnt)+1]*(*data)[idx*(*ds_cnt)+1];
	if (r_value<0) { 
	  r_value=DNAN; 
	} else {
	  r_value=sqrt(r_value/(count*(count-1)));
	}
      }
      (*data)[idx*(*ds_cnt)+4]=r_value;
      /* now the average */
      (*data)[idx*(*ds_cnt)+1]/=count;
    }
  }

  /* and return OK */
  return 0;
}
