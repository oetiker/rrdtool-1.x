/* Builds src/rrd_open.c into a test program. Including it, rather than listing
 * ${top_srcdir}/src/rrd_open.c as a source, keeps automake from placing the
 * object in another directory (see subdir-objects). */
#include "rrd_open.c"
