
#include <stdio.h>
#include <string.h>
#include <glob.h>
#include <dirent.h>
#include <sys/types.h>

#include "rrd_tool.h"
#include "rrd_client.h"

char *rrd_list_r(char *dirname);
char *rrd_list(int argc, char **argv);

char *rrd_list_r(char *dirname)
{
#define SANE_ASPRINTF(_dest_str, _format, _params...)			\
	if (asprintf(&_dest_str, _format, _params) == -1) {		\
		if (out != NULL) {					\
			free(out);					\
		}							\
		errno = ENOMEM;						\
		return NULL;						\
	}
#define SANE_ASPRINTF2(_dest_str, _format, _params...)			\
	if (asprintf(&_dest_str, _format, _params) == -1) {		\
		if (out != NULL) {					\
			free(out);					\
		}							\
		closedir(dir);						\
		errno = ENOMEM;						\
		return NULL;						\
	}

	char *out = NULL, *tmp;
	char current[PATH_MAX];
	glob_t buf;
	char *ptr;
	unsigned int i;
	struct stat st;
	DIR *dir;
  	struct dirent *entry;

	/* Prevent moving up the directory tree */
	if (strstr(dirname, "..")) {
		errno = EACCES;
		return NULL;
	}

	/* if filename contains wildcards, then use glob() */
	if (strchr(dirname, '*') || strchr(dirname, '?')) {

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

			if (out == NULL) {
				SANE_ASPRINTF(out, "%s\n", ptr + 1);

			} else {
				tmp = out;
				SANE_ASPRINTF(out, "%s%s\n", out, ptr + 1);
				free(tmp);
			}
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

		if (!stat(dirname, &st) && S_ISREG(st.st_mode)) {
			ptr = strrchr(dirname, '/');

			if (ptr) {
				SANE_ASPRINTF(out, "%s\n", ptr + 1);
			}
		}
		return out;
	}

	/* Process directory */
	dir = opendir(dirname);

	if (dir == NULL) {
		/* opendir sets errno */
		return NULL;
	}

	while ((entry = readdir(dir)) != NULL) {

		if ((strcmp(entry->d_name, ".") == 0) ||
		    (strcmp(entry->d_name, "..") == 0)) {
			continue;
		}

		if (strlen(dirname) + strlen(entry->d_name) + 1 >= PATH_MAX) {
			continue;
		}
		snprintf(&current[0], PATH_MAX, "%s/%s", dirname, entry->d_name);

		/* Only return directories and rrd files.
		 * NOTE: stat(2) follows symlinks and gives info on target. */
		if (stat(current, &st) != 0) {
			continue;
		}

		if (!S_ISDIR(st.st_mode)) {

			if (S_ISREG(st.st_mode)) {

				ptr = strstr(entry->d_name, ".rrd");

	 			/* strlen() used to make sure it's matching
	 			 * the end of string.
	 			 */
				if (ptr == NULL || strlen(ptr) != 4) {
					continue;
				}

			} else {
				continue;
			}
		}

		if (out == NULL) {
			SANE_ASPRINTF2(out, "%s\n", entry->d_name);

		} else {
			tmp = out;
			SANE_ASPRINTF2(out, "%s%s\n", out, entry->d_name);
			free(tmp);
		}
	}
	closedir(dir);

	return out;
}

char *rrd_list(int argc, char **argv)
{
	char *opt_daemon = NULL;
	int status;
	int flushfirst = 1;
	char *list;

	static struct optparse_long long_options[] = {
		{"daemon", 'd', OPTPARSE_REQUIRED},
		{"noflush", 'F', OPTPARSE_NONE},
		{0},
	};
	struct optparse options;
	int    opt;
	char    *err = NULL;

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
		rrd_set_error ("Usage: rrdtool %s [--daemon <addr> [--noflush]] <directory>",
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
		list = rrdc_list(argv[options.optind]);
		rrdc_disconnect();

	} else {
		if (opt_daemon) {
			fprintf(stderr, "Error connecting to rrdcached");
			err = rrd_get_error();

			if (err)
				fprintf(stderr, ": %s", err);
			fprintf(stderr, "\n");
		}
		list = rrd_list_r(argv[options.optind]);

		if (list == NULL) {
			fprintf(stderr, "%s", strerror(errno));
		}
	}

	if (opt_daemon != NULL) {
		free(opt_daemon);
	}

	return list;
}
