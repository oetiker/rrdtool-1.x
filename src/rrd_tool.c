/*****************************************************************************
 * RRDtool 1.0.33  Copyright Tobias Oetiker, 1997 - 2001
 *****************************************************************************
 * rrd_tool.c  Startup wrapper
 *****************************************************************************/

#include "rrd_tool.h"

void PrintUsage(char *cmd);
int CountArgs(char *aLine);
int CreateArgs(char *, char *, int, char **);
int HandleInputLine(int, char **, FILE*);
#define TRUE		1
#define FALSE		0
#define MAX_LENGTH	10000


void PrintUsage(char *cmd)
{

    char help_main[] =
	   "RRDtool 1.0.33  Copyright 1997-2001 by Tobias Oetiker <tobi@oetiker.ch>\n\n"
	   "Usage: rrdtool [options] command command_options\n\n";

    char help_list[] =
	   "Valid commands: create, update, graph, dump, restore,\n"
	   "\t\tlast, info, fetch, tune, resize\n\n";

    char help_create[] =
	   "* create - create a new RRD\n\n"
	   "\trrdtool create filename [--start|-b start time]\n"
	   "\t\t[--step|-s step]\n"
	   "\t\t[DS:ds-name:DST:heartbeat:min:max] [RRA:CF:xff:steps:rows]\n\n";

    char help_dump[] =
	   "* dump - dump an RRD to XML\n\n"
	   "\trrdtool dump filename.rrd >filename.xml\n\n";

    char help_info[] =
	   "* info - returns the configuration and status of the\n\n"
	   "\trrdtool info filename.rrd\n\n";

    char help_restore[] =
	   "* restore - restore an RRD file from its XML form\n\n"
	   "\trrdtool restore [--range-check|-r] filename.xml filename.rrd\n\n";

    char help_last[] =
           "* last - show last update time for RRD\n\n"
           "\trrdtool last filename.rrd\n\n";

    char help_update[] =
	   "* update - update an RRD\n\n"
	   "\trrdtool update filename\n"
	   "\t\t--template|-t ds-name:ds-name:...\n"
	   "\t\ttime|N:value[:value...]\n\n"
	   "\t\t[ time:value[:value...] ..]\n\n";

    char help_fetch[] =
	   "* fetch - fetch data out of an RRD\n\n"
	   "\trrdtool fetch filename.rrd CF\n"
	   "\t\t[--resolution|-r resolution]\n"
	   "\t\t[--start|-s start] [--end|-e end]\n\n";

    char help_graph[] =
	   "* graph - generate a graph from one or several RRD\n\n"
	   "\trrdtool graph filename [-s|--start seconds] [-e|--end seconds]\n"
	   "\t\t[-x|--x-grid x-axis grid and label]\n"
	   "\t\t[--alt-y-grid]\n"
	   "\t\t[-y|--y-grid y-axis grid and label]\n"
	   "\t\t[-v|--vertical-label string] [-w|--width pixels]\n"
	   "\t\t[-h|--height pixels] [-o|--logarithmic]\n"
	   "\t\t[-u|--upper-limit value] [-z|--lazy]\n"
	   "\t\t[-l|--lower-limit value] [-r|--rigid]\n"
           "\t\t[-g|--no-legend]\n"
	   "\t\t[--alt-autoscale]\n"
	   "\t\t[--alt-autoscale-max]\n"
	   "\t\t[--units-exponent value]\n"	   
	   "\t\t[--step seconds]\n"	   
	   "\t\t[-f|--imginfo printfstr]\n"
	   "\t\t[-a|--imgformat GIF|PNG]\n"
	   "\t\t[-c|--color COLORTAG#rrggbb] [-t|--title string]\n"
	   "\t\t[DEF:vname=rrd:ds-name:CF]\n"
	   "\t\t[CDEF:vname=rpn-expression]\n"
	   "\t\t[PRINT:vname:CF:format]\n"
	   "\t\t[GPRINT:vname:CF:format]\n"
	   "\t\t[HRULE:value#rrggbb[:legend]]\n"
	   "\t\t[VRULE:value#rrggbb[:legend]]\n"
	   "\t\t[LINE{1|2|3}:vname[#rrggbb[:legend]]]\n"
	   "\t\t[AREA:vname[#rrggbb[:legend]]]\n"
	   "\t\t[STACK:vname[#rrggbb[:legend]]]\n\n";

    char help_tune[] =
	   " * tune -  Modify some basic properties of an RRD\n\n"
	   "\trrdtool tune filename\n"
	   "\t\t[--heartbeat|-h ds-name:heartbeat]\n"
	   "\t\t[--data-source-type|-d ds-name:DST\n"
	   "\t\t[--data-source-rename|-r old-name:new-name\n"
	   "\t\t[--minimum|-i ds-name:min] [--maximum|-a ds-name:max]\n\n";

    char help_resize[] =
	   " * resize - alter the lenght of one of the RRAs in an RRD\n\n"
	   "\trrdtool resize filename rranum GROW|SHRINK rows\n\n";

    char help_lic[] =
	   "RRDtool is distributed under the Terms of the GNU General\n"
	   "Public License Version 2. (www.gnu.org/copyleft/gpl.html)\n\n"

	   "For more information read the RRD manpages\n\n";

    enum { C_NONE, C_CREATE, C_DUMP, C_INFO, C_RESTORE, C_LAST,
	   C_UPDATE, C_FETCH, C_GRAPH, C_TUNE, C_RESIZE };

    int help_cmd = C_NONE;

    if (cmd)
	{
	    if (!strcmp(cmd,"create"))
		help_cmd = C_CREATE;
    	    else if (!strcmp(cmd,"dump"))
		help_cmd = C_DUMP;
    	    else if (!strcmp(cmd,"info"))
		help_cmd = C_INFO;
    	    else if (!strcmp(cmd,"restore"))
		help_cmd = C_RESTORE;
    	    else if (!strcmp(cmd,"last"))
		help_cmd = C_LAST;
    	    else if (!strcmp(cmd,"update"))
		help_cmd = C_UPDATE;
    	    else if (!strcmp(cmd,"fetch"))
		help_cmd = C_FETCH;
    	    else if (!strcmp(cmd,"graph"))
		help_cmd = C_GRAPH;
    	    else if (!strcmp(cmd,"tune"))
		help_cmd = C_TUNE;
    	    else if (!strcmp(cmd,"resize"))
		help_cmd = C_RESIZE;
	}
    fputs(help_main, stdout);
    switch (help_cmd)
	{
	    case C_NONE:
		fputs(help_list, stdout);
		break;
	    case C_CREATE:
		fputs(help_create, stdout);
		break;
	    case C_DUMP:
		fputs(help_dump, stdout);
		break;
	    case C_INFO:
		fputs(help_info, stdout);
		break;
	    case C_RESTORE:
		fputs(help_restore, stdout);
		break;
	    case C_LAST:
		fputs(help_last, stdout);
		break;
	    case C_UPDATE:
		fputs(help_update, stdout);
		break;
	    case C_FETCH:
		fputs(help_fetch, stdout);
		break;
	    case C_GRAPH:
		fputs(help_graph, stdout);
		break;
	    case C_TUNE:
		fputs(help_tune, stdout);
		break;
	    case C_RESIZE:
		fputs(help_resize, stdout);
		break;
	}
    fputs(help_lic, stdout);
}


