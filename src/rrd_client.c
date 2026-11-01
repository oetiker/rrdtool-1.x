/**
 * RRDtool - src/rrd_client.c
 * Copyright (C) 2008-2013  Florian octo Forster
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *   Florian octo Forster <octo at verplant.org>
 *   Sebastian tokkee Harl <sh at tokkee.org>
 **/

#ifdef _WIN32

#include <ws2tcpip.h>   // contain #include <winsock2.h>
#ifdef _MSC_VER
// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")
#endif
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <tchar.h>
#include <locale.h>

#endif

#ifdef __MINGW32__
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#endif

#include "rrd_strtod.h"
#include "rrd.h"
#include "rrd_tool.h"
#include "rrd_client.h"
#include "mutex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#ifndef _WIN32
#include <strings.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <locale.h>
#endif
#include <sys/types.h>
#include <inttypes.h>
#include <limits.h>

#include "compat-cloexec.h"

struct rrdc_response_s {
    int       status;
    char     *message;
    char    **lines;
    size_t    lines_num;
};
typedef struct rrdc_response_s rrdc_response_t;

struct rrd_client {
    int       sd;
    char     *sd_path;

    char      _inbuf[RRD_CMD_MAX];
    char     *inbuf;
    size_t    inbuf_used;
};

static rrd_client_t default_client = { -1, NULL, {0}, NULL, 0 };

static mutex_t lock = MUTEX_INITIALIZER;

static int reconnect(
    rrd_client_t *client);

/* get_path: Return a path name appropriate to be sent to the daemon.
 *
 * When talking to a local daemon (thru a UNIX socket), relative path names
 * are resolved to absolute path names to allow for transparent integration
 * into existing solutions (as requested by Tobi). Else, absolute path names
 * are not allowed, since path name translation is done by the server.
 *
 * The caller must call free() on the returned value.
 *
 * One must hold `lock' when calling this function with `default_client'. */
static char *get_path(
    rrd_client_t *client,
    const char *path)
{                       /* {{{ */
    char     *ret = NULL;
    const char *strip = getenv(ENV_RRDCACHED_STRIPPATH);
    size_t    len;
    int       is_unix = 0;

    if ((client == NULL) || (path == NULL) || (client->sd_path == NULL))
        return (NULL);

    if ((*client->sd_path == '/')
        || (strncmp("unix:", client->sd_path, strlen("unix:")) == 0))
        is_unix = 1;

    if (is_unix) {
        if (path == NULL || strlen(path) == 0)
            return NULL;
        ret = realpath(path, NULL);
        if (ret == NULL) {
            /* this may happen, because the file DOES NOT YET EXIST (as would be
             * the case for rrdcreate) - retry by stripping the last path element,
             * resolving the directory and re-concatenate them.... */
            char     *dir_path;
            char     *lastslash = strrchr(path, '/');
            char     *dir = (lastslash == NULL
                             || lastslash == path) ? strdup(".")
#ifdef HAVE_STRNDUP
                : strndup(path, lastslash - path);
#else
          :    strdup(path);

            if (lastslash && lastslash != path) {
                dir[lastslash - path] = '\0';
            }
#endif
            if (dir != NULL) {
                dir_path = realpath(dir, NULL);
                free(dir);
                if (dir_path == NULL) {
                    rrd_set_error("realpath(%s): %s", path,
                                  rrd_strerror(errno));
                    return NULL;
                }
                ret = malloc(strlen(dir_path)
                             + (lastslash ? strlen(lastslash) : 1 +
                                strlen(path)) + 1);
                if (ret == NULL) {
                    rrd_set_error("cannot allocate memory");
                    free(dir_path);
                    return NULL;
                }

                strcpy(ret, dir_path);
                if (lastslash != NULL) {
                    strcat(ret, lastslash);
                } else {
                    strcat(ret, "/");
                    strcat(ret, path);
                }
                free(dir_path);
            } else {
                // out of memory
                rrd_set_error("cannot allocate memory");
                ret = NULL; // redundant, but make intention clear
            }
        }
        return ret;
    } else {
        if (*path == '/') { /* absolute path */
            /* if we are stripping, then check and remove the head */
            if (strip) {
                len = strlen(strip);
                if (strncmp(path, strip, len) == 0) {
                    path += len;
                    while (*path == '/')
                        path++;
                    return strdup(path);
                }
            } else
                rrd_set_error("absolute path names not allowed when talking "
                              "to a remote daemon");
            return NULL;
        }
    }

    return strdup(path);
}                       /* }}} char *get_path */

static size_t strsplit(
    char *string,
    char **fields,
    size_t size)
{                       /* {{{ */
    size_t    i;
    char     *ptr;
    char     *saveptr;

    i = 0;
    ptr = string;
    saveptr = NULL;
    while (i < size &&
           (fields[i] = strtok_r(ptr, " \t\r\n", &saveptr)) != NULL) {
        ptr = NULL;
        i++;
    }

    if ((i == size) && (strtok_r(NULL, " \t\r\n", &saveptr) != NULL))
        return (size + 1);

    return (i);
}                       /* }}} size_t strsplit */

static int parse_header(
    char *line,         /* {{{ */
    char **ret_key,
    char **ret_value)
{
    char     *tmp;

    *ret_key = line;

    tmp = strchr(line, ':');
    if (tmp == NULL)
        return (-1);

    do {
        *tmp = 0;
        tmp++;
    }
    while ((tmp[0] == ' ') || (tmp[0] == '\t'));

    if (*tmp == 0)
        return (-1);

    *ret_value = tmp;
    return (0);
}                       /* }}} int parse_header */

static int parse_uintmax_header(
    char *line,         /* {{{ */
    char **ret_key,
    uintmax_t *ret_value)
{
    char     *str_value;
    char     *endptr;
    int       status;

    str_value = NULL;
    status = parse_header(line, ret_key, &str_value);
    if (status != 0)
        return (status);

    if ((str_value[0] < '0') || (str_value[0] > '9'))
        return (-1);

    endptr = NULL;
    errno = 0;
    *ret_value = strtoumax(str_value, &endptr, /* base = */ 10);
    if ((endptr == str_value) || (errno != 0) || (endptr[0] != 0))
        return (-1);

    return (0);
}                       /* }}} int parse_uintmax_header */

static int parse_intmax_header(
    char *line,         /* {{{ */
    char **ret_key,
    intmax_t *ret_value)
{
    char     *str_value;
    char     *first_digit;
    char     *endptr;
    int       status;

    str_value = NULL;
    status = parse_header(line, ret_key, &str_value);
    if (status != 0)
        return (status);

    first_digit = str_value;
    if ((first_digit[0] == '+') || (first_digit[0] == '-'))
        first_digit++;
    if ((first_digit[0] < '0') || (first_digit[0] > '9'))
        return (-1);

    endptr = NULL;
    errno = 0;
    *ret_value = strtoimax(str_value, &endptr, /* base = */ 10);
    if ((endptr == str_value) || (errno != 0) || (endptr[0] != 0))
        return (-1);

    return (0);
}                       /* }}} int parse_intmax_header */

static int parse_time_value_suffix(
    const char *str_value,
    const char *suffix,
    time_t *ret_value)
{
    const char *first_digit;
    char     *endptr;
    intmax_t  parsed_value;
    time_t    converted_value;

    if ((str_value == NULL) || (suffix == NULL) || (ret_value == NULL))
        return (-1);

    first_digit = str_value;
    if ((first_digit[0] == '+') || (first_digit[0] == '-'))
        first_digit++;
    if ((first_digit[0] < '0') || (first_digit[0] > '9'))
        return (-1);

    errno = 0;
    endptr = NULL;
    parsed_value = strtoimax(str_value, &endptr, /* base = */ 10);
    converted_value = (time_t) parsed_value;
    if ((endptr == str_value) || (errno != 0) ||
        (strcmp(endptr, suffix) != 0) ||
        ((intmax_t) converted_value != parsed_value))
        return (-1);

    *ret_value = converted_value;
    return (0);
}

static int parse_time_value(
    const char *str_value,
    time_t *ret_value)
{
    return parse_time_value_suffix(str_value, "", ret_value);
}

static int parse_char_array_header(
    char *line,         /* {{{ */
    char **ret_key,
    char **array,
    size_t array_len,
    int alloc)
{
    char    **tmp_array;
    char     *value;
    size_t    num;
    int       status;

    if ((array_len == 0) || (array_len > SIZE_MAX / sizeof(*tmp_array)))
        return (-1);

    tmp_array = (char **) malloc(array_len * sizeof(*tmp_array));
    if (tmp_array == NULL)
        return (-1);

    value = NULL;
    status = parse_header(line, ret_key, &value);
    if (status != 0) {
        free(tmp_array);
        return (-1);
    }

    num = strsplit(value, tmp_array, array_len);
    if (num != array_len) {
        free(tmp_array);
        return (-1);
    }

    if (alloc == 0) {
        memcpy(array, tmp_array, array_len * sizeof(char *));
    } else {
        size_t    i;

        for (i = 0; i < array_len; i++) {
            array[i] = strdup(tmp_array[i]);
            if (array[i] == NULL) {
                while (i > 0) {
                    i--;
                    free(array[i]);
                    array[i] = NULL;
                }
                free(tmp_array);
                return (-1);
            }
        }
    }

    free(tmp_array);

    return (0);
}                       /* }}} int parse_char_array_header */

static int parse_value_array_header(
    char *line,         /* {{{ */
    time_t *ret_time,
    rrd_value_t *array,
    size_t array_len)
{
    char     *str_key;
    char    **str_array;
    int       status;
    size_t    i;
    double    tmp;

    if ((array_len == 0) || (array_len > SIZE_MAX / sizeof(*str_array)))
        return (-1);

    str_array = (char **) malloc(array_len * sizeof(*str_array));
    if (str_array == NULL)
        return (-1);

    str_key = NULL;
    status = parse_char_array_header(line, &str_key,
                                     str_array, array_len, /* alloc = */ 0);
    if (status != 0) {
        free(str_array);
        return (-1);
    }

    while (str_key[0] == ' ')
        str_key++;
    if (parse_time_value(str_key, ret_time) != 0) {
        free(str_array);
        return (-1);
    }

    /* Enforce the "C" locale so that parsing of the response is not dependent on
     * the locale. For example, when using a German locale the strtod() function
     * will expect a comma as the decimal separator, i.e. "42,77". */
    for (i = 0; i < array_len; i++) {
        if (rrd_strtodbl(str_array[i], 0, &tmp, "parse_value_array_header") ==
            2) {
            array[i] = (rrd_value_t) tmp;
        } else {
            free(str_array);
            return (-1);
        }
    }

    free(str_array);
    return (0);
}                       /* }}} int parse_value_array_header */

