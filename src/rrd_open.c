/*****************************************************************************
 * RRDtool 1.GIT, Copyright by Tobi Oetiker
 *****************************************************************************
 * rrd_open.c  Open an RRD File
 *****************************************************************************
 * $Id$
 *****************************************************************************/

#ifdef WIN32
#include <windows.h>
#if _WIN32_MAXVER >= 0x0602 /* _WIN32_WINNT_WIN8 */
#include <synchapi.h>
#endif

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#endif                          /* WIN32 */

#include "rrd_tool.h"
#include "unused.h"

#ifdef HAVE_BROKEN_MS_ASYNC
#include <sys/types.h>
#include <utime.h>
#endif

#ifdef HAVE_LIBRADOS
#include "rrd_rados.h"
#endif

#define MEMBLK 8192

#ifdef WIN32
#define	_LK_UNLCK	0   /* Unlock */
#define	_LK_LOCK	1   /* Lock */
#define	_LK_NBLCK	2   /* Non-blocking lock */
#define	_LK_RLCK	3   /* "Same as _LK_NBLCK" */
#define	_LK_NBRLCK	4   /* "Same as _LK_LOCK" */


#define	LK_UNLCK	_LK_UNLCK
#define	LK_LOCK		_LK_LOCK
#define	LK_NBLCK	_LK_NBLCK
#define	LK_RLCK		_LK_RLCK
#define	LK_NBRLCK	_LK_NBRLCK
#endif

/* DEBUG 2 prints information obtained via mincore(2) */
// #define DEBUG 1
/* do not calculate exact madvise hints but assume 1 page for headers and
 * set DONTNEED for the rest, which is assumed to be data */
/* Avoid calling madvise on areas that were already hinted. May be beneficial if
 * your syscalls are very slow */

#ifdef HAVE_MMAP
/* the cast to void* is there to avoid this warning seen on ia64 with certain
   versions of gcc: 'cast increases required alignment of target type'
*/
#define __rrd_read_mmap(dst, dst_t, cnt) { \
	size_t wanted = sizeof(dst_t)*(cnt); \
	if (offset + wanted > rrd_file->file_len) { \
		rrd_set_error("reached EOF while loading header " #dst); \
		goto out_close; \
	} \
	(dst) = (dst_t*)(void*) (data + offset); \
	offset += wanted; \
    }
#else
#define __rrd_read_seq(dst, dst_t, cnt) { \
	size_t wanted = sizeof(dst_t)*(cnt); \
        size_t got; \
	if ((dst = (dst_t*)malloc(wanted)) == NULL) { \
		rrd_set_error(#dst " malloc"); \
		goto out_close; \
	} \
        got = read (rrd_simple_file->fd, dst, wanted); \
	if (got != wanted) { \
		rrd_set_error("short read while reading header " #dst); \
                goto out_close; \
	} \
	offset += got; \
    }
#endif

#ifdef HAVE_LIBRADOS
#define __rrd_read_rados(dst, dst_t, cnt) { \
	size_t wanted = sizeof(dst_t)*(cnt); \
        size_t got; \
	if ((dst = (dst_t*)malloc(wanted)) == NULL) { \
		rrd_set_error(#dst " malloc"); \
		goto out_close; \
	} \
        got = rrd_rados_read(rrd_file->rados, dst, wanted, offset); \
	if (got != wanted) { \
		rrd_set_error("short read while reading header " #dst); \
                goto out_close; \
	} \
	offset += got; \
    }
#endif

#if defined(HAVE_LIBRADOS) && defined(HAVE_MMAP)
#define __rrd_read(dst, dst_t, cnt) { \
    if (rrd_file->rados) \
      __rrd_read_rados(dst, dst_t, cnt) \
    else \
      __rrd_read_mmap(dst, dst_t, cnt) \
    }
#elif defined(HAVE_LIBRADOS) && !defined(HAVE_MMAP)
if (rrd_file->rados)
    __rrd_read_rados(dst, dst_t, cnt)
        else
    __rrd_read_seq(dst, dst_t, cnt)
    }
#elif defined(HAVE_MMAP)
#define __rrd_read(dst, dst_t, cnt) \
    __rrd_read_mmap(dst, dst_t, cnt)
#else
#define __rrd_read(dst, dst_t, cnt) \
    __rrd_read_seq(dst, dst_t, cnt)
#endif

/* get the address of the start of this page */
#if defined USE_MADVISE || defined HAVE_POSIX_FADVISE
#ifndef PAGE_START
#define PAGE_START(addr) ((addr)&(~(_page_size-1)))
#endif
#endif

static int rrd_rwlock(
    rrd_file_t *rrd_file,
    int writelock);
