#include <pmath-core/numbers-private.h>

#include <pmath-util/approximate.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/number-theory-private.h>

PMATH_PRIVATE pmath_t builtin_quotient(pmath_expr_t expr) {
  /* Quotient(m, n) = Floor(m/n)
  
     Quotient(m, n, d) = x   ==>   d <= m-nx < d+n
   */
  pmath_t m, n;
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 2 || exprlen > 3) {
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  m = pmath_expr_get_item(expr, 1);
  n = pmath_expr_get_item(expr, 2);
  
  if(pmath_is_number(n) && pmath_number_sign(n) == 0) {
    pmath_message(PMATH_NULL, "divz", 2, n, pmath_ref(expr));
    pmath_unref(m);
    return expr;
  }
  
  if(!pmath_is_numeric(m)
      || !pmath_is_numeric(n)) {
    pmath_unref(m);
    pmath_unref(n);
    return expr;
  }
  
  if(exprlen == 3) {
    pmath_t d = pmath_expr_get_item(expr, 3);
    
    if(pmath_is_numeric(d)) {
      pmath_unref(m);
      pmath_unref(n);
      pmath_unref(d);
      return expr;
    }
    
    m = pmath_evaluate(MINUS(m, d));
  }
  
  pmath_unref(expr);
  
  return FUNC(pmath_ref(PMATH_SYMBOL_FLOOR), DIV(m, n));
}

PMATH_PRIVATE pmath_t builtin_mod(pmath_expr_t expr) {
  /* Mod(m, n)
     Mod(m, n, d)
  
     n * Quotient(m, n, d) + Mod(m, n, d) == m
   */
  pmath_t m, n, md;
  size_t exprlen;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 2 || exprlen > 3) {
    pmath_message_argxxx(exprlen, 2, 3);
    return expr;
  }
  
  m = pmath_expr_get_item(expr, 1);
  n = pmath_expr_get_item(expr, 2);
  
  if(pmath_is_number(n) && pmath_number_sign(n) == 0) {
    pmath_message(PMATH_NULL, "divz", 2, n, pmath_ref(expr));
    pmath_unref(m);
    return expr;
  }
  
  if(!pmath_is_numeric(m)
      || !pmath_is_numeric(n)) {
    pmath_unref(m);
    pmath_unref(n);
    return expr;
  }
  
  if(exprlen == 3) {
    pmath_t d = pmath_expr_get_item(expr, 3);
    
    if(pmath_is_numeric(d)) {
      pmath_unref(m);
      pmath_unref(n);
      pmath_unref(d);
      return expr;
    }
    
    md = pmath_evaluate(MINUS(m, d));
  }
  else
    md = pmath_ref(m);
    
  pmath_unref(expr);
  // Mod(m,n,d) = m - n * Quotient(m,n,d) = m - n * Floor((m-d)/n) = m - Floor(m-d, n)
  return MINUS(m, FUNC2(pmath_ref(PMATH_SYMBOL_FLOOR), md, n));
}
