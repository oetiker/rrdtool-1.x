/*****************************************************************************
 * RRDtool 1.2rc3  Copyright by Tobi Oetiker, 1997-2005
 *****************************************************************************
 * rrd_cgi.c  RRD Web Page Generator
 *****************************************************************************/

#include "rrd_tool.h"
#include <cgi.h>
#include <time.h>


#define MEMBLK 1024
/*#define DEBUG_PARSER
#define DEBUG_VARS*/

/* global variable for libcgi */
s_cgi *cgiArg;

/* in arg[0] find tags beginning with arg[1] call arg[2] on them
   and replace by result of arg[2] call */
int parse(char **, long, char *, char *(*)(long , const char **));

/**************************************************/
/* tag replacers ... they are called from parse   */
/* through function pointers                      */
/**************************************************/

/* return cgi var named arg[0] */ 
char* cgiget(long , const char **);

/* return a quoted cgi var named arg[0] */ 
char* cgigetq(long , const char **);

/* return a quoted and sanitized cgi variable */
char* cgigetqp(long , const char **);

/* call rrd_graph and insert appropriate image tag */
char* drawgraph(long, char **);

/* return PRINT functions from last rrd_graph call */
char* drawprint(long, const char **);

/* pretty-print the <last></last> value for some.rrd via strftime() */
char* printtimelast(long, const char **);

/* pretty-print current time */
char* printtimenow(long, const char **);

/* set an environment variable */
char* rrdsetenv(long, const char **);

/* get an environment variable */
char* rrdgetenv(long, const char **);

/* include the named file at this point */
char* includefile(long, const char **);

/* for how long is the output of the cgi valid ? */
char* rrdgoodfor(long, const char **);

char* rrdstrip(char *buf);
char* scanargs(char *line, int *argc, char ***args);

/* format at-time specified times using strftime */
char* printstrftime(long, const char**);

/** HTTP protocol needs special format, and GMT time **/
char *http_time(time_t *);

/* return a pointer to newly allocated copy of this string */
char *stralloc(const char *);


/* rrd interface to the variable functions {put,get}var() */
char* rrdgetvar(long argc, const char **args);
char* rrdsetvar(long argc, const char **args);
char* rrdsetvarconst(long argc, const char **args);


/* variable store: put/get key-value pairs */
static int   initvar();
static void  donevar();
static const char* getvar(const char* varname);
static const char* putvar(const char* name, const char* value, int is_const);

/* key value pair that makes up an entry in the variable store */
typedef struct
{
	int is_const;		/* const variable or not */
	const char* name;	/* variable name */
	const char* value;	/* variable value */
} vardata;

/* the variable heap: 
   start with a heapsize of 10 variables */
#define INIT_VARSTORE_SIZE	10
static vardata* varheap    = NULL;
static size_t varheap_size = 0;

/* allocate and initialize variable heap */
static int
initvar()
{
	varheap = (vardata*)malloc(sizeof(vardata) * INIT_VARSTORE_SIZE);
	if (varheap == NULL) {
		fprintf(stderr, "ERROR: unable to initialize variable store\n");
		return -1;
	}
	memset(varheap, 0, sizeof(vardata) * INIT_VARSTORE_SIZE);
	varheap_size = INIT_VARSTORE_SIZE;
	return 0;
}

/* cleanup: free allocated memory */
static void
donevar()
{
	int i;
	if (varheap) {
		for (i=0; i<(int)varheap_size; i++) {
			if (varheap[i].name) {
				free((char*)varheap[i].name);
			}
			if (varheap[i].value) {
				free((char*)varheap[i].value);
			}
		}
		free(varheap);
	}
}

/* Get a variable from the variable store.
   Return NULL in case the requested variable was not found. */
static const char*
getvar(const char* name)
{
	int i;
	for (i=0; i<(int)varheap_size && varheap[i].name; i++) {
		if (0 == strcmp(name, varheap[i].name)) {
#ifdef		DEBUG_VARS
			printf("<!-- getvar(%s) -> %s -->\n", name, varheap[i].value);
#endif
			return varheap[i].value;
		}
	}
#ifdef DEBUG_VARS
	printf("<!-- getvar(%s) -> Not found-->\n", name);
#endif
	return NULL;
}

/* Put a variable into the variable store. If a variable by that
   name exists, it's value is overwritten with the new value unless it was
   marked as 'const' (initialized by RRD::SETCONSTVAR).
   Returns a copy the newly allocated value on success, NULL on error. */
