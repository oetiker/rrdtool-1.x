# 
# INCLUDED_LIB(LIBRARY, FUNCTION, OTHER-LIBRARIES = none)
# ---------------------------------------------
# Contributed by Mat Zimmermann
#
AC_DEFUN(INCLUDED_LIB,
[
use_installed_lib$1=
AC_ARG_WITH([lib$1],
            [AC_HELP_STRING([--with-lib$1],
                            [Use installed lib$1 (under PREFIX)])],
            [if test -n "$withval"; then
               if test "$withval" = yes; then
                 withval="default path"
               else
                 CFLAGS="$CFLAGS -I${withval}/include"
                 LDFLAGS="$LDFLAGS -L${withval}/lib"
               fi
             fi
             AC_CHECK_LIB([$1],[$2],
                          [AC_MSG_NOTICE([using lib$1 in ${withval}])
                           use_installed_lib$1=yes],
                          [AC_MSG_ERROR([lib$1 not found in ${withval}])],
                          [$3])],
             [AC_MSG_NOTICE([using included lib$1])])
AM_CONDITIONAL([USE_INSTALLED_lib$1],
               [test "$use_installed_lib$1" = yes])
])
