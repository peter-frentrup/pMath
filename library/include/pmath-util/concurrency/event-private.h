#ifndef __PMATH_UTIL__CONCURRENCY__EVENT_PRIVATE_H__
#define __PMATH_UTIL__CONCURRENCY__EVENT_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

#include <pmath-util/concurrency/atomic.h>

#ifdef PMATH_OS_WIN32
  #define NOGDI
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>

  typedef HANDLE pmath_event_t;

#else
  #include <pthread.h>

  typedef struct {
    pthread_cond_t  cond;
    pthread_mutex_t mutex;
    pmath_atomic_t  is_signaled; // we don't want to loose a signal
  } pmath_event_t;

#endif

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
pmath_bool_t _pmath_event_init(pmath_event_t *event);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_event_destroy(pmath_event_t *event);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_event_wait(pmath_event_t *event);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_event_timedwait(pmath_event_t *event, double abs_timeout);

PMATH_PRIVATE
PMATH_ATTRIBUTE_NONNULL(1)
void _pmath_event_signal(pmath_event_t *event);

#endif /* __PMATH_UTIL__CONCURRENCY__EVENT_PRIVATE_H__ */
