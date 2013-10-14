/*
 *
 * mutex.c
 *
 * Cross platform mutex
 *
 */

#include "mutex.h"

int mutex_init(mutex_t *mutex)
{
#ifdef WIN32
  *mutex = CreateMutex(NULL, FALSE, NULL);
  return (*mutex == NULL);
#else
  return pthread_mutex_init(mutex, NULL);;
#endif
}

int mutex_lock(mutex_t *mutex)
{
#ifdef WIN32
  if (*mutex == NULL) { /* static initializer? */
    HANDLE p = CreateMutex(NULL, FALSE, NULL);
    if (InterlockedCompareExchangePointer((PVOID*)mutex, (PVOID)p, NULL) != NULL)
      CloseHandle(p);
  }
  return (WaitForSingleObject(*mutex, INFINITE) == WAIT_FAILED);
#else
  return pthread_mutex_lock(mutex);
#endif
}

int mutex_unlock(mutex_t *mutex)
{
#ifdef WIN32
  return (ReleaseMutex(*mutex) == 0);
#else
  return pthread_mutex_unlock(mutex);
#endif
}

int mutex_cleanup(mutex_t *mutex)
{
#ifdef WIN32
  return (CloseHandle(mutex) == 0);
#else
  return pthread_mutex_destroy(mutex);
#endif
}

/*
 * vim: set sw=2 sts=2 ts=8 et fdm=marker :
 */

