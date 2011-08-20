#include <pmath-core/numbers.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


// ignores head of expressions
PMATH_PRIVATE long _pmath_object_depth(pmath_t obj) {
  if(pmath_is_expr(obj)) {
    long result = 1;
    size_t i;
    
    //for(i = 01;i <= pmath_expr_length(obj);++i)
    for(i = pmath_expr_length(obj); i > 0; --i) {
      pmath_t item = pmath_expr_get_item(obj, i);
      
      long d = _pmath_object_depth(item);
      if(result < d)
        result = d;
        
      pmath_unref(item);
    }
    
    return result + 1;
  }
  
  return 1;
}

PMATH_PRIVATE pmath_t builtin_depth(pmath_expr_t expr) {
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  expr = pmath_integer_new_slong(_pmath_object_depth(obj));
  
  pmath_unref(obj);
  return expr;
}
