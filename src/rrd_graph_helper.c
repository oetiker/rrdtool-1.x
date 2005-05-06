/****************************************************************************
 * RRDtool 1.2.1  Copyright by Tobi Oetiker, 1997-2005
 ****************************************************************************
 * rrd_graph_helper.c  commandline parser functions 
 *                     this code initially written by Alex van den Bogaerdt
 ****************************************************************************/

#include "rrd_graph.h"

#define dprintf if (gdp->debug) printf

/* Define prototypes for the parsing methods.
  Inputs:
   char *line         - pointer to base of input source
   unsigned int eaten - index to next input character (INPUT/OUTPUT)
   graph_desc_t *gdp  - pointer to a graph description
   image_desc_t *im   - pointer to an image description
*/

int rrd_parse_find_gf (char *, unsigned int *, graph_desc_t *);
int rrd_parse_legend  (char *, unsigned int *, graph_desc_t *);
int rrd_parse_color   (char *, graph_desc_t *);
int rrd_parse_CF      (char *, unsigned int *, graph_desc_t *, enum cf_en *);
int rrd_parse_print   (char *, unsigned int *, graph_desc_t *, image_desc_t *);
int rrd_parse_shift   (char *, unsigned int *, graph_desc_t *, image_desc_t *);
int rrd_parse_xport   (char *, unsigned int *, graph_desc_t *, image_desc_t *);
int rrd_parse_PVHLAST (char *, unsigned int *, graph_desc_t *, image_desc_t *);
int rrd_parse_vname   (char *, unsigned int *, graph_desc_t *, image_desc_t *);
int rrd_parse_def     (char *, unsigned int *, graph_desc_t *, image_desc_t *);
int rrd_parse_vdef    (char *, unsigned int *, graph_desc_t *, image_desc_t *);
int rrd_parse_cdef    (char *, unsigned int *, graph_desc_t *, image_desc_t *);



int
rrd_parse_find_gf(char *line, unsigned int *eaten, graph_desc_t *gdp) {
    char funcname[11],c1=0;
    int i=0;

    /* start an argument with DEBUG to be able to see how it is parsed */
    sscanf(&line[*eaten], "DEBUG%n", &i);
    if (i) {
	gdp->debug=1;
	(*eaten)+=i;
	i=0;
	dprintf("Scanning line '%s'\n",&line[*eaten]);
    }
    sscanf(&line[*eaten], "%10[A-Z]%n%c", funcname, &i, &c1);
    if (!i) {
	rrd_set_error("Could not make sense out of '%s'",line);
	return 1;
    }
    if ((int)(gdp->gf=gf_conv(funcname)) == -1) {
	rrd_set_error("'%s' is not a valid function name", funcname);
	return 1;
    } else {
	dprintf("- found function name '%s'\n",funcname);
    }

    if (gdp->gf == GF_LINE) {
	if (c1 == ':') {
	    gdp->linewidth=1;
	    dprintf("- - using default width of 1\n");
	} else {
	    double width;
	    (*eaten)+=i;
	    if (sscanf(&line[*eaten],"%lf%n:",&width,&i)) {
		if (width < 0 || isnan(width) || isinf(width) ) {
		    rrd_set_error("LINE width is %lf. It must be finite and >= 0 though",width);
		    return 1;
		}
		gdp->linewidth=width;
		dprintf("- - using width %f\n",width);
	    } else {
		rrd_set_error("LINE width: %s",line);
		return 1;
	    }
	}
    } else {
	if (c1 != ':') {
	    rrd_set_error("Malformed %s command: %s",funcname,line);
	    return 1;
	}
    }
    (*eaten)+=++i;
    return 0;
}

int
rrd_parse_legend(char *line, unsigned int *eaten, graph_desc_t *gdp) {
    int i;

    if (line[*eaten]=='\0' || line[*eaten]==':') {
	dprintf("- no (or: empty) legend found\n");
	return 0;
    }

    i=scan_for_col(&line[*eaten],FMT_LEG_LEN,gdp->legend);

    (*eaten)+=i;

    if (line[*eaten]!='\0' && line[*eaten]!=':') {
	rrd_set_error("Legend too long");
	return 1;
    } else {
	return 0;
    }
}

