/**
 * RRDTool - src/rrd_client.c
 * Copyright (C) 2008 Florian octo Forster
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Florian octo Forster <octo at verplant.org>
 **/

#include "rrd.h"
#include "rrd_client.h"
#include "rrd_tool.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>

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
static char *sd_path = NULL; /* cache the path for sd */
static void _disconnect(void);

static ssize_t sread (void *buffer_void, size_t buffer_size) /* {{{ */
{
  char    *buffer;
  size_t   buffer_used;
  size_t   buffer_free;
  ssize_t  status;

  buffer       = (char *) buffer_void;
  buffer_used  = 0;
  buffer_free  = buffer_size;

  while (buffer_free > 0)
  {
    status = read (sd, buffer + buffer_used, buffer_free);
    if ((status < 0) && ((errno == EAGAIN) || (errno == EINTR)))
      continue;

    if (status < 0)
      return (-1);

    if (status == 0)
    {
      _disconnect();
      errno = EPROTO;
      return (-1);
    }

    assert ((0 > status) || (buffer_free >= (size_t) status));

    buffer_free -= status;
    buffer_used += status;

    if (buffer[buffer_used - 1] == '\n')
      break;
  }

  if (buffer[buffer_used - 1] != '\n')
  {
    errno = ENOBUFS;
    return (-1);
  }

  buffer[buffer_used - 1] = '\0';
  return (buffer_used);
} /* }}} ssize_t sread */

static ssize_t swrite (const void *buf, size_t count) /* {{{ */
{
  const char *ptr;
  size_t      nleft;
  ssize_t     status;

  ptr   = (const char *) buf;
  nleft = count;

  while (nleft > 0)
  {
    status = write (sd, (const void *) ptr, nleft);

    if ((status < 0) && ((errno == EAGAIN) || (errno == EINTR)))
      continue;

    if (status < 0)
    {
      _disconnect();
      rrd_set_error("lost connection to rrdcached");
      return (status);
    }

    nleft -= status;
    ptr   += status;
  }

  return (0);
} /* }}} ssize_t swrite */

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
  char temp[4096];

  if (strncmp (value, "N:", 2) == 0)
    snprintf (temp, sizeof (temp), "%lu:%s",
        (unsigned long) time (NULL), value + 2);
  else
    strncpy (temp, value, sizeof (temp));
  temp[sizeof (temp) - 1] = 0;

  return (buffer_add_string (temp, buffer_ret, buffer_size_ret));
} /* }}} int buffer_add_value */

static int response_parse (char *buffer, size_t buffer_size, /* {{{ */
    rrdc_response_t **ret_response)
{
  rrdc_response_t *ret;

  char *dummy;
  char *saveptr;

  char *line_ptr;
  size_t line_counter;

  if (buffer == NULL)
    return (EINVAL);
  if (buffer_size <= 0)
    return (EINVAL);

  if (buffer[buffer_size - 1] != 0)
    return (-1);

  ret = (rrdc_response_t *) malloc (sizeof (rrdc_response_t));
  if (ret == NULL)
    return (ENOMEM);
  memset (ret, 0, sizeof (*ret));

  line_counter = 0;

  dummy = buffer;
  saveptr = NULL;
  while ((line_ptr = strtok_r (dummy, "\r\n", &saveptr)) != NULL)
  {
    dummy = NULL;

    if (ret->message == NULL)
    {
      ret->status = strtol (buffer, &ret->message, 0);
      if (buffer == ret->message)
      {
        free (ret);
        return (EPROTO);
      }

      /* Skip leading whitespace of the status message */
      ret->message += strspn (ret->message, " \t");

      if (ret->status > 0)
      {
        ret->lines = (char **) malloc (sizeof (char *) * ret->status);
        if (ret->lines == NULL)
        {
          free (ret);
          return (ENOMEM);
        }
        memset (ret->lines, 0, sizeof (char *) * ret->status);
        ret->lines_num = (size_t) ret->status;
      }
      else
      {
        ret->lines = NULL;
        ret->lines_num = 0;
      }
    }
    else /* if (ret->message != NULL) */
    {
      if (line_counter < ret->lines_num)
        ret->lines[line_counter] = line_ptr;
      line_counter++;
    }
  } /* while (strtok_r) */

  if (ret->lines_num != line_counter)
  {
    errno = EPROTO;
    if (ret->lines != NULL)
      free (ret->lines);
    free (ret);
    return (-1);
  }

  *ret_response = ret;
  return (0);
} /* }}} int response_parse */

