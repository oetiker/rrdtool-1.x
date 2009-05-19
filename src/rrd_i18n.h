/*****************************************************************************
 * RRDtool 1.3.8  Copyright by Takao Fujiwara, 2008
 *****************************************************************************
 * rrd_i18n.h   Common Header File
 *****************************************************************************/
#ifdef  __cplusplus
extern    "C" {
#endif


#ifndef _RRD_I18N_H
#define _RRD_I18N_H

#ifndef _
/* This is for other GNU distributions with internationalized messages.
   When compiling libc, the _ macro is predefined.  */
#if defined(HAVE_LIBINTL_H) && defined(BUILD_LIBINTL)
#  include <libintl.h>
#define _(String) gettext (String)
#else
#define _(String) (String)
#endif
#define N_(String) (String)
#endif


#endif

#ifdef  __cplusplus
}
#endif