static const char*
putvar(const char* name, const char* value, int is_const)
{
	int i;
	for (i=0; i < (int)varheap_size && varheap[i].name; i++) {
		if (0 == strcmp(name, varheap[i].name)) {
			/* overwrite existing entry */
			if (varheap[i].is_const) {
#ifdef			DEBUG_VARS
				printf("<!-- setver(%s, %s): not assigning: "
						"const variable -->\n", name, value);
#				endif
				return varheap[i].value;
			}
#ifdef		DEBUG_VARS
			printf("<!-- setvar(%s, %s): overwriting old value (%s) -->\n",
					name, value, varheap[i].value);
#endif
			/* make it possible to promote a variable to readonly */
			varheap[i].is_const = is_const;
			free((char*)varheap[i].value);
			varheap[i].value = stralloc(value);
			return varheap[i].value;
		}
	}

	/* no existing variable found by that name, add it */
	if (i == (int)varheap_size) {
		/* ran out of heap: resize heap to double size */
		size_t new_size = varheap_size * 2;
		varheap = (vardata*)(realloc(varheap, sizeof(vardata) * new_size));
		if (!varheap) {
			fprintf(stderr, "ERROR: Unable to realloc variable heap\n");
			return NULL;
		}
		/* initialize newly allocated memory */;
		memset(&varheap[varheap_size], 0, sizeof(vardata) * varheap_size);
		varheap_size = new_size;
	}
	varheap[i].is_const = is_const;
	varheap[i].name  = stralloc(name);
	varheap[i].value = stralloc(value);

#ifdef		DEBUG_VARS
	printf("<!-- setvar(%s, %s): adding new variable -->\n", name, value);
#endif
	return varheap[i].value;
}

/* expand those RRD:* directives that can be used recursivly */
static char*
rrd_expand_vars(char* buffer)
{
	int i;

#ifdef DEBUG_PARSER
	printf("expanding variables in '%s'\n", buffer);
#endif

	for (i=0; buffer[i]; i++) {
		if (buffer[i] != '<')
			continue;
		parse(&buffer, i, "<RRD::CV", cgiget);
		parse(&buffer, i, "<RRD::CV::QUOTE", cgigetq);
		parse(&buffer, i, "<RRD::CV::PATH", cgigetqp);
		parse(&buffer, i, "<RRD::GETENV", rrdgetenv);	 
		parse(&buffer, i, "<RRD::GETVAR", rrdgetvar);	 
                parse(&buffer, i, "<RRD::TIME::LAST", printtimelast);
                parse(&buffer, i, "<RRD::TIME::NOW", printtimenow);
                parse(&buffer, i, "<RRD::TIME::STRFTIME", printstrftime);
	}
	return buffer;
}

static long goodfor=0;
static char **calcpr=NULL;
static void calfree (void){
  if (calcpr) {
    long i;
    for(i=0;calcpr[i];i++){
      if (calcpr[i]){
	      free(calcpr[i]);
      }
    } 
    if (calcpr) {
	    free(calcpr);
    }
  }
}

/* create freeable version of the string */
char * stralloc(const char *str){
  char* nstr;
  if (!str) {
	  return NULL;
  }
  nstr = malloc((strlen(str)+1));
  strcpy(nstr,str);
  return(nstr);
}