int main(int argc, char *argv[])
{
    char **myargv;
    char aLine[MAX_LENGTH];
#ifdef MUST_DISABLE_SIGFPE
    signal(SIGFPE,SIG_IGN);
#endif
#ifdef MUST_DISABLE_FPMASK
    fpsetmask(0);
#endif
    if (argc == 1)
	{
	    PrintUsage("");
	    return 0;
	}
    
    if ((argc == 2) && !strcmp("-",argv[1]))
	{
#if HAVE_GETRUSAGE
	  struct rusage  myusage;
	  struct timeval starttime;
	  struct timeval currenttime;
	  struct timezone tz;

	    tz.tz_minuteswest =0;
	    tz.tz_dsttime=0;
	    gettimeofday(&starttime,&tz);
#endif

	    while (fgets(aLine, sizeof(aLine)-1, stdin)){
		if ((argc = CountArgs(aLine)) == 0)  {
		    fprintf(stderr,"ERROR: not enough arguments\n");		    
		}
		if ((myargv = (char **) malloc((argc+1) * 
					       sizeof(char *))) == NULL)   {
		    perror("malloc");
		    return -1;
		}
		if ((argc=CreateArgs(argv[0], aLine, argc, myargv)) < 0) {
		    fprintf(stderr, "ERROR: creating arguments\n");
		    return -1;
		}

		if (HandleInputLine(argc, myargv, stdout))
		    return -1;
		free(myargv);

#if HAVE_GETRUSAGE
		getrusage(RUSAGE_SELF,&myusage);
		gettimeofday(&currenttime,&tz);
		printf("OK u:%1.2f s:%1.2f r:%1.2f\n",
		       (double)myusage.ru_utime.tv_sec+
		       (double)myusage.ru_utime.tv_usec/1000000.0,
		       (double)myusage.ru_stime.tv_sec+
		       (double)myusage.ru_stime.tv_usec/1000000.0,
		       (double)(currenttime.tv_sec-starttime.tv_sec)
		       +(double)(currenttime.tv_usec-starttime.tv_usec)
		       /1000000.0);
#else
		printf("OK\n");
#endif
		fflush(stdout); /* this is important for pipes to work */
	    }
	}
    else if (argc == 2)
	{
		PrintUsage(argv[1]);
		exit(0);
	}
    else
	HandleInputLine(argc, argv, stderr);    
    return 0;
}

