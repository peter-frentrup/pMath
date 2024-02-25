#ifndef __PMATH_UTIL__CONCURRENCY__THREADS_H__
#define __PMATH_UTIL__CONCURRENCY__THREADS_H__

#include <pmath-core/objects-inline.h>

/**\defgroup threads Multithreading with pMath
   \brief The Thread abstraction in pMath.
   
   pMath stores several data local to a thread. Therefor, it maintains a 
   pmath_thread_t in every operating system thread it runs on. Those 
   pmath_thread_t variables are created and freed via pmath_init() and 
   pmath_done() respectively. Thus, you have to call those two functions once in 
   every thread that uses pMath functions (and abort the thread if pmath_init() 
   fails).
   
   pMath Threads can have parents. While one thread is running, its parent 
   thread waits (for all its children) and is effectively immutable. This way, 
   child threads can read their parent thread's local variables.

   \section section_thread_syncronization Synchronization
   In other environments, you normaly do synchronization with mutexes and the 
   like. But if we did so, a deadlock could occur when a mutex is allready
   locked by the parent thread, which in turn is waiting for its children to 
   finish.

   The solution is to use pMath threadlocks: You simply synchronize with a
   \ref pmath_symbol_t through pmath_symbol_synchronized() or directly with a
   \ref pmath_threadlock_t and pmath_thread_call_locked(). This is reentrant
   and locks execution to a given thread \em and its child threads. pMath cares
   about avoiding deadlocks behind the scenes.

   Note that threadlocks are needed only if the syncronized code might create
   child threads or calls other code that utilizes thread locks.
   Threads might be created by pmath_evaluate() & co.

   In other situations, you should use mutexes/semaphores from your operating 
   system library or spinlocks (see pmath_atomic_lock() and 
   pmath_atomic_unlock() ), because they are much faster.

   For some simple changes on global integer/pointer variables, you can use 
   \ref atomic_ops.
   @{
 */
 
/**\class pmath_thread_t
   \brief The Representation of a thread.

   Every operating system thread that runs pMath functions has its own 
   pmath_thread_t after it successfuly initialized with pmath_init().
   \todo Implement pmath_run_parallel(number_of_parallel_threads, callback).
 */
typedef struct _pmath_thread_t *pmath_thread_t;

/**\brief Get the current pMath thread.
   \relates pmath_thread_t
   \return A \ref pmath_thread_t. This is PMATH_NULL, if you did not register the 
           current thread to pMath via pmath_init().
 */
PMATH_API 
PMATH_ATTRIBUTE_PURE 
pmath_thread_t pmath_thread_get_current(void);

/**\brief Get a thread's direct parent.
   \relates pmath_thread_t
   \param thread A thread.
   \return The direct parent of thread. Usualy PMATH_NULL.
 */
PMATH_API 
pmath_thread_t pmath_thread_get_parent(pmath_thread_t thread);

/**\brief Queries whether a thread is one of the parents of another.
   \relates pmath_thread_t
   \param parent A thread.
   \param child A thread.
   \return TRUE, if parent is a parent thread of child or if parent == child.
           FALSE otherwise.

   It is important to know that a parent thread is never executed in parallel 
   with its children. However, to check for threads that depend on \a child
   (e.q. to evaluate a funciton on a specific thread or any thread that it waits
   on), use pmath_thread_queue_is_blocked_by().
 */
PMATH_API 
pmath_bool_t pmath_thread_is_parent(
  pmath_thread_t parent,
  pmath_thread_t child);

/**\brief Store a thread/thread-local value.
   \param key The key that can be used to obtain the value with
          _pmath_thread_local_load(). It wont be freed.
   \param value The thread/thread-local value. It will be freed.
   \return \ref pmath_type_t "PMATH_UNDEFINED" or the previous value that was 
           stored with the same key. You must destroy it.

   Note that keys of the form `symbol::tag` are used to store whether a message
   should be suppressed (value System`Off) or not (value PMATH_NULL).

   All keys that are \ref pmath_type_t "magic numbers", have special meanings 
   for pmath_thread_local_save(). You should not use them as the a key.

   Keys which are only symbols are used for thread-local symbols
   (\see pmath_symbol_attributes_t).
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT 
pmath_t pmath_thread_local_save(
  pmath_t key,
  pmath_t value);

/**\brief Load a thread/thread-local value.
   \param key A key that was used to save the value with _pmath_thread_local_save()
          before. It wont be freed.
   \return PMATH_UNDEFINED or the stored value. You must destroy the it.
   
   If there is nothing stored for key in the current thread, its parent threads are
   processed. If none of them stores something under `key` and key is a symbol,
   The global value is used.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_thread_local_load(
  pmath_t key);

/*----------------------------------------------------------------------------*/

/**\brief Throw an exception.
   \param exception The exception to be thrown. It will be freed. You cannot 
          throw the \ref pmath_type_t "magic number PMATH_UNDEFINED".
     
   If there is already an uncought exception, this new exception is lost.
 */
PMATH_API void pmath_throw(pmath_t exception);

/**\brief Catch any exception.
   \return exception The exception to be thrown. If there is no exception 
           available, PMATH_UNDEFINED will be returned.
   
   If you cannot handle the exception, you can re-throw it with pmath_throw().
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_catch(void);

/*----------------------------------------------------------------------------*/

/**\brief Queries whether pMath was requested to abort the evaluation of the 
          current thread.
   \return Whether the user called pmath_abort_please() or an exception was
           thrown or a time-out is passed.
 */
PMATH_API 
pmath_bool_t pmath_aborting(void);

/**\brief Queries whether pMath was requested to abort the evaluation of a 
          specific thread or its parents.
   \relates pmath_thread_t
   \param thread A thread that should be tested.
   \return Whether the given thread should abort evaluation.
   
   \see pmath_aborting
 */
PMATH_API 
pmath_bool_t pmath_thread_aborting(pmath_thread_t thread);

/**\brief Requests pMath to abort the current evaluation.
   
   This function is signal-safe.
   
   \see pmath_continue_after_abort()
 */
PMATH_API 
void pmath_abort_please(void);

/**\brief Requests pMath to stop aborting the current evaluation.
   \ingroup frontend
   \return Whether the global aborting-flag was set (by pmath_abort_please())

   This is for use in front-ends to allow the user to continue working after he 
   or she aborted an evaluation.
   
   Any uncought exception will be deleted silently.
   
   This function also clears the $MessageCount cache.

   \see pmath_abort_please()
 */
PMATH_API 
pmath_bool_t pmath_continue_after_abort(void);

/*----------------------------------------------------------------------------*/

/**\brief Suspend all other threads.
   This function does not realy suspend threads immediately. Any other thread, 
   that calls pmath_aborting() (or pmath_thread_aborting()), will block until we
   call pmath_resume_all().
 */
PMATH_API 
void pmath_suspend_all_please(void);

/**\brief Resume all other threads.
   
   \see pmath_suspend_all_please
 */
PMATH_API 
void pmath_resume_all(void);

/** @} */

#endif /* __PMATH_UTIL__CONCURRENCY__THREADS_H__ */
