/**
 * RRDTool - src/rrd_daemon.c
 * Copyright (C) 2008 Florian octo Forster
 * Copyright (C) 2008 Kevin Brintnall
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
 *   kevin brintnall <kbrint@rufus.net>
 **/

#if 0
/*
 * First tell the compiler to stick to the C99 and POSIX standards as close as
 * possible.
 */
#ifndef __STRICT_ANSI__ /* {{{ */
# define __STRICT_ANSI__
#endif

#ifndef _ISOC99_SOURCE
# define _ISOC99_SOURCE
#endif

#ifdef _POSIX_C_SOURCE
# undef _POSIX_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200112L

/* Single UNIX needed for strdup. */
#ifdef _XOPEN_SOURCE
# undef _XOPEN_SOURCE
#endif
#define _XOPEN_SOURCE 500

#ifndef _REENTRANT
# define _REENTRANT
#endif

#ifndef _THREAD_SAFE
# define _THREAD_SAFE
#endif

#ifdef _GNU_SOURCE
# undef _GNU_SOURCE
#endif
/* }}} */
#endif /* 0 */

/*
 * Now for some includes..
 */
#include "rrd.h" /* {{{ */
#include "rrd_client.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <poll.h>
#include <syslog.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>

#include <glib-2.0/glib.h>
/* }}} */

#define RRDD_LOG(severity, ...) syslog ((severity), __VA_ARGS__)

#ifndef __GNUC__
# define __attribute__(x) /**/
#endif

/*
 * Types
 */
struct listen_socket_s
{
  int fd;
  char path[PATH_MAX + 1];
};
typedef struct listen_socket_s listen_socket_t;

struct cache_item_s;
typedef struct cache_item_s cache_item_t;
struct cache_item_s
{
  char *file;
  char **values;
  int values_num;
  time_t last_flush_time;
#define CI_FLAGS_IN_TREE  (1<<0)
#define CI_FLAGS_IN_QUEUE (1<<1)
  int flags;
  pthread_cond_t  flushed;
  cache_item_t *next;
};

struct callback_flush_data_s
{
  time_t now;
  time_t abs_timeout;
  char **keys;
  size_t keys_num;
};
typedef struct callback_flush_data_s callback_flush_data_t;

enum queue_side_e
{
  HEAD,
  TAIL
};
typedef enum queue_side_e queue_side_t;

/* max length of socket command or response */
#define CMD_MAX 4096

/*
 * Variables
 */
static int stay_foreground = 0;

static listen_socket_t *listen_fds = NULL;
static size_t listen_fds_num = 0;

static int do_shutdown = 0;

static pthread_t queue_thread;

static pthread_t *connection_threads = NULL;
static pthread_mutex_t connection_threads_lock = PTHREAD_MUTEX_INITIALIZER;
static int connection_threads_num = 0;

/* Cache stuff */
static GTree          *cache_tree = NULL;
static cache_item_t   *cache_queue_head = NULL;
static cache_item_t   *cache_queue_tail = NULL;
static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cache_cond = PTHREAD_COND_INITIALIZER;

static int config_write_interval = 300;
static int config_write_jitter   = 0;
static int config_flush_interval = 3600;
static int config_flush_at_shutdown = 0;
static char *config_pid_file = NULL;
static char *config_base_dir = NULL;

static char **config_listen_address_list = NULL;
static int config_listen_address_list_len = 0;

static uint64_t stats_queue_length = 0;
static uint64_t stats_updates_received = 0;
static uint64_t stats_flush_received = 0;
static uint64_t stats_updates_written = 0;
static uint64_t stats_data_sets_written = 0;
static uint64_t stats_journal_bytes = 0;
static uint64_t stats_journal_rotate = 0;
static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

/* Journaled updates */
static char *journal_cur = NULL;
static char *journal_old = NULL;
static FILE *journal_fh = NULL;
static pthread_mutex_t journal_lock = PTHREAD_MUTEX_INITIALIZER;
static int journal_write(char *cmd, char *args);
static void journal_done(void);
static void journal_rotate(void);

/* 
 * Functions
 */
static void sig_common (const char *sig) /* {{{ */
{
  RRDD_LOG(LOG_NOTICE, "caught SIG%s", sig);
  do_shutdown++;
  pthread_cond_broadcast(&cache_cond);
} /* }}} void sig_common */

static void sig_int_handler (int s __attribute__((unused))) /* {{{ */
{
  sig_common("INT");
} /* }}} void sig_int_handler */

static void sig_term_handler (int s __attribute__((unused))) /* {{{ */
{
  sig_common("TERM");
} /* }}} void sig_term_handler */

static void sig_usr1_handler (int s __attribute__((unused))) /* {{{ */
{
  config_flush_at_shutdown = 1;
  sig_common("USR1");
} /* }}} void sig_usr1_handler */

static void sig_usr2_handler (int s __attribute__((unused))) /* {{{ */
{
  config_flush_at_shutdown = 0;
  sig_common("USR2");
} /* }}} void sig_usr2_handler */

static void install_signal_handlers(void) /* {{{ */
{
  /* These structures are static, because `sigaction' behaves weird if the are
   * overwritten.. */
  static struct sigaction sa_int;
  static struct sigaction sa_term;
  static struct sigaction sa_pipe;
  static struct sigaction sa_usr1;
  static struct sigaction sa_usr2;

  /* Install signal handlers */
  memset (&sa_int, 0, sizeof (sa_int));
  sa_int.sa_handler = sig_int_handler;
  sigaction (SIGINT, &sa_int, NULL);

  memset (&sa_term, 0, sizeof (sa_term));
  sa_term.sa_handler = sig_term_handler;
  sigaction (SIGTERM, &sa_term, NULL);

  memset (&sa_pipe, 0, sizeof (sa_pipe));
  sa_pipe.sa_handler = SIG_IGN;
  sigaction (SIGPIPE, &sa_pipe, NULL);

  memset (&sa_pipe, 0, sizeof (sa_usr1));
  sa_usr1.sa_handler = sig_usr1_handler;
  sigaction (SIGUSR1, &sa_usr1, NULL);

  memset (&sa_usr2, 0, sizeof (sa_usr2));
  sa_usr2.sa_handler = sig_usr2_handler;
  sigaction (SIGUSR2, &sa_usr2, NULL);

} /* }}} void install_signal_handlers */

static int open_pidfile(void) /* {{{ */
{
  int fd;
  char *file;

  file = (config_pid_file != NULL)
    ? config_pid_file
    : LOCALSTATEDIR "/run/rrdcached.pid";

  fd = open(file, O_CREAT|O_EXCL|O_WRONLY, S_IRUSR|S_IRGRP|S_IROTH);
  if (fd < 0)
    fprintf(stderr, "FATAL: cannot create '%s' (%s)\n",
            file, rrd_strerror(errno));

  return(fd);
} /* }}} static int open_pidfile */

static int write_pidfile (int fd) /* {{{ */
{
  pid_t pid;
  FILE *fh;

  pid = getpid ();

  fh = fdopen (fd, "w");
  if (fh == NULL)
  {
    RRDD_LOG (LOG_ERR, "write_pidfile: fdopen() failed.");
    close(fd);
    return (-1);
  }

  fprintf (fh, "%i\n", (int) pid);
  fclose (fh);

  return (0);
} /* }}} int write_pidfile */

static int remove_pidfile (void) /* {{{ */
{
  char *file;
  int status;

  file = (config_pid_file != NULL)
    ? config_pid_file
    : LOCALSTATEDIR "/run/rrdcached.pid";

  status = unlink (file);
  if (status == 0)
    return (0);
  return (errno);
} /* }}} int remove_pidfile */

