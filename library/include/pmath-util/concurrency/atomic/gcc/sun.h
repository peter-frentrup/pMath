#ifndef __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__SUN_H__
#define __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__SUN_H__

#ifndef __STDC__
  #warning no __STDC__ defined
  #define __STDC__ 1
  #define PMATH_SUN_HAVE_NO_STDC
#endif

#include <sys/atomic.h>

PMATH_FORCE_INLINE
uint8_t pmath_atomic_read_uint8_aquire(pmath_atomic_uint8_t *atom){
  uint8_t data = atom->_data;
  membar_enter(); // all reads or writes below this line keep below it
  return data;
}

PMATH_FORCE_INLINE
uint16_t pmath_atomic_read_uint16_aquire(pmath_atomic_uint16_t *atom){
  uint16_t data = atom->_data;
  membar_enter(); // all reads or writes below this line keep below it
  return data;
}

PMATH_FORCE_INLINE
uint32_t pmath_atomic_read_uint32_aquire(pmath_atomic_uint32_t *atom){
  uint32_t data = atom->_data;
  membar_enter(); // all reads or writes below this line keep below it
  return data;
}

PMATH_FORCE_INLINE
intptr_t pmath_atomic_read_aquire(pmath_atomic_t *atom){
  intptr_t data = atom->_data;
  membar_enter(); // all reads or writes below this line keep below it
  return data;
}


PMATH_FORCE_INLINE
void pmath_atomic_write_uint8_release(pmath_atomic_uint8_t *atom, uint8_t value){
  membar_exit(); // all reads or writes above this line keep above it
  atom->data = value;
}

PMATH_FORCE_INLINE
void pmath_atomic_write_uint16_release(pmath_atomic_uint16_t *atom, uint16_t value){
  membar_exit(); // all reads or writes above this line keep above it
  atom->data = value;
}

PMATH_FORCE_INLINE
void pmath_atomic_write_uint32_release(pmath_atomic_uint32_t *atom, uint32_t value){
  membar_exit(); // all reads or writes above this line keep above it
  atom->data = value;
}

PMATH_FORCE_INLINE
void pmath_atomic_write_release(pmath_atomic_t *atom, intptr_t value){
  membar_exit(); // all reads or writes above this line keep above it
  atom->data = value;
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_add(pmath_atomic_t *atom, intptr_t delta){
  return atomic_add_ptr_nv(&atom->data, delta) - delta;
}


PMATH_FORCE_INLINE
void pmath_atomic_or_uint8(pmath_atomic_uint8_t *atom, uint8_t mask){
  atomic_or_8(&atom->data, mask);
}

PMATH_FORCE_INLINE
void pmath_atomic_or_uint16(pmath_atomic_uint16_t *atom, uint16_t mask){
  atomic_or_16(&atom->data, mask);
}

PMATH_FORCE_INLINE
void pmath_atomic_or_uint32(pmath_atomic_uint32_t *atom, uint32_t mask){
  atomic_or_32(&atom->data, mask);
}


PMATH_FORCE_INLINE
void pmath_atomic_and_uint8(pmath_atomic_uint8_t *atom, uint8_t mask){
  atomic_and_8(&atom->data, mask);
}

PMATH_FORCE_INLINE
void pmath_atomic_and_uint16(pmath_atomic_uint16_t *atom, uint16_t mask){
  atomic_and_16(&atom->data, mask);
}

PMATH_FORCE_INLINE
void pmath_atomic_and_uint32(pmath_atomic_uint32_t *atom, uint32_t mask){
  atomic_and_32(&atom->data, mask);
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_set(pmath_atomic_t *atom, intptr_t value){
  return (intptr_t)atomic_swap_ptr((void**)&atom->data, (void*)value);
}


PMATH_FORCE_INLINE
uint8_t pmath_atomic_fetch_set_uint8(pmath_atomic_uint8_t *atom, uint8_t value) {
  return atomic_swap_8(&atom->data, value);
}


PMATH_FORCE_INLINE
uint16_t pmath_atomic_fetch_set_uint16(pmath_atomic_uint16_t *atom, uint16_t value) {
  return atomic_swap_16(&atom->data, value);
}


PMATH_FORCE_INLINE
uint32_t pmath_atomic_fetch_set_uint32(pmath_atomic_uint32_t *atom, uint32_t value) {
  return atomic_swap_32(&atom->data, value);
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_compare_and_set(pmath_atomic_t *atom, intptr_t old_value, intptr_t new_value){
  return (intptr_t)atomic_cas_ptr((void**)&atom->_data, (void*)old_value, (void*)new_value);
}


PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_compare_and_set(pmath_atomic_t *atom, intptr_t old_value, intptr_t new_value){
  return (void*)old == atomic_cas_ptr((void**)&atom->_data, (void*)old_value, (void*)new_value);
}


PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_compare_and_set_2(
  pmath_atomic2_t *atom,
  intptr_t old_value_fst,
  intptr_t old_value_snd,
  intptr_t new_value_fst,
  intptr_t new_value_snd
){
  return FALSE;
}


PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_have_cas2(void){
  return FALSE;
}


PMATH_FORCE_INLINE
void pmath_atomic_barrier(void){
  membar_producer();
  membar_consumer();
}


PMATH_FORCE_INLINE
void pmath_atomic_lock(pmath_atomic_t *atom){
  int count = PMATH_ATOMIC_FASTLOOP_COUNT;
  
  while(count > 0 && atom->data != 0){
    --count;
  }
  
  if(atom->data != 0){
    pmath_atomic_loop_yield();
  }
  
  while(0 != atomic_swap_ptr((void**)&atom->_data, (void*)1)){
    membar_enter();
    pmath_atomic_loop_nop();
  }
  membar_enter();
}


PMATH_FORCE_INLINE
void pmath_atomic_unlock(pmath_atomic_t *atom){
  membar_exit();
  atomic_swap_ptr((void**)&atom->_data, (void*)0);
}

#ifdef PMATH_SUN_HAVE_NO_STDC
  #undef __STDC__
#endif

#endif /* __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__SUN_H__ */
