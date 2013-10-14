/* rrd_config.h for Visual Studio 2010, 2012 */

#ifndef RRD_CONFIG_H
#define RRD_CONFIG_H

/* Define to the full name of this package. */
#define PACKAGE_NAME "rrdtool"

/* Define to the version of this package. */
#define PACKAGE_MAJOR       1
#define PACKAGE_MINOR       4
#define PACKAGE_REVISION    999
#define PACKAGE_VERSION     "1.4.999"
#define NUMVERS             1.4999

#define RRD_DEFAULT_FONT "Courier"

/* Vertical label angle: -90.0 (default) or 90.0 */
#define RRDGRAPH_YLEGEND_ANGLE 90.0

/*
    _MSC_VER
    _WIN32          _WIN64

    __GNUC__        __GNUC_MINOR__  __GNUC_PATCHLEVEL__
    _WIN32          _WIN64
    __MINGW32__     __MINGW64__

    sizeof(time_t): 4
    Linux x86 gcc, Windows x86 gcc

    sizeof(time_t): 8
    Linux x64 gcc, Windows x64 gcc, Visual C++ 2005 or later
 */

/*
 * The size of `time_t', as computed by sizeof.
 * VS2005 and later dafault size for time_t is 64-bit, unless
 * _USE_32BIT_TIME_T has been defined to use a 32-bit time_t.
 */
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#  ifndef _USE_32BIT_TIME_T
#    define SIZEOF_TIME_T 8
#  else
#    define SIZEOF_TIME_T 4
#  endif
#else
#  ifdef _WIN64
#    define SIZEOF_TIME_T 8
#  else
#    define SIZEOF_TIME_T 4
#  endif
#endif

/* Define to 1 if you have the `chdir' function. */
#define HAVE_CHDIR 1

/* Define to 1 if you have the <ctype.h> header file. */
#define HAVE_CTYPE_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the `isnan' function. */
#define HAVE_ISNAN 1

/* is rrd_graph supported by this install */
#define HAVE_RRD_GRAPH /**/

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strftime' function. */
#define HAVE_STRFTIME 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* is there an external timezone variable instead ? */
#define HAVE_TIMEZONE 1

/* Define to 1 if you have the `tzset' function. */
#define HAVE_TZSET 1

/* Misc Missing Windows defines */
#define PATH_MAX 1024

/*
 * Windows Sockets errors redefined as regular Berkeley error constants.
 */
#define ENOBUFS WSAENOBUFS
#define ENOTCONN WSAENOTCONN

#include <ctype.h>
#include <direct.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <WinSock.h>

#define isinf(a) (_fpclass(a) == _FPCLASS_NINF || _fpclass(a) == _FPCLASS_PINF)
#define isnan _isnan
#define finite _finite
#define snprintf _snprintf
#define rrd_realloc(a,b) ( (a) == NULL ? malloc( (b) ) : realloc( (a) , (b) ))
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#define strcasecmp _stricmp
#define strcasencmp _strnicmp

#pragma warning(disable: 4244)
__inline int round(double a){ return (int) (a + 0.5); }

#endif