static ssize_t sread (int fd, void *buffer_void, size_t buffer_size) /* {{{ */
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
    status = read (fd, buffer + buffer_used, buffer_free);
    if ((status < 0) && ((errno == EAGAIN) || (errno == EINTR)))
      continue;

    if (status < 0)
      return (-1);

    if (status == 0)
      return (0);

    assert ((0 > status) || (buffer_free >= (size_t) status));

    buffer_free = buffer_free - status;
    buffer_used = buffer_used + status;

    if (buffer[buffer_used - 1] == '\n')
      break;
  }

  assert (buffer_used > 0);

  if (buffer[buffer_used - 1] != '\n')
  {
    errno = ENOBUFS;
    return (-1);
  }

  buffer[buffer_used - 1] = 0;

  /* Fix network line endings. */
  if ((buffer_used > 1) && (buffer[buffer_used - 2] == '\r'))
  {
    buffer_used--;
    buffer[buffer_used - 1] = 0;
  }

  return (buffer_used);
} /* }}} ssize_t sread */

static ssize_t swrite (int fd, const void *buf, size_t count) /* {{{ */
{
  const char *ptr;
  size_t      nleft;
  ssize_t     status;

  /* special case for journal replay */
  if (fd < 0) return 0;

  ptr   = (const char *) buf;
  nleft = count;

  while (nleft > 0)
  {
    status = write (fd, (const void *) ptr, nleft);

    if ((status < 0) && ((errno == EAGAIN) || (errno == EINTR)))
      continue;

    if (status < 0)
      return (status);

    nleft -= status;
    ptr   += status;
  }

  return (0);
} /* }}} ssize_t swrite */

static void _wipe_ci_values(cache_item_t *ci, time_t when)
{
  ci->values = NULL;
  ci->values_num = 0;

  ci->last_flush_time = when;
  if (config_write_jitter > 0)
    ci->last_flush_time += (random() % config_write_jitter);

  ci->flags &= ~(CI_FLAGS_IN_QUEUE);
}

/*
 * enqueue_cache_item:
 * `cache_lock' must be acquired before calling this function!
 */
static int enqueue_cache_item (cache_item_t *ci, /* {{{ */
    queue_side_t side)
{
  int did_insert = 0;

  if (ci == NULL)
    return (-1);

  if (ci->values_num == 0)
    return (0);

  if (side == HEAD)
  {
    if ((ci->flags & CI_FLAGS_IN_QUEUE) == 0)
    {
      assert (ci->next == NULL);
      ci->next = cache_queue_head;
      cache_queue_head = ci;

      if (cache_queue_tail == NULL)
        cache_queue_tail = cache_queue_head;

      did_insert = 1;
    }
    else if (cache_queue_head == ci)
    {
      /* do nothing */
    }
    else /* enqueued, but not first entry */
    {
      cache_item_t *prev;

      /* find previous entry */
      for (prev = cache_queue_head; prev != NULL; prev = prev->next)
        if (prev->next == ci)
          break;
      assert (prev != NULL);

      /* move to the front */
      prev->next = ci->next;
      ci->next = cache_queue_head;
      cache_queue_head = ci;

      /* check if we need to adapt the tail */
      if (cache_queue_tail == ci)
        cache_queue_tail = prev;
    }
  }
  else /* (side == TAIL) */
  {
    /* We don't move values back in the list.. */
    if ((ci->flags & CI_FLAGS_IN_QUEUE) != 0)
      return (0);

    assert (ci->next == NULL);

    if (cache_queue_tail == NULL)
      cache_queue_head = ci;
    else
      cache_queue_tail->next = ci;
    cache_queue_tail = ci;

    did_insert = 1;
  }

  ci->flags |= CI_FLAGS_IN_QUEUE;

  if (did_insert)
  {
    pthread_cond_broadcast(&cache_cond);
    pthread_mutex_lock (&stats_lock);
    stats_queue_length++;
    pthread_mutex_unlock (&stats_lock);
  }

  return (0);
} /* }}} int enqueue_cache_item */

/*
 * tree_callback_flush:
 * Called via `g_tree_foreach' in `queue_thread_main'. `cache_lock' is held
 * while this is in progress.
 */
static gboolean tree_callback_flush (gpointer key, gpointer value, /* {{{ */
    gpointer data)
{
  cache_item_t *ci;
  callback_flush_data_t *cfd;

  ci = (cache_item_t *) value;
  cfd = (callback_flush_data_t *) data;

  if ((ci->last_flush_time <= cfd->abs_timeout)
      && ((ci->flags & CI_FLAGS_IN_QUEUE) == 0)
      && (ci->values_num > 0))
  {
    enqueue_cache_item (ci, TAIL);
  }
  else if ((do_shutdown != 0)
      && ((ci->flags & CI_FLAGS_IN_QUEUE) == 0)
      && (ci->values_num > 0))
  {
    enqueue_cache_item (ci, TAIL);
  }
  else if (((cfd->now - ci->last_flush_time) >= config_flush_interval)
      && ((ci->flags & CI_FLAGS_IN_QUEUE) == 0)
      && (ci->values_num <= 0))
  {
    char **temp;

    temp = (char **) realloc (cfd->keys,
        sizeof (char *) * (cfd->keys_num + 1));
    if (temp == NULL)
    {
      RRDD_LOG (LOG_ERR, "tree_callback_flush: realloc failed.");
      return (FALSE);
    }
    cfd->keys = temp;
    /* Make really sure this points to the _same_ place */
    assert ((char *) key == ci->file);
    cfd->keys[cfd->keys_num] = (char *) key;
    cfd->keys_num++;
  }

  return (FALSE);
} /* }}} gboolean tree_callback_flush */

static int flush_old_values (int max_age)
{
  callback_flush_data_t cfd;
  size_t k;

  memset (&cfd, 0, sizeof (cfd));
  /* Pass the current time as user data so that we don't need to call
   * `time' for each node. */
  cfd.now = time (NULL);
  cfd.keys = NULL;
  cfd.keys_num = 0;

  if (max_age > 0)
    cfd.abs_timeout = cfd.now - max_age;
  else
    cfd.abs_timeout = cfd.now + 2*config_write_jitter + 1;

  /* `tree_callback_flush' will return the keys of all values that haven't
   * been touched in the last `config_flush_interval' seconds in `cfd'.
   * The char*'s in this array point to the same memory as ci->file, so we
   * don't need to free them separately. */
  g_tree_foreach (cache_tree, tree_callback_flush, (gpointer) &cfd);

  for (k = 0; k < cfd.keys_num; k++)
  {
    cache_item_t *ci;

    /* This must not fail. */
    ci = (cache_item_t *) g_tree_lookup (cache_tree, cfd.keys[k]);
    assert (ci != NULL);

    /* If we end up here with values available, something's seriously
     * messed up. */
    assert (ci->values_num == 0);

    /* Remove the node from the tree */
    g_tree_remove (cache_tree, cfd.keys[k]);
    cfd.keys[k] = NULL;

    /* Now free and clean up `ci'. */
    free (ci->file);
    ci->file = NULL;
    free (ci);
    ci = NULL;
  } /* for (k = 0; k < cfd.keys_num; k++) */

  if (cfd.keys != NULL)
  {
    free (cfd.keys);
    cfd.keys = NULL;
  }

  return (0);
} /* int flush_old_values */

