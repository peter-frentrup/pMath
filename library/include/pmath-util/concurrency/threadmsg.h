#ifndef __PMATH_UTIL__CONCURRENCY__THREADMSG_H__
#define __PMATH_UTIL__CONCURRENCY__THREADMSG_H__

#include <pmath-core/custom.h>

/**\defgroup threadmsg Thread Messaging
   \brief Sending messages to other threads.

   Every pMath thread has its own message queue. Other threads can send
   messages to such a queue and optionally wait for a result. Messages to any
   queue can also be registered for delivery at a later point in time.

   Threads can go to sleep when they have no work to do. They will be awaken any
   time a message arrives to handle it.

   Technical Note: Pending messages are handled as soon as time pmath_aborting()
   is called, which happens periodically. For the pMath code, it looks like
   asynchronous signals, because messages can occur any time during the
   evaluation. But from the native code's point of view, messages are
   synchronous, because they can only occur during pmath_aborting().

   \note Message passing is not signal-safe. You must not send any messages from
   within a UNIX signal handler.

   @{
 */

/**\class pmath_messages_t
   \extends pmath_custom_t
   \brief A message queue for interthread communication.
 */
typedef pmath_custom_t pmath_messages_t;

/**\brief Test if an object is a message queue.
   \relates pmath_messages_t
   \param obj Any pMath object. It wont be freed.
   \return TRUE if the object is a valid message queue object (pmath_messages_t).
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_is_message_queue(pmath_t obj);

/**\brief Get the current thread's message queue.
   \relates pmath_thread_t
   \relates pmath_messages_t
   \return A refernce to the message queue or PMATH_NULL on error. You must 
           destroy it with pmath_unref() when its no longer needed.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_messages_t pmath_thread_get_queue(void);

/**\brief Send the current thread to sleep.
   \relates pmath_thread_t
   \relates pmath_messages_t

   The thread will fall asleep until
   - it receives a message or
   - it is waken up with pmath_thread_wakeup() or
   - an abort-condition (pmath_abort_please() or pmath_throw()) is met \em
     anywhere in the system.

   Because of the last point, this function is normally called in a loop:
   \code
while(!pmath_aborting() && some_wait_condition){
  pmath_thread_sleep();
}
   \endcode
 */
PMATH_API
void pmath_thread_sleep(void);

/**\brief Send the current thread to sleep.
   \relates pmath_thread_t
   \relates pmath_messages_t
   \param timeout_end_tick  When to end the sleep. By means of pmath_tickcount().

   The thread will fall asleep until
   - it receives a message or
   - it is waken up with pmath_thread_wakeup() or
   - an abort-condition (pmath_abort_please() or pmath_throw()) is met \em
     anywhere in the system or
   - pmath_tickcount() passes \arg timeout_end_tick.

   \see pmath_thread_sleep, pmath_tickcount
 */
PMATH_API
void pmath_thread_sleep_timeout(double timeout_end_tick);

/**\brief Measuring durations.
   \return The number of seconds since an arbitrary fixed point of time in the 
           past.
   
   This function can be used for measuring durations. It uses a monotonic clock.
 */
PMATH_API
double pmath_tickcount(void);

/**\brief Gives the seconds since January 1, 1970 (UTC)
   \return The number of seconds since January 1, 1970 (UTC)
   
   This function is *not* appropriate for measureing durations.
 */
PMATH_API
double pmath_datetime(void);

/**\brief Wake up another thread.
   \relates pmath_thread_t
   \relates pmath_messages_t
   \param mq The message queue associated with the sleeping thread. It wont be
             freed.

   This function wakes up the thread that is associated with the message queue.
   It is safe to try to wake up threads that are not sleeping.

   To follow the loop-style waiting idiom described in pmath_thread_sleep(), you
   must modify <tt>some_wait_condition</tt> \em before calling this function to
   successfully awake the other thread.
 */
PMATH_API
void pmath_thread_wakeup(pmath_messages_t mq);