static void close_socket(
    rrd_client_t *client)
{
    if (client->sd >= 0) {
#ifdef _WIN32
        closesocket(client->sd);
        WSACleanup();
#else
        close(client->sd);
#endif
    }

    client->sd = -1;
    client->inbuf = NULL;
    client->inbuf_used = 0;
}

static void close_connection(
    rrd_client_t *client)
{                       /* {{{ */
    if (client == NULL)
        return;

    close_socket(client);

    if (client->sd_path != NULL)
        free(client->sd_path);
    client->sd_path = NULL;
}                       /* }}} void close_connection */

static int buffer_add_string(
    const char *str,    /* {{{ */
    char **buffer_ret,
    size_t *buffer_size_ret)
{
    char     *buffer;
    size_t    buffer_size;
    size_t    buffer_pos;
    size_t    i;
    int       status;

    buffer = *buffer_ret;
    buffer_size = *buffer_size_ret;
    buffer_pos = 0;

    i = 0;
    status = -1;
    while (buffer_pos < buffer_size) {
        if (str[i] == 0) {
            buffer[buffer_pos] = ' ';
            buffer_pos++;
            status = 0;
            break;
        } else if ((str[i] == ' ') || (str[i] == '\\')) {
            if (buffer_pos >= (buffer_size - 1))
                break;
            buffer[buffer_pos] = '\\';
            buffer_pos++;
            buffer[buffer_pos] = str[i];
            buffer_pos++;
        } else {
            buffer[buffer_pos] = str[i];
            buffer_pos++;
        }
        i++;
    }                   /* while (buffer_pos < buffer_size) */

    if (status != 0)
        return (-1);

    *buffer_ret = buffer + buffer_pos;
    *buffer_size_ret = buffer_size - buffer_pos;

    return (0);
}                       /* }}} int buffer_add_string */

/* rrdcached only accepts numeric update times ("<seconds>:<values>").
 * Turn "N:<values>", negative times ("-5:<values>", seconds before now)
 * and at-style times ("<at-time>@<values>", e.g.
 * "now@5" or "epoch+1d@5") into that form here, on the client, with the
 * same rules a local update uses (see parse_ds() and
 * get_time_from_reading() in rrd_update.c): any '@' marks an at-style
 * time, which ends at the first '@'. The time is resolved now, against
 * this host's clock and time zone, just as a local update resolves it
 * when it runs. Returns a newly allocated string, or NULL with the rrd
 * error set. */
char     *rrd_client_numeric_update_value(
    const char *value)
{                       /* {{{ */
    const char *at;
    char     *result;
    char     *end;
    double    offset;

    at = strchr(value, '@');
    if (at != NULL) {
        rrd_time_value_t tv;
        char     *spec, *parse_error;
        time_t    t;

        spec = strdup(value);
        if (spec == NULL) {
            rrd_set_error("out of memory");
            return NULL;
        }
        spec[at - value] = '\0';
        if ((parse_error = rrd_parsetime(spec, &tv)) != NULL) {
            rrd_set_error("ds time: %s: %s", spec, parse_error);
            free(spec);
            return NULL;
        }
        if (tv.type == RELATIVE_TO_END_TIME
            || tv.type == RELATIVE_TO_START_TIME) {
            rrd_set_error("specifying time relative to the 'start' "
                          "or 'end' makes no sense here: %s", spec);
            free(spec);
            return NULL;
        }
        free(spec);
        t = mktime(&tv.tm) + tv.offset;
        result = sprintf_alloc("%lld:%s", (long long) t, at + 1);
    } else if (strncmp(value, "N:", 2) == 0) {
        result = sprintf_alloc("%lu:%s", (unsigned long) time(NULL),
                               value + 2);
    } else if (value[0] == '-'
               && rrd_strtodbl(value, &end, &offset, NULL) == 1
               && *end == ':' && offset < 0) {
        /* a negative time counts back from now */
        double    t = (double) time(NULL) + offset;
        long long sec = (long long) t;
        long      usec;

        if ((double) sec > t)
            sec--;
        usec = (long) ((t - (double) sec) * 1e6 + 0.5);
        if (usec >= 1000000) {
            sec++;
            usec -= 1000000;
        }
        /* not "%f": the decimal point would follow the locale */
        if (usec == 0)
            result = sprintf_alloc("%lld%s", sec, end);
        else
            result = sprintf_alloc("%lld.%06ld%s", sec, usec, end);
    } else {
        result = strdup(value);
    }
    if (result == NULL)
        rrd_set_error("out of memory");
    return result;
}                       /* }}} char *rrd_client_numeric_update_value */

static int buffer_add_ulong(
    const unsigned long value,  /* {{{ */
    char **buffer_ret,
    size_t *buffer_size_ret)
{
    char      temp[RRD_CMD_MAX];

    snprintf(temp, sizeof(temp), "%lu", value);
    temp[sizeof(temp) - 1] = 0;
    return (buffer_add_string(temp, buffer_ret, buffer_size_ret));
}                       /* }}} int buffer_add_ulong */

/* Remove trailing newline (NL) and carriage return (CR) characters. Similar to
 * the Perl function `chomp'. Returns the number of characters that have been
 * removed. */
static int chomp(
    char *str)
{                       /* {{{ */
    size_t    len;
    int       removed;

    if (str == NULL)
        return (-1);

    len = strlen(str);
    removed = 0;
    while ((len > 0) && ((str[len - 1] == '\n') || (str[len - 1] == '\r'))) {
        str[len - 1] = 0;
        len--;
        removed++;
    }

    return (removed);
}                       /* }}} int chomp */

static void response_free(
    rrdc_response_t *res)
{                       /* {{{ */
    if (res == NULL)
        return;

    if (res->lines != NULL) {
        size_t    i;

        for (i = 0; i < res->lines_num; i++)
            if (res->lines[i] != NULL)
                free(res->lines[i]);
        free(res->lines);
    }

    if (res->message != NULL)
        free(res->message);

    free(res);
}                       /* }}} void response_free */

static int recvline(
    rrd_client_t *client,
    char *buf,
    size_t n)
{                       /* {{{ */
    size_t    len;
    char     *s, *p, *t;
#ifdef _WIN32
    int       received;
#else
    ssize_t   received;
#endif

    /* Sanity check */
    if ((client == NULL) || (n <= 0))
        return (-1);

    s = buf;
    n--;                /* leave space for the NULL */
    while (n != 0) {
        /*
         * If the buffer is empty, refill it.
         */
        if (((len = client->inbuf_used) <= 0) || (client->inbuf == NULL)) {
            client->inbuf = client->_inbuf;
            received = recv(client->sd, client->inbuf, RRD_CMD_MAX, 0);
            if (received <= 0) {
                client->inbuf = NULL;
                client->inbuf_used = 0;
                return (-1);
            }
            client->inbuf_used = (size_t) received;
            len = client->inbuf_used;
        }
        p = client->inbuf;
        /*
         * Scan through at most n bytes of the current buffer,
         * looking for '\n'.  If found, copy up to and including
         * newline, and stop.  Otherwise, copy entire chunk
         * and loop.
         */
        if (len > n)
            len = n;
        t = (char *) memchr((void *) p, '\n', len);
        if (t != NULL) {
            len = ++t - p;
            if (memchr((void *) p, 0, len) != NULL)
                return (-1);
            client->inbuf_used -= len;
            client->inbuf = t;
            (void) memcpy((void *) s, (void *) p, len);
            s[len] = 0;
            return (1);
        }
        if (memchr((void *) p, 0, len) != NULL)
            return (-1);
        client->inbuf_used -= len;
        client->inbuf += len;
        (void) memcpy((void *) s, (void *) p, len);
        s += len;
        n -= len;
    }
    *s = 0;
    return (-1);
}                       /* }}} int recvline */

static int response_read(
    rrd_client_t *client,
    rrdc_response_t **ret_response)
{                       /* {{{ */
    rrdc_response_t *ret = NULL;
    int       status = 0;

    char      buffer[RRD_CMD_MAX];
    char     *message_ptr;
    char     *first_digit;
    long      response_status;

    size_t    i;
    size_t    expected_lines;

#define DIE(code) do { status = code; goto err_out; } while(0)

    if ((client == NULL) || (client->sd == -1))
        DIE(-1);

    ret = (rrdc_response_t *) malloc(sizeof(rrdc_response_t));
    if (ret == NULL)
        DIE(-2);
    memset(ret, 0, sizeof(*ret));
    ret->message = NULL;
    ret->lines = NULL;
    ret->lines_num = 0;

    if (recvline(client, buffer, sizeof(buffer)) == -1)
        DIE(-3);

    chomp(buffer);

    first_digit = buffer;
    if ((first_digit[0] == '+') || (first_digit[0] == '-'))
        first_digit++;
    if ((first_digit[0] < '0') || (first_digit[0] > '9'))
        DIE(-4);

    errno = 0;
    response_status = strtol(buffer, &message_ptr, 10);
    if ((buffer == message_ptr) || (errno != 0) ||
        (response_status < INT_MIN) || (response_status > INT_MAX) ||
        ((message_ptr[0] != 0) && (message_ptr[0] != ' ') &&
         (message_ptr[0] != '\t')))
        DIE(-4);
    ret->status = (int) response_status;

    /* Skip leading whitespace of the status message */
    message_ptr += strspn(message_ptr, " \t");

    ret->message = strdup(message_ptr);
    if (ret->message == NULL)
        DIE(-5);

    if (ret->status <= 0) {
        if (ret->status < 0)
            rrd_set_error("rrdcached@%s: %s", client->sd_path, ret->message);
        goto out;
    }

    if ((size_t) ret->status > SIZE_MAX / sizeof(*ret->lines))
        DIE(-6);

    expected_lines = (size_t) ret->status;
    ret->lines = (char **) calloc(expected_lines, sizeof(*ret->lines));
    if (ret->lines == NULL)
        DIE(-6);

    for (i = 0; i < expected_lines; i++) {
        if (recvline(client, buffer, sizeof(buffer)) == -1)
            DIE(-7);

        chomp(buffer);

        ret->lines[ret->lines_num] = strdup(buffer);
        if (ret->lines[ret->lines_num] == NULL)
            DIE(-8);
        ret->lines_num++;
    }

  out:
    *ret_response = ret;
    return (status);

  err_out:
    response_free(ret);
    close_connection(client);
    return (status);

#undef DIE

}                       /* }}} rrdc_response_t *response_read */

