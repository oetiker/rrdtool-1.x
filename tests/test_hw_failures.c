/* Exercise the real smoother's cleanup with failed allocator and I/O calls. */
#include "rrd_tool.h"
#include <errno.h>
#include <limits.h>

static int allocation, fail_at, live, short_read;

static void *test_malloc(size_t size)
{
    void *ptr;
    if (++allocation == fail_at)
        return NULL;
    ptr = malloc(size);
    if (ptr)
        live++;
    return ptr;
}

static void *test_calloc(size_t count, size_t size)
{
    void *ptr;
    if (++allocation == fail_at)
        return NULL;
    ptr = calloc(count, size);
    if (ptr)
        live++;
    return ptr;
}

static void test_free(void *ptr)
{
    if (ptr)
        live--;
    free(ptr);
}

static int test_seek(rrd_file_t *file, off_t offset, int whence)
{
    (void) file;
    (void) offset;
    (void) whence;
    return 0;
}

static ssize_t test_read(rrd_file_t *file, void *buffer, size_t count)
{
    (void) file;
    if (short_read || count != sizeof(rrd_value_t)) {
        errno = EIO;
        return -1;
    }
    *(rrd_value_t *) buffer = 1.0;
    return (ssize_t) count;
}

#define malloc test_malloc
#define calloc test_calloc
#define free test_free
#define rrd_seek test_seek
#define rrd_read test_read
#include "../src/rrd_hw.c"
#undef malloc
#undef calloc
#undef free
#undef rrd_seek
#undef rrd_read

int main(void)
{
    rrd_t rrd;
    stat_head_t header = {0};
    rra_def_t archive = {0};
    int status;

    rrd_init(&rrd);
    rrd.stat_head = &header;
    rrd.rra_def = &archive;
    strcpy(header.version, "0004");
    header.ds_cnt = 2;
    archive.row_cnt = ULONG_MAX;
    archive.par[RRA_seasonal_smoothing_window].u_val = 1.0;
    status = apply_smoother(&rrd, 0, 0, NULL);
    if (status != -1 || !strstr(rrd_get_error(), "too long to smooth"))
        return 1;
    archive.row_cnt = 10;
    archive.par[RRA_seasonal_smoothing_window].u_val = 0.5;
    short_read = 1;
    rrd_clear_error();
    status = apply_smoother(&rrd, 0, 0, NULL);
    if (status != -1 || live || !strstr(rrd_get_error(), "reading value failed"))
        return 1;
    short_read = 0;
    /* Values, four workspace arrays, then two allocations per DS queue.
     * Failing the second queue also checks cleanup of the first queue. */
    for (fail_at = 1; fail_at <= 9; fail_at++) {
        allocation = 0;
        rrd_clear_error();
        status = apply_smoother(&rrd, 0, 0, NULL);
        if (status != -1 || live ||
            !strstr(rrd_get_error(), "memory allocation failure")) {
            fprintf(stderr, "smoother allocation %d: status %d, live %d: %s\n",
                    fail_at, status, live, rrd_get_error());
            return 1;
        }
    }
    return 0;
}
