#ifndef __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__BUILT_IN_FUNCTIONS_H__
#define __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__BUILT_IN_FUNCTIONS_H__

/*#include <cpuid.h>*/

//#if defined(PMATH_X86) || defined(PMATH_AMD64) 
//  #undef pmath_atomic_loop_nop
//  #define pmath_atomic_loop_nop()  __asm __volatile("rep; nop"::)
//#endif

#define pmath_atomic_fetch_add(atom_ptr, delta) \
  ((intptr_t)__sync_fetch_and_add( \
    (intptr_t volatile *)(atom_ptr), \
    (intptr_t)           (delta)))

#define pmath_atomic_fetch_set(atom_ptr, new_value) \
  ((intptr_t)__sync_lock_test_and_set( \
    (intptr_t volatile *)(atom_ptr), \
    (intptr_t)           (new_value)))

#define pmath_atomic_fetch_compare_and_set(atom_ptr, old_value, new_value) \
  ((intptr_t)__sync_val_compare_and_swap( \
    (intptr_t volatile *)(atom_ptr), \
    (intptr_t)           (old_value), \
    (intptr_t)           (new_value)))

#define pmath_atomic_compare_and_set(atom_ptr, old_value, new_value) \
  ((pmath_bool_t)__sync_bool_compare_and_swap( \
    (intptr_t volatile *)(atom_ptr), \
    (intptr_t)           (old_value), \
    (intptr_t)           (new_value)))

PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
pmath_bool_t pmath_atomic_compare_and_set_2(
  intptr_t volatile *atom,
  intptr_t old_value_fst,
  intptr_t old_value_snd,
  intptr_t new_value_fst,
  intptr_t new_value_snd
){
  union{
    #if PMATH_BITSIZE == 32
      int64_t     big;
    #elif PMATH_BITSIZE == 64
      __int128_t  big;
    #endif
    PMATH_DECLARE_ATOMIC_2(arr);
  } cmp, xch;
  
  cmp.arr[0] = old_value_fst;
  cmp.arr[1] = old_value_snd;
  xch.arr[0] = new_value_fst;
  xch.arr[1] = new_value_snd;
  
  return __sync_bool_compare_and_swap(
    #if PMATH_BITSIZE == 32
      (int64_t volatile*)atom,
    #elif PMATH_BITSIZE == 64
      (__int128_t volatile*)atom,
    #endif
    cmp.big,
    xch.big);
}

/*#define pmath_atomic_compare_and_set_2( \
  atom_ptr2, old_value_fst, old_value_snd, new_value_fst, new_value_snd) \
    ((pmath_bool_t)__sync_bool_compare_and_swap(\
      (intptr2_t volatile *)(atom_ptr2), \
      PMATH_MAKE_INTPTR_2(old_value_fst, old_value_snd), \
      PMATH_MAKE_INTPTR_2(new_value_fst, new_value_snd)))*/

PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
pmath_bool_t pmath_atomic_have_cas2(void){
#if 0
  int a,b,c,d;
  __cpuid(1, a, b, c, d);
  #if PMATH_BITSIZE == 64
    return (c & (1 << 13)) != 0; /* CMPXCHG16B support */
  #elif PMATH_BITSIZE == 32
    return (d & (1 << 8)) != 0; /* CPMXCHG8B support */
  #else
    #error unsupported PMATH_BITSIZE
  #endif
#elif defined(PMATH_AMD64)
  uintptr_t ecx;
  
  __asm __volatile(
    "pushq %%rbx     \n\t"
    "pushq %%rdx     \n\t"
    "cpuid           \n\t"
    "popq %%rdx      \n\t"
    "popq %%rbx      \n\t"
  : "=c"(ecx)
  : "a"(1));
  
  return (ecx & (1 << 13)) != 0; /* CMPXCHG16B support */
#elif defined(PMATH_X86)
  uintptr_t edx;
  
  __asm __volatile(
    "pushl %%ebx     \n\t"
    "pushl %%ecx     \n\t"
    "cpuid           \n\t"
    "popl %%ecx      \n\t"
    "popl %%ebx      \n\t"
  : "=d"(edx)
  : "a"(1));
  
  return (edx & (1 << 8)) != 0; /* CPMXCHG8B support */
#else
  return FALSE;
#endif
}

#define pmath_atomic_barrier()  __sync_synchronize()

#define pmath_atomic_lock(atom_ptr) \
  do{ \
    intptr_t volatile *_pmath_atomic_lock__ptr = (intptr_t volatile*)(atom_ptr); \
     \
    int _pmath_atomic_lock__cnt = PMATH_ATOMIC_FASTLOOP_COUNT; \
    while(_pmath_atomic_lock__cnt > 0 && *_pmath_atomic_lock__ptr != 0){ \
      --_pmath_atomic_lock__cnt; \
    } \
     \
    if(*_pmath_atomic_lock__ptr != 0){ \
      pmath_atomic_loop_yield(); \
    } \
    while(0 != __sync_lock_test_and_set(_pmath_atomic_lock__ptr, (intptr_t)1)){ \
      pmath_atomic_loop_nop(); \
    } \
  }while(0)

#define pmath_atomic_unlock(atom_ptr) \
  __sync_lock_release( \
    (intptr_t volatile *)(atom_ptr))

#define HAVE_ATOMIC_OPS

#endif /* __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__BUILT_IN_FUNCTIONS_H__ */