static int sendall(
    rrd_client_t *client,   /* {{{ */
    const char *msg,
    size_t len,
    int allow_retry)
{
    int       ret = 0;
    char     *bufp = (char *) msg;

    if (client == NULL)
        return -1;

    while (ret != -1 && len > 0) {
        ret = send(client->sd, bufp, len, 0);
        if (ret > 0) {
            bufp += ret;
            len -= ret;
            allow_retry = 0; // partial read forbids retry
        }
    }

    if ((ret < 0) && allow_retry) {
        /* Try to reconnect and start over.
         * Don't further allow retries to avoid endless loops. */
        if (reconnect(client) == 0)
            return sendall(client, msg, len, 0);
    }

    return ret;
}                       /* }}} int sendall */

static int request(
    rrd_client_t *client,
    const char *buffer,
    size_t buffer_size, /* {{{ */
    rrdc_response_t **ret_response)
{
    int       status;
    rrdc_response_t *res;

    if ((client == NULL) || (client->sd == -1))
        return (ENOTCONN);

    status = sendall(client, buffer, buffer_size, 1);
    if (status == -1) {
        close_connection(client);
        rrd_set_error("request: socket error (%d) while talking to rrdcached",
                      status);
        return (-1);
    }

    res = NULL;
    status = response_read(client, &res);

    if (status != 0) {
        if (status < 0)
            rrd_set_error
                ("request: internal error while talking to rrdcached");
        return (status);
    }

    *ret_response = res;
    return (0);
}                       /* }}} int request */

int rrd_client_is_connected(
    rrd_client_t *client)
{                       /* {{{ */
    return (client != NULL) && (client->sd >= 0);
}                       /* }}} rrd_client_is_connected */

const char *rrd_client_address(
    rrd_client_t *client)
{                       /* {{{ */
    if (!client)
        return NULL;
    return client->sd_path;
}                       /* }}} rrd_client_address */

/* determine whether we are connected to the specified daemon_addr if
 * NULL, return whether we are connected at all
 */
int rrdc_is_connected(
    const char *daemon_addr)
{                       /* {{{ */
    if (default_client.sd < 0)
        return 0;
    else if (daemon_addr == NULL) {
        char     *addr = getenv(ENV_RRDCACHED_ADDRESS);

        /* here we have to handle the case i.e.
         *   UPDATE --daemon ...; UPDATEV (no --daemon) ...
         * In other words: we have a cached connection,
         * but it is not specified in the current command.
         * Daemon is only implied in this case if set in ENV
         */
        if (addr != NULL && strcmp(addr, "") != 0)
            return 1;
        else
            return 0;
    } else if (strcmp(daemon_addr, default_client.sd_path) == 0)
        return 1;
    else
        return 0;
}                       /* }}} int rrdc_is_connected */

/* determine whether we are connected to any daemon */
int rrdc_is_any_connected(
    void)
{
    return default_client.sd >= 0;
}

int rrd_client_ping(
    rrd_client_t *client)
{                       /* {{{ */
    int       status;
    rrdc_response_t *res = NULL;

    status = request(client, "PING\n", strlen("PING\n"), &res);
    if (status != 0)
        return 0;

    status = res->status;
    response_free(res);

    return status == 0;
}                       /* }}} int rrd_client_ping */

int rrdc_ping(
    void)
{                       /* {{{ */
    int       status;

    mutex_lock(&lock);
    status = rrd_client_ping(&default_client);
    mutex_unlock(&lock);
    return status;
}                       /* }}} int rrdc_ping */

static int connect_unix(
    rrd_client_t *client,
    const char *path)
{                       /* {{{ */
#ifdef _WIN32
    /* Silence unused parameter compiler warnings */
    (void) client;
    (void) path;
    return (WSAEPROTONOSUPPORT);
#else
    struct sockaddr_un sa;
    int       status;

    assert(path != NULL);
    assert(client->sd == -1);

    /* a longer path would be cut short and name a different socket */
    if (strlen(path) >= sizeof(sa.sun_path)) {
        rrd_set_error("socket path '%s' is too long (%lu bytes, at most "
                      "%lu allowed)", path, (unsigned long) strlen(path),
                      (unsigned long) (sizeof(sa.sun_path) - 1));
        return (-1);
    }

    client->sd = socket(PF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, /* protocol = */ 0);
    if (client->sd < 0) {
        status = errno;
        return (status);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, path, sizeof(sa.sun_path) - 1);

    status = connect(client->sd, (struct sockaddr *) &sa, sizeof(sa));
    if (status != 0) {
        status = errno;
        close_connection(client);
        return (status);
    }

    return (0);
#endif
}                       /* }}} int connect_unix */

