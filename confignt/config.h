/* config.h.nt: what configure _would_ say, if it ran on NT */

#define STDC_HEADERS 1

/* Define if you have the strftime function.  */
#define HAVE_STRFTIME 1

/* Define if you have the <math.h> header file.  */
#define HAVE_MATH_H 1

#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1

#define rrd_realloc(a,b) realloc((a), (b))

#define snprintf _snprintf

/* Code in rrd_graph.c:rrd_graph_init() uses the %windir% environment
 * variable to find the actual location of this relative font path to avoid
 * the recompile problem if the system directory is c:/windows vs. d:/winnt. 
 */

#define RRD_DEFAULT_FONT "cour.ttf"

#define RRDGRAPH_YLEGEND_ANGLE 90.0

#define HAVE_STRING_H 1