static void response_free (rrdc_response_t *res) /* {{{ */
{
  if (res == NULL)
    return;

  if (res->lines != NULL)
  {
    res->lines_num = 0;
    free (res->lines);
    res->lines = NULL;
  }

  free (res);
} /* }}} void response_free */


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
    return (status);
  }

  return (0);
} /* }}} int rrdc_connect_unix */

static int rrdc_connect_network (const char *addr) /* {{{ */
{
  struct addrinfo ai_hints;
  struct addrinfo *ai_res;
  struct addrinfo *ai_ptr;

  assert (addr != NULL);
  assert (sd == -1);

  int status;
  memset (&ai_hints, 0, sizeof (ai_hints));
  ai_hints.ai_flags = 0;
#ifdef AI_ADDRCONFIG
  ai_hints.ai_flags |= AI_ADDRCONFIG;
#endif
  ai_hints.ai_family = AF_UNSPEC;
  ai_hints.ai_socktype = SOCK_STREAM;

  ai_res = NULL;
  status = getaddrinfo (addr, RRDCACHED_DEFAULT_PORT, &ai_hints, &ai_res);
  if (status != 0)
    return (status);

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
      _disconnect();
      continue;
    }

    assert (status == 0);
    break;
  } /* for (ai_ptr) */

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
    _disconnect();
  }

  if (strncmp ("unix:", addr, strlen ("unix:")) == 0)
    status = rrdc_connect_unix (addr + strlen ("unix:"));
  else if (addr[0] == '/')
    status = rrdc_connect_unix (addr);
  else
    status = rrdc_connect_network(addr);

  if (status == 0 && sd >= 0)
    sd_path = strdup(addr);
  else
    rrd_set_error("Unable to connect to rrdcached: %s",
                  (status < 0)
                  ? "Internal error"
                  : rrd_strerror (status));

  pthread_mutex_unlock (&lock);
  return (status);
} /* }}} int rrdc_connect */

static void _disconnect(void) /* {{{ */
{
  if (sd >= 0)
    close(sd);

  if (sd_path != NULL)
    free(sd_path);

  sd = -1;
  sd_path = NULL;
} /* }}} static void _disconnect(void) */

int rrdc_disconnect (void) /* {{{ */
{
  pthread_mutex_lock (&lock);

  _disconnect();

  pthread_mutex_unlock (&lock);

  return (0);
} /* }}} int rrdc_disconnect */

int rrdc_update (const char *filename, int values_num, /* {{{ */
		const char * const *values)
{
  char buffer[4096];
  char *buffer_ptr;
  size_t buffer_free;
  size_t buffer_size;
  int status;
  int i;

  memset (buffer, 0, sizeof (buffer));
  buffer_ptr = &buffer[0];
  buffer_free = sizeof (buffer);

  status = buffer_add_string ("update", &buffer_ptr, &buffer_free);
  if (status != 0)
    return (ENOBUFS);

  status = buffer_add_string (filename, &buffer_ptr, &buffer_free);
  if (status != 0)
    return (ENOBUFS);

  for (i = 0; i < values_num; i++)
  {
    status = buffer_add_value (values[i], &buffer_ptr, &buffer_free);
    if (status != 0)
      return (ENOBUFS);
  }

  assert (buffer_free < sizeof (buffer));
  buffer_size = sizeof (buffer) - buffer_free;
  assert (buffer[buffer_size - 1] == ' ');
  buffer[buffer_size - 1] = '\n';

  pthread_mutex_lock (&lock);

  if (sd < 0)
  {
    pthread_mutex_unlock (&lock);
    return (ENOTCONN);
  }

  status = swrite (buffer, buffer_size);
  if (status != 0)
  {
    pthread_mutex_unlock (&lock);
    return (status);
  }

  status = sread (buffer, sizeof (buffer));
  if (status < 0)
  {
    status = errno;
    pthread_mutex_unlock (&lock);
    return (status);
  }
  else if (status == 0)
  {
    pthread_mutex_unlock (&lock);
    return (ENODATA);
  }

  pthread_mutex_unlock (&lock);

  status = atoi (buffer);
  return (status);
} /* }}} int rrdc_update */

