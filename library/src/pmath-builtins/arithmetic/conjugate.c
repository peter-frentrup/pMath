#include <pmath-core/numbers-private.h>

#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>

PMATH_PRIVATE pmath_t builtin_conjugate(pmath_expr_t expr){
  pmath_t z;
  int z_class;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  z = pmath_expr_get_item(expr, 1);
  if(pmath_is_number(z)){
    pmath_unref(expr);
    return z;
  }
  
  if(pmath_is_expr_of(z, PMATH_SYMBOL_PLUS)){
    size_t i;
    
    expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
    for(i = pmath_expr_length(z);i > 0;--i){
      pmath_t summand = pmath_expr_get_item(z, i);
      
      if(!pmath_is_number(summand)){
        expr = pmath_expr_set_item(expr, 1, summand);
        z = pmath_expr_set_item(z, i, pmath_ref(expr));
      }
      else
        pmath_unref(summand);
    }
    
    pmath_unref(expr);
    return z;
  }
  
  if(pmath_is_expr_of_len(z, PMATH_SYMBOL_COMPLEX, 2)){
    pmath_unref(expr);
    expr = pmath_expr_get_item(z, 2);
    z = pmath_expr_set_item(z, 2, NEG(expr));
    return z;
  }
  
  z_class = _pmath_number_class(z);
  if(z_class & (PMATH_CLASS_REAL | PMATH_CLASS_RINF)){
    pmath_unref(expr);
    return z;
  }
  
  if(z_class & PMATH_CLASS_IMAGINARY){
    pmath_unref(expr);
    return NEG(z);
  }
  
  if(pmath_is_expr_of(z, PMATH_SYMBOL_DIRECTEDINFINITY)){
    if(pmath_expr_length(z) == 1){
      expr = pmath_expr_set_item(expr, 1, pmath_expr_get_item(z, 1));
      z = pmath_expr_set_item(z, 1, expr);
    }
    else
      pmath_unref(expr);
    
    return z;
  }
  
  if(pmath_equals(z, _pmath_object_overflow)
  || pmath_equals(z, _pmath_object_underflow)){
    pmath_unref(expr);
    return z;
  }
  
  return expr;
}
