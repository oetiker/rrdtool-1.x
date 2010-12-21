/**
 * RRDTool - src/rrd_client.c
 * Copyright (C) 2008-2010  Florian octo Forster
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

#include "rrd.h"
#include "rrd_tool.h"
#include "rrd_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <limits.h>

#ifndef ENODATA
#define ENODATA ENOENT
#endif

struct rrdc_response_s
{
  int status;
  char *message;
  char **lines;
  size_t lines_num;
};
typedef struct rrdc_response_s rrdc_response_t;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int sd = -1;
static FILE *sh = NULL;
static char *sd_path = NULL; /* cache the path for sd */

/* get_path: Return a path name appropriate to be sent to the daemon.
 *
 * When talking to a local daemon (thru a UNIX socket), relative path names
 * are resolved to absolute path names to allow for transparent integration
 * into existing solutions (as requested by Tobi). Else, absolute path names
 * are not allowed, since path name translation is done by the server.
 *
 * One must hold `lock' when calling this function. */
static const char *get_path (const char *path, char *resolved_path) /* {{{ */
{
  const char *ret = path;
  int is_unix = 0;

  if ((path == NULL) || (resolved_path == NULL) || (sd_path == NULL))
    return (NULL);

  if ((*sd_path == '/')
      || (strncmp ("unix:", sd_path, strlen ("unix:")) == 0))
    is_unix = 1;

  if (is_unix)
  {
    ret = realpath(path, resolved_path);
    if (ret == NULL)
      rrd_set_error("realpath(%s): %s", path, rrd_strerror(errno));
    return ret;
  }
  else
  {
    if (*path == '/') /* not absolute path */
    {
      rrd_set_error ("absolute path names not allowed when talking "
          "to a remote daemon");
      return NULL;
    }
  }

  return path;
} /* }}} char *get_path */

static size_t strsplit (char *string, char **fields, size_t size) /* {{{ */
{
  size_t i;
  char *ptr;
  char *saveptr;

  i = 0;
  ptr = string;
  saveptr = NULL;
  while ((fields[i] = strtok_r (ptr, " \t\r\n", &saveptr)) != NULL)
  {
    ptr = NULL;
    i++;

    if (i >= size)
      break;
  }

  return (i);
} /* }}} size_t strsplit */

static int parse_header (char *line, /* {{{ */
    char **ret_key, char **ret_value)
{
  char *tmp;

  *ret_key = line;

  tmp = strchr (line, ':');
  if (tmp == NULL)
    return (-1);

  do
  {
    *tmp = 0;
    tmp++;
  }
  while ((tmp[0] == ' ') || (tmp[0] == '\t'));

  if (*tmp == 0)
    return (-1);

  *ret_value = tmp;
  return (0);
} /* }}} int parse_header */

static int parse_ulong_header (char *line, /* {{{ */
    char **ret_key, unsigned long *ret_value)
{
  char *str_value;
  char *endptr;
  int status;

  str_value = NULL;
  status = parse_header (line, ret_key, &str_value);
  if (status != 0)
    return (status);

  endptr = NULL;
  errno = 0;
  *ret_value = (unsigned long) strtol (str_value, &endptr, /* base = */ 0);
  if ((endptr == str_value) || (errno != 0))
    return (-1);

  return (0);
} /* }}} int parse_ulong_header */

static int parse_char_array_header (char *line, /* {{{ */
    char **ret_key, char **array, size_t array_len, int alloc)
{
  char *tmp_array[array_len];
  char *value;
  size_t num;
  int status;

  value = NULL;
  status = parse_header (line, ret_key, &value);
  if (status != 0)
    return (-1);

  num = strsplit (value, tmp_array, array_len);
  if (num != array_len)
    return (-1);

  if (alloc == 0)
  {
    memcpy (array, tmp_array, sizeof (tmp_array));
  }
  else
  {
    size_t i;

    for (i = 0; i < array_len; i++)
      array[i] = strdup (tmp_array[i]);
  }

  return (0);
} /* }}} int parse_char_array_header */

static int parse_value_array_header (char *line, /* {{{ */
    time_t *ret_time, rrd_value_t *array, size_t array_len)
{
  char *str_key;
  char *str_array[array_len];
  char *endptr;
  int status;
  size_t i;

  str_key = NULL;
  status = parse_char_array_header (line, &str_key,
      str_array, array_len, /* alloc = */ 0);
  if (status != 0)
    return (-1);

  errno = 0;
  endptr = NULL;
  *ret_time = (time_t) strtol (str_key, &endptr, /* base = */ 10);
  if ((endptr == str_key) || (errno != 0))
    return (-1);

  for (i = 0; i < array_len; i++)
  {
    endptr = NULL;
    array[i] = (rrd_value_t) strtod (str_array[i], &endptr);
    if ((endptr == str_array[i]) || (errno != 0))
      return (-1);
  }

  return (0);
} /* }}} int parse_value_array_header */

