/**
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 **/

#include "rrd_tool.h"

#include <stdlib.h>

#ifdef WIN32
#	define random() rand()
#	define srandom(x) srand(x)
#	define getpid() 0
#endif /* WIN32 */

/* make sure that the random number generator seeded EXACTLY ONCE */
long rrd_random(void)
{
    static int rand_init = 0;
    if (!rand_init) {
        srandom((unsigned int) time(NULL) + (unsigned int) getpid());
        rand_init++;
    }

    return random();
}