static void *queue_thread_main (void *args __attribute__((unused))) /* {{{ */
{
  struct timeval now;
  struct timespec next_flush;
  int final_flush = 0; /* make sure we only flush once on shutdown */

  gettimeofday (&now, NULL);
  next_flush.tv_sec = now.tv_sec + config_flush_interval;
  next_flush.tv_nsec = 1000 * now.tv_usec;

  pthread_mutex_lock (&cache_lock);
  while ((do_shutdown == 0) || (cache_queue_head != NULL))
  {
    cache_item_t *ci;
    char *file;
    char **values;
    int values_num;
    int status;
    int i;

    /* First, check if it's time to do the cache flush. */
    gettimeofday (&now, NULL);
    if ((now.tv_sec > next_flush.tv_sec)
        || ((now.tv_sec == next_flush.tv_sec)
          && ((1000 * now.tv_usec) > next_flush.tv_nsec)))
    {
      /* Flush all values that haven't been written in the last
       * `config_write_interval' seconds. */
      flush_old_values (config_write_interval);

      /* Determine the time of the next cache flush. */
      while (next_flush.tv_sec <= now.tv_sec)
        next_flush.tv_sec += config_flush_interval;

      /* unlock the cache while we rotate so we don't block incoming
       * updates if the fsync() blocks on disk I/O */
      pthread_mutex_unlock(&cache_lock);
      journal_rotate();
      pthread_mutex_lock(&cache_lock);
    }

    /* Now, check if there's something to store away. If not, wait until
     * something comes in or it's time to do the cache flush.  if we are
     * shutting down, do not wait around.  */
    if (cache_queue_head == NULL && !do_shutdown)
    {
      status = pthread_cond_timedwait (&cache_cond, &cache_lock, &next_flush);
      if ((status != 0) && (status != ETIMEDOUT))
      {
        RRDD_LOG (LOG_ERR, "queue_thread_main: "
            "pthread_cond_timedwait returned %i.", status);
      }
    }

    /* We're about to shut down */
    if (do_shutdown != 0 && !final_flush++)
    {
      if (config_flush_at_shutdown)
        flush_old_values (-1); /* flush everything */
      else
        break;
    }

    /* Check if a value has arrived. This may be NULL if we timed out or there
     * was an interrupt such as a signal. */
    if (cache_queue_head == NULL)
      continue;

    ci = cache_queue_head;

    /* copy the relevant parts */
    file = strdup (ci->file);
    if (file == NULL)
    {
      RRDD_LOG (LOG_ERR, "queue_thread_main: strdup failed.");
      continue;
    }

    assert(ci->values != NULL);
    assert(ci->values_num > 0);

    values = ci->values;
    values_num = ci->values_num;

    _wipe_ci_values(ci, time(NULL));

    cache_queue_head = ci->next;
    if (cache_queue_head == NULL)
      cache_queue_tail = NULL;
    ci->next = NULL;

    pthread_mutex_lock (&stats_lock);
    assert (stats_queue_length > 0);
    stats_queue_length--;
    pthread_mutex_unlock (&stats_lock);

    pthread_mutex_unlock (&cache_lock);

    rrd_clear_error ();
    status = rrd_update_r (file, NULL, values_num, (void *) values);
    if (status != 0)
    {
      RRDD_LOG (LOG_NOTICE, "queue_thread_main: "
          "rrd_update_r (%s) failed with status %i. (%s)",
          file, status, rrd_get_error());
    }

    journal_write("wrote", file);
    pthread_cond_broadcast(&ci->flushed);

    for (i = 0; i < values_num; i++)
      free (values[i]);

    free(values);
    free(file);

    if (status == 0)
    {
      pthread_mutex_lock (&stats_lock);
      stats_updates_written++;
      stats_data_sets_written += values_num;
      pthread_mutex_unlock (&stats_lock);
    }

    pthread_mutex_lock (&cache_lock);

    /* We're about to shut down */
    if (do_shutdown != 0 && !final_flush++)
    {
      if (config_flush_at_shutdown)
          flush_old_values (-1); /* flush everything */
      else
        break;
    }
  } /* while ((do_shutdown == 0) || (cache_queue_head != NULL)) */
  pthread_mutex_unlock (&cache_lock);

  if (config_flush_at_shutdown)
  {
    assert(cache_queue_head == NULL);
    RRDD_LOG(LOG_INFO, "clean shutdown; all RRDs flushed");
  }

  journal_done();

  return (NULL);
} /* }}} void *queue_thread_main */

static int buffer_get_field (char **buffer_ret, /* {{{ */
    size_t *buffer_size_ret, char **field_ret)
{
  char *buffer;
  size_t buffer_pos;
  size_t buffer_size;
  char *field;
  size_t field_size;
  int status;

  buffer = *buffer_ret;
  buffer_pos = 0;
  buffer_size = *buffer_size_ret;
  field = *buffer_ret;
  field_size = 0;

  if (buffer_size <= 0)
    return (-1);

  /* This is ensured by `handle_request'. */
  assert (buffer[buffer_size - 1] == '\0');

  status = -1;
  while (buffer_pos < buffer_size)
  {
    /* Check for end-of-field or end-of-buffer */
    if (buffer[buffer_pos] == ' ' || buffer[buffer_pos] == '\0')
    {
      field[field_size] = 0;
      field_size++;
      buffer_pos++;
      status = 0;
      break;
    }
    /* Handle escaped characters. */
    else if (buffer[buffer_pos] == '\\')
    {
      if (buffer_pos >= (buffer_size - 1))
        break;
      buffer_pos++;
      field[field_size] = buffer[buffer_pos];
      field_size++;
      buffer_pos++;
    }
    /* Normal operation */ 
    else
    {
      field[field_size] = buffer[buffer_pos];
      field_size++;
      buffer_pos++;
    }
  } /* while (buffer_pos < buffer_size) */

  if (status != 0)
    return (status);

  *buffer_ret = buffer + buffer_pos;
  *buffer_size_ret = buffer_size - buffer_pos;
  *field_ret = field;

  return (0);
} /* }}} int buffer_get_field */

static int flush_file (const char *filename) /* {{{ */
{
  cache_item_t *ci;

  pthread_mutex_lock (&cache_lock);

  ci = (cache_item_t *) g_tree_lookup (cache_tree, filename);
  if (ci == NULL)
  {
    pthread_mutex_unlock (&cache_lock);
    return (ENOENT);
  }

  if (ci->values_num > 0)
  {
    /* Enqueue at head */
    enqueue_cache_item (ci, HEAD);
    pthread_cond_wait(&ci->flushed, &cache_lock);
  }

  pthread_mutex_unlock(&cache_lock);

  return (0);
} /* }}} int flush_file */

