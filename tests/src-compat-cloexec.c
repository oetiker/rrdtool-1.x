/* Builds src/compat-cloexec.c into a test program. Including it, rather than listing
 * ${top_srcdir}/src/compat-cloexec.c as a source, keeps automake from placing the
 * object in another directory (see subdir-objects). */
#include "compat-cloexec.c"