/* One must hold `lock' when calling `close_connection'. */
static void close_connection (void) /* {{{ */
{
  if (sh != NULL)
  {
    fclose (sh);
    sh = NULL;
    sd = -1;
  }
  else if (sd >= 0)
  {
    close (sd);
    sd = -1;
  }

  if (sd_path != NULL)
    free (sd_path);
  sd_path = NULL;
} /* }}} void close_connection */

static int buffer_add_string (const char *str, /* {{{ */
    char **buffer_ret, size_t *buffer_size_ret)
{
  char *buffer;
  size_t buffer_size;
  size_t buffer_pos;
  size_t i;
  int status;

  buffer = *buffer_ret;
  buffer_size = *buffer_size_ret;
  buffer_pos = 0;

  i = 0;
  status = -1;
  while (buffer_pos < buffer_size)
  {
    if (str[i] == 0)
    {
      buffer[buffer_pos] = ' ';
      buffer_pos++;
      status = 0;
      break;
    }
    else if ((str[i] == ' ') || (str[i] == '\\'))
    {
      if (buffer_pos >= (buffer_size - 1))
        break;
      buffer[buffer_pos] = '\\';
      buffer_pos++;
      buffer[buffer_pos] = str[i];
      buffer_pos++;
    }
    else
    {
      buffer[buffer_pos] = str[i];
      buffer_pos++;
    }
    i++;
  } /* while (buffer_pos < buffer_size) */

  if (status != 0)
    return (-1);

  *buffer_ret = buffer + buffer_pos;
  *buffer_size_ret = buffer_size - buffer_pos;

  return (0);
} /* }}} int buffer_add_string */

static int buffer_add_value (const char *value, /* {{{ */
    char **buffer_ret, size_t *buffer_size_ret)
{
  char temp[RRD_CMD_MAX];

  if (strncmp (value, "N:", 2) == 0)
    snprintf (temp, sizeof (temp), "%lu:%s",
        (unsigned long) time (NULL), value + 2);
  else
    strncpy (temp, value, sizeof (temp));
  temp[sizeof (temp) - 1] = 0;

  return (buffer_add_string (temp, buffer_ret, buffer_size_ret));
} /* }}} int buffer_add_value */

static int buffer_add_ulong (const unsigned long value, /* {{{ */
    char **buffer_ret, size_t *buffer_size_ret)
{
  char temp[RRD_CMD_MAX];

  snprintf (temp, sizeof (temp), "%lu", value);
  temp[sizeof (temp) - 1] = 0;
  return (buffer_add_string (temp, buffer_ret, buffer_size_ret));
} /* }}} int buffer_add_ulong */

/* Remove trailing newline (NL) and carriage return (CR) characters. Similar to
 * the Perl function `chomp'. Returns the number of characters that have been
 * removed. */
static int chomp (char *str) /* {{{ */
{
  size_t len;
  int removed;

  if (str == NULL)
    return (-1);

  len = strlen (str);
  removed = 0;
  while ((len > 0) && ((str[len - 1] == '\n') || (str[len - 1] == '\r')))
  {
    str[len - 1] = 0;
    len--;
    removed++;
  }

  return (removed);
} /* }}} int chomp */

static void response_free (rrdc_response_t *res) /* {{{ */
{
  if (res == NULL)
    return;

  if (res->lines != NULL)
  {
    size_t i;

    for (i = 0; i < res->lines_num; i++)
      if (res->lines[i] != NULL)
        free (res->lines[i]);
    free (res->lines);
  }

  free (res);
} /* }}} void response_free */

static int response_read (rrdc_response_t **ret_response) /* {{{ */
{
  rrdc_response_t *ret = NULL;
  int status = 0;

  char buffer[RRD_CMD_MAX];
  char *buffer_ptr;

  size_t i;

#define DIE(code) do { status = code; goto err_out; } while(0)

  if (sh == NULL)
    DIE(-1);

  ret = (rrdc_response_t *) malloc (sizeof (rrdc_response_t));
  if (ret == NULL)
    DIE(-2);
  memset (ret, 0, sizeof (*ret));
  ret->lines = NULL;
  ret->lines_num = 0;

  buffer_ptr = fgets (buffer, sizeof (buffer), sh);
  if (buffer_ptr == NULL)
    DIE(-3);

  chomp (buffer);

  ret->status = strtol (buffer, &ret->message, 0);
  if (buffer == ret->message)
    DIE(-4);

  /* Skip leading whitespace of the status message */
  ret->message += strspn (ret->message, " \t");

  if (ret->status <= 0)
  {
    if (ret->status < 0)
      rrd_set_error("rrdcached: %s", ret->message);
    goto out;
  }

  ret->lines = (char **) malloc (sizeof (char *) * ret->status);
  if (ret->lines == NULL)
    DIE(-5);

  memset (ret->lines, 0, sizeof (char *) * ret->status);
  ret->lines_num = (size_t) ret->status;

  for (i = 0; i < ret->lines_num; i++)
  {
    buffer_ptr = fgets (buffer, sizeof (buffer), sh);
    if (buffer_ptr == NULL)
      DIE(-6);

    chomp (buffer);

    ret->lines[i] = strdup (buffer);
    if (ret->lines[i] == NULL)
      DIE(-7);
  }

out:
  *ret_response = ret;
  fflush(sh);
  return (status);

err_out:
  response_free(ret);
  close_connection();
  return (status);

#undef DIE

} /* }}} rrdc_response_t *response_read */

