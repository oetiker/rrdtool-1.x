#ifndef RRD_SNPRINTF_H
#define RRD_SNPRINTF_H

#include "rrd_config.h"

#ifdef  __cplusplus
extern    "C" {
#endif

#include <stdarg.h>

int rrd_vsnprintf(char *, size_t, const char *, va_list);

int rrd_snprintf(char *, size_t, const char *, ...);

int rrd_vasprintf(char **, const char *, va_list);
#ifndef HAVE_VASPRINTF
#define vasprintf rrd_vasprintf
#endif

int rrd_asprintf(char **, const char *, ...);
#ifndef HAVE_ASPRINTF
#define asprintf rrd_asprintf
#endif

#ifdef  __cplusplus
}
#endif


#endif	/* RRD_SNPRINTF_H */