static int handle_request_help (int fd, /* {{{ */
    char *buffer, size_t buffer_size)
{
  int status;
  char **help_text;
  size_t help_text_len;
  char *command;
  size_t i;

  char *help_help[] =
  {
    "5 Command overview\n",
    "FLUSH <filename>\n",
    "FLUSHALL\n",
    "HELP [<command>]\n",
    "UPDATE <filename> <values> [<values> ...]\n",
    "STATS\n"
  };
  size_t help_help_len = sizeof (help_help) / sizeof (help_help[0]);

  char *help_flush[] =
  {
    "4 Help for FLUSH\n",
    "Usage: FLUSH <filename>\n",
    "\n",
    "Adds the given filename to the head of the update queue and returns\n",
    "after is has been dequeued.\n"
  };
  size_t help_flush_len = sizeof (help_flush) / sizeof (help_flush[0]);

  char *help_flushall[] =
  {
    "3 Help for FLUSHALL\n",
    "Usage: FLUSHALL\n",
    "\n",
    "Triggers writing of all pending updates.  Returns immediately.\n"
  };
  size_t help_flushall_len = sizeof(help_flushall) / sizeof(help_flushall[0]);

  char *help_update[] =
  {
    "9 Help for UPDATE\n",
    "Usage: UPDATE <filename> <values> [<values> ...]\n"
    "\n",
    "Adds the given file to the internal cache if it is not yet known and\n",
    "appends the given value(s) to the entry. See the rrdcached(1) manpage\n",
    "for details.\n",
    "\n",
    "Each <values> has the following form:\n",
    "  <values> = <time>:<value>[:<value>[...]]\n",
    "See the rrdupdate(1) manpage for details.\n"
  };
  size_t help_update_len = sizeof (help_update) / sizeof (help_update[0]);

  char *help_stats[] =
  {
    "4 Help for STATS\n",
    "Usage: STATS\n",
    "\n",
    "Returns some performance counters, see the rrdcached(1) manpage for\n",
    "a description of the values.\n"
  };
  size_t help_stats_len = sizeof (help_stats) / sizeof (help_stats[0]);

  status = buffer_get_field (&buffer, &buffer_size, &command);
  if (status != 0)
  {
    help_text = help_help;
    help_text_len = help_help_len;
  }
  else
  {
    if (strcasecmp (command, "update") == 0)
    {
      help_text = help_update;
      help_text_len = help_update_len;
    }
    else if (strcasecmp (command, "flush") == 0)
    {
      help_text = help_flush;
      help_text_len = help_flush_len;
    }
    else if (strcasecmp (command, "flushall") == 0)
    {
      help_text = help_flushall;
      help_text_len = help_flushall_len;
    }
    else if (strcasecmp (command, "stats") == 0)
    {
      help_text = help_stats;
      help_text_len = help_stats_len;
    }
    else
    {
      help_text = help_help;
      help_text_len = help_help_len;
    }
  }

  for (i = 0; i < help_text_len; i++)
  {
    status = swrite (fd, help_text[i], strlen (help_text[i]));
    if (status < 0)
    {
      status = errno;
      RRDD_LOG (LOG_ERR, "handle_request_help: swrite returned an error.");
      return (status);
    }
  }

  return (0);
} /* }}} int handle_request_help */

static int handle_request_stats (int fd, /* {{{ */
    char *buffer __attribute__((unused)),
    size_t buffer_size __attribute__((unused)))
{
  int status;
  char outbuf[CMD_MAX];

  uint64_t copy_queue_length;
  uint64_t copy_updates_received;
  uint64_t copy_flush_received;
  uint64_t copy_updates_written;
  uint64_t copy_data_sets_written;
  uint64_t copy_journal_bytes;
  uint64_t copy_journal_rotate;

  uint64_t tree_nodes_number;
  uint64_t tree_depth;

  pthread_mutex_lock (&stats_lock);
  copy_queue_length       = stats_queue_length;
  copy_updates_received   = stats_updates_received;
  copy_flush_received     = stats_flush_received;
  copy_updates_written    = stats_updates_written;
  copy_data_sets_written  = stats_data_sets_written;
  copy_journal_bytes      = stats_journal_bytes;
  copy_journal_rotate     = stats_journal_rotate;
  pthread_mutex_unlock (&stats_lock);

  pthread_mutex_lock (&cache_lock);
  tree_nodes_number = (uint64_t) g_tree_nnodes (cache_tree);
  tree_depth        = (uint64_t) g_tree_height (cache_tree);
  pthread_mutex_unlock (&cache_lock);

#define RRDD_STATS_SEND \
  outbuf[sizeof (outbuf) - 1] = 0; \
  status = swrite (fd, outbuf, strlen (outbuf)); \
  if (status < 0) \
  { \
    status = errno; \
    RRDD_LOG (LOG_INFO, "handle_request_stats: swrite returned an error."); \
    return (status); \
  }

  strncpy (outbuf, "9 Statistics follow\n", sizeof (outbuf));
  RRDD_STATS_SEND;

  snprintf (outbuf, sizeof (outbuf),
      "QueueLength: %"PRIu64"\n", copy_queue_length);
  RRDD_STATS_SEND;

  snprintf (outbuf, sizeof (outbuf),
      "UpdatesReceived: %"PRIu64"\n", copy_updates_received);
  RRDD_STATS_SEND;

  snprintf (outbuf, sizeof (outbuf),
      "FlushesReceived: %"PRIu64"\n", copy_flush_received);
  RRDD_STATS_SEND;

  snprintf (outbuf, sizeof (outbuf),
      "UpdatesWritten: %"PRIu64"\n", copy_updates_written);
  RRDD_STATS_SEND;

  snprintf (outbuf, sizeof (outbuf),
      "DataSetsWritten: %"PRIu64"\n", copy_data_sets_written);
  RRDD_STATS_SEND;

  snprintf (outbuf, sizeof (outbuf),
      "TreeNodesNumber: %"PRIu64"\n", tree_nodes_number);
  RRDD_STATS_SEND;

  snprintf (outbuf, sizeof (outbuf),
      "TreeDepth: %"PRIu64"\n", tree_depth);
  RRDD_STATS_SEND;

  snprintf (outbuf, sizeof(outbuf),
      "JournalBytes: %"PRIu64"\n", copy_journal_bytes);
  RRDD_STATS_SEND;

  snprintf (outbuf, sizeof(outbuf),
      "JournalRotate: %"PRIu64"\n", copy_journal_rotate);
  RRDD_STATS_SEND;

  return (0);
#undef RRDD_STATS_SEND
} /* }}} int handle_request_stats */

static int handle_request_flush (int fd, /* {{{ */
    char *buffer, size_t buffer_size)
{
  char *file;
  int status;
  char result[CMD_MAX];

  status = buffer_get_field (&buffer, &buffer_size, &file);
  if (status != 0)
  {
    strncpy (result, "-1 Usage: flush <filename>\n", sizeof (result));
  }
  else
  {
    pthread_mutex_lock(&stats_lock);
    stats_flush_received++;
    pthread_mutex_unlock(&stats_lock);

    status = flush_file (file);
    if (status == 0)
      snprintf (result, sizeof (result), "0 Successfully flushed %s.\n", file);
    else if (status == ENOENT)
    {
      /* no file in our tree; see whether it exists at all */
      struct stat statbuf;

      memset(&statbuf, 0, sizeof(statbuf));
      if (stat(file, &statbuf) == 0 && S_ISREG(statbuf.st_mode))
        snprintf (result, sizeof (result), "0 Nothing to flush: %s.\n", file);
      else
        snprintf (result, sizeof (result), "-1 No such file: %s.\n", file);
    }
    else if (status < 0)
      strncpy (result, "-1 Internal error.\n", sizeof (result));
    else
      snprintf (result, sizeof (result), "-1 Failed with status %i.\n", status);
  }
  result[sizeof (result) - 1] = 0;

  status = swrite (fd, result, strlen (result));
  if (status < 0)
  {
    status = errno;
    RRDD_LOG (LOG_INFO, "handle_request_flush: swrite returned an error.");
    return (status);
  }

  return (0);
} /* }}} int handle_request_flush */

static int handle_request_flushall(int fd) /* {{{ */
{
  int status;
  char answer[] ="0 Started flush.\n";

  RRDD_LOG(LOG_DEBUG, "Received FLUSHALL");

  pthread_mutex_lock(&cache_lock);
  flush_old_values(-1);
  pthread_mutex_unlock(&cache_lock);

  status = swrite(fd, answer, strlen(answer));
  if (status < 0)
  {
    status = errno;
    RRDD_LOG(LOG_INFO, "handle_request_flushall: swrite returned an error.");
  }

  return (status);
} /* }}} static int handle_request_flushall */

