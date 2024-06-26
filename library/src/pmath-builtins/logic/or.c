#include <pmath-core/expressions-private.h>

#include <pmath-util/evaluation.h>

#include <pmath-builtins/all-symbols-private.h>


extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_Undefined;

PMATH_PRIVATE pmath_t builtin_or(pmath_expr_t expr) {
// FIXME: what should Undefined || True give ?

  const size_t elen = pmath_expr_length(expr);
  if(elen > 1) {
    pmath_bool_t have_null = FALSE;
    size_t i;
    
    for(i = 0; i <= elen; i++) {
      pmath_t item = pmath_evaluate(pmath_expr_get_item(expr, i));
      
      if( pmath_same(item, pmath_System_True) ||
          pmath_same(item, pmath_System_Undefined))
      {
        pmath_unref(expr);
        return item;
      }
      
      if(pmath_same(item, pmath_System_False)) {
        expr = pmath_expr_set_item(expr, i, PMATH_NULL);
        have_null = TRUE;
        pmath_unref(item);
      }
      else {
        have_null |= pmath_is_null(item);
        expr = pmath_expr_set_item(expr, i, item);
      }
    }
    
    if(have_null)
      return _pmath_expr_shrink_associative(expr, PMATH_NULL);
    return expr;
  }
  
  if(elen == 1) {
    pmath_t item = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return item;
  }
  
  pmath_unref(expr);
  return pmath_ref(pmath_System_False);
}
