#include "rrd_nan_inf.h"

#if defined(WIN32)

double set_to_DNAN(void) { return (double)fmod(0.0,0.0); }
double set_to_DINF(void) { return (double)log(0.0); }

#else

double set_to_DNAN(void) { return (double)(0.0/0.0); }
double set_to_DINF(void) { return (double)(1.0/0.0); }

#endif
