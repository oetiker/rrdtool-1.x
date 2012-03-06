/****************************************************************************
 * RRDtool 1.4.3  Copyright by Tobi Oetiker, 1997-2010
 ****************************************************************************
 * rrd_graph_helper.c  commandline parser functions 
 *                     this code initially written by Alex van den Bogaerdt
 ****************************************************************************/

#include "rrd_graph.h"
#define dprintf(...) if (gdp->debug&1) fprintf(stderr,__VA_ARGS__);
#define dprintfparsed(...) if (gdp->debug&2) fprintf(stderr,__VA_ARGS__);

void initParsedArguments(parsedargs_t* pa) {
  /* initialize */
  pa->arg=NULL;
  pa->arg_orig=NULL;
  pa->kv_args=NULL;
  pa->kv_cnt=0;
}

void freeParsedArguments(parsedargs_t* pa) {
  if (pa->arg) {free(pa->arg);}
  if (pa->kv_args) {free(pa->kv_args);}
  initParsedArguments(pa);
}

void dumpKeyValue(char* pre,keyvalue_t* t) {
  if (t) {
    fprintf(stderr,"%s%i: '%s' = '%s' %i\n",
	    pre,
	    t->pos,
	    t->key,
	    t->value,
	    t->flag
	    );
  } else {
    fprintf(stderr,"%sNULL\n",pre);
  }
}

void dumpArguments(parsedargs_t* pa) {
  fprintf(stderr,"====================\nParsed Arguments of: %s\n",pa->arg_orig);
  for(int i=0;i<pa->kv_cnt;i++) {
    dumpKeyValue("  ",&(pa->kv_args[i]));
  }
  fprintf(stderr,"---------------\n");
}

char* getKeyValueArgument(const char* key,int flag, parsedargs_t* pa) {
  /* we count backwords for "overwrites" */
  for(int i=pa->kv_cnt-1;i>=0;i--) {
    char* akey=(pa->kv_args[i]).key;
    if (strcmp(akey,key)==0) {
      /* set flag */
      if (flag) {pa->kv_args[i].flag=flag;}
      /* return value */
      return pa->kv_args[i].value;
    }
  }
  return NULL;
}

keyvalue_t* getFirstUnusedArgument(int flag, parsedargs_t* pa) {
  for(int i=0;i<pa->kv_cnt;i++) {
    if (! pa->kv_args[i].flag) {
      pa->kv_args[i].flag=flag;
      return &(pa->kv_args[i]);
    }
  }
  return NULL;
}

char* checkUnusedValues(parsedargs_t* pa){
  char *res=NULL;
  for(int i=0;i<pa->kv_cnt;i++) {
    if (!pa->kv_args[i].flag) {
      int len=0;
      if (res) {len=strlen(res); }
      char* t=realloc(res,len+3
		      +strlen(pa->kv_args[i].key)
		      +strlen(pa->kv_args[i].value)
		      );
      if (! t) { return res; }
      res=t;
      strcat(res,pa->kv_args[i].key);
      strcat(res,"=");
      strcat(res,pa->kv_args[i].value);
      strcat(res,":");
    }
  }
  /* if we got one, then strip the final : */
  if (res) { res[strlen(res)-1]=0; }
  /* and return res */
  return res;
}

int getMappedKeyValueArgument(const char* key,int flag, parsedargs_t* pa,
			      int* val,keyint_t** transpose) {
  /* get the value itself as string */
  char* v=getKeyValueArgument(key,flag,pa);
  /* now try to parse the value */
  if (v) {
    for(;(*transpose)->key;transpose++) {
      if (strcmp((*transpose)->key,v)==0) {
	*val=(*transpose)->value;
	return 0;
      }
    }
  }
  /* not found, so return error */
  return 1;
}

int getLong(const char* v,long *val,char**extra,int base) {
  /* try to execute the parser */
  /* NOTE that this may be a bit different from the original parser */
  *extra=NULL;
  *val=strtol(v,extra,base);
  /* and error handling */
  if (extra==NULL) {
    return 0;
  } else {
    if (*extra==v) {
      return -1; /* failed miserably */
    }  else {
      if ((*extra)[0]==0) { return 0; }
      return 1; /* got extra bytes */
    }
  }
  /* not found, so return error */
  return -2;
}   

int getDouble(const char* v, double *val,char**extra) {
  /* try to execute the parser */
  /* NOTE that this may be a bit different from the original parser */
  *extra=NULL;
  *val=strtod(v,extra);
  /* and error handling */
  if (extra==NULL) {
    return 0;
  } else {
    if (*extra==v) {
      return -1; /* failed miserably */
    }  else {
      if ((*extra)[0]==0) { return 0; }
      return 1; /* got extra bytes */
    }
  }
  /* not found, so return error */
  return -2;
}

int addToArguments(parsedargs_t* pa, char*key, char*value, int cnt) {
  /* resize the field */
  keyvalue_t* t=realloc(pa->kv_args,(pa->kv_cnt+1)*sizeof(keyvalue_t));
  if (!t) { 
    rrd_set_error("could not realloc memory"); 
    return -1;
  } else {
    /* assign pointer */
    pa->kv_args=t;
  }
  /* fill in data */
  pa->kv_args[pa->kv_cnt].key=key;
  pa->kv_args[pa->kv_cnt].value=value;
  pa->kv_args[pa->kv_cnt].pos=cnt;
  pa->kv_args[pa->kv_cnt].flag=0;
  pa->kv_cnt++;
  /* and return ok */
  return 0;
}

char *poskeys[]={"pos0","pos1","pos2","pos3","pos4","pos5","pos6","pos7","pos8","pos9"};
int parseArguments(const char* origarg, parsedargs_t* pa) {
  initParsedArguments(pa);
  /* now assign a copy */
  pa->arg=strdup(origarg);
  if (!pa->arg) { rrd_set_error("Could not allocate memory");return -1; }
  pa->arg_orig=origarg;

  /* first split arg into : */
  char  last=1;
  char c;
  int cnt=0;
  int poscnt=0;
  char* pos=pa->arg;
  char* field=pos;
  do {
    c=*pos;
    if (! field) { field=pos;cnt++;}
    switch (c) {
      /* if the char is a backslash, then this escapes the next one */
    case '\\':
      if (pos[1]) { pos++; }
      break;
    case 0:
    case ':':
      /* null and : separate the string */
      *pos=0;
      /* handle the case where we have got an = */
      /* find equal sign */
      char* equal=field;
      for (equal=field;(*equal)&&(*equal!='=');equal++) { ; }
      /* if we are on position 1 then check for position 0 to be [CV]?DEV */
      int checkforkeyvalue=1;
      /* nw define key to use */
      char *key,*value;
      if ((*equal=='=') && (checkforkeyvalue)) {
	*equal=0;
	key=field;
	value=equal+1;
      } else {
	if ((poscnt>0)&&(strcmp(field,"STACK")==0)) {
	  key="stack";
	  value="1";
	} else if ((poscnt>0)&&(strcmp(field,"strftime")==0)) {
	  key="strftime";
	  value="1";
	} else {
	  if (poscnt>9) {
	    rrd_set_error("too many positional arguments"); 
	    freeParsedArguments(pa);
	    return -1;
	  }
	  key=poskeys[poscnt];
	  poscnt++;
	  value=field;
	}
      }
      /* do some synonym translations */
      if (strcmp(key,"label")==0) { key="legend"; }
      if (strcmp(key,"colour")==0) { key="color"; }
      if (strcmp(key,"colour2")==0) { key="color2"; }

      /* add to fields */
      if (addToArguments(pa,key,value,cnt)) {
	freeParsedArguments(pa);
	return -1;
      }

      /* and reset field */
      field=NULL;
      break;
    default:
      break;
    }
    /* assign last */
    last=c;
    /* and step to next one byte */
    pos++;
  } while (c);
  /* and return OK */
  return 0;
}

