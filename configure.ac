dnl RRDtool AutoConf script ... 
dnl ---------------------------
dnl
dnl Created by Jeff Allen, Tobi Oetiker, Blair Zajac
dnl
dnl

dnl tell automake the this script is for rrdtool
AC_INIT([rrdtool],[1.1.0])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
# AC_CONFIG_AUX_DIR(config)

dnl all our local stuff like install scripts and include files
dnl is in there


dnl determine the type of system we are running on


AC_SUBST(VERSION)

dnl where we install our stuff ...
AC_PREFIX_DEFAULT( /usr/local/rrdtool-1.1.0 )

dnl Minimum Autoconf version required.
AC_PREREQ(2.58)

dnl At the TOP of the HEADER

AH_TOP([

#ifndef CONFIG_H
#define CONFIG_H
/* IEEE can be prevented from raising signals with fpsetmask(0) */
#undef MUST_DISABLE_FPMASK

/* IEEE math only works if SIGFPE gets actively set to IGNORE */

#undef MUST_DISABLE_SIGFPE

/* realloc does not support NULL as argument */
#undef NO_NULL_REALLOC
 ])

AH_BOTTOM([

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
])



dnl Check for the compiler and static/shared library creation.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_LIBTOOL

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_CHECK_HEADERS(sys/stat.h sys/types.h fcntl.h time.h locale.h fp_class.h malloc.h unistd.h ieeefp.h math.h sys/time.h sys/times.h sys/param.h sys/resource.h float.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_TIME
AC_STRUCT_TM

dnl substitute them in all the files listed in AC_OUTPUT
AC_SUBST(PERLFLAGS)

dnl Checks for libraries.
AC_CHECK_FUNC(acos, , AC_CHECK_LIB(m, acos))

AC_DEFUN([EX_CHECK_ALL],
[ AC_CHECK_LIB($1,$2,
  [ AC_CHECK_HEADER($3,[LIBS="-l$1 ${LIBS}"],[
     [ $echo "#include <$3>" | $CC -E - > /dev/null ] 
     AC_MSG_ERROR([Adjust CPPFLAGS so that the $4 header $3 AND its dependencies gets found])
  ] ) ],
  [ AC_MSG_ERROR([
############################################################################
I could not find a working copy of $4. Check config.log for hints on why
this is the case. Maybe you need to set LDFLAGS appropriately so that the
linker can find lib$1. If you have not installed $4, you can get it
either from its original home on

   $6

Or you can find an archive copy on

   http://people.ee.ethz.ch/~oetiker/webtools/rrdtool/pub/libs

The last tested version of $4 is $5.
############################################################################
])] )
]
)
 
dnl How the vertical axis label is printed
AC_ARG_VAR(RRDGRAPH_YLEGEND_ANGLE, 
 [Vertical label angle: 90.0 (default) or 270.0])
AC_DEFINE_UNQUOTED(RRDGRAPH_YLEGEND_ANGLE,${RRDGRAPH_YLEGEND_ANGLE:-90.0},
 [Vertical label angle: 90.0 (default) or 270.0])


AC_ARG_ENABLE(rrdcgi,[  --disable-rrdcgi        disable building of rrdcgi],
[],[enable_rrdcgi=yes])
AM_CONDITIONAL(BUILD_RRDCGI,[test $enable_rrdcgi != no])
 
if test $enable_rrdcgi != no; then
EX_CHECK_ALL(cgi,        cgiInit,                   cgi.h,                  cgilib,      0.5,    http://www.infodrom.org/projects/cgilib)
fi
EX_CHECK_ALL(art_lgpl_2, art_vpath_add_point,       libart_lgpl/libart.h,   libart_lgpl, 2.3.16, ftp://ftp.gnome.org/pub/GNOME/sources/libart_lgpl/2.3/)
EX_CHECK_ALL(z,          zlibVersion,               zlib.h,                 zlib,        1.2.1,  http://www.gzip.org/zlib/)
EX_CHECK_ALL(png,        png_access_version_number, png.h,                  libpng,      1.2.5,  http://prdownloads.sourceforge.net/libpng/)
EX_CHECK_ALL(freetype,   FT_Init_FreeType,          ft2build.h,             freetype,    2.1.5,  http://prdownloads.sourceforge.net/freetype/)
 
dnl Check for pthreads (pieces of code from Bert Driehuis)
 
AC_SUBST(MULTITHREAD_CFLAGS)
AC_SUBST(MULTITHREAD_LDFLAGS)

AC_DEFUN(AC_CHECK_PTHREAD, [
   AC_MSG_CHECKING([pthreads: trying no flags])
   AC_CACHE_VAL(ac_cv_flag_pthread,[
 	AC_TRY_LINK([
 		#include <pthread.h>
 	],
 	[pthread_exit(0);],
 	eval "ac_cv_flag_pthread=no",
 	[
 		AC_MSG_RESULT([didn't work])
 		AC_MSG_CHECKING([pthreads: trying to add -pthread])
 		ac_save_CFLAGS="$CFLAGS"
 		CFLAGS="$CFLAGS -pthread"
     AC_TRY_LINK([
       #include <pthread.h>
       ],
       [pthread_exit(0);],
       eval "ac_cv_flag_pthread=yes",
       AC_MSG_ERROR([Can't figure out pthreads])
     )
     CFLAGS="$ac_save_CFLAGS"
 	])
   ])
 
   if test "$ac_cv_flag_pthread" = "no"; then
     AC_MSG_RESULT([good, -pthread flag not necessary])
   else
     AC_MSG_RESULT([adding -pthread flag])
     MULTITHREAD_CFLAGS="-pthread"
   fi
 
   AC_MSG_CHECKING([for libpthread])
   AC_CACHE_VAL(ac_cv_lib_pthread,[
     ac_save_LIBS="$LIBS"
     LIBS="-lpthread"
     AC_TRY_LINK([
       #include <pthread.h>
       ],
       [pthread_exit(0);],
       eval "ac_cv_lib_pthread=-lphtread",
       eval "ac_cv_lib_pthread=no"
     )
     LIBS="$ac_save_LIBS"
   ])
   if test "$ac_cv_lib_pthread" = "no"; then
     AC_MSG_RESULT([no -lpthread necessary])
   else
     AC_MSG_RESULT([adding -lpthread to LIBS])
     MULTITHREAD_LDFLAGS="-lpthread"
   fi
])
 
AC_ARG_ENABLE(pthread,[  --disable-pthread       disable multithread support],
[],[enable_pthread=yes])
 
if test $enable_pthread != no; then 
 	AC_CHECK_PTHREAD
fi
 
AM_CONDITIONAL(BUILD_MULTITHREAD,[test $enable_pthread != no])
  
dnl Check for Perl.
AC_PATH_PROG(PERL, perl, no)
if test "x$PERL" = "xno"; then
	COMP_PERL=
else
	COMP_PERL="perl_piped perl_shared"
	AC_MSG_CHECKING(for shared library extension)
	SO_EXT=`$PERL -e 'use Config; if (defined $Config{so} and $Config{so} ne 'a') {print "$Config{so}\n"} else {print "so\n"};'`
	AC_MSG_RESULT($SO_EXT)
        AC_MSG_CHECKING(for the C compiler perl wants to use to build its modules)
        PROBLEMCC=`$PERL -e 'use Config; ($cc = $Config{cc}) =~ s/\s.*//; exit 0 if -x $cc; map {if (-x "$_/$cc"){exit 0}} split /:/, $ENV{PATH};print $Config{cc}'`
        if test x$PROBLEMCC != x; then
                AC_MSG_RESULT(no)
                echo Could not find the Compiler: '"'$PROBLEMCC'"'
                echo which has been used to build perl. You may have to compile
                echo your own copy of perl to make this work. Binary perls often
                echo exhibit this problem
                exit 1
        else
                AC_MSG_RESULT(found)
        fi
fi
AC_SUBST(COMP_PERL)
AC_SUBST(SO_EXT)

dnl Check for Tcl.
withval=""
AC_ARG_WITH(tcllib,[  --with-tcllib=DIR       location of the tclConfig.sh])
found=0
AC_MSG_CHECKING(for tclConfig.sh in $withval)
if test -f "$withval/tclConfig.sh" ; then
    	tcl_config=$withval/tclConfig.sh
        found=1
        AC_MSG_RESULT(yes)
        break
else
        AC_MSG_RESULT(no)
fi

if test $found -eq 0 ; then
        AC_MSG_WARN([tclConfig.sh not found - Tcl interface won't be built])
else
	. $tcl_config
fi

dnl Pass additional perl options when generating Makefile from Makefile.PL
# Options to pass when configuring perl module
AC_ARG_WITH(perl-options,
[  --with-perl-options=[OPTIONS]  options to pass on command-line when
                          generating Makefile from Makefile.PL],
[PERL_MAKE_OPTIONS=$withval])
AC_SUBST(PERL_MAKE_OPTIONS)

AM_CONDITIONAL(COMP_TCL, test x$found = x1 )

AC_SUBST(TCL_PREFIX)
AC_SUBST(TCL_SHLIB_CFLAGS)
AC_SUBST(TCL_SHLIB_LD)
AC_SUBST(TCL_SHLIB_SUFFIX)
AC_SUBST(TCL_PACKAGE_PATH)
AC_SUBST(TCL_LD_SEARCH_FLAGS)



dnl Check for nroff
AC_PATH_PROGS(NROFF, gnroff nroff)
AC_PATH_PROGS(TROFF, groff troff)

dnl Does the compiler like -Wall and -pedantic?
if test "x$GCC" = "xyes"; then
  oCFLAGS=$CFLAGS
  CFLAGS="$CFLAGS -Wall -pedantic -Wshadow -Wpointer-arith -Wcast-align -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Winline -W"
  AC_CACHE_CHECK(if we can use GCC-specific compiler options, rd_cv_gcc_opt,
                [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[return 0 ]])],[rd_cv_gcc_opt=yes],[rd_cv_gcc_opt=no ])
               ]
        )
  if test $rd_cv_gcc_opt = no; then
         CFLAGS=$oCFLAGS
  fi
fi

dnl add pic flag in any case this makes sure all our code is relocatable
CFLAGS="$CFLAGS "`grep pic_flag= libtool | head -1|sed -e 's/.*pic_flag=//' -e 's/"//g'`

ZLIB_CFLAGS=$CFLAGS
dnl it seems that hpux chockes on -fPIC for some reason
case $target_os in
*hpux*)
        ZLIB_CFLAGS=`echo $CFLAGS|sed -e 's/-fPIC/-fpic/g'`
;;
esac
AC_SUBST(ZLIB_CFLAGS)

dnl Checks for library functions.
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF

dnl for each function found we get a definition in config.h 
dnl of the form HAVE_FUNCTION

AC_CHECK_FUNCS(tzset opendir readdir chdir chroot getuid setlocale strerror strerror_r snprintf vsnprintf fpclass class fp_class isnan memmove strchr mktime getrusage gettimeofday)

dnl HP-UX 11.00 does not have finite but does have isfinite as a macro
AC_CHECK_FUNCS(fpclassify, ,
  [AC_MSG_CHECKING(for fpclassify with <math.h>)
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <math.h>]], [[float f = 0.0; fpclassify(f)]])],[AC_MSG_RESULT(yes)
      AC_DEFINE(HAVE_FPCLASSIFY)],[AC_MSG_RESULT(no)])])
AC_CHECK_FUNCS(finite, ,
  [AC_CHECK_FUNCS(isfinite, ,
    [AC_MSG_CHECKING(for isfinite with <math.h>)
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <math.h>]], [[float f = 0.0; isfinite(f)]])],[AC_MSG_RESULT(yes)
      AC_DEFINE(HAVE_ISFINITE)],[AC_MSG_RESULT(no)])])])