static int request (const char *buffer, size_t buffer_size, /* {{{ */
    rrdc_response_t **ret_response)
{
  int status;
  rrdc_response_t *res;

  if (sh == NULL)
    return (ENOTCONN);

  status = (int) fwrite (buffer, buffer_size, /* nmemb = */ 1, sh);
  if (status != 1)
  {
    close_connection ();
    rrd_set_error("request: socket error (%d) while talking to rrdcached",
                  status);
    return (-1);
  }
  fflush (sh);

  res = NULL;
  status = response_read (&res);

  if (status != 0)
  {
    if (status < 0)
      rrd_set_error("request: internal error while talking to rrdcached");
    return (status);
  }

  *ret_response = res;
  return (0);
} /* }}} int request */

/* determine whether we are connected to the specified daemon_addr if
 * NULL, return whether we are connected at all
 */
int rrdc_is_connected(const char *daemon_addr) /* {{{ */
{
  if (sd < 0)
    return 0;
  else if (daemon_addr == NULL)
  {
    /* here we have to handle the case i.e.
     *   UPDATE --daemon ...; UPDATEV (no --daemon) ...
     * In other words: we have a cached connection,
     * but it is not specified in the current command.
     * Daemon is only implied in this case if set in ENV
     */
    if (getenv(ENV_RRDCACHED_ADDRESS) != NULL)
      return 1;
    else
      return 0;
  }
  else if (strcmp(daemon_addr, sd_path) == 0)
    return 1;
  else
    return 0;

} /* }}} int rrdc_is_connected */

static int rrdc_connect_unix (const char *path) /* {{{ */
{
  struct sockaddr_un sa;
  int status;

  assert (path != NULL);
  assert (sd == -1);

  sd = socket (PF_UNIX, SOCK_STREAM, /* protocol = */ 0);
  if (sd < 0)
  {
    status = errno;
    return (status);
  }

  memset (&sa, 0, sizeof (sa));
  sa.sun_family = AF_UNIX;
  strncpy (sa.sun_path, path, sizeof (sa.sun_path) - 1);

  status = connect (sd, (struct sockaddr *) &sa, sizeof (sa));
  if (status != 0)
  {
    status = errno;
    close_connection ();
    return (status);
  }

  sh = fdopen (sd, "r+");
  if (sh == NULL)
  {
    status = errno;
    close_connection ();
    return (status);
  }

  return (0);
} /* }}} int rrdc_connect_unix */

