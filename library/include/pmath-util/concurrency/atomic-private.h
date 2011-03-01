#ifndef __PMATH_UTIL__CONCURRENCY__ATOMIC_PRIVATE_H__
#define __PMATH_UTIL__CONCURRENCY__ATOMIC_PRIVATE_H__

#ifndef BUILDING_PMATH
  #error This header file is not part of the public pMath API
#endif

#include <pmath-core/objects-inline.h>
#include <stdlib.h>

/*----------------------------------------------------------------------------*/

/* Non-recursive pointer and lock.
   Do NOT mix "_pmath_atomic_[un]lock_ptr(p)" with direct memory access "*p [= ...]"
   
   usage:
     static void * volatile global_value;
     
     void *value = _pmath_atomic_lock_ptr(&global_value);
     ... use value ...
     _pmath_atomic_unlock_ptr(&global_value, value);
 */
 
PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
void *_pmath_atomic_lock_ptr(void * volatile *ptr){
  void *result;
  
  do{
    result = (void*)pmath_atomic_fetch_set(
      (intptr_t*)ptr,
      (intptr_t)PMATH_INVALID_PTR);
  }while(result == PMATH_INVALID_PTR);
  
  return result;
}

PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
void _pmath_atomic_unlock_ptr(void * volatile *ptr, void *value){
  assert(*ptr == PMATH_INVALID_PTR);
  assert(value != PMATH_INVALID_PTR);
  
  *ptr = value;
  pmath_atomic_barrier();
}

/*----------------------------------------------------------------------------*/

/* Access pMath objects "atomically" (using a spinlock).
 */

PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
void _pmath_object_atomic_write(
  pmath_t volatile *ptr,
  pmath_t          value
){
  pmath_t old;

  assert(value != PMATH_INVALID_PTR);

  old = (pmath_t)pmath_atomic_fetch_set(
    (intptr_t*)ptr,
    (intptr_t)value);

  if(old != PMATH_INVALID_PTR)
    pmath_unref(old);
}

PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
pmath_t _pmath_object_atomic_read_start(
  pmath_t volatile *ptr
){
  return (pmath_t)_pmath_atomic_lock_ptr((void * volatile *)ptr);
}

/* All reads are blocked between 
     _pmath_object_atomic_read_start() and _pmath_object_atomic_read_end(),
     
   but _pmath_object_atomic_write() succeeds.
 */
 
PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
void _pmath_object_atomic_read_end(
  pmath_t volatile *ptr,
  pmath_t          value // will be freed
){
  assert(value != PMATH_INVALID_PTR);
  
  if(!pmath_atomic_compare_and_set(
      (intptr_t*)ptr,
      (intptr_t)PMATH_INVALID_PTR,
      (intptr_t)value))
    pmath_unref(value);
}

PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
pmath_t _pmath_object_atomic_read(
  pmath_t volatile *ptr
){
  pmath_t value;
  
  value = _pmath_object_atomic_read_start(ptr);
  _pmath_object_atomic_read_end(ptr, pmath_ref(value));
  
  return value;
}

/*----------------------------------------------------------------------------*/

PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
void *_pmath_atomic_global_need(
  void * volatile *ptr,
  void *(*create_default)(void),
  void (*ref)(void*)
){
  void *result;
  do{
    result = (void*)pmath_atomic_fetch_set(
      (intptr_t*)ptr,
      (intptr_t)PMATH_INVALID_PTR);
  }while(result == PMATH_INVALID_PTR);

  if(result)
    ref(result);
  else
    result = create_default();

  assert(result != PMATH_INVALID_PTR);

  *ptr = result;
  pmath_atomic_barrier();
  return result;
}

PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
void *_pmath_atomic_global_done(
  void * volatile *ptr,
  void *expected_value,
  void *(*decref)(void*)  // does not free its argument
){
  void *value;

  if(expected_value == NULL)
    return NULL;

  assert(expected_value != PMATH_INVALID_PTR);

  do{
    value = (void*)pmath_atomic_fetch_set(
      (intptr_t*)ptr,
      (intptr_t)PMATH_INVALID_PTR);
  }while(value != expected_value); // value == PMATH_INVALID_PTR

  *ptr = decref(value);

  return value;
}

#endif /* __PMATH_UTIL__CONCURRENCY__ATOMIC_PRIVATE_H__ */
