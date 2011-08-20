#include <pmath-builtins/all-symbols-private.h>

PMATH_PRIVATE pmath_t builtin_stringexpression(pmath_expr_t expr) {
  pmath_bool_t have_undef = FALSE;
  pmath_t item;
  pmath_string_t str;
  size_t i, j, k, len;
  int slen;
  
  len = pmath_expr_length(expr);
  
  if(len > 1) {
    i = 1;
    while(i < len) {
      item = pmath_expr_get_item(expr, i);
      
      j = i;
      slen = 0;
      while(j <= len && pmath_is_string(item)) {
        slen += pmath_string_length(item);
        pmath_unref(item);
        item = pmath_expr_get_item(expr, ++j);
      }
      pmath_unref(item);
      
      if(i + 1 < j && slen >= 0) {
        str = pmath_string_new(slen);
        
        k = i;
        while(i < j) {
          str = pmath_string_concat(str, pmath_expr_get_item(expr, i));
          ++i;
        }
        
        if(k == 1 && j == len + 1) {
          pmath_unref(expr);
          return str;
        }
        
        have_undef = TRUE;
        
        expr = pmath_expr_set_item(expr, k, str);
        ++k;
        while(k < j) {
          expr = pmath_expr_set_item(expr, k, PMATH_UNDEFINED);
          ++k;
        }
      }
      else
        ++i;
    }
    
    if(have_undef)
      return pmath_expr_remove_all(expr, PMATH_UNDEFINED);
      
    return expr;
  }
  else if(len == 1) {
    item = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return item;
  }
  
  pmath_unref(expr);
  return pmath_string_new(0);
}
