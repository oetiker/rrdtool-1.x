#include "rrd_graph.h"

#define dprintf if (gdp->debug) printf

int
rrd_parse_find_gf(char *line, unsigned int *eaten, graph_desc_t *gdp) {
    char funcname[11],c1=0,c2=0;
    int i=0;

    sscanf(&line[*eaten], "DEBUG%n", &i);
    if (i) {
	gdp->debug=1;
	(*eaten)+=i;
	i=0;
	dprintf("Scanning line '%s'\n",&line[*eaten]);
    }
    sscanf(&line[*eaten], "%10[A-Z]%n%c%c", funcname, &i, &c1, &c2);
    if (!i) {
	rrd_set_error("Could not make sense out of '%s'",line);
	return 1;
    }
    if ((int)(gdp->gf=gf_conv(funcname)) == -1) {
	rrd_set_error("'%s' is not a valid function name", funcname);
	return 1;
    }
    if (gdp->gf == GF_LINE) {
	if (c1 < '1' || c1 > '3' || c2 != ':') {
	    rrd_set_error("Malformed LINE command: %s",line);
	    return 1;
	}
	gdp->linewidth=c1-'0';
	i++;
    } else {
	if (c1 != ':') {
	    rrd_set_error("Malformed %s command: %s",funcname,line);
	    return 1;
	}
    }
    *eaten+=++i;
    return 0;
}

int
rrd_parse_legend(char *line, unsigned int *eaten, graph_desc_t *gdp) {
    int i;

    dprintf("- examining '%s'\n",&line[*eaten]);

    i=scan_for_col(&line[*eaten],FMT_LEG_LEN,gdp->legend);

    *eaten += i;
    if (line[*eaten]!='\0' && line[*eaten]!=':') {
	rrd_set_error("Legend too long");
	return 1;
    } else {
	dprintf("- found legend '%s'\n", gdp->legend);
	return 0;
    }
}

int
rrd_parse_color(char *string, graph_desc_t *gdp) {
    unsigned int r=0,g=0,b=0,a=0;
    int i1=0,i2=0,i3=0;

    if (string[0] != '#') return 1;
    sscanf(string, "#%02x%02x%02x%n%02x%n%*s%n",
				&r,&g,&b,&i1,&a,&i2,&i3);

    if (i3) return 1; /* garbage after color */
    if (!i2) a=0xFF;
    if (!i1) return 1; /* no color after '#' */
    gdp->col = r<<24|g<<16|b<<8|a;
    return 0;
}

int
rrd_parse_CF(char *line, unsigned int *eaten, graph_desc_t *gdp) {
    char 		symname[CF_NAM_SIZE];
    int			i=0;

    sscanf(&line[*eaten], CF_NAM_FMT "%n", symname,&i);
    if ((!i)||((line[*eaten+i]!='\0')&&(line[*eaten+i]!=':'))) {
	rrd_set_error("Cannot parse CF in '%s'",line);
	return 1;
    }
    (*eaten)+=i;
    dprintf("- using CF '%s'\n",symname);

    if ((int)(gdp->cf = cf_conv(symname))==-1) {
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
	    if (rrd_parse_CF(line,eaten,gdp)) return 1;
	    break;
	case GF_VDEF:
	    dprintf("- vname is of type VDEF\n");
	    break;
	default:
	    rrd_set_error("Encountered unknown type variable '%s'",tmpstr);
	    return 1;
    }

    if (rrd_parse_legend(line,eaten,gdp)) return 1;

    /* Why is there a separate structure member "format" ??? */
    strcpy(gdp->format,gdp->legend);

    return 0;
}