static int connect_network(
    rrd_client_t *client,
    const char *addr_orig)
{                       /* {{{ */
    struct addrinfo ai_hints;
    struct addrinfo *ai_res;
    struct addrinfo *ai_ptr;
    char      addr_copy[NI_MAXHOST];
    char     *addr;
    char     *port;

    assert(addr_orig != NULL);
    assert(client->sd == -1);

    strncpy(addr_copy, addr_orig, sizeof(addr_copy));
    addr_copy[sizeof(addr_copy) - 1] = '\0';
    addr = addr_copy;

    int       status;

    memset(&ai_hints, 0, sizeof(ai_hints));
    ai_hints.ai_flags = 0;
#ifdef AI_ADDRCONFIG
    ai_hints.ai_flags |= AI_ADDRCONFIG;
#endif
    ai_hints.ai_family = AF_UNSPEC;
    ai_hints.ai_socktype = SOCK_STREAM;

    port = NULL;
    if (*addr == '[') { /* IPv6+port format */
        /* `addr' is something like "[2001:780:104:2:211:24ff:feab:26f8]:12345" */
        addr++;

        port = strchr(addr, ']');
        if (port == NULL) {
            rrd_set_error("malformed address: %s", addr_orig);
            return (-1);
        }
        *port = 0;
        port++;

        if (*port == ':')
            port++;
        else if (*port == 0)
            port = NULL;
        else {
            rrd_set_error("garbage after address: %s", port);
            return (-1);
        }
    }                   /* if (*addr == '[') */
    else {
        port = strrchr(addr, ':');
        if (port != NULL) {
            *port = 0;
            port++;
        }
    }

#ifdef _WIN32
    WORD      wVersionRequested;
    WSADATA   wsaData;

    wVersionRequested = MAKEWORD(2, 0);
    status = WSAStartup(wVersionRequested, &wsaData);
    if (status != 0) {
        rrd_set_error("failed to initialize socket library %d", status);
        return (-1);
    }
#endif

    ai_res = NULL;
    status = getaddrinfo(addr,
                         port == NULL ? RRDCACHED_DEFAULT_PORT : port,
                         &ai_hints, &ai_res);
    if (status != 0) {
        rrd_set_error("failed to resolve address '%s' (port %s): %s (%d)",
                      addr, port == NULL ? RRDCACHED_DEFAULT_PORT : port,
                      gai_strerror(status), status);
        return (-1);
    }

    for (ai_ptr = ai_res; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
        client->sd =
            socket(ai_ptr->ai_family, ai_ptr->ai_socktype,
                   ai_ptr->ai_protocol);
        if (client->sd < 0) {
            status = errno;
            client->sd = -1;
            continue;
        }

        status = connect(client->sd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
        if (status != 0) {
            status = errno;
            close_connection(client);
            continue;
        }
        assert(status == 0);
        break;
    }                   /* for (ai_ptr) */

    freeaddrinfo(ai_res);

    return (status);
}                       /* }}} int connect_network */

static int client_connect(
    rrd_client_t *client,
    const char *addr)
{                       /* {{{ */
    rrd_clear_error();
    if (strncmp("unix:", addr, strlen("unix:")) == 0)
        return connect_unix(client, addr + strlen("unix:"));
    else if (addr[0] == '/')
        return connect_unix(client, addr);
    return connect_network(client, addr);
}                       /* }}} int client_connect */

static int reconnect(
    rrd_client_t *client)
{                       /* {{{ */
    if ((client == NULL) || (client->sd_path == NULL))
        return -1;

    close_socket(client);
    return client_connect(client, client->sd_path);
}                       /* }}} int reconnect */

int rrd_client_connect(
    rrd_client_t *client,
    const char *addr)
{                       /* {{{ */
    int       status = 0;

    if (addr == NULL) {
        addr = getenv(ENV_RRDCACHED_ADDRESS);
    }

    if ((client == NULL) || (addr == NULL) || (strcmp(addr, "") == 0)) {
        return 0;
    }

    if ((client->sd >= 0) && (client->sd_path != NULL)
        && (strcmp(addr, client->sd_path) == 0)) {
        /* connection to the same daemon; use existing connection */
        return (0);
    } else {
        close_connection(client);
    }

    status = client_connect(client, addr);

    if ((status == 0) && (client->sd >= 0))
        client->sd_path = strdup(addr);
    else {
        char     *err = rrd_test_error()? rrd_get_error() : "Internal error";

        /* err points the string that gets written to by rrd_set_error(), thus we
         * cannot pass it to that function */
        err = strdup(err);
        rrd_set_error("Unable to connect to rrdcached: %s", (status < 0)
                      ? (err ? err : "Internal error")
                      : rrd_strerror(status));
        if (err != NULL)
            free(err);
    }

    return (status);
}                       /* }}} rrd_client_connect */

int rrdc_connect(
    const char *addr)
{                       /* {{{ */
    int       status;

    mutex_lock(&lock);
    status = rrd_client_connect(&default_client, addr);
    mutex_unlock(&lock);
    return status;
}                       /* }}} int rrdc_connect */

int rrd_client_disconnect(
    rrd_client_t *client)
{                       /* {{{ */
    close_connection(client);
    return 0;
}                       /* }}} int rrd_client_disconnect */

int rrdc_disconnect(
    void)
{                       /* {{{ */
    mutex_lock(&lock);
    close_connection(&default_client);
    mutex_unlock(&lock);
    return 0;
}                       /* }}} int rrdc_disconnect */

rrd_client_t *rrd_client_new(
    const char *addr)
{                       /* {{{ */
    rrd_client_t *client;

    client = calloc(1, sizeof(*client));
    if (client == NULL)
        return NULL;

    client->sd = -1;

    if (addr == NULL)
        return client;

    if (rrd_client_connect(client, addr) != 0) {
        rrd_client_destroy(client);
        return NULL;
    }

    return client;
}                       /* }}} rrd_client_t rrd_client_new */

void rrd_client_destroy(
    rrd_client_t *client)
{                       /* {{{ */
    if (client == NULL)
        return;

    close_connection(client);
    free(client);
}                       /* }}} void rrd_client_destroy */

/* Send one UPDATE command assembled in buffer (RRD_CMD_MAX bytes, of
 * which buffer_free are unused) and return the daemon's status. */
static int send_update_buffer(
    rrd_client_t *client,
    char *buffer,
    size_t buffer_free)
{                       /* {{{ */
    size_t    buffer_size;
    rrdc_response_t *res;
    int       status;

    assert(buffer_free < RRD_CMD_MAX);
    buffer_size = RRD_CMD_MAX - buffer_free;
    assert(buffer[buffer_size - 1] == ' ');
    buffer[buffer_size - 1] = '\n';

    res = NULL;
    status = request(client, buffer, buffer_size, &res);
    if (status != 0)
        return (status);

    status = res->status;
    response_free(res);
    return (status);
}                       /* }}} int send_update_buffer */

int rrd_client_update(
    rrd_client_t *client,
    const char *filename,   /* {{{ */
    int values_num,
    const char *const *values)
{
    char      buffer[RRD_CMD_MAX];
    char     *buffer_ptr;
    size_t    buffer_free;
    size_t    header_free;
    int       status;
    int       i;
    int       queued;
    int       convert_failed;
    char     *file_path;
    char     *value;

    if (client == NULL)
        return -1;

    memset(buffer, 0, sizeof(buffer));
    buffer_ptr = &buffer[0];
    buffer_free = sizeof(buffer);

    status = buffer_add_string("update", &buffer_ptr, &buffer_free);
    if (status != 0)
        return (ENOBUFS);

    file_path = get_path(client, filename);
    if (file_path == NULL) {
        return (-1);
    }

    status = buffer_add_string(file_path, &buffer_ptr, &buffer_free);
    free(file_path);

    if (status != 0) {
        return (ENOBUFS);
    }

    /* A command must fit into RRD_CMD_MAX bytes: that is what rrdcached
     * keeps of it for its journal, and a line longer than its read
     * buffer (2 * RRD_CMD_MAX) makes it drop the connection. So send
     * long value lists as several UPDATE commands for the same file. */
    header_free = buffer_free;
    queued = 0;
    convert_failed = 0;
    status = 0;
    for (i = 0; i < values_num; i++) {
        value = rrd_client_numeric_update_value(values[i]);
        if (value == NULL) {
            /* like a local update, keep the values before the bad one */
            convert_failed = 1;
            break;
        }
        status = buffer_add_string(value, &buffer_ptr, &buffer_free);
        if (status != 0 && queued > 0) {
            status = send_update_buffer(client, buffer, buffer_free);
            buffer_ptr = buffer + sizeof(buffer) - header_free;
            buffer_free = header_free;
            queued = 0;
            if (status == 0)
                status = buffer_add_string(value, &buffer_ptr, &buffer_free);
            else {
                free(value);
                return (status);
            }
        }
        free(value);
        if (status != 0)
            return (ENOBUFS);
        queued++;
    }

    if (queued > 0 || values_num == 0)
        status = send_update_buffer(client, buffer, buffer_free);
    if (status == 0 && convert_failed)
        status = -1;

    return (status);
}                       /* int rrd_client_update */

int rrdc_update(
    const char *filename,
    int values_num,     /* {{{ */
    const char *const *values)
{
    int       status;

    mutex_lock(&lock);
    status = rrd_client_update(&default_client, filename, values_num, values);
    mutex_unlock(&lock);
    return status;
}                       /* }}} int rrdc_update */

static int filebased_command(
    rrd_client_t *client,   /* {{{ */
    const char *command,
    const char *filename)
{
    char      buffer[RRD_CMD_MAX];
    char     *buffer_ptr;
    size_t    buffer_free;
    size_t    buffer_size;
    rrdc_response_t *res;
    int       status;
    char     *file_path;

    if ((client == NULL) || (filename == NULL))
        return (-1);

    memset(buffer, 0, sizeof(buffer));
    buffer_ptr = &buffer[0];
    buffer_free = sizeof(buffer);

    status = buffer_add_string(command, &buffer_ptr, &buffer_free);
    if (status != 0)
        return (ENOBUFS);

    file_path = get_path(client, filename);
    if (file_path == NULL) {
        return (-1);
    }

    status = buffer_add_string(file_path, &buffer_ptr, &buffer_free);
    free(file_path);

    if (status != 0) {
        return (ENOBUFS);
    }

    assert(buffer_free < sizeof(buffer));
    buffer_size = sizeof(buffer) - buffer_free;
    assert(buffer[buffer_size - 1] == ' ');
    buffer[buffer_size - 1] = '\n';

    res = NULL;
    status = request(client, buffer, buffer_size, &res);

    if (status != 0)
        return (status);

    status = res->status;
    response_free(res);
    return (status);
}                       /* }}} int rrdc_flush */

int rrd_client_flush(
    rrd_client_t *client,
    const char *filename)
{
    return filebased_command(client, "flush", filename);
}

int rrdc_flush(
    const char *filename)
{
    int       status;

    mutex_lock(&lock);
    status = rrd_client_flush(&default_client, filename);
    mutex_unlock(&lock);
    return status;
}

int rrd_client_forget(
    rrd_client_t *client,
    const char *filename)
{
    return filebased_command(client, "forget", filename);
}

int rrdc_forget(
    const char *filename)
{
    int       status;

    mutex_lock(&lock);
    status = rrd_client_forget(&default_client, filename);
    mutex_unlock(&lock);
    return status;
}

int rrd_client_flushall(
    rrd_client_t *client)
{                       /* {{{ */
    char      buffer[RRD_CMD_MAX];
    char     *buffer_ptr;
    size_t    buffer_free;
    size_t    buffer_size;
    rrdc_response_t *res;
    int       status;

    memset(buffer, 0, sizeof(buffer));
    buffer_ptr = &buffer[0];
    buffer_free = sizeof(buffer);

    status = buffer_add_string("flushall", &buffer_ptr, &buffer_free);

    if (status != 0)
        return (ENOBUFS);

    assert(buffer_free < sizeof(buffer));
    buffer_size = sizeof(buffer) - buffer_free;
    assert(buffer[buffer_size - 1] == ' ');
    buffer[buffer_size - 1] = '\n';

    res = NULL;
    status = request(client, buffer, buffer_size, &res);

    if (status != 0)
        return (status);

    status = res->status;
    response_free(res);

    return (status);
}                       /* }}} int rrd_client_flushall */

int rrdc_flushall(
    void)
{                       /* {{{ */
    int       status;

    mutex_lock(&lock);
    status = rrd_client_flushall(&default_client);
    mutex_unlock(&lock);
    return status;
}                       /* }}} int rrdc_flushall */

rrd_info_t *rrd_client_info(
    rrd_client_t *client,
    const char *filename)
{                       /* {{{ */
    char      buffer[RRD_CMD_MAX];
    char     *buffer_ptr;
    size_t    buffer_free;
    size_t    buffer_size;
    rrdc_response_t *res;
    int       status;
    char     *file_path;
    rrd_info_t *data = NULL, *cd;
    rrd_infoval_t info;
    unsigned int l;
    rrd_info_type_t itype;
    char     *k, *s;

    if (client == NULL)
        return NULL;
    if (filename == NULL) {
        rrd_set_error("rrdc_info: no filename");
        return (NULL);
    }

    memset(buffer, 0, sizeof(buffer));
    buffer_ptr = &buffer[0];
    buffer_free = sizeof(buffer);

    status = buffer_add_string("info", &buffer_ptr, &buffer_free);
    if (status != 0) {
        rrd_set_error("rrdc_info: out of memory");
        return (NULL);
    }

    file_path = get_path(client, filename);
    if (file_path == NULL) {
        return (NULL);
    }

    status = buffer_add_string(file_path, &buffer_ptr, &buffer_free);
    free(file_path);

    if (status != 0) {
        rrd_set_error("rrdc_info: out of memory");
        return (NULL);
    }

    assert(buffer_free < sizeof(buffer));
    buffer_size = sizeof(buffer) - buffer_free;
    assert(buffer[buffer_size - 1] == ' ');
    buffer[buffer_size - 1] = '\n';

    res = NULL;
    status = request(client, buffer, buffer_size, &res);

    if (status != 0 || res->status < 0)
        goto out_free_res;

    data = cd = NULL;
    for (l = 0; l < res->lines_num; l++) {
        char     *sep, *endp;
        long      tcode;

        /* each line is "<key> <type code> <value>"; the value may be
         * empty for string fields only (#1080). Anything else is a
         * protocol error: fail instead of returning a truncated info. */
        k = res->lines[l];
        sep = strchr(k, ' ');
        if (sep == NULL || sep == k)
            goto malformed;
        *sep = 0;
        s = sep + 1;
        errno = 0;
        tcode = strtol(s, &endp, 10);
        if (endp == s || errno != 0 || (*endp != ' ' && *endp != 0)) {
            *sep = ' ';
            goto malformed;
        }
        if ((tcode < RD_I_VAL) || (tcode > RD_I_BLO)) {
            *sep = ' ';
            goto malformed;
        }
        itype = (rrd_info_type_t) tcode;
        s = (*endp == ' ') ? endp + 1 : endp;
        if (*s == 0 && itype != RD_I_STR) {
            *sep = ' ';
            goto malformed;
        }
        /* finally, we're pointing to the value */
        switch (itype) {
        case RD_I_VAL:
            if (strcmp(s, "NaN") == 0) {
                info.u_val = DNAN;
            } else if (rrd_strtodbl(s, NULL, &info.u_val, NULL) != 2)
                goto malformed_value;
            break;
        case RD_I_CNT: {
            char     *value_end;

            if ((s[0] < '0') || (s[0] > '9'))
                goto malformed_value;
            errno = 0;
            info.u_cnt = strtoul(s, &value_end, 10);
            if ((errno != 0) || (value_end[0] != 0))
                goto malformed_value;
            break;
        }
        case RD_I_INT: {
            char     *value_end;
            char     *first_digit = s;
            long      value;

            if ((first_digit[0] == '+') || (first_digit[0] == '-'))
                first_digit++;
            if ((first_digit[0] < '0') || (first_digit[0] > '9'))
                goto malformed_value;
            errno = 0;
            value = strtol(s, &value_end, 10);
            if ((errno != 0) || (value_end[0] != 0) ||
                (value < INT_MIN) || (value > INT_MAX))
                goto malformed_value;
            info.u_int = (int) value;
            break;
        }
        case RD_I_STR:
            chomp(s);
            info.u_str = strdup(s);
            if (info.u_str == NULL) {
                rrd_set_error("rrdc_info: out of memory");
                goto out_free_data;
            }
            break;
        case RD_I_BLO:
            rrd_set_error("rrdc_info: BLOB objects are not supported");
            goto out_free_data;
        default:
            rrd_set_error("rrdc_info: Unsupported info type %d", itype);
            goto out_free_data;
        }

        cd = rrd_info_push(cd, sprintf_alloc("%s", k), itype, info);
        if (!data)
            data = cd;
        if (itype == RD_I_STR) {
            free(info.u_str);
            info.u_str = NULL;
        }
        continue;

      malformed_value:
        *sep = ' ';
        goto malformed;
    }
    goto out_free_res;

  malformed:
    rrd_set_error("rrdc_info: malformed line %u in reply from rrdcached: "
                  "'%s'", l + 1, res->lines[l]);
  out_free_data:
    rrd_info_free(data);
    data = NULL;
    close_connection(client);

  out_free_res:
    response_free(res);

    return (data);
}                       /* }}} rrd_client_info */

rrd_info_t *rrdc_info(
    const char *filename)
{                       /* {{{ */
    rrd_info_t *info;

    mutex_lock(&lock);
    info = rrd_client_info(&default_client, filename);
    mutex_unlock(&lock);
    return info;
}                       /* }}} int rrdc_info */

char     *rrd_client_list(
    rrd_client_t *client,
    int recursive,
    const char *dirname)
{                       /* {{{ */
    char      buffer[RRD_CMD_MAX];
    char     *buffer_ptr;
    size_t    buffer_free;
    size_t    buffer_size;
    rrdc_response_t *res;
    int       status;
    size_t    i;
    char     *list = NULL;
    size_t    list_len = 0;

    if (client == NULL)
        return NULL;
    if (dirname == NULL) {
        rrd_set_error("rrdc_info: no directory name");
        return (NULL);
    }

    memset(buffer, 0, sizeof(buffer));
    buffer_ptr = &buffer[0];
    buffer_free = sizeof(buffer);

    status = buffer_add_string("list", &buffer_ptr, &buffer_free);

    if (status != 0) {
        rrd_set_error("rrdc_list: out of memory");
        return NULL;
    }

    if (recursive) {
        status = buffer_add_string("RECURSIVE", &buffer_ptr, &buffer_free);
        if (status != 0) {
            rrd_set_error("rrdc_list: out of memory");
            return (NULL);
        }
    }

    status = buffer_add_string(dirname, &buffer_ptr, &buffer_free);
    if (status != 0) {
        rrd_set_error("rrdc_list: out of memory");
        return (NULL);
    }


    assert(buffer_free < sizeof(buffer));
    buffer_size = sizeof(buffer) - buffer_free;
    assert(buffer[buffer_size - 1] == ' ');
    buffer[buffer_size - 1] = '\n';

    res = NULL;
    status = request(client, buffer, buffer_size, &res);

    if (status != 0 || res->status < 0)
        goto out_free_res;

    /* Handle the case where the list is empty, allocate
     * a single byte zeroed string.
     */
    if (res->lines_num == 0) {
        list = calloc(1, 1);

        if (!list) {
            rrd_set_error("rrdc_list: out of memory");
            goto out_free_res;
        }

        goto out_free_res;
    }

    for (i = 0; i < res->lines_num; i++) {
        size_t    len;
        char     *buf;

        len = strlen(res->lines[i]);
        if ((len > SIZE_MAX - 2) || (list_len > SIZE_MAX - len - 2)) {
            rrd_set_error("rrdc_list: response is too large");
            free(list);
            list = NULL;
            goto out_free_res;
        }
        buf = realloc(list, list_len + len + 2);

        if (!buf) {
            rrd_set_error("rrdc_list: out of memory");

            if (list) {
                free(list);
                list = NULL;
            }

            goto out_free_res;
        }

        memcpy(buf + list_len, res->lines[i], len);
        list_len += len;
        buf[list_len++] = '\n';
        buf[list_len] = 0;
        list = buf;
    }

  out_free_res:
    response_free(res);

    return list;
}                       /* }}} char *rrd_client_list */

char     *rrdc_list(
    int recursive,
    const char *dirname)
{                       /* {{{ */
    char     *files;

    mutex_lock(&lock);
    files = rrd_client_list(&default_client, recursive, dirname);
    mutex_unlock(&lock);
    return files;
}                       /* }}} char *rrdc_list */

time_t rrd_client_last(
    rrd_client_t *client,
    const char *filename)
{                       /* {{{ */
    char      buffer[RRD_CMD_MAX];
    char     *buffer_ptr;
    size_t    buffer_free;
    size_t    buffer_size;
    rrdc_response_t *res;
    int       status;
    char     *file_path;
    time_t    lastup;

    if (client == NULL)
        return 0;
    if (filename == NULL) {
        rrd_set_error("rrdc_last: no filename");
        return (-1);
    }

    memset(buffer, 0, sizeof(buffer));
    buffer_ptr = &buffer[0];
    buffer_free = sizeof(buffer);

    status = buffer_add_string("last", &buffer_ptr, &buffer_free);
    if (status != 0) {
        rrd_set_error("rrdc_last: out of memory");
        return (-1);
    }

    file_path = get_path(client, filename);
    if (file_path == NULL) {
        return (-1);
    }

    status = buffer_add_string(file_path, &buffer_ptr, &buffer_free);
    free(file_path);

    if (status != 0) {
        rrd_set_error("rrdc_last: out of memory");
        return (-1);
    }

    assert(buffer_free < sizeof(buffer));
    buffer_size = sizeof(buffer) - buffer_free;
    assert(buffer[buffer_size - 1] == ' ');
    buffer[buffer_size - 1] = '\n';

    res = NULL;
    status = request(client, buffer, buffer_size, &res);

    if (status != 0)
        return (-1);
    if (res->status < 0) {
        response_free(res);
        return (-1);
    }
    if (res->status != 0) {
        rrd_set_error("rrdc_last: unexpected response body");
        response_free(res);
        close_connection(client);
        return (-1);
    }

    if (parse_time_value(res->message, &lastup) != 0) {
        rrd_set_error("rrdc_last: malformed response from rrdcached");
        response_free(res);
        close_connection(client);
        return (-1);
    }
    response_free(res);

    return (lastup);
}                       /* }}} int rrd_client_last */

time_t rrdc_last(
    const char *filename)
{                       /* {{{ */
    time_t    t;

    mutex_lock(&lock);
    t = rrd_client_last(&default_client, filename);
    mutex_unlock(&lock);
    return t;
}                       /* }}} int rrdc_last */

int rrd_client_lastupdate(
    rrd_client_t *client,
    const char *filename,   /* {{{ */
    time_t *ret_last_update,
    unsigned long *ret_ds_count,
    char ***ret_ds_names,
    char ***ret_last_ds)
{
    char      buffer[RRD_CMD_MAX];
    char     *buffer_ptr;
    size_t    buffer_free;
    size_t    buffer_size;
    rrdc_response_t *res;
    int       status;
    char     *file_path;
    unsigned long ds_count;
    size_t    i;

    if ((client == NULL) || (ret_last_update == NULL) ||
        (ret_ds_count == NULL) || (ret_ds_names == NULL) ||
        (ret_last_ds == NULL))
        return -1;
    *ret_ds_count = 0;
    *ret_ds_names = NULL;
    *ret_last_ds = NULL;
    *ret_last_update = 0;
    if (filename == NULL) {
        rrd_set_error("rrdc_lastupdate: no filename");
        return (-1);
    }

    memset(buffer, 0, sizeof(buffer));
    buffer_ptr = &buffer[0];
    buffer_free = sizeof(buffer);

    status = buffer_add_string("lastupdate", &buffer_ptr, &buffer_free);
    if (status != 0) {
        rrd_set_error("rrdc_lastupdate: out of memory");
        return (-1);
    }

    file_path = get_path(client, filename);
    if (file_path == NULL) {
        return (-1);
    }

    status = buffer_add_string(file_path, &buffer_ptr, &buffer_free);
    free(file_path);

    if (status != 0) {
        rrd_set_error("rrdc_lastupdate: out of memory");
        return (-1);
    }

    assert(buffer_free < sizeof(buffer));
    buffer_size = sizeof(buffer) - buffer_free;
    assert(buffer[buffer_size - 1] == ' ');
    buffer[buffer_size - 1] = '\n';

    res = NULL;
    status = request(client, buffer, buffer_size, &res);

    if (status != 0)
        return (-1);
    if (res->status < 0) {
        response_free(res);
        return (-1);
    }
    /* The status line message contains the timestamp */
    if (parse_time_value_suffix(res->message, " LastUpdate values follow",
                                ret_last_update) != 0) {
        rrd_set_error("rrdc_lastupdate: malformed timestamp in response");
        response_free(res);
        close_connection(client);
        return (-1);
    }
    ds_count = (unsigned long) res->lines_num;

    if ((ds_count == 0) ||
        ((uintmax_t) ds_count > SIZE_MAX / sizeof(char *))) {
        rrd_set_error("rrdc_lastupdate: invalid data-source count in "
                      "response");
        response_free(res);
        close_connection(client);
        return (-1);
    }

    *ret_ds_names = (char **) calloc(ds_count, sizeof(char *));
    if (*ret_ds_names == NULL) {
        rrd_set_error("rrdc_lastupdate: out of memory");
        response_free(res);
        return (-1);
    }

    *ret_last_ds = (char **) calloc(ds_count, sizeof(char *));
    if (*ret_last_ds == NULL) {
        rrd_set_error("rrdc_lastupdate: out of memory");
        free(*ret_ds_names);
        *ret_ds_names = NULL;
        response_free(res);
        return (-1);
    }

    /* Each response line has the format: "<dsname> <lastvalue>" */
    for (i = 0; i < ds_count; i++) {
        char     *line = res->lines[i];
        char     *space = strchr(line, ' ');

        if ((space == NULL) || (space == line) || (space[1] == 0)) {
            rrd_set_error("rrdc_lastupdate: malformed response line: %s",
                          line);
            /* free what we've allocated so far */
            for (size_t j = 0; j < i; j++) {
                free((*ret_ds_names)[j]);
                free((*ret_last_ds)[j]);
            }
            free(*ret_ds_names);
            *ret_ds_names = NULL;
            free(*ret_last_ds);
            *ret_last_ds = NULL;
            response_free(res);
            close_connection(client);
            return (-1);
        }
        *space = '\0';
        (*ret_ds_names)[i] = strdup(line);
        (*ret_last_ds)[i]  = strdup(space + 1);

        if ((*ret_ds_names)[i] == NULL || (*ret_last_ds)[i] == NULL) {
            rrd_set_error("rrdc_lastupdate: out of memory");
            for (size_t j = 0; j <= i; j++) {
                if ((*ret_ds_names)[j]) free((*ret_ds_names)[j]);
                if ((*ret_last_ds)[j])  free((*ret_last_ds)[j]);
            }
            free(*ret_ds_names);
            *ret_ds_names = NULL;
            free(*ret_last_ds);
            *ret_last_ds = NULL;
            response_free(res);
            close_connection(client);
            return (-1);
        }
    }

    *ret_ds_count = ds_count;
    response_free(res);
    return (0);
}                       /* }}} int rrd_client_lastupdate */

int rrdc_lastupdate(
    const char *filename,   /* {{{ */
    time_t *ret_last_update,
    unsigned long *ret_ds_count,
    char ***ret_ds_names,
    char ***ret_last_ds)
{
    int       status;

    mutex_lock(&lock);
    status = rrd_client_lastupdate(&default_client, filename,
                                   ret_last_update, ret_ds_count,
                                   ret_ds_names, ret_last_ds);
    mutex_unlock(&lock);
    return status;
}                       /* }}} int rrdc_lastupdate */

time_t rrd_client_first(
    rrd_client_t *client,
    const char *filename,
    int rraindex)
{                       /* {{{ */
    char      buffer[RRD_CMD_MAX];
    char     *buffer_ptr;
    size_t    buffer_free;
    size_t    buffer_size;
    rrdc_response_t *res;
    int       status;
    char     *file_path;
    time_t    firstup;

    if (client == NULL)
        return 0;
    if (filename == NULL) {
        rrd_set_error("rrdc_first: no filename specified");
        return (-1);
    }

    memset(buffer, 0, sizeof(buffer));
    buffer_ptr = &buffer[0];
    buffer_free = sizeof(buffer);

    status = buffer_add_string("first", &buffer_ptr, &buffer_free);
    if (status != 0) {
        rrd_set_error("rrdc_first: out of memory");
        return (-1);
    }

    file_path = get_path(client, filename);
    if (file_path == NULL) {
        return (-1);
    }

    status = buffer_add_string(file_path, &buffer_ptr, &buffer_free);
    free(file_path);

    if (status != 0) {
        rrd_set_error("rrdc_first: out of memory");
        return (-1);
    }
    status = buffer_add_ulong(rraindex, &buffer_ptr, &buffer_free);
    if (status != 0) {
        rrd_set_error("rrdc_first: out of memory");
        return (-1);
    }

    assert(buffer_free < sizeof(buffer));
    buffer_size = sizeof(buffer) - buffer_free;
    assert(buffer[buffer_size - 1] == ' ');
    buffer[buffer_size - 1] = '\n';

    res = NULL;
    status = request(client, buffer, buffer_size, &res);

    if (status != 0)
        return (-1);
    if (res->status < 0) {
        response_free(res);
        return (-1);
    }
    if (res->status != 0) {
        rrd_set_error("rrdc_first: unexpected response body");
        response_free(res);
        close_connection(client);
        return (-1);
    }

    if (parse_time_value(res->message, &firstup) != 0) {
        rrd_set_error("rrdc_first: malformed response from rrdcached");
        response_free(res);
        close_connection(client);
        return (-1);
    }
    response_free(res);

    return (firstup);
}                       /* }}} int rrd_client_first */

time_t rrdc_first(
    const char *filename,
    int rraindex)
{                       /* {{{ */
    time_t    t;

    mutex_lock(&lock);
    t = rrd_client_first(&default_client, filename, rraindex);
    mutex_unlock(&lock);
    return t;
}                       /* }}} int rrdc_first */

int rrd_client_create(
    rrd_client_t *client,
    const char *filename,   /* {{{ */
    unsigned long pdp_step,
    time_t last_up,
    int no_overwrite,
    int argc,
    const char **argv)
{
    return rrd_client_create_r2(client, filename, pdp_step, last_up,
                                no_overwrite, NULL, NULL, argc, argv);
}

int rrdc_create(
    const char *filename,   /* {{{ */
    unsigned long pdp_step,
    time_t last_up,
    int no_overwrite,
    int argc,
    const char **argv)
{
    return rrdc_create_r2(filename, pdp_step, last_up, no_overwrite,
                          NULL, NULL, argc, argv);
}

int rrd_client_create_r2(
    rrd_client_t *client,
    const char *filename,   /* {{{ */
    unsigned long pdp_step,
    time_t last_up,
    int no_overwrite,
    const char **sources,
    const char *template,
    int argc,
    const char **argv)
{
    char      buffer[RRD_CMD_MAX];
    char     *buffer_ptr;
    size_t    buffer_free;
    size_t    buffer_size;
    rrdc_response_t *res;
    int       status;
    char     *file_path;
    int       i;

    if (client == NULL)
        return -1;
    if (filename == NULL) {
        rrd_set_error("rrdc_create: no filename specified");
        return (-1);
    }

    memset(buffer, 0, sizeof(buffer));
    buffer_ptr = &buffer[0];
    buffer_free = sizeof(buffer);

    status = buffer_add_string("create", &buffer_ptr, &buffer_free);
    if (status != 0) {
        rrd_set_error("rrdc_create: out of memory");
        return (-1);
    }

    file_path = get_path(client, filename);
    if (file_path == NULL) {
        return (-1);
    }

    status = buffer_add_string(file_path, &buffer_ptr, &buffer_free);
    free(file_path);

    if (last_up >= 0) {
        status = buffer_add_string("-b", &buffer_ptr, &buffer_free);
        status = buffer_add_ulong(last_up, &buffer_ptr, &buffer_free);
    }
    status = buffer_add_string("-s", &buffer_ptr, &buffer_free);
    status = buffer_add_ulong(pdp_step, &buffer_ptr, &buffer_free);
    if (no_overwrite) {
        status = buffer_add_string("-O", &buffer_ptr, &buffer_free);
    }

    if (sources != NULL) {
        for (const char **p = sources; *p; p++) {
            status = buffer_add_string("-r", &buffer_ptr, &buffer_free);
            status = buffer_add_string(*p, &buffer_ptr, &buffer_free);
        }
    }

    if (template != NULL) {
        status = buffer_add_string("-t", &buffer_ptr, &buffer_free);
        status = buffer_add_string(template, &buffer_ptr, &buffer_free);
    }

    if (status != 0) {
        rrd_set_error("rrdc_create: out of memory");
        return (-1);
    }

    for (i = 0; i < argc; i++) {
        if (argv[i]) {
            status = buffer_add_string(argv[i], &buffer_ptr, &buffer_free);
            if (status != 0) {
                rrd_set_error("rrdc_create: out of memory");
                return (-1);
            }
        }
    }

    /* buffer ready to send? */
    assert(buffer_free < sizeof(buffer));
    buffer_size = sizeof(buffer) - buffer_free;
    assert(buffer[buffer_size - 1] == ' ');
    buffer[buffer_size - 1] = '\n';

    res = NULL;
    status = request(client, buffer, buffer_size, &res);

    if (status != 0)
        return (-1);

    status = res->status;
    response_free(res);
    return (status);
}                       /* }}} int rrd_client_create_r2 */

int rrdc_create_r2(
    const char *filename,   /* {{{ */
    unsigned long pdp_step,
    time_t last_up,
    int no_overwrite,
    const char **sources,
    const char *template,
    int argc,
    const char **argv)
{
    int       status;

    mutex_lock(&lock);
    status =
        rrd_client_create_r2(&default_client, filename, pdp_step, last_up,
                             no_overwrite, sources, template, argc, argv);
    mutex_unlock(&lock);
    return status;
}                       /* }}} int rrdc_create_r2 */

int rrd_client_fetch(
    rrd_client_t *client,
    const char *filename,   /* {{{ */
    const char *cf,
    time_t *ret_start,
    time_t *ret_end,
    unsigned long *ret_step,
    unsigned long *ret_ds_num,
    char ***ret_ds_names,
    rrd_value_t **ret_data)
{
    char      buffer[RRD_CMD_MAX];
    char     *buffer_ptr;
    size_t    buffer_free;
    size_t    buffer_size;
    rrdc_response_t *res;
    char     *file_path;

    char     *str_tmp;
    uintmax_t flush_version;

    time_t    start;
    time_t    end;
    unsigned long step;
    unsigned long ds_num;
    char    **ds_names;

    rrd_value_t *data;
    size_t    data_size;
    size_t    data_fill;
    size_t    row_count;
    size_t    row;
    uintmax_t interval;
    uintmax_t expected_rows;

    int       status;
    size_t    current_line;

    if ((client == NULL) || (filename == NULL) || (cf == NULL) ||
        (ret_start == NULL) || (ret_end == NULL) || (ret_step == NULL) ||
        (ret_ds_num == NULL) || (ret_ds_names == NULL) || (ret_data == NULL))
        return (-1);

    /* Send request {{{ */

    memset(buffer, 0, sizeof(buffer));
    buffer_ptr = &buffer[0];
    buffer_free = sizeof(buffer);
    status = buffer_add_string("FETCH", &buffer_ptr, &buffer_free);
    if (status != 0) {
        return (ENOBUFS);
    }

    /* change to path for rrdcached */
    file_path = get_path(client, filename);
    if (file_path == NULL) {
        return (EINVAL);
    }

    status = buffer_add_string(file_path, &buffer_ptr, &buffer_free);
    free(file_path);

    if (status != 0) {
        return (ENOBUFS);
    }

    status = buffer_add_string(cf, &buffer_ptr, &buffer_free);
    if (status != 0) {
        return (ENOBUFS);
    }

    if ((ret_start != NULL) && (*ret_start > 0)) {
        char      tmp[64];

        snprintf(tmp, sizeof(tmp), "%jd", (intmax_t) *ret_start);
        tmp[sizeof(tmp) - 1] = 0;
        status = buffer_add_string(tmp, &buffer_ptr, &buffer_free);
        if (status != 0) {
            return (ENOBUFS);
        }

        if ((ret_end != NULL) && (*ret_end > 0)) {
            snprintf(tmp, sizeof(tmp), "%jd", (intmax_t) *ret_end);
            tmp[sizeof(tmp) - 1] = 0;
            status = buffer_add_string(tmp, &buffer_ptr, &buffer_free);
            if (status != 0) {
                return (ENOBUFS);
            }
        }
    }

    assert(buffer_free < sizeof(buffer));
    buffer_size = sizeof(buffer) - buffer_free;
    assert(buffer[buffer_size - 1] == ' ');
    buffer[buffer_size - 1] = '\n';

    res = NULL;
    status = request(client, buffer, buffer_size, &res);
    if (status != 0) {
        return (status);
    }
    status = res->status;
    if (status < 0) {
        response_free(res);
        return (status);
    }

    /* }}} Send request */

    ds_names = NULL;
    ds_num = 0;
    data = NULL;
    current_line = 0;

    /* Macros to make error handling a little easier (i. e. less to type and
     * read. `BAIL_OUT' sets the error message, frees all dynamically allocated
     * variables and returns the provided status code. */
#define BAIL_OUT(status, ...) do { \
    rrd_set_error ("rrdc_fetch: " __VA_ARGS__); \
    free (data); \
    if (ds_names != 0) { size_t k; for (k = 0; k < ds_num; k++) free (ds_names[k]); } \
    free (ds_names); \
    response_free (res); \
    close_connection (client); \
    return (status); \
  } while (0)

#define READ_UNSIGNED_FIELD(name,type,max,var) do { \
    char *key; \
    uintmax_t value; \
    if (current_line >= res->lines_num) \
      BAIL_OUT (-1, "Premature end of response packet"); \
    status = parse_uintmax_header (res->lines[current_line], &key, &value); \
    if (status != 0) \
      BAIL_OUT (-1, "Unable to parse header `%s'", name); \
    if (strcasecmp (key, name) != 0) \
      BAIL_OUT (-1, "Unexpected header line: Expected `%s', got `%s'", \
                name, key); \
    if (value > (uintmax_t) (max)) \
      BAIL_OUT (-1, "Value for `%s' is out of range", name); \
    var = (type) value; \
    current_line++; \
  } while (0)

