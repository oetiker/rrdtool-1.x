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
#include <assert.h>

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

/* rrd_add_ptr: add a pointer to a dynamically sized array of pointers,
 * realloc as necessary.  returns 1 on success, 0 on failure.
 */

int rrd_add_ptr(void ***dest, size_t *dest_size, void *src)
{
    void **temp;

    assert(dest != NULL);

    temp = (void **) rrd_realloc(*dest, (*dest_size+1) * sizeof(*dest));
    if (!temp)
        return 0;

    *dest = temp;
    temp[*dest_size] = src;
    (*dest_size)++;

    return 1;
}

/* like rrd_add_ptr, but calls strdup() on a string first. */
int rrd_add_strdup(char ***dest, size_t *dest_size, char *src)
{
    char *dup_src;
    int add_ok;

    assert(dest != NULL);
    assert(src  != NULL);

    dup_src = strdup(src);
    if (!dup_src)
        return 0;

    add_ok = rrd_add_ptr((void ***)dest, dest_size, (void *)dup_src);
    if (!add_ok)
        free(dup_src);

    return add_ok;
}

void rrd_free_ptrs(void ***src, size_t *cnt)
{
    void **sp;

    assert(src != NULL);
    sp = *src;

    if (sp == NULL)
        return;

    while (*cnt > 0) {
        (*cnt)--;
        free(sp[*cnt]);
    }

    free (sp);
    *src = NULL;
}
