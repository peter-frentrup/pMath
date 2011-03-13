#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_append(pmath_expr_t expr){
  pmath_t list, elem;
  
  if(pmath_expr_length(expr) != 2){
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }
  
  list = pmath_expr_get_item(expr, 1);
  if(!pmath_is_expr(list)){
    pmath_unref(list);
    pmath_message(PMATH_NULL, "nexprat", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  elem = pmath_expr_get_item(expr, 2);
  pmath_unref(expr);
  return pmath_expr_append(list, 1, elem);
}