int main(int argc, char *argv[]) {
	long length;
	char *buffer;
	char *server_url = NULL;
	long i;
	long filter=0;
#ifdef MUST_DISABLE_SIGFPE
	signal(SIGFPE,SIG_IGN);
#endif
#ifdef MUST_DISABLE_FPMASK
	fpsetmask(0);
#endif
	/* what do we get for cmdline arguments?
	for (i=0;i<argc;i++)
	printf("%d-'%s'\n",i,argv[i]); */
	while (1) {
		static struct option long_options[] = {
			{ "filter", no_argument, 0, 'f' },
			{ 0, 0, 0, 0}
		};
		int option_index = 0;
		int opt;
		opt = getopt_long(argc, argv, "f", long_options, &option_index);
		if (opt == EOF) {
			break;
		}

		switch(opt) {
		case 'f':
				filter=1;
			break;
		case '?':
			printf("unknown commandline option '%s'\n",argv[optind-1]);
			return -1;
		}
	}

	if (!filter) {
		cgiDebug(0,0);
		cgiArg = cgiInit();
		server_url = getenv("SERVER_URL");
	}

	/* make sure we have one extra argument, 
	   if there are others, we do not care Apache gives several */

	/* if ( (optind != argc-2 
	   && strstr( getenv("SERVER_SOFTWARE"),"Apache/2") != NULL) 
	   && optind != argc-1) { */

	if ( optind >= argc ) { 
		fprintf(stderr, "ERROR: expected a filename\n");
		exit(1);
	} else {
		length = readfile(argv[optind], &buffer, 1);
	}

	if(rrd_test_error()) {
		fprintf(stderr, "ERROR: %s\n",rrd_get_error());
		exit(1);
	}

	/* initialize variable heap */
	initvar();

#ifdef DEBUG_PARSER
       /* some fake header for testing */
       printf ("Content-Type: text/html\nContent-Length: 10000000\n\n\n");
#endif


	/* expand rrd directives in buffer recursivly */
	for (i=0; buffer[i]; i++) {
		if (buffer[i] != '<')
			continue;
		if (!filter) {
			parse(&buffer, i, "<RRD::CV", cgiget);
			parse(&buffer, i, "<RRD::CV::PATH", cgigetqp);
			parse(&buffer, i, "<RRD::CV::QUOTE", cgigetq);
			parse(&buffer, i, "<RRD::GETENV", rrdgetenv);
		}
		parse(&buffer, i, "<RRD::GETVAR", rrdgetvar);
		parse(&buffer, i, "<RRD::GOODFOR", rrdgoodfor);
		parse(&buffer, i, "<RRD::GRAPH", drawgraph);
		parse(&buffer, i, "<RRD::INCLUDE", includefile);
		parse(&buffer, i, "<RRD::PRINT", drawprint);
		parse(&buffer, i, "<RRD::SETCONSTVAR", rrdsetvarconst);
		parse(&buffer, i, "<RRD::SETENV", rrdsetenv);
		parse(&buffer, i, "<RRD::SETVAR", rrdsetvar);
		parse(&buffer, i, "<RRD::TIME::LAST", printtimelast);
		parse(&buffer, i, "<RRD::TIME::NOW", printtimenow);
		parse(&buffer, i, "<RRD::TIME::STRFTIME", printstrftime);
	}

	if (!filter) {
		printf ("Content-Type: text/html\n" 
				"Content-Length: %d\n", 
				strlen(buffer));

		if (labs(goodfor) > 0) {
			time_t now;
			now = time(NULL);
			printf("Last-Modified: %s\n", http_time(&now));
			now += labs(goodfor);
			printf("Expires: %s\n", http_time(&now));
			if (goodfor < 0) {
				printf("Refresh: %ld\n", labs(goodfor));
			}
		}
		printf("\n");
	}

	/* output result */
	printf("%s", buffer);

	/* cleanup */
	calfree();
	if (buffer){
		free(buffer);
	}
	donevar();
	exit(0);
}

/* remove occurrences of .. this is a general measure to make
   paths which came in via cgi do not go UP ... */

char* rrdsetenv(long argc, const char **args) {
	if (argc >= 2) {
		char *xyz = malloc((strlen(args[0]) + strlen(args[1]) + 2));
		if (xyz == NULL) {
			return stralloc("[ERROR: allocating setenv buffer]");
		};
		sprintf(xyz, "%s=%s", args[0], args[1]);
		if(putenv(xyz) == -1) {
			free(xyz);
			return stralloc("[ERROR: failed to do putenv]");
		};
 	        return stralloc("");
	}
	return stralloc("[ERROR: setenv failed because not enough "
		  			"arguments were defined]");
}

/* rrd interface to the variable function putvar() */
char*
rrdsetvar(long argc, const char **args)
{
	if (argc >= 2)
	{
		const char* result = putvar(args[0], args[1], 0 /* not const */);
		if (result) {
			/* setvar does not return the value set */
			return stralloc("");
		}
		return stralloc("[ERROR: putvar failed]");
	}
	return stralloc("[ERROR: putvar failed because not enough arguments "
					"were defined]");
}

