#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_identitymatrix(pmath_expr_t expr) {
  /* IdentityMatrix(n)
   */
  pmath_t n_obj;
  size_t i, j, n;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  n_obj = pmath_expr_get_item(expr, 1);
  if(!pmath_is_int32(n_obj) || PMATH_AS_INT32(n_obj) <= 0) {
    pmath_message(PMATH_NULL, "intpm", 2, pmath_ref(expr), PMATH_FROM_INT32(1));
    pmath_unref(n_obj);
    return expr;
  }
  
  pmath_unref(expr);
  
  n = PMATH_AS_INT32(n_obj);
  
  expr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), n);
  for(i = n; i > 0; --i) {
    pmath_t row = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), n);
    
    for(j = 1; j < i; ++j)
      row = pmath_expr_set_item(row, j, PMATH_FROM_INT32(0));
      
    for(j = i + 1; j <= n; ++j)
      row = pmath_expr_set_item(row, j, PMATH_FROM_INT32(0));
      
    row  = pmath_expr_set_item(row,  i, PMATH_FROM_INT32(1));
    expr = pmath_expr_set_item(expr, i, row);
  }
  
  return expr;
}
