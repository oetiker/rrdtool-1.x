
#include <stdio.h>

#include "rrd_tool.h"
#include "rrd_client.h"

char *rrd_list_r(char *dirname);
char *rrd_list(int argc, char **argv);

char *rrd_list_r(char *dirname)
{
	printf("rrd_list_r not implemented yet\n");
	return NULL;
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
			opt_daemon = strdup (optarg);
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

	if ((argc - optind) != 1) {
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
		list = rrdc_list(argv[optind]);
		rrdc_disconnect();

	} else {
		if (opt_daemon) {
			fprintf(stderr, "Error connecting to rrdcached");
			err = rrd_get_error();

			if (err)
				fprintf(stderr, ": %s", err);
			fprintf(stderr, "\n");
		}
		list = rrd_list_r(argv[optind]);
	}

	if (opt_daemon != NULL) {
		free(opt_daemon);
	}

	return list;
}
