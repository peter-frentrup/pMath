#ifndef __PMATH_CORE__OBJECTS_INLINE_H__
#define __PMATH_CORE__OBJECTS_INLINE_H__

#include <pmath-core/objects.h>
#include <pmath-util/concurrency/atomic.h>
#include <assert.h>

struct _pmath_t{ // do not access members
  unsigned int  type_shift; /* 0..31 */
  PMATH_DECLARE_ATOMIC(refcount);
};

/*============================================================================*/

#if PMATH_BITSIZE == 64
  #define PMATH_AS_PTR(obj)  (assert(pmath_is_pointer(obj)), ((struct _pmath_t*)(((obj).as_bits << PMATH_TAGMASK_BITCOUNT) >> PMATH_TAGMASK_BITCOUNT)))
#elif PMATH_BITSIZE == 32
  #define PMATH_AS_PTR(obj)  (assert(pmath_is_pointer(obj)), ((obj).s.u.as_pointer_32))
#endif

#define PMATH_AS_TAG(obj)    ((obj).s.tag)
#define PMATH_AS_INT32(obj)  ((obj).s.u.as_int32)

#define pmath_same(objA, objB)  ((objA).as_bits == (objB).as_bits)

#define pmath_is_double(obj)  (((obj).s.tag & PMATH_TAGMASK_NONDOUBLE) != PMATH_TAGMASK_NONDOUBLE)
#define pmath_is_pointer(obj) (((obj).s.tag & PMATH_TAGMASK_OBJECT)    == PMATH_TAGMASK_POINTER)

#define pmath_is_null(obj)    (pmath_is_pointer(obj) && PMATH_AS_PTR(obj) == NULL)
#define pmath_is_magic(obj)   ((obj).s.tag == PMATH_TAG_MAGIC)

#define pmath_is_pointer_of(obj, type)  (pmath_is_pointer(obj) && ((1 << (PMATH_AS_PTR(obj)->type_shift)) & (type)) != 0)

#define pmath_is_integer(obj) (pmath_is_pointer_of(obj, PMATH_TYPE_INTEGER))
#define pmath_is_mpfloat(obj) (pmath_is_pointer_of(obj, PMATH_TYPE_MP_FLOAT))

#define pmath_is_custom(obj)   (pmath_is_pointer_of(obj, PMATH_TYPE_CUSTOM))
#define pmath_is_expr(obj)     (pmath_is_pointer_of(obj, PMATH_TYPE_EXPRESSION))
#define pmath_is_real(obj)     (pmath_is_double(obj) || pmath_is_mpfloat(obj))
#define pmath_is_number(obj)   (pmath_is_real(obj) || pmath_is_rational(obj))
#define pmath_is_quotient(obj) (pmath_is_pointer_of(obj, PMATH_TYPE_QUOTIENT))
#define pmath_is_rational(obj) (pmath_is_integer(obj) || pmath_is_quotient(obj))
#define pmath_is_string(obj)   (pmath_is_pointer_of(obj, PMATH_TYPE_STRING))
#define pmath_is_symbol(obj)   (pmath_is_pointer_of(obj, PMATH_TYPE_SYMBOL))

#define pmath_is_evaluatable(obj)  (pmath_is_null(obj) || pmath_is_number(obj) || pmath_is_string(obj) || pmath_is_symbol(obj) || pmath_is_expr(obj))


/*============================================================================*/

/**\def pmath_ref
   \brief Increments the reference counter of an object and returns it.
   \memberof pmath_t
   \param obj The object to be referenced.
   \return The referenced object.
   You must free the result with pmath_unref().
 */

/**\def pmath_unref
   \brief Decrements the reference counter of an object and frees its memory
          if the reference counter becomes 0.
   \memberof pmath_t
   \param obj The object to be destroyed.
 */
 
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
   e.g. for an integer A and q floating point value B.
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

/* inline versions. 
   Those are the default for C/C++ (#defining pmath_ref pmath_fast_ref, ...)
 */

PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_ref(pmath_t obj){
  if(PMATH_UNLIKELY(!pmath_is_pointer(obj)))
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

PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
void pmath_unref(pmath_t obj){
  if(PMATH_UNLIKELY(!pmath_is_pointer(obj)))
    return;

  pmath_atomic_barrier();
  if(1 == pmath_atomic_fetch_add(&(PMATH_AS_PTR(obj)->refcount), -1)){ // was 1 -> is 0
    _pmath_destroy_object(obj);
  }
  pmath_atomic_barrier();
}

#ifndef PMATH_DEBUG_NO_FASTREF
  #define pmath_equals       pmath_fast_equals
#endif

#endif /* __PMATH_CORE__OBJECTS_INLINE_H__ */
