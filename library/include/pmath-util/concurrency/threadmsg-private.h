#ifndef __PMATH_UTIL__CONCURRENCY__THREADMSG_PRIVATE_H__
#define __PMATH_UTIL__CONCURRENCY__THREADMSG_PRIVATE_H__

struct _pmath_thread_t;

PMATH_PRIVATE
void _pmath_msq_queue_awake_all(void);

PMATH_PRIVATE
pmath_messages_t _pmath_msg_queue_create(void);

PMATH_PRIVATE
void _pmath_msg_queue_inform_death(pmath_messages_t mq);

// me must be the current thread or a parent
PMATH_PRIVATE
void _pmath_msq_queue_handle_next(struct _pmath_thread_t *me);

// me must be the current thread
PMATH_PRIVATE
void _pmath_msq_queue_set_child(
  struct _pmath_thread_t *me, 
  struct _pmath_thread_t *child);

#endif /* __PMATH_UTIL__CONCURRENCY__THREADMSG_PRIVATE_H__ */