/* rrd interface to the variable function putvar() */
char*
rrdsetvarconst(long argc, const char **args)
{
	if (argc >= 2)
	{
		const char* result = putvar(args[0], args[1], 1 /* const */);
		if (result) {
			/* setvar does not return the value set */
			return stralloc("");
		}
		return stralloc("[ERROR: putvar failed]");
	}
	return stralloc("[ERROR: putvar failed because not enough arguments "
					"were defined]");
}

char* rrdgetenv(long argc, const char **args) {
	char buf[128];
	const char* envvar;
	if (argc != 1) {
		return stralloc("[ERROR: getenv failed because it did not "
						"get 1 argument only]");
	};
	envvar = getenv(args[0]);
	if (envvar) {
		return stralloc(envvar);
	} else {
#if defined(WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)
               _snprintf(buf, sizeof(buf), "[ERROR:_getenv_'%s'_failed", args[0]);
#else
                snprintf(buf, sizeof(buf), "[ERROR:_getenv_'%s'_failed", args[0]);
#endif         
                return stralloc(buf);
	}
}

char* rrdgetvar(long argc, const char **args) {
	char buf[128];
	const char* value;
	if (argc != 1) {
		return stralloc("[ERROR: getvar failed because it did not "
						"get 1 argument only]");
	};
	value = getvar(args[0]);
	if (value) {
		return stralloc(value);
	} else {
#if defined(WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)
               _snprintf(buf, sizeof(buf), "[ERROR:_getvar_'%s'_failed", args[0]);
#else
                snprintf(buf, sizeof(buf), "[ERROR:_getvar_'%s'_failed", args[0]);
#endif
		return stralloc(buf);
	}
}

char* rrdgoodfor(long argc, const char **args){
  if (argc == 1) {
      goodfor = atol(args[0]);
  } else {
    return stralloc("[ERROR: goodfor expected 1 argument]");
  }
   
  if (goodfor == 0){
     return stralloc("[ERROR: goodfor value must not be 0]");
  }
   
  return stralloc("");
}

/* Format start or end times using strftime.  We always need both the
 * start and end times, because, either might be relative to the other.
 * */
#define MAX_STRFTIME_SIZE 256
char* printstrftime(long argc, const char **args){
	struct	rrd_time_value start_tv, end_tv;
	char    *parsetime_error = NULL;
	char	formatted[MAX_STRFTIME_SIZE];
	struct tm *the_tm;
	time_t	start_tmp, end_tmp;

	/* Make sure that we were given the right number of args */
	if( argc != 4) {
		rrd_set_error( "wrong number of args %d", argc);
		return (char *) -1;
	}

	/* Init start and end time */
	parsetime("end-24h", &start_tv);
	parsetime("now", &end_tv);

	/* Parse the start and end times we were given */
	if( (parsetime_error = parsetime( args[1], &start_tv))) {
		rrd_set_error( "start time: %s", parsetime_error);
		return (char *) -1;
	}
	if( (parsetime_error = parsetime( args[2], &end_tv))) {
		rrd_set_error( "end time: %s", parsetime_error);
		return (char *) -1;
	}
	if( proc_start_end( &start_tv, &end_tv, &start_tmp, &end_tmp) == -1) {
		return (char *) -1;
	}

	/* Do we do the start or end */
	if( strcasecmp( args[0], "START") == 0) {
		the_tm = localtime( &start_tmp);
	}
	else if( strcasecmp( args[0], "END") == 0) {
		the_tm = localtime( &end_tmp);
	}
	else {
		rrd_set_error( "start/end not found in '%s'", args[0]);
		return (char *) -1;
	}

	/* now format it */
	if( strftime( formatted, MAX_STRFTIME_SIZE, args[3], the_tm)) {
		return( stralloc( formatted));
	}
	else {
		rrd_set_error( "strftime failed");
		return (char *) -1;
	}
}

char* includefile(long argc, const char **args){
  char *buffer;
  if (argc >= 1) {
      char* filename = args[0];
      readfile(filename, &buffer, 0);
      if (rrd_test_error()) {
	  	char *err = malloc((strlen(rrd_get_error())+DS_NAM_SIZE));
	  sprintf(err, "[ERROR: %s]",rrd_get_error());
	  rrd_clear_error();
	  return err;
      } else {
       return buffer;
      }
  }
  else
  {
      return stralloc("[ERROR: No Inclue file defined]");
  }
}

