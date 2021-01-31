#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_True;

PMATH_PRIVATE pmath_t builtin_identical(pmath_expr_t expr) {
  pmath_t prev;
  size_t i, len;
  
  len = pmath_expr_length(expr);
  if(len <= 1) {
    pmath_unref(expr);
    return pmath_ref(pmath_System_True);
  }
  
  prev = pmath_expr_get_item(expr, 1);
  
  for(i = 2; i <= len; i++) {
    pmath_t next = pmath_expr_get_item(expr, i);
    if(!pmath_equals(prev, next)) {
      pmath_unref(prev);
      pmath_unref(next);
      pmath_unref(expr);
      return pmath_ref(pmath_System_False);
    }
    
    pmath_unref(prev);
    prev = next;
  }
  pmath_unref(prev);
  pmath_unref(expr);
  
  return pmath_ref(pmath_System_True);
}

PMATH_PRIVATE pmath_t builtin_unidentical(pmath_expr_t expr) {
  size_t len = pmath_expr_length(expr);
  size_t i, j;
  for(i = 1; i < len; i++) {
    pmath_t a = pmath_expr_get_item(expr, i);
    for(j = i + 1; j <= len; j++) {
      pmath_t b = pmath_expr_get_item(expr, j);
      if(pmath_equals(a, b)) {
        pmath_unref(a);
        pmath_unref(b);
        pmath_unref(expr);
        return pmath_ref(pmath_System_False);
      }
      pmath_unref(b);
    }
    pmath_unref(a);
  }
  pmath_unref(expr);
  
  return pmath_ref(pmath_System_True);
}
