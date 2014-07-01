#ifndef RRD_SNPRINTF_H
#define RRD_SNPRINTF_H

#ifdef  __cplusplus
extern    "C" {
#endif

#include <stdarg.h>

int rrd_vsnprintf(char *, size_t, const char *, va_list);

int rrd_snprintf(char *, size_t, const char *, ...);

int rrd_vasprintf(char **, const char *, va_list);

int rrd_asprintf(char **, const char *, ...);

#ifdef  __cplusplus
}
#endif


#endif	/* RRD_SNPRINTF_H */