int parse_color( const char *const, struct gfx_color_t *);
int parse_color( const char *const string, struct gfx_color_t *c)
{
  unsigned int r = 0, g = 0, b = 0, a = 0, i;
  
  /* matches the following formats:
  ** RGB
  ** RGBA
  ** RRGGBB
  ** RRGGBBAA
  */
  
  i = 0;
  while (string[i] && isxdigit((unsigned int) string[i]))
    i++;
  if (string[i] != '\0')
    return 1;       /* garbage follows hexdigits */
  switch (i) {
  case 3:
  case 4:
    sscanf(string, "%1x%1x%1x%1x", &r, &g, &b, &a);
    r *= 0x11;
    g *= 0x11;
    b *= 0x11;
    a *= 0x11;
    if (i == 3)
      a = 0xFF;
    break;
  case 6:
  case 8:
    sscanf(string, "%02x%02x%02x%02x", &r, &g, &b, &a);
    if (i == 6)
      a = 0xFF;
    break;
  default:
    return 1;       /* wrong number of digits */
  }
  /* I wonder how/why this works... */
  *c=gfx_hex_to_col(r << 24 | g << 16 | b << 8 | a);
  return 0;
}

/* this would allow for 240 different values */
#define PARSE_FIELD1       (1L<<60)
#define PARSE_FIELD2       (1L<<61)
#define PARSE_FIELD3       (1L<<62)
#define PARSE_FIELD4       (1L<<63)
#define PARSE_POSITIONAL   (1L<<59)
#define PARSE_ONYAXIS      (1L<<58)
#define PARSE_VNAME        (PARSE_FIELD1|(1L<< 0))
#define PARSE_RRD          (PARSE_FIELD1|(1L<< 1))
#define PARSE_DS           (PARSE_FIELD1|(1L<< 2))
#define PARSE_CF           (PARSE_FIELD1|(1L<< 3))
#define PARSE_COLOR        (PARSE_FIELD1|(1L<< 4))
#define PARSE_COLOR2       (PARSE_FIELD1|(1L<< 5))
#define PARSE_LEGEND       (PARSE_FIELD1|(1L<< 6))
#define PARSE_RPN          (PARSE_FIELD1|(1L<< 7))
#define PARSE_START        (PARSE_FIELD1|(1L<< 8))
#define PARSE_STEP         (PARSE_FIELD1|(1L<< 9))
#define PARSE_END          (PARSE_FIELD1|(1L<<10))
#define PARSE_STACK        (PARSE_FIELD1|(1L<<11))
#define PARSE_LINEWIDTH    (PARSE_FIELD1|(1L<<12))
#define PARSE_XAXIS        (PARSE_FIELD1|(1L<<13))
#define PARSE_YAXIS        (PARSE_FIELD1|(1L<<14))
#define PARSE_REDUCE       (PARSE_FIELD1|(1L<<15))
#define PARSE_DASHES       (PARSE_FIELD1|(1L<<20))
#define PARSE_HEIGHT       (PARSE_FIELD1|(1L<<21))
#define PARSE_FORMAT       (PARSE_FIELD1|(1L<<22))
#define PARSE_STRFTIME     (PARSE_FIELD1|(1L<<23))
#define PARSE_FRACTION     (PARSE_FIELD1|(1L<<24))
/* VNAME Special cases for generic parsing */
#define PARSE_VNAMEDEF            (PARSE_VNAME|(1L<<57))
#define PARSE_VNAMEREF            (PARSE_VNAME|(1L<<56))
#define PARSE_VNAMEREFNUM         (PARSE_VNAMEREF|(1L<<55))
#define PARSE_VNAMEREFNUMNOPARSE  (PARSE_FIELD1|(1L<<54))
/* special positional cases */
#define PARSE_VNAMERRDDSCF     (PARSE_POSITIONAL|PARSE_VNAMEDEF|PARSE_RRD|PARSE_DS|PARSE_CF)
#define PARSE_VNAMECOLORLEGEND (PARSE_POSITIONAL|PARSE_VNAMEREFNUM|PARSE_COLOR|PARSE_COLOR2|PARSE_LEGEND)
#define PARSE_VNAMECOLORFRACTIONLEGEND (PARSE_VNAMECOLORLEGEND|PARSE_FRACTION)
#define PARSE_VNAMERPN         (PARSE_POSITIONAL|PARSE_VNAMEDEF|PARSE_RPN)
#define PARSE_VNAMEREFPOS      (PARSE_POSITIONAL|PARSE_VNAMEREF)

