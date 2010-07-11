#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pmath-config.h>
#include <pmath-types.h>
#include <pmath-core/objects.h>
#include <pmath-core/expressions.h>
#include <pmath-core/numbers.h>
#include <pmath-core/strings.h>
#include <pmath-core/symbols.h>

#include <pmath-util/messages.h>

#include <pmath-util/concurrency/atomic.h>

#include <pmath-core/objects-inline.h>
#include <pmath-core/numbers-private.h>

#include <pmath-builtins/all-symbols.h>
#include <pmath-builtins/all-symbols-private.h>

#if PMATH_USE_PTHREAD
  #include <pthread.h>
#elif PMATH_USE_WINDOWS_THREADS
  #include <windows.h>
#endif

PMATH_PRIVATE pmath_t builtin_getthreadid(pmath_expr_t expr){
  pmath_unref(expr);
  
  #if PMATH_USE_PTHREAD
    {
      struct _pmath_integer_t *result = _pmath_create_integer();
      pthread_t thread = pthread_self(); 
      
      if(result){
        mpz_import(
          result->value,
          sizeof(pthread_t),
          1,
          1,
          0,
          0,
          &thread);
      }
      
      return (pmath_integer_t)result;
      //return pmath_integer_new_ui((unsigned long)pthread_self());
    }
  #elif PMATH_USE_WINDOWS_THREADS
    return pmath_integer_new_ui((unsigned long)GetCurrentThreadId());
  #endif
}