#define READ_SIGNED_FIELD(name,type,var) do { \
    char *key; \
    intmax_t value; \
    if (current_line >= res->lines_num) \
      BAIL_OUT (-1, "Premature end of response packet"); \
    status = parse_intmax_header (res->lines[current_line], &key, &value); \
    if (status != 0) \
      BAIL_OUT (-1, "Unable to parse header `%s'", name); \
    if (strcasecmp (key, name) != 0) \
      BAIL_OUT (-1, "Unexpected header line: Expected `%s', got `%s'", \
                name, key); \
    var = (type) value; \
    if ((intmax_t) var != value) \
      BAIL_OUT (-1, "Value for `%s' is out of range", name); \
    current_line++; \
  } while (0)

    if (res->lines_num < 1)
        BAIL_OUT(-1, "Premature end of response packet");

    /* We're making some very strong assumptions about the fields below. We
     * therefore check the version of the `flush' command first, so that later
     * versions can change the order of fields and it's easier to implement
     * backwards compatibility. */
    READ_UNSIGNED_FIELD("FlushVersion", uintmax_t, UINTMAX_MAX,
                        flush_version);

    if (flush_version != 1)
        BAIL_OUT(-1, "Don't know how to handle flush format version %ju.",
                 flush_version);

    if (res->lines_num < 6)
        BAIL_OUT(-1, "Premature end of response packet");

    READ_SIGNED_FIELD("Start", time_t, start);
    READ_SIGNED_FIELD("End", time_t, end);

    if (start >= end)
        BAIL_OUT(-1, "Malformed start and end times: start = %jd; end = %jd;",
                 (intmax_t) start, (intmax_t) end);

    READ_UNSIGNED_FIELD("Step", unsigned long, ULONG_MAX, step);

    if (step < 1)
        BAIL_OUT(-1, "Invalid number for Step: %lu", step);

    READ_UNSIGNED_FIELD("DSCount", unsigned long, ULONG_MAX, ds_num);

    if (ds_num < 1)
        BAIL_OUT(-1, "Invalid number for DSCount: %lu", ds_num);

    if ((ds_num > RRD_CMD_MAX) ||
        ((uintmax_t) ds_num > SIZE_MAX / sizeof(*ds_names)))
        BAIL_OUT(-1, "DSCount is too large");

    /* It's time to allocate some memory */
    ds_names = (char **) calloc((size_t) ds_num, sizeof(*ds_names));
    if (ds_names == NULL)
        BAIL_OUT(-1, "Out of memory");

    status = parse_char_array_header(res->lines[current_line],
                                     &str_tmp, ds_names, (size_t) ds_num,
                                     /* alloc = */ 1);
    if (status != 0)
        BAIL_OUT(-1, "Unable to parse header `DSName'");
    if (strcasecmp("DSName", str_tmp) != 0)
        BAIL_OUT(-1, "Unexpected header line: Expected `DSName', got `%s'",
                 str_tmp);
    current_line++;

    row_count = res->lines_num - current_line;
    interval = (uintmax_t) end - (uintmax_t) start;
    expected_rows = interval / (uintmax_t) step;
    if ((expected_rows == 0) || ((uintmax_t) row_count != expected_rows))
        BAIL_OUT(-1, "No data returned or headers invalid.");

    if ((size_t) ds_num > SIZE_MAX / row_count)
        BAIL_OUT(-1, "Returned data size is too large");
    data_size = row_count * (size_t) ds_num;
    if (data_size > SIZE_MAX / sizeof(*data))
        BAIL_OUT(-1, "Returned data size is too large");

    data = (rrd_value_t *) calloc(data_size, sizeof(*data));
    if (data == NULL)
        BAIL_OUT(-1, "Out of memory");


    data_fill = 0;
    for (row = 0; row < row_count; row++, current_line++) {
        time_t    tmp;

        status = parse_value_array_header(res->lines[current_line],
                                          &tmp, data + data_fill,
                                          (size_t) ds_num);
        if (status != 0)
            BAIL_OUT(-1, "Cannot parse value line");
        if (((uintmax_t) tmp - (uintmax_t) start) !=
            ((uintmax_t) row + 1) * (uintmax_t) step)
            BAIL_OUT(-1, "Unexpected timestamp in value line");

        data_fill += (size_t) ds_num;
    }

    *ret_start = start;
    *ret_end = end;
    *ret_step = step;
    *ret_ds_num = ds_num;
    *ret_ds_names = ds_names;
    *ret_data = data;

    response_free(res);
    return (0);