int HandleInputLine(int argc, char **argv, FILE* out)
{
    optind=0; /* reset gnu getopt */
    opterr=0; /* no error messages */

    if (argc < 3 
	|| strcmp("help", argv[1]) == 0
	|| strcmp("--help", argv[1]) == 0
	|| strcmp("-help", argv[1]) == 0
	|| strcmp("-?", argv[1]) == 0
	|| strcmp("-h", argv[1]) == 0 ) {
	PrintUsage("");
	return 0;
    }
    
    if (strcmp("create", argv[1]) == 0)	
	rrd_create(argc-1, &argv[1]);
    else if (strcmp("dump", argv[1]) == 0)
	rrd_dump(argc-1, &argv[1]);
    else if (strcmp("info", argv[1]) == 0){
	info_t *data,*save;
	data=rrd_info(argc-1, &argv[1]);
	while (data) {
	    save=data;
	    printf ("%s = ", data->key);
	    free(data->key);
	    
	    switch (data->type) {
	    case RD_I_VAL:
		if (isnan (data->value.u_val))
		    printf("NaN");
		else
		    printf ("%0.10e", data->value.u_val);
		break;
	    case RD_I_CNT:
		printf ("%lu", data->value.u_cnt);
		break;
	    case RD_I_STR:
		printf ("\"%s\"", data->value.u_str);
		free(data->value.u_str);
		break;
	    }
	    data = data->next;
	    free(save);
	    printf ("\n");
	}
	free(data);
    }
	
    else if (strcmp("--version", argv[1]) == 0 ||
	     strcmp("version", argv[1]) == 0 || 
	     strcmp("v", argv[1]) == 0 ||
	     strcmp("-v", argv[1]) == 0  ||
	     strcmp("-version", argv[1]) == 0  )
        printf("RRDtool 1.0.33  Copyright (C) 1997-2001 by Tobias Oetiker <tobi@oetiker.ch>\n");
    else if (strcmp("restore", argv[1]) == 0)
	rrd_restore(argc-1, &argv[1]);
    else if (strcmp("resize", argv[1]) == 0)
	rrd_resize(argc-1, &argv[1]);
    else if (strcmp("last", argv[1]) == 0)
        printf("%ld\n",rrd_last(argc-1, &argv[1]));
    else if (strcmp("update", argv[1]) == 0)
	rrd_update(argc-1, &argv[1]);
    else if (strcmp("fetch", argv[1]) == 0) {
	time_t        start,end;
	unsigned long step, ds_cnt,i,ii;
	rrd_value_t   *data,*datai;
	char          **ds_namv;
	if (rrd_fetch(argc-1, &argv[1],&start,&end,&step,&ds_cnt,&ds_namv,&data) != -1) {
	    datai=data;
	    printf("           ");
	    for (i = 0; i<ds_cnt;i++)
	        printf("%14s",ds_namv[i]);
	    printf ("\n\n");
	    for (i = start; i <= end; i += step){
	        printf("%10lu:", i);
	        for (ii = 0; ii < ds_cnt; ii++)
		    printf(" %0.10e", *(datai++));
	        printf("\n");
	    }
	    for (i=0;i<ds_cnt;i++)
	          free(ds_namv[i]);
	    free(ds_namv);
	    free (data);
	}
    }
    else if (strcmp("graph", argv[1]) == 0) {
	char **calcpr;
	int xsize, ysize;
	int i;
	if( rrd_graph(argc-1, &argv[1], &calcpr, &xsize, &ysize) != -1 ) {
	    if (strcmp(argv[2],"-") != 0) 
		printf ("%dx%d\n",xsize,ysize);
	    if (calcpr) {
		for(i=0;calcpr[i];i++){
		    if (strcmp(argv[2],"-") != 0) 
			printf("%s\n",calcpr[i]);
		    free(calcpr[i]);
		} 
		free(calcpr);
	    }
	}
	
    } else if (strcmp("tune", argv[1]) == 0) 
		rrd_tune(argc-1, &argv[1]);
    else {
		rrd_set_error("unknown function '%s'",argv[1]);
    }
    if (rrd_test_error()) {
	fprintf(out, "ERROR: %s\n",rrd_get_error());
	rrd_clear_error();
    }
    return(0);
}

