#ifndef __PMATH_CORE__OBJECTS_INLINE_H__
#define __PMATH_CORE__OBJECTS_INLINE_H__

#include <pmath-core/objects.h>
#include <pmath-util/concurrency/atomic.h>
#include <assert.h>

struct _pmath_t{ // do not access members
  uint8_t        type_shift; /* 0..31 */
  uint8_t        flags8;
  uint16_t       flags16;
  pmath_atomic_t refcount;
};

/*============================================================================*/

#define pmath_is_double(obj)  (((obj).s.tag & PMATH_TAGMASK_NONDOUBLE) != PMATH_TAGMASK_NONDOUBLE)
#define pmath_is_pointer(obj) (((obj).s.tag & PMATH_TAGMASK_POINTER)   == PMATH_TAGMASK_POINTER)
#define pmath_is_magic(obj)   ((obj).s.tag == PMATH_TAG_MAGIC)
#define pmath_is_int32(obj)   ((obj).s.tag == PMATH_TAG_INT32)
#define pmath_is_str0(obj)    ((obj).s.tag == PMATH_TAG_STR0)
#define pmath_is_str1(obj)    ((obj).s.tag == PMATH_TAG_STR1)
#define pmath_is_str2(obj)    ((obj).s.tag == PMATH_TAG_STR2)
#define pmath_is_ministr(obj) (pmath_is_str0(obj) || pmath_is_str1(obj) || pmath_is_str2(obj))

PMATH_FORCE_INLINE
struct _pmath_t *PMATH_AS_PTR(pmath_t obj){
  if(!pmath_is_pointer(obj)){
    assert(pmath_is_pointer(obj));
  }
  #if PMATH_BITSIZE == 64
    return (struct _pmath_t*)((obj.as_bits << PMATH_TAGMASK_BITCOUNT) >> PMATH_TAGMASK_BITCOUNT);
  #elif PMATH_BITSIZE == 32
    return obj.s.u.as_pointer_32;
  #endif
}

PMATH_FORCE_INLINE
double PMATH_AS_DOUBLE(pmath_t obj){
  if(!pmath_is_double(obj)){
    assert(pmath_is_double(obj));
  }
  return obj.as_double;
}

#define PMATH_AS_TAG(obj)     ((obj).s.tag)
#define PMATH_AS_INT32(obj)   ((obj).s.u.as_int32)

#define pmath_same(objA, objB)  ((objA).as_bits == (objB).as_bits)
#define pmath_is_null(obj)      (pmath_same(obj, PMATH_NULL))

PMATH_FORCE_INLINE
pmath_bool_t pmath_is_pointer_of(pmath_t obj, pmath_type_t type){
  return pmath_is_pointer(obj) 
    && PMATH_AS_PTR(obj) != NULL
    && ((1 << (PMATH_AS_PTR(obj)->type_shift)) & type) != 0;
}

#define pmath_is_mpint(obj)   (pmath_is_pointer_of(obj, PMATH_TYPE_MP_INT))
#define pmath_is_mpfloat(obj) (pmath_is_pointer_of(obj, PMATH_TYPE_MP_FLOAT))

#define pmath_is_custom(obj)   (pmath_is_pointer_of(obj, PMATH_TYPE_CUSTOM))
#define pmath_is_expr(obj)     (pmath_is_pointer_of(obj, PMATH_TYPE_EXPRESSION))
#define pmath_is_float(obj)    (pmath_is_double(obj) || pmath_is_mpfloat(obj))
#define pmath_is_integer(obj)  (pmath_is_int32(obj) || pmath_is_mpint(obj))
#define pmath_is_number(obj)   (pmath_is_float(obj) || pmath_is_rational(obj))
#define pmath_is_quotient(obj) (pmath_is_pointer_of(obj, PMATH_TYPE_QUOTIENT))
#define pmath_is_rational(obj) (pmath_is_integer(obj) || pmath_is_quotient(obj))
#define pmath_is_bigstr(obj)   (pmath_is_pointer_of(obj, PMATH_TYPE_BIGSTRING))
#define pmath_is_string(obj)   (pmath_is_ministr(obj) || pmath_is_bigstr(obj))
#define pmath_is_symbol(obj)   (pmath_is_pointer_of(obj, PMATH_TYPE_SYMBOL))

