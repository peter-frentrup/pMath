
#ifndef __PMATH_CORE__OBJECTS_INLINE_H__
#define __PMATH_CORE__OBJECTS_INLINE_H__

#include <pmath-core/objects.h>
#include <pmath-util/concurrency/atomic.h>

struct _pmath_t { // do not access members
  uint8_t                type_shift; /* 0..31 */
  pmath_atomic_uint8_t   flags8;
  pmath_atomic_uint16_t  flags16;
#if PMATH_BITSIZE >= 64
  pmath_atomic_uint32_t  padding_flags32;
#endif
  pmath_atomic_t refcount;
};

#define  PMATH_OBJECT_FLAGS8_VALID         ((uint8_t)0x01u)
#define  PMATH_OBJECT_FLAGS8_TRAP_DELETED  ((uint8_t)0x02u)

/*============================================================================*/

/**\brief Check for an (inlined) machine-precision float.
   \hideinitializer
   \memberof pmath_float_t
 */
#define pmath_is_double(obj)  (((obj).s.tag & PMATH_TAGMASK_NONDOUBLE) != PMATH_TAGMASK_NONDOUBLE)

/**\brief Check for a non-inlined object.
   \hideinitializer
   \memberof pmath_t
   
   Non-inlined objects are reference counted with pmath_ref/pmath_unref.
 */
#define pmath_is_pointer(obj) (((obj).s.tag & PMATH_TAGMASK_POINTER)   == PMATH_TAGMASK_POINTER)

/**\brief Check for a magic number.
   \hideinitializer
   \memberof pmath_t
   
   Magic numbers are inlined and non-evaluatable.
 */
#define pmath_is_magic(obj)   ((obj).s.tag == PMATH_TAG_MAGIC)

/**\brief Check for an inlined 32-bit number.
   \hideinitializer
   \memberof pmath_integer_t
 */
#define pmath_is_int32(obj)   ((obj).s.tag == PMATH_TAG_INT32)

#define pmath_is_str0(obj)    ((obj).s.tag == PMATH_TAG_STR0)
#define pmath_is_str1(obj)    ((obj).s.tag == PMATH_TAG_STR1)
#define pmath_is_str2(obj)    ((obj).s.tag == PMATH_TAG_STR2)
#define pmath_is_ministr(obj) (pmath_is_str0(obj) || pmath_is_str1(obj) || pmath_is_str2(obj))

PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_PURE
struct _pmath_t *PMATH_AS_PTR(pmath_t obj) {
#ifndef NDEBUG
  if(!pmath_is_pointer(obj)) {
    assert(pmath_is_pointer(obj));
  }
#endif
  
#if PMATH_BITSIZE == 64
  return (struct _pmath_t *)((obj.as_bits << PMATH_TAGMASK_BITCOUNT) >> PMATH_TAGMASK_BITCOUNT);
#elif PMATH_BITSIZE == 32
  return obj.s.u.as_pointer_32;
#endif
}

PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_PURE
double PMATH_AS_DOUBLE(pmath_t obj) {
#ifndef NDEBUG
  if(!pmath_is_double(obj)) {
    assert(pmath_is_double(obj));
  }
#endif

  return obj.as_double;
}

#define PMATH_AS_TAG(obj)     ((obj).s.tag)
#define PMATH_AS_INT32(obj)   ((obj).s.u.as_int32)

/**\brief Check for reference equality.
   \hideinitializer
   \memberof pmath_t
   
   \param objA The first object. It wont be freed.
   \param objB The second object. It wont be freed.
   \return TRUE if both arguments have the same bit pattern.
 */
#define pmath_same(objA, objB)  ((objA).as_bits == (objB).as_bits)

/**\brief Check for equality to PMATH_NULL.
   \hideinitializer
   \memberof pmath_t
   
   \param obj A pMath object. It wont be freed.
   \return TRUE if the argument equals PMATH_NULL.
 */
#define pmath_is_null(obj)      (pmath_same((obj), PMATH_NULL))

/**\brief Check for a non-inlined object of some type.
   \memberof pmath_t
   \param obj The object to test. It wont be freed.
   \param type A mask of allowed types.
 */
PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_is_pointer_of(pmath_t obj, pmath_type_t type) {
  return pmath_is_pointer(obj)     &&
         PMATH_AS_PTR(obj) != NULL &&
         ((1 << (PMATH_AS_PTR(obj)->type_shift)) & type) != 0;
}

/**\brief Check for a multi-precision integer.
   \hideinitializer
   \memberof pmath_integer_t
 */
#define pmath_is_mpint(obj)   (pmath_is_pointer_of((obj), PMATH_TYPE_MP_INT))

/**\brief Check for a multi-precision floating point number.
   \hideinitializer
   \memberof pmath_float_t
 */
#define pmath_is_mpfloat(obj) (pmath_is_pointer_of((obj), PMATH_TYPE_MP_FLOAT))

/**\brief Check for a custom object.
   \hideinitializer
   \memberof pmath_custom_t
 */
#define pmath_is_custom(obj)        (pmath_is_pointer_of((obj), PMATH_TYPE_CUSTOM))

/**\brief Check for a pMath expression.
   \hideinitializer
   \memberof pmath_expr_t
 */
#define pmath_is_expr(obj)          (pmath_is_pointer_of((obj), PMATH_TYPE_EXPRESSION))

/**\brief Check for a floating point number.
   \hideinitializer
   \memberof pmath_float_t
 */
