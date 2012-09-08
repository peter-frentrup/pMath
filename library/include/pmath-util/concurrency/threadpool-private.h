#ifndef __PMATH_UTIL__CONCURRENCY__THREADPOOL_PRIVATE_H__
#define __PMATH_UTIL__CONCURRENCY__THREADPOOL_PRIVATE_H__

#ifndef BUILDING_PMATH
#error This header file is not part of the public pMath API
#endif

#include <pmath-util/concurrency/threadpool.h>
#include <pmath-util/concurrency/threads.h>


struct _pmath_timed_message_t {
  struct _pmath_timed_message_t *_reserved_next;
  
  pmath_t  message_queue; // pmath_messages_t
  double   fire_at_tick;
  pmath_t  subject;
};

/* this is private, because the caller must ensure that thread's parent waits
   for the task and does not run any evaluation. We can only ensure this for
   code inside pMath.
 */
PMATH_PRIVATE
PMATH_ATTRIBUTE_USE_RESULT
PMATH_ATTRIBUTE_NONNULL(1, 2)
pmath_task_t _pmath_task_new_with_thread(
  pmath_callback_t  run,
  pmath_callback_t  destroy,
  void             *data,
  pmath_thread_t    thread); // thread will be freed

PMATH_PRIVATE
pmath_thread_t _pmath_task_get_thread(pmath_task_t task);

PMATH_PRIVATE int _pmath_processor_count(void);

PMATH_PRIVATE void _pmath_register_timed_msg(struct _pmath_timed_message_t *msg);

extern PMATH_PRIVATE pmath_atomic_t _pmath_threadpool_deamon_count;
PMATH_PRIVATE void _pmath_threadpool_kill_daemons(void);

PMATH_PRIVATE pmath_bool_t _pmath_threadpool_init(void);
PMATH_PRIVATE void         _pmath_threadpool_done(void);

#endif /* __PMATH_UTIL__CONCURRENCY__THREADPOOL_PRIVATE_H__ */
