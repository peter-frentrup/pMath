#include <pmath-util/messages.h>

#include <pmath-core/numbers.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_unitvector(pmath_expr_t expr) {
  size_t exprlen = pmath_expr_length(expr);
  size_t n, k, i;
  pmath_t obj;
  
  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 2);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, exprlen);
  if(!pmath_is_int32(obj) || PMATH_AS_INT32(obj) <= 0) {
    pmath_unref(obj);
    pmath_message(PMATH_NULL, "intpm", 2, pmath_integer_new_uiptr(exprlen), pmath_ref(expr));
    return expr;
  }
  
  k = (size_t)PMATH_AS_INT32(obj);
  n = 2;
  
  if(exprlen > 1) {
    obj = pmath_expr_get_item(expr, 1);
    if(!pmath_is_int32(obj) || PMATH_AS_INT32(obj) <= 0) {
      pmath_unref(obj);
      pmath_message(PMATH_NULL, "intpm", 1, PMATH_FROM_INT32(1), pmath_ref(expr));
      return expr;
    }
    
    n = (size_t)PMATH_AS_INT32(obj);
  }
  
  if(n < k) {
    pmath_message(PMATH_NULL, "dir", 2, pmath_integer_new_uiptr(n), pmath_integer_new_uiptr(k));
    return expr;
  }
  
  pmath_unref(expr);
  expr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), n);
  for(i = n; i > 0; --i)
    expr = pmath_expr_set_item(expr, i, PMATH_FROM_INT32(0));
    
  expr = pmath_expr_set_item(expr, k, PMATH_FROM_INT32(1));
  return expr;
}
