#include <pmath-core/numbers.h>

#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_lengthwhile(pmath_expr_t expr) {
  /* LengthWhile(list, crit)
   */
  pmath_expr_t list;
  pmath_t crit;
  size_t i, len;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  crit = pmath_expr_get_item(expr, 2);
  
  if(!pmath_is_expr(list)) {
    pmath_unref(list);
    pmath_unref(crit);
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  pmath_unref(expr);
  len = pmath_expr_length(list);
  for(i = 1; i <= len; ++i) {
    pmath_t item = pmath_expr_get_item(list, i);
    item = pmath_expr_new_extended(pmath_ref(crit), 1, item);
    item = pmath_evaluate(item);
    pmath_unref(item);
    
    if(!pmath_same(item, PMATH_SYMBOL_TRUE)) {
      pmath_unref(list);
      pmath_unref(crit);
      return pmath_integer_new_uiptr(i - 1);
    }
  }
  
  pmath_unref(list);
  pmath_unref(crit);
  return pmath_integer_new_uiptr(len);
}
