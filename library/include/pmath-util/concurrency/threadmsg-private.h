#ifndef __PMATH_UTIL__CONCURRENCY__THREADMSG_PRIVATE_H__
#define __PMATH_UTIL__CONCURRENCY__THREADMSG_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

#include <pmath-util/concurrency/threadmsg.h>

struct _pmath_thread_t;

PMATH_PRIVATE
void _pmath_msq_queue_awake_all(void);

PMATH_PRIVATE
pmath_messages_t _pmath_msg_queue_create(void);

PMATH_PRIVATE
void _pmath_msg_queue_inform_death(pmath_messages_t mq);

// me must be the current thread or a parent
PMATH_PRIVATE
void _pmath_msq_queue_handle_next(pmath_thread_t me);

// me must be the current thread
PMATH_PRIVATE
void _pmath_msq_queue_set_child(
  struct _pmath_thread_t *me,
  struct _pmath_thread_t *child);

PMATH_PRIVATE pmath_bool_t _pmath_threadmsg_init(void);
PMATH_PRIVATE void         _pmath_threadmsg_done(void);

#endif /* __PMATH_UTIL__CONCURRENCY__THREADMSG_PRIVATE_H__ */
