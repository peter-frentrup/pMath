#ifndef __PMATH_UTIL__CONCURRENCY__ATOMIC__MSVC__INTRINSIC_FUNCTIONS_H__
#define __PMATH_UTIL__CONCURRENCY__ATOMIC__MSVC__INTRINSIC_FUNCTIONS_H__

#include <intrin.h>
#include <windows.h>


PMATH_FORCE_INLINE
void pmath_atomic_barrier(void){
  #ifdef _AMD64_
    __faststorefence();
  #elif defined(_IA64_) && defined(_M_IA64)
    __mf();
  #else
    long barrier;
    __asm { xchg barrier, eax }
  #endif
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_read_aquire(pmath_atomic_t *atom){
  pmath_atomic_barrier();
  return atom->_data;
}


PMATH_FORCE_INLINE
void pmath_atomic_write_release(pmath_atomic_t *atom, intptr_t value){
  atom->_data = value;
  pmath_atomic_barrier();
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_add(pmath_atomic_t *atom, intptr_t delta){
  #if PMATH_BITSIZE == 64
    return _InterlockedExchangeAdd64(&atom->_data, delta);
  #else
    return _InterlockedExchangeAdd((volatile long*)&atom->_data, delta);
  #endif
}

#if PMATH_BITSIZE == 64

  PMATH_FORCE_INLINE
  pmath_bool_t pmath_atomic_compare_and_set_2(
    pmath_atomic2_t *atom,
    intptr_t old_value_fst,
    intptr_t old_value_snd,
    intptr_t new_value_fst,
    intptr_t new_value_snd
  ){
    pmath_atomic2_t cmp;
    cmp._data[0] = old_value_fst;
    cmp._data[1] = old_value_snd;
    
    return (pmath_bool_t)_InterlockedCompareExchange128(
      (__int64 volatile *)atom->_data,
      new_value_snd, // high
      new_value_fst, // low   ... little endian
      cmp);
  }

#elif PMATH_BITSIZE == 32
  
  //#undef pmath_atomic_loop_nop
  //#define pmath_atomic_loop_nop() __asm{ rep nop }

  PMATH_FORCE_INLINE
  pmath_bool_t pmath_atomic_compare_and_set_2(
    pmath_atomic2_t *atom,
    intptr_t old_value_fst,
    intptr_t old_value_snd,
    intptr_t new_value_fst,
    intptr_t new_value_snd
  ){
    uint64_t cmp = (uint64_t)old_value_fst | ((uint64_t)old_value_snd << 32);
    uint64_t xch = (uint64_t)new_value_fst | ((uint64_t)new_value_snd << 32);
    
    return cmp == _InterlockedCompareExchange64(
      (__int64 volatile *)atom->_data,
      xch,
      cmp);
  }
  
#else
  #error unsupported PMATH_BITSIZE
#endif


PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_have_cas2(void){
  int cpuinfo[4];
  __cpuid(cpuinfo, 1);
  #if PMATH_BITSIZE == 64
    return (cpuinfo[2] & (1 << 13)) != 0; // CMPXCHG16B support
  #elif PMATH_BITSIZE == 32
    return (cpuinfo[3] & (1 << 8)) != 0; // CPMXCHG8B support
  #else
    #error unsupported PMATH_BITSIZE
  #endif
}


#if PMATH_BITSIZE == 32

  #ifndef _InterlockedExchangePointer
    #define _InterlockedExchangePointer(atom_ptr, new_value) \
      (_InterlockedExchange( \
        (long*)(atom_ptr), \
        (new_value)))
  #endif
  
  #ifndef _InterlockedCompareExchangePointer
    #define _InterlockedCompareExchangePointer(atom_ptr, old_value, new_value) \
      (_InterlockedCompareExchange( \
        (long*)(atom_ptr), \
        (old_value), \
        (new_value)))
  #endif
  
#endif


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_set(pmath_atomic_t *atom, intptr_t new_value){
  return (intptr_t)_InterlockedExchangePointer(&atom->_data, new_value);
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_compare_and_set(pmath_atomic_t *atom, intptr_t old_value, intptr_t new_value){
  return (intptr_t)_InterlockedCompareExchangePointer(&atom->_data, new_value, old_value);
}


PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_compare_and_set(pmath_atomic_t *atom, intptr_t old_value, intptr_t new_value){
  return old_value == _InterlockedCompareExchangePointer(&atom->_data, new_value, old_value);
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
  
  while(0 != pmath_atomic_fetch_set(atom, 1)){
    pmath_atomic_loop_nop();
    pmath_atomic_barrier();
  }
  
  pmath_atomic_barrier();
}


PMATH_FORCE_INLINE
void pmath_atomic_unlock(pmath_atomic_t *atom){
  pmath_atomic_barrier();
  pmath_atomic_fetch_set(atom, 0);
}


#define HAVE_ATOMIC_OPS

#endif /* __PMATH_UTIL__CONCURRENCY__ATOMIC__MSVC__INTRINSIC_FUNCTIONS_H__ */