int rrdc_flush (const char *filename) /* {{{ */
{
  char buffer[4096];
  char *buffer_ptr;
  size_t buffer_free;
  size_t buffer_size;
  int status;

  if (filename == NULL)
    return (-1);

  memset (buffer, 0, sizeof (buffer));
  buffer_ptr = &buffer[0];
  buffer_free = sizeof (buffer);

  status = buffer_add_string ("flush", &buffer_ptr, &buffer_free);
  if (status != 0)
    return (ENOBUFS);

  status = buffer_add_string (filename, &buffer_ptr, &buffer_free);
  if (status != 0)
    return (ENOBUFS);

  assert (buffer_free < sizeof (buffer));
  buffer_size = sizeof (buffer) - buffer_free;
  assert (buffer[buffer_size - 1] == ' ');
  buffer[buffer_size - 1] = '\n';

  pthread_mutex_lock (&lock);

  if (sd < 0)
  {
    pthread_mutex_unlock (&lock);
    return (ENOTCONN);
  }

  status = swrite (buffer, buffer_size);
  if (status != 0)
  {
    pthread_mutex_unlock (&lock);
    return (status);
  }

  status = sread (buffer, sizeof (buffer));
  if (status < 0)
  {
    status = errno;
    pthread_mutex_unlock (&lock);
    return (status);
  }
  else if (status == 0)
  {
    pthread_mutex_unlock (&lock);
    return (ENODATA);
  }

  pthread_mutex_unlock (&lock);

  status = atoi (buffer);
  return (status);
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
    status = rrdc_flush (filename);
    if (status != 0)
    {
      rrd_set_error ("rrdc_flush (%s) failed with status %i.",
                     filename, status);
    }
  } /* if (daemon_addr) */

  return status;
} /* }}} int rrdc_flush_if_daemon */


int rrdc_stats_get (rrdc_stats_t **ret_stats) /* {{{ */
{
  rrdc_stats_t *head;
  rrdc_stats_t *tail;

  rrdc_response_t *response;

  char buffer[4096];
  size_t buffer_size;
  int status;
  size_t i;

  pthread_mutex_lock (&lock);

  if (sd < 0)
  {
    pthread_mutex_unlock (&lock);
    return (ENOTCONN);
  }

  /* Protocol example: {{{
   * ->  STATS
   * <-  5 Statistics follow
   * <-  QueueLength: 0
   * <-  UpdatesWritten: 0
   * <-  DataSetsWritten: 0
   * <-  TreeNodesNumber: 0
   * <-  TreeDepth: 0
   * }}} */
  status = swrite ("STATS\n", strlen ("STATS\n"));
  if (status != 0)
  {
    pthread_mutex_unlock (&lock);
    return (status);
  }

  status = sread (buffer, sizeof (buffer));
  if (status < 0)
  {
    status = errno;
    pthread_mutex_unlock (&lock);
    return (status);
  }
  else if (status == 0)
  {
    pthread_mutex_unlock (&lock);
    return (ENODATA);
  }

  pthread_mutex_unlock (&lock);

  /* Assert NULL termination */
  buffer_size = (size_t) status;
  if (buffer[buffer_size - 1] != 0)
  {
    if (buffer_size < sizeof (buffer))
    {
      buffer[buffer_size] = 0;
      buffer_size++;
    }
    else
    {
      return (ENOBUFS);
    }
  }

  status = response_parse (buffer, buffer_size, &response);
  if (status != 0)
    return (status);

  if (response->status <= 0)
  {
    response_free (response);
    return (EIO);
  }

  head = NULL;
  tail = NULL;
  for (i = 0; i < response->lines_num; i++)
  {
    char *key;
    char *value;
    char *endptr;
    rrdc_stats_t *s;

    key = response->lines[i];
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
        || (strcmp ("TreeNodesNumber", key) == 0)
        || (strcmp ("TreeDepth", key) == 0))
    {
      s->type = RRDC_STATS_TYPE_GAUGE;
      s->value.gauge = strtod (value, &endptr);
    }
    else if ((strcmp ("UpdatesWritten", key) == 0)
        || (strcmp ("DataSetsWritten", key) == 0))
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
  } /* for (i = 0; i < response->lines_num; i++) */

  response_free (response);

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
      free (this->name);
      this->name = NULL;
    }
    free (this);

    this = next;
  } /* while (this != NULL) */
} /* }}} void rrdc_stats_free */

/*
 * vim: set sw=2 sts=2 ts=8 et fdm=marker :
 */
