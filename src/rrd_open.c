/*****************************************************************************
 * RRDtool 1.2.23  Copyright by Tobi Oetiker, 1997-2007
 *****************************************************************************
 * rrd_open.c  Open an RRD File
 *****************************************************************************
 * $Id$
 * $Log$
 * Revision 1.10  2004/05/26 22:11:12  oetiker
 * reduce compiler warnings. Many small fixes. -- Mike Slifcak <slif@bellsouth.net>
 *
 * Revision 1.9  2003/04/29 21:56:49  oetiker
 * readline in rrd_open.c reads the file in 8 KB blocks, and calls realloc for
 * each block. realloc is very slow in Mac OS X for huge blocks, e.g. when
 * restoring databases from huge xml files. This patch finds the size of the
 * file, and starts out with malloc'ing the full size.
 * -- Peter Speck <speck@ruc.dk>
 *
 * Revision 1.8  2003/04/11 19:43:44  oetiker
 * New special value COUNT which allows calculations based on the position of a
 * value within a data set. Bug fix in rrd_rpncalc.c. PREV returned erroneus
 * value for the second value. Bug fix in rrd_restore.c. Bug causing seek error
 * when accesing an RRD restored from an xml that holds an RRD version <3.
 * --  Ruben Justo <ruben@ainek.com>
 *
 * Revision 1.7  2003/03/31 21:22:12  oetiker
 * enables RRDtool updates with microsecond or in case of windows millisecond
 * precision. This is needed to reduce time measurement error when archive step
 * is small. (<30s) --  Sasha Mikheev <sasha@avalon-net.co.il>
 *
 * Revision 1.6  2003/02/13 07:05:27  oetiker
 * Find attached the patch I promised to send to you. Please note that there
 * are three new source files (src/rrd_is_thread_safe.h, src/rrd_thread_safe.c
 * and src/rrd_not_thread_safe.c) and the introduction of librrd_th. This
 * library is identical to librrd, but it contains support code for per-thread
 * global variables currently used for error information only. This is similar
 * to how errno per-thread variables are implemented.  librrd_th must be linked
 * alongside of libpthred
 *
 * There is also a new file "THREADS", holding some documentation.
 *
 * -- Peter Stamfest <peter@stamfest.at>
 *
 * Revision 1.5  2002/06/20 00:21:03  jake
 * More Win32 build changes; thanks to Kerry Calvert.
 *
 * Revision 1.4  2002/02/01 20:34:49  oetiker
 * fixed version number and date/time
 *
 * Revision 1.3  2001/03/04 13:01:55  oetiker
 * Aberrant Behavior Detection support. A brief overview added to rrdtool.pod.
 * Major updates to rrd_update.c, rrd_create.c. Minor update to other core files.
 * This is backwards compatible! But new files using the Aberrant stuff are not readable
 * by old rrdtool versions. See http://cricket.sourceforge.net/aberrant/rrd_hw.htm
 * -- Jake Brutlag <jakeb@corp.webtv.net>
 *
 * Revision 1.2  2001/03/04 10:29:20  oetiker
 * fixed filedescriptor leak
 * -- Mike Franusich <mike@franusich.com>
 *
 * Revision 1.1.1.1  2001/02/25 22:25:05  oetiker
 * checkin
 *
 *****************************************************************************/

#include "rrd_tool.h"
#include "unused.h"
#define MEMBLK 8192

#ifdef HAVE_MMAP
#define __rrd_read(dst, dst_t, cnt) \
	(dst) = (dst_t*) (data + offset); \
	offset += sizeof(dst_t) * (cnt)
