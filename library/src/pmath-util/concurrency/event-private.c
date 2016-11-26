#include <pmath-util/concurrency/event-private.h>
#include <pmath-util/concurrency/threadmsg-private.h>

#include <pmath-util/debug.h>


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
  if(timeout_tick - now >= 0) {
    double t = (timeout_tick - now) * 1000;
    
    if(t >= INFINITE)
      WaitForSingleObject(*event, INFINITE);
    else
      WaitForSingleObject(*event, (DWORD)t);
  }
}

PMATH_PRIVATE
void _pmath_event_signal(pmath_event_t *event) {
  SetEvent(*event);
}

#else

#  include <errno.h>
#  include <math.h>
#  include <limits.h>

PMATH_PRIVATE
pmath_bool_t _pmath_event_init(pmath_event_t *event) {
  pthread_condattr_t condatt;
  pmath_bool_t       success;
  int ret;
  
  ret = pthread_condattr_init(&condatt);
  if(ret) {
    pmath_debug_print("[pthread_condattr_init returned error %d]\n", ret);
  }
  
  ret = pthread_condattr_setclock(&condatt, _pmath_tickcount_clockid);
  if(ret) {
    pmath_debug_print("[pthread_condattr_setclock returned error %d]\n", ret);
  }
  
  ret = pthread_condattr_getclock(&condatt, &event->cond_clock_id);
  if(ret) {
    event->cond_clock_id = CLOCK_REALTIME;
    pmath_debug_print("[pthread_condattr_getclock returned error %d]\n", ret);
  }
  
  success = (0 == pthread_cond_init(&event->cond, &condatt));
  
  pthread_condattr_destroy(&condatt);
  
  if(!success)
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

  if(pmath_atomic_fetch_set(&event->is_signaled, 0) == 0) {
    int ret = pthread_cond_wait(&event->cond, &event->mutex);
    if(ret && ret != ETIMEDOUT)
      pmath_debug_print("[pthread_cond_wait retuned error %d]\n", ret);
  }
  
  pthread_mutex_unlock(&event->mutex);
}

PMATH_PRIVATE
void _pmath_event_timedwait(pmath_event_t *event, double timeout_tick) {
  struct timespec ts = {0, 0};
  
  if(timeout_tick > INT_MAX) { // TODO: consider sizeof(time_t) != sizeof(int)
    _pmath_event_wait(event);
    return;
  }
  
  if(event->cond_clock_id != _pmath_tickcount_clockid) {
    double delta_seconds = timeout_tick - pmath_tickcount();
    
    clock_gettime(event->cond_clock_id, &ts);
    timeout_tick = (double)ts.tv_sec + ts.tv_nsec * 1e-9;
    timeout_tick+= delta_seconds;
  }
  
  ts.tv_sec  = (time_t)floor(timeout_tick);
  ts.tv_nsec = (long)((timeout_tick - ts.tv_sec) * 1e9);//(long)fmod(timeout_tick * 1.0e9, 1.0e9);
  
  pthread_mutex_lock(&event->mutex);

  if(pmath_atomic_fetch_set(&event->is_signaled, 0) == 0) {
    int ret = pthread_cond_timedwait(&event->cond, &event->mutex, &ts);
    if(ret && ret != ETIMEDOUT)
      pmath_debug_print("[pthread_cond_timedwait retuned error %d]\n", ret);
  }
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
