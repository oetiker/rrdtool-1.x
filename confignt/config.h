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
/* Code in rrd_graph.c:rrd_graph_init() uses the %windir%
 * environment variable to override this. This should
 * avoid the recompile problem if the system directory is
 * c:/windows vs. d:/winnt.
 * This #define can't be removed because:
 * (1) the constant is used outside of rrd_graph_init() to init a struct
 * (2) windir might not be available in all environments
 */
#define RRD_DEFAULT_FONT "c:/windows/fonts/cour.ttf"

#define RRDGRAPH_YLEGEND_ANGLE 90.0

#define HAVE_STRING_H 1