static int rrdc_connect_network (const char *addr_orig) /* {{{ */
{
  struct addrinfo ai_hints;
  struct addrinfo *ai_res;
  struct addrinfo *ai_ptr;
  char addr_copy[NI_MAXHOST];
  char *addr;
  char *port;

  assert (addr_orig != NULL);
  assert (sd == -1);

  strncpy(addr_copy, addr_orig, sizeof(addr_copy));
  addr_copy[sizeof(addr_copy) - 1] = '\0';
  addr = addr_copy;

  int status;
  memset (&ai_hints, 0, sizeof (ai_hints));
  ai_hints.ai_flags = 0;
#ifdef AI_ADDRCONFIG
  ai_hints.ai_flags |= AI_ADDRCONFIG;
#endif
  ai_hints.ai_family = AF_UNSPEC;
  ai_hints.ai_socktype = SOCK_STREAM;

  port = NULL;
  if (*addr == '[') /* IPv6+port format */
  {
    /* `addr' is something like "[2001:780:104:2:211:24ff:feab:26f8]:12345" */
    addr++;

    port = strchr (addr, ']');
    if (port == NULL)
    {
      rrd_set_error("malformed address: %s", addr_orig);
      return (-1);
    }
    *port = 0;
    port++;

    if (*port == ':')
      port++;
    else if (*port == 0)
      port = NULL;
    else
    {
      rrd_set_error("garbage after address: %s", port);
      return (-1);
    }
  } /* if (*addr == '[') */
  else
  {
    port = rindex(addr, ':');
    if (port != NULL)
    {
      *port = 0;
      port++;
    }
  }

  ai_res = NULL;
  status = getaddrinfo (addr,
                        port == NULL ? RRDCACHED_DEFAULT_PORT : port,
                        &ai_hints, &ai_res);
  if (status != 0)
  {
    rrd_set_error ("failed to resolve address `%s' (port %s): %s",
        addr, port == NULL ? RRDCACHED_DEFAULT_PORT : port,
        gai_strerror (status));
    return (-1);
  }

  for (ai_ptr = ai_res; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next)
  {
    sd = socket (ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
    if (sd < 0)
    {
      status = errno;
      sd = -1;
      continue;
    }

    status = connect (sd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
    if (status != 0)
    {
      status = errno;
      close_connection();
      continue;
    }

    sh = fdopen (sd, "r+");
    if (sh == NULL)
    {
      status = errno;
      close_connection ();
      continue;
    }

    assert (status == 0);
    break;
  } /* for (ai_ptr) */

  freeaddrinfo(ai_res);

  return (status);
} /* }}} int rrdc_connect_network */

int rrdc_connect (const char *addr) /* {{{ */
{
  int status = 0;

  if (addr == NULL)
    addr = getenv (ENV_RRDCACHED_ADDRESS);

  if (addr == NULL)
    return 0;

  pthread_mutex_lock(&lock);

  if (sd >= 0 && sd_path != NULL && strcmp(addr, sd_path) == 0)
  {
    /* connection to the same daemon; use cached connection */
    pthread_mutex_unlock (&lock);
    return (0);
  }
  else
  {
    close_connection();
  }

  rrd_clear_error ();
  if (strncmp ("unix:", addr, strlen ("unix:")) == 0)
    status = rrdc_connect_unix (addr + strlen ("unix:"));
  else if (addr[0] == '/')
    status = rrdc_connect_unix (addr);
  else
    status = rrdc_connect_network(addr);

  if (status == 0 && sd >= 0)
    sd_path = strdup(addr);
  else
  {
    char *err = rrd_test_error () ? rrd_get_error () : "Internal error";
    /* err points the string that gets written to by rrd_set_error(), thus we
     * cannot pass it to that function */
    err = strdup (err);
    rrd_set_error("Unable to connect to rrdcached: %s",
                  (status < 0)
                  ? (err ? err : "Internal error")
                  : rrd_strerror (status));
    if (err != NULL)
      free (err);
  }

  pthread_mutex_unlock (&lock);
  return (status);
} /* }}} int rrdc_connect */

int rrdc_disconnect (void) /* {{{ */
{
  pthread_mutex_lock (&lock);

  close_connection();

  pthread_mutex_unlock (&lock);

  return (0);
} /* }}} int rrdc_disconnect */

int rrdc_update (const char *filename, int values_num, /* {{{ */
		const char * const *values)
{
  char buffer[RRD_CMD_MAX];
  char *buffer_ptr;
  size_t buffer_free;
  size_t buffer_size;
  rrdc_response_t *res;
  int status;
  int i;
  char file_path[PATH_MAX];

  memset (buffer, 0, sizeof (buffer));
  buffer_ptr = &buffer[0];
  buffer_free = sizeof (buffer);

  status = buffer_add_string ("update", &buffer_ptr, &buffer_free);
  if (status != 0)
    return (ENOBUFS);

  pthread_mutex_lock (&lock);
  filename = get_path (filename, file_path);
  if (filename == NULL)
  {
    pthread_mutex_unlock (&lock);
    return (-1);
  }

  status = buffer_add_string (filename, &buffer_ptr, &buffer_free);
  if (status != 0)
  {
    pthread_mutex_unlock (&lock);
    return (ENOBUFS);
  }

  for (i = 0; i < values_num; i++)
  {
    status = buffer_add_value (values[i], &buffer_ptr, &buffer_free);
    if (status != 0)
    {
      pthread_mutex_unlock (&lock);
      return (ENOBUFS);
    }
  }

  assert (buffer_free < sizeof (buffer));
  buffer_size = sizeof (buffer) - buffer_free;
  assert (buffer[buffer_size - 1] == ' ');
  buffer[buffer_size - 1] = '\n';

  res = NULL;
  status = request (buffer, buffer_size, &res);
  pthread_mutex_unlock (&lock);

  if (status != 0)
    return (status);

  status = res->status;
  response_free (res);

  return (status);
} /* }}} int rrdc_update */

int rrdc_flush (const char *filename) /* {{{ */
{
  char buffer[RRD_CMD_MAX];
  char *buffer_ptr;
  size_t buffer_free;
  size_t buffer_size;
  rrdc_response_t *res;
  int status;
  char file_path[PATH_MAX];

  if (filename == NULL)
    return (-1);

  memset (buffer, 0, sizeof (buffer));
  buffer_ptr = &buffer[0];
  buffer_free = sizeof (buffer);

  status = buffer_add_string ("flush", &buffer_ptr, &buffer_free);
  if (status != 0)
    return (ENOBUFS);

  pthread_mutex_lock (&lock);
  filename = get_path (filename, file_path);
  if (filename == NULL)
  {
    pthread_mutex_unlock (&lock);
    return (-1);
  }

  status = buffer_add_string (filename, &buffer_ptr, &buffer_free);
  if (status != 0)
  {
    pthread_mutex_unlock (&lock);
    return (ENOBUFS);
  }

  assert (buffer_free < sizeof (buffer));
  buffer_size = sizeof (buffer) - buffer_free;
  assert (buffer[buffer_size - 1] == ' ');
  buffer[buffer_size - 1] = '\n';

  res = NULL;
  status = request (buffer, buffer_size, &res);
  pthread_mutex_unlock (&lock);

  if (status != 0)
    return (status);

  status = res->status;
  response_free (res);

  return (status);
} /* }}} int rrdc_flush */

rrd_info_t * rrdc_info (const char *filename) /* {{{ */
{
  char buffer[RRD_CMD_MAX];
  char *buffer_ptr;
  size_t buffer_free;
  size_t buffer_size;
  rrdc_response_t *res;
  int status;
  char file_path[PATH_MAX];
  rrd_info_t *data = NULL, *cd;
  rrd_infoval_t info;
  unsigned int l;
  rrd_info_type_t itype;
  char *k, *s;

  if (filename == NULL) {
    rrd_set_error ("rrdc_info: no filename");
    return (NULL);
  }

  memset (buffer, 0, sizeof (buffer));
  buffer_ptr = &buffer[0];
  buffer_free = sizeof (buffer);

  status = buffer_add_string ("info", &buffer_ptr, &buffer_free);
  if (status != 0) {
    rrd_set_error ("rrdc_info: out of memory");
    return (NULL);
  }

  pthread_mutex_lock (&lock);
  filename = get_path (filename, file_path);
  if (filename == NULL)
  {
    pthread_mutex_unlock (&lock);
    return (NULL);
  }

  status = buffer_add_string (filename, &buffer_ptr, &buffer_free);
  if (status != 0)
  {
    pthread_mutex_unlock (&lock);
    rrd_set_error ("rrdc_info: out of memory");
    return (NULL);
  }

  assert (buffer_free < sizeof (buffer));
  buffer_size = sizeof (buffer) - buffer_free;
  assert (buffer[buffer_size - 1] == ' ');
  buffer[buffer_size - 1] = '\n';

  res = NULL;
  status = request (buffer, buffer_size, &res);
  pthread_mutex_unlock (&lock);

  if (status != 0) {
    rrd_set_error ("rrdcached: %s", res->message);
    return (NULL);
  }
  data = cd = NULL;
  for( l=0 ; l < res->lines_num ; l++ ) {
    /* first extract the keyword */
	for(k = s = res->lines[l];s && *s;s++) {
      if(*s == ' ') { *s = 0; s++; break; }
	}
    if(!s || !*s) break;
	itype = atoi(s); /* extract type code */
	for(;*s;s++) { if(*s == ' ') { *s = 0; s++; break; } }
    if(!*s) break;
    /* finally, we're pointing to the value */
    switch(itype) {
    case RD_I_VAL:
        if(*s == 'N') { info.u_val = DNAN; } else { info.u_val = atof(s); }
        break;
    case RD_I_CNT:
        info.u_cnt = atol(s);
        break;
    case RD_I_INT:
        info.u_int = atoi(s);
        break;
    case RD_I_STR:
        chomp(s);
        info.u_str = (char*)malloc(sizeof(char) * (strlen(s) + 1));
        strcpy(info.u_str,s);
        break;
    case RD_I_BLO:
        rrd_set_error ("rrdc_info: BLOB objects are not supported");
        return (NULL);
    default:
        rrd_set_error ("rrdc_info: Unsupported info type %d",itype);
        return (NULL);
    }
	
    cd = rrd_info_push(cd, sprintf_alloc("%s",k), itype, info);
	if(!data) data = cd;
  }
  response_free (res);

  return (data);
} /* }}} int rrdc_info */

time_t rrdc_last (const char *filename) /* {{{ */
{
  char buffer[RRD_CMD_MAX];
  char *buffer_ptr;
  size_t buffer_free;
  size_t buffer_size;
  rrdc_response_t *res;
  int status;
  char file_path[PATH_MAX];
  time_t lastup;

  if (filename == NULL) {
    rrd_set_error ("rrdc_last: no filename");
    return (-1);
  }

  memset (buffer, 0, sizeof (buffer));
  buffer_ptr = &buffer[0];
  buffer_free = sizeof (buffer);

  status = buffer_add_string ("last", &buffer_ptr, &buffer_free);
  if (status != 0) {
    rrd_set_error ("rrdc_last: out of memory");
    return (-1);
  }

  pthread_mutex_lock (&lock);
  filename = get_path (filename, file_path);
  if (filename == NULL)
  {
    pthread_mutex_unlock (&lock);
    return (-1);
  }

  status = buffer_add_string (filename, &buffer_ptr, &buffer_free);
  if (status != 0)
  {
    pthread_mutex_unlock (&lock);
    rrd_set_error ("rrdc_last: out of memory");
    return (-1);
  }

  assert (buffer_free < sizeof (buffer));
  buffer_size = sizeof (buffer) - buffer_free;
  assert (buffer[buffer_size - 1] == ' ');
  buffer[buffer_size - 1] = '\n';

  res = NULL;
  status = request (buffer, buffer_size, &res);
  pthread_mutex_unlock (&lock);

  if (status != 0) {
    rrd_set_error ("rrdcached: %s", res->message);
    return (-1);
  }
  lastup = atol(res->message);
  response_free (res);

  return (lastup);
} /* }}} int rrdc_last */

time_t rrdc_first (const char *filename, int rraindex) /* {{{ */
{
  char buffer[RRD_CMD_MAX];
  char *buffer_ptr;
  size_t buffer_free;
  size_t buffer_size;
  rrdc_response_t *res;
  int status;
  char file_path[PATH_MAX];
  time_t firstup;

  if (filename == NULL) {
    rrd_set_error ("rrdc_first: no filename specified");
    return (-1);
  }

  memset (buffer, 0, sizeof (buffer));
  buffer_ptr = &buffer[0];
  buffer_free = sizeof (buffer);

  status = buffer_add_string ("first", &buffer_ptr, &buffer_free);
  if (status != 0) {
    rrd_set_error ("rrdc_first: out of memory");
    return (-1);
  }

  pthread_mutex_lock (&lock);
  filename = get_path (filename, file_path);
  if (filename == NULL)
  {
    pthread_mutex_unlock (&lock);
    return (-1);
  }

  status = buffer_add_string (filename, &buffer_ptr, &buffer_free);
  if (status != 0)
  {
    pthread_mutex_unlock (&lock);
    rrd_set_error ("rrdc_first: out of memory");
    return (-1);
  }
  status = buffer_add_ulong (rraindex, &buffer_ptr, &buffer_free);
  if (status != 0)
  {
    pthread_mutex_unlock (&lock);
    rrd_set_error ("rrdc_first: out of memory");
    return (-1);
  }

  assert (buffer_free < sizeof (buffer));
  buffer_size = sizeof (buffer) - buffer_free;
  assert (buffer[buffer_size - 1] == ' ');
  buffer[buffer_size - 1] = '\n';

  res = NULL;
  status = request (buffer, buffer_size, &res);
  pthread_mutex_unlock (&lock);

  if (status != 0) {
    rrd_set_error ("rrdcached: %s", res->message);
    return (-1);
  }
  firstup = atol(res->message);
  response_free (res);

  return (firstup);
} /* }}} int rrdc_first */

int rrdc_create (const char *filename, /* {{{ */
    unsigned long pdp_step,
    time_t last_up,
    int no_overwrite,
    int argc,
    const char **argv)
{
  char buffer[RRD_CMD_MAX];
  char *buffer_ptr;
  size_t buffer_free;
  size_t buffer_size;
  rrdc_response_t *res;
  int status;
  char file_path[PATH_MAX];
  int i;

  if (filename == NULL) {
    rrd_set_error ("rrdc_create: no filename specified");
    return (-1);
  }

  memset (buffer, 0, sizeof (buffer));
  buffer_ptr = &buffer[0];
  buffer_free = sizeof (buffer);

  status = buffer_add_string ("create", &buffer_ptr, &buffer_free);
  if (status != 0) {
    rrd_set_error ("rrdc_create: out of memory");
    return (-1);
  }

  pthread_mutex_lock (&lock);
  filename = get_path (filename, file_path);
  if (filename == NULL)
  {
    pthread_mutex_unlock (&lock);
    return (-1);
  }

  status = buffer_add_string (filename, &buffer_ptr, &buffer_free);
  status = buffer_add_string ("-b", &buffer_ptr, &buffer_free);
  status = buffer_add_ulong (last_up, &buffer_ptr, &buffer_free);
  status = buffer_add_string ("-s", &buffer_ptr, &buffer_free);
  status = buffer_add_ulong (pdp_step, &buffer_ptr, &buffer_free);
  if(no_overwrite) {
    status = buffer_add_string ("-O", &buffer_ptr, &buffer_free);
  }
  if (status != 0)
  {
    pthread_mutex_unlock (&lock);
    rrd_set_error ("rrdc_create: out of memory");
    return (-1);
  }

  for( i=0; i<argc; i++ ) {
    if( argv[i] ) {
      status = buffer_add_string (argv[i], &buffer_ptr, &buffer_free);
      if (status != 0)
      {
        pthread_mutex_unlock (&lock);
        rrd_set_error ("rrdc_create: out of memory");
        return (-1);
      }
	}
  }

  /* buffer ready to send? */
  assert (buffer_free < sizeof (buffer));
  buffer_size = sizeof (buffer) - buffer_free;
  assert (buffer[buffer_size - 1] == ' ');
  buffer[buffer_size - 1] = '\n';

  res = NULL;
  status = request (buffer, buffer_size, &res);
  pthread_mutex_unlock (&lock);

  if (status != 0) {
    rrd_set_error ("rrdcached: %s", res->message);
    return (-1);
  }
  response_free (res);
  return(0);
} /* }}} int rrdc_create */

int rrdc_fetch (const char *filename, /* {{{ */
    const char *cf,
    time_t *ret_start, time_t *ret_end,
    unsigned long *ret_step,
    unsigned long *ret_ds_num,
    char ***ret_ds_names,
    rrd_value_t **ret_data)
{
  char buffer[RRD_CMD_MAX];
  char *buffer_ptr;
  size_t buffer_free;
  size_t buffer_size;
  rrdc_response_t *res;
  char path_buffer[PATH_MAX];
  const char *path_ptr;

  char *str_tmp;
  unsigned long flush_version;

  time_t start;
  time_t end;
  unsigned long step;
  unsigned long ds_num;
  char **ds_names;

  rrd_value_t *data;
  size_t data_size;
  size_t data_fill;

  int status;
  size_t current_line;
  time_t t;

  if ((filename == NULL) || (cf == NULL))
    return (-1);

  /* Send request {{{ */
  memset (buffer, 0, sizeof (buffer));
  buffer_ptr = &buffer[0];
  buffer_free = sizeof (buffer);

  status = buffer_add_string ("FETCH", &buffer_ptr, &buffer_free);
  if (status != 0)
    return (ENOBUFS);

  /* change to path for rrdcached */
  path_ptr = get_path (filename, path_buffer);
  if (path_ptr == NULL)
    return (EINVAL);

  status = buffer_add_string (path_ptr, &buffer_ptr, &buffer_free);
  if (status != 0)
    return (ENOBUFS);

  status = buffer_add_string (cf, &buffer_ptr, &buffer_free);
  if (status != 0)
    return (ENOBUFS);

  if ((ret_start != NULL) && (*ret_start > 0))
  {
    char tmp[64];
    snprintf (tmp, sizeof (tmp), "%lu", (unsigned long) *ret_start);
    tmp[sizeof (tmp) - 1] = 0;
    status = buffer_add_string (tmp, &buffer_ptr, &buffer_free);
    if (status != 0)
      return (ENOBUFS);

    if ((ret_end != NULL) && (*ret_end > 0))
    {
      snprintf (tmp, sizeof (tmp), "%lu", (unsigned long) *ret_end);
      tmp[sizeof (tmp) - 1] = 0;
      status = buffer_add_string (tmp, &buffer_ptr, &buffer_free);
      if (status != 0)
        return (ENOBUFS);
    }
  }

  assert (buffer_free < sizeof (buffer));
  buffer_size = sizeof (buffer) - buffer_free;
  assert (buffer[buffer_size - 1] == ' ');
  buffer[buffer_size - 1] = '\n';

  res = NULL;
  status = request (buffer, buffer_size, &res);
  if (status != 0)
    return (status);

  status = res->status;
  if (status < 0)
  {
    rrd_set_error ("rrdcached: %s", res->message);
    response_free (res);
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
    return (status); \
  } while (0)

#define READ_NUMERIC_FIELD(name,type,var) do { \
    char *key; \
    unsigned long value; \
    assert (current_line < res->lines_num); \
    status = parse_ulong_header (res->lines[current_line], &key, &value); \
    if (status != 0) \
      BAIL_OUT (-1, "Unable to parse header `%s'", name); \
    if (strcasecmp (key, name) != 0) \
      BAIL_OUT (-1, "Unexpected header line: Expected `%s', got `%s'", name, key); \
    var = (type) value; \
    current_line++; \
  } while (0)

  if (res->lines_num < 1)
    BAIL_OUT (-1, "Premature end of response packet");

  /* We're making some very strong assumptions about the fields below. We
   * therefore check the version of the `flush' command first, so that later
   * versions can change the order of fields and it's easier to implement
   * backwards compatibility. */
  READ_NUMERIC_FIELD ("FlushVersion", unsigned long, flush_version);
  if (flush_version != 1)
    BAIL_OUT (-1, "Don't know how to handle flush format version %lu.",
        flush_version);

  if (res->lines_num < 5)
    BAIL_OUT (-1, "Premature end of response packet");

  READ_NUMERIC_FIELD ("Start", time_t, start);
  READ_NUMERIC_FIELD ("End", time_t, end);
  if (start >= end)
    BAIL_OUT (-1, "Malformed start and end times: start = %lu; end = %lu;",
        (unsigned long) start,
        (unsigned long) end);

  READ_NUMERIC_FIELD ("Step", unsigned long, step);
  if (step < 1)
    BAIL_OUT (-1, "Invalid number for Step: %lu", step);

  READ_NUMERIC_FIELD ("DSCount", unsigned long, ds_num);
  if (ds_num < 1)
    BAIL_OUT (-1, "Invalid number for DSCount: %lu", ds_num);
  
  /* It's time to allocate some memory */
  ds_names = calloc ((size_t) ds_num, sizeof (*ds_names));
  if (ds_names == NULL)
    BAIL_OUT (-1, "Out of memory");

  status = parse_char_array_header (res->lines[current_line],
      &str_tmp, ds_names, (size_t) ds_num, /* alloc = */ 1);
  if (status != 0)
    BAIL_OUT (-1, "Unable to parse header `DSName'");
  if (strcasecmp ("DSName", str_tmp) != 0)
    BAIL_OUT (-1, "Unexpected header line: Expected `DSName', got `%s'", str_tmp);
  current_line++;

  data_size = ds_num * (end - start) / step;
  if (data_size < 1)
    BAIL_OUT (-1, "No data returned or headers invalid.");

  if (res->lines_num != (6 + (data_size / ds_num)))
    BAIL_OUT (-1, "Got %zu lines, expected %zu",
        res->lines_num, (6 + (data_size / ds_num)));

  data = calloc (data_size, sizeof (*data));
  if (data == NULL)
    BAIL_OUT (-1, "Out of memory");
  

  data_fill = 0;
  for (t = start + step; t <= end; t += step, current_line++)
  {
    time_t tmp;

    assert (current_line < res->lines_num);

    status = parse_value_array_header (res->lines[current_line],
        &tmp, data + data_fill, (size_t) ds_num);
    if (status != 0)
      BAIL_OUT (-1, "Cannot parse value line");

    data_fill += (size_t) ds_num;
  }

  *ret_start = start;
  *ret_end = end;
  *ret_step = step;
  *ret_ds_num = ds_num;
  *ret_ds_names = ds_names;
  *ret_data = data;

  response_free (res);
  return (0);
#undef READ_NUMERIC_FIELD
#undef BAIL_OUT
} /* }}} int rrdc_flush */

/* convenience function; if there is a daemon specified, or if we can
 * detect one from the environment, then flush the file.  Otherwise, no-op
 */
int rrdc_flush_if_daemon (const char *opt_daemon, const char *filename) /* {{{ */
{
  int status = 0;

  rrdc_connect(opt_daemon);

  if (rrdc_is_connected(opt_daemon))
  {
    rrd_clear_error();
    status = rrdc_flush (filename);

    if (status != 0 && !rrd_test_error())
    {
      if (status > 0)
      {
        rrd_set_error("rrdc_flush (%s) failed: %s",
                      filename, rrd_strerror(status));
      }
      else if (status < 0)
      {
        rrd_set_error("rrdc_flush (%s) failed with status %i.",
                      filename, status);
      }
    }
  } /* if (rrdc_is_connected(..)) */

  return status;
} /* }}} int rrdc_flush_if_daemon */


int rrdc_stats_get (rrdc_stats_t **ret_stats) /* {{{ */
{
  rrdc_stats_t *head;
  rrdc_stats_t *tail;

  rrdc_response_t *res;

  int status;
  size_t i;

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
  pthread_mutex_lock (&lock);
  status = request ("STATS\n", strlen ("STATS\n"), &res);
  pthread_mutex_unlock (&lock);

  if (status != 0)
    return (status);

  if (res->status <= 0)
  {
    response_free (res);
    return (EIO);
  }

  head = NULL;
  tail = NULL;
  for (i = 0; i < res->lines_num; i++)
  {
    char *key;
    char *value;
    char *endptr;
    rrdc_stats_t *s;

    key = res->lines[i];
    value = strchr (key, ':');
    if (value == NULL)
      continue;
    *value = 0;
    value++;

    while ((value[0] == ' ') || (value[0] == '\t'))
      value++;

    s = (rrdc_stats_t *) malloc (sizeof (rrdc_stats_t));
    if (s == NULL)
      continue;
    memset (s, 0, sizeof (*s));

    s->name = strdup (key);

    endptr = NULL;
    if ((strcmp ("QueueLength", key) == 0)
        || (strcmp ("TreeDepth", key) == 0)
        || (strcmp ("TreeNodesNumber", key) == 0))
    {
      s->type = RRDC_STATS_TYPE_GAUGE;
      s->value.gauge = strtod (value, &endptr);
    }
    else if ((strcmp ("DataSetsWritten", key) == 0)
        || (strcmp ("FlushesReceived", key) == 0)
        || (strcmp ("JournalBytes", key) == 0)
        || (strcmp ("JournalRotate", key) == 0)
        || (strcmp ("UpdatesReceived", key) == 0)
        || (strcmp ("UpdatesWritten", key) == 0))
    {
      s->type = RRDC_STATS_TYPE_COUNTER;
      s->value.counter = (uint64_t) strtoll (value, &endptr, /* base = */ 0);
    }
    else
    {
      free (s);
      continue;
    }

    /* Conversion failed */
    if (endptr == value)
    {
      free (s);
      continue;
    }

    if (head == NULL)
    {
      head = s;
      tail = s;
      s->next = NULL;
    }
    else
    {
      tail->next = s;
      tail = s;
    }
  } /* for (i = 0; i < res->lines_num; i++) */

  response_free (res);

  if (head == NULL)
    return (EPROTO);

  *ret_stats = head;
  return (0);
} /* }}} int rrdc_stats_get */

void rrdc_stats_free (rrdc_stats_t *ret_stats) /* {{{ */
{
  rrdc_stats_t *this;

  this = ret_stats;
  while (this != NULL)
  {
    rrdc_stats_t *next;

    next = this->next;

    if (this->name != NULL)
    {
      free ((char *)this->name);
      this->name = NULL;
    }
    free (this);

    this = next;
  } /* while (this != NULL) */
} /* }}} void rrdc_stats_free */

/*
 * vim: set sw=2 sts=2 ts=8 et fdm=marker :
 */