#else
#define __rrd_read(dst, dst_t, cnt) \
	if ((dst = malloc(sizeof(dst_t)*(cnt))) == NULL) { \
		rrd_set_error(#dst " malloc"); \
		goto out_nullify_head; \
	} \
	offset += read (rrd_file->fd, dst, sizeof(dst_t)*(cnt))
#endif

/* open a database file, return its header and an open filehandle */
/* positioned to the first cdp in the first rra */

rrd_file_t *rrd_open(
    const char *const file_name,
    rrd_t *rrd,
    unsigned rdwr)
{
    int       flags = 0;
    mode_t    mode = S_IRUSR;
    int       version;

#ifdef HAVE_MMAP
    int       mm_prot = PROT_READ, mm_flags = 0;
    char     *data;
#endif
    off_t     offset = 0;
    struct stat statb;
    rrd_file_t *rrd_file = malloc(sizeof(rrd_file_t));

    if (rrd_file == NULL) {
        rrd_set_error("allocating rrd_file descriptor for '%s'", file_name);
        return NULL;
    }
    memset(rrd_file, 0, sizeof(rrd_file_t));
    rrd_init(rrd);
#ifdef DEBUG
    if ((rdwr & (RRD_READONLY | RRD_READWRITE)) ==
        (RRD_READONLY | RRD_READWRITE)) {
        /* Both READONLY and READWRITE were given, which is invalid.  */
        rrd_set_error("in read/write request mask");
        exit(-1);
    }
#endif
    if (rdwr & RRD_READONLY) {
        flags |= O_RDONLY;
#ifdef HAVE_MMAP
        mm_flags = MAP_PRIVATE;
# ifdef MAP_NORESERVE
        mm_flags |= MAP_NORESERVE;
# endif
        mm_flags |= MAP_PRIVATE;
#endif
    } else {
        if (rdwr & RRD_READWRITE) {
            mode |= S_IWUSR;
            flags |= O_RDWR;
#ifdef HAVE_MMAP
            mm_flags = MAP_SHARED;
            mm_prot |= PROT_WRITE;
#endif
        }
        if (rdwr & RRD_CREAT) {
            flags |= (O_CREAT | O_TRUNC);
        }
    }
    if (rdwr & RRD_READAHEAD) {
#ifdef MAP_POPULATE
        mm_flags |= MAP_POPULATE;
#endif
#if defined MAP_NONBLOCK && !defined USE_DIRECT_IO
        mm_flags |= MAP_NONBLOCK;   /* just populage ptes */
#endif
    } else {
#ifdef USE_DIRECT_IO
        flags |= O_DIRECT;
#endif
#if 0                   //def O_NONBLOCK
        flags |= O_NONBLOCK;
#endif
    }

    if ((rrd_file->fd = open(file_name, flags, mode)) < 0) {
        rrd_set_error("opening '%s': %s", file_name, rrd_strerror(errno));
        return NULL;
    }

    /* Better try to avoid seeks as much as possible. stat may be heavy but
     * many concurrent seeks are even worse.  */
    if ((fstat(rrd_file->fd, &statb)) < 0) {
        rrd_set_error("fstat '%s': %s", file_name, rrd_strerror(errno));
        goto out_close;
    }
    rrd_file->file_len = statb.st_size;

#ifdef HAVE_POSIX_FADVISE
    /* In general we need no read-ahead when dealing with rrd_files.
       When we stop reading, it is highly unlikely that we start up again.
       In this manner we actually save time and diskaccess (and buffer cache).
       Thanks to Dave Plonka for the Idea of using POSIX_FADV_RANDOM here. */
    if (0 != posix_fadvise(rrd_file->fd, 0, 0, POSIX_FADV_RANDOM)) {
        rrd_set_error("setting POSIX_FADV_RANDOM on '%s': %s", file_name,
                      rrd_strerror(errno));
        goto out_close;
    }
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
    data = mmap(0, rrd_file->file_len, mm_prot, mm_flags,
                rrd_file->fd, offset);

    /* lets see if the first read worked */
    if (data == MAP_FAILED) {
        rrd_set_error("error mmaping file '%s': %s", file_name,
                      rrd_strerror(errno));
        goto out_close;
    }
    rrd_file->file_start = data;
#else
#endif
#ifdef USE_MADVISE
    if (rdwr & RRD_COPY) {  /*XXX: currently not used! */
        /* We will read everything in a moment (copying) */
        madvise(data, rrd_file->file_len, MADV_WILLNEED | MADV_SEQUENTIAL);
        goto out_done;
    }
    /* We do not need to read anything in for the moment */
    madvise(data, rrd_file->file_len, MADV_DONTNEED);
#endif

#ifdef USE_MADVISE
    /* the stat_head will be needed soonish, so hint accordingly */
    madvise(data + offset, sizeof(stat_head_t), MADV_WILLNEED);
#endif

    __rrd_read(rrd->stat_head, stat_head_t,
               1);

    /* lets do some test if we are on track ... */
    if (memcmp(rrd->stat_head->cookie, RRD_COOKIE, sizeof(RRD_COOKIE)) != 0) {
        rrd_set_error("'%s' is not an RRD file", file_name);
        goto out_nullify_head;
    }

    if (rrd->stat_head->float_cookie != FLOAT_COOKIE) {
        rrd_set_error("This RRD was created on other architecture");
        goto out_nullify_head;
    }

    version = atoi(rrd->stat_head->version);

    if (version > atoi(RRD_VERSION)) {
        rrd_set_error("can't handle RRD file version %s",
                      rrd->stat_head->version);
        goto out_nullify_head;
    }
#ifdef USE_MADVISE
    /* the ds_def will be needed soonish, so hint accordingly */
    madvise(data + offset, sizeof(ds_def_t) * rrd->stat_head->ds_cnt,
            MADV_WILLNEED);
#endif
    __rrd_read(rrd->ds_def, ds_def_t,
               rrd->stat_head->ds_cnt);

#ifdef USE_MADVISE
    /* the rra_def will be needed soonish, so hint accordingly */
    madvise(data + offset, sizeof(rra_def_t) * rrd->stat_head->rra_cnt,
            MADV_WILLNEED);
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
#ifdef HAVE_MMAP
        memmove(&rrd->live_head->last_up, data + offset, sizeof(long));
        offset += sizeof(long);
#else
        offset += read(rrd_file->fd, &rrd->live_head->last_up, sizeof(long));
#endif
        rrd->live_head->last_up_usec = 0;
    } else {
#ifdef USE_MADVISE
        /* the live_head will be needed soonish, so hint accordingly */
        madvise(data + offset, sizeof(live_head_t), MADV_WILLNEED);
#endif
        __rrd_read(rrd->live_head, live_head_t,
                   1);
    }
//XXX: This doesn't look like it needs madvise
    __rrd_read(rrd->pdp_prep, pdp_prep_t,
               rrd->stat_head->ds_cnt);

//XXX: This could benefit from madvise()ing
    __rrd_read(rrd->cdp_prep, cdp_prep_t,
               rrd->stat_head->rra_cnt * rrd->stat_head->ds_cnt);

//XXX: This could benefit from madvise()ing
    __rrd_read(rrd->rra_ptr, rra_ptr_t,
               rrd->stat_head->rra_cnt);

#ifdef USE_MADVISE
  out_done:
#endif
    rrd_file->header_len = offset;
    rrd_file->pos = offset;
/* we could close(rrd_file->fd); here, the mapping is still valid anyway */
    return (rrd_file);
  out_nullify_head:
    rrd->stat_head = NULL;
  out_close:
    close(rrd_file->fd);
    return NULL;
}

