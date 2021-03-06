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
uint8_t pmath_atomic_read_uint8_aquire(const pmath_atomic_uint8_t *atom){
  uint8_t data = atom->_data;
  pmath_atomic_barrier(); // all reads or writes below this line keep below it (pmath_atomic_barrier() is even a full barrier)
  return data;
}

PMATH_FORCE_INLINE
uint16_t pmath_atomic_read_uint16_aquire(const pmath_atomic_uint16_t *atom){
  uint16_t data = atom->_data;
  pmath_atomic_barrier(); // all reads or writes below this line keep below it (pmath_atomic_barrier() is even a full barrier)
  return data;
}

PMATH_FORCE_INLINE
uint32_t pmath_atomic_read_uint32_aquire(const pmath_atomic_uint32_t *atom){
  uint32_t data = atom->_data;
  pmath_atomic_barrier(); // all reads or writes below this line keep below it (pmath_atomic_barrier() is even a full barrier)
  return data;
}

PMATH_FORCE_INLINE
intptr_t pmath_atomic_read_aquire(const pmath_atomic_t *atom) {
  intptr_t data = atom->_data;
  pmath_atomic_barrier(); // all reads or writes below this line keep below it (pmath_atomic_barrier() is even a full barrier)
  return data;
}


PMATH_FORCE_INLINE
void pmath_atomic_write_uint8_release(pmath_atomic_uint8_t *atom, uint8_t value) {
  pmath_atomic_barrier(); // all reads or writes above this line keep above it (pmath_atomic_barrier() is even a full barrier)
  atom->_data = value;
}

PMATH_FORCE_INLINE
void pmath_atomic_write_uint16_release(pmath_atomic_uint16_t *atom, uint16_t value) {
  pmath_atomic_barrier(); // all reads or writes above this line keep above it (pmath_atomic_barrier() is even a full barrier)
  atom->_data = value;
}

PMATH_FORCE_INLINE
void pmath_atomic_write_uint32_release(pmath_atomic_uint32_t *atom, uint32_t value) {
  pmath_atomic_barrier(); // all reads or writes above this line keep above it (pmath_atomic_barrier() is even a full barrier)
  atom->_data = value;
}

PMATH_FORCE_INLINE
void pmath_atomic_write_release(pmath_atomic_t *atom, intptr_t value){
  pmath_atomic_barrier(); // all reads or writes above this line keep above it (pmath_atomic_barrier() is even a full barrier)
  atom->_data = value;
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_add(pmath_atomic_t *atom, intptr_t delta){
  #if PMATH_BITSIZE == 64
    return _InterlockedExchangeAdd64(&atom->_data, delta);
  #else
    return _InterlockedExchangeAdd((volatile long*)&atom->_data, delta);
  #endif
}


PMATH_FORCE_INLINE
void pmath_atomic_or_uint8(pmath_atomic_uint8_t *atom, uint8_t mask) {
  (void)_InterlockedOr8((char volatile*)&atom->_data, (char)mask);
}

PMATH_FORCE_INLINE
void pmath_atomic_or_uint16(pmath_atomic_uint16_t *atom, uint16_t mask) {
  (void)_InterlockedOr16((short volatile*)&atom->_data, (short)mask);
}

PMATH_FORCE_INLINE
void pmath_atomic_or_uint32(pmath_atomic_uint32_t *atom, uint32_t mask) {
  (void)_InterlockedOr((long volatile*)&atom->_data, (long)mask);
}


PMATH_FORCE_INLINE
void pmath_atomic_and_uint8(pmath_atomic_uint8_t *atom, uint8_t mask) {
  (void)_InterlockedAnd8((char volatile*)&atom->_data, (char)mask);
}

PMATH_FORCE_INLINE
void pmath_atomic_and_uint16(pmath_atomic_uint16_t *atom, uint16_t mask) {
  (void)_InterlockedAnd16((short volatile*)&atom->_data, (short)mask);
}

PMATH_FORCE_INLINE
void pmath_atomic_and_uint32(pmath_atomic_uint32_t *atom, uint32_t mask) {
  (void)_InterlockedAnd((long volatile*)&atom->_data, (long)mask);
}


#if PMATH_BITSIZE == 64

  PMATH_FORCE_INLINE
  pmath_bool_t pmath_atomic_compare_and_set_2(
    pmath_atomic2_t *atom,
    intptr_t old_value_fst,
    intptr_t old_value_snd,
    intptr_t new_value_fst,
    intptr_t new_value_snd
  ) {
    pmath_atomic2_t cmp;
    cmp._data[0] = old_value_fst;
    cmp._data[1] = old_value_snd;

    return (pmath_bool_t)_InterlockedCompareExchange128(
      &atom->_data[0],
      new_value_snd, // high
      new_value_fst, // low   ... little endian
      &cmp._data[0]);
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
pmath_bool_t pmath_atomic_have_cas2(void) {
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
        (long)(new_value)))
  #endif

  #ifndef _InterlockedCompareExchangePointer
    #define _InterlockedCompareExchangePointer(atom_ptr, old_value, new_value) \
      (_InterlockedCompareExchange( \
        (long*)(atom_ptr), \
        (long)(old_value), \
        (long)(new_value)))
  #endif

#endif


PMATH_FORCE_INLINE
uint8_t pmath_atomic_fetch_set_uint8(pmath_atomic_uint8_t *atom, uint8_t new_value) {
  return (uint8_t)_InterlockedExchange8((char*)&atom->_data, (char)new_value);
}


PMATH_FORCE_INLINE
uint16_t pmath_atomic_fetch_set_uint16(pmath_atomic_uint16_t *atom, uint16_t new_value) {
  return (uint16_t)_InterlockedExchange16((short*)&atom->_data, (short)new_value);
}


PMATH_FORCE_INLINE
uint32_t pmath_atomic_fetch_set_uint32(pmath_atomic_uint32_t *atom, uint32_t new_value) {
  return (uint32_t)_InterlockedExchange((long*)&atom->_data, (long)new_value);
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_set(pmath_atomic_t *atom, intptr_t new_value) {
  return (intptr_t)_InterlockedExchangePointer((void**)&atom->_data, (void*)new_value);
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_compare_and_set(pmath_atomic_t *atom, intptr_t old_value, intptr_t new_value){
  return (intptr_t)_InterlockedCompareExchangePointer((void**)&atom->_data, (void*)new_value, (void*)old_value);
}


PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_compare_and_set(pmath_atomic_t *atom, intptr_t old_value, intptr_t new_value){
  return old_value == (intptr_t)_InterlockedCompareExchangePointer((void**)&atom->_data, (void*)new_value, (void*)old_value);
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