/**\brief Asynchronously send a message to another thread.
   \relates pmath_thread_t
   \relates pmath_messages_t
   \param mq The receivers message queue. It won't be freed.
   \param msg The message. It will be freed.

   The message will be evaluated by the receiver. This function returns
   immediately. If the receiver cannot handle the message (since it is dead or
   there is not enough memory), the message will be discarded.

   Note that messages might not be handled in the order they were send.
 */
PMATH_API
void pmath_thread_send(pmath_messages_t mq, pmath_t msg);

/**\brief Send a message to another thread and wait for the answer.
   \relates pmath_thread_t
   \relates pmath_messages_t
   \param mq The receivers message queue. It won't be freed.
   \param msg The message. It will be freed.
   \param timeout_seconds The maximum number of seconds tho wait for the answer.
                          Use HUGE_VAL if you do not want a timeout.
   \param idle_function An optional function that will be called continuously 
                        until the evaluation finished or timed out. It should 
                        return TRUE when it needs to be called again as soon as 
                        possible, otherwise <tt>pmath_thread_send_wait</tt> will 
                        do a pmath_thread_sleep_timeout(...).
                        You may change \a *end_tick to wait longer for a result. 
   \param idle_data Argument for \arg idle_function.
   \return The result of pmath_evaluate(message) called by the receiver or
           PMATH_UNDEFINED in case of an error.

   The message will be evaluated by the receiver. If the receiver cannot handle
   it (since it is dead or there is not enough memory), the message will be
   discarded.

   The calling thread will fall asleep until
   - it receives an answer to return or
   - the message is discarded or
   - the timeout is reached or
   - another abort situation occurs in the calling thread (e.g.
     pmath_abort_please() is called anywhere in the system)

   In the last two cases (timeout or abort), the remote evaluation will be
   aborted.

   \todo Check, what happens if mq belongs to a parent thread.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_thread_send_wait(
  pmath_messages_t mq,
  pmath_t          msg,
  double           timeout_seconds,
  pmath_bool_t   (*idle_function)(double *end_tick,void *idle_data),
  void            *idle_data);

/**\brief Asynchronously send a message to a thread sometime in the future.
   \relates pmath_thread_t
   \relates pmath_messages_t
   \param mq The receivers message queue. It wont be freed.
   \param msg The message. It will be freed.
   \param seconds The delay in seconds before the message will be delivered.

   The message will be evaluated by the receiver. This function returns
   immediately. If the receiver cannot handle the message (since it is dead or
   there is not enough memory), the message will be discarded.
 */
PMATH_API
void pmath_thread_send_delayed(
  pmath_messages_t mq,
  pmath_t          msg,
  double           seconds);

/**\brief Queries whether a thread is blocked by another thread.
   \ingroup threads
   \relates pmath_thread_t
   \param waiter_mq A message queue. It will be freed
   \param waitee_mq A message queue. It will be freed.
   \return TRUE, if thread which owns waiter_mq is a blocked by the thread which
           owns waitee_mq or if waiter_mq == waitee_mq. FALSE otherwise.

   A use-case for this function is a function that wants to be evaluated on a
   specific thread A or any thread that A waits on. See builtin_interrupt() in
   the reference front-end implementation test.exe
 */
PMATH_API
pmath_bool_t pmath_thread_queue_is_blocked_by(
  pmath_messages_t waiter_mq,
  pmath_messages_t waitee_mq);

/**\brief Execute a function with an interrupt notifier installed.
   \ingroup threads
   \relates pmath_thread_t
   \param callback The function to be called.
   \param notify Is called when a message is delivered to the current thread.
          This function is called by the thread that sends the message, but
          concurrent sending threads wait on each other when calling the
          function. \a notify must not call any function that could send
          messages, because that would lead to a deadlock.
   \param callback_closure The argument for \a callback.
   \param notify_closure The argument for \a notify.
 */
PMATH_API
void pmath_thread_run_with_interrupt_notifier(
  pmath_callback_t   callback,
  pmath_callback_t   notify,
  void              *callback_closure,
  void              *notify_closure
);

/** @} */

#endif /* __PMATH_UTIL__CONCURRENCY__THREADMSG_H__ */