graph_desc_t* newGraphDescription(image_desc_t *const,enum gf_en,parsedargs_t*,unsigned long);
graph_desc_t* newGraphDescription(image_desc_t *const im,enum gf_en gf,parsedargs_t* pa,unsigned long bits) {
  /* check that none of the othe bitfield marker is set */
  if ((bits&PARSE_FIELD1)&&((bits&(PARSE_FIELD2|PARSE_FIELD3|PARSE_FIELD4)))) {
    rrd_set_error("newGraphDescription: bad bitfield1 value %08x",bits);return NULL; }
  /* the normal handler that adds to img */
  if (gdes_alloc(im)) { return NULL; }
  /* set gdp */
  graph_desc_t *gdp= &im->gdes[im->gdes_c - 1];

  /* set some generic things */
  gdp->gf=gf;
  if (1) {
    char *t,*x; 
    long debug=0;
    if ((t=getKeyValueArgument("debug",1,pa)) && ((getLong(t,&debug,&x,10)))) {
      rrd_set_error("Bad debug value: %s",t); return NULL; }
    gdp->debug=debug;
  }

  /* and the "flagged" parser implementation 
   *
   * first the fields with legacy positional args 
   */
#define bitscmp(v) ((bits&v)==v)
  char* vname=NULL;
  if (bitscmp(PARSE_VNAME)) { vname=getKeyValueArgument("vname",1,pa); 
    dprintfparsed("got vname: %s\n",vname);}
  char *rrd=NULL;
  if (bitscmp(PARSE_RRD)) { rrd=getKeyValueArgument("rrd",1,pa);
    dprintfparsed("got rrd: %s\n",rrd);}
  char *ds=NULL;
  if (bitscmp(PARSE_DS)) { ds=getKeyValueArgument("ds",1,pa);
    dprintfparsed("got ds: %s\n",ds);}
  char *cf=NULL;
  if (bitscmp(PARSE_CF)) { cf=getKeyValueArgument("cf",1,pa);
    dprintfparsed("got cf: %s\n",cf);}
  char *color=NULL;
  if (bitscmp(PARSE_COLOR)) { color=getKeyValueArgument("color",1,pa);
    dprintfparsed("got color: %s\n",color);}
  char *color2=NULL;
  if (bitscmp(PARSE_COLOR2)) { color2=getKeyValueArgument("color2",1,pa); 
    dprintfparsed("got color2: %s\n",color2);}
  char *rpn=NULL;
  if (bitscmp(PARSE_RPN)) { rpn=getKeyValueArgument("rpn",1,pa); 
    dprintfparsed("got rpn: %s\n",rpn);}
  char *legend=NULL;
  if (bitscmp(PARSE_LEGEND)) { legend=getKeyValueArgument("legend",1,pa); 
    dprintfparsed("got legend: %s\n",legend);}
  char *fraction=NULL;
  if (bitscmp(PARSE_FRACTION)) { fraction=getKeyValueArgument("fraction",1,pa); 
    dprintfparsed("got fraction: %s\n",fraction);}
  /* 
   * here the ones without delayed assigns (which are for positional parsers) 
  */
  if (bitscmp(PARSE_FORMAT)) { 
    char *format=getKeyValueArgument("format",1,pa);
    if(format) {
      strncpy(gdp->format,format,FMT_LEG_LEN);
      dprintfparsed("got format: %s\n",format);
    }
  }
  if (bitscmp(PARSE_STRFTIME)) { 
    char *strft=getKeyValueArgument("strftime",1,pa);
    gdp->strftm=(strft)?1:0;
    dprintfparsed("got strftime: %s\n",strft);
  }
  if (bitscmp(PARSE_STACK)) { 
    char *stack=getKeyValueArgument("stack",1,pa);
    gdp->stack=(stack)?1:0;
    dprintfparsed("got stack: %s\n",stack);
  }
  if (bitscmp(PARSE_REDUCE)) { 
    char *reduce=getKeyValueArgument("reduce",1,pa);
    if (reduce) { 
      gdp->cf_reduce=cf_conv(reduce);
      dprintfparsed("got reduce: %s (%i)\n",reduce,gdp->cf_reduce);
      if (((int)gdp->cf_reduce)==-1) { rrd_set_error("bad reduce CF: %s",reduce); return NULL; }
    }
  }
  if (bitscmp(PARSE_XAXIS)) {
    long xaxis=0;
    char *t,*x; 
    if ((t=getKeyValueArgument("xaxis",1,pa)) && ((getLong(t,&xaxis,&x,10))||(xaxis<1)||(xaxis>MAX_AXIS))) {
      rrd_set_error("Bad xaxis value: %s",t); return NULL; }
    dprintfparsed("got xaxis: %s (%li)\n",t,xaxis);
    gdp->xaxisidx=xaxis;
  }
  if (bitscmp(PARSE_YAXIS)) {
    long yaxis=0;
    char *t,*x; 
    if ((t=getKeyValueArgument("yaxis",1,pa)) && ((getLong(t,&yaxis,&x,10))||(yaxis<1)||(yaxis>MAX_AXIS))) {
      rrd_set_error("Bad yaxis value: %s",t); return NULL; }
    dprintfparsed("got yaxis: %s (%li)\n",t,yaxis);
    gdp->yaxisidx=yaxis;
  }
  if (bitscmp(PARSE_LINEWIDTH)) {
    double linewidth=0;
    char *t,*x; 
    if ((t=getKeyValueArgument("linewidth",1,pa))&&(*t!=0)) {
      if ((getDouble(t,&linewidth,&x))||(linewidth<=0)) {
	rrd_set_error("Bad line width: %s",t); return NULL;
      }
      dprintfparsed("got linewidth: %s (%g)\n",t,linewidth);
      gdp->linewidth=linewidth;
    }
  }
  if (bitscmp(PARSE_HEIGHT)) {
    double height=0;
    char *t,*x; 
    if ((t=getKeyValueArgument("height",1,pa))&&(*t!=0)) {
      if (getDouble(t,&height,&x)) {
	rrd_set_error("Bad height: %s",t); return NULL;
      }
      dprintfparsed("got height: %s (%g)\n",t,height);
      gdp->gradheight=height;
    }
  }
  if (bitscmp(PARSE_STEP)) {
    long step=0;
    char *t,*x; 
    if ((t=getKeyValueArgument("step",1,pa)) && ((getLong(t,&step,&x,10))||(step<1))) {
      rrd_set_error("Bad step value: %s",t); return NULL; }
    dprintfparsed("got step: %s (%li)\n",t,step);
    gdp->step=step;
  }
  if ((bitscmp(PARSE_START)||bitscmp(PARSE_END))) {
    /* these should get done together to use the start/end code correctly*/
    char* parsetime_error;
    /* first start */
    char* start;
    rrd_time_value_t start_tv;
    start_tv.type   = ABSOLUTE_TIME;
    start_tv.offset = 0;
    localtime_r(&gdp->start, &start_tv.tm);   
    if (bitscmp(PARSE_START)) { 
      start=getKeyValueArgument("start",1,pa);
      if ((start)&&(parsetime_error = rrd_parsetime(start, &start_tv))) {
	rrd_set_error("start time: %s", parsetime_error);return NULL; }
      dprintfparsed("got start: %s\n",start);
    }
    /* now end */
    char* end;
    rrd_time_value_t end_tv;
    end_tv.type   = ABSOLUTE_TIME;
    end_tv.offset = 0;
    localtime_r(&gdp->end, &end_tv.tm);   
    if (bitscmp(PARSE_END)) { 
      end=getKeyValueArgument("end",1,pa);
      if ((end)&&(parsetime_error = rrd_parsetime(end, &end_tv))) {
	rrd_set_error("end time: %s", parsetime_error); return NULL; }
      dprintfparsed("got end: %s\n",end);
    }
    /* and now put the pieces together (relative times like start=end-2days) */
    time_t    start_tmp = 0, end_tmp = 0;
    if (rrd_proc_start_end(&start_tv, &end_tv, &start_tmp, &end_tmp) == -1) {
      return NULL;
    }
    dprintfparsed("got start %s translated to: %lld\n",start,(long long int)start_tmp);
    dprintfparsed("got end %s translated to: %lld\n",end,(long long int)end_tmp);

    /* check some ranges */
    if (start_tmp < 3600 * 24 * 365 * 10) {
      rrd_set_error("the first entry to fetch should be "
		    "after 1980 (%ld)", start_tmp);
      return NULL; }
    if (end_tmp < start_tmp) {
      rrd_set_error("start (%ld) should be less than end (%ld)",
		    start_tmp, end_tmp);
      return NULL; }
    
    /* and finally set it irrespectively of if it has been set or not
     * it may have been a relative time and if you just set it partially
     * then that is wrong...
     */
    gdp->start = start_tmp;
    gdp->start_orig = start_tmp;
    gdp->end = end_tmp;
    gdp->end_orig = end_tmp;
  }
  if (bitscmp(PARSE_DASHES)) {
    char* dashes=getKeyValueArgument("dashes",1,pa);
    /* if we got dashes */
    if (dashes) {
      gdp->offset = 0;
      /* count the , in  dashes */
      int cnt=0;for(char*t=dashes;(*t)&&(t=strchr(t,','));t++,cnt++) {;}
      dprintfparsed("Got dashes argument: %s with %i comma\n",dashes,cnt);
      /* now handle */
      gdp->ndash = cnt+1;
      gdp->p_dashes = (double *) malloc(sizeof(double)*(gdp->ndash+1));
      /* now loop dashes */
      for(int i=0;i<gdp->ndash;i++) {
	char *x; 
	int f=getDouble(dashes,&gdp->p_dashes[i],&x);
	if(f<0) {
	  rrd_set_error("Could not parse number: %s",dashes); return NULL;
	}
	/* we should have this most of the time */
	dprintfparsed("Processed %s to %g at index %i\n",dashes,gdp->p_dashes[i],i);
	if (f>0) { 
	  if (*x!=',') {
	    rrd_set_error("expected a ',' at : %s",x); return NULL;}
	  dashes=x+1;
	}
	if ((f==0)&&(i!=gdp->ndash-1)) {
	  rrd_set_error("unexpected end at : %s",dashes); return NULL;}
      }
    }
    char* dashoffset=getKeyValueArgument("dash-offset",1,pa);
    if (dashoffset) {
      char* x;
      if (getDouble(dashes,&gdp->offset,&x)) {
	rrd_set_error("Could not parse dash-offset: %s",dashoffset); return NULL; }
    }
  }

  /* here now the positional(=legacy) parsers which are EXCLUSIVE - SO ELSE IF !!! 
   * we also only parse the extra here and assign just further down
   * TODO maybe we can generalize this a bit more...
   */
  if (bitscmp(PARSE_VNAMERRDDSCF)) {
    if ((!vname)||(!rrd)) {
      /* get the first unused argument */
      keyvalue_t* first=getFirstUnusedArgument(1,pa);
      if (!first) { rrd_set_error("No argument for definition of vdef/rrd in %s",pa->arg_orig); return NULL; }
      dprintfparsed("got positional vname and rrd: %s - %s\n",first->key,first->value);
      if (!vname) {vname=first->key;}
      if (!rrd) {rrd=first->value; }
    }
    /* and now look for datasource */
    if (!ds) {
      /* get the first unused argument */
      keyvalue_t* first=getFirstUnusedArgument(1,pa);
      if (!first) { rrd_set_error("No argument for definition of DS in %s",pa->arg_orig); return NULL; }
      dprintfparsed("got positional ds: %s - \n",first->value);
      ds=first->value;
    }
    /* and for CF */
    if (!cf) {
      /* get the first unused argument */
      keyvalue_t* first=getFirstUnusedArgument(1,pa);
      if (!first) { rrd_set_error("No argument for definition of CF in %s",pa->arg_orig); return NULL; }
      dprintfparsed("got positional cf: %s - \n",first->value);
      cf=first->value;
    }    
  } else if (bitscmp(PARSE_VNAMECOLORLEGEND)) {
    /* vname */
    if (!vname) {
      keyvalue_t* first=getFirstUnusedArgument(1,pa);
      if (first) { vname=first->value;
      } else { rrd_set_error("No positional VNAME"); return NULL; }
    }
    /* fraction added into the parsing mix for TICK */
    if ((bitscmp(PARSE_VNAMECOLORFRACTIONLEGEND))&&(!fraction)) {
      keyvalue_t* first=getFirstUnusedArgument(1,pa);
      if (first) { fraction=first->value;
      } else { rrd_set_error("No positional FRACTION"); return NULL; }
    }    
    /* legend */
    if (!legend) {
      keyvalue_t* first=getFirstUnusedArgument(1,pa);
      if (first) { legend=first->value;
	dprintfparsed("got positional legend: %s - \n",first->value);
      } else { rrd_set_error("No positional legend found"); return NULL; }
    }
  } else if (bitscmp(PARSE_VNAMERPN)) {
    if ((!vname)||(!rpn)) {
      /* get the first unused argument */
      keyvalue_t* first=getFirstUnusedArgument(1,pa);
      if (!first) { rrd_set_error("No argument for definition of vdef/rrd in %s",pa->arg_orig); return NULL; }
      dprintfparsed("got positional vname and rpn: %s - %s\n",first->key,first->value);
      if (!vname) {vname=first->key;}
      if (!rpn) {rpn=first->value; }
    }
  } else if (bitscmp(PARSE_VNAMEREFPOS)) {
    if ((!vname)) {
      /* get the first unused argument */
      keyvalue_t* first=getFirstUnusedArgument(1,pa);
      if (!first) { rrd_set_error("No argument for definition of vdef/rrd in %s",pa->arg_orig); return NULL; }
      dprintfparsed("got positional vname and rrd: %s - %s\n",first->key,first->value);
      if (!vname) {vname=first->value;}
    }
  }
    
  /* and set some of those late assignments to accomodate the legacy parser*/
  /* first split vname into color */
  if (vname) { 
    /* check for color */
    char *h1=strchr(vname,'#');
    char* h2=NULL;
    if (h1) {
      *h1=0;h1++;
      dprintfparsed("got positional color: %s - \n",h1);
      h2=strchr(h1,'#');
      if (h2) {
	*h2=0;h2++;
	dprintfparsed("got positional color2: %s - \n",h2);
      }
    }
    if (bitscmp(PARSE_COLOR) && (! color) && (h1)) { color=h1;}
    if (bitscmp(PARSE_COLOR2) && (! color2) && (h2)) { color2=h2;}
  }

  /* check if we are reusing the vname */
  if (vname) {
    int idx=find_var(im, vname);
    dprintfparsed("got positional index %i for %s - \n",idx,vname);
    
    /* some handling */
    if (bitscmp(PARSE_VNAMEDEF)) {
      if (idx>=0) {
	rrd_set_error("trying to reuse vname %s",vname); return NULL; }
    } else if (bitscmp(PARSE_VNAMEREF)) {
      if (idx>=0) {
	gdp->vidx=idx;    
      } else if (bitscmp(PARSE_VNAMEREFNUM)) {
	if (!bitscmp(PARSE_VNAMEREFNUMNOPARSE)) {
	  double val;
	  char *x; 
	  int f=getDouble(vname,&val,&x);
	  if (f) {
	    rrd_set_error("error parsing number %s",vname); return NULL;
	  }
	  gdp->yrule=val;
	} else {
	  rrd_set_error("vname %s not found",vname); return NULL;
	}
      } else {
	gdp->vidx=-1;
      }
    }
  }
  
  /* and assign it */
  if (vname) { strncpy(gdp->vname,vname,MAX_VNAME_LEN + 1); }
  if (rrd) { strncpy(gdp->rrd,rrd,1024); }
  if (ds) { strncpy(gdp->ds_nam,ds,DS_NAM_SIZE); }
  if (cf) { 
    gdp->cf=cf_conv(cf);
    if (((int)gdp->cf)==-1) { 
      rrd_set_error("bad CF: %s",cf); return NULL; }
  } else { if (bitscmp(PARSE_CF)) {gdp->cf=-1;}}
  if ((color)&&(parse_color(color,&(gdp->col)))) { return NULL; }
  if ((color2)&&(parse_color(color2,&(gdp->col2)))) { return NULL; }
  if (rpn) {gdp->rpn=rpn;}
  if ((legend)&&(*legend!=0)) {
    /* some spacing before we really start with the legend - needed for some reason */
    char* t=gdp->legend;
    *t=' ';t++;
    *t=' ';t++;
    /* and copy it into place */
    strncpy(t,legend,FMT_LEG_LEN);
  }
  if (fraction) {
    if (strcmp(fraction,"vname")==0) {
      /* check that vname is really a DEF|CDEF */
      if (im->gdes[gdp->vidx].gf != GF_DEF && im->gdes[gdp->vidx].gf != GF_CDEF) {
	rrd_set_error("variable '%s' not DEF nor CDEF when using dynamic fractions", gdp->vname);
	return NULL;
      }
      /* add as flag to use (c)?def */
      gdp->cf=CF_LAST;
      gdp->yrule=0.5;
    } else {
      /* parse number */
      double val;
      char *x; 
      int f=getDouble(fraction,&val,&x);
      if (f) {
	rrd_set_error("error parsing number %s",vname); return NULL;
      }
      gdp->yrule=val;
    }
  }
  /* and return it */
  return gdp;
}

