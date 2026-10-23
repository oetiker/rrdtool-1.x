
#include <stdio.h>
#include <string.h>
#include "rrd_config.h"
#ifdef _MSC_VER
#include "win32-glob.h"    /* from https://sourceforge.net/projects/sox/ */
#include "dirent.h"        /* from https://github.com/tronkko/dirent */
#else
#if defined(__MINGW32__) && !defined(HAVE_GLOB_H)   /* MinGW has glob.h, MinGW-w64 not (yet?) */
#include "win32/win32-glob.h"    /* from https://sourceforge.net/projects/sox/ */
#else
#include <glob.h>
#endif
#include <dirent.h>
#endif
#include <sys/types.h>

#include "rrd_tool.h"
#include "rrd_client.h"
#include "rrd_snprintf.h"

static char *move_past_prefix(const char *prefix, const char *string)
{
	int index = 0;

	if (strlen(prefix) > strlen(string)) {
		return (char *)string;
	}

	while (prefix[index] != '\0') {
		if (prefix[index] != string[index]) {
			break;
		}
		index++;
	}

	return (char *)&(string[index]);
}

static char *rrd_list_rec(int recursive, const char *root, const char *dirname)
{
	struct stat st;
	struct dirent *entry;
	DIR *dir;
	char *out = NULL, *out_rec = NULL, *out_short, *next, *ptr;
	char *current = NULL, *fullpath = NULL;
	int saved_errno;

	dir = opendir(dirname);

	if (dir == NULL) {
		/* opendir sets errno */
		return NULL;
	}

	while ((entry = readdir(dir)) != NULL) {
		free(current);
		current = NULL;
		free(fullpath);
		fullpath = NULL;

		if ((strcmp(entry->d_name, ".") == 0) ||
		    (strcmp(entry->d_name, "..") == 0)) {
			continue;
		}

		if (strlen(dirname) + strlen(entry->d_name) + 1 >= PATH_MAX) {
			continue;
		}

		if (asprintf(&current, "%s/%s", dirname, entry->d_name) < 0) {
			current = NULL;
			goto oom;
		}

		/* NOTE: stat(2) follows symlinks and gives info on target. */
		if (stat(current, &st) != 0) {
			continue;
		}

		if (S_ISDIR(st.st_mode) && recursive) {
			if (asprintf(&fullpath, "%s/%s", dirname, entry->d_name) < 0) {
				fullpath = NULL;
				goto oom;
			}
			out_rec = rrd_list_rec(recursive, root, fullpath);

			if (out_rec == NULL) {
				/* Skip an empty or unreadable subdirectory, and
				 * one removed or replaced while we list. Any
				 * other failure, such as running out of memory,
				 * would leave the list incomplete. */
				if (errno == 0 || errno == EACCES ||
				    errno == EPERM || errno == ENOENT ||
				    errno == ENOTDIR) {
					continue;
				}
				goto fail;
			}

			if (asprintf(&next, "%s%s", out ? out : "",
				     out_rec) == -1) {
				goto oom;
			}
			free(out);
			out = next;
			free(out_rec);
			out_rec = NULL;

		} else {

			if (S_ISREG(st.st_mode)) {

				ptr = strstr(entry->d_name, ".rrd");

	 			/* strlen() used to make sure it's matching
	 			 * the end of string. Non-rrd-suffixed regular
	 			 * files are skipped.
	 			 */
				if (ptr == NULL || strlen(ptr) != 4) {
					continue;
				}
			}
			if (asprintf(&fullpath, "%s/%s", dirname, entry->d_name) < 0) {
				fullpath = NULL;
				goto oom;
			}
			out_short = move_past_prefix(root, fullpath);

			/* don't start output with a '/' */
			if (out_short[0] == '/') {
				out_short++;
			}

			if (asprintf(&next, "%s%s\n", out ? out : "",
				     out_short) == -1) {
				goto oom;
			}
			free(out);
			out = next;
		}
	}
	closedir(dir);
	free(current);
	free(fullpath);

	errno = 0;
	return out;

oom:
	errno = ENOMEM;
fail:
	/* asprintf leaves its target undefined on failure: free only
	   what we still own */
	saved_errno = errno;
	free(out);
	free(out_rec);
	free(current);
	free(fullpath);
	closedir(dir);
	errno = saved_errno;
	return NULL;
}