static int handle_request_update (int fd, /* {{{ */
    char *buffer, size_t buffer_size)
{
  char *file;
  int values_num = 0;
  int status;

  time_t now;

  cache_item_t *ci;
  char answer[CMD_MAX];

#define RRDD_UPDATE_SEND \
  answer[sizeof (answer) - 1] = 0; \
  status = swrite (fd, answer, strlen (answer)); \
  if (status < 0) \
  { \
    status = errno; \
    RRDD_LOG (LOG_INFO, "handle_request_update: swrite returned an error."); \
    return (status); \
  }

  now = time (NULL);

  status = buffer_get_field (&buffer, &buffer_size, &file);
  if (status != 0)
  {
    strncpy (answer, "-1 Usage: UPDATE <filename> <values> [<values> ...]\n",
        sizeof (answer));
    RRDD_UPDATE_SEND;
    return (0);
  }

  pthread_mutex_lock(&stats_lock);
  stats_updates_received++;
  pthread_mutex_unlock(&stats_lock);

  pthread_mutex_lock (&cache_lock);
  ci = g_tree_lookup (cache_tree, file);

  if (ci == NULL) /* {{{ */
  {
    struct stat statbuf;

    /* don't hold the lock while we setup; stat(2) might block */
    pthread_mutex_unlock(&cache_lock);

    memset (&statbuf, 0, sizeof (statbuf));
    status = stat (file, &statbuf);
    if (status != 0)
    {
      RRDD_LOG (LOG_NOTICE, "handle_request_update: stat (%s) failed.", file);

      status = errno;
      if (status == ENOENT)
        snprintf (answer, sizeof (answer), "-1 No such file: %s\n", file);
      else
        snprintf (answer, sizeof (answer), "-1 stat failed with error %i.\n",
            status);
      RRDD_UPDATE_SEND;
      return (0);
    }
    if (!S_ISREG (statbuf.st_mode))
    {
      snprintf (answer, sizeof (answer), "-1 Not a regular file: %s\n", file);
      RRDD_UPDATE_SEND;
      return (0);
    }
    if (access(file, R_OK|W_OK) != 0)
    {
      snprintf (answer, sizeof (answer), "-1 Cannot read/write %s: %s\n",
                file, rrd_strerror(errno));
      RRDD_UPDATE_SEND;
      return (0);
    }

    ci = (cache_item_t *) malloc (sizeof (cache_item_t));
    if (ci == NULL)
    {
      RRDD_LOG (LOG_ERR, "handle_request_update: malloc failed.");

      strncpy (answer, "-1 malloc failed.\n", sizeof (answer));
      RRDD_UPDATE_SEND;
      return (0);
    }
    memset (ci, 0, sizeof (cache_item_t));

    ci->file = strdup (file);
    if (ci->file == NULL)
    {
      free (ci);
      RRDD_LOG (LOG_ERR, "handle_request_update: strdup failed.");

      strncpy (answer, "-1 strdup failed.\n", sizeof (answer));
      RRDD_UPDATE_SEND;
      return (0);
    }

    _wipe_ci_values(ci, now);
    ci->flags = CI_FLAGS_IN_TREE;

    pthread_mutex_lock(&cache_lock);
    g_tree_insert (cache_tree, (void *) ci->file, (void *) ci);
  } /* }}} */
  assert (ci != NULL);

  while (buffer_size > 0)
  {
    char **temp;
    char *value;

    status = buffer_get_field (&buffer, &buffer_size, &value);
    if (status != 0)
    {
      RRDD_LOG (LOG_INFO, "handle_request_update: Error reading field.");
      break;
    }

    temp = (char **) realloc (ci->values,
        sizeof (char *) * (ci->values_num + 1));
    if (temp == NULL)
    {
      RRDD_LOG (LOG_ERR, "handle_request_update: realloc failed.");
      continue;
    }
    ci->values = temp;

    ci->values[ci->values_num] = strdup (value);
    if (ci->values[ci->values_num] == NULL)
    {
      RRDD_LOG (LOG_ERR, "handle_request_update: strdup failed.");
      continue;
    }
    ci->values_num++;

    values_num++;
  }

  if (((now - ci->last_flush_time) >= config_write_interval)
      && ((ci->flags & CI_FLAGS_IN_QUEUE) == 0)
      && (ci->values_num > 0))
  {
    enqueue_cache_item (ci, TAIL);
  }

  pthread_mutex_unlock (&cache_lock);

  if (values_num < 1)
  {
    strncpy (answer, "-1 No values updated.\n", sizeof (answer));
  }
  else
  {
    snprintf (answer, sizeof (answer), "0 Enqueued %i value%s\n", values_num,
        (values_num == 1) ? "" : "s");
  }
  RRDD_UPDATE_SEND;
  return (0);
#undef RRDD_UPDATE_SEND
} /* }}} int handle_request_update */

/* we came across a "WROTE" entry during journal replay.
 * throw away any values that we have accumulated for this file
 */
static int handle_request_wrote (int fd __attribute__((unused)), /* {{{ */
                                 const char *buffer,
                                 size_t buffer_size __attribute__((unused)))
{
  int i;
  cache_item_t *ci;
  const char *file = buffer;

  pthread_mutex_lock(&cache_lock);

  ci = g_tree_lookup(cache_tree, file);
  if (ci == NULL)
  {
    pthread_mutex_unlock(&cache_lock);
    return (0);
  }

  if (ci->values)
  {
    for (i=0; i < ci->values_num; i++)
      free(ci->values[i]);

    free(ci->values);
  }

  _wipe_ci_values(ci, time(NULL));

  pthread_mutex_unlock(&cache_lock);
  return (0);
} /* }}} int handle_request_wrote */

/* if fd < 0, we are in journal replay mode */
static int handle_request (int fd, char *buffer, size_t buffer_size) /* {{{ */
{
  char *buffer_ptr;
  char *command;
  int status;

  assert (buffer[buffer_size - 1] == '\0');

  buffer_ptr = buffer;
  command = NULL;
  status = buffer_get_field (&buffer_ptr, &buffer_size, &command);
  if (status != 0)
  {
    RRDD_LOG (LOG_INFO, "handle_request: Unable parse command.");
    return (-1);
  }

  if (strcasecmp (command, "update") == 0)
  {
    /* don't re-write updates in replay mode */
    if (fd >= 0)
      journal_write(command, buffer_ptr);

    return (handle_request_update (fd, buffer_ptr, buffer_size));
  }
  else if (strcasecmp (command, "wrote") == 0 && fd < 0)
  {
    /* this is only valid in replay mode */
    return (handle_request_wrote (fd, buffer_ptr, buffer_size));
  }
  else if (strcasecmp (command, "flush") == 0)
  {
    return (handle_request_flush (fd, buffer_ptr, buffer_size));
  }
  else if (strcasecmp (command, "flushall") == 0)
  {
    return (handle_request_flushall(fd));
  }
  else if (strcasecmp (command, "stats") == 0)
  {
    return (handle_request_stats (fd, buffer_ptr, buffer_size));
  }
  else if (strcasecmp (command, "help") == 0)
  {
    return (handle_request_help (fd, buffer_ptr, buffer_size));
  }
  else
  {
    char result[CMD_MAX];

    snprintf (result, sizeof (result), "-1 Unknown command: %s\n", command);
    result[sizeof (result) - 1] = 0;

    status = swrite (fd, result, strlen (result));
    if (status < 0)
    {
      RRDD_LOG (LOG_ERR, "handle_request: swrite failed.");
      return (-1);
    }
  }

  return (0);
} /* }}} int handle_request */

/* MUST NOT hold journal_lock before calling this */
static void journal_rotate(void) /* {{{ */
{
  FILE *old_fh = NULL;

  if (journal_cur == NULL || journal_old == NULL)
    return;

  pthread_mutex_lock(&journal_lock);

  /* we rotate this way (rename before close) so that the we can release
   * the journal lock as fast as possible.  Journal writes to the new
   * journal can proceed immediately after the new file is opened.  The
   * fclose can then block without affecting new updates.
   */
  if (journal_fh != NULL)
  {
    old_fh = journal_fh;
    rename(journal_cur, journal_old);
    ++stats_journal_rotate;
  }

  journal_fh = fopen(journal_cur, "a");
  pthread_mutex_unlock(&journal_lock);

  if (old_fh != NULL)
    fclose(old_fh);

  if (journal_fh == NULL)
  {
    RRDD_LOG(LOG_CRIT,
             "JOURNALING DISABLED: Cannot open journal file '%s' : (%s)",
             journal_cur, rrd_strerror(errno));

    RRDD_LOG(LOG_ERR,
             "JOURNALING DISABLED: All values will be flushed at shutdown");
    config_flush_at_shutdown = 1;
  }

} /* }}} static void journal_rotate */

