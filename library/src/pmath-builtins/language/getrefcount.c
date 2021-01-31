#include <pmath-core/expressions.h>
#include <pmath-core/numbers-private.h>
#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_Infinity;

PMATH_PRIVATE pmath_t builtin_private_getrefcount(pmath_expr_t expr) {
  pmath_t item;
  
  if(pmath_expr_length(expr) != 1)
    return expr;
  
  item = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  if(pmath_is_pointer(item)) 
    expr = pmath_integer_new_siptr(pmath_refcount(item));
  else 
    expr = pmath_ref(_pmath_object_pos_infinity);
  
  pmath_unref(item);
  return expr;
}