#undef READ_SIGNED_FIELD
#undef READ_UNSIGNED_FIELD
#undef BAIL_OUT
}                       /* }}} int_rrd_client_fetch */

int rrdc_fetch(
    const char *filename,   /* {{{ */
    const char *cf,
    time_t *ret_start,
    time_t *ret_end,
    unsigned long *ret_step,
    unsigned long *ret_ds_num,
    char ***ret_ds_names,
    rrd_value_t **ret_data)
{
    int       status;

    mutex_lock(&lock);
    status =
        rrd_client_fetch(&default_client, filename, cf, ret_start, ret_end,
                         ret_step, ret_ds_num, ret_ds_names, ret_data);
    mutex_unlock(&lock);
    return status;
}                       /* }}} int rrdc_fetch */

/* Replace the connection of `client' by a new one to the same daemon.
 * Used after the daemon closed the connection on purpose. If the new
 * connection fails, the client is left disconnected, as if
 * rrd_client_disconnect had been called, and no error is set: the
 * caller's request has already succeeded, and the next request reports
 * that the client is not connected. */
static void reopen_connection(
    rrd_client_t *client)
{                       /* {{{ */
    char     *addr = NULL;

    if (client->sd_path != NULL)
        addr = strdup(client->sd_path);
    close_connection(client);
    if (addr == NULL)
        return;
    if (rrd_client_connect(client, addr) != 0)
        rrd_clear_error();
    free(addr);
}                       /* }}} void reopen_connection */

