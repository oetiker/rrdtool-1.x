/* Exercise the real RADOS branch of rrd_open without a Ceph cluster. */
#include "rrd_rados.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int object_fd, stat_fail, reads;

int rados_stat(rados_ioctx_t io, const char *oid, uint64_t *size, time_t *mtime)
{
    struct stat st;
    (void) io;
    (void) oid;
    if (stat_fail)
        return -EACCES;
    if (fstat(object_fd, &st))
        return -errno;
    *size = st.st_size;
    *mtime = st.st_mtime;
    return 0;
}

rrd_rados_t *rrd_rados_open(const char *oid)
{
    rrd_rados_t *r = calloc(1, sizeof(*r));
    if (r == NULL)
        return NULL;
    object_fd = open(oid, O_RDWR);
    if (object_fd < 0) {
        free(r);
        return NULL;
    }
    r->oid = oid;
    return r;
}

int rrd_rados_close(rrd_rados_t *r)
{
    int status = close(object_fd);
    free(r);
    return status;
}

int rrd_rados_flush(rrd_rados_t *r)
{
    (void) r;
    return 0;
}

int rrd_rados_lock(rrd_rados_t *r)
{
    (void) r;
    return 0;
}

int rrd_rados_create(const char *oid, rrd_t *rrd)
{
    (void) oid;
    (void) rrd;
    return -1; /* The fixture is created through the local-file backend. */
}

size_t rrd_rados_read(rrd_rados_t *r, void *buf, size_t len, uint64_t off)
{
    (void) r;
    reads++;
    return pread(object_fd, buf, len, off);
}

size_t rrd_rados_write(rrd_rados_t *r, const void *buf, size_t len, uint64_t off)
{
    (void) r;
    return pwrite(object_fd, buf, len, off);
}

static int fail(const char *message)
{
    fprintf(stderr, "%s: %s\n", message, rrd_get_error());
    return 1;
}

int main(int argc, char **argv)
{
    rrd_t rrd;
    rrd_file_t *f;
    rrd_value_t value = 42.0, actual;
    char path[4096];
    int length;

    if (argc != 2)
        return 1;
    length = snprintf(path, sizeof(path), "ceph//%s", argv[1]);
    if (length < 0 || (size_t) length >= sizeof(path))
        return fail("fixture path is too long");
    rrd_init(&rrd);
    f = rrd_open(path, &rrd, RRD_READWRITE | RRD_LOCK_NONE);
    if (!f)
        return fail("could not open mocked RADOS object");
    if (rrd_seek(f, f->header_len, SEEK_SET) ||
        rrd_write(f, &value, sizeof(value)) != sizeof(value))
        return fail("could not update mocked RADOS object");
    if (rrd_seek(f, f->header_len, SEEK_SET) ||
        rrd_read(f, &actual, sizeof(actual)) != sizeof(actual) || actual != value)
        return fail("RADOS read did not return the updated value");
    if (rrd_close(f))
        return fail("could not close mocked RADOS object");
    rrd_free(&rrd);

    stat_fail = 1;
    reads = 0;
    rrd_init(&rrd);
    rrd_clear_error();
    f = rrd_open(path, &rrd, RRD_READONLY | RRD_LOCK_NONE);
    if (f || reads || !strstr(rrd_get_error(), "could not stat RADOS"))
        return fail("stat failure did not prevent header reads");
    rrd_free(&rrd);

    stat_fail = 0;
    reads = 0;
    /* LP64: the RRA starts at 248 and needs another 120 bytes. */
    if (truncate(argv[1], 350))
        return fail("could not truncate the fixture");
    rrd_init(&rrd);
    rrd_clear_error();
    f = rrd_open(path, &rrd, RRD_READONLY | RRD_LOCK_NONE);
    if (f || reads != 2 || !strstr(rrd_get_error(), "reached EOF"))
        return fail("truncated RRA was read instead of rejected");
    rrd_free(&rrd);
    return 0;
}