/* make a copy of buf and replace open/close brackets with '_' */
char* rrdstrip(char *buf) {
  char* p;
  if (buf == NULL) {
	  return NULL;
  }
  /* make a copy of the buffer */
  buf = stralloc(buf);
  if (buf == NULL) {
	  return NULL;
  }

  p = buf;
  while (*p) {
	  if (*p == '<' || *p == '>') {
		  *p = '_';
	  }
	  p++;
  }
  return buf;
}

char* cgigetq(long argc, const char **args){
  if (argc>= 1){
    char *buf = rrdstrip(cgiGetValue(cgiArg,args[0]));
    char *buf2;
    char *c,*d;
    int  qc=0;
    if (buf==NULL) return NULL;

    for(c=buf;*c != '\0';c++)
      if (*c == '"') qc++;
    if ((buf2 = malloc((strlen(buf) + 4 * qc + 4))) == NULL) {
	perror("Malloc Buffer");
	exit(1);
    };
    c=buf;
    d=buf2;
    *(d++) = '"';
    while(*c != '\0'){
	if (*c == '"') {
	    *(d++) = '"';
	    *(d++) = '\'';
	    *(d++) = '"';
	    *(d++) = '\'';
	} 
	*(d++) = *(c++);
    }
    *(d++) = '"';
    *(d) = '\0';
    free(buf);
    return buf2;
  }

  return stralloc("[ERROR: not enough argument for RRD::CV::QUOTE]");
}

/* remove occurrences of .. this is a general measure to make
   paths which came in via cgi do not go UP ... */

char* cgigetqp(long argc, const char **args){
       char* buf;
    char* buf2;
    char* p;
        char* d;

        if (argc < 1)
        {
                return stralloc("[ERROR: not enough arguments for RRD::CV::PATH]");
        }

        buf = rrdstrip(cgiGetValue(cgiArg, args[0]));
    if (!buf)
        {
                return NULL;
        }

        buf2 = malloc(strlen(buf)+1);
    if (!buf2)
        {
                perror("cgigetqp(): Malloc Path Buffer");
                exit(1);
    };

    p = buf;
    d = buf2;

    while (*p)
        {
                /* prevent mallicious paths from entering the system */
                if (p[0] == '.' && p[1] == '.')
                {
                        p += 2;
                        *d++ = '_';
                        *d++ = '_';     
                }
                else
                {
                        *d++ = *p++;
                }
    }

    *d = 0;
    free(buf);

    /* Make sure the path is relative, e.g. does not start with '/' */
    p = buf2;
    while ('/' == *p)
        {
            *p++ = '_';
    }

    return buf2;
}


char* cgiget(long argc, const char **args){
  if (argc>= 1)
    return rrdstrip(cgiGetValue(cgiArg,args[0]));
  else
    return stralloc("[ERROR: not enough arguments for RRD::CV]");
}



char* drawgraph(long argc, char **args){
  int i,xsize, ysize;
  double ymin,ymax;
  for(i=0;i<argc;i++)
    if(strcmp(args[i],"--imginfo")==0 || strcmp(args[i],"-g")==0) break;
  if(i==argc) {
    args[argc++] = "--imginfo";
    args[argc++] = "<IMG SRC=\"./%s\" WIDTH=\"%lu\" HEIGHT=\"%lu\">";
  }
  optind=0; /* reset gnu getopt */
  opterr=0; /* reset gnu getopt */
  calfree();
  if( rrd_graph(argc+1, args-1, &calcpr, &xsize, &ysize,NULL,&ymin,&ymax) != -1 ) {
    return stralloc(calcpr[0]);
  } else {
    if (rrd_test_error()) {
      char *err = malloc((strlen(rrd_get_error())+DS_NAM_SIZE)*sizeof(char));
      sprintf(err, "[ERROR: %s]",rrd_get_error());
      rrd_clear_error();
      calfree();
      return err;
    }
  }
  return NULL;
}

char* drawprint(long argc, const char **args){
  if (argc==1 && calcpr){
    long i=0;
    while (calcpr[i] != NULL) i++; /*determine number lines in calcpr*/
    if (atol(args[0])<i-1)
      return stralloc(calcpr[atol(args[0])+1]);    
  }
  return stralloc("[ERROR: RRD::PRINT argument error]");
}