int rrd_client_dump(
    rrd_client_t *client,
    const char *filename,   /* {{{ */
    const char *opt_header,
    rrd_output_callback_t output_cb,
    void *cb_userdata)
{
    char    buffer[RRD_CMD_MAX];
    char   *buffer_ptr;
    size_t  buffer_free;
    size_t  buffer_size;
    int     status;
    char   *file_path;
    char    resp_buffer[256];

    if (client == NULL) return -1;
    if (filename == NULL) {
        rrd_set_error("rrdc_dump: no input filename specified");
        return -1;
    }

    memset(buffer, 0, sizeof(buffer));
    buffer_ptr = &buffer[0];
    buffer_free = sizeof(buffer);

    status = buffer_add_string("dump", &buffer_ptr, &buffer_free);
    if (status != 0) {
        rrd_set_error("rrdc_dump: out of memory");
        return -1;
    }

    file_path = get_path(client, filename);
    if (file_path == NULL) {
        return -1;
    }

    status = buffer_add_string(file_path, &buffer_ptr, &buffer_free);
    free(file_path);
    if (status != 0) {
        rrd_set_error("rrdc_dump: out of memory");
        return -1;
    }

    if (opt_header) {
        status = buffer_add_string(opt_header, &buffer_ptr, &buffer_free);
        if (status != 0) {
            rrd_set_error("rrdc_dump: out of memory");
            return -1;
        }
    }

    /* buffer ready to send? */
    assert(buffer_free < sizeof(buffer));
    buffer_size = sizeof(buffer) - buffer_free;
    assert(buffer[buffer_size - 1] == ' ');
    buffer[buffer_size - 1] = '\n';

    /* send request to rrdcached */
    status = sendall(client, buffer, buffer_size, 1);
    if (status == -1) {
        rrd_set_error("rrdc_dump: socket error (%s) while talking to rrdcached", 
                rrd_strerror(errno));
        close_connection(client);
        return -1;
    }

    /* receive response from rrdcached, relay to output_cb */
    ssize_t received, written;
    ssize_t response_len = 0;
    while (1) {
        received = recv(client->sd, buffer, sizeof(buffer), 0);
        if (received == -1L) {
            rrd_set_error("rrdc_dump: failed to recv from rrdcached: %s",
                    rrd_strerror(errno));
            close_connection(client);
            return -1;
        }
        if (received == 0) {
            /* the daemon marks the end of the dump by closing the
             * connection; open a new one to the same daemon so the
             * caller's client stays usable */
            reopen_connection(client);
            break; // EOF
        }
        written = output_cb(buffer, received, cb_userdata);
        if (written != received) {
            rrd_set_error("rrdc_dump: unexpected number of bytes (%ld) "
                    "written (output_cb)", written);
            close_connection(client);
            return -1;
        }

        // gather the first response bytes to detect XML response or status
        size_t remaining_response_len =
            ((signed) sizeof(resp_buffer) - response_len) > written ? written
            : (signed)sizeof(resp_buffer) - response_len;
        if (remaining_response_len > 0) { // continuously append to response buffer
            memcpy(resp_buffer+response_len, buffer, remaining_response_len);
            response_len += remaining_response_len;
        }

        if (response_len < 1) continue; // unlikely empty write

        // handle non-xml response (error)
        if (resp_buffer[0] != '<') {
            char *nl = (char *) memchr((void *) resp_buffer, '\n', response_len);
            if (nl == NULL) {
                continue; // we did not get a line (yet)
            }
            *nl = '\0'; // \0 terminate at newline
            chomp(resp_buffer); // chomp away possible \r too
            rrd_set_error("rrdc_dump: failed to dump: %s", resp_buffer);
            /* the daemon answered with a single status line and keeps
             * the connection open, so it stays usable */
            return -1;
        }
        // if the response starts with `<` an XML payload is assumed and the connection
        // will be shutdown from the daemon to indicate EOF.
    }

    return 0;
}                       /* }}} int rrd_client_dump */

