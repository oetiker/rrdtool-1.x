/*
 *  mutex.h - Cross platform mutex
 */

#ifndef MUTEX_H_B13C67AB432C4C39AF823A339537CA40
#define MUTEX_H_B13C67AB432C4C39AF823A339537CA40

#ifdef WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

#ifndef WIN32
#define mutex_t            pthread_mutex_t
#define MUTEX_INITIALIZER  PTHREAD_MUTEX_INITIALIZER
#else
#define mutex_t            HANDLE
#define MUTEX_INITIALIZER  NULL
#endif

int mutex_init(mutex_t *mutex);
int mutex_lock(mutex_t *mutex);
int mutex_unlock(mutex_t *mutex);
int mutex_cleanup(mutex_t *mutex);

#endif /* MUTEX__H */

/*
 * vim: set sw=2 sts=2 ts=8 et fdm=marker :
 */

