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
   \return A refernce to the message queue or NULL on error. You must destroy it
   with pmath_unref() when its no longer needed.
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

/**\brief Wake up another thread.
   \relates pmath_thread_t
   \relates pmath_messages_t
   \param mq The message queue associated with the sleeping thread. It wont be 
             freed.
   
   This function wakes up the thread that is associated with the message queue.
   It is safe to try to wake up threads, that are not sleeping.
   
   To follow the loop-style waiting idiom described in pmath_thread_sleep(), you
   must modify <tt>some_wait_condition</tt> \em before calling this function to
   successfully awake the other thread.
 */
PMATH_API
void pmath_thread_wakeup(pmath_messages_t mq);

/**\brief Asynchronously send a message to another thread.
   \relates pmath_thread_t
   \relates pmath_messages_t
   \param mq The receivers message queue. It wont be freed.
   \param msg The message. It will be freed.
   
   The message will be evaluated by the receiver. This function returns 
   immediately. If the receiver cannot handle the message (since it is dead or 
   there is not enough memory), the message will be deleted.
   
   Note that messages might not be handled in the order they were send.
 */
PMATH_API
void pmath_thread_send(pmath_messages_t mq, pmath_t msg);

/**\brief Send a message to another thread and wait for the answer.
   \relates pmath_thread_t
   \relates pmath_messages_t
   \param mq The receivers message queue. It wont be freed.
   \param msg The message. It will be freed.
   \param timeout_seconds The maximum number of seconds tho wait for the answer.
                          Use HUGE_VAL if you do not want a timeout.
   \return The result of pmath_evaluate(message) called by the receiver or 
           PMATH_UNDEFINED in case of an error.
   
   The message will be evaluated by the receiver. If the receiver cannot handle 
   it (since it is dead or there is not enough memory), the message will be 
   deleted.
   
   The calling thread will fall asleep until 
   - it receives an answer to return or
   - the message is deleted or 
   - the timeout is reached or 
   - another abort situation occurs in the calling thread (e.g. 
     pmath_abort_please() is called anywhere in the system)
   
   In the last two cases (timeout or abort), a the remote evaluation will be 
   aborted.
   
   \todo Check, what happens if mq belongs to a parent thread.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_thread_send_wait(
  pmath_messages_t mq, 
  pmath_t   msg,
  double           timeout_seconds);

/**\brief Asynchronously send a message to a thread sometime in the future.
   \relates pmath_thread_t
   \relates pmath_messages_t
   \param mq The receivers message queue. It wont be freed.
   \param msg The message. It will be freed.
   \param seconds The delay in seconds before the message will be delivered.
   
   The message will be evaluated by the receiver. This function returns 
   immediately. If the receiver cannot handle the message (since it is dead or 
   there is not enough memory), the message will be deleted.
 */
PMATH_API
void pmath_thread_send_delayed(
  pmath_messages_t mq, 
  pmath_t   msg,
  double           seconds);

/*@}*/

#endif /* __PMATH_UTIL__CONCURRENCY__THREADMSG_H__ */
