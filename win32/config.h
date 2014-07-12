/* config.h.msvc.  Hand-tweaked config.h for MSVC compiler.  */
#ifndef CONFIG_H
#define CONFIG_H

#include <math.h>
#include <float.h>
#include <direct.h>
#include <ctype.h>
#include <stdlib.h>

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
#define HAVE_STDLIB_H 1
#define HAVE_VSNPRINTF 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_RRD_GRAPH 1

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

#define NUMVERS 1.4050
#define PACKAGE_NAME "rrdtool"
#define PACKAGE_VERSION "1.4.5"
#define PACKAGE_STRING PACKAGE_NAME " " PACKAGE_VERSION

#define isinf(a) (_fpclass(a) == _FPCLASS_NINF || _fpclass(a) == _FPCLASS_PINF)
#define isnan _isnan
#define finite _finite
#define snprintf _snprintf
//#define vsnprintf _vsnprintf
//#define strftime strftime_

#define NO_NULL_REALLOC 1
#if NO_NULL_REALLOC
# define rrd_realloc(a,b) ( (a) == NULL ? malloc( (b) ) : realloc( (a) , (b) ))
#else
# define rrd_realloc(a,b) realloc((a), (b))
#endif

/* Vertical label angle: 90.0 (default) or 270.0 */
#define RRDGRAPH_YLEGEND_ANGLE 90.0

#define RRD_DEFAULT_FONT "Courier"

/* #define DEBUG 1 */

// in MSVC++ 12.0 / Visual Studio 2013 is a definition of round in math.h
// some values of _MSC_VER
//MSVC++ 12.0 _MSC_VER == 1800 (Visual Studio 2013)
//MSVC++ 11.0 _MSC_VER == 1700 (Visual Studio 2012)
//MSVC++ 10.0 _MSC_VER == 1600 (Visual Studio 2010)
//MSVC++ 9.0  _MSC_VER == 1500 (Visual Studio 2008)
//MSVC++ 8.0  _MSC_VER == 1400 (Visual Studio 2005)
//MSVC++ 7.1  _MSC_VER == 1310 (Visual Studio 2003)
//MSVC++ 7.0  _MSC_VER == 1300
//MSVC++ 6.0  _MSC_VER == 1200
//MSVC++ 5.0  _MSC_VER == 1100
#if _MSC_VER < 1800
__inline int round(double a){ int x = (a + 0.5); return x; }
#endif

#define SIZEOF_TIME_T 8 

#endif                          /* CONFIG_H */
