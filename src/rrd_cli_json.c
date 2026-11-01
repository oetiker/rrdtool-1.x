/*****************************************************************************
 * RRDtool 1.11.0 Copyright by Tobi Oetiker, 1997-2026
 * rrd_cli_json.c  Structured output for the command-line client
 *****************************************************************************/

#include "rrd_cli.h"
#include "rrd_xport.h"

#include <locale.h>
#include <stdint.h>

/* Keep UTF-8 strings intact, but escape invalid filename bytes so the
 * result is still a JSON document on filesystems allowing arbitrary bytes. */
static size_t utf8_length(const unsigned char *p)
{
    size_t length, i;

    if (*p >= 0xc2 && *p <= 0xdf)
        length = 2;
    else if (*p >= 0xe0 && *p <= 0xef)
        length = 3;
    else if (*p >= 0xf0 && *p <= 0xf4)
        length = 4;
    else
        return 0;
    for (i = 1; i < length; i++)
        if (p[i] < 0x80 || p[i] > 0xbf)
            return 0;
    if ((*p == 0xe0 && p[1] < 0xa0) ||
        (*p == 0xed && p[1] >= 0xa0) ||
        (*p == 0xf0 && p[1] < 0x90) ||
        (*p == 0xf4 && p[1] >= 0x90))
        return 0;
    return length;
}

void rrd_json_string(FILE *out, const char *value)
{
    const unsigned char *p = (const unsigned char *) value;

    if (p == NULL) {
        fputs("null", out);
        return;
    }
    fputc('"', out);
    while (*p) {
        if (*p == '"' || *p == '\\') {
            fputc('\\', out);
            fputc(*p++, out);
        } else if (*p < 0x20) {
            fprintf(out, "\\u%04x", *p++);
        } else if (*p >= 0x80) {
            size_t length = utf8_length(p);

            if (length) {
                fwrite(p, 1, length, out);
                p += length;
            } else {
                fprintf(out, "\\u%04x", *p++);
            }
        } else {
            fputc(*p++, out);
        }
    }
    fputc('"', out);
}

void rrd_json_number(FILE *out, double value)
{
    char number[128];
    const char *radix;
    char *point;

    if (isnan(value)) {
        fputs("null", out);
    } else if (isinf(value)) {
        rrd_json_string(out, value < 0 ? "-Infinity" : "Infinity");
    } else {
        snprintf(number, sizeof(number), "%.17g", value);
        radix = localeconv()->decimal_point;
        point = *radix ? strstr(number, radix) : NULL;
        if (point && strcmp(radix, ".") != 0) {
            fwrite(number, 1, (size_t) (point - number), out);
            fputc('.', out);
            fputs(point + strlen(radix), out);
        } else {
            fputs(number, out);
        }
    }
}

static void json_blob(FILE *out, const unsigned char *data, unsigned long size)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    fputs("{\"encoding\":\"base64\",\"data\":\"", out);
    while (size >= 3) {
        fputc(alphabet[data[0] >> 2], out);
        fputc(alphabet[((data[0] & 3) << 4) | (data[1] >> 4)], out);
        fputc(alphabet[((data[1] & 15) << 2) | (data[2] >> 6)], out);
        fputc(alphabet[data[2] & 63], out);
        data += 3;
        size -= 3;
    }
    if (size) {
        fputc(alphabet[data[0] >> 2], out);
        fputc(alphabet[((data[0] & 3) << 4) |
                       (size == 2 ? data[1] >> 4 : 0)], out);
        fputc(size == 2 ? alphabet[(data[1] & 15) << 2] : '=', out);
        fputc('=', out);
    }
    fputs("\"}", out);
}

void rrd_json_info(FILE *out, const rrd_info_t *info)
{
    int comma = 0;

    fputc('{', out);
    for (; info; info = info->next) {
        if (comma++)
            fputc(',', out);
        rrd_json_string(out, info->key);
        fputc(':', out);
        switch (info->type) {
        case RD_I_VAL:
            rrd_json_number(out, info->value.u_val);
            break;
        case RD_I_CNT:
            fprintf(out, "%lu", info->value.u_cnt);
            break;
        case RD_I_INT:
            fprintf(out, "%d", info->value.u_int);
            break;
        case RD_I_STR:
            rrd_json_string(out, info->value.u_str);
            break;
        case RD_I_BLO:
            json_blob(out, info->value.u_blo.ptr, info->value.u_blo.size);
            break;
        }
    }
    fputs("}\n", out);
}

static void json_names(char **names, unsigned long count)
{
    unsigned long i;

    fputc('[', stdout);
    for (i = 0; i < count; i++) {
        if (i)
            fputc(',', stdout);
        rrd_json_string(stdout, names[i]);
    }
    fputc(']', stdout);
}

