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
void _pmath_event_timedwait(pmath_event_t *event, double timeout_tick) {
  double now = pmath_tickcount();
  if(timeout_tick - now >= 0)
    WaitForSingleObject(*event, (long)((timeout_tick - now) * 1000));
}

PMATH_PRIVATE
void _pmath_event_signal(pmath_event_t *event) {
  SetEvent(*event);
}

#else

#  include <math.h>

PMATH_PRIVATE
pmath_bool_t _pmath_event_init(pmath_event_t *event) {
  pthread_condattr_t condatt;
  
  if(0 != pthread_cond_init(&event->cond, NULL))
    return FALSE;
  
  pthread_condattr_init(&condatt);
  pthread_condattr_setclock(&condatt, _pmath_tickcount_clockid);

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
void _pmath_event_timedwait(pmath_event_t *event, double timeout_tick) {
  struct timespec ts;

  ts.tv_sec  = (time_t)floor(timeout_tick);
  ts.tv_nsec = (long)((timeout_tick - ts.tv_sec) * 1e9);//(long)fmod(timeout_tick * 1.0e9, 1.0e9);

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