PMATH_FORCE_INLINE
pmath_bool_t pmath_is_evaluatable(pmath_t obj){
  return pmath_is_null(obj) 
      || pmath_is_number(obj) 
      || pmath_is_string(obj) 
      || pmath_is_symbol(obj) 
      || pmath_is_expr(obj);
}

PMATH_FORCE_INLINE
pmath_t PMATH_FROM_DOUBLE(double d){
  pmath_t r;
  
  r.as_double = d;
  if(!pmath_is_double(r))
    return PMATH_NULL;
  
  return r;
}

/*============================================================================*/

PMATH_FORCE_INLINE
intptr_t pmath_refcount(pmath_t obj){
  if(pmath_is_pointer(obj) && !pmath_is_null(obj))
    return pmath_atomic_read_aquire(&PMATH_AS_PTR(obj)->refcount);
  return 0;
}

/*============================================================================*/

/**\brief Compares two objects for identity.
   \memberof pmath_t
   \param objA The first object.
   \param objB The second one.
   \return TRUE iff both objects are identical.

   `identity` means, that X != Y is possible, even if X and Y evaluate to the 
   same value. 
   
   If objA and objB are symbols, the result is identical to testing
   objA == objB. 
   
   \note pmath_equals(A, B) might return FALSE although pmath_compare(A, B) == 0
   e.g. for an integer A and a floating point value B.
 */
PMATH_API
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_equals(
  pmath_t objA,
  pmath_t objB);

/*============================================================================*/

/**\brief Internal function
   \private \memberof pmath_t
   
   DO NOT CALL THIS FUNCTION DIRECTLY.
   It is only visible for performance reasons (inlining of pmath_unref()).
 */
PMATH_API 
void _pmath_destroy_object(pmath_t obj);

/**\brief Increments the reference counter of an object and returns it.
   \memberof pmath_t
   \param obj The object to be referenced.
   \return The referenced object.
   You must free the result with pmath_unref().
 */
PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_ref(pmath_t obj){
  if(PMATH_UNLIKELY(!pmath_is_pointer(obj)))
    return obj;
  
  if(PMATH_UNLIKELY(PMATH_AS_PTR(obj) == NULL))
    return obj;
  
  #ifndef PMATH_DEBUG_LOG
    (void)pmath_atomic_fetch_add(&(PMATH_AS_PTR(obj)->refcount), 1);
  #else
    if(   pmath_atomic_fetch_add(&(PMATH_AS_PTR(obj)->refcount), 1) == 0){
      if(PMATH_AS_PTR(obj)->type_shift != PMATH_TYPE_SHIFT_SYMBOL){
        assert("referencing deleted object" && 0);
      }
    }
  #endif
  
  return obj;
}

/**\brief Decrements the reference counter of an object and frees its memory
          if the reference counter becomes 0.
   \memberof pmath_t
   \param obj The object to be destroyed.
 */
PMATH_FORCE_INLINE
void pmath_unref(pmath_t obj){
  if(PMATH_UNLIKELY(!pmath_is_pointer(obj)))
    return;

  if(PMATH_UNLIKELY(PMATH_AS_PTR(obj) == NULL))
    return;
  
  pmath_atomic_barrier();
  if(1 == pmath_atomic_fetch_add(&(PMATH_AS_PTR(obj)->refcount), -1)){ // was 1 -> is 0
    _pmath_destroy_object(obj);
  }
  pmath_atomic_barrier();
}

#endif /* __PMATH_CORE__OBJECTS_INLINE_H__ */