static void journal_done(void) /* {{{ */
{
  if (journal_cur == NULL)
    return;

  pthread_mutex_lock(&journal_lock);
  if (journal_fh != NULL)
  {
    fclose(journal_fh);
    journal_fh = NULL;
  }

  if (config_flush_at_shutdown)
  {
    RRDD_LOG(LOG_INFO, "removing journals");
    unlink(journal_old);
    unlink(journal_cur);
  }
  else
  {
    RRDD_LOG(LOG_INFO, "expedited shutdown; "
             "journals will be used at next startup");
  }

  pthread_mutex_unlock(&journal_lock);

} /* }}} static void journal_done */

static int journal_write(char *cmd, char *args) /* {{{ */
{
  int chars;

  if (journal_fh == NULL)
    return 0;

  pthread_mutex_lock(&journal_lock);
  chars = fprintf(journal_fh, "%s %s\n", cmd, args);
  pthread_mutex_unlock(&journal_lock);

  if (chars > 0)
  {
    pthread_mutex_lock(&stats_lock);
    stats_journal_bytes += chars;
    pthread_mutex_unlock(&stats_lock);
  }

  return chars;
} /* }}} static int journal_write */

static int journal_replay (const char *file) /* {{{ */
{
  FILE *fh;
  int entry_cnt = 0;
  int fail_cnt = 0;
  uint64_t line = 0;
  char entry[CMD_MAX];

  if (file == NULL) return 0;

  fh = fopen(file, "r");
  if (fh == NULL)
  {
    if (errno != ENOENT)
      RRDD_LOG(LOG_ERR, "journal_replay: cannot open journal file: '%s' (%s)",
               file, rrd_strerror(errno));
    return 0;
  }
  else
    RRDD_LOG(LOG_NOTICE, "replaying from journal: %s", file);

  while(!feof(fh))
  {
    size_t entry_len;

    ++line;
    fgets(entry, sizeof(entry), fh);
    entry_len = strlen(entry);

    /* check \n termination in case journal writing crashed mid-line */
    if (entry_len == 0)
      continue;
    else if (entry[entry_len - 1] != '\n')
    {
      RRDD_LOG(LOG_NOTICE, "Malformed journal entry at line %"PRIu64, line);
      ++fail_cnt;
      continue;
    }

    entry[entry_len - 1] = '\0';

    if (handle_request(-1, entry, entry_len) == 0)
      ++entry_cnt;
    else
      ++fail_cnt;
  }

  fclose(fh);

  if (entry_cnt > 0)
  {
    RRDD_LOG(LOG_INFO, "Replayed %d entries (%d failures)",
             entry_cnt, fail_cnt);
    return 1;
  }
  else
    return 0;

} /* }}} static int journal_replay */

static void *connection_thread_main (void *args) /* {{{ */
{
  pthread_t self;
  int i;
  int fd;
  
  fd = *((int *) args);
  free (args);

  pthread_mutex_lock (&connection_threads_lock);
  {
    pthread_t *temp;

    temp = (pthread_t *) realloc (connection_threads,
        sizeof (pthread_t) * (connection_threads_num + 1));
    if (temp == NULL)
    {
      RRDD_LOG (LOG_ERR, "connection_thread_main: realloc failed.");
    }
    else
    {
      connection_threads = temp;
      connection_threads[connection_threads_num] = pthread_self ();
      connection_threads_num++;
    }
  }
  pthread_mutex_unlock (&connection_threads_lock);

  while (do_shutdown == 0)
  {
    char buffer[CMD_MAX];

    struct pollfd pollfd;
    int status;

    pollfd.fd = fd;
    pollfd.events = POLLIN | POLLPRI;
    pollfd.revents = 0;

    status = poll (&pollfd, 1, /* timeout = */ 500);
    if (do_shutdown)
      break;
    else if (status == 0) /* timeout */
      continue;
    else if (status < 0) /* error */
    {
      status = errno;
      if (status == EINTR)
        continue;
      RRDD_LOG (LOG_ERR, "connection_thread_main: poll(2) failed.");
      continue;
    }

    if ((pollfd.revents & POLLHUP) != 0) /* normal shutdown */
    {
      close (fd);
      break;
    }
    else if ((pollfd.revents & (POLLIN | POLLPRI)) == 0)
    {
      RRDD_LOG (LOG_WARNING, "connection_thread_main: "
          "poll(2) returned something unexpected: %#04hx",
          pollfd.revents);
      close (fd);
      break;
    }

    status = (int) sread (fd, buffer, sizeof (buffer));
    if (status <= 0)
    {
      close (fd);

      if (status < 0)
        RRDD_LOG(LOG_ERR, "connection_thread_main: sread failed.");

      break;
    }

    status = handle_request (fd, buffer, /*buffer_size=*/ status);
    if (status != 0)
      break;
  }

  close(fd);

  self = pthread_self ();
  /* Remove this thread from the connection threads list */
  pthread_mutex_lock (&connection_threads_lock);
  /* Find out own index in the array */
  for (i = 0; i < connection_threads_num; i++)
    if (pthread_equal (connection_threads[i], self) != 0)
      break;
  assert (i < connection_threads_num);

  /* Move the trailing threads forward. */
  if (i < (connection_threads_num - 1))
  {
    memmove (connection_threads + i,
        connection_threads + i + 1,
        sizeof (pthread_t) * (connection_threads_num - i - 1));
  }

  connection_threads_num--;
  pthread_mutex_unlock (&connection_threads_lock);

  return (NULL);
} /* }}} void *connection_thread_main */

static int open_listen_socket_unix (const char *path) /* {{{ */
{
  int fd;
  struct sockaddr_un sa;
  listen_socket_t *temp;
  int status;

  temp = (listen_socket_t *) realloc (listen_fds,
      sizeof (listen_fds[0]) * (listen_fds_num + 1));
  if (temp == NULL)
  {
    RRDD_LOG (LOG_ERR, "open_listen_socket_unix: realloc failed.");
    return (-1);
  }
  listen_fds = temp;
  memset (listen_fds + listen_fds_num, 0, sizeof (listen_fds[0]));

  fd = socket (PF_UNIX, SOCK_STREAM, /* protocol = */ 0);
  if (fd < 0)
  {
    RRDD_LOG (LOG_ERR, "open_listen_socket_unix: socket(2) failed.");
    return (-1);
  }

  memset (&sa, 0, sizeof (sa));
  sa.sun_family = AF_UNIX;
  strncpy (sa.sun_path, path, sizeof (sa.sun_path) - 1);

  status = bind (fd, (struct sockaddr *) &sa, sizeof (sa));
  if (status != 0)
  {
    RRDD_LOG (LOG_ERR, "open_listen_socket_unix: bind(2) failed.");
    close (fd);
    unlink (path);
    return (-1);
  }

  status = listen (fd, /* backlog = */ 10);
  if (status != 0)
  {
    RRDD_LOG (LOG_ERR, "open_listen_socket_unix: listen(2) failed.");
    close (fd);
    unlink (path);
    return (-1);
  }
  
  listen_fds[listen_fds_num].fd = fd;
  snprintf (listen_fds[listen_fds_num].path,
      sizeof (listen_fds[listen_fds_num].path) - 1,
      "unix:%s", path);
  listen_fds_num++;

  return (0);
} /* }}} int open_listen_socket_unix */