/* Close a reference to an rrd_file.  */
int rrd_close(
    rrd_file_t *rrd_file)
{
    int       ret;

#ifdef HAVE_MMAP
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
//XXX: mimic fseek, which returns 0 upon success
    return ret == -1;   //XXX: or just ret to mimic lseek
}

/* Get current position in rrd_file.  */
inline off_t rrd_tell(
    rrd_file_t *rrd_file)
{
    return rrd_file->pos;
}

/* read count bytes into buffer buf, starting at rrd_file->pos.
 * Returns the number of bytes read.  */
ssize_t rrd_read(
    rrd_file_t *rrd_file,
    void *buf,
    size_t count)
{
#ifdef HAVE_MMAP
    char     *pos = rrd_file->file_start + rrd_file->pos;

    buf = memmove(buf, pos, count);
    rrd_file->pos += count; /* mimmic read() semantics */
    return count;
#else
    ssize_t   ret;

    ret = read(rrd_file->fd, buf, count);
    //XXX: eventually add generic rrd_set_error(""); here
    rrd_file->pos += count; /* mimmic read() semantics */
    return ret;
#endif
}

/* write count bytes from buffer buf to the current position
 * rrd_file->pos of rrd_file->fd.
 * Returns the number of bytes written.  */
ssize_t rrd_write(
    rrd_file_t *rrd_file,
    const void *buf,
    size_t count)
{
#ifdef HAVE_MMAP
    memmove(rrd_file->file_start + rrd_file->pos, buf, count);
    return count;       /* mimmic write() semantics */
#else
    return write(rrd_file->fd, buf, count);
#endif
}

