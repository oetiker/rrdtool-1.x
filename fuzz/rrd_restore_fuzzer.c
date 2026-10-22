/*****************************************************************************
 * RRDtool 1.11.0 Copyright by Tobi Oetiker, 1997-2026
 *****************************************************************************
 * rrd_restore_fuzzer.c  libFuzzer harness for rrd_restore()
 *****************************************************************************
 * rrd_restore() reads an XML dump via libxml2's pull parser and writes a
 * fresh .rrd file out of it, so -- like rrd_open() -- it needs a real file
 * on disk rather than an in-memory buffer, and each iteration additionally
 * produces a real output file that has to be cleaned up. -f/--force-overwrite
 * is always passed so restore doesn't reject the pre-existing mkstemp()
 * output path with O_EXCL, and -r/--range-check is always passed to also
 * exercise value_check_range() on every <row>.
 *****************************************************************************/

#include "rrd_tool.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(
    const uint8_t *data,
    size_t size)
{
    const char *tmpdir = getenv("TMPDIR");
    char     *in_path;
    char     *out_path;
    int       in_fd;
    int       out_fd;
    size_t    written;
    const char *argv[6];

    if (tmpdir == NULL || *tmpdir == '\0')
        tmpdir = "/tmp";
    size_t in_path_size = strlen(tmpdir) + sizeof("/rrd_restore_fuzz_in.XXXXXX");
    in_path = malloc(in_path_size);
    if (in_path == NULL) {
        return 0;
    }
    snprintf(in_path, in_path_size, "%s/rrd_restore_fuzz_in.XXXXXX", tmpdir);
    size_t out_path_size = strlen(tmpdir) + sizeof("/rrd_restore_fuzz_out.XXXXXX");
    out_path = malloc(out_path_size);
    if (out_path == NULL) {
        free(in_path);
        return 0;
    }
    snprintf(out_path, out_path_size, "%s/rrd_restore_fuzz_out.XXXXXX", tmpdir);

    in_fd = mkstemp(in_path);
    if (in_fd < 0)
        goto cleanup_paths;

    /* short writes can't happen for a regular file barring ENOSPC/EINTR,
     * but check anyway rather than feed rrd_restore() a truncated fixture */
    written = 0;
    while (written < size) {
        ssize_t   n = write(in_fd, data + written, size - written);

        if (n <= 0) {
            if (n < 0 && errno == EINTR)
                continue;
            break;
        }
        written += (size_t) n;
    }
    {
        int status = close(in_fd);
        if (written != size || status != 0)
            goto cleanup_input;
    }

    /* reserve a unique output path; -f below lets rrd_restore() reuse it
     * without tripping over the O_EXCL it'd otherwise open with */
    out_fd = mkstemp(out_path);
    if (out_fd < 0) {
        goto cleanup_input;
    }
    if (close(out_fd) != 0)
        goto cleanup_output;

    argv[0] = "restore";
    argv[1] = "-f";
    argv[2] = "-r";
    argv[3] = in_path;
    argv[4] = out_path;
    argv[5] = NULL;

    rrd_clear_error();
    rrd_restore(5, argv);

  cleanup_output:
    unlink(out_path);
  cleanup_input:
    unlink(in_path);
  cleanup_paths:
    free(in_path);
    free(out_path);

    return 0;
}
