#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_expr_t _pmath_expr_prepend(
  pmath_expr_t expr,  // will be freed
  pmath_t      item   // will be freed
) {
  size_t len;
  
  len = pmath_expr_length(expr);
  expr = pmath_expr_resize(expr, len + 1);
  for(; len > 0; --len) {
    expr = pmath_expr_set_item(
             expr, len + 1,
             pmath_expr_get_item(
               expr, len));
  }
  
  return pmath_expr_set_item(expr, 1, item);
}

PMATH_PRIVATE pmath_t builtin_prepend(pmath_expr_t expr) {
  pmath_t list, elem;
  size_t len;
  
  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)) {
    pmath_unref(list);
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  elem = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  
  len = pmath_expr_length(list);
  list = pmath_expr_resize(list, len + 1);
  for(; len > 0; --len) {
    list = pmath_expr_set_item(
             list, len + 1,
             pmath_expr_get_item(
               list, len));
  }
  
  return pmath_expr_set_item(list, 1, elem);
}