int CountArgs(char *aLine)
{
    int i=0;
    int aCount = 0;
    int inarg = 0;
    while (aLine[i] == ' ') i++;
    while (aLine[i] != 0){       
	if((aLine[i]== ' ') && inarg){
	    inarg = 0;
	}
	if((aLine[i]!= ' ') && ! inarg){
	    inarg = 1;
	    aCount++;
	}
	i++;
    }
    return aCount;
}

/*
 * CreateArgs - take a string (aLine) and tokenize
 */
int CreateArgs(char *pName, char *aLine, int argc, char **argv)
{
    char	*getP, *putP;
    char	**pargv = argv;
    char        Quote = 0;
    int inArg = 0;
    int	len;

    len = strlen(aLine);
    /* remove trailing space and newlines */
    while (len && aLine[len] <= ' ') {
	aLine[len] = 0 ; len--;
    }
    /* sikp leading blanks */
    while (*aLine && *aLine <= ' ') aLine++;

    pargv[0] = pName;
    argc = 1;
    getP = aLine;
    putP = aLine;
    while (*getP){
	switch (*getP) {
	case ' ': 
	    if (Quote){
		*(putP++)=*getP;
	    } else 
		if(inArg) {
		    *(putP++) = 0;
		    inArg = 0;
		}
	    break;
	case '"':
	case '\'':
	    if (Quote != 0) {
		if (Quote == *getP) 
		    Quote = 0;
		else {
		    *(putP++)=*getP;
		}
	    } else {
		if(!inArg){
		    pargv[argc++] = putP;
		    inArg=1;
		}	    
		Quote = *getP;
	    }
	    break;
	default:
	    if(!inArg){
		pargv[argc++] = putP;
		inArg=1;
	    }
	    *(putP++)=*getP;
	    break;
	}
	getP++;
    }

    *putP = '\0';

    if (Quote) 
	return -1;
    else
	return argc;
}