int rrd_client_tune(
    rrd_client_t *client,
    const char *filename,   /* {{{ */
    int argc,
    const char **argv)
{
    char      buffer[RRD_CMD_MAX];
    char     *buffer_ptr;
    size_t    buffer_free;
    size_t    buffer_size;
    rrdc_response_t *res;
    int       status;
    char     *file_path;
    int       i;

    if (client == NULL)
        return -1;
    if (filename == NULL) {
        rrd_set_error("rrdc_tune: no filename specified");
        return (-1);
    }

    memset(buffer, 0, sizeof(buffer));
    buffer_ptr = &buffer[0];
    buffer_free = sizeof(buffer);

    status = buffer_add_string("tune", &buffer_ptr, &buffer_free);
    if (status != 0) {
        rrd_set_error("rrdc_tune: out of memory");
        return (-1);
    }

    file_path = get_path(client, filename);
    if (file_path == NULL) {
        return (-1);
    }

    status = buffer_add_string(file_path, &buffer_ptr, &buffer_free);
    free(file_path);

    if (status != 0) {
        rrd_set_error("rrdc_tune: out of memory");
        return (-1);
    }

    status = buffer_add_ulong(argc, &buffer_ptr, &buffer_free);
    if (status != 0) {
        rrd_set_error("rrdc_tune: out of memory");
        return (-1);
    }

    for (i = 0; i < argc; i++) {
        if (argv[i]) {
            status = buffer_add_string(argv[i], &buffer_ptr, &buffer_free);
            if (status != 0) {
                rrd_set_error("rrdc_tune: out of memory");
                return (-1);
            }
        }
    }

    /* buffer ready to send? */
    assert(buffer_free < sizeof(buffer));
    buffer_size = sizeof(buffer) - buffer_free;
    assert(buffer[buffer_size - 1] == ' ');
    buffer[buffer_size - 1] = '\n';

    res = NULL;
    status = request(client, buffer, buffer_size, &res);

    if (status != 0)
        return (-1);

    status = res->status;
    response_free(res);
    return (status);
}                       /* }}} int rrd_client_tune */

int rrdc_tune(
    const char *filename,   /* {{{ */
    int argc,
    const char **argv)
{
    int       status;

    mutex_lock(&lock);
    status =
        rrd_client_tune(&default_client, filename, argc, argv);
    mutex_unlock(&lock);
    return status;
}                       /* }}} int rrdc_tune */

int rrdc_dump(
    const char *filename,   /* {{{ */
    const char *opt_header,
    rrd_output_callback_t output_cb,
    void *cb_userdata)
{
    mutex_lock(&lock);
    int status =
        rrd_client_dump(&default_client, filename, opt_header,
                output_cb, cb_userdata);
    mutex_unlock(&lock);
    return status;
}                       /* }}} int rrdc_tune */

/* convenience function; if there is a daemon specified, or if we can
 * detect one from the environment, then flush the file.  Otherwise, no-op
 */
int rrdc_flush_if_daemon(
    const char *opt_daemon,
    const char *filename)
{                       /* {{{ */
    int       status;

    mutex_lock(&lock);
    rrd_client_connect(&default_client, opt_daemon);

    if (!rrdc_is_connected(opt_daemon)) {
        mutex_unlock(&lock);
        return 0;
    }

    rrd_clear_error();
    status = rrd_client_flush(&default_client, filename);
    mutex_unlock(&lock);

    if (status != 0 && !rrd_test_error()) {
        if (status > 0) {
            rrd_set_error("rrdc_flush (%s) failed: %s",
                          filename, rrd_strerror(status));
        } else if (status < 0) {
            rrd_set_error("rrdc_flush (%s) failed with status %i.",
                          filename, status);
        }
    }

    return status;
}                       /* }}} int rrdc_flush_if_daemon */

/* convenience function; if there is a daemon specified, or if we can
 * detect one from the environment, then flush the file.  Otherwise, no-op
 */
int rrdc_flushall_if_daemon(
    const char *opt_daemon)
{                       /* {{{ */
    int       status;

    mutex_lock(&lock);
    rrd_client_connect(&default_client, opt_daemon);

    if (!rrdc_is_connected(opt_daemon)) {
        mutex_unlock(&lock);
        return 0;
    }

    rrd_clear_error();
    status = rrd_client_flushall(&default_client);
    mutex_unlock(&lock);

    if (status != 0 && !rrd_test_error()) {
        if (status > 0) {
            rrd_set_error("rrdc_flushall failed: %s", rrd_strerror(status));
        } else if (status < 0) {
            rrd_set_error("rrdc_flushall failed with status %i.", status);
        }
    }

    return status;
}                       /* }}} rrdc_flushall_if_daemon */

int rrd_client_stats_get(
    rrd_client_t *client,
    rrdc_stats_t **ret_stats)
{                       /* {{{ */
    rrdc_stats_t *head;
    rrdc_stats_t *tail;

    rrdc_response_t *res;

    int       status;
    int       malformed;
    int       out_of_memory;
    size_t    i;

    if ((client == NULL) || (ret_stats == NULL))
        return (EINVAL);
    *ret_stats = NULL;

    /* Protocol example: {{{
     * ->  STATS
     * <-  5 Statistics follow
     * <-  QueueLength: 0
     * <-  UpdatesWritten: 0
     * <-  DataSetsWritten: 0
     * <-  TreeNodesNumber: 0
     * <-  TreeDepth: 0
     * }}} */

    res = NULL;
    status = request(client, "STATS\n", strlen("STATS\n"), &res);

    if (status != 0)
        return (status);

    if (res->status <= 0) {
        response_free(res);
        return (EIO);
    }

    head = NULL;
    tail = NULL;
    malformed = 0;
    out_of_memory = 0;
    for (i = 0; i < res->lines_num; i++) {
        char     *key;
        char     *value;
        char     *endptr;
        rrdc_stats_t *s;

        key = res->lines[i];
        value = strchr(key, ':');
        if ((value == NULL) || (value == key)) {
            malformed = 1;
            continue;
        }
        *value = 0;
        value++;

        while ((value[0] == ' ') || (value[0] == '\t'))
            value++;

        s = (rrdc_stats_t *) malloc(sizeof(rrdc_stats_t));
        if (s == NULL) {
            out_of_memory = 1;
            break;
        }
        memset(s, 0, sizeof(*s));

        s->name = strdup(key);
        if (s->name == NULL) {
            free(s);
            out_of_memory = 1;
            break;
        }

        endptr = NULL;
        if ((strcmp("QueueLength", key) == 0)
            || (strcmp("TreeDepth", key) == 0)
            || (strcmp("TreeNodesNumber", key) == 0)) {
            s->type = RRDC_STATS_TYPE_GAUGE;
            rrd_strtodbl(value, &endptr, &(s->value.gauge),
                         "QueueLength or TreeDepth or TreeNodesNumber");
        } else if ((strcmp("DataSetsWritten", key) == 0)
                   || (strcmp("FlushesReceived", key) == 0)
                   || (strcmp("JournalBytes", key) == 0)
                   || (strcmp("JournalRotate", key) == 0)
                   || (strcmp("UpdatesReceived", key) == 0)
                   || (strcmp("UpdatesWritten", key) == 0)) {
            uintmax_t counter;

            s->type = RRDC_STATS_TYPE_COUNTER;
            errno = 0;
            if ((value[0] < '0') || (value[0] > '9')) {
                malformed = 1;
                free((char *) s->name);
                free(s);
                continue;
            }
            counter = strtoumax(value, &endptr, /* base = */ 10);
            if ((errno != 0) || (counter > UINT64_MAX)) {
                malformed = 1;
                free((char *) s->name);
                free(s);
                continue;
            }
            s->value.counter = (uint64_t) counter;
        } else {
            free((char *) s->name);
            free(s);
            continue;
        }

        /* Conversion failed */
        if ((endptr == value) || (endptr[0] != '\0')) {
            malformed = 1;
            free((char *) s->name);
            free(s);
            continue;
        }

        if (head == NULL) {
            head = s;
            tail = s;
            s->next = NULL;
        } else {
            tail->next = s;
            tail = s;
        }
    }                   /* for (i = 0; i < res->lines_num; i++) */

    response_free(res);

    if (out_of_memory || malformed) {
        rrdc_stats_free(head);
        if (malformed)
            close_connection(client);
        if (out_of_memory)
            return (ENOMEM);
#ifdef EPROTO
        return (EPROTO);
#else
        return (EINVAL);
#endif
    }

    if (head == NULL) {
        close_connection(client);
#ifdef EPROTO
        return (EPROTO);
#else
        return (EINVAL);
#endif
    }

    *ret_stats = head;
    return (0);
}                       /* }}} int rrd_client_stats_get */

int rrdc_stats_get(
    rrdc_stats_t **ret_stats)
{                       /* {{{ */
    int       status;

    mutex_lock(&lock);
    status = rrd_client_stats_get(&default_client, ret_stats);
    mutex_unlock(&lock);
    return status;
}                       /* }}} int rrdc_stats_get */

void rrdc_stats_free(
    rrdc_stats_t *ret_stats)
{                       /* {{{ */
    rrdc_stats_t *stats;

    stats = ret_stats;
    while (stats != NULL) {
        rrdc_stats_t *next;

        next = stats->next;

        if (stats->name != NULL) {
            free((char *) stats->name);
            stats->name = NULL;
        }
        free(stats);

        stats = next;
    }                   /* while (this != NULL) */
}                       /* }}} void rrdc_stats_free */

/*
 * vim: set sw=4 sts=4 ts=4 et fdm=marker :
 */