static int close_and_unlock(
    int fd);

/* Open a database file, return its header and an open filehandle,
 * positioned to the first cdp in the first rra.
 * In the error path of rrd_open, only rrd_free(&rrd) has to be called
 * before returning an error. Do not call rrd_close upon failure of rrd_open.
 * If creating a new file, the parameter rrd must be initialized with
 * details of the file content.
 * If opening an existing file, then use rrd must be initialized by
 * rrd_init(rrd) prior to invoking rrd_open
 */

rrd_file_t *rrd_open(
    const char *const file_name,
    rrd_t *rrd,
    unsigned rdwr)
{
    unsigned long ui;
    int       flags = 0;
    int       version;

#ifdef HAVE_MMAP
    char     *data = MAP_FAILED;
#endif
    off_t     offset = 0;
    struct stat statb;
    rrd_file_t *rrd_file = NULL;
    rrd_simple_file_t *rrd_simple_file = NULL;
    size_t    newfile_size = 0;

    /* Are we creating a new file? */
    if (rdwr & RRD_CREAT) {
        size_t    header_len, value_cnt, data_len;

        header_len = rrd_get_header_size(rrd);

        value_cnt = 0;
        for (ui = 0; ui < rrd->stat_head->rra_cnt; ui++)
            value_cnt += rrd->stat_head->ds_cnt * rrd->rra_def[ui].row_cnt;

        data_len = sizeof(rrd_value_t) * value_cnt;

        newfile_size = header_len + data_len;
    }

    rrd_file = (rrd_file_t *) malloc(sizeof(rrd_file_t));
    if (rrd_file == NULL) {
        rrd_set_error("allocating rrd_file descriptor for '%s'", file_name);
        return NULL;
    }
    memset(rrd_file, 0, sizeof(rrd_file_t));
    rrd_file->rrd = rrd;

    rrd_file->pvt = malloc(sizeof(rrd_simple_file_t));
    if (rrd_file->pvt == NULL) {
        rrd_set_error("allocating rrd_simple_file for '%s'", file_name);
        free(rrd_file);
        return NULL;
    }
    memset(rrd_file->pvt, 0, sizeof(rrd_simple_file_t));
    rrd_simple_file = (rrd_simple_file_t *) rrd_file->pvt;
    rrd_simple_file->fd = -1;

#ifdef DEBUG
    if ((rdwr & (RRD_READONLY | RRD_READWRITE)) ==
        (RRD_READONLY | RRD_READWRITE)) {
        /* Both READONLY and READWRITE were given, which is invalid.  */
        rrd_set_error("in read/write request mask");
        free(rrd_file);
        return NULL;
    }
#endif

#ifdef HAVE_LIBRADOS
    if (strncmp("ceph//", file_name, 6) == 0) {
        rrd_file->rados = rrd_rados_open(file_name + 6);
        if (rrd_file->rados == NULL)
            goto out_free;

        if (rdwr & RRD_LOCK) {
            /* Note: rados read lock is not implemented.  See rrd_lock(). */
            if (rrd_rwlock(rrd_file, rdwr & RRD_READWRITE) != 0) {
                rrd_set_error("could not lock RRD");
                goto out_close;
            }
        }

        if (rdwr & RRD_CREAT)
            goto out_done;

        goto read_check;
    }
#endif

#ifdef HAVE_MMAP
    rrd_simple_file->mm_prot = PROT_READ;
    rrd_simple_file->mm_flags = 0;
#endif

    if (rdwr & RRD_READONLY) {
        flags |= O_RDONLY;
#ifdef HAVE_MMAP
# if !defined(AIX)
        rrd_simple_file->mm_flags = MAP_PRIVATE;
# endif
# ifdef MAP_NORESERVE
        rrd_simple_file->mm_flags |= MAP_NORESERVE; /* readonly, so no swap backing needed */
# endif
#endif
    } else {
        if (rdwr & RRD_READWRITE) {
            flags |= O_RDWR;
#ifdef HAVE_MMAP
            rrd_simple_file->mm_flags = MAP_SHARED;
            rrd_simple_file->mm_prot |= PROT_WRITE;
#endif
        }
        if (rdwr & RRD_CREAT) {
            flags |= (O_CREAT | O_TRUNC);
        }
        if (rdwr & RRD_EXCL) {
            flags |= O_EXCL;
        }
    }
    if (rdwr & RRD_READAHEAD) {
#ifdef MAP_POPULATE
        rrd_simple_file->mm_flags |= MAP_POPULATE;  /* populate ptes and data */
#endif
#if defined MAP_NONBLOCK
        rrd_simple_file->mm_flags |= MAP_NONBLOCK;  /* just populate ptes */
#endif
    }
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)
    flags |= O_BINARY;