int
rrd_parse_color(char *string, graph_desc_t *gdp) {
    unsigned int r=0,g=0,b=0,a=0,i;

    /* matches the following formats:
    ** RGB
    ** RGBA
    ** RRGGBB
    ** RRGGBBAA
    */

    i=0;
    while (string[i] && isxdigit(string[i])) i++;
    if (string[i] != '\0') return 1; /* garbage follows hexdigits */
    switch (i) {
	case 3:
	case 4:
	    sscanf(string, "%1x%1x%1x%1x",&r,&g,&b,&a);
	    r *= 0x11;
	    g *= 0x11;
	    b *= 0x11;
	    a *= 0x11;
	    if (i==3) a=0xFF;
	    break;
	case 6:
	case 8:
	    sscanf(string, "%02x%02x%02x%02x",&r,&g,&b,&a);
	    if (i==6) a=0xFF;
	    break;
	default:
	    return 1;	/* wrong number of digits */
    }
    gdp->col = r<<24|g<<16|b<<8|a;
    return 0;
}

int
rrd_parse_CF(char *line, unsigned int *eaten, graph_desc_t *gdp, enum cf_en *cf) {
    char 		symname[CF_NAM_SIZE];
    int			i=0;

    sscanf(&line[*eaten], CF_NAM_FMT "%n", symname,&i);
    if ((!i)||((line[(*eaten)+i]!='\0')&&(line[(*eaten)+i]!=':'))) {
	rrd_set_error("Cannot parse CF in '%s'",line);
	return 1;
    }
    (*eaten)+=i;
    dprintf("- using CF '%s'\n",symname);

    if ((int)(*cf = cf_conv(symname))==-1) {
	rrd_set_error("Unknown CF '%s' in '%s'",symname,line);
	return 1;
    }

    if (line[*eaten]!='\0') (*eaten)++;
    return 0;
}

/* Parsing old-style xPRINT and new-style xPRINT */
int
rrd_parse_print(char *line, unsigned int *eaten, graph_desc_t *gdp, image_desc_t *im) {
    /* vname:CF:format in case of DEF-based vname
    ** vname:CF:format in case of CDEF-based vname
    ** vname:format in case of VDEF-based vname
    */
    char tmpstr[MAX_VNAME_LEN+1];
    int i=0;

    sscanf(&line[*eaten], DEF_NAM_FMT ":%n", tmpstr,&i);
    if (!i) {
	rrd_set_error("Could not parse line '%s'",line);
	return 1;
    }
    (*eaten)+=i;
    dprintf("- Found candidate vname '%s'\n",tmpstr);

    if ((gdp->vidx=find_var(im,tmpstr))<0) {
	rrd_set_error("Not a valid vname: %s in line %s",tmpstr,line);
	return 1;
    }
    switch (im->gdes[gdp->vidx].gf) {
	case GF_DEF:
	case GF_CDEF:
	    dprintf("- vname is of type DEF or CDEF, looking for CF\n");
	    if (rrd_parse_CF(line,eaten,gdp,&gdp->cf)) return 1;
	    break;
	case GF_VDEF:
	    dprintf("- vname is of type VDEF\n");
	    break;
	default:
	    rrd_set_error("Encountered unknown type variable '%s'",tmpstr);
	    return 1;
    }

    if (rrd_parse_legend(line,eaten,gdp)) return 1;
    /* for *PRINT the legend itself gets rendered later. We only
       get the format at this juncture */
    strcpy(gdp->format,gdp->legend);
    gdp->legend[0]='\0';	
    return 0;
}

int
rrd_parse_shift(char *line, unsigned int *eaten, graph_desc_t *gdp, image_desc_t *im) {
	char	*l = strdup(line + *eaten), *p;
	int	rc = 1;

	p = strchr(l, ':');
	if (p == NULL) {
		rrd_set_error("Invalid SHIFT syntax");
		goto out;
	}
	*p++ = '\0';
	
	if ((gdp->vidx=find_var(im,l))<0) {
		rrd_set_error("Not a valid vname: %s in line %s",l,line);
		goto out;
	}
	
        /* constant will parse; otherwise, must be VDEF reference */
	if (sscanf(p, "%ld", &gdp->shval) != 1) {
		graph_desc_t	*vdp;
		
		if ((gdp->shidx=find_var(im, p))<0) {
			rrd_set_error("invalid offset vname: %s", p);
			goto out;
		}
		
		vdp = &im->gdes[gdp->shidx];
		if (vdp->gf != GF_VDEF) {
			rrd_set_error("offset must specify value or VDEF");
			goto out;
		}
	} else {
		gdp->shidx = -1;
	}
	
	*eaten = strlen(line);
	rc = 0;

 out:
	free(l);
	return rc;
}

