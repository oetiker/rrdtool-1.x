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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int sd = -1;

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
      close (sd);
      sd = -1;
      errno = EPROTO;
      return (-1);
    }

    assert ((0 > status) || (buffer_free >= (size_t) status));

    buffer_free = buffer_free - status;
    buffer_used = buffer_used + status;

    if (buffer[buffer_used - 1] == '\n')
      break;
  }

  if (buffer[buffer_used - 1] != '\n')
  {
    errno = ENOBUFS;
    return (-1);
  }

  buffer[buffer_used - 1] = 0;
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
      close (sd);
      sd = -1;
      return (status);
    }

    nleft = nleft - status;
    ptr   = ptr   + status;
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

static int rrdc_connect_unix (const char *path) /* {{{ */
{
  struct sockaddr_un sa;
  int status;

  assert (path != NULL);

  pthread_mutex_lock (&lock);

  if (sd >= 0)
  {
    pthread_mutex_unlock (&lock);
    return (0);
  }

  sd = socket (PF_UNIX, SOCK_STREAM, /* protocol = */ 0);
  if (sd < 0)
  {
    status = errno;
    pthread_mutex_unlock (&lock);
    return (status);
  }

  memset (&sa, 0, sizeof (sa));
  sa.sun_family = AF_UNIX;
  strncpy (sa.sun_path, path, sizeof (sa.sun_path) - 1);

  status = connect (sd, (struct sockaddr *) &sa, sizeof (sa));
  if (status != 0)
  {
    status = errno;
    pthread_mutex_unlock (&lock);
    return (status);
  }

  pthread_mutex_unlock (&lock);

  return (0);
} /* }}} int rrdc_connect_unix */

int rrdc_connect (const char *addr) /* {{{ */
{
  struct addrinfo ai_hints;
  struct addrinfo *ai_res;
  struct addrinfo *ai_ptr;
  int status;

  if (addr == NULL)
    addr = RRDCACHED_DEFAULT_ADDRESS;

  if (strncmp ("unix:", addr, strlen ("unix:")) == 0)
    return (rrdc_connect_unix (addr + strlen ("unix:")));
  else if (addr[0] == '/')
    return (rrdc_connect_unix (addr));

  pthread_mutex_lock (&lock);

  if (sd >= 0)
  {
    pthread_mutex_unlock (&lock);
    return (0);
  }

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
  {
    pthread_mutex_unlock (&lock);
    return (status);
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
      close (sd);
      sd = -1;
      continue;
    }

    assert (status == 0);
    break;
  } /* for (ai_ptr) */
  pthread_mutex_unlock (&lock);

  return (status);
} /* }}} int rrdc_connect */

int rrdc_disconnect (void) /* {{{ */
{
  pthread_mutex_lock (&lock);

  if (sd < 0)
  {
    pthread_mutex_unlock (&lock);
    return (0);
  }

  close (sd);
  sd = -1;

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

/*
 * vim: set sw=2 sts=2 ts=8 et fdm=marker :
 */
