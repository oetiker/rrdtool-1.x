#include "rrd_nan_inf.h"

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)

#include <math.h>

double set_to_DNAN(void) { return (double)fmod(0.0,0.0); }
double set_to_DINF(void) { return (double)fabs((double)log(0.0)); }

#else

double set_to_DNAN(void) { return (double)(0.0/0.0); }
double set_to_DINF(void) { return (double)(1.0/0.0); }

#endif