static void json_fetch(int argc, const char **argv)
{
    time_t start, end;
    unsigned long step, count, i;
    rrd_value_t *values;
    char **names;
    uintmax_t rows, row;

    if (rrd_fetch(argc, argv, &start, &end, &step, &count, &names,
                  &values) != 0)
        return;
    rows = ((uintmax_t) end - (uintmax_t) start) / step;
    fprintf(stdout, "{\"start\":%lld,\"end\":%lld,\"step\":%lu,\"ds_names\":",
            (long long) start, (long long) end, step);
    json_names(names, count);
    fputs(",\"data\":[", stdout);
    for (row = 0; row < rows; row++) {
        /* Unsigned arithmetic also covers intervals crossing the epoch,
         * without overflowing a signed time_t intermediate. */
        time_t timestamp = (time_t) ((uintmax_t) start + (row + 1) * step);

        if (row)
            fputc(',', stdout);
        fprintf(stdout, "{\"timestamp\":%lld,\"values\":[", (long long) timestamp);
        for (i = 0; i < count; i++) {
            if (i)
                fputc(',', stdout);
            rrd_json_number(stdout, values[row * count + i]);
        }
        fputs("]}", stdout);
    }
    fputs("]}\n", stdout);
    for (i = 0; i < count; i++)
        free(names[i]);
    free(names);
    free(values);
}

static void json_lastupdate(int argc, const char **argv)
{
    time_t timestamp;
    unsigned long count, i;
    char **names, **values;

    if (rrd_lastupdate_v(argc, argv, &timestamp, &count, &names, &values) != 0)
        return;
    fprintf(stdout, "{\"last_update\":%lld,\"ds_names\":", (long long) timestamp);
    json_names(names, count);
    fputs(",\"values\":[", stdout);
    for (i = 0; i < count; i++) {
        if (i)
            fputc(',', stdout);
        /* Raw counters may exceed the exact integer range of a JSON
         * consumer's double, so preserve their original decimal strings. */
        rrd_json_string(stdout, strcmp(values[i], "U") == 0 ? NULL : values[i]);
        free(names[i]);
        free(values[i]);
    }
    fputs("]}\n", stdout);
    free(names);
    free(values);
}

static void json_xport(int argc, const char **argv)
{
    const char **args = malloc((argc + 1) * sizeof(*args));
    time_t start, end;
    unsigned long step, count;
    char **legend;
    rrd_value_t *values;

    if (args == NULL) {
        rrd_set_error("allocating JSON xport arguments");
        return;
    }
    args[0] = argv[0];
    args[1] = "--json";
    memcpy(args + 2, argv + 1, (argc - 1) * sizeof(*args));
    if (rrd_xport(argc + 1, args, NULL, &start, &end, &step, &count,
                  &legend, &values) == 0) {
        while (count)
            free(legend[--count]);
        free(legend);
        free(values);
    }
    free(args);
}

int rrd_cli_json(int argc, const char **argv)
{
    const char *command = argv[0];

    if (strcmp(command, "info") == 0 || strcmp(command, "updatev") == 0 ||
        strcmp(command, "graphv") == 0 || strcmp(command, "graph") == 0) {
        rrd_info_t *info;

        if (strcmp(command, "info") == 0)
            info = rrd_info(argc, argv);
        else if (strcmp(command, "updatev") == 0)
            info = rrd_update_v(argc, argv);
        else {
#ifdef HAVE_RRD_GRAPH
            info = rrd_graph_v(argc, argv);
#else
            rrd_set_error("the instance of rrdtool has been compiled without graphics");
            info = NULL;
#endif
        }
        if (info && !rrd_test_error()) {
            rrd_json_info(stdout, info);
        }
        if (info == NULL && !rrd_test_error())
            rrd_set_error("'%s' returned no result", command);
        rrd_info_free(info);
    } else if (strcmp(command, "fetch") == 0) {
        json_fetch(argc, argv);
    } else if (strcmp(command, "lastupdate") == 0) {
        json_lastupdate(argc, argv);
    } else if (strcmp(command, "xport") == 0) {
        json_xport(argc, argv);
    } else if (strcmp(command, "first") == 0 || strcmp(command, "last") == 0) {
        time_t timestamp = strcmp(command, "first") == 0 ?
            rrd_first(argc, argv) : rrd_last(argc, argv);

        if (!rrd_test_error())
            fprintf(stdout, "{\"timestamp\":%lld}\n", (long long) timestamp);
    } else if (strcmp(command, "list") == 0) {
        char *listing = rrd_list(argc, argv);

        if (!rrd_test_error()) {
            fputs("{\"listing\":", stdout);
            rrd_json_string(stdout, listing ? listing : "");
            fputs("}\n", stdout);
        }
        free(listing);
    } else {
        rrd_set_error("JSON output is not supported for '%s'", command);
    }
    if (rrd_test_error()) {
        fputs("{\"error\":", stdout);
        rrd_json_string(stdout, rrd_get_error());
        fputs("}\n", stdout);
        rrd_clear_error();
        return 1;
    }
    return ferror(stdout) ? 1 : 0;
}