char *rrd_list_r(int recursive, const char *dirname)
{
	char *out = NULL, *next;
	glob_t buf;
	char *ptr;
	unsigned int i;
	struct stat st;

	/* Prevent moving up the directory tree */
	if (strstr(dirname, "..")) {
		errno = EACCES;
		return NULL;
	}

	/* if filename contains wildcards, then use glob() */
	if (strchr(dirname, '*') || strchr(dirname, '?')) {

		/* recursive list + globbing forbidden */
		if (recursive) {
			errno = EINVAL;
			return NULL;
		}

		if (glob(dirname, 0, NULL, &buf)) {
			globfree(&buf);
			errno = ENOENT;
			return NULL;
		}

		for (i = 0; i < buf.gl_pathc; i++) {
			ptr = strrchr(buf.gl_pathv[i], '/');

			if (ptr == NULL) {
				continue;
			}

			if (asprintf(&next, "%s%s\n", out ? out : "",
				     ptr + 1) == -1) {
				free(out);
				globfree(&buf);
				errno = ENOMEM;
				return NULL;
			}
			free(out);
			out = next;
		}
		globfree(&buf);

		if (out == NULL) {
			errno = ENOENT;
		}
		return out;
	}

	/* If 'dirname' matches an RRD file, then return it.
	 * strlen() used to make sure it's matching the end of string.
	 */
	ptr = strstr(dirname, ".rrd");

	if (ptr != NULL && strlen(ptr) == 4) {

		if (stat(dirname, &st)) {
			return NULL;
                }

		if (!S_ISREG(st.st_mode)) {
			/* Same errno as if you try to open() a named socket.
			 * This is less misleading than e.g. EISDIR */
			errno = ENXIO;
			return NULL;
		}

		ptr = strrchr(dirname, '/');

		if (ptr) {
			if (asprintf(&out, "%s\n", ptr + 1) == -1) {
				errno = ENOMEM;
				return NULL;
			}
		} else {
			errno = EINVAL;
		}
		return out;
	}

	/* Process directory */
	if (stat(dirname, &st) != 0) {
		return NULL;
	}

	if (!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		return NULL;
	}
	return rrd_list_rec(recursive, dirname, dirname);
}

char *rrd_list(int argc, const char **argv)
{
	char *opt_daemon = NULL;
	int status;
	int flushfirst = 1;
	int recursive = 0;
	char *list;

	static struct optparse_long long_options[] = {
		{"daemon", 'd', OPTPARSE_REQUIRED},
		{"noflush", 'F', OPTPARSE_NONE},
		{"recursive", 'r', OPTPARSE_NONE},
		{0},
	};
	struct optparse options;
	int    opt;

        optparse_init(&options, argc, argv);

        while ((opt = optparse_long(&options, long_options, NULL)) != -1) {

		switch (opt) {
		case 'd':
			if (opt_daemon != NULL) {
				free (opt_daemon);
			}
			opt_daemon = strdup (options.optarg);
			if (opt_daemon == NULL)
			{
				rrd_set_error ("strdup failed.");
				return NULL;
			}
			break;

		case 'F':
			flushfirst = 0;
			break;

		case 'r':
			recursive=1;
			break;

                case '?':
                        if (opt_daemon)
                        	free(opt_daemon);
                        rrd_set_error("%s", options.errmsg);
                        return NULL;
                        break;

		default:
			rrd_set_error ("Usage: rrdtool %s [--daemon <addr> [--noflush]] <file>",
				       argv[0]);
			if (opt_daemon != NULL) {
				free (opt_daemon);
			}

			return NULL;
			break;
		}

	}

	if ((argc - options.optind) != 1) {
		rrd_set_error ("Usage: rrdtool %s [--daemon <addr> [--noflush]] [--recursive] <directory>",
                argv[0]);

		if (opt_daemon != NULL) {
			free (opt_daemon);
		}

		return NULL;
	}

	if( flushfirst ) {
		status = rrdc_flushall_if_daemon(opt_daemon);

		if (status) {
			if (opt_daemon != NULL) {
				free (opt_daemon);
			}

			return NULL;
		}
	}

	rrdc_connect (opt_daemon);

	if (rrdc_is_connected (opt_daemon)) {
		list = rrdc_list(recursive, argv[options.optind]);
		rrdc_disconnect();

	} else {
		if (opt_daemon) {
			fprintf(stderr, "Error connecting to rrdcached");
			if (rrd_test_error())
				fprintf(stderr, ": %s", rrd_get_error());
			fprintf(stderr, "\n");

			free(opt_daemon);
			return NULL;
		}
		list = rrd_list_r(recursive, argv[options.optind]);

		if (list == NULL) {
			fprintf(stderr, "%s", strerror(errno));
		}
	}

	if (opt_daemon != NULL) {
		free(opt_daemon);
	}

	return list;
}