AC_CHECK_FUNCS(isinf, ,
  [AC_MSG_CHECKING(for isinf with <math.h>)
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <math.h>]], [[float f = 0.0; isinf(f)]])],[AC_MSG_RESULT(yes)
      AC_DEFINE(HAVE_ISINF)],[AC_MSG_RESULT(no)])])

dnl what does realloc do if it gets called with a NULL pointer

AC_CACHE_CHECK([if realloc can deal with NULL], rd_cv_null_realloc,
[AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <stdlib.h>
	      int main(void){
              char *x = NULL;
	      x = realloc (x,10);
	      if (x==NULL) return 1;
	      return 0;
             }]])],[rd_cv_null_realloc=yes],[rd_cv_null_realloc=nope],[:])])

if test x"$rd_cv_null_realloc" = xnope; then
AC_DEFINE(NO_NULL_REALLOC)
fi

dnl determine how to get IEEE math working
dnl AC_IEEE(MESSAGE, set rd_cv_ieee_[var] variable, INCLUDES,
dnl   FUNCTION-BODY, [ACTION-IF-FOUND [,ACTION-IF-NOT-FOUND]])

AC_C_BIGENDIAN

AC_DEFUN([AC_IEEE], 
AC_MSG_CHECKING([if IEEE math works $1])
AC_CACHE_VAL([rd_cv_ieee_$2],
[AC_RUN_IFELSE([AC_LANG_SOURCE([[$3

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

/* Solaris */
#if (! defined(HAVE_ISINF) && defined(HAVE_FPCLASS))
#  define HAVE_ISINF 1
#  define isinf(a) (fpclass(a) == FP_NINF || fpclass(a) == FP_PINF)
#endif

/* Digital UNIX */
#if (! defined(HAVE_ISINF) && defined(HAVE_FP_CLASS) && defined(HAVE_FP_CLASS_H))
#  define HAVE_ISINF 1
#  define isinf(a) (fp_class(a) == FP_NEG_INF || fp_class(a) == FP_POS_INF)
#endif 

/* AIX */
#if (! defined(HAVE_ISINF) && defined(HAVE_CLASS))
#  define HAVE_ISINF 1
#  define isinf(a) (class(a) == FP_MINUS_INF || class(a) == FP_PLUS_INF)
#endif

#if (! defined(HAVE_ISINF) && defined(HAVE_FPCLASSIFY) && defined(FP_PLUS_INF) && defined(FP_MINUS_INF))
#  define HAVE_ISINF 1
#  define isinf(a) (fpclassify(a) == FP_MINUS_INF || fpclassify(a) == FP_PLUS_INF)
#endif

#if (! defined(HAVE_ISINF) && defined(HAVE_FPCLASSIFY) && defined(FP_INFINITE))
#  define HAVE_ISINF 1
#  define isinf(a) (fpclassify(a) == FP_INFINITE)
#endif

#include <stdio.h>
int main(void){
    double nan,inf,c,zero;
    $4;
    /* some math to see if we get a floating point exception */
    zero=sin(0.0); /* don't let the compiler optimize us away */
    nan=0.0/zero; /* especially here */
    inf=1.0/zero; /* and here. I want to know if it can do the magic */
		  /* at run time without sig fpe */
    c = inf + nan;
    c = inf / nan;
    if (! isnan(nan)) {printf ("not isnan(NaN) ... "); return 1;}
    if (nan == nan) {printf ("nan == nan ... "); return 1;}
    if (! isinf(inf)) {printf ("not isinf(oo) ... "); return 1;}
    if (! isinf(-inf)) {printf ("not isinf(-oo) ... "); return 1;}
    if (! inf > 0) {printf ("not inf > 0 ... "); return 1;}
    if (! -inf < 0) {printf ("not -inf < 0 ... "); return 1;}
    return 0;
 }]])],[rd_cv_ieee_$2=yes],[rd_cv_ieee_$2=no],[:])])
dnl these we run regardles is cached or not
if test x${rd_cv_ieee_$2} = "xyes"; then
 AC_MSG_RESULT(yes)
 $5
else
 AC_MSG_RESULT(no)
 $6
fi

)

