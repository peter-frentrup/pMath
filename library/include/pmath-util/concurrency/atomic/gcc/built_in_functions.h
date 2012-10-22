#ifndef __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__BUILT_IN_FUNCTIONS_H__
#define __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__BUILT_IN_FUNCTIONS_H__

/*#include <cpuid.h>*/

//#if defined(PMATH_X86) || defined(PMATH_AMD64) 
//  #undef pmath_atomic_loop_nop
//  #define pmath_atomic_loop_nop()  __asm __volatile("rep; nop"::)
//#endif


PMATH_FORCE_INLINE
void pmath_atomic_barrier(void){
  __sync_synchronize();
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_read_aquire(pmath_atomic_t *atom){
  intptr_t data = atom->_data;
  __sync_synchronize(); // all reads or writes below this line keep below it (__sync_synchronize() is even a full barrier)
  return data;
}


PMATH_FORCE_INLINE
void pmath_atomic_write_release(pmath_atomic_t *atom, intptr_t value){
  __sync_synchronize(); // all reads or writes above this line keep above it (__sync_synchronize() is even a full barrier)
  atom->_data = value;
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_add(pmath_atomic_t *atom, intptr_t delta){
  return __sync_fetch_and_add(&atom->_data, delta);
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_set(pmath_atomic_t *atom, intptr_t delta){
  intptr_t result = __sync_lock_test_and_set(&atom->_data, delta); // has only aquire semantics
  __sync_synchronize(); // has aquire & release semantics
  return result;
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_compare_and_set(pmath_atomic_t *atom, intptr_t old_value, intptr_t new_value){
  return __sync_val_compare_and_swap(&atom->_data, old_value, new_value);
}


PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_compare_and_set(pmath_atomic_t *atom, intptr_t old_value, intptr_t new_value){
  return __sync_bool_compare_and_swap(&atom->_data, old_value, new_value);
}


PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_compare_and_set_2(
  pmath_atomic2_t *atom,
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
    pmath_atomic2_t arr;
  } cmp, xch;
  
  cmp.arr._data[0] = old_value_fst;
  cmp.arr._data[1] = old_value_snd;
  xch.arr._data[0] = new_value_fst;
  xch.arr._data[1] = new_value_snd;
  
  return __sync_bool_compare_and_swap(
    #if PMATH_BITSIZE == 32
      (int64_t volatile*)atom->_data,
    #elif PMATH_BITSIZE == 64
      (__int128_t volatile*)atom->_data,
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
pmath_bool_t pmath_atomic_have_cas2(void){
#if 0
  int a,b,c,d;
  __cpuid(1, a, b, c, d);
  #if PMATH_BITSIZE == 64
    return (c & (1 << 13)) != 0; // CMPXCHG16B support 
  #elif PMATH_BITSIZE == 32
    return (d & (1 << 8)) != 0; // CPMXCHG8B support 
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
  
  return (ecx & (1 << 13)) != 0; // CMPXCHG16B support
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
  
  return (edx & (1 << 8)) != 0; // CPMXCHG8B support
#else
  return FALSE;
#endif
}


PMATH_FORCE_INLINE
void pmath_atomic_lock(pmath_atomic_t *atom){
  int count = PMATH_ATOMIC_FASTLOOP_COUNT;
  
  while(count > 0 && pmath_atomic_read_aquire(atom) != 0){
    --count;
  }
  
  if(pmath_atomic_read_aquire(atom) != 0){
    pmath_atomic_loop_yield();
  }
  
  while(0 != __sync_lock_test_and_set(&atom->_data, (intptr_t)1)){
    pmath_atomic_loop_nop();
  }
}


PMATH_FORCE_INLINE
void pmath_atomic_unlock(pmath_atomic_t *atom){
  __sync_lock_release(&atom->_data);
}

#define HAVE_ATOMIC_OPS

#endif /* __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__BUILT_IN_FUNCTIONS_H__ */