#endif

    if ((rrd_simple_file->fd = open(file_name, flags, 0666)) < 0) {
        rrd_set_error("opening '%s': %s", file_name, rrd_strerror(errno));
        goto out_free;
    }

#ifdef HAVE_MMAP
#ifdef HAVE_BROKEN_MS_ASYNC
    if (rdwr & RRD_READWRITE) {
        /* some unices, the files mtime does not get updated
           on memory mapped files, in order to help them,     
           we update the timestamp at this point.      
           The thing happens pretty 'close' to the open    
           call so the chances of a race should be minimal.    

           Maybe ask your vendor to fix your OS ... */
        utime(file_name, NULL);
    }
#endif
#endif

    if (rdwr & RRD_LOCK) {
        if (rrd_rwlock(rrd_file, rdwr & RRD_READWRITE) != 0) {
            rrd_set_error("could not lock RRD");
            goto out_close;
        }
    }

    /* Better try to avoid seeks as much as possible. stat may be heavy but
     * many concurrent seeks are even worse.  */
    if (newfile_size == 0 && ((fstat(rrd_simple_file->fd, &statb)) < 0)) {
        rrd_set_error("fstat '%s': %s", file_name, rrd_strerror(errno));
        goto out_close;
    }
    if (newfile_size == 0) {
        rrd_file->file_len = statb.st_size;
    } else {
        rrd_file->file_len = newfile_size;
#ifdef HAVE_POSIX_FALLOCATE
        /* man: posix_fallocate() returns zero on success,
         * or an error number on failure.  Note that errno is not set.
         */
        int       fret =
            posix_fallocate(rrd_simple_file->fd, 0, newfile_size);
        if (fret) {
            rrd_set_error("posix_fallocate '%s': %s", file_name,
                          rrd_strerror(fret));
            goto out_close;
        } else {
            goto no_lseek_necessary;
        }
#endif
        if (lseek(rrd_simple_file->fd, newfile_size - 1, SEEK_SET) == -1) {
            rrd_set_error("lseek '%s': %s", file_name, rrd_strerror(errno));
            goto out_close;
        }
        if (write(rrd_simple_file->fd, "\0", 1) == -1) {    /* poke */
            rrd_set_error("write '%s': %s", file_name, rrd_strerror(errno));
            goto out_close;
        }
        if (lseek(rrd_simple_file->fd, 0, SEEK_SET) == -1) {
            rrd_set_error("lseek '%s': %s", file_name, rrd_strerror(errno));
            goto out_close;
        }
    }
  no_lseek_necessary:

#ifdef HAVE_MMAP
#ifndef HAVE_POSIX_FALLOCATE
    /* force allocating the file on the underlying filesystem to prevent any
     * future bus error when the filesystem is full and attempting to write
     * trough the file mapping. Filling the file using memset on the file
     * mapping can also lead some bus error, so we use the old fashioned
     * write().
     */
    if (rdwr & RRD_CREAT) {
        char      buf[4096];
        unsigned  i;

        memset(buf, DNAN, sizeof buf);
        lseek(rrd_simple_file->fd, offset, SEEK_SET);

        for (i = 0; i < (newfile_size - 1) / sizeof buf; ++i) {
            if (write(rrd_simple_file->fd, buf, sizeof buf) == -1) {
                rrd_set_error("write '%s': %s", file_name,
                              rrd_strerror(errno));
                goto out_close;
            }
        }

        if (write(rrd_simple_file->fd, buf,
                  (newfile_size - 1) % sizeof buf) == -1) {
            rrd_set_error("write '%s': %s", file_name, rrd_strerror(errno));
            goto out_close;
        }

        lseek(rrd_simple_file->fd, 0, SEEK_SET);
    }
#endif

    data = mmap(0, rrd_file->file_len,
                rrd_simple_file->mm_prot, rrd_simple_file->mm_flags,
                rrd_simple_file->fd, offset);

    /* lets see if the first read worked */
    if (data == MAP_FAILED) {
        rrd_set_error("mmaping file '%s': %s", file_name,
                      rrd_strerror(errno));
        goto out_close;
    }
    rrd->__mmap_start = data;
    rrd->__mmap_size = rrd_file->file_len;

    rrd_simple_file->file_start = data;
#endif
    if (rdwr & RRD_CREAT)
        goto out_done;

    if (rdwr & RRD_READAHEAD) {
        /* If perfect READAHEAD is not achieved for whatever reason, caller
           will not thank us for advising the kernel of RANDOM access below. */
        rdwr |= RRD_COPY;
    }
    /* In general we need no read-ahead when dealing with rrd_files.
       When we stop reading, it is highly unlikely that we start up again.
       In this manner we actually save time and disk access (and buffer cache).
       Thanks to Dave Plonka for the Idea of using POSIX_FADV_RANDOM here. */