/* Parsing of PART, VRULE, HRULE, LINE, AREA, STACK and TICK
** is done in one function.  Stacking STACK is silently ignored
** as it is redundant.  Stacking PART, VRULE, HRULE or TICK is
** not allowed.  The check for color doesn't need to be so strict
** anymore, the user can specify the color '#00000000' and
** effectively circumvent this check, so why bother.
**
** If a number (which is valid to enter) is more than a
** certain amount of characters, it is caught as an error.
** While this is arguable, so is entering fixed numbers
** with more than MAX_VNAME_LEN significant digits.
*/
int
rrd_parse_PVHLAST(char *line, unsigned int *eaten, graph_desc_t *gdp, image_desc_t *im) {
    int i,j;
    int colorfound=0;
    char tmpstr[MAX_VNAME_LEN + 10];	/* vname#RRGGBBAA\0 */

    dprintf("- parsing '%s'\n",&line[*eaten]);
    dprintf("- from line '%s'\n",line);

    i=scan_for_col(&line[*eaten],MAX_VNAME_LEN+9,tmpstr);
    if (line[*eaten+i]!='\0' && line[*eaten+i]!=':') {
	rrd_set_error("Cannot parse line '%s'",line);
	return 1;
    }

    j=i; while (j>0 && tmpstr[j]!='#') j--;

    if (tmpstr[j]=='#') {
	if (rrd_parse_color(&tmpstr[j],gdp)) {
	    rrd_set_error("Could not parse color in '%s'",tmpstr[j]);
	    return 1;
	}
	tmpstr[j]='\0';
	dprintf("- parsed color 0x%08x\n",(unsigned int)gdp->col);
	colorfound=1;
    }

    dprintf("- examining '%s'\n",tmpstr);
    j=0;
    if (gdp->gf == GF_VRULE) {
	sscanf(tmpstr,"%li%n",&gdp->xrule,&j);
	if (j) dprintf("- found time: %li\n",gdp->xrule);
    } else {
	sscanf(tmpstr,"%lf%n",&gdp->yrule,&j);
	if (j) dprintf("- found number: %f\n",gdp->yrule);
    }
    if (!j) {
	if ((gdp->vidx=find_var(im,tmpstr))<0) {
	    rrd_set_error("Not a valid vname: %s in line %s",tmpstr,line);
	    return 1;
	}
	dprintf("- found vname: '%s' vidx %li\n",tmpstr,gdp->vidx);
    }
    /* "*eaten" is still pointing to the original location,
    ** "*eaten +i" is pointing to the character after the color
    ** or to the terminating '\0' in which case we're finished.
    */
    if (line[*eaten+i]=='\0') {
	*eaten+=i;
	return 0;
    }
    *eaten+=++i;

    /* If a color is specified and the only remaining part is
    ** ":STACK" then it is assumed to be the legend.  An empty
    ** legend can be specified as expected.  This means the
    ** following can be done:  LINE1:x#FF0000FF::STACK
    */
    if (colorfound) { /* no legend if no color */
	if (gdp->gf == GF_TICK) {
	    dprintf("- looking for optional number\n");
	    sscanf(&line[*eaten],"%lf:%n",&gdp->yrule,&j);
	    if (j) {
		dprintf("- found number %f\n",gdp->yrule);
		(*eaten)+=j;
		if (gdp->yrule > 1.0 || gdp->yrule < -1.0) {
		    rrd_set_error("Tick factor should be <= 1.0");
		    return 1;
		}
	    } else {
		dprintf("- not found, defaulting to 0.1\n");
		gdp->yrule=0.1;
		return 0;
	    }
	}
	dprintf("- looking for optional legend\n");
	dprintf("- in '%s'\n",&line[*eaten]);
	if (rrd_parse_legend(line, eaten, gdp)) return 1;
    }

    /* PART, HRULE, VRULE and TICK cannot be stacked.  We're finished */
    if (   (gdp->gf == GF_HRULE)
	|| (gdp->gf == GF_VRULE)
	|| (gdp->gf == GF_PART)
	|| (gdp->gf == GF_TICK)
	) return 0;

    if (line[*eaten]!='\0') {
	dprintf("- still more, should be STACK\n");
	(*eaten)++;
	j=scan_for_col(&line[*eaten],5,tmpstr);
	if (line[*eaten+j]!='\0') {
	    rrd_set_error("Garbage found where STACK expected");
	    return 1;
	}
	if (!strcmp("STACK",tmpstr)) {
	    dprintf("- found STACK\n");
	    gdp->stack=1;
	    (*eaten)+=5;
	} else {
	    rrd_set_error("Garbage found where STACK expected");
	    return 1;
	}
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
    char 		command[6]; /* step, start, end */
    char		tmpstr[256];
    struct time_value	start_tv,end_tv;
    time_t		start_tmp=0,end_tmp=0;
    char		*parsetime_error=NULL;

    start_tv.type   = end_tv.type=ABSOLUTE_TIME;
    start_tv.offset = end_tv.offset=0;
    memcpy(&start_tv.tm, localtime(&gdp->start) , sizeof(struct tm) );
    memcpy(&end_tv.tm,   localtime(&gdp->end) ,   sizeof(struct tm) );
    
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

    if (rrd_parse_CF(line,eaten,gdp)) return 1;

    if (line[*eaten]=='\0') return 0;

    while (1) {
	dprintf("- optional parameter follows: %s\n", &line[*eaten]);
	i=0;
	sscanf(&line[*eaten], "%5[a-z]=%n", command, &i);
	if (!i) {
	    rrd_set_error("Parse error in '%s'",line);
	    return 1;
	}
	(*eaten)+=i;
	dprintf("- processing '%s'\n",command);
	if (!strcmp("step",command)) {
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
rrd_graph_script(int argc, char *argv[], image_desc_t *im) {
    int i;

    for (i=optind+1;i<argc;i++) {
	graph_desc_t *gdp;
	unsigned int eaten=0;

	if (gdes_alloc(im)) return; /* the error string is already set */
	gdp = &im->gdes[im->gdes_c-1];
#ifdef DEBUG
	gdp->debug = 1;
#endif

	if (rrd_parse_find_gf(argv[i],&eaten,gdp)) return;

	switch (gdp->gf) {
#if 0
	/* future command */
	    case GF_SHIFT:	vname:value
#endif
	    case GF_XPORT:
		break;
	    case GF_PRINT:	/* vname:CF:format -or- vname:format */
	    case GF_GPRINT:	/* vname:CF:format -or- vname:format */
		if (rrd_parse_print(argv[i],&eaten,gdp,im)) return;
		break;
            case GF_COMMENT:	/* text */
		if (rrd_parse_legend(argv[i],&eaten,gdp)) return;
		break;
	    case GF_PART:	/* value[#color[:legend]] */
	    case GF_VRULE:	/* value#color[:legend] */
	    case GF_HRULE:	/* value#color[:legend] */
	    case GF_LINE:	/* vname-or-value[#color[:legend]][:STACK] */
	    case GF_AREA:	/* vname-or-value[#color[:legend]][:STACK] */
	    case GF_STACK:	/* vname-or-value[#color[:legend]] */
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
