#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_identitymatrix(pmath_expr_t expr){
/* IdentityMatrix(n)
 */
  pmath_t n_obj, zero, one;
  size_t i, j, n;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  n_obj = pmath_expr_get_item(expr, 1);
  if(!pmath_is_integer(n_obj)
  || !pmath_integer_fits_ui(n_obj)
  || pmath_number_sign(n_obj) <= 0){
    pmath_message(NULL, "intpm", 2, pmath_ref(expr), pmath_integer_new_si(1));
    pmath_unref(n_obj);
    return expr;
  }
  
  pmath_unref(expr);
  
  n = pmath_integer_get_ui(n_obj);
  pmath_unref(n_obj);
  
  expr = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), n);
  zero = pmath_integer_new_si(0);
  one  = pmath_integer_new_si(1);
  for(i = n;i > 0;--i){
    pmath_t row = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), n);
    
    for(j = 1;j < i;++j)
      row = pmath_expr_set_item(row, j, pmath_ref(zero));
    
    for(j = i+1;j <= n;++j)
      row = pmath_expr_set_item(row, j, pmath_ref(zero));
    
    row  = pmath_expr_set_item(row,  i, pmath_ref(one));
    expr = pmath_expr_set_item(expr, i, row);
  }
  
  pmath_unref(zero);
  pmath_unref(one);
  
  return expr;
}