#ifdef USE_MADVISE
    if (rdwr & RRD_COPY) {
        /* We will read everything in a moment (copying) */
        madvise(data, rrd_file->file_len, MADV_SEQUENTIAL);
    } else {
        /* We do not need to read anything in for the moment */
        madvise(data, rrd_file->file_len, MADV_RANDOM);
    }
#endif
#if !defined(HAVE_MMAP) && defined(HAVE_POSIX_FADVISE)
    if (rdwr & RRD_COPY) {
        posix_fadvise(rrd_simple_file->fd, 0, 0, POSIX_FADV_SEQUENTIAL);
    } else {
        posix_fadvise(rrd_simple_file->fd, 0, 0, POSIX_FADV_RANDOM);
    }
#endif

#ifdef HAVE_LIBRADOS
  read_check:
#endif

    __rrd_read(rrd->stat_head, stat_head_t,
               1);

    /* lets do some test if we are on track ... */
    if (memcmp(rrd->stat_head->cookie, RRD_COOKIE, sizeof(RRD_COOKIE)) != 0) {
        rrd_set_error("'%s' is not an RRD file", file_name);
        goto out_close;
    }

    if (rrd->stat_head->float_cookie != FLOAT_COOKIE) {
        rrd_set_error("This RRD was created on another architecture");
        goto out_close;
    }

    version = atoi(rrd->stat_head->version);

    if (version > atoi(RRD_VERSION5)) {
        rrd_set_error("can't handle RRD file version %s",
                      rrd->stat_head->version);
        goto out_close;
    }
    __rrd_read(rrd->ds_def, ds_def_t,
               rrd->stat_head->ds_cnt);

    __rrd_read(rrd->rra_def, rra_def_t,
               rrd->stat_head->rra_cnt);

    /* handle different format for the live_head */
    if (version < 3) {
        rrd->live_head = (live_head_t *) malloc(sizeof(live_head_t));
        if (rrd->live_head == NULL) {
            rrd_set_error("live_head_t malloc");
            goto out_close;
        }
        __rrd_read(rrd->legacy_last_up, time_t,
                   1);

        rrd->live_head->last_up = *rrd->legacy_last_up;
        rrd->live_head->last_up_usec = 0;
    } else {
        __rrd_read(rrd->live_head, live_head_t,
                   1);
    }
    __rrd_read(rrd->pdp_prep, pdp_prep_t,
               rrd->stat_head->ds_cnt);
    __rrd_read(rrd->cdp_prep, cdp_prep_t,
               rrd->stat_head->rra_cnt * rrd->stat_head->ds_cnt);
    __rrd_read(rrd->rra_ptr, rra_ptr_t,
               rrd->stat_head->rra_cnt);

    rrd_file->header_len = offset;
    rrd_file->pos = offset;

#if defined(HAVE_MMAP) && defined(USE_MADVISE)
    if (data != MAP_FAILED) {
        /* MADV_SEQUENTIAL mentions drop-behind.  Override it for the header
         * now we've read it, in case anyone implemented drop-behind.
         *
         * Do *not* fall back to fadvise() for !HAVE_MMAP.  In that case,
         * we've copied the header and will not read it again.  Doing e.g.
         * FADV_NORMAL on Linux (4.12) on *any* region would negate the
         * effect of previous FADV_SEQUENTIAL.
         */
        madvise(data, sysconf(_SC_PAGESIZE), MADV_NORMAL);
        madvise(data, sysconf(_SC_PAGESIZE), MADV_WILLNEED);
    }
#endif

    {
        unsigned long row_cnt = 0;

        for (ui = 0; ui < rrd->stat_head->rra_cnt; ui++)
            row_cnt += rrd->rra_def[ui].row_cnt;

        size_t    correct_len = rrd_file->header_len +
            sizeof(rrd_value_t) * row_cnt * rrd->stat_head->ds_cnt;

#ifdef HAVE_LIBRADOS
        /* skip length checking for rados file */
        if (rrd_file->rados) {
            rrd_file->file_len = correct_len;
        }
#endif

        if (correct_len > rrd_file->file_len) {
            rrd_set_error("'%s' is too small (should be %ld bytes)",
                          file_name, (long long) correct_len);
            goto out_close;
        }
        if (rdwr & RRD_READVALUES) {
            __rrd_read(rrd->rrd_value, rrd_value_t,
                       row_cnt * rrd->stat_head->ds_cnt);

            if (rrd_seek(rrd_file, rrd_file->header_len, SEEK_SET) != 0)
                goto out_close;
        }
    }

  out_done:
    return (rrd_file);

  out_close:
