/* config.h.nt: what configure _would_ say, if it ran on NT */

#define STDC_HEADERS 1

/* Define if you have the strftime function.  */
#define HAVE_STRFTIME 1

/* Define if you have the <math.h> header file.  */
#define HAVE_MATH_H 1

#define rrd_realloc(a,b) realloc((a), (b))

#define snprintf _snprintf