char* printtimelast(long argc, const char **args) {
  time_t last;
  struct tm tm_last;
  char *buf;
  if ( argc == 2 ) {
    buf = malloc(255);
    if (buf == NULL){	
	return stralloc("[ERROR: allocating strftime buffer]");
    };
    last = rrd_last(argc+1, args-1); 
    if (rrd_test_error()) {
      char *err = malloc((strlen(rrd_get_error())+DS_NAM_SIZE)*sizeof(char));
      sprintf(err, "[ERROR: %s]",rrd_get_error());
      rrd_clear_error();
      return err;
    }
    tm_last = *localtime(&last);
    strftime(buf,254,args[1],&tm_last);
    return buf;
  }
  if ( argc < 2 ) {
    return stralloc("[ERROR: too few arguments for RRD::TIME::LAST]");
  }
  return stralloc("[ERROR: not enough arguments for RRD::TIME::LAST]");
}

char* printtimenow(long argc, const char **args) {
  time_t now = time(NULL);
  struct tm tm_now;
  char *buf;
  if ( argc == 1 ) {
    buf = malloc(255);
    if (buf == NULL){	
	return stralloc("[ERROR: allocating strftime buffer]");
    };
    tm_now = *localtime(&now);
    strftime(buf,254,args[0],&tm_now);
    return buf;
  }
  if ( argc < 1 ) {
    return stralloc("[ERROR: too few arguments for RRD::TIME::NOW]");
  }
  return stralloc("[ERROR: not enough arguments for RRD::TIME::NOW]");
}

/* Scan buffer until an unescaped '>' arives.
 * Update argument array with arguments found.
 * Return end cursor where parsing stopped, or NULL in case of failure.
 *
 * FIXME:
 * To allow nested constructs, we call rrd_expand_vars() for arguments
 * that contain RRD::x directives. These introduce a small memory leak
 * since we have to stralloc the arguments the way parse() works.
 */
char*
scanargs(char *line, int *argument_count, char ***arguments)
{
	char 	*getP;		/* read cursor */
	char 	*putP;		/* write cursor */
	char 	Quote;		/* type of quote if in quoted string, 0 otherwise */
	int 	tagcount;	/* open tag count */
	int 	in_arg;		/* if we currently are parsing an argument or not */
	int 	argsz;		/* argument array size */
	int		curarg_contains_rrd_directives;

	/* local array of arguments while parsing */
	int argc = 0;
	char** argv;

#ifdef DEBUG_PARSER
	printf("<-- scanargs(%s) -->\n", line);
#endif

	*arguments = NULL;
	*argument_count = 0;

	/* create initial argument array of char pointers */
	argsz = 32;
	argv = (char **)malloc(argsz * sizeof(char *));
	if (!argv) {
		return NULL;
	}

	/* skip leading blanks */
	while (isspace((int)*line)) {
		line++;
	}

	getP = line;
	putP = line;

	Quote    = 0;
	in_arg   = 0;
	tagcount = 0;

	curarg_contains_rrd_directives = 0;

	/* start parsing 'line' for arguments */
	while (*getP)
	{
		unsigned char c = *getP++;

		if (c == '>' && !Quote && !tagcount) {
			/* this is our closing tag, quit scanning */
			break;
		}

 		/* remove all special chars */
		if (c < ' ') {
			c = ' ';
		}

		switch (c)
		{
		case ' ': 
			if (Quote || tagcount) {
				/* copy quoted/tagged (=RRD expanded) string */
				*putP++ = c;
			}
			else if (in_arg)
			{
				/* end argument string */
				*putP++ = 0;
				in_arg = 0;
				if (curarg_contains_rrd_directives) {
					argv[argc-1] = rrd_expand_vars(stralloc(argv[argc-1]));
					curarg_contains_rrd_directives = 0;
				}
			}
			break;

		case '"': /* Fall through */
		case '\'':
			if (Quote != 0) {
				if (Quote == c) {
			  		Quote = 0;
				} else {
					/* copy quoted string */
					*putP++ = c;
				}
			} else {
				if (!in_arg) {
					/* reference start of argument string in argument array */
					argv[argc++] = putP;
					in_arg=1;
				}
				Quote = c;
			}
			break;

		default:
				if (!in_arg) {
					/* start new argument */
					argv[argc++] = putP;
					in_arg = 1;
				}
				if (c == '>') {
					if (tagcount) {
						tagcount--;
					}
				}
				if (c == '<') {
					tagcount++;
					if (0 == strncmp(getP, "RRD::", strlen("RRD::"))) {
						curarg_contains_rrd_directives = 1;
					}
				}
			*putP++ = c;
			break;
		}

		/* check if our argument array is still large enough */
		if (argc == argsz) {
			/* resize argument array */
			argsz *= 2;
			argv = rrd_realloc(argv, argsz * sizeof(char *));
			if (*argv == NULL) {
				return NULL;
			}
		}
	}

	/* terminate last argument found */
	*putP = '\0';
	if (curarg_contains_rrd_directives) {
		argv[argc-1] = rrd_expand_vars(stralloc(argv[argc-1]));
	}

#ifdef DEBUG_PARSER
	if (argc > 0) {
		int n;
		printf("<-- arguments found [%d]\n", argc);
		for (n=0; n<argc; n++) {
			printf("arg %02d: '%s'\n", n, argv[n]);
		}
		printf("-->\n");
	} else {
		printf("<!-- No arguments found -->\n");
	}
#endif

	/* update caller's notion of the argument array and it's size */
	*arguments = argv;
	*argument_count = argc;

	if (Quote) {
		return NULL;
	}

	/* Return new scanning cursor:
	   pointer to char after closing bracket */
	return getP;
}