#ifdef HAVE_MMAP
    if (data != MAP_FAILED)
        munmap(data, rrd_file->file_len);
#endif
#ifdef HAVE_LIBRADOS
    if (rrd_file->rados)
        rrd_rados_close(rrd_file->rados);
#endif
    if (rrd_simple_file->fd >= 0) {
        /* keep the original error */
        char     *e = strdup(rrd_get_error());

        close_and_unlock(rrd_simple_file->fd);

        if (e) {
            rrd_set_error(e);
            free(e);
        } else
            rrd_set_error("error message was lost (out of memory)");
    }
  out_free:
    free(rrd_file->pvt);
    free(rrd_file);
    return NULL;
}


#if defined DEBUG && DEBUG > 1
/* Print list of in-core pages of a the current rrd_file.  */
static
void mincore_print(
    rrd_file_t *rrd_file,
    char *mark)
{
    rrd_simple_file_t *rrd_simple_file;

    rrd_simple_file = (rrd_simple_file_t *) rrd_file->pvt;
#ifdef HAVE_MMAP
    /* pretty print blocks in core */
    size_t    off;
    unsigned char *vec;
    ssize_t   _page_size = sysconf(_SC_PAGESIZE);

    off = rrd_file->file_len +
        ((rrd_file->file_len + _page_size - 1) / _page_size);
    vec = malloc(off);
    if (vec != NULL) {
        memset(vec, 0, off);
        if (mincore(rrd_simple_file->file_start, rrd_file->file_len, vec) ==
            0) {
            int       prev;
            unsigned  is_in = 0, was_in = 0;

            for (off = 0, prev = 0; off < rrd_file->file_len; ++off) {
                is_in = vec[off] & 1;   /* if lsb set then is core resident */
                if (off == 0)
                    was_in = is_in;
                if (was_in != is_in) {
                    fprintf(stderr, "%s: %sin core: %p len %ld\n", mark,
                            was_in ? "" : "not ", vec + prev, off - prev);
                    was_in = is_in;
                    prev = off;
                }
            }
            fprintf(stderr,
                    "%s: %sin core: %p len %ld\n", mark,
                    was_in ? "" : "not ", vec + prev, off - prev);
        } else
            fprintf(stderr, "mincore: %s", rrd_strerror(errno));
    }
#else
    fprintf(stderr, "sorry mincore only works with mmap");
#endif
}
#endif                          /* defined DEBUG && DEBUG > 1 */

/*
 * get exclusive lock to whole file.
 * lock gets removed when we close the file
 *
 * returns 0 on success
 */
int rrd_lock(
    rrd_file_t *rrd_file)
{
    return rrd_rwlock(rrd_file, 1);
}

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)
#define USE_WINDOWS_LOCK 1
#endif

#ifdef USE_WINDOWS_LOCK
static
int rrd_windows_lock(
    int fd)
{
    int       ret;
    long      pos;

    /*
     * _locking() is relative to fd position.
     * We need to consistently lock bytes starting from 0,
     * so we can successfully unlock on close.
     *
     * Note rrd_lock() API doesn't set a specific error message.
     * Knowing that rrd_lock() (or even rrd_open()) failed should
     * be specific enough, if someone manages to invoke rrdtool
     * on something silly like a named pipe or COM1.
     */
    pos = tell(fd);
    if (pos < 0)
        return -1;

    if (lseek(fd, 0, SEEK_SET) < 0)
        return -1;

    while (1) {
        ret = _locking(fd, _LK_NBLCK, LONG_MAX);
        if (ret == 0)
            break;      /* success */
        if (errno != EACCES)
            break;      /* failure */
        /* EACCES: someone else has the lock. */

        /*
         * Wait 0.01 seconds before trying again.  _locking()
         * with _LK_LOCK would work similarly but waits 1 second
         * between tries, which seems less desirable.
         */
        Sleep(10);
    }

    /* restore saved fd position */
    if (lseek(fd, pos, SEEK_SET) < 0)
        return -1;

    return ret;
}
#endif