static int open_listen_socket (const char *addr_orig) /* {{{ */
{
  struct addrinfo ai_hints;
  struct addrinfo *ai_res;
  struct addrinfo *ai_ptr;
  char addr_copy[NI_MAXHOST];
  char *addr;
  char *port;
  int status;

  assert (addr_orig != NULL);

  strncpy (addr_copy, addr_orig, sizeof (addr_copy));
  addr_copy[sizeof (addr_copy) - 1] = 0;
  addr = addr_copy;

  if (strncmp ("unix:", addr, strlen ("unix:")) == 0)
    return (open_listen_socket_unix (addr + strlen ("unix:")));
  else if (addr[0] == '/')
    return (open_listen_socket_unix (addr));

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
      RRDD_LOG (LOG_ERR, "open_listen_socket: Malformed address: %s",
          addr_orig);
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
      RRDD_LOG (LOG_ERR, "open_listen_socket: Garbage after address: %s",
          port);
      return (-1);
    }
  } /* if (*addr = ']') */
  else if (strchr (addr, '.') != NULL) /* Hostname or IPv4 */
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
    RRDD_LOG (LOG_ERR, "open_listen_socket: getaddrinfo(%s) failed: "
        "%s", addr, gai_strerror (status));
    return (-1);
  }

  for (ai_ptr = ai_res; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next)
  {
    int fd;
    listen_socket_t *temp;
    int one = 1;

    temp = (listen_socket_t *) realloc (listen_fds,
        sizeof (listen_fds[0]) * (listen_fds_num + 1));
    if (temp == NULL)
    {
      RRDD_LOG (LOG_ERR, "open_listen_socket: realloc failed.");
      continue;
    }
    listen_fds = temp;
    memset (listen_fds + listen_fds_num, 0, sizeof (listen_fds[0]));

    fd = socket (ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
    if (fd < 0)
    {
      RRDD_LOG (LOG_ERR, "open_listen_socket: socket(2) failed.");
      continue;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    status = bind (fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
    if (status != 0)
    {
      RRDD_LOG (LOG_ERR, "open_listen_socket: bind(2) failed.");
      close (fd);
      continue;
    }

    status = listen (fd, /* backlog = */ 10);
    if (status != 0)
    {
      RRDD_LOG (LOG_ERR, "open_listen_socket: listen(2) failed.");
      close (fd);
      return (-1);
    }

    listen_fds[listen_fds_num].fd = fd;
    strncpy (listen_fds[listen_fds_num].path, addr,
        sizeof (listen_fds[listen_fds_num].path) - 1);
    listen_fds_num++;
  } /* for (ai_ptr) */

  return (0);
} /* }}} int open_listen_socket */

static int close_listen_sockets (void) /* {{{ */
{
  size_t i;

  for (i = 0; i < listen_fds_num; i++)
  {
    close (listen_fds[i].fd);
    if (strncmp ("unix:", listen_fds[i].path, strlen ("unix:")) == 0)
      unlink (listen_fds[i].path + strlen ("unix:"));
  }

  free (listen_fds);
  listen_fds = NULL;
  listen_fds_num = 0;

  return (0);
} /* }}} int close_listen_sockets */

static void *listen_thread_main (void *args __attribute__((unused))) /* {{{ */
{
  struct pollfd *pollfds;
  int pollfds_num;
  int status;
  int i;

  for (i = 0; i < config_listen_address_list_len; i++)
    open_listen_socket (config_listen_address_list[i]);

  if (config_listen_address_list_len < 1)
    open_listen_socket (RRDCACHED_DEFAULT_ADDRESS);

  if (listen_fds_num < 1)
  {
    RRDD_LOG (LOG_ERR, "listen_thread_main: No listen sockets "
        "could be opened. Sorry.");
    return (NULL);
  }

  pollfds_num = listen_fds_num;
  pollfds = (struct pollfd *) malloc (sizeof (*pollfds) * pollfds_num);
  if (pollfds == NULL)
  {
    RRDD_LOG (LOG_ERR, "listen_thread_main: malloc failed.");
    return (NULL);
  }
  memset (pollfds, 0, sizeof (*pollfds) * pollfds_num);

  RRDD_LOG(LOG_INFO, "listening for connections");

  while (do_shutdown == 0)
  {
    assert (pollfds_num == ((int) listen_fds_num));
    for (i = 0; i < pollfds_num; i++)
    {
      pollfds[i].fd = listen_fds[i].fd;
      pollfds[i].events = POLLIN | POLLPRI;
      pollfds[i].revents = 0;
    }

    status = poll (pollfds, pollfds_num, /* timeout = */ 1000);
    if (do_shutdown)
      break;
    else if (status == 0) /* timeout */
      continue;
    else if (status < 0) /* error */
    {
      status = errno;
      if (status != EINTR)
      {
        RRDD_LOG (LOG_ERR, "listen_thread_main: poll(2) failed.");
      }
      continue;
    }

    for (i = 0; i < pollfds_num; i++)
    {
      int *client_sd;
      struct sockaddr_storage client_sa;
      socklen_t client_sa_size;
      pthread_t tid;
      pthread_attr_t attr;

      if (pollfds[i].revents == 0)
        continue;

      if ((pollfds[i].revents & (POLLIN | POLLPRI)) == 0)
      {
        RRDD_LOG (LOG_ERR, "listen_thread_main: "
            "poll(2) returned something unexpected for listen FD #%i.",
            pollfds[i].fd);
        continue;
      }

      client_sd = (int *) malloc (sizeof (int));
      if (client_sd == NULL)
      {
        RRDD_LOG (LOG_ERR, "listen_thread_main: malloc failed.");
        continue;
      }

      client_sa_size = sizeof (client_sa);
      *client_sd = accept (pollfds[i].fd,
          (struct sockaddr *) &client_sa, &client_sa_size);
      if (*client_sd < 0)
      {
        RRDD_LOG (LOG_ERR, "listen_thread_main: accept(2) failed.");
        continue;
      }

      pthread_attr_init (&attr);
      pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);

      status = pthread_create (&tid, &attr, connection_thread_main,
          /* args = */ (void *) client_sd);
      if (status != 0)
      {
        RRDD_LOG (LOG_ERR, "listen_thread_main: pthread_create failed.");
        close (*client_sd);
        free (client_sd);
        continue;
      }
    } /* for (pollfds_num) */
  } /* while (do_shutdown == 0) */

  RRDD_LOG(LOG_INFO, "starting shutdown");

  close_listen_sockets ();

  pthread_mutex_lock (&connection_threads_lock);
  while (connection_threads_num > 0)
  {
    pthread_t wait_for;

    wait_for = connection_threads[0];

    pthread_mutex_unlock (&connection_threads_lock);
    pthread_join (wait_for, /* retval = */ NULL);
    pthread_mutex_lock (&connection_threads_lock);
  }
  pthread_mutex_unlock (&connection_threads_lock);

  return (NULL);
} /* }}} void *listen_thread_main */

static int daemonize (void) /* {{{ */
{
  int status;
  int fd;

  fd = open_pidfile();
  if (fd < 0) return fd;

  if (!stay_foreground)
  {
    pid_t child;
    char *base_dir;

    child = fork ();
    if (child < 0)
    {
      fprintf (stderr, "daemonize: fork(2) failed.\n");
      return (-1);
    }
    else if (child > 0)
    {
      return (1);
    }

    /* Change into the /tmp directory. */
    base_dir = (config_base_dir != NULL)
      ? config_base_dir
      : "/tmp";
    status = chdir (base_dir);
    if (status != 0)
    {
      fprintf (stderr, "daemonize: chdir (%s) failed.\n", base_dir);
      return (-1);
    }

    /* Become session leader */
    setsid ();

    /* Open the first three file descriptors to /dev/null */
    close (2);
    close (1);
    close (0);

    open ("/dev/null", O_RDWR);
    dup (0);
    dup (0);
  } /* if (!stay_foreground) */

  install_signal_handlers();

  openlog ("rrdcached", LOG_PID, LOG_DAEMON);
  RRDD_LOG(LOG_INFO, "starting up");

  cache_tree = g_tree_new ((GCompareFunc) strcmp);
  if (cache_tree == NULL)
  {
    RRDD_LOG (LOG_ERR, "daemonize: g_tree_new failed.");
    return (-1);
  }

  status = write_pidfile (fd);
  return status;
} /* }}} int daemonize */

static int cleanup (void) /* {{{ */
{
  do_shutdown++;

  pthread_cond_signal (&cache_cond);
  pthread_join (queue_thread, /* return = */ NULL);

  remove_pidfile ();

  RRDD_LOG(LOG_INFO, "goodbye");
  closelog ();

  return (0);
} /* }}} int cleanup */

static int read_options (int argc, char **argv) /* {{{ */
{
  int option;
  int status = 0;

  while ((option = getopt(argc, argv, "gl:f:w:b:z:p:j:h?F")) != -1)
  {
    switch (option)
    {
      case 'g':
        stay_foreground=1;
        break;

      case 'l':
      {
        char **temp;

        temp = (char **) realloc (config_listen_address_list,
            sizeof (char *) * (config_listen_address_list_len + 1));
        if (temp == NULL)
        {
          fprintf (stderr, "read_options: realloc failed.\n");
          return (2);
        }
        config_listen_address_list = temp;

        temp[config_listen_address_list_len] = strdup (optarg);
        if (temp[config_listen_address_list_len] == NULL)
        {
          fprintf (stderr, "read_options: strdup failed.\n");
          return (2);
        }
        config_listen_address_list_len++;
      }
      break;

      case 'f':
      {
        int temp;

        temp = atoi (optarg);
        if (temp > 0)
          config_flush_interval = temp;
        else
        {
          fprintf (stderr, "Invalid flush interval: %s\n", optarg);
          status = 3;
        }
      }
      break;

      case 'w':
      {
        int temp;

        temp = atoi (optarg);
        if (temp > 0)
          config_write_interval = temp;
        else
        {
          fprintf (stderr, "Invalid write interval: %s\n", optarg);
          status = 2;
        }
      }
      break;

      case 'z':
      {
        int temp;

        temp = atoi(optarg);
        if (temp > 0)
          config_write_jitter = temp;
        else
        {
          fprintf (stderr, "Invalid write jitter: -z %s\n", optarg);
          status = 2;
        }

        break;
      }

      case 'b':
      {
        size_t len;

        if (config_base_dir != NULL)
          free (config_base_dir);
        config_base_dir = strdup (optarg);
        if (config_base_dir == NULL)
        {
          fprintf (stderr, "read_options: strdup failed.\n");
          return (3);
        }

        len = strlen (config_base_dir);
        while ((len > 0) && (config_base_dir[len - 1] == '/'))
        {
          config_base_dir[len - 1] = 0;
          len--;
        }

        if (len < 1)
        {
          fprintf (stderr, "Invalid base directory: %s\n", optarg);
          return (4);
        }
      }
      break;

      case 'p':
      {
        if (config_pid_file != NULL)
          free (config_pid_file);
        config_pid_file = strdup (optarg);
        if (config_pid_file == NULL)
        {
          fprintf (stderr, "read_options: strdup failed.\n");
          return (3);
        }
      }
      break;

      case 'F':
        config_flush_at_shutdown = 1;
        break;

      case 'j':
      {
        struct stat statbuf;
        const char *dir = optarg;

        status = stat(dir, &statbuf);
        if (status != 0)
        {
          fprintf(stderr, "Cannot stat '%s' : %s\n", dir, rrd_strerror(errno));
          return 6;
        }

        if (!S_ISDIR(statbuf.st_mode)
            || access(dir, R_OK|W_OK|X_OK) != 0)
        {
          fprintf(stderr, "Must specify a writable directory with -j! (%s)\n",
                  errno ? rrd_strerror(errno) : "");
          return 6;
        }

        journal_cur = malloc(PATH_MAX + 1);
        journal_old = malloc(PATH_MAX + 1);
        if (journal_cur == NULL || journal_old == NULL)
        {
          fprintf(stderr, "malloc failure for journal files\n");
          return 6;
        }
        else 
        {
          snprintf(journal_cur, PATH_MAX, "%s/rrd.journal", dir);
          snprintf(journal_old, PATH_MAX, "%s/rrd.journal.old", dir);
        }
      }
      break;

      case 'h':
      case '?':
        printf ("RRDCacheD %s  Copyright (C) 2008 Florian octo Forster\n"
            "\n"
            "Usage: rrdcached [options]\n"
            "\n"
            "Valid options are:\n"
            "  -l <address>  Socket address to listen to.\n"
            "  -w <seconds>  Interval in which to write data.\n"
            "  -z <delay>    Delay writes up to <delay> seconds to spread load\n"
            "  -f <seconds>  Interval in which to flush dead data.\n"
            "  -p <file>     Location of the PID-file.\n"
            "  -b <dir>      Base directory to change to.\n"
            "  -g            Do not fork and run in the foreground.\n"
            "  -j <dir>      Directory in which to create the journal files.\n"
            "  -F            Always flush all updates at shutdown\n"
            "\n"
            "For more information and a detailed description of all options "
            "please refer\n"
            "to the rrdcached(1) manual page.\n",
            VERSION);
        status = -1;
        break;
    } /* switch (option) */
  } /* while (getopt) */

  /* advise the user when values are not sane */
  if (config_flush_interval < 2 * config_write_interval)
    fprintf(stderr, "WARNING: flush interval (-f) should be at least"
            " 2x write interval (-w) !\n");
  if (config_write_jitter > config_write_interval)
    fprintf(stderr, "WARNING: write delay (-z) should NOT be larger than"
            " write interval (-w) !\n");

  if (journal_cur == NULL)
    config_flush_at_shutdown = 1;

  return (status);
} /* }}} int read_options */

int main (int argc, char **argv)
{
  int status;

  status = read_options (argc, argv);
  if (status != 0)
  {
    if (status < 0)
      status = 0;
    return (status);
  }

  status = daemonize ();
  if (status == 1)
  {
    struct sigaction sigchld;

    memset (&sigchld, 0, sizeof (sigchld));
    sigchld.sa_handler = SIG_IGN;
    sigaction (SIGCHLD, &sigchld, NULL);

    return (0);
  }
  else if (status != 0)
  {
    fprintf (stderr, "daemonize failed, exiting.\n");
    return (1);
  }

  if (journal_cur != NULL)
  {
    int had_journal = 0;

    pthread_mutex_lock(&journal_lock);

    RRDD_LOG(LOG_INFO, "checking for journal files");

    had_journal += journal_replay(journal_old);
    had_journal += journal_replay(journal_cur);

    if (had_journal)
      flush_old_values(-1);

    pthread_mutex_unlock(&journal_lock);
    journal_rotate();

    RRDD_LOG(LOG_INFO, "journal processing complete");
  }

  /* start the queue thread */
  memset (&queue_thread, 0, sizeof (queue_thread));
  status = pthread_create (&queue_thread,
                           NULL, /* attr */
                           queue_thread_main,
                           NULL); /* args */
  if (status != 0)
  {
    RRDD_LOG (LOG_ERR, "FATAL: cannot create queue thread");
    cleanup();
    return (1);
  }

  listen_thread_main (NULL);
  cleanup ();

  return (0);
} /* int main */

/*
 * vim: set sw=2 sts=2 ts=8 et fdm=marker :
 */