int
rrd_parse_xport(char *line, unsigned int *eaten, graph_desc_t *gdp, image_desc_t *im) {
	char	*l = strdup(line + *eaten), *p;
	int	rc = 1;

	p = strchr(l, ':');
	if (p != NULL)
		*p++ = '\0';
	else
		p = "";
	
	if ((gdp->vidx=find_var(im, l))==-1){
		rrd_set_error("unknown variable '%s'",l);
		goto out;
	}
	
	if (strlen(p) >= FMT_LEG_LEN)
		*(p + FMT_LEG_LEN) = '\0';
	
	strcpy(gdp->legend, p);
	*eaten = strlen(line);
	rc = 0;
	
 out:
	free(l);
	return rc;
}

/* Parsing of PART, VRULE, HRULE, LINE, AREA, STACK and TICK
** is done in one function.
**
** Stacking PART, VRULE, HRULE or TICK is not allowed.
**
** If a number (which is valid to enter) is more than a
** certain amount of characters, it is caught as an error.
** While this is arguable, so is entering fixed numbers
** with more than MAX_VNAME_LEN significant digits.
*/
int
rrd_parse_PVHLAST(char *line, unsigned int *eaten, graph_desc_t *gdp, image_desc_t *im) {
    int i,j,k;
    int colorfound=0;
    char tmpstr[MAX_VNAME_LEN + 10];	/* vname#RRGGBBAA\0 */

    dprintf("- parsing '%s'\n",&line[*eaten]);

    i=scan_for_col(&line[*eaten],MAX_VNAME_LEN+9,tmpstr);
    if (line[*eaten+i]!='\0' && line[*eaten+i]!=':') {
	rrd_set_error("Cannot parse line '%s'",line);
	return 1;
    }

    j=i; while (j>0 && tmpstr[j]!='#') j--;

    if (j) {
	tmpstr[j]='\0';
    }

    dprintf("- examining value '%s'\n",tmpstr);
    k=0;
    if (gdp->gf == GF_VRULE) {
	sscanf(tmpstr,"%li%n",&gdp->xrule,&k);
	if (k) dprintf("- found time: %li\n",gdp->xrule);
    } else {
	sscanf(tmpstr,"%lf%n",&gdp->yrule,&k);
	if (k) dprintf("- found number: %f\n",gdp->yrule);
    }
    if (!k) {
	if ((gdp->vidx=find_var(im,tmpstr))<0) {
	    rrd_set_error("Not a valid vname: %s in line %s",tmpstr,line);
	    return 1;
	}
	dprintf("- found vname: '%s' vidx %li\n",tmpstr,gdp->vidx);
    }

    if (j) {
	j++;
	dprintf("- examining color '%s'\n",&tmpstr[j]);
	if (rrd_parse_color(&tmpstr[j],gdp)) {
	    rrd_set_error("Could not parse color in '%s'",&tmpstr[j]);
	    return 1;
	}
	dprintf("- parsed color 0x%08x\n",(unsigned int)gdp->col);
	colorfound=1;
    } else {
	dprintf("- no color present in '%s'\n",tmpstr);
    }

    (*eaten) += i; /* after vname#color */
    if (line[*eaten]!='\0') {
	(*eaten)++;	/* after colon */
    }

    if (gdp->gf == GF_TICK) {
	dprintf("- parsing '%s'\n",&line[*eaten]);
	dprintf("- looking for optional TICK number\n");
	j=0;
	sscanf(&line[*eaten],"%lf%n",&gdp->yrule,&j);
	if (j) {
	    if (line[*eaten+j]!='\0' && line[*eaten+j]!=':') {
		rrd_set_error("Cannot parse TICK fraction '%s'",line);
		return 1;
	    }
	    dprintf("- found number %f\n",gdp->yrule);
	    if (gdp->yrule > 1.0 || gdp->yrule < -1.0) {
		rrd_set_error("Tick factor should be <= 1.0");
		return 1;
	    }
	    (*eaten)+=j;
	} else {
            dprintf("- not found, defaulting to 0.1\n");
            gdp->yrule=0.1;
	}
	if (line[*eaten] == '\0') {
	    dprintf("- done parsing line\n");
	    return 0;
	} else { if (line[*eaten] == ':') {
	   	    (*eaten)++;	   
	         } else {
   	             rrd_set_error("Can't make sense of that TICK line");
	           return 1;
                 }
        }
    }

    dprintf("- parsing '%s'\n",&line[*eaten]);

    /* Legend is next.  A legend without a color is an error.
    ** Stacking an item without having a legend is OK however
    ** then an empty legend should be specified.
    **   LINE:val#color:STACK	means legend is string "STACK"
    **   LINE:val#color::STACK	means no legend, and do STACK
    **   LINE:val:STACK		is an error (legend but no color)
    **   LINE:val::STACK	means no legend, and do STACK
    */
    if (colorfound) {
	char c1,c2;
	int err=0;
        char *linecp = strdup(line);
	dprintf("- looking for optional legend\n");

	/* The legend needs to be prefixed with "m ". This then gets
	** replaced by the color box. */

	dprintf("- examining '%s'\n",&line[*eaten]);

	(*eaten)--;
	linecp[*eaten]=' ';
	(*eaten)--;
	linecp[*eaten]='m';

	if (rrd_parse_legend(linecp, eaten, gdp)) err=1;
	
	free(linecp);
	if (err) return 1;

	dprintf("- found legend '%s'\n", &gdp->legend[2]);
    } else {
	dprintf("- skipping empty legend\n");
	if (line[*eaten] != '\0' && line[*eaten] != ':') {
	    rrd_set_error("Legend set but no color: %s",&line[*eaten]);
	    return 1;
	}
    }
    if (line[*eaten]=='\0') {
	dprintf("- done parsing line\n");
	return 0;
    }
    (*eaten)++;	/* after colon */

    /* PART, HRULE, VRULE and TICK cannot be stacked. */
    if (   (gdp->gf == GF_HRULE)
	|| (gdp->gf == GF_VRULE)
#ifdef WITH_PIECHART
	|| (gdp->gf == GF_PART)
#endif
	|| (gdp->gf == GF_TICK)
	) return 0;

    dprintf("- parsing '%s'\n",&line[*eaten]);
    if (line[*eaten]!='\0') {
	dprintf("- still more, should be STACK\n");
	j=scan_for_col(&line[*eaten],5,tmpstr);
	if (line[*eaten+j]!='\0' && line[*eaten+j]!=':') {
	    /* not 5 chars */
	    rrd_set_error("Garbage found where STACK expected");
	    return 1;
	}
	if (!strcmp("STACK",tmpstr)) {
	    dprintf("- found STACK\n");
	    gdp->stack=1;
	    (*eaten)+=j;
	} else {
	    rrd_set_error("Garbage found where STACK expected");
	    return 1;
	}
    }
    if (line[*eaten]=='\0') {
	dprintf("- done parsing line\n");
	return 0;
    }
    (*eaten)++;
    dprintf("- parsing '%s'\n",&line[*eaten]);

    /* have simpler code in the drawing section */
    if ( gdp->gf == GF_STACK ){
	    gdp->stack=1;
    }
    return 0;
}