static
int close_and_unlock(
    int fd)
{
    int       ret = 0;

#ifdef USE_WINDOWS_LOCK
    /*
     * "If a process closes a file that has outstanding locks, the locks are
     *  unlocked by the operating system. However, the time it takes for the
     *  operating system to unlock these locks depends upon available system
     *  resources. Therefore, it is recommended that your process explicitly
     *  unlock all files it has locked when it terminates."  (?!)
     */

    if (lseek(fd, 0, SEEK_SET) < 0) {
        rrd_set_error("lseek: %s", rrd_strerror(errno));
        ret = -1;
        goto out_close;
    }

    ret = _locking(fd, LK_UNLCK, LONG_MAX);
    if (ret != 0 && errno == EACCES)
        /* fd was not locked - this is entirely possible, ignore the error */
        ret = 0;

    if (ret != 0)
        rrd_set_error("unlock file: %s", rrd_strerror(errno));
  out_close:
#endif

    if (close(fd) != 0) {
        ret = -1;
        rrd_set_error("closing file: %s", rrd_strerror(errno));
    }

    return ret;
}

static
int rrd_rwlock(
    rrd_file_t *rrd_file,
    int writelock)
{
#ifdef DISABLE_FLOCK
    (void) rrd_file;
    return 0;
#else
#ifdef HAVE_LIBRADOS
    if (rrd_file->rados) {
        /*
         * No read lock on rados.  It would be complicated by the
         * use of a short finite lock duration in rrd_rados_lock().
         * Also rados does not provide blocking locks.
         *
         * Rados users may use snapshots if they need to
         * e.g. obtain a consistent backup.
         */
        if (writelock)
            return rrd_rados_lock(rrd_file->rados);
        else
            return 0;
    }
#endif
    int       rcstat;
    rrd_simple_file_t *rrd_simple_file;

    rrd_simple_file = (rrd_simple_file_t *) rrd_file->pvt;
#ifdef USE_WINDOWS_LOCK
    /* _locking() does not support read locks; we always take a write lock */
    rcstat = rrd_windows_lock(rrd_simple_file->fd);
#else
    {
        struct flock lock;

        lock.l_type = writelock ? F_WRLCK : /* exclusive write lock or */
            F_RDLCK;    /* shared read lock */
        lock.l_len = 0; /* whole file */
        lock.l_start = 0;   /* start of file */
        lock.l_whence = SEEK_SET;   /* end of file */

        rcstat = fcntl(rrd_simple_file->fd, F_SETLK, &lock);
    }
#endif

    return (rcstat);
#endif
}


/* drop cache except for the header and the active pages */
void rrd_dontneed(
    rrd_file_t *rrd_file,
    rrd_t *rrd)
{
    rrd_simple_file_t *rrd_simple_file;

#if defined USE_MADVISE || defined HAVE_POSIX_FADVISE
    size_t    dontneed_start;
    size_t    rra_start;
    size_t    active_block;
    size_t    i;
    ssize_t   _page_size = sysconf(_SC_PAGESIZE);

    if (rrd_file == NULL) {
#if defined DEBUG && DEBUG
        fprintf(stderr, "rrd_dontneed: Argument 'rrd_file' is NULL.\n");
#endif
        return;
    }
    rrd_simple_file = (rrd_simple_file_t *) rrd_file->pvt;

#if defined DEBUG && DEBUG > 1
    mincore_print(rrd_file, "before");
#endif

    /* ignoring errors from RRDs that are smaller then the file_len+rounding */
    rra_start = rrd_file->header_len;
    dontneed_start = PAGE_START(rra_start) + _page_size;
    for (i = 0; i < rrd->stat_head->rra_cnt; ++i) {
        active_block =
            PAGE_START(rra_start
                       + rrd->rra_ptr[i].cur_row
                       * rrd->stat_head->ds_cnt * sizeof(rrd_value_t));
        if (active_block > dontneed_start) {
#ifdef USE_MADVISE
            madvise(rrd_simple_file->file_start + dontneed_start,
                    active_block - dontneed_start - 1, MADV_DONTNEED);
#else
#ifdef HAVE_POSIX_FADVISE
/* in linux at least only fadvise DONTNEED seems to purge pages from cache */
            posix_fadvise(rrd_simple_file->fd, dontneed_start,
                          active_block - dontneed_start - 1,
                          POSIX_FADV_DONTNEED);
#endif
#endif
        }
        dontneed_start = active_block;
        /* do not release 'hot' block if update for this RAA will occur
         * within 10 minutes */
        if (rrd->stat_head->pdp_step * rrd->rra_def[i].pdp_cnt -
            rrd->live_head->last_up % (rrd->stat_head->pdp_step *
                                       rrd->rra_def[i].pdp_cnt) < 10 * 60) {
            dontneed_start += _page_size;
        }
        rra_start +=
            rrd->rra_def[i].row_cnt * rrd->stat_head->ds_cnt *
            sizeof(rrd_value_t);
    }

    if (dontneed_start < rrd_file->file_len) {
#ifdef USE_MADVISE
        madvise(rrd_simple_file->file_start + dontneed_start,
                rrd_file->file_len - dontneed_start, MADV_DONTNEED);
#else
#ifdef HAVE_POSIX_FADVISE
        posix_fadvise(rrd_simple_file->fd, dontneed_start,
                      rrd_file->file_len - dontneed_start,
                      POSIX_FADV_DONTNEED);
#endif
#endif
    }

#if defined DEBUG && DEBUG > 1
    mincore_print(rrd_file, "after");
#endif
#endif                          /* without madvise and posix_fadvise it does not make much sense todo anything */
}





