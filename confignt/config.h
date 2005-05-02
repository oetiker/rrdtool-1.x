/* config.h.in.  Generated from configure.ac by autoheader.  */
#ifndef CONFIG_H
#define CONFIG_H

#include <math.h>
#include <float.h>

/* realloc does not support NULL as argument */

#define HAVE_STRFTIME 1
#define HAVE_TIME_H 1
#define HAVE_LOCALE_H 1
#define HAVE_TZSET 1
#define HAVE_SETLOCALE 1
#define HAVE_MATH_H 1
#define HAVE_FLOAT_H 1
#define HAVE_MEMMOVE 1
#define HAVE_MALLOC_H 1
#define HAVE_MKTIME 1
#define HAVE_STRFTIME 1
#define HAVE_STRING_H 1
#define HAVE_VPRINTF 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1

//#define PACKAGE "rrdtool"
//#define PACKAGE_NAME "rrdtool"
//#define PACKAGE_STRING "rrdtool 1.2rc6"
//#define PACKAGE_VERSION "1.2rc6"
//#define VERSION "1.2rc6"

#define STDC_HEADERS 1
#define RRD_DEFAULT_FONT "arial.ttf"

#define snprintf _snprintf
#define strftime strftime_ 

#define NO_NULL_REALLOC 1
#if NO_NULL_REALLOC
# define rrd_realloc(a,b) ( (a) == NULL ? malloc( (b) ) : realloc( (a) , (b) ))
#else
# define rrd_realloc(a,b) realloc((a), (b))
#endif      

//#define DEBUG 1

/* Vertical label angle: 90.0 (default) or 270.0 */
#define RRDGRAPH_YLEGEND_ANGLE 90.0

/* Define to 1 if you have the ANSI C header files. */


#endif /* CONFIG_H */