int
rrd_parse_vname(char *line, unsigned int *eaten, graph_desc_t *gdp, image_desc_t *im) {
    char tmpstr[MAX_VNAME_LEN + 10];
    int i=0;

    sscanf(&line[*eaten], DEF_NAM_FMT "=%n", tmpstr,&i);
    if (!i) {
	rrd_set_error("Cannot parse vname from '%s'",line);
	return 1;
    }
    dprintf("- found candidate '%s'\n",tmpstr);

    if ((gdp->vidx=find_var(im,tmpstr))>=0) {
	rrd_set_error("Attempting to reuse '%s'",im->gdes[gdp->vidx].vname);
	return 1;
    }
    strcpy(gdp->vname,tmpstr);
    dprintf("- created vname '%s' vidx %lu\n", gdp->vname,im->gdes_c-1);
    (*eaten)+=i;
    return 0;
}

int
rrd_parse_def(char *line, unsigned int *eaten, graph_desc_t *gdp, image_desc_t *im) {
    int			i=0;
    char 		command[7]; /* step, start, end, reduce */
    char		tmpstr[256];
    struct rrd_time_value	start_tv,end_tv;
    time_t		start_tmp=0,end_tmp=0;
    char		*parsetime_error=NULL;

    start_tv.type   = end_tv.type=ABSOLUTE_TIME;
    start_tv.offset = end_tv.offset=0;
    localtime_r(&gdp->start, &start_tv.tm);
    localtime_r(&gdp->end, &end_tv.tm);
    
    dprintf("- parsing '%s'\n",&line[*eaten]);
    dprintf("- from line '%s'\n",line);

    if (rrd_parse_vname(line,eaten,gdp,im)) return 1;
    i=scan_for_col(&line[*eaten],254,gdp->rrd);
    if (line[*eaten+i]!=':') {
	rrd_set_error("Problems reading database name");
	return 1;
    }
    (*eaten)+=++i;
    dprintf("- using file '%s'\n",gdp->rrd);

    i=0;
    sscanf(&line[*eaten], DS_NAM_FMT ":%n", gdp->ds_nam,&i);
    if (!i) {
	rrd_set_error("Cannot parse DS in '%s'",line);
	return 1;
    }
    (*eaten)+=i;
    dprintf("- using DS '%s'\n",gdp->ds_nam);

    if (rrd_parse_CF(line,eaten,gdp,&gdp->cf)) return 1;
    gdp->cf_reduce = gdp->cf;
    
    if (line[*eaten]=='\0') return 0;

    while (1) {
	dprintf("- optional parameter follows: %s\n", &line[*eaten]);
	i=0;
	sscanf(&line[*eaten], "%6[a-z]=%n", command, &i);
	if (!i) {
	    rrd_set_error("Parse error in '%s'",line);
	    return 1;
	}
	(*eaten)+=i;
	dprintf("- processing '%s'\n",command);
	if (!strcmp("reduce",command)) {
	  if (rrd_parse_CF(line,eaten,gdp,&gdp->cf_reduce)) return 1;
	  if (line[*eaten] != '\0')
	      (*eaten)--;
	} else if (!strcmp("step",command)) {
	    i=0;
	    sscanf(&line[*eaten],"%lu%n",&gdp->step,&i);
	    (*eaten)+=i;
	    dprintf("- using step %lu\n",gdp->step);
	} else if (!strcmp("start",command)) {
	    i=scan_for_col(&line[*eaten],255,tmpstr);
	    (*eaten)+=i;
	    if ((parsetime_error = parsetime(tmpstr, &start_tv))) {
		rrd_set_error( "start time: %s", parsetime_error );
		return 1;
	    }
	    dprintf("- done parsing:  '%s'\n",&line[*eaten]);
	} else if (!strcmp("end",command)) {
	    i=scan_for_col(&line[*eaten],255,tmpstr);
	    (*eaten)+=i;
	    if ((parsetime_error = parsetime(tmpstr, &end_tv))) {
		rrd_set_error( "end time: %s", parsetime_error );
		return 1;
	    }
	    dprintf("- done parsing:  '%s'\n",&line[*eaten]);
	} else {
	    rrd_set_error("Parse error in '%s'",line);
	    return 1;
	}
	if (line[*eaten]=='\0') break;
	if (line[*eaten]!=':') {
	    dprintf("- Expected to see end of string but got '%s'\n",\
							 &line[*eaten]);
	    rrd_set_error("Parse error in '%s'",line);
	    return 1;
	}
	(*eaten)++;
    }
    if (proc_start_end(&start_tv,&end_tv,&start_tmp,&end_tmp) == -1){
	/* error string is set in parsetime.c */
	return 1;
    }
    if (start_tmp < 3600*24*365*10) {
	rrd_set_error("the first entry to fetch should be "
			"after 1980 (%ld)",start_tmp);
	return 1;
    }

    if (end_tmp < start_tmp) {
	rrd_set_error("start (%ld) should be less than end (%ld)",
			start_tmp, end_tmp);
	return 1;
    }

    gdp->start = start_tmp;
    gdp->end = end_tmp;

    dprintf("- start time %lu\n",gdp->start);
    dprintf("- end   time %lu\n",gdp->end);

    return 0;
}

