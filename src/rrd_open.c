/*****************************************************************************
 * RRDtool 1.3.2  Copyright by Tobi Oetiker, 1997-2008
 *****************************************************************************
 * rrd_open.c  Open an RRD File
 *****************************************************************************
 * $Id$
 *****************************************************************************/

#include "rrd_tool.h"
#include "unused.h"
#define MEMBLK 8192

/* DEBUG 2 prints information obtained via mincore(2) */
#define DEBUG 1
/* do not calculate exact madvise hints but assume 1 page for headers and
 * set DONTNEED for the rest, which is assumed to be data */
/* Avoid calling madvise on areas that were already hinted. May be benefical if
 * your syscalls are very slow */

#ifdef HAVE_MMAP
/* the cast to void* is there to avoid this warning seen on ia64 with certain
   versions of gcc: 'cast increases required alignment of target type'
*/
#define __rrd_read(dst, dst_t, cnt) { \
	size_t wanted = sizeof(dst_t)*(cnt); \
	if (offset + wanted > rrd_file->file_len) { \
		rrd_set_error("reached EOF while loading header " #dst); \
		goto out_nullify_head; \
	} \
	(dst) = (dst_t*)(void*) (data + offset); \
	offset += wanted; \
    }
#else
#define __rrd_read(dst, dst_t, cnt) { \
	size_t wanted = sizeof(dst_t)*(cnt); \
        size_t got; \
	if ((dst = malloc(wanted)) == NULL) { \
		rrd_set_error(#dst " malloc"); \
		goto out_nullify_head; \
	} \
        got = read (rrd_file->fd, dst, wanted); \
	if (got != wanted) { \
		rrd_set_error("short read while reading header " #dst); \
                goto out_nullify_head; \
	} \
	offset += got; \
    }
#endif

/* get the address of the start of this page */
#if defined USE_MADVISE || defined HAVE_POSIX_FADVISE
#ifndef PAGE_START
#define PAGE_START(addr) ((addr)&(~(_page_size-1)))
#endif
#endif

/* Open a database file, return its header and an open filehandle,
 * positioned to the first cdp in the first rra.
 * In the error path of rrd_open, only rrd_free(&rrd) has to be called
 * before returning an error. Do not call rrd_close upon failure of rrd_open.
 * If creating a new file, the parameter rrd must be initialised with
 * details of the file content.
 * If opening an existing file, then use rrd must be initialised by
 * rrd_init(rrd) prior to invoking rrd_open
 */

rrd_file_t *rrd_open(
    const char *const file_name,
    rrd_t *rrd,
    unsigned rdwr)
{
    int i;
    int       flags = 0;
    mode_t    mode = S_IRUSR;
    int       version;

#ifdef HAVE_MMAP
    ssize_t   _page_size = sysconf(_SC_PAGESIZE);
    char     *data = MAP_FAILED;
#endif
    off_t     offset = 0;
    struct stat statb;
    rrd_file_t *rrd_file = NULL;
    off_t     newfile_size = 0;
    off_t header_len, value_cnt, data_len;

    /* Are we creating a new file? */
    if((rdwr & RRD_CREAT) && (rrd->stat_head != NULL))
    {
        header_len = \
          sizeof(stat_head_t) + \
          sizeof(ds_def_t) * rrd->stat_head->ds_cnt + \
          sizeof(rra_def_t) * rrd->stat_head->rra_cnt + \
          sizeof(time_t) + \
          sizeof(live_head_t) + \
          sizeof(pdp_prep_t) * rrd->stat_head->ds_cnt + \
          sizeof(cdp_prep_t) * rrd->stat_head->ds_cnt * rrd->stat_head->rra_cnt + \
          sizeof(rra_ptr_t) * rrd->stat_head->rra_cnt;

        value_cnt = 0;
        for (i = 0; i < rrd->stat_head->rra_cnt; i++)
            value_cnt += rrd->stat_head->ds_cnt * rrd->rra_def[i].row_cnt;

        data_len = sizeof(rrd_value_t) * value_cnt;

        newfile_size = header_len + data_len;
    }
    
    rrd_file = malloc(sizeof(rrd_file_t));
    if (rrd_file == NULL) {
        rrd_set_error("allocating rrd_file descriptor for '%s'", file_name);
        return NULL;
    }
    memset(rrd_file, 0, sizeof(rrd_file_t));

#ifdef DEBUG
    if ((rdwr & (RRD_READONLY | RRD_READWRITE)) ==
        (RRD_READONLY | RRD_READWRITE)) {
        /* Both READONLY and READWRITE were given, which is invalid.  */
        rrd_set_error("in read/write request mask");
        exit(-1);
    }
#endif

#ifdef HAVE_MMAP
    rrd_file->mm_prot = PROT_READ;
    rrd_file->mm_flags = 0;
#endif

    if (rdwr & RRD_READONLY) {
        flags |= O_RDONLY;
#ifdef HAVE_MMAP
        rrd_file->mm_flags = MAP_PRIVATE;
# ifdef MAP_NORESERVE
        rrd_file->mm_flags |= MAP_NORESERVE;  /* readonly, so no swap backing needed */
# endif
#endif
    } else {
        if (rdwr & RRD_READWRITE) {
            mode |= S_IWUSR;
            flags |= O_RDWR;
#ifdef HAVE_MMAP
            rrd_file->mm_flags = MAP_SHARED;
            rrd_file->mm_prot |= PROT_WRITE;
#endif
        }
        if (rdwr & RRD_CREAT) {
            flags |= (O_CREAT | O_TRUNC);
        }
    }
    if (rdwr & RRD_READAHEAD) {
#ifdef MAP_POPULATE
        rrd_file->mm_flags |= MAP_POPULATE;   /* populate ptes and data */
#endif
#if defined MAP_NONBLOCK
        rrd_file->mm_flags |= MAP_NONBLOCK;   /* just populate ptes */
#endif
    }
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)
    flags |= O_BINARY;
#endif

    if ((rrd_file->fd = open(file_name, flags, mode)) < 0) {
        rrd_set_error("opening '%s': %s", file_name, rrd_strerror(errno));
        goto out_free;
    }

    /* Better try to avoid seeks as much as possible. stat may be heavy but
     * many concurrent seeks are even worse.  */
    if (newfile_size == 0 && ((fstat(rrd_file->fd, &statb)) < 0)) {
        rrd_set_error("fstat '%s': %s", file_name, rrd_strerror(errno));
        goto out_close;
    }
    if (newfile_size == 0) {
        rrd_file->file_len = statb.st_size;
    } else {
        rrd_file->file_len = newfile_size;
        lseek(rrd_file->fd, newfile_size - 1, SEEK_SET);
        write(rrd_file->fd, "\0", 1);   /* poke */
        lseek(rrd_file->fd, 0, SEEK_SET);
    }
#ifdef HAVE_POSIX_FADVISE
    /* In general we need no read-ahead when dealing with rrd_files.
       When we stop reading, it is highly unlikely that we start up again.
       In this manner we actually save time and diskaccess (and buffer cache).
       Thanks to Dave Plonka for the Idea of using POSIX_FADV_RANDOM here. */
    posix_fadvise(rrd_file->fd, 0, 0, POSIX_FADV_RANDOM);
#endif

/*
        if (rdwr & RRD_READWRITE)
        {
           if (setvbuf((rrd_file->fd),NULL,_IONBF,2)) {
                  rrd_set_error("failed to disable the stream buffer\n");
                  return (-1);
           }
        }
*/

#ifdef HAVE_MMAP
    data = mmap(0, rrd_file->file_len, rrd_file->mm_prot, rrd_file->mm_flags,
                rrd_file->fd, offset);

    /* lets see if the first read worked */
    if (data == MAP_FAILED) {
        rrd_set_error("mmaping file '%s': %s", file_name,
                      rrd_strerror(errno));
        goto out_close;
    }
    rrd_file->file_start = data;
    if (rdwr & RRD_CREAT) {
        memset(data, DNAN, newfile_size - 1);
        goto out_done;
    }
#endif
    if (rdwr & RRD_CREAT)
        goto out_done;
#ifdef USE_MADVISE
    if (rdwr & RRD_COPY) {
        /* We will read everything in a moment (copying) */
        madvise(data, rrd_file->file_len, MADV_WILLNEED | MADV_SEQUENTIAL);
    } else {
        /* We do not need to read anything in for the moment */
        madvise(data, rrd_file->file_len, MADV_RANDOM);
        /* the stat_head will be needed soonish, so hint accordingly */
        madvise(data, sizeof(stat_head_t), MADV_WILLNEED | MADV_RANDOM);
    }
#endif

    __rrd_read(rrd->stat_head, stat_head_t,
               1);

    /* lets do some test if we are on track ... */
    if (memcmp(rrd->stat_head->cookie, RRD_COOKIE, sizeof(RRD_COOKIE)) != 0) {
        rrd_set_error("'%s' is not an RRD file", file_name);
        goto out_nullify_head;
    }

    if (rrd->stat_head->float_cookie != FLOAT_COOKIE) {
        rrd_set_error("This RRD was created on another architecture");
        goto out_nullify_head;
    }

    version = atoi(rrd->stat_head->version);

    if (version > atoi(RRD_VERSION)) {
        rrd_set_error("can't handle RRD file version %s",
                      rrd->stat_head->version);
        goto out_nullify_head;
    }
#if defined USE_MADVISE
    /* the ds_def will be needed soonish, so hint accordingly */
    madvise(data + PAGE_START(offset),
            sizeof(ds_def_t) * rrd->stat_head->ds_cnt, MADV_WILLNEED);
#endif
    __rrd_read(rrd->ds_def, ds_def_t,
               rrd->stat_head->ds_cnt);

#if defined USE_MADVISE
    /* the rra_def will be needed soonish, so hint accordingly */
    madvise(data + PAGE_START(offset),
            sizeof(rra_def_t) * rrd->stat_head->rra_cnt, MADV_WILLNEED);
#endif
    __rrd_read(rrd->rra_def, rra_def_t,
               rrd->stat_head->rra_cnt);

    /* handle different format for the live_head */
    if (version < 3) {
        rrd->live_head = (live_head_t *) malloc(sizeof(live_head_t));
        if (rrd->live_head == NULL) {
            rrd_set_error("live_head_t malloc");
            goto out_close;
        }
#if defined USE_MADVISE
        /* the live_head will be needed soonish, so hint accordingly */
        madvise(data + PAGE_START(offset), sizeof(time_t), MADV_WILLNEED);
#endif
        __rrd_read(rrd->legacy_last_up, time_t,
                   1);

        rrd->live_head->last_up = *rrd->legacy_last_up;
        rrd->live_head->last_up_usec = 0;
    } else {
#if defined USE_MADVISE
        /* the live_head will be needed soonish, so hint accordingly */
        madvise(data + PAGE_START(offset),
                sizeof(live_head_t), MADV_WILLNEED);
#endif
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

    {
      unsigned long row_cnt = 0;
      unsigned long i;

      for (i=0; i<rrd->stat_head->rra_cnt; i++)
        row_cnt += rrd->rra_def[i].row_cnt;

      off_t correct_len = rrd_file->header_len +
        sizeof(rrd_value_t) * row_cnt * rrd->stat_head->ds_cnt;

      if (correct_len > rrd_file->file_len)
      {
        rrd_set_error("'%s' is too small (should be %ld bytes)",
                      file_name, (long long) correct_len);
        goto out_nullify_head;
      }
    }

  out_done:
    return (rrd_file);
  out_nullify_head:
    rrd->stat_head = NULL;
  out_close:
#ifdef HAVE_MMAP
    if (data != MAP_FAILED)
      munmap(data, rrd_file->file_len);
#endif
    close(rrd_file->fd);
  out_free:
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
#ifdef HAVE_MMAP
    /* pretty print blocks in core */
    off_t     off;
    unsigned char *vec;
    ssize_t   _page_size = sysconf(_SC_PAGESIZE);

    off = rrd_file->file_len +
        ((rrd_file->file_len + _page_size - 1) / _page_size);
    vec = malloc(off);
    if (vec != NULL) {
        memset(vec, 0, off);
        if (mincore(rrd_file->file_start, rrd_file->file_len, vec) == 0) {
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
    rrd_file_t *file)
{
    int       rcstat;

    {
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__)
        struct _stat st;

        if (_fstat(file->fd, &st) == 0) {
            rcstat = _locking(file->fd, _LK_NBLCK, st.st_size);
        } else {
            rcstat = -1;
        }
#else
        struct flock lock;

        lock.l_type = F_WRLCK;  /* exclusive write lock */
        lock.l_len = 0; /* whole file */
        lock.l_start = 0;   /* start of file */
        lock.l_whence = SEEK_SET;   /* end of file */

        rcstat = fcntl(file->fd, F_SETLK, &lock);
#endif
    }

    return (rcstat);
}


/* drop cache except for the header and the active pages */
void rrd_dontneed(
    rrd_file_t *rrd_file,
    rrd_t *rrd)
{
#if defined USE_MADVISE || defined HAVE_POSIX_FADVISE
    off_t dontneed_start;
    off_t rra_start;
    off_t active_block;
    unsigned long i;
    ssize_t   _page_size = sysconf(_SC_PAGESIZE);

    if (rrd_file == NULL) {
#if defined DEBUG && DEBUG
	    fprintf (stderr, "rrd_dontneed: Argument 'rrd_file' is NULL.\n");
#endif
	    return;
    }

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
            madvise(rrd_file->file_start + dontneed_start,
                    active_block - dontneed_start - 1, MADV_DONTNEED);
#endif
/* in linux at least only fadvise DONTNEED seems to purge pages from cache */
#ifdef HAVE_POSIX_FADVISE
            posix_fadvise(rrd_file->fd, dontneed_start,
                          active_block - dontneed_start - 1,
                          POSIX_FADV_DONTNEED);
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
	    madvise(rrd_file->file_start + dontneed_start,
		    rrd_file->file_len - dontneed_start, MADV_DONTNEED);
#endif
#ifdef HAVE_POSIX_FADVISE
	    posix_fadvise(rrd_file->fd, dontneed_start,
			  rrd_file->file_len - dontneed_start,
			  POSIX_FADV_DONTNEED);
#endif
    }

#if defined DEBUG && DEBUG > 1
    mincore_print(rrd_file, "after");
#endif
#endif                          /* without madvise and posix_fadvise ist does not make much sense todo anything */
}





int rrd_close(
    rrd_file_t *rrd_file)
{
    int       ret;

#ifdef HAVE_MMAP
    ret = msync(rrd_file->file_start, rrd_file->file_len, MS_ASYNC);
    if (ret != 0)
        rrd_set_error("msync rrd_file: %s", rrd_strerror(errno));
    ret = munmap(rrd_file->file_start, rrd_file->file_len);
    if (ret != 0)
        rrd_set_error("munmap rrd_file: %s", rrd_strerror(errno));
#endif
    ret = close(rrd_file->fd);
    if (ret != 0)
        rrd_set_error("closing file: %s", rrd_strerror(errno));
    free(rrd_file);
    rrd_file = NULL;
    return ret;
}


/* Set position of rrd_file.  */

off_t rrd_seek(
    rrd_file_t *rrd_file,
    off_t off,
    int whence)
{
    off_t     ret = 0;

#ifdef HAVE_MMAP
    if (whence == SEEK_SET)
        rrd_file->pos = off;
    else if (whence == SEEK_CUR)
        rrd_file->pos += off;
    else if (whence == SEEK_END)
        rrd_file->pos = rrd_file->file_len + off;
#else
    ret = lseek(rrd_file->fd, off, whence);
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
    buf = memcpy(buf, rrd_file->file_start + rrd_file->pos, _cnt);

    rrd_file->pos += _cnt;  /* mimmic read() semantics */
    return _cnt;
#else
    ssize_t   ret;

    ret = read(rrd_file->fd, buf, count);
    if (ret > 0)
        rrd_file->pos += ret;   /* mimmic read() semantics */
    return ret;
#endif
}


/* Write count bytes from buffer buf to the current position
 * rrd_file->pos of rrd_file->fd.
 * Returns the number of bytes written or <0 on error.  */

ssize_t rrd_write(
    rrd_file_t *rrd_file,
    const void *buf,
    size_t count)
{
#ifdef HAVE_MMAP
    int old_size = rrd_file->file_len;
    if (count == 0)
        return 0;
    if (buf == NULL)
        return -1;      /* EINVAL */
    
    if((rrd_file->pos + count) > old_size)
    {
        rrd_set_error("attempting to write beyond end of file");
        return -1;
    }
    memcpy(rrd_file->file_start + rrd_file->pos, buf, count);
    rrd_file->pos += count;
    return count;       /* mimmic write() semantics */
#else
    ssize_t   _sz = write(rrd_file->fd, buf, count);

    if (_sz > 0)
        rrd_file->pos += _sz;
    return _sz;
#endif
}


/* flush all data pending to be written to FD.  */

void rrd_flush(
    rrd_file_t *rrd_file)
{
    if (fdatasync(rrd_file->fd) != 0) {
        rrd_set_error("flushing fd %d: %s", rrd_file->fd,
                      rrd_strerror(errno));
    }
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
}


/* free RRD header data.  */

#ifdef HAVE_MMAP
void rrd_free(
    rrd_t *rrd)
{
    if (rrd->legacy_last_up) {  /* this gets set for version < 3 only */
        free(rrd->live_head);
    }
}
#else
void rrd_free(
    rrd_t *rrd)
{
    free(rrd->live_head);
    free(rrd->stat_head);
    free(rrd->ds_def);
    free(rrd->rra_def);
    free(rrd->rra_ptr);
    free(rrd->pdp_prep);
    free(rrd->cdp_prep);
    free(rrd->rrd_value);
}
#endif


/* routine used by external libraries to free memory allocated by
 * rrd library */

void rrd_freemem(
    void *mem)
{
    free(mem);
}
