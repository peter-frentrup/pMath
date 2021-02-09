#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_True;

PMATH_PRIVATE pmath_t builtin_if(pmath_expr_t expr) {
  pmath_t condition;
  size_t len = pmath_expr_length(expr);
  
  if(len < 2 || len > 4) {
    pmath_message_argxxx(len, 2, 4);
    return expr;
  }
  
  condition = pmath_evaluate(pmath_expr_get_item(expr, 1));
  
  if(pmath_same(condition, pmath_System_True)) {
    pmath_t onTrue = pmath_expr_get_item(expr, 2);
    pmath_unref(condition);
    pmath_unref(expr);
    return onTrue;
  }
  else if(pmath_same(condition, pmath_System_False)) {
    pmath_t onFalse = pmath_expr_get_item(expr, 3);
    pmath_unref(condition);
    pmath_unref(expr);
    return onFalse;
  }
  else if(len == 4) {
    pmath_t onUnknown = pmath_expr_get_item(expr, 4);
    pmath_unref(condition);
    pmath_unref(expr);
    return onUnknown;
  }
  else {
    pmath_t orig = pmath_expr_get_item(expr, 1);
    if(!pmath_equals(orig, condition)) { // only warn if condition changed during evaluation
      pmath_message(PMATH_NULL, "cond", 1, pmath_ref(condition));
    }
    pmath_unref(orig);
  }
  
  expr = pmath_expr_set_item(expr, 1, condition);
  return expr;
}
