#include <pmath-core/expressions-private.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_xor(pmath_expr_t expr) {
  pmath_bool_t result = FALSE;
  size_t i;
  
  for(i = pmath_expr_length(expr); i > 0; --i) {
    pmath_t item = pmath_expr_get_item(expr, i);
    pmath_unref(item);
    
    if(pmath_same(item, PMATH_SYMBOL_TRUE)) {
      result = (result != FALSE);
    }
    else if(pmath_same(item, PMATH_SYMBOL_FALSE)) {
      result = (result == FALSE);
    }
    else
      return expr;
  }
  
  pmath_unref(expr);
  return pmath_ref(result ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE);
}
