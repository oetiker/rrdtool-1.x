#ifndef RRD_CLI_H
#define RRD_CLI_H

#include "rrd_tool.h"

void rrd_json_string(FILE *out, const char *value);
void rrd_json_number(FILE *out, double value);
void rrd_json_info(FILE *out, const rrd_info_t *info);
int rrd_cli_json(int argc, const char **argv);
int rrd_cli_verify(int argc, const char **argv, int json);

#endif
