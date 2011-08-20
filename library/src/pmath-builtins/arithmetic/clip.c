#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/number-theory-private.h>


static pmath_bool_t all_non_complex(pmath_t expr) {
  if(pmath_is_expr_of(expr, PMATH_SYMBOL_COMPLEX))
    return FALSE;
    
  if(pmath_is_expr_of(expr, PMATH_SYMBOL_LIST)) {
    size_t i;
    for(i = pmath_expr_length(expr); i > 0; --i) {
      pmath_t item = pmath_expr_get_item(expr, i);
      
      pmath_bool_t result = all_non_complex(item);
      
      pmath_unref(item);
      if(!result)
        return FALSE;
    }
  }
  
  return TRUE;
}

static pmath_t clip_all( // PMATH_NULL on error
  pmath_t x,    // will be freed
  pmath_t min,  // wont be freed
  pmath_t max,  // wont be freed
  pmath_t vmin, // wont be freed
  pmath_t vmax  // wont be freed
) {
  pmath_t test;
  
  if(pmath_is_expr_of(x, PMATH_SYMBOL_LIST)) {
    size_t i;
    
    for(i = pmath_expr_length(x); i > 0; --i) {
      pmath_t item = pmath_expr_get_item(x, i);
      
      x = pmath_expr_set_item(x, i, PMATH_NULL);
      item = clip_all(item, min, max, vmin, vmax);
      if(pmath_is_null(item)) {
        pmath_unref(x);
        return PMATH_NULL;
      }
      
      x = pmath_expr_set_item(x, i, item);
    }
    
    return x;
  }
  
  test = pmath_evaluate(pmath_expr_new_extended(
                          pmath_ref(PMATH_SYMBOL_LESS), 2,
                          pmath_ref(x),
                          pmath_ref(min)));
  pmath_unref(test);
  
  if(pmath_same(test, PMATH_SYMBOL_TRUE)) {
    pmath_unref(x);
    return pmath_ref(vmin);
  }
  
  if(!pmath_same(test, PMATH_SYMBOL_FALSE)) {
    pmath_unref(x);
    return PMATH_NULL;
  }
  
  test = pmath_evaluate(pmath_expr_new_extended(
                          pmath_ref(PMATH_SYMBOL_GREATER), 2,
                          pmath_ref(x),
                          pmath_ref(max)));
  pmath_unref(test);
  
  if(pmath_same(test, PMATH_SYMBOL_TRUE)) {
    pmath_unref(x);
    return pmath_ref(vmax);
  }
  
  if(!pmath_same(test, PMATH_SYMBOL_FALSE)) {
    pmath_unref(x);
    return PMATH_NULL;
  }
  
  return x;
}

PMATH_PRIVATE pmath_t builtin_clip(pmath_t expr) {
  /* Clip(x, min..max, {vmin, vmax})
     Clip(x, min..max)                 = Clip(x, min..max, {min, max})
     Clip(x)                           = Clip(x, -1..1, {-1, 1})
   */
  size_t exprlen = pmath_expr_length(expr);
  pmath_t x, min, max, vmin, vmax;
  
  if(exprlen < 1 || exprlen > 3) {
    pmath_message_argxxx(exprlen, 1, 3);
    return expr;
  }
  
  x = pmath_expr_get_item(expr, 1);
  if(exprlen >= 2) {
    pmath_t minmax = pmath_expr_get_item(expr, 2);
    if(pmath_is_expr_of_len(minmax, PMATH_SYMBOL_RANGE, 2)) {
      min = pmath_expr_get_item(minmax, 1);
      max = pmath_expr_get_item(minmax, 2);
    }
    else {
      pmath_unref(minmax);
      if(!all_non_complex(x)) {
        pmath_message(PMATH_NULL, "ncompl", 0);
      }
      pmath_unref(x);
      return expr;
    }
  }
  else {
    min = PMATH_FROM_INT32(-1);
    max = PMATH_FROM_INT32(1);
  }
  
  if(exprlen == 3) {
    pmath_t vminmax = pmath_expr_get_item(expr, 3);
    if(pmath_is_expr_of_len(vminmax, PMATH_SYMBOL_RANGE, 2)
        || pmath_is_expr_of_len(vminmax, PMATH_SYMBOL_LIST,  2)) {
      vmin = pmath_expr_get_item(vminmax, 1);
      vmax = pmath_expr_get_item(vminmax, 2);
    }
    else {
      pmath_unref(vminmax);
      if(!all_non_complex(x)
          || !all_non_complex(min)
          || !all_non_complex(max)) {
        pmath_message(PMATH_NULL, "ncompl", 0);
      }
      pmath_unref(x);
      pmath_unref(min);
      pmath_unref(max);
      return expr;
    }
  }
  else {
    vmin = pmath_ref(min);
    vmax = pmath_ref(max);
  }
  
  if(!all_non_complex(x)
      || !all_non_complex(min)
      || !all_non_complex(max)) {
    pmath_message(PMATH_NULL, "ncompl", 0);
    pmath_unref(x);
    pmath_unref(min);
    pmath_unref(max);
    return expr;
  }
  
  x = clip_all(x, min, max, vmin, vmax);
  pmath_unref(min);
  pmath_unref(max);
  pmath_unref(vmin);
  pmath_unref(vmax);
  if(pmath_is_null(x)) {
    pmath_unref(expr);
    return x;
  }
  
  return expr;
}
