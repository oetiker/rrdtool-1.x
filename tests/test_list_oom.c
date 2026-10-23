/*
 * Helper for the test list-recursion-errors.
 *
 * Usage: list-oom DIR LIMIT
 *
 * Lists DIR recursively with rrd_list_r() while every allocation larger
 * than LIMIT bytes fails, so that only the listing of a directory with a
 * long output runs out of memory.  Prints the list, or the error.
 * Exit code: 0 the list was returned, 1 rrd_list_r failed with ENOMEM,
 * 2 any other failure, 77 when allocations cannot be made to fail here.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rrd.h"

#ifdef __GLIBC__
extern void *__libc_malloc(size_t size);
extern void *__libc_realloc(void *ptr, size_t size);

static size_t alloc_limit = 0;  /* 0: no limit */

void     *malloc(
    size_t size)
{
    if (alloc_limit && size > alloc_limit) {
        errno = ENOMEM;
        return NULL;
    }
    return __libc_malloc(size);
}

void     *realloc(
    void *ptr,
    size_t size)
{
    if (alloc_limit && size > alloc_limit) {
        errno = ENOMEM;
        return NULL;
    }
    return __libc_realloc(ptr, size);
}
#endif

int main(
    int argc,
    char **argv)
{
    char     *list;
    int       err;

    if (argc != 3) {
        fprintf(stderr, "usage: %s DIR LIMIT\n", argv[0]);
        return 2;
    }
#ifdef __GLIBC__
    alloc_limit = strtoul(argv[2], NULL, 10);
#else
    fprintf(stderr, "cannot make allocations fail without glibc\n");
    return 77;
#endif
    errno = 0;
    list = rrd_list_r(1, argv[1]);
    err = errno;
#ifdef __GLIBC__
    alloc_limit = 0;
#endif
    if (list != NULL) {
        fputs(list, stdout);
        free(list);
        return 0;
    }
    printf("error: %s\n", strerror(err));
    return err == ENOMEM ? 1 : 2;
}
