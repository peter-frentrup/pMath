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

/**\brief Increments the reference counter of an object and returns it.
   \memberof pmath_t
   \param obj The object to be referenced.
   \return The referenced object.
   You must free the result with pmath_unref().
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_ref(pmath_t obj);

/**\brief Decrements the reference counter of an object and frees its memory
          if the reference counter becomes 0.
   \memberof pmath_t
   \param obj The object to be destroyed.
 */
PMATH_API 
void pmath_unref(pmath_t obj);

/**\brief Determine whether an object has a specific type.
   \memberof pmath_t
   \param obj The pMath object.
   \param type A type or a set of types.
   \return TRUE when the object is of the specified type.

   To test, whether an object is a `magic value`, the PMATH_OBJECT_IS_MAGIC()
   macro is prefered.
 */
PMATH_API 
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_instance_of(
  pmath_t      obj,
  pmath_type_t type);

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
pmath_t pmath_fast_ref(pmath_t obj){
  if(PMATH_UNLIKELY(PMATH_IS_MAGIC(obj)))
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
void pmath_fast_unref(pmath_t obj){
  if(PMATH_UNLIKELY(PMATH_IS_MAGIC(obj)))
    return;

  pmath_atomic_barrier();
  if(1 == pmath_atomic_fetch_add(&(PMATH_AS_PTR(obj)->refcount), -1)){ // was 1 -> is 0
    _pmath_destroy_object(obj);
  }
  pmath_atomic_barrier();
}

PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_fast_instance_of(
  pmath_t obj,
  pmath_type_t type
){
  if(PMATH_UNLIKELY(PMATH_IS_MAGIC(obj)))
    return (type & PMATH_TYPE_MAGIC) != 0;

  return ((1 << PMATH_AS_PTR(obj)->type_shift) & type) != 0;
}

PMATH_FORCE_INLINE
PMATH_INLINE_NODEBUG
PMATH_ATTRIBUTE_PURE
pmath_bool_t pmath_fast_equals(
  pmath_t objA,
  pmath_t objB
){
  if(PMATH_AS_PTR(objA) == PMATH_AS_PTR(objB)) return TRUE;
  return pmath_equals(objA, objB);
}

#ifndef PMATH_DEBUG_NO_FASTREF
  #define pmath_ref          pmath_fast_ref
  #define pmath_unref        pmath_fast_unref
  #define pmath_instance_of  pmath_fast_instance_of
  #define pmath_equals       pmath_fast_equals
#endif

/*============================================================================*/

#define pmath_same(objA, objB)  (PMATH_AS_PTR(objA) == PMATH_AS_PTR(objB))
#define pmath_is_null(obj)      (PMATH_AS_PTR(obj) == NULL)

#define pmath_is_custom(obj)  (pmath_instance_of(obj, PMATH_TYPE_CUSTOM))
#define pmath_is_double(obj)  (pmath_instance_of(obj, PMATH_TYPE_MACHINE_FLOAT))
#define pmath_is_expr(obj)    (pmath_instance_of(obj, PMATH_TYPE_EXPRESSION))
#define pmath_is_integer(obj) (pmath_instance_of(obj, PMATH_TYPE_INTEGER))
#define pmath_is_number(obj)  (pmath_instance_of(obj, PMATH_TYPE_NUMBER))
#define pmath_is_string(obj)  (pmath_instance_of(obj, PMATH_TYPE_STRING))
#define pmath_is_symbol(obj)  (pmath_instance_of(obj, PMATH_TYPE_SYMBOL))

#endif /* __PMATH_CORE__OBJECTS_INLINE_H__ */
