#include <pmath-builtins/control-private.h>
#include <pmath-builtins/all-symbols-private.h>

#include <pmath-util/concurrency/atomic.h>
#include <pmath-util/messages.h>


PMATH_PRIVATE
pmath_t builtin_private_isvalid(pmath_expr_t expr) {
/*  System`Private`IsValid(expr)
    
    Read the "VALID" flag of an object (default: False)
 */
  pmath_t obj;
  uint8_t flags8;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(!pmath_is_pointer(obj))
    return pmath_ref(PMATH_SYMBOL_FALSE);
  
  flags8 = pmath_atomic_read_uint8_aquire(&PMATH_AS_PTR(obj)->flags8);
  pmath_unref(obj);
  
  if(flags8 & PMATH_OBJECT_FLAGS8_VALID)
    return pmath_ref(PMATH_SYMBOL_TRUE);
  else
    return pmath_ref(PMATH_SYMBOL_FALSE);
}

PMATH_PRIVATE
pmath_t builtin_private_setvalid(pmath_expr_t expr) {
/*  System`Private`SetValid(expr, flag)
    
    Change the "VALID" flag of an object and return the object.
    Note that this has a "spooky distant effect":
    
      a:= b:= f(123)
      c:= f(123)
      {System`Private`IsValid(a), System`Private`IsValid(b), System`Private`IsValid(c)} % gives {False, False, False}
      System`Private`SetValid(a, True)
      {System`Private`IsValid(a), System`Private`IsValid(b), System`Private`IsValid(c)} % gives {True, True, False}
 */
  pmath_t obj;
  pmath_t value;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  if(!pmath_is_pointer(obj)) {
    pmath_message(PMATH_NULL, "atom", 1, obj);
    return expr;
  }
  
  value = pmath_expr_get_item(expr, 2);
  pmath_unref(value);
  if(pmath_same(value, PMATH_SYMBOL_TRUE)) {
    pmath_atomic_or_uint8(&PMATH_AS_PTR(obj)->flags8, PMATH_OBJECT_FLAGS8_VALID);
    pmath_unref(expr);
    return obj;
  }
  if(pmath_same(value, PMATH_SYMBOL_FALSE)) {
    pmath_atomic_and_uint8(&PMATH_AS_PTR(obj)->flags8, (uint8_t)~PMATH_OBJECT_FLAGS8_VALID);
    pmath_unref(expr);
    return obj;
  }
  
  pmath_unref(obj);
  pmath_message(PMATH_NULL, "bool", 2, pmath_ref(expr), PMATH_FROM_INT32(2));
  return expr;
}