/* and some defines */
#define set_match(str,pat,cmd) if (strcmp(pat, str) == 0)  { cmd ;}

/* prototypes */
int parse_axis(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_def(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_cvdef(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_line(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_area(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_stack(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_print(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_gprint(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_comment(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_hvrule(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_grad(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_tick(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_textalign(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_shift(enum gf_en,parsedargs_t*,image_desc_t *const);
int parse_xport(enum gf_en,parsedargs_t*,image_desc_t *const);

/* implementations */
int parse_axis(enum gf_en gf,parsedargs_t*pa,image_desc_t *const im){
#if 0
  /* define X or y axis */
  axis_t *a=im->xaxis;
  if (gf == GF_YAXIS) { a=im->yaxis; }
  /* try to parse the number */
  char* cmd=getKeyValueArgument("cmd",1,pa);
  if (cmd[5]) {
    int num=atoi(cmd+5);
    if ((num<1)||(num>MAX_AXIS)) {
      rrd_set_error("invalid axis ID %i in %s - should be in range [1:%i]",num,cmd,MAX_AXIS);
      return 1;
    }
    /* and forward by that much */
    a=a+(num-1);
  }

  /* and set type */
  char* t=getKeyValueArgument("type",1,pa);
  if (t) {
    set_match(t,"TIME",a->type=AXIS_TYPE_TIME)
    else 
      set_match(t,"LINEAR",a->type=AXIS_TYPE_LINEAR)
      else
	set_match(t,"LOGARITHMIC",a->type=AXIS_TYPE_LOGARITHMIC)
      else {
	rrd_set_error("unsupported axis type %s",t); 
	return 1;
      }
  }
  /* and other stuff */
  a->bounds.lowertxt=getKeyValueArgument("min",1,pa);
  a->bounds.uppertxt=getKeyValueArgument("max",1,pa);
#endif
  /* and return */
  return 0;
}

int parse_def(enum gf_en gf,parsedargs_t*pa,image_desc_t *const im){
  /* get new graph that we fill */
  graph_desc_t *gdp=newGraphDescription(im,gf,pa,
					PARSE_VNAMERRDDSCF 
					|PARSE_START
					|PARSE_STEP
					|PARSE_END
					|PARSE_REDUCE
					); 
  if (!gdp) { return -1;}

  /* debugging output */
  dprintf("=================================\n");
  dprintf("DEF   : %s\n",pa->arg_orig);
  dprintf("VNAME : %s\n",gdp->vname);
  dprintf("RRD   : %s\n",gdp->rrd);
  dprintf("DS    : %s\n",gdp->ds_nam);
  dprintf("CF    : %i\n",gdp->cf);
  dprintf("START : (%lld)\n",(long long int)gdp->start);
  dprintf("STEP  : (%lld)\n",(long long int)gdp->step);
  dprintf("END   : (%lld)\n",(long long int)gdp->end);
  dprintf("REDUCE: (%i)\n",gdp->cf_reduce);
  dprintf("=================================\n");

  /* and return fine */
  return 0;
}  

int parse_cvdef(enum gf_en gf,parsedargs_t*pa,image_desc_t *const im){
  /* get new graph that we fill */
  graph_desc_t *gdp=newGraphDescription(im,gf,pa,
					PARSE_VNAMERPN 
					); 
  if (!gdp) { return -1;}

  /* handle RPN parsing */
  if (gf==GF_CDEF) {
    /* parse rpn */
    if ((gdp->rpnp= rpn_parse((void *) im, gdp->rpn, &find_var_wrapper)) == NULL) {
      return -1; }
  } else {
    /* parse vdef, as vdef_parse is a bit "stupid" right now we have to touch things here */
    /* so find first , */
    char*c=strchr(gdp->rpn,',');
    if (! c) { rrd_set_error("Comma expected in VDEF definition %s",gdp->rpn); return -1;}
    /* found a comma, so copy the first part to ds_name (re/abusing it) */
    *c=0;
    strncpy(gdp->ds_nam,gdp->rpn,DS_NAM_SIZE);
    *c=',';
    /* trying to find the vidx for that name */
    gdp->vidx = find_var(im, gdp->ds_nam);
    if (gdp->vidx<0) { *c=',';
      rrd_set_error("Not a valid vname: %s in line %s", gdp->ds_nam, gdp->rpn);
      return -1;}
    if (im->gdes[gdp->vidx].gf != GF_DEF && im->gdes[gdp->vidx].gf != GF_CDEF) {
      rrd_set_error("variable '%s' not DEF nor "
		    "CDEF in VDEF '%s'", gdp->ds_nam, gdp->rpn);
      return 1;
    }
    /* and parsing the rpn */
    int r=vdef_parse(gdp, c+1);
    /* original code does not check here for some reason */
    if (r) { return -1; }
  }

  /* debugging output */
  dprintf("=================================\n");
  if (gf==GF_CDEF) {
    dprintf("CDEF  : %s\n",pa->arg_orig);
  } else {
    dprintf("VDEF  : %s\n",pa->arg_orig);
  }
  dprintf("VNAME : %s\n",gdp->vname);
  dprintf("RPN   : %s\n",gdp->rpn);
  dprintf("=================================\n");

  /* and return fine */
  return 0;
}  


int parse_line(enum gf_en gf,parsedargs_t*pa,image_desc_t *const im){
  /* get new graph that we fill */
  graph_desc_t *gdp=newGraphDescription(im,gf,pa,
					PARSE_VNAMECOLORLEGEND
					|PARSE_STACK
					|PARSE_LINEWIDTH
					|PARSE_DASHES
					|PARSE_XAXIS
					|PARSE_YAXIS
					); 
  if (!gdp) { return -1;}

  /* debug output */
  dprintf("=================================\n");
  dprintf("LINE  : %s\n",pa->arg_orig);
  if (gdp->vidx<0) {
    dprintf("VAL   : %g\n",gdp->yrule);
  } else {
    dprintf("VNAME : %s (%li)\n",gdp->vname,gdp->vidx);
  }
  dprintf("COLOR : r=%g g=%g b=%g a=%g\n",
	  gdp->col.red,gdp->col.green,gdp->col.blue,gdp->col.alpha);
  dprintf("COLOR2: r=%g g=%g b=%g a=%g\n",
	  gdp->col2.red,gdp->col2.green,gdp->col2.blue,gdp->col2.alpha);
  dprintf("LEGEND: %s\n",gdp->legend);
  dprintf("STACK : %i\n",gdp->stack);
  dprintf("WIDTH : %g\n",gdp->linewidth);
  dprintf("XAXIS : %i\n",gdp->xaxisidx);
  dprintf("YAXIS : %i\n",gdp->yaxisidx);
  if (gdp->ndash) {
    dprintf("DASHES: %i - %g",gdp->ndash,gdp->p_dashes[0]);
    for(int i=1;i<gdp->ndash;i++){dprintf(", %g",gdp->p_dashes[i]);}
    dprintf("\n");
  }
  dprintf("=================================\n");

  /* and return fine */
  return 0;
}

int parse_area(enum gf_en gf,parsedargs_t*pa,image_desc_t *const im){
  /* get new graph that we fill */
  graph_desc_t *gdp=newGraphDescription(im,gf,pa,
					PARSE_VNAMECOLORLEGEND
					|PARSE_STACK
					|PARSE_XAXIS
					|PARSE_YAXIS
					|PARSE_HEIGHT
					); 
  if (!gdp) { return -1;}

  /* debug output */
  dprintf("=================================\n");
  dprintf("AREA  : %s\n",pa->arg_orig);
  if (gdp->vidx<0) {
    dprintf("VAL   : %g\n",gdp->yrule);
  } else {
    dprintf("VNAME : %s (%li)\n",gdp->vname,gdp->vidx);
  }
  dprintf("COLOR : r=%g g=%g b=%g a=%g\n",
	  gdp->col.red,gdp->col.green,gdp->col.blue,gdp->col.alpha);
  dprintf("COLOR2: r=%g g=%g b=%g a=%g\n",
	  gdp->col2.red,gdp->col2.green,gdp->col2.blue,gdp->col2.alpha);
  dprintf("LEGEND: %s\n",gdp->legend);
  dprintf("STACK : %i\n",gdp->stack);
  dprintf("XAXIS : %i\n",gdp->xaxisidx);
  dprintf("YAXIS : %i\n",gdp->yaxisidx);
  dprintf("=================================\n");

  /* and return fine */
  return 0;
}

int parse_stack(enum gf_en gf,parsedargs_t*pa,image_desc_t *const im){
  /* get new graph that we fill */
  graph_desc_t *gdp=newGraphDescription(im,gf,pa,
					PARSE_VNAMECOLORLEGEND
					|PARSE_XAXIS
					|PARSE_YAXIS
					); 
  if (!gdp) { return -1;}
  gdp->stack=1;
  /* and try to get the one index before ourselves */
  long i;
  for (i=im->gdes_c;(gdp->gf==gf)&&(i>=0);i--) {
    dprintfparsed("trying to process entry %li with type %u\n",i,im->gdes[i].gf);
    switch (im->gdes[i].gf) {
    case GF_LINE:
    case GF_AREA:
      gdp->gf=im->gdes[i].gf;
      gdp->linewidth=im->gdes[i].linewidth;
      dprintfparsed("found matching LINE/AREA at %li with type %u\n",i,im->gdes[i].gf);
      break;
    default: break;
    }
  }
  /* error the unhandled */
  if (gdp->gf==gf) { 
    rrd_set_error("No previous LINE or AREA found for %s",pa->arg_orig); return -1;}

  /* debug output */
  dprintf("=================================\n");
  dprintf("STACK : %s\n",pa->arg_orig);
  if (gdp->vidx<0) {
    dprintf("VAL   : %g\n",gdp->yrule);
  } else {
    dprintf("VNAME : %s (%li)\n",gdp->vname,gdp->vidx);
  }
  dprintf("COLOR : r=%g g=%g b=%g a=%g\n",
	  gdp->col.red,gdp->col.green,gdp->col.blue,gdp->col.alpha);
  dprintf("COLOR2: r=%g g=%g b=%g a=%g\n",
	  gdp->col2.red,gdp->col2.green,gdp->col2.blue,gdp->col2.alpha);
  dprintf("LEGEND: %s\n",gdp->legend);
  dprintf("STACK : %i\n",gdp->stack);
  dprintf("WIDTH : %g\n",gdp->linewidth);
  dprintf("XAXIS : %i\n",gdp->xaxisidx);
  dprintf("YAXIS : %i\n",gdp->yaxisidx);
  dprintf("DASHES: TODI\n");
  dprintf("=================================\n");

  /* and return fine */
  return 0;
}

int parse_hvrule(enum gf_en gf,parsedargs_t*pa,image_desc_t *const im){
  /* get new graph that we fill */
  graph_desc_t *gdp=newGraphDescription(im,gf,pa,
					PARSE_VNAMECOLORLEGEND
					|PARSE_VNAMEREFNUMNOPARSE
					|PARSE_XAXIS
					|PARSE_YAXIS
					|PARSE_DASHES
					); 
  if (!gdp) { return -1;}

  /* check that vidx is of type VDEF */
  if (im->gdes[gdp->vidx].gf != GF_VDEF) {
    rrd_set_error("Using vname %s of wrong type in line %s\n",
		  gdp->vname,pa->arg_orig);
    return -1;
  }

  /* and here we place number parsing - depends on axis in the long-run*/
  if (gdp->vidx<0) {
    rrd_set_error("TODO: NOT SUPPORTED: Need to add number handler here for %s\n",
		  gdp->vname); return -1; 
    /* depending on axis type this is either number or time 
       essentially becoming generic when we get axis support
    */
  }

  /* debug output */
  dprintf("=================================\n");
  if (gf==GF_VRULE) {
    dprintf("VRULE : %s\n",pa->arg_orig);
  } else {
    dprintf("HRULE : %s\n",pa->arg_orig);
  }
  if (gdp->vidx<0) {
    if (gf==GF_VRULE) {
      dprintf("VAL   : %g\n",gdp->yrule);
    } else {
      dprintf("VAL   : %g\n",gdp->yrule);
    }
  } else {
    dprintf("VNAME : %s (%li)\n",gdp->vname,gdp->vidx);
  }
  dprintf("COLOR : r=%g g=%g b=%g a=%g\n",
	  gdp->col.red,gdp->col.green,gdp->col.blue,gdp->col.alpha);
  dprintf("COLOR2: r=%g g=%g b=%g a=%g\n",
	  gdp->col2.red,gdp->col2.green,gdp->col2.blue,gdp->col2.alpha);
  dprintf("LEGEND: %s\n",gdp->legend);
  dprintf("DASHES: TODO\n");
  dprintf("XAXIS : %i\n",gdp->xaxisidx);
  dprintf("YAXIS : %i\n",gdp->yaxisidx);
  dprintf("=================================\n");

  /* and return fine */
  return 0;
}

int parse_gprint(enum gf_en gf,parsedargs_t*pa,image_desc_t *const im) {
  /* get new graph that we fill */
  graph_desc_t *gdp=newGraphDescription(im,gf,pa,
					PARSE_VNAMEREF
					|PARSE_CF
					|PARSE_FORMAT
					|PARSE_STRFTIME
					); 
  if (!gdp) { return -1;}
   /* here we parse pos arguments locally */
  /* vname */
  if (gdp->vname[0]==0) {
    dprintfparsed("Processing postitional vname\n");
    keyvalue_t* first=getFirstUnusedArgument(1,pa);
    if (first) { 
      strncpy(gdp->vname,first->value,MAX_VNAME_LEN + 1);
      /* get type of reference */
      gdp->vidx=find_var(im, gdp->vname);
      if (gdp->vidx<0) {
	rrd_set_error("undefined vname %s",gdp->vname); return -1; }
    } else { rrd_set_error("No positional VNAME"); return -1; }
  }
  /* check type of ref in general */
  enum gf_en vnamegf=im->gdes[gdp->vidx].gf;
  dprintfparsed("Processing referenced type %i\n",vnamegf);
  switch (vnamegf) {
    /* depreciated */
  case GF_DEF:
  case GF_CDEF:
    dprintfparsed("Processing postitional CF\n");
    /* look for CF if not given */
    if (((int)gdp->cf)==-1) {
      keyvalue_t* first=getFirstUnusedArgument(1,pa);
      if (first) {
	gdp->cf=cf_conv(first->value);
	if (((int)gdp->cf)==-1) { 
	  rrd_set_error("bad CF: %s",first->value); return -1; }
      } else { rrd_set_error("No positional CDEF"); return -1; }      
    }
    break;
  case GF_VDEF:
    break;
  default:
    rrd_set_error("Encountered unknown type variable '%s'",
		  im->gdes[gdp->vidx].vname);
    return -1;
  }
  /* and get positional format */
  if (gdp->format[0]==0) {
    dprintfparsed("Processing postitional format\n");
    keyvalue_t* first=getFirstUnusedArgument(1,pa);
    if (first) {
      strncpy(gdp->format,first->value,FMT_LEG_LEN);
      dprintfparsed("got positional format: %s\n",gdp->format);
    } else { rrd_set_error("No positional CF/FORMAT"); return -1; }    
  }
  /* debug output */
  dprintf("=================================\n");
  if (gf==GF_GPRINT) {
    dprintf("GPRINT : %s\n",pa->arg_orig);
  } else {
    dprintf("PRINT  : %s\n",pa->arg_orig);
  }
  dprintf("VNAME : %s (%li)\n",gdp->vname,gdp->vidx);  
  if ((int)gdp->cf>-1) {
    dprintf("CF : (%u)\n",gdp->cf);  
  }
  dprintf("FORMAT: %s\n",gdp->legend);
  dprintf("=================================\n");

  /* and return */
  return 0;
}

int parse_comment(enum gf_en gf,parsedargs_t*pa,image_desc_t *const im){
  /* get new graph that we fill */
  graph_desc_t *gdp=newGraphDescription(im,gf,pa,
					PARSE_LEGEND
					); 
  if (!gdp) { return -1;}

  /* and if we have no legend, then use the first positional one */
  if (gdp->legend[0]==0) {
    keyvalue_t* first=getFirstUnusedArgument(1,pa);
    if (first) {
      strncpy(gdp->legend,first->value,FMT_LEG_LEN);      
    } else { rrd_set_error("No positional CF/FORMAT"); return -1; }    
  }
  /* debug output */
  dprintf("=================================\n");
  dprintf("COMMENT : %s\n",pa->arg_orig);
  dprintf("LEGEND  : %s\n",gdp->legend);
  /* and return */
  return 0;
}

int parse_tick(enum gf_en gf,parsedargs_t* pa,image_desc_t *const im) {
  /* get new graph that we fill */
  graph_desc_t *gdp=newGraphDescription(im,gf,pa,
					PARSE_VNAMECOLORFRACTIONLEGEND
					); 
  if (!gdp) { return -1;}
  /* debug output */
  dprintf("=================================\n");
  dprintf("TICK  : %s\n",pa->arg_orig);
  dprintf("VNAME : %s (%li)\n",gdp->vname,gdp->vidx);  
  dprintf("COLOR : r=%g g=%g b=%g a=%g\n",
	  gdp->col.red,gdp->col.green,gdp->col.blue,gdp->col.alpha);
  if (gdp->cf==CF_LAST) {
    dprintf("FRAC  : %s\n",gdp->vname);
  } else {
    dprintf("FRAC  : %g\n",gdp->yrule);
  }
  dprintf("LEGEND: %s\n",gdp->legend);
  dprintf("XAXIS : %i\n",gdp->xaxisidx);
  dprintf("YAXIS : %i\n",gdp->yaxisidx);
  dprintf("=================================\n");
  /* and return */
  return 0;
}

int parse_textalign(enum gf_en gf,parsedargs_t* pa,image_desc_t *const im) {
  /* get new graph that we fill */
  graph_desc_t *gdp=newGraphDescription(im,gf,pa,0); 
  if (!gdp) { return -1;}

  /* get align */
  char* align=getKeyValueArgument("align",1,pa);
  if (!align) align=getFirstUnusedArgument(1,pa)->value;
  if (!align) { rrd_set_error("No alignment given"); return -1; }

  /* parse align */
  if (strcmp(align, "left") == 0) {
    gdp->txtalign = TXA_LEFT;
  } else if (strcmp(align, "right") == 0) {
    gdp->txtalign = TXA_RIGHT;
  } else if (strcmp(align, "justified") == 0) {
    gdp->txtalign = TXA_JUSTIFIED;
  } else if (strcmp(align, "center") == 0) {
    gdp->txtalign = TXA_CENTER;
  } else {
    rrd_set_error("Unknown alignement type '%s'", align);
    return 1;
  }

  /* debug output */
  dprintf("=================================\n");
  dprintf("TEXTALIGN : %s\n",pa->arg_orig);
  dprintf("ALIGNMENT : %s (%u)\n",align,gdp->txtalign);  
  dprintf("=================================\n");
  /* and return */
  return 0;
}

int parse_shift(enum gf_en gf,parsedargs_t* pa,image_desc_t *const im) {
  /* get new graph that we fill */
  graph_desc_t *gdp=newGraphDescription(im,gf,pa,PARSE_VNAMEREFPOS);
  if (!gdp) { return -1;}
  /* and check that it is a CDEF */
  switch (im->gdes[gdp->vidx].gf) {
  case GF_DEF:
  case GF_CDEF:
    dprintf("- vname is of type DEF or CDEF, OK\n");
    break;
  case GF_VDEF:
    rrd_set_error("Cannot shift a VDEF: '%s' in line '%s'\n",
		  im->gdes[gdp->vidx].vname, pa->arg_orig);
    return 1;
  default:
    rrd_set_error("Encountered unknown type variable '%s' in line '%s'",
		  im->gdes[gdp->vidx].vname, pa->arg_orig);
    return 1;
  }

  /* now parse the "shift" */
  char* shift=getKeyValueArgument("shift",1,pa);
  if (!shift) {shift=getFirstUnusedArgument(1,pa)->value;}
  if (!shift) { rrd_set_error("No shift given"); return -1; }
  /* identify shift */
  gdp->shidx=find_var(im, shift);
  if (gdp->shidx>=0) {
    /* it is a def, so let us check its type*/
    switch (im->gdes[gdp->shidx].gf) {
    case GF_DEF:
    case GF_CDEF:
      rrd_set_error("Offset cannot be a (C)DEF: '%s' in line '%s'\n",
		    im->gdes[gdp->shidx].vname, pa->arg_orig);
      return 1;
    case GF_VDEF:
      dprintf("- vname is of type VDEF, OK\n");
      break;
    default:
            rrd_set_error
	      ("Encountered unknown type variable '%s' in line '%s'",
	       im->gdes[gdp->vidx].vname, pa->arg_orig);
            return 1;
    }
  } else {
    /* it is no def, so parse as number */
    long val;
    char *x; 
    int f=getLong(shift,&val,&x,10);
    if (f) { rrd_set_error("error parsing number %s",shift); return -1; }
    gdp->shval = val;
    gdp->shidx = -1;
  }

  /* debug output */
  dprintf("=================================\n");
  dprintf("SHIFT   : %s\n",pa->arg_orig);
  dprintf("VNAME   : %s (%li)\n",im->gdes[gdp->vidx].vname,gdp->vidx);
  if (gdp->shidx>=0) {
    dprintf("SHIFTBY : %s (%i)\n",im->gdes[gdp->shidx].vname,gdp->shidx);  
  } else {
    dprintf("SHIFTBY : %li\n",gdp->shval);  
  }
  dprintf("=================================\n");
  /* and return */
  return 0;
}
int parse_xport(enum gf_en gf,parsedargs_t* pa,image_desc_t *const im) {
  /* get new graph that we fill */
  graph_desc_t *gdp=newGraphDescription(im,gf,pa,PARSE_VNAMECOLORLEGEND);
  if (!gdp) { return -1;}
  /* check for cdef */
  /* and check that it is a CDEF */
  switch (im->gdes[gdp->vidx].gf) {
  case GF_DEF:
  case GF_CDEF:
    dprintf("- vname is of type DEF or CDEF, OK\n");
    break;
  case GF_VDEF:
    rrd_set_error("Cannot shift a VDEF: '%s' in line '%s'\n",
		  im->gdes[gdp->vidx].vname, pa->arg_orig);
    return 1;
  default:
    rrd_set_error("Encountered unknown type variable '%s' in line '%s'",
		  im->gdes[gdp->vidx].vname, pa->arg_orig);
    return 1;
  }

  /* debug output */
  dprintf("=================================\n");
  dprintf("LINE  : %s\n",pa->arg_orig);
  dprintf("VNAME : %s (%li)\n",gdp->vname,gdp->vidx);
  dprintf("LEGEND: %s\n",gdp->legend);
  dprintf("=================================\n");

  return 0;
}

void rrd_graph_script(
    int argc,
    char *argv[],
    image_desc_t *const im,
    int optno)
{
    int       i;

    /* and now handle the things*/
    parsedargs_t pa;
    initParsedArguments(&pa);

    /* loop arguments */
    for (i = optind + optno; i < argc; i++) {
	/* release parsed args - avoiding late cleanups*/
	freeParsedArguments(&pa);
	/* processed parsed args */
	if (parseArguments(argv[i],&pa)) {
	  return; }

	/* now let us handle the field based on the first command or cmd=...*/
	char*cmd=NULL;
	/* and try to get via cmd */
	char* t=getKeyValueArgument("cmd",1,&pa);
	if (t) {
	  cmd=t;
	} else if ((t=getKeyValueArgument("pos0",1,&pa))) {
	  cmd=t;
	} else {
	  rrd_set_error("no command set in argument %s",pa.arg_orig);
	  freeParsedArguments(&pa);
	  return;
	}

	/* convert to enum but handling LINE special*/
	enum gf_en gf=-1;
	gf=gf_conv(cmd);
	if ((int)gf == -1) {
	  if (strncmp("LINE",cmd,4)==0) {
	    gf=GF_LINE;
	    addToArguments(&pa,"linewidth",cmd+4,0);
	  } else {
	    rrd_set_error("'%s' is not a valid function name in %s", cmd,pa.arg_orig );
	    return;
	  }
	}
	/* now we can handle the commands */
	int r=0;
	switch (gf) {
	case GF_XAXIS:     r=parse_axis(gf,&pa,im); break;
	case GF_YAXIS:     r=parse_axis(gf,&pa,im); break;
	case GF_DEF:       r=parse_def(gf,&pa,im); break;
	case GF_CDEF:      r=parse_cvdef(gf,&pa,im); break;
	case GF_VDEF:      r=parse_cvdef(gf,&pa,im); break;
	case GF_LINE:      r=parse_line(gf,&pa,im); break;
	case GF_AREA:      r=parse_area(gf,&pa,im); break;
	case GF_PRINT:     r=parse_gprint(gf,&pa,im); break;
	case GF_GPRINT:    r=parse_gprint(gf,&pa,im); break;
	case GF_COMMENT:   r=parse_comment(gf,&pa,im); break;
	case GF_HRULE:     r=parse_hvrule(gf,&pa,im); break;
	case GF_VRULE:     r=parse_hvrule(gf,&pa,im); break;
	case GF_STACK:     r=parse_stack(gf,&pa,im); break;
	case GF_TICK:      r=parse_tick(gf,&pa,im); break;
	case GF_TEXTALIGN: r=parse_textalign(gf,&pa,im); break;
	case GF_SHIFT:     r=parse_shift(gf,&pa,im); break;
	case GF_XPORT:     r=parse_xport(gf,&pa,im); break;
	  /* unsupported types right now */
	case GF_GRAD:      
	  rrd_set_error("GRAD unsupported - use AREA instead");
	  break;
	}
	/* handle the return error case */
	if (r) { freeParsedArguments(&pa); return;}
	/* check for unprocessed keyvalue args */
	char *s;
	if ((s=checkUnusedValues(&pa))) {
	  rrd_set_error("Unused Arguments in %s: %s",pa.arg_orig,s);
	  freeParsedArguments(&pa);
	  free(s);
	  return;
	}
    }
    /* finally free arguments */
    freeParsedArguments(&pa);
}
