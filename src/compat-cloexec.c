#include "compat-cloexec.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef _POSIX_C_SOURCE
#  include <fcntl.h>
#  include <unistd.h>
#else
#  define O_CREAT	0
#  define O_WRONLY	0
#  define O_RDONLY	0
#  define O_APPEND	0
#  define O_RDWR	0
#endif

inline static bool have_decl_o_cloexec(void)
{
#ifdef HAVE_DECL_O_CLOEXEC
	return true;
#else
	return false;
#endif
}

FILE *_rrd_fopen(const char *restrict pathname, const char *restrict mode_raw)
{
	char mode[20];
	const char *in = mode_raw;
	char *out = mode;
	int flags = 0;
	int rw_flags = 0;
	bool is_cloexec = false;

	/* We are the only caller and never use mode strings with more than 20
	   chars... But just to be sure... */
	if (strlen(mode_raw) >= sizeof mode)
		abort();

	/* parse the mode string and strip away the 'e' flag */
	while (*in) {
		char c = *in++;

		switch (c) {
		case 'w':
			flags |= O_CREAT;
			rw_flags = O_WRONLY;
			break;
		case 'r':
			rw_flags = O_RDONLY;
			break;
		case 'a':
			flags |= O_CREAT | O_APPEND;
			rw_flags = O_WRONLY;
			break;
		case '+':
			rw_flags = O_RDWR;
			break;
		case 'e':
			is_cloexec = true;
			/* continue loop and do not copy mode char */
			continue;
		case 'b':
			break;
		default:
			/* we are the only caller and should not set any
			   unknown flag */
			abort();
		}

		*out++ = c;
	}

	*out = '\0';

#ifndef _POSIX_C_SOURCE
	(void)flags;
	(void)rw_flags;
	(void)is_cloexec;
	/* TODO: do we have to care about O_CLOEXEC behavior on non-POSIX
	   systems? */
#else
	if (have_decl_o_cloexec() && is_cloexec) {
		int fd;
		FILE *res;

		fd = open(pathname, flags | rw_flags | O_CLOEXEC, 0666);
		if (fd < 0)
			return NULL;

		res = fdopen(fd, mode);
		if (!res)
			close(fd);

		return res;
	}
#endif

	return fopen(pathname, mode);
}
