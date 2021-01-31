#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_Undefined;

PMATH_PRIVATE pmath_t builtin_boole(pmath_expr_t expr) {
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(obj);
  if(pmath_same(obj, pmath_System_True)) {
    pmath_unref(expr);
    return PMATH_FROM_INT32(1);
  }
  
  if(pmath_same(obj, pmath_System_False)) {
    pmath_unref(expr);
    return PMATH_FROM_INT32(0);
  }
  
  if(pmath_same(obj, pmath_System_Undefined)) {
    pmath_unref(expr);
    return pmath_ref(pmath_System_Undefined);
  }
  
  return expr;
}
