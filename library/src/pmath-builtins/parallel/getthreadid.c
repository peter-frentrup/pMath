#include <pmath-core/numbers.h>

#include <pmath-builtins/all-symbols-private.h>

#if PMATH_USE_PTHREAD
  #include <pmath-core/numbers-private.h>
  
  #include <pthread.h>
#elif PMATH_USE_WINDOWS_THREADS
  #include <windows.h>
#endif


PMATH_PRIVATE pmath_t builtin_getthreadid(pmath_expr_t expr){
  pmath_unref(expr);
  
  #if PMATH_USE_PTHREAD
    {
      pmath_integer_t result = _pmath_create_mp_int();
      pthread_t thread = pthread_self(); 
      
      if(!pmath_is_null(result)){
        mpz_import(
          PMATH_AS_MPZ(result),
          sizeof(pthread_t),
          1,
          1,
          0,
          0,
          &thread);
      }
      
      return result;
      //return pmath_integer_new_ui((unsigned long)pthread_self());
    }
  #elif PMATH_USE_WINDOWS_THREADS
    return pmath_integer_new_ui((unsigned long)GetCurrentThreadId());
  #endif
}
