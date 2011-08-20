#include <pmath-util/concurrency/event-private.h>
#include <pmath-util/concurrency/threadmsg.h>

#ifdef PMATH_OS_WIN32

PMATH_PRIVATE
pmath_bool_t _pmath_event_init(pmath_event_t *event) {
  *event = CreateEvent(NULL, FALSE, FALSE, NULL);
  return *event != NULL;
}

PMATH_PRIVATE
void _pmath_event_destroy(pmath_event_t *event) {
  CloseHandle(*event);
}

PMATH_PRIVATE
void _pmath_event_wait(pmath_event_t *event) {
  WaitForSingleObject(*event, INFINITE);
}

PMATH_PRIVATE
void _pmath_event_timedwait(pmath_event_t *event, double abs_timeout) {
  double now = pmath_tickcount();
  if(abs_timeout - now >= 0)
    WaitForSingleObject(*event, (long)((abs_timeout - now) * 1000));
}

PMATH_PRIVATE
void _pmath_event_signal(pmath_event_t *event) {
  SetEvent(*event);
}

#else

#include <math.h>

PMATH_PRIVATE
pmath_bool_t _pmath_event_init(pmath_event_t *event) {
  if(0 != pthread_cond_init(&event->cond, NULL))
    return FALSE;

  if(0 != pthread_mutex_init(&event->mutex, NULL)) {
    pthread_cond_destroy(&event->cond);
    return FALSE;
  }

  pmath_atomic_write_release(&event->is_signaled, 0);

  return TRUE;
}

PMATH_PRIVATE
void _pmath_event_destroy(pmath_event_t *event) {
  pthread_cond_destroy( &event->cond);
  pthread_mutex_destroy(&event->mutex);
}

PMATH_PRIVATE
void _pmath_event_wait(pmath_event_t *event) {
  pthread_mutex_lock(&event->mutex);

  if(pmath_atomic_fetch_set(&event->is_signaled, 0) == 0)
    pthread_cond_wait(&event->cond, &event->mutex);

  pthread_mutex_unlock(&event->mutex);
}

PMATH_PRIVATE
void _pmath_event_timedwait(pmath_event_t *event, double abs_timeout) {
  struct timespec ts;

  ts.tv_sec  = (time_t)abs_timeout;
  ts.tv_nsec = (long)fmod(abs_timeout * 1e9, 1e9);

  pthread_mutex_lock(&event->mutex);

  if(pmath_atomic_fetch_set(&event->is_signaled, 0) == 0)
    pthread_cond_timedwait(&event->cond, &event->mutex, &ts);

  pthread_mutex_unlock(&event->mutex);
}

PMATH_PRIVATE
void _pmath_event_signal(pmath_event_t *event) {
  pthread_mutex_lock(&event->mutex);

  pthread_cond_signal(&event->cond);
  pmath_atomic_write_release(&event->is_signaled, 1);

  pthread_mutex_unlock(&event->mutex);
}

#endif