int
rrd_parse_vdef(char *line, unsigned int *eaten, graph_desc_t *gdp, image_desc_t *im) {
    char tmpstr[MAX_VNAME_LEN+1];	/* vname\0 */
    int i=0;

    dprintf("- parsing '%s'\n",&line[*eaten]);
    if (rrd_parse_vname(line,eaten,gdp,im)) return 1;

    sscanf(&line[*eaten], DEF_NAM_FMT ",%n", tmpstr,&i);
    if (!i) {
	rrd_set_error("Cannot parse line '%s'",line);
	return 1;
    }
    if ((gdp->vidx=find_var(im,tmpstr))<0) {
	rrd_set_error("Not a valid vname: %s in line %s",tmpstr,line);
	return 1;
    }
    if (   im->gdes[gdp->vidx].gf != GF_DEF
	&& im->gdes[gdp->vidx].gf != GF_CDEF) {
	rrd_set_error("variable '%s' not DEF nor "
			"CDEF in VDEF '%s'", tmpstr,gdp->vname);
	return 1;
    }
    dprintf("- found vname: '%s' vidx %li\n",tmpstr,gdp->vidx);
    (*eaten)+=i;

    dprintf("- calling vdef_parse with param '%s'\n",&line[*eaten]);
    vdef_parse(gdp,&line[*eaten]);
    while (line[*eaten]!='\0'&&line[*eaten]!=':')
	(*eaten)++;

    return 0;
}

