#ifndef __PMATH_UTIL__CONCURRENCY__THREADPOOL_H__
#define __PMATH_UTIL__CONCURRENCY__THREADPOOL_H__

#include <pmath-util/concurrency/threadmsg.h>

/** \todo document pmath-util/concurrency/threadpool.h */

typedef struct _pmath_task_t *pmath_task_t;

PMATH_API
pmath_task_t pmath_task_ref(pmath_task_t task);

PMATH_API
void pmath_task_unref(pmath_task_t task);

PMATH_API
void *pmath_task_get_data(pmath_task_t task);

PMATH_API
pmath_bool_t pmath_task_has_destructor(
  pmath_task_t      task,
  pmath_callback_t  dtor);

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
PMATH_ATTRIBUTE_NONNULL(1, 2)
pmath_task_t pmath_task_new(
  pmath_callback_t run,
  pmath_callback_t destroy,
  void *data);

PMATH_API
void pmath_task_wait(pmath_task_t task);

/**\brief Create a new deamon thread.
   \relates pmath_thread_t
   \param callback The thread function.
   \param kill An optional function to inform the thread that it will be killed.
   \param data A pointer to be passed to callback() and kill()
   \return A reference to the new thread's message queue or PMATH_NULL on error. You
           have to destroy the result.

   pMath will automatically kill any daemon thread when there are no other
   threads remaining (normaly, when pmath_done() is called from main()). Killing
   deamons works as follows:

   \code
for each deamon thread t:
    call t->kill() if the method exists
    throw PMATH_ABORT_EXCEPTION in t

for each deamon thread t:
    wait for t to finish (to return from t->callback)
   \endcode

   pmath_init() will be called automatically before callback() and pmath_done()
   after callback returns. So the pMath thread (pmath_thread_get_current()) will 
   be already initialized and you must not call these two functions in the
   \a callback routine.

   You can use the \a kill function to set your own abort-please-flags if
   nessecary.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_messages_t pmath_thread_fork_daemon(
  pmath_callback_t  callback,
  pmath_callback_t  kill,
  void             *data);


/**\brief Create a new system thread.
   \relates pmath_thread_t
   \param init     An optional function to be called in the new thread before 
                   pmath_thread_fork_unmanaged() returns.
   \param callback The thread function.
   \param data A pointer to be passed to callback()
   \return TRUE if a new thead was created and init() returned TRUE there.
   
   This function is just a wrapper around operating system functions. It can be
   used without initializing the pMath library.
 */
PMATH_API
pmath_bool_t pmath_thread_fork_unmanaged(
  pmath_bool_t    (*init)(    void*),
  void            (*callback)(void*),
  void             *data);


/**\brief Collect unused symbols with the Temporary attribute.
   \relates pmath_symbol_t

   This function is called periodically by the garbage collector.

   \see pmath_symbol_attributes_t
 */
PMATH_API
void pmath_collect_temporary_symbols(void);

#endif /* __PMATH_UTIL__CONCURRENCY__THREADPOOL_H__ */
