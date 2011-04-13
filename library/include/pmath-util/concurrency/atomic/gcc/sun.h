#ifndef __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__SUN_H__
#define __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__SUN_H__

#ifndef __STDC__
  #warning no __STDC__ defined
  #define __STDC__ 1
  #define PMATH_SUN_HAVE_NO_STDC
#endif

#include <sys/atomic.h>

PMATH_FORCE_INLINE
intptr_t pmath_atomic_read_aquire(pmath_atomic_t *atom){
  membar_enter();
  return atom->_data;
}


PMATH_FORCE_INLINE
void pmath_atomic_write_release(pmath_atomic_t *atom, intptr_t value){
  atom->data = value;
  membar_exit();
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_add(pmath_atomic_t *atom, intptr_t delta){
  return atomic_add_ptr_nv(&atom->data, delta) - delta;
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_set(pmath_atomic_t *atom, intptr_t value){
  return (intptr_t)atomic_swap_ptr((void**)&atom->data, (void*)value);
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
