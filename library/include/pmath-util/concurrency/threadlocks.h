#ifndef __PMATH_UTIL__CONCURRENCY__THREADLOCKS_H__
#define __PMATH_UTIL__CONCURRENCY__THREADLOCKS_H__

#include <pmath-config.h>
#include <pmath-types.h>

/**\addtogroup threads
   @{
 */

/**\class pmath_threadlock_t
   \brief A reentrant lock for threads.

   A thread lock is like a thread lock, but it does not block child threads of 
   the currently holding thread.
 */
typedef struct _pmath_threadlock_t *pmath_threadlock_t;

/**\brief Execute a function synchronized with a threadlock.
   \relates pmath_threadlock_t
   \param threadlock_ptr A pointer to the threadlock.
   \param callback The function to be executed when the symbol is locked.
   \param data A pointer that will be passed to callback.

   All you have to do is initialize the threadlock \c threadlock_ptr points to 
   with NULL before you call this function for the first time:
   \code
static pmath_threadlock_t lock = NULL;
...
pmath_thread_call_locked(&lock, my_callback, my_data);
   \endcode

   To synchronize with a symbol, use pmath_symbol_synchronized().
 */
PMATH_API 
void pmath_thread_call_locked(
  pmath_threadlock_t  *threadlock_ptr,
  pmath_callback_t     callback,
  void                *data);

/*@}*/

#endif /* __PMATH_UTIL__CONCURRENCY__THREADLOCKS_H__ */