_cflags=${CFLAGS}
AC_IEEE([out of the box], works, , , ,
  [CFLAGS="$_cflags -ieee"
  AC_IEEE([with the -ieee switch], switch, , , ,
    [CFLAGS="$_cflags -qfloat=nofold"
    AC_IEEE([with the -qfloat=nofold switch], nofold, , , ,
      [CFLAGS="$_cflags -w -qflttrap=enable:zerodivide"
      AC_IEEE([with the -w -qflttrap=enable:zerodivide], flttrap, , , ,
       [CFLAGS="$_cflags -mieee"
       AC_IEEE([with the -mieee switch], mswitch, , , ,
         [CFLAGS="$_cflags -q float=rndsngl"
         AC_IEEE([with the -q float=rndsngl switch], qswitch, , , ,
           [CFLAGS="$_cflags -OPT:IEEE_NaN_inf=ON"
           AC_IEEE([with the -OPT:IEEE_NaN_inf=ON switch], ieeenaninfswitch, , , ,
             [CFLAGS="$_cflags -OPT:IEEE_comparisons=ON"
             AC_IEEE([with the -OPT:IEEE_comparisons=ON switch], ieeecmpswitch, , , ,
               [CFLAGS=$_cflags
               AC_IEEE([with fpsetmask(0)], mask,
                 [#include <floatingpoint.h>], [fpsetmask(0)],
                 [AC_DEFINE(MUST_DISABLE_FPMASK)
	         PERLFLAGS="CCFLAGS=-DMUST_DISABLE_FPMASK"],
                 [AC_IEEE([with signal(SIGFPE,SIG_IGN)], sigfpe,
                   [#include <signal.h>], [signal(SIGFPE,SIG_IGN)],
                   [AC_DEFINE(MUST_DISABLE_SIGFPE)
                   PERLFLAGS="CCFLAGS=-DMUST_DISABLE_SIGFPE"],		
                   AC_MSG_ERROR([
Your Compiler does not do propper IEEE math ... Please find out how to
make IEEE math work with your compiler and let me know (oetiker@ee.ethz.ch).
Check config.log to see what went wrong ...
]))])])])])])])])])])


AC_CONFIG_FILES([examples/shared-demo.pl			\
          examples/piped-demo.pl			\
          examples/stripes.pl				\
          examples/bigtops.pl				\
          examples/minmax.pl				\
          examples/cgi-demo.cgi				\
          examples/Makefile				\
          doc/Makefile					\
          src/Makefile					\
          bindings/Makefile                             \
          bindings/tcl/Makefile				\
          Makefile])
AC_CONFIG_COMMANDS([default],[[\
          chmod +x examples/*.cgi examples/*.pl]],[[]])
AC_OUTPUT

AC_MSG_CHECKING(in)
AC_MSG_RESULT(and out again)

echo $ECHO_N "ordering CD from http://people.ee.ethz.ch/~oetiker/wish $ECHO_C" 1>&6
sleep 1
echo $ECHO_N ".$ECHO_C" 1>&6
sleep 1
echo $ECHO_N ".$ECHO_C" 1>&6
sleep 1
echo $ECHO_N ".$ECHO_C" 1>&6
sleep 1
echo $ECHO_N ".$ECHO_C" 1>&6
sleep 1
AC_MSG_RESULT([ just kidding ;-)])
echo
echo "----------------------------------------------------------------"
echo "Config is DONE!"
echo
echo "Type 'make' to compile the software and use 'make install' to "
echo "install everything to: $prefix."
echo
echo "If you want to install the perl"
echo "modules in site-perl, try 'make site-perl-install'."
echo 
echo "       ... that wishlist is NO JOKE. If you find RRDtool useful"
echo "make me happy. Go to http://people.ee.ethz.ch/~oetiker/wish and"
echo "place an order."
echo 
echo "                               -- Tobi Oetiker <tobi@oetiker.ch>"
echo "----------------------------------------------------------------"
