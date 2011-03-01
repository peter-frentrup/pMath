#ifndef __PMATH_UTIL__CONCURRENCY__ATOMIC__MSVC__INTRINSIC_FUNCTIONS_H__
#define __PMATH_UTIL__CONCURRENCY__ATOMIC__MSVC__INTRINSIC_FUNCTIONS_H__

#include <intrin.h>
#include <windows.h>

#if PMATH_BITSIZE == 64
  
  #define pmath_atomic_fetch_add(atom_ptr, delta) \
    ((intptr_t)_InterlockedExchangeAdd64(\
      (__int64 volatile *)(atom_ptr), \
      (__int64)(delta)))

  PMATH_FORCE_INLINE
  PMATH_INLINE_NODEBUG
  pmath_bool_t pmath_atomic_compare_and_set_2(
    intptr_t volatile *atom, // array of two intptr_ts, aligned to 128 bit
    intptr_t old_value_fst,
    intptr_t old_value_snd,
    intptr_t new_value_fst,
    intptr_t new_value_snd
  ){
    PMATH_DECLARE_ATOMIC_2(cmp) = {old_value_fst, old_value_snd};
    
    return (pmath_bool_t)_InterlockedCompareExchange128(
      (__int64 volatile *)atom,
      new_value_snd, // high
      new_value_fst, // low   ... little endian
      cmp);
  }

#elif PMATH_BITSIZE == 32
  
  //#undef pmath_atomic_loop_nop
  //#define pmath_atomic_loop_nop() __asm{ rep nop }

  #define pmath_atomic_fetch_add(atom_ptr, delta) \
    ((intptr_t)_InterlockedExchangeAdd(\
      (long volatile *)(atom_ptr), \
      (long)(delta)))
  
  PMATH_FORCE_INLINE
  PMATH_INLINE_NODEBUG
  pmath_bool_t pmath_atomic_compare_and_set_2(
    intptr_t volatile *atom, // array of two intptr_ts, aligned to 64 bit
    intptr_t old_value_fst,
    intptr_t old_value_snd,
    intptr_t new_value_fst,
    intptr_t new_value_snd
  ){
    uint64_t cmp = (uint64_t)old_value_fst | ((uint64_t)old_value_snd << 32);
    uint64_t xch = (uint64_t)new_value_fst | ((uint64_t)new_value_snd << 32);
    
    return cmp == _InterlockedCompareExchange64(
      (__int64 volatile *)atom,
      xch,
      cmp);
  }
  
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
  
#else
  #error unsupported PMATH_BITSIZE
#endif

#define pmath_atomic_fetch_set(atom_ptr, new_value) \
  ((intptr_t)_InterlockedExchangePointer(\
    (void*volatile*)(atom_ptr), \
    (new_value)))

#define pmath_atomic_fetch_compare_and_set(atom_ptr, old_value, new_value) \
  ((intptr_t)_InterlockedCompareExchangePointer(\
    (void*volatile*)(atom_ptr), \
    (new_value), \
    (old_value)))

#define pmath_atomic_compare_and_set(atom_ptr, old_value, new_value) \
  ((pmath_bool_t)((old_value) == _InterlockedCompareExchangePointer(\
    (void*volatile*)(atom_ptr), \
    (new_value), \
    (old_value))))

PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
pmath_bool_t pmath_atomic_have_cas2(void){
  int cpuinfo[4];
  __cpuid(cpuinfo, 1);
  #if PMATH_BITSIZE == 64
    return (cpuinfo[2] & (1 << 13)) != 0; /* CMPXCHG16B support */
  #elif PMATH_BITSIZE == 32
    return (cpuinfo[3] & (1 << 8)) != 0; /* CPMXCHG8B support */
  #else
    #error unsupported PMATH_BITSIZE
  #endif
}

#define pmath_atomic_barrier()  _ReadWriteBarrier()

#define pmath_atomic_lock(atom_ptr) \
  do{ \
    intptr_t volatile *_pmath_atomic_lock__ptr = (intptr_t volatile *)(atom_ptr); \
     \
    int _pmath_atomic_lock__cnt = PMATH_ATOMIC_FASTLOOP_COUNT; \
    while(_pmath_atomic_lock__cnt > 0 && *_pmath_atomic_lock__ptr != 0){ \
      --_pmath_atomic_lock__cnt; \
    } \
     \
    if(*_pmath_atomic_lock__ptr != 0){ \
      pmath_atomic_loop_yield(); \
    } \
    while(0 != pmath_atomic_fetch_set(atom_ptr, 1)){ \
      pmath_atomic_loop_nop(); \
    } \
  }while(0)

#define pmath_atomic_unlock(atom_ptr) \
  ((void)pmath_atomic_fetch_set(atom_ptr, 0))

#define HAVE_ATOMIC_OPS

#endif /* __PMATH_UTIL__CONCURRENCY__ATOMIC__MSVC__INTRINSIC_FUNCTIONS_H__ */