int rrd_close(
    rrd_file_t *rrd_file)
{
    rrd_simple_file_t *rrd_simple_file;

    rrd_simple_file = (rrd_simple_file_t *) rrd_file->pvt;
    int       ret = 0;

#ifdef HAVE_LIBRADOS
    if (rrd_file->rados) {
        if (rrd_rados_close(rrd_file->rados) != 0)
            ret = -1;
    }
#endif
#ifdef HAVE_MMAP
    if (rrd_simple_file->file_start != NULL) {
        if (munmap(rrd_simple_file->file_start, rrd_file->file_len) != 0) {
            ret = -1;
            rrd_set_error("munmap rrd_file: %s", rrd_strerror(errno));
        }
    }
#endif
    if (rrd_simple_file->fd >= 0) {
        if (close_and_unlock(rrd_simple_file->fd) != 0)
            ret = -1;
    }
    free(rrd_file->pvt);
    free(rrd_file);
    return ret;
}


/* Set position of rrd_file.  */

off_t rrd_seek(
    rrd_file_t *rrd_file,
    off_t off,
    int whence)
{
#ifdef HAVE_LIBRADOS
    /* no seek for rados */
    if (rrd_file->rados) {
        rrd_file->pos = off;
        return 0;
    }
#endif

    off_t     ret = 0;

#ifndef HAVE_MMAP
    rrd_simple_file_t *rrd_simple_file;

    rrd_simple_file = (rrd_simple_file_t *) rrd_file->pvt;
#endif

#ifdef HAVE_MMAP
    if (whence == SEEK_SET)
        rrd_file->pos = off;
    else if (whence == SEEK_CUR)
        rrd_file->pos += off;
    else if (whence == SEEK_END)
        rrd_file->pos = rrd_file->file_len + off;
#else
    ret = lseek(rrd_simple_file->fd, off, whence);
    if (ret < 0)
        rrd_set_error("lseek: %s", rrd_strerror(errno));
    rrd_file->pos = ret;
#endif
    /* mimic fseek, which returns 0 upon success */
    return ret < 0;     /*XXX: or just ret to mimic lseek */
}


/* Get current position in rrd_file.  */

off_t rrd_tell(
    rrd_file_t *rrd_file)
{
    return rrd_file->pos;
}


/* Read count bytes into buffer buf, starting at rrd_file->pos.
 * Returns the number of bytes read or <0 on error.  */

ssize_t rrd_read(
    rrd_file_t *rrd_file,
    void *buf,
    size_t count)
{
#ifdef HAVE_LIBRADOS
    if (rrd_file->rados) {
        ssize_t   ret =
            rrd_rados_read(rrd_file->rados, buf, count, rrd_file->pos);
        if (ret > 0)
            rrd_file->pos += ret;
        return ret;
    }
#endif
    rrd_simple_file_t *rrd_simple_file = (rrd_simple_file_t *) rrd_file->pvt;

#ifdef HAVE_MMAP
    size_t    _cnt = count;
    ssize_t   _surplus;

    if (rrd_file->pos > rrd_file->file_len || _cnt == 0)    /* EOF */
        return 0;
    if (buf == NULL)
        return -1;      /* EINVAL */
    _surplus = rrd_file->pos + _cnt - rrd_file->file_len;
    if (_surplus > 0) { /* short read */
        _cnt -= _surplus;
    }
    if (_cnt == 0)
        return 0;       /* EOF */
    buf = memcpy(buf, rrd_simple_file->file_start + rrd_file->pos, _cnt);

    rrd_file->pos += _cnt;  /* mimic read() semantics */
    return _cnt;
#else
    ssize_t   ret;

    ret = read(rrd_simple_file->fd, buf, count);
    if (ret > 0)
        rrd_file->pos += ret;   /* mimic read() semantics */
    return ret;
#endif
}


/* Write count bytes from buffer buf to the current position
 * rrd_file->pos of rrd_simple_file->fd.
 * Returns the number of bytes written or <0 on error.  */

