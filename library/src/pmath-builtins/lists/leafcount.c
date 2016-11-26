#include <pmath-core/numbers.h>
#include <pmath-core/packed-arrays.h>

#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>


static size_t leafcount(pmath_t obj) { // obj will be freed
  if(pmath_is_packed_array(obj)) {
    size_t dims = pmath_packed_array_get_dimensions(obj);
    const size_t *sizes = pmath_packed_array_get_sizes(obj);
    
    size_t i, total;
    total = 1;
    i = dims;
    while(i-- > 0) {
      total = sizes[i] * total + 1;
    }
    
    pmath_unref(obj);
    return total;
  }
  
  if(pmath_is_expr(obj)) {
    size_t result = 0;
    size_t i;
    
    for(i = 0; i <= pmath_expr_length(obj); ++i)
      result += leafcount(pmath_expr_get_item(obj, i));
      
    pmath_unref(obj);
    return result;
  }
  
  pmath_unref(obj);
  return 1;
}

PMATH_PRIVATE pmath_t builtin_leafcount(pmath_expr_t expr) {
  pmath_t obj;
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  return pmath_integer_new_uiptr(leafcount(obj));
}