int
rrd_parse_cdef(char *line, unsigned int *eaten, graph_desc_t *gdp, image_desc_t *im) {
    dprintf("- parsing '%s'\n",&line[*eaten]);
    if (rrd_parse_vname(line,eaten,gdp,im)) return 1;
    if ((gdp->rpnp = rpn_parse(
	(void *)im,
	&line[*eaten],
	&find_var_wrapper)
    )==NULL) {
	rrd_set_error("invalid rpn expression in: %s",&line[*eaten]);
	return 1;
    };
    while (line[*eaten]!='\0'&&line[*eaten]!=':')
	(*eaten)++;
    return 0;
}

void
rrd_graph_script(int argc, char *argv[], image_desc_t *im, int optno) {
    int i;

    for (i=optind+optno;i<argc;i++) {
	graph_desc_t *gdp;
	unsigned int eaten=0;

	if (gdes_alloc(im)) return; /* the error string is already set */
	gdp = &im->gdes[im->gdes_c-1];
#ifdef DEBUG
	gdp->debug = 1;
#endif

	if (rrd_parse_find_gf(argv[i],&eaten,gdp)) return;

	switch (gdp->gf) {
	    case GF_SHIFT:	/* vname:value */
		if (rrd_parse_shift(argv[i],&eaten,gdp,im)) return;
		break;
	    case GF_XPORT:
		if (rrd_parse_xport(argv[i],&eaten,gdp,im)) return;
		break;
	    case GF_PRINT:	/* vname:CF:format -or- vname:format */
	    case GF_GPRINT:	/* vname:CF:format -or- vname:format */
		if (rrd_parse_print(argv[i],&eaten,gdp,im)) return;
		break;
            case GF_COMMENT:	/* text */
		if (rrd_parse_legend(argv[i],&eaten,gdp)) return;
		break;
	    case GF_STACK:	/* vname-or-value[#color[:legend]] */		
#ifdef WITH_PIECHART
	    case GF_PART:	/* value[#color[:legend]] */
#endif
	    case GF_VRULE:	/* value#color[:legend] */
	    case GF_HRULE:	/* value#color[:legend] */
	    case GF_LINE:	/* vname-or-value[#color[:legend]][:STACK] */
	    case GF_AREA:	/* vname-or-value[#color[:legend]][:STACK] */
	    case GF_TICK:	/* vname#color[:num[:legend]] */
		if (rrd_parse_PVHLAST(argv[i],&eaten,gdp,im)) return;
		break;
	/* data acquisition */
	    case GF_DEF:	/* vname=x:DS:CF:[:step=#][:start=#][:end=#] */
		if (rrd_parse_def(argv[i],&eaten,gdp,im)) return;
		break;
	    case GF_CDEF:	/* vname=rpn-expression */
		if (rrd_parse_cdef(argv[i],&eaten,gdp,im)) return;
		break;
	    case GF_VDEF:	/* vname=rpn-expression */
		if (rrd_parse_vdef(argv[i],&eaten,gdp,im)) return;
		break;
	}
	if (gdp->debug) {
	    dprintf("used %i out of %i chars\n",eaten,strlen(argv[i]));
	    dprintf("parsed line: '%s'\n",argv[i]);
	    dprintf("remaining: '%s'\n",&argv[i][eaten]);
	    if (eaten >= strlen(argv[i]))
		dprintf("Command finished successfully\n");
	}
	if (eaten < strlen(argv[i])) {
	    rrd_set_error("Garbage '%s' after command:\n%s",
			&argv[i][eaten],argv[i]);
	    return;
	}
    }
}