/*
 * Parse(): scan current portion of buffer for given tag.
 * If found, parse tag arguments and call 'func' for it.
 * The result of func is inserted at the current position
 * in the buffer.
 */
int
parse(
	char **buf, 	/* buffer */
	long i, 		/* offset in buffer */
	char *tag,  	/* tag to handle  */
	char *(*func)(long , const char **) /* function to call for 'tag' */
	)
{
	/* the name of the vairable ... */
	char *val;
	long valln;  
	char **args;
	char *end;
	long end_offset;
	int  argc;
	size_t taglen = strlen(tag);

	/* Current position in buffer should start with 'tag' */
	if (strncmp((*buf)+i, tag, taglen) != 0) {
		return 0;
	}
	/* .. and match exactly (a whitespace following 'tag') */
	if (! isspace(*((*buf) + i + taglen)) ) {
		return 0;
	}

#ifdef DEBUG_PARSER
	printf("parse(): handling tag '%s'\n", tag);
#endif

	/* Scan for arguments following the tag;
	   scanargs() puts \0 into *buf ... so after scanargs it is probably
	   not a good time to use strlen on buf */
	end = scanargs((*buf) + i + taglen, &argc, &args);
	if (end)
	{
		/* got arguments, call function for 'tag' with arguments */
		val = func(argc, args);
		free(args);
	}
	else
	{
		/* unable to parse arguments, undo 0-termination by scanargs */
		for (; argc > 0; argc--) {
			*((args[argc-1])-1) = ' ';
		}

		/* next call, try parsing at current offset +1 */
		end = (*buf) + i + 1;

		val = stralloc("[ERROR: Parsing Problem with the following text\n"
			   			" Check original file. This may have been altered "
						"by parsing.]\n\n");
	}

	/* remember offset where we have to continue parsing */
	end_offset = end - (*buf);

	valln = 0;
	if (val) {
		valln = strlen(val);
	}

	/* Optionally resize buffer to hold the replacement value:
	   Calculating the new length of the buffer is simple. add current
	   buffer pos (i) to length of string after replaced tag to length
	   of replacement string and add 1 for the final zero ... */
	if (end - (*buf) < (i + valln)) {
		/* make sure we do not shrink the mallocd block */
		size_t newbufsize = i + strlen(end) + valln + 1;
		*buf = rrd_realloc(*buf, newbufsize);

		if (*buf == NULL) {
			perror("Realoc buf:");
			exit(1);
		};
	}

	/* Update new end pointer:
	   make sure the 'end' pointer gets moved along with the 
	   buf pointer when realloc moves memory ... */
	end = (*buf) + end_offset; 

	/* splice the variable:
	   step 1. Shift pending data to make room for 'val' */
	memmove((*buf) + i + valln, end, strlen(end) + 1);

	/* step 2. Insert val */
	if (val) {
		memmove((*buf)+i, val, valln);
		free(val);
	}
	return (valln > 0 ? valln-1: valln);
}

char *
http_time(time_t *now) {
        struct tm *tmptime;
        static char buf[60];

        tmptime=gmtime(now);
        strftime(buf,sizeof(buf),"%a, %d %b %Y %H:%M:%S GMT",tmptime);
        return(buf);
}