#define pmath_is_float(obj)         (pmath_is_double(obj) || pmath_is_mpfloat(obj))

/**\brief Check for an integer.
   \hideinitializer
   \memberof pmath_integer_t
 */
#define pmath_is_integer(obj)       (pmath_is_int32(obj) || pmath_is_mpint(obj))

/**\brief Check for a number.
   \hideinitializer
   \memberof pmath_number_t
 */
#define pmath_is_number(obj)        (pmath_is_float(obj) || pmath_is_rational(obj))

/**\brief Check for a quotient.
   \hideinitializer
   \memberof pmath_quotient_t
 */
#define pmath_is_quotient(obj)      (pmath_is_pointer_of((obj), PMATH_TYPE_QUOTIENT))

/**\brief Check for a rational number.
   \hideinitializer
   \memberof pmath_rational_t
 */
#define pmath_is_rational(obj)      (pmath_is_integer(obj) || pmath_is_quotient(obj))

#define pmath_is_bigstr(obj)        (pmath_is_pointer_of((obj), PMATH_TYPE_BIGSTRING))

/**\brief Check for a string.
   \hideinitializer
   \memberof pmath_string_t
 */
#define pmath_is_string(obj)        (pmath_is_ministr(obj) || pmath_is_bigstr(obj))

/**\brief Check for a symbol.
   \hideinitializer
   \memberof pmath_symbol_t
 */
#define pmath_is_symbol(obj)        (pmath_is_pointer_of((obj), PMATH_TYPE_SYMBOL))

/**\brief Check for a blob object.
   \hideinitializer
   \memberof pmath_blob_t
 */
#define pmath_is_blob(obj)          (pmath_is_pointer_of((obj), PMATH_TYPE_BLOB))

/**\brief Check for a packed array expression.
   \hideinitializer
   \memberof pmath_packed_array_t
 */
#define pmath_is_packed_array(obj)  (pmath_is_pointer_of((obj), PMATH_TYPE_PACKED_ARRAY))

/**\brief Check if an object is evaluatable.
   \memberof pmath_t
   \param obj A pMath object. It wont be freed.
   \return FALSE if evaluating the object yields PMATH_NULL.
   
   Evaluatebale objects are [numbers](@ref pmath_number_t), 
   [strings](@ref pmath_string_t), [symbols](@ref pmath_symbol_t) and all
   [expressions](@ref pmath_expr_t).
 */
PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_is_evaluatable(pmath_t obj) {
  return pmath_is_null(obj)     ||
         pmath_is_number(obj)   ||
         pmath_is_string(obj)   ||
         pmath_is_symbol(obj)   ||
         pmath_is_expr(obj);
}

PMATH_FORCE_INLINE
PMATH_ATTRIBUTE_PURE
pmath_t PMATH_FROM_DOUBLE(double d) {
  pmath_t r;
  
  r.as_double = d;
  if(!pmath_is_double(r))
    return PMATH_NULL;
    
  return r;
}

/*============================================================================*/

/**\brief Get the current reference count.
   \memberof pmath_t
   \param obj A pMath object. It wont be freed.
   \return The reference count or 0 if \a obj is an inlined object.
 */
PMATH_FORCE_INLINE
intptr_t pmath_refcount(pmath_t obj) {
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

   `identity` means, that X &ne; Y is possible, even if X and Y evaluate to the
   same value.

   If objA and objB are symbols, the result is identical to pmath_same(objA,objB).

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
pmath_t pmath_ref(pmath_t obj) {
  struct _pmath_t *ptr;
  
  if(PMATH_UNLIKELY(!pmath_is_pointer(obj)))
    return obj;
    
  ptr = PMATH_AS_PTR(obj);
  if(PMATH_UNLIKELY(ptr == NULL))
    return obj;
  
#ifdef PMATH_DEBUG_LOG
  if(PMATH_UNLIKELY(pmath_atomic_read_uint8_aquire(&ptr->flags8) & PMATH_OBJECT_FLAGS8_TRAP_DELETED)) {
    assert("referencing deleted object" && 0);
  }
#endif
  
#ifndef PMATH_DEBUG_LOG
  (void)pmath_atomic_fetch_add(&(ptr->refcount), 1);
#else
  if(pmath_atomic_fetch_add(&(ptr->refcount), 1) == 0) {
#  ifndef NDEBUG
    if(ptr->type_shift != PMATH_TYPE_SHIFT_SYMBOL) {
      assert("referencing deleted object" && 0);
    }
#  endif
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
void pmath_unref(pmath_t obj) {
  struct _pmath_t *ptr;
  
  if(PMATH_UNLIKELY(!pmath_is_pointer(obj)))
    return;
  
  ptr = PMATH_AS_PTR(obj);
  if(PMATH_UNLIKELY(ptr == NULL))
    return;
    
#ifdef PMATH_DEBUG_LOG
  if(PMATH_UNLIKELY(pmath_atomic_read_uint8_aquire(&ptr->flags8) & PMATH_OBJECT_FLAGS8_TRAP_DELETED)) {
    assert("unref deleted object" && 0);
  }
#endif
  
  pmath_atomic_barrier();
  if(1 == pmath_atomic_fetch_add(&(ptr->refcount), -1)) { // was 1 -> is 0
    _pmath_destroy_object(obj);
  }
  pmath_atomic_barrier();
}

#endif /* __PMATH_CORE__OBJECTS_INLINE_H__ */
