#include <pmath-core/expressions-private.h>

#include <pmath-util/evaluation.h>

#include <pmath-builtins/all-symbols-private.h>


PMATH_PRIVATE pmath_t builtin_and(pmath_expr_t expr){
  pmath_bool_t have_null = FALSE;
  size_t i, elen;

  elen = pmath_expr_length(expr);
  if(elen > 1){
    for(i = 0;i <= elen;i++){
      pmath_t item = pmath_evaluate(pmath_expr_get_item(expr, i));
      if(pmath_same(item, PMATH_SYMBOL_FALSE)
      || pmath_same(item, PMATH_SYMBOL_UNDEFINED)){
        pmath_unref(expr);
        return item;
      }
      
      if(pmath_same(item, PMATH_SYMBOL_TRUE)){
        expr = pmath_expr_set_item(expr, i, NULL);
        have_null = TRUE;
      }
      else
        have_null|= item == NULL;
      
      pmath_unref(item);
    }
    if(have_null)
      return _pmath_expr_shrink_associative(expr, NULL);
    return expr;
  }

  if(elen == 1){
    pmath_t item = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return item;
  }

  pmath_unref(expr);
  return pmath_ref(PMATH_SYMBOL_TRUE);
}
