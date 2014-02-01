#ifndef RRD_I18N_H_E9D8F44A32654DF9B92B1862D5371142
#define RRD_I18N_H_E9D8F44A32654DF9B92B1862D5371142

/*****************************************************************************
 * RRDtool 1.4.3  Copyright by Takao Fujiwara, 2008
 *****************************************************************************
 * rrd_i18n.h   Common Header File
 *****************************************************************************/
#ifdef  __cplusplus
extern    "C" {
#endif

#ifdef ENABLE_NLS
#  ifdef _LIBC
#    include <libintl.h>
#  else
#    include "gettext.h"
#    define _(String) gettext (String)
#  endif
#else
#  define _(String) (String)
#endif

#define N_(String) String

#ifdef  __cplusplus
}
#endif

#endif
