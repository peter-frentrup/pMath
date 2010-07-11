#ifndef __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__SUN_H__
#define __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__SUN_H__

#ifndef __STDC__
  #warning no __STDC__ defined
  #define __STDC__ 1
  #define PMATH_SUN_HAVE_NO_STDC
#endif

#include <sys/atomic.h>



#define pmath_atomic_fetch_add(ptr, delta) \
  ((intptr_t)atomic_add_ptr_nv(ptr, delta) - (delta))

#define pmath_atomic_fetch_set(ptr, new) \
  ((intptr_t)atomic_swap_ptr((ptr), (void*)(new)))

#define pmath_atomic_fetch_compare_and_set(ptr, old, new) \
  ((intptr_t)atomic_cas_ptr((ptr), (void*)(old), (void*)(new)))
  
#define pmath_atomic_compare_and_set(ptr, old, new) \
  ((old) == pmath_atomic_fetch_compare_and_set((ptr), (old), (new)))

#define pmath_atomic_compare_and_set_2(ptr, old1, old2, new1, new2) \
  (FALSE)
  
#define pmath_atomic_have_cas2() \
  (FALSE)

#define pmath_atomic_barrier()  do{membar_producer();membar_consumer();}while(0)

#define pmath_atomic_lock(ptr)  \
  do{ \
    while(NULL != atomic_swap_ptr((ptr), (void*)1)){ \
      membar_enter(); \
    } \
    membar_enter(); \
  }while(0)
  
#define pmath_atomic_unlock(ptr) \
  do{  \
    membar_exit(); \
    atomic_swap_ptr((ptr), (void*)0); \
  }while(0)

#ifdef PMATH_SUN_HAVE_NO_STDC
  #undef __STDC__
#endif

#endif /* __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__SUN_H__ */
