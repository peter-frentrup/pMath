#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_update(pmath_expr_t expr) {
  pmath_t  sym;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  sym = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(sym)) {
    sym = pmath_symbol_find(sym, FALSE);
  }
  
  if(!pmath_is_symbol(sym)) {
    pmath_message(PMATH_NULL, "ssym", 1, sym);
    return expr;
  }
  
  pmath_unref(expr);
  
  pmath_symbol_update(sym);
  pmath_unref(sym);
  return PMATH_NULL;
}
