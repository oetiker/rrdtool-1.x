#ifndef H_RRDTOOL_SRC_COMPAT_CLOEXEC_H
#define H_RRDTOOL_SRC_COMPAT_CLOEXEC_H

#include <rrd_config.h>

#ifndef HAVE_DECL_O_CLOEXEC
#  define O_CLOEXEC 0
#endif

#ifndef HAVE_DECL_SOCK_CLOEXEC
#  define SOCK_CLOEXEC 0
#endif

#include <stdio.h>

FILE *_rrd_fopen(const char *restrict pathname, const char *restrict mode_raw);

#ifdef RRD_HAVE_WORKING_FOPEN_E
#  define rrd_fopen(_pathname, _mode) fopen(_pathname, _mode)
#else

inline static
FILE *rrd_fopen(const char *restrict pathname, const char *restrict mode)
{
	return _rrd_fopen(pathname, mode);
}

#endif	/* RD_HAVE_WORKING_FOPEN_E */

#endif	/* H_RRDTOOL_SRC_COMPAT_CLOEXEC_H */
