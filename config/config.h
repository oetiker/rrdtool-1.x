/* config/config.h.  Generated automatically by configure.  */
/* config/config.h.in.  Generated automatically from configure.in by autoheader.  */
#ifndef CONFIG_H
#define CONFIG_H


/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you don't have vprintf but do have _doprnt.  */
/* #undef HAVE_DOPRNT */

/* Define if you have the strftime function.  */
#define HAVE_STRFTIME 1

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* IEEE can be prevented from raising signals with fpsetmask(0) */
/* #undef MUST_DISABLE_FPMASK */

/* #undef MUST_DISABLE_SIGFPE */

/* realloc does not support NULL as argument */
/* #undef NO_NULL_REALLOC */

/* Define if you have the class function.  */
/* #undef HAVE_CLASS */

/* Define if you have the finite function.  */
#define HAVE_FINITE 1

/* Define if you have the fp_class function.  */
/* #undef HAVE_FP_CLASS */

/* Define if you have the fpclass function.  */
#define HAVE_FPCLASS 1

/* Define if you have the fpclassify function.  */
/* #undef HAVE_FPCLASSIFY */

/* Define if you have the getrusage function.  */
#define HAVE_GETRUSAGE 1

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the isinf function.  */
/* #undef HAVE_ISINF */

/* Define if you have the isnan function.  */
#define HAVE_ISNAN 1

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the mktime function.  */
#define HAVE_MKTIME 1

/* Define if you have the snprintf function.  */
#define HAVE_SNPRINTF 1

/* Define if you have the strchr function.  */
#define HAVE_STRCHR 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the vsnprintf function.  */
#define HAVE_VSNPRINTF 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <float.h> header file.  */
#define HAVE_FLOAT_H 1

/* Define if you have the <fp_class.h> header file.  */
/* #undef HAVE_FP_CLASS_H */

/* Define if you have the <ieeefp.h> header file.  */
#define HAVE_IEEEFP_H 1

/* Define if you have the <malloc.h> header file.  */
#define HAVE_MALLOC_H 1

/* Define if you have the <math.h> header file.  */
#define HAVE_MATH_H 1

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/resource.h> header file.  */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/times.h> header file.  */
#define HAVE_SYS_TIMES_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the m library (-lm).  */
#define HAVE_LIBM 1

/* Name of package */
#define PACKAGE "rrdtool"

/* Version number of package */
#define VERSION "1.0.33"


/* define strrchr, strchr and memcpy, memmove in terms of bsd funcs
   make sure you are NOT using bcopy, index or rindex in the code */
      
#if STDC_HEADERS
# include <string.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr (), *strrchr ();
# ifndef HAVE_MEMMOVE
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif


#if NO_NULL_REALLOC
# define rrd_realloc(a,b) ( (a) == NULL ? malloc( (b) ) : realloc( (a) , (b) ))
#else
# define rrd_realloc(a,b) realloc((a), (b))
#endif      

#if HAVE_MATH_H
#  include <math.h>
#endif

#if HAVE_FLOAT_H
#  include <float.h>
#endif

#if HAVE_IEEEFP_H
#  include <ieeefp.h>
#endif

#if HAVE_FP_CLASS_H
#  include <fp_class.h>
#endif

/* for Solaris */
#if (! defined(HAVE_ISINF) && defined(HAVE_FPCLASS))
#  define HAVE_ISINF 1
#  define isinf(a) (fpclass(a) == FP_NINF || fpclass(a) == FP_PINF)
#endif

/* for OSF1 Digital Unix */
#if (! defined(HAVE_ISINF) && defined(HAVE_FP_CLASS) && defined(HAVE_FP_CLASS_H))
#  define HAVE_ISINF 1
#  define isinf(a) (fp_class(a) == FP_NEG_INF || fp_class(a) == FP_POS_INF)
#endif

#if (! defined(HAVE_ISINF) && defined(HAVE_FPCLASSIFY) && defined(FP_PLUS_INF) && defined(FP_MINUS_INF))
#  define HAVE_ISINF 1
#  define isinf(a) (fpclassify(a) == FP_MINUS_INF || fpclassify(a) == FP_PLUS_INF)
#endif

#if (! defined(HAVE_ISINF) && defined(HAVE_FPCLASSIFY) && defined(FP_INFINITE))
#  define HAVE_ISINF 1
#  define isinf(a) (fpclassify(a) == FP_INFINITE)
#endif

/* for AIX */
#if (! defined(HAVE_ISINF) && defined(HAVE_CLASS))
#  define HAVE_ISINF 1
#  define isinf(a) (class(a) == FP_MINUS_INF || class(a) == FP_PLUS_INF)
#endif

#if (! defined (HAVE_FINITE) && defined (HAVE_ISFINITE))
#  define HAVE_FINITE 1
#  define finite(a) isfinite(a)
#endif

#if (! defined(HAVE_FINITE) && defined(HAVE_ISNAN) && defined(HAVE_ISINF))
#  define HAVE_FINITE 1
#  define finite(a) (! isnan(a) && ! isinf(a))
#endif

#ifndef HAVE_FINITE
#error "Can't compile without finite function"
#endif

#ifndef HAVE_ISINF
#error "Can't compile without isinf function"
#endif

#endif /* CONFIG_H */

