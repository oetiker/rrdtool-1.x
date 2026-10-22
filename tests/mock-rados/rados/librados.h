#ifndef TEST_LIBRADOS_H
#define TEST_LIBRADOS_H
#include <stdint.h>
#include <time.h>
typedef void *rados_t;
typedef void *rados_ioctx_t;
typedef void *rados_write_op_t;
int rados_stat(rados_ioctx_t, const char *, uint64_t *, time_t *);
#endif