/* flush all data pending to be written to FD.  */
inline void rrd_flush(
    rrd_file_t *rrd_file)
{
    if (fdatasync(rrd_file->fd) != 0) {
        rrd_set_error("flushing fd %d: %s", rrd_file->fd,
                      rrd_strerror(errno));
    }
}

void rrd_init(
    rrd_t *rrd)
{
    rrd->stat_head = NULL;
    rrd->ds_def = NULL;
    rrd->rra_def = NULL;
    rrd->live_head = NULL;
    rrd->rra_ptr = NULL;
    rrd->pdp_prep = NULL;
    rrd->cdp_prep = NULL;
    rrd->rrd_value = NULL;
}

void rrd_free(
    rrd_t UNUSED(*rrd))
{
#ifndef HAVE_MMAP
    if (atoi(rrd->stat_head->version) < 3)
        free(rrd->live_head);
    free(rrd->stat_head);
    free(rrd->ds_def);
    free(rrd->rra_def);
    free(rrd->rra_ptr);
    free(rrd->pdp_prep);
    free(rrd->cdp_prep);
    free(rrd->rrd_value);
//XXX: ? rrd_init(rrd);
#endif
}

/* routine used by external libraries to free memory allocated by
 * rrd library */
void rrd_freemem(
    void *mem)
{
    free(mem);
}

int readfile(
    const char *file_name,
    char **buffer,
    int skipfirst)
{
    long      writecnt = 0, totalcnt = MEMBLK;
    long      offset = 0;
    FILE     *input = NULL;
    char      c;

    if ((strcmp("-", file_name) == 0)) {
        input = stdin;
    } else {
        if ((input = fopen(file_name, "rb")) == NULL) {
            rrd_set_error("opening '%s': %s", file_name, rrd_strerror(errno));
            return (-1);
        }
    }
    if (skipfirst) {
        do {
            c = getc(input);
            offset++;
        } while (c != '\n' && !feof(input));
    }
    if (strcmp("-", file_name)) {
        fseek(input, 0, SEEK_END);
        /* have extra space for detecting EOF without realloc */
        totalcnt = (ftell(input) + 1) / sizeof(char) - offset;
        if (totalcnt < MEMBLK)
            totalcnt = MEMBLK;  /* sanitize */
        fseek(input, offset * sizeof(char), SEEK_SET);
    }
    if (((*buffer) = (char *) malloc((totalcnt + 4) * sizeof(char))) == NULL) {
        perror("Allocate Buffer:");
        exit(1);
    };
    do {
        writecnt +=
            fread((*buffer) + writecnt, 1,
                  (totalcnt - writecnt) * sizeof(char), input);
        if (writecnt >= totalcnt) {
            totalcnt += MEMBLK;
            if (((*buffer) =
                 rrd_realloc((*buffer),
                             (totalcnt + 4) * sizeof(char))) == NULL) {
                perror("Realloc Buffer:");
                exit(1);
            };
        }
    } while (!feof(input));
    (*buffer)[writecnt] = '\0';
    if (strcmp("-", file_name) != 0) {
        fclose(input);
    };
    return writecnt;
}
