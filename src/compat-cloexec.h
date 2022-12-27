#ifndef H_RRDTOOL_SRC_COMPAT_CLOEXEC_H
#define H_RRDTOOL_SRC_COMPAT_CLOEXEC_H

#include <rrd_config.h>

#ifndef HAVE_DECL_O_CLOEXEC
#  define O_CLOEXEC 0
#endif

#endif	/* H_RRDTOOL_SRC_COMPAT_CLOEXEC_H */
