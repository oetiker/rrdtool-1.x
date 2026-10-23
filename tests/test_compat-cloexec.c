#include <compat-cloexec.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef _POSIX_C_SOURCE
#  include <fcntl.h>
#else
#  define O_RDONLY	0
#  define O_RDWR	0
#  define O_APPEND	0
#endif

/* Native Windows programs know no /dev/null; the null device is "NUL". */
#ifdef _WIN32
#  define NULL_DEVICE	"NUL"
#else
#  define NULL_DEVICE	"/dev/null"
#endif

static void fail(char *msg, int line)
{
	fprintf(stderr, "%s:%u %s\n", __FILE__, line, msg);
	abort();
}

static void check_file(FILE *f, int exp_flags, bool is_cloexec, int line)
{
	int		flags;
	int		fd;

	if (!f)
		fail("fopen() failed", line);

#ifndef _POSIX_C_SOURCE
	(void)flags;
	(void)fd;
	(void)exp_flags;
	(void)is_cloexec;
#else
	fd = fileno(f);
	if (fd < 0)
		fail("failed to get fd", line);

	flags = fcntl(fd, F_GETFD);

	if (O_CLOEXEC != 0) {
		if (is_cloexec != (((flags & FD_CLOEXEC) != 0)))
			fail("O_CLOEXEC mismatch", line);
	}

	flags = fcntl(fd, F_GETFL);
	flags &= (O_RDONLY | O_WRONLY | O_RDWR | O_APPEND);
	if (flags != exp_flags)
		fail("flag mismatch", line);
#endif

	fclose(f);
}

int main(void) {
	FILE *f;

	f = _rrd_fopen(NULL_DEVICE, "r");
	check_file(f, O_RDONLY, false, __LINE__);

	f = _rrd_fopen(NULL_DEVICE, "re");
	check_file(f, O_RDONLY, true, __LINE__);

	f = _rrd_fopen(NULL_DEVICE, "w+be");
	check_file(f, O_RDWR, true, __LINE__);

	f = _rrd_fopen(NULL_DEVICE, "a+be");
	check_file(f, O_RDWR | O_APPEND, true, __LINE__);

	errno = 0;
	f = _rrd_fopen(NULL_DEVICE, "rrrrrrrrrrrrrrrrrrrrrrrrr");
	if (f != NULL)
		fail("oversized _rrd_fopen() mode did not fail", __LINE__);
	if (errno != EINVAL)
		fail("oversized _rrd_fopen() mode did not set EINVAL", __LINE__);

	errno = 0;
	f = _rrd_fopen(NULL_DEVICE, "z");
	if (f != NULL)
		fail("unknown _rrd_fopen() mode did not fail", __LINE__);
	if (errno != EINVAL)
		fail("unknown _rrd_fopen() mode did not set EINVAL", __LINE__);
}
