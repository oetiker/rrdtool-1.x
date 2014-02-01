/*
**  STRFTIME.H - For older compilers which lack strftime()
**
**  Note: To avoid name collision with newer compilers, the function name
**	    strftime_() is used.
*/

#ifndef STRFTIME_H_14FA27AFAACA4FB5820D2B15278D9B96
#define STRFTIME_H_14FA27AFAACA4FB5820D2B15278D9B96

#include <stddef.h>     /* for size_t */
#include <time.h>       /* for struct tm */

size_t    strftime_(
    char *s,
    size_t maxs,
    const char *f,
    const struct tm *t);

#if defined(TZNAME_STD) && defined(TZNAME_DST)
extern char *tzname_[2];
#endif

#endif                          /* STRFTIME__H */
