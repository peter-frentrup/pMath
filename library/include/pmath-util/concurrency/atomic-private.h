#ifndef __PMATH_UTIL__CONCURRENCY__ATOMIC_PRIVATE_H__
#define __PMATH_UTIL__CONCURRENCY__ATOMIC_PRIVATE_H__

#ifndef BUILDING_PMATH
#error This header file is not part of the public pMath API
#endif

#include <pmath-core/objects-inline.h>

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
void *_pmath_atomic_lock_ptr(pmath_atomic_t *ptr) {
  void *result;
  
  do {
    result = (void *)pmath_atomic_fetch_set(ptr, (intptr_t)PMATH_INVALID_PTR);
  } while(result == PMATH_INVALID_PTR);
  
  return result;
}

PMATH_FORCE_INLINE
void _pmath_atomic_unlock_ptr(pmath_atomic_t *ptr, void *value) {
  assert((void *)ptr->_data == PMATH_INVALID_PTR);
  assert(value != PMATH_INVALID_PTR);
  
  pmath_atomic_write_release(ptr, (intptr_t)value);
}

/*----------------------------------------------------------------------------*/

typedef struct {
  PMATH_DECLARE_ALIGNED(pmath_t, _data, 8);
} pmath_locked_t;

/* Access pMath objects "atomically" (using a spinlock).
 */

PMATH_FORCE_INLINE
pmath_t _pmath_object_atomic_read_start(
  pmath_locked_t *ptr
) {
  pmath_t value;
  
#if PMATH_BITSIZE == 64
  {
    pmath_t invalid;
    invalid.s.tag        = PMATH_TAG_INVALID;
    invalid.s.u.as_int32 = 0;
    
    value.as_bits = pmath_atomic_fetch_set((pmath_atomic_t *)&ptr->_data.as_bits, invalid.as_bits);
    
    while(PMATH_AS_TAG(value) == PMATH_TAG_INVALID) {
      pmath_atomic_loop_nop();
      
      value.as_bits = pmath_atomic_fetch_set((pmath_atomic_t *)&ptr->_data.as_bits, invalid.as_bits);
    }
    
  }
#else
  {
    value.s.tag = pmath_atomic_fetch_set((pmath_atomic_t *)&ptr->_data.s.tag, PMATH_TAG_INVALID);
  
    while(PMATH_AS_TAG(value) == PMATH_TAG_INVALID) {
      pmath_atomic_loop_nop();
  
      value.s.tag = pmath_atomic_fetch_set((pmath_atomic_t *)&ptr->_data.s.tag, PMATH_TAG_INVALID);
    }
  
    value.s.u.as_int32 = pmath_atomic_fetch_set((pmath_atomic_t *)&ptr->_data.s.u.as_int32, 0);
  
  }
#endif
  
  return value;
}

PMATH_FORCE_INLINE
void _pmath_object_atomic_read_end(
  pmath_locked_t *ptr,
  pmath_t         value // will be freed
) {
  assert(PMATH_AS_TAG(value) != PMATH_TAG_INVALID);
  
#if PMATH_BITSIZE == 64
  {
    pmath_t invalid;
    invalid.s.tag        = PMATH_TAG_INVALID;
    invalid.s.u.as_int32 = 0;
    
    if(!pmath_atomic_compare_and_set((pmath_atomic_t *)&ptr->_data.as_bits, invalid.as_bits, value.as_bits))
      pmath_unref(value);
  }
#else
  {
    assert(PMATH_AS_TAG(ptr->_data) == PMATH_TAG_INVALID);
  
    (void)pmath_atomic_fetch_set((pmath_atomic_t *)&ptr->_data.s.u.as_int32, value.s.u.as_int32);
    (void)pmath_atomic_fetch_set((pmath_atomic_t *)&ptr->_data.s.tag,        value.s.tag);
  
  }
#endif
}

PMATH_FORCE_INLINE
pmath_t _pmath_object_atomic_read(
  pmath_locked_t *ptr
) {
  pmath_t value;
  
  value = _pmath_object_atomic_read_start(ptr);
  _pmath_object_atomic_read_end(ptr, pmath_ref(value));
  
  return value;
}

PMATH_FORCE_INLINE
void _pmath_object_atomic_write(
  pmath_locked_t *ptr,
  pmath_t         value
) {
  pmath_t old;
  
  assert(PMATH_AS_TAG(value) != PMATH_TAG_INVALID);
  
#if PMATH_BITSIZE == 64
  {
    old.as_bits = pmath_atomic_fetch_set(
                    (pmath_atomic_t *)&ptr->_data.as_bits,
                    value.as_bits);
                    
    if(PMATH_AS_TAG(old) != PMATH_TAG_INVALID)
      pmath_unref(old);
  }
#else
  {
    old = _pmath_object_atomic_read_start(ptr);
    _pmath_object_atomic_read_end(ptr, value);
    pmath_unref(old);
  }
#endif
}

/*----------------------------------------------------------------------------*/

PMATH_FORCE_INLINE
void *_pmath_atomic_global_need(
  pmath_atomic_t *ptr,
  void * (*create_default)(void),
  void (*ref)(void *)
) {
  void *result;
  do {
    result = (void *)pmath_atomic_fetch_set(ptr, (intptr_t)PMATH_INVALID_PTR);
  } while(result == PMATH_INVALID_PTR);
  
  if(result)
    ref(result);
  else
    result = create_default();
    
  assert(result != PMATH_INVALID_PTR);
  
  pmath_atomic_write_release(ptr, (intptr_t)result);
  return result;
}

PMATH_FORCE_INLINE
void *_pmath_atomic_global_done(
  pmath_atomic_t *ptr,
  void *expected_value,
  void * (*decref)(void *) // does not free its argument
) {
  void *value;
  
  if(expected_value == NULL)
    return NULL;
    
  assert(expected_value != PMATH_INVALID_PTR);
  
  do {
    value = (void *)pmath_atomic_fetch_set(ptr, (intptr_t)PMATH_INVALID_PTR);
  } while(value != expected_value); // value == PMATH_INVALID_PTR
  
  pmath_atomic_write_release(ptr, (intptr_t)decref(value));
  return value;
}

#endif /* __PMATH_UTIL__CONCURRENCY__ATOMIC_PRIVATE_H__ */