ssize_t rrd_write(
    rrd_file_t *rrd_file,
    const void *buf,
    size_t count)
{
#ifdef HAVE_LIBRADOS
    if (rrd_file->rados) {
        size_t    ret =
            rrd_rados_write(rrd_file->rados, buf, count, rrd_file->pos);
        if (ret > 0)
            rrd_file->pos += count;
        return ret;
    }
#endif
    rrd_simple_file_t *rrd_simple_file = (rrd_simple_file_t *) rrd_file->pvt;

#ifdef HAVE_MMAP
    size_t    old_size = rrd_file->file_len;

    if (count == 0)
        return 0;
    if (buf == NULL)
        return -1;      /* EINVAL */

    if ((rrd_file->pos + count) > old_size) {
        rrd_set_error
            ("attempting to write beyond end of file (%ld + %ld > %ld)",
             rrd_file->pos, count, old_size);
        return -1;
    }
    /* can't use memcpy since the areas overlap when tuning */
    memmove(rrd_simple_file->file_start + rrd_file->pos, buf, count);
    rrd_file->pos += count;
    return count;       /* mimic write() semantics */
#else
    ssize_t   _sz = write(rrd_simple_file->fd, buf, count);

    if (_sz > 0)
        rrd_file->pos += _sz;
    return _sz;
#endif
}


/* this is a leftover from the old days, it serves no purpose
   and is therefore turned into a no-op */
void rrd_flush(
    rrd_file_t UNUSED(*rrd_file))
{
}

/* Initialize RRD header.  */

void rrd_init(
    rrd_t *rrd)
{
    rrd->stat_head = NULL;
    rrd->ds_def = NULL;
    rrd->rra_def = NULL;
    rrd->live_head = NULL;
    rrd->legacy_last_up = NULL;
    rrd->rra_ptr = NULL;
    rrd->pdp_prep = NULL;
    rrd->cdp_prep = NULL;
    rrd->rrd_value = NULL;
    rrd->__mmap_start = NULL;
    rrd->__mmap_size = 0;
}


/* free RRD data, act correctly, regardless of mmap'ped or malloc'd memory. */
static void free_rrd_ptr_if_not_mmapped(
    void *m,
    const rrd_t *rrd)
{
    if (m == NULL)
        return;

    if (rrd == NULL || rrd->__mmap_start == NULL) {
        free(m);
        return;
    }

    /* is this ALWAYS correct on all supported platforms ??? */
    long      ofs = (char *) m - (char *) rrd->__mmap_start;

    if (ofs < rrd->__mmap_size) {
        // DO NOT FREE, this memory is mmapped!!
        return;
    }

    free(m);
}

void rrd_free(
    rrd_t *rrd)
{
    if (rrd == NULL)
        return;

    free_rrd_ptr_if_not_mmapped(rrd->live_head, rrd);
    rrd->live_head = NULL;
    free_rrd_ptr_if_not_mmapped(rrd->stat_head, rrd);
    rrd->stat_head = NULL;
    free_rrd_ptr_if_not_mmapped(rrd->ds_def, rrd);
    rrd->ds_def = NULL;
    free_rrd_ptr_if_not_mmapped(rrd->rra_def, rrd);
    rrd->rra_def = NULL;
    free_rrd_ptr_if_not_mmapped(rrd->rra_ptr, rrd);
    rrd->rra_ptr = NULL;
    free_rrd_ptr_if_not_mmapped(rrd->pdp_prep, rrd);
    rrd->pdp_prep = NULL;
    free_rrd_ptr_if_not_mmapped(rrd->cdp_prep, rrd);
    rrd->cdp_prep = NULL;
    free_rrd_ptr_if_not_mmapped(rrd->rrd_value, rrd);
    rrd->rrd_value = NULL;
}

/* routine used by external libraries to free memory allocated by
 * rrd library */

void rrd_freemem(
    void *mem)
{
    free(mem);
}

/*
 * rra_update informs us about the RRAs being updated
 * The low level storage API may use this information for
 * aligning RRAs within stripes, or other performance enhancements
 */
void rrd_notify_row(
    rrd_file_t UNUSED(*rrd_file),
    int UNUSED(rra_idx),
    unsigned long UNUSED(rra_row),
    time_t UNUSED(rra_time))
{
}

/*
 * This function is called when creating a new RRD
 * The storage implementation can use this opportunity to select
 * a sensible starting row within the file.
 * The default implementation is random, to ensure that all RRAs
 * don't change to a new disk block at the same time
 */
unsigned long rrd_select_initial_row(
    rrd_file_t UNUSED(*rrd_file),
    int UNUSED(rra_idx),
    rra_def_t *rra)
{
    return rrd_random() % rra->row_cnt;
}
