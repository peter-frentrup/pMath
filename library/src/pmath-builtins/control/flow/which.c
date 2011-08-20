#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_which(pmath_expr_t expr) {
  size_t i;
  size_t exprlen = pmath_expr_length(expr);
  
  if((exprlen & 1) == 1) {
    pmath_message(PMATH_NULL, "argct", 1, pmath_integer_new_uiptr(exprlen));
    return expr;
  }
  
  for(i = 1; i <= exprlen; i += 2) {
    pmath_t test = pmath_expr_get_item(expr, i);
    test = pmath_evaluate(test);
    pmath_unref(test);
    
    if(pmath_same(test, PMATH_SYMBOL_TRUE)) {
      test = pmath_expr_get_item(expr, i + 1);
      pmath_unref(expr);
      return test;
    }
    
    if(pmath_same(test, PMATH_SYMBOL_FALSE)) {
      expr = pmath_expr_set_item(expr, i,   PMATH_UNDEFINED);
      expr = pmath_expr_set_item(expr, i + 1, PMATH_UNDEFINED);
    }
  }
  
  expr = pmath_expr_remove_all(expr, PMATH_UNDEFINED);
  if(pmath_expr_length(expr) == 0) {
    pmath_unref(expr);
    return PMATH_NULL;
  }
  
  return expr;
}
