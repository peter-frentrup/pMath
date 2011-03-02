#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


static pmath_t operate(
  pmath_t expr, // will be freed
  pmath_t p,    // will be freed
  size_t level
){
  if(level == 0)
    return pmath_expr_new_extended(p, 1, expr);
  
  if(!pmath_is_expr(expr)){
    pmath_unref(p);
    return expr;
  }
  
  return pmath_expr_set_item(expr, 0, 
    operate(pmath_expr_get_item(expr, 0), p, level - 1));
}

PMATH_PRIVATE pmath_t builtin_operate(pmath_expr_t expr){
  /* Operate(h(x, y), p)     =  p(h)(x, y)
     Operate(h(x, y), p, n)  operates on level n (default = 1) of h
   */
  pmath_t obj, p;
  size_t exprlen, level;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 2 || exprlen > 3){
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  level = 1;
  
  if(exprlen == 3){
    pmath_t obj = pmath_expr_get_item(expr, 3);
    if(!pmath_is_integer(obj) || !pmath_integer_fits_ui(obj)){
      pmath_unref(obj);
      pmath_message(
        PMATH_NULL, "numn", 2,
        pmath_integer_new_ui(3),
        pmath_ref(expr));
      return expr;
    }
    
    level = pmath_integer_get_ui(obj);
    pmath_unref(obj);
  }
  
  obj = pmath_expr_get_item(expr, 1);
  p =   pmath_expr_get_item(expr, 2);
  
  pmath_unref(expr);
  
  return operate(obj, p, level);
}
