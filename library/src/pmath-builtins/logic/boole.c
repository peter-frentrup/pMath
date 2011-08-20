#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_boole(pmath_expr_t expr) {
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(obj);
  if(pmath_same(obj, PMATH_SYMBOL_TRUE)) {
    pmath_unref(expr);
    return PMATH_FROM_INT32(1);
  }
  
  if(pmath_same(obj, PMATH_SYMBOL_FALSE)) {
    pmath_unref(expr);
    return PMATH_FROM_INT32(0);
  }
  
  if(pmath_same(obj, PMATH_SYMBOL_UNDEFINED)) {
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_UNDEFINED);
  }
  
  return expr;
}
