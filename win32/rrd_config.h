/* rrd_config.h for Visual Studio 2010, 2012 and newer */

#ifndef RRD_CONFIG_H
#define RRD_CONFIG_H

/* Define to the full name of this package. */
#define PACKAGE_NAME "rrdtool"

/* Define to the version of this package. */
#define PACKAGE_MAJOR       1
#define PACKAGE_MINOR       7
#define PACKAGE_REVISION    0
#define PACKAGE_VERSION     "1.7.0"
#define NUMVERS             1.70

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
 * VS2005 and later default size for time_t is 64-bit, unless
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

/* Define to 1 if you have the `asprintf' function. */
#define HAVE_ASPRINTF 1

/* Define to 1 if you have the `chdir' function. */
#define HAVE_CHDIR 1

/* Define to 1 if you have the <ctype.h> header file. */
#define HAVE_CTYPE_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the `isnan' function. */
#define HAVE_ISNAN 1

/* glib has g_regex_new since 2.14 */
#define HAVE_G_REGEX_NEW 1

/* is rrd_graph supported by this install */
#define HAVE_RRD_GRAPH /**/

/* is rrd_restore supported by this install */
#define HAVE_RRD_RESTORE /**/

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

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* is there an external timezone variable instead ? */
#define HAVE_TIMEZONE 1

/* Define to 1 if you have the `tzset' function. */
#define HAVE_TZSET 1

/* Define to 1 if you have the `uintptr_t' standard type. */
#define HAVE_UINTPTR_T 1

/* Define to 1 if you have the `vasprintf' function. */
#define HAVE_VASPRINTF 1

/* Misc missing Windows defines */
#ifndef PATH_MAX    /* PATH_MAX is defined in win32/dirent.h too. Relevant, if included before rrd_config.h */
#define PATH_MAX _MAX_PATH  /* max. length of full pathname is 260 under Windows, _MAX_PATH is defined in stdlib.h */
#endif


#include <ctype.h>
#include <direct.h>
#include <float.h>
#include <math.h>
#include <io.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <WinSock.h>

#include <errno.h>  /* errno.h has to be included before the redefinitions of ENOBUFS and ENOTCONN below */
/*
 * Windows Sockets errors redefined as regular Berkeley error constants.
 */
#undef ENOBUFS  /* undefine first, because this is defined in errno.h and redefined here */
#define ENOBUFS WSAENOBUFS
#undef ENOTCONN /* undefine first, because this is defined in errno.h and redefined here */
#define ENOTCONN WSAENOTCONN

#include "mkstemp.h"

/* _MSC_VER is not defined, when using the resource compiler (rc).
 * See: https://docs.microsoft.com/en-us/windows/desktop/menurc/predefined-macros
 * for how to conditionally compile the code with the RC compiler using RC_INVOKED
*/
#ifndef RC_INVOKED
#if _MSC_VER < 1900
#define isinf(a) (_fpclass(a) == _FPCLASS_NINF || _fpclass(a) == _FPCLASS_PINF)
#define isnan _isnan
#define snprintf _snprintf
#endif
#endif

#define finite _finite
#define rrd_realloc(a,b) ( (a) == NULL ? malloc( (b) ) : realloc( (a) , (b) ))
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)

// in MSVC++ 12.0 / Visual Studio 2013 is a definition of round in math.h
// some values of _MSC_VER
//MSVC++ 14.0 _MSC_VER == 1900 (Visual Studio 2015)
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

#endif
